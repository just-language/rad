#include <rad/async/io_executor.h> // For eof error
#include <rad/function_view.h>
#include <rad/net/ssl/wolfssl_ctx.h>
#include <rad/net/types.h>
#define NO_WOLFSSL_STUB
#define HAVE_EX_DATA
#define WOLFSSL_NO_OPTIONS_H
#define OPENSSL_EXTRA
#define HAVE_ALPN
#define HAVE_SNI
#define WOLFSSL_TLS13
#include <wolfssl/error-ssl.h>
#include <wolfssl/ssl.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace ssl;
using namespace ssl::wolfssl;
using ssl::context_base;
using ssl::engine_state;
using ssl::ring_bio;

namespace {
    struct init_wolfssl_t {
        init_wolfssl_t() {
            wolfSSL_Init();
        }

        ~init_wolfssl_t() {
            wolfSSL_Cleanup();
        }
    };

    const init_wolfssl_t init_wolfssl;

    using wolfssl_method_t = WOLFSSL_METHOD* (*)();

    constexpr wolfssl_method_t select_wolfssl_method(version v) {
        switch (v) {
        case version::tls_client:
            return wolfSSLv23_client_method;
        case version::tls_server:
            return wolfSSLv23_server_method;

        case version::tlsv1_client:
            return wolfTLSv1_2_client_method;
        case version::tlsv1_server:
            return wolfTLSv1_2_server_method;

        case version::tlsv11_client:
            return wolfTLSv1_2_client_method;
        case version::tlsv11_server:
            return wolfTLSv1_2_server_method;

        case version::tlsv12_client:
            return wolfTLSv1_2_client_method;
        case version::tlsv12_server:
            return wolfTLSv1_2_server_method;

        case version::tlsv13_client:
            return wolfTLSv1_3_client_method;
        case version::tlsv13_server:
            return wolfTLSv1_3_server_method;

        default:
            return nullptr;
        }
    }

    inline std::error_code last_net_error() noexcept {
#ifdef _WIN32
        return std::error_code{::WSAGetLastError(), system_category()};
#else
        return std::error_code{errno, system_category()};
#endif // _WIN32
    }

    class ssl_error_category : public std::error_category {
        const char* name() const noexcept override {
            return "ssl";
        }

        std::string message(int condition) const override {
            if (condition == SSL_ERROR_SYSCALL) {
                return last_net_error().message();
            }
            std::array<char, 1024> error_str = {0};
            wolfSSL_ERR_error_string_n(
                static_cast<unsigned long>(condition), error_str.data(),
                static_cast<unsigned long>(error_str.size()));
            if (error_str.back() != '\0') {
                return std::string{error_str.data(), error_str.size()};
            }
            else {
                return std::string{error_str.data()};
            }
        }
    };

    ssl_error_category ssl_error_category_inst;

    inline void get_last_ssl_error(std::error_code& ec) noexcept {
        int error_code = static_cast<int>(wolfSSL_ERR_get_error());
        if (error_code == WOLFSSL_ERROR_ZERO_RETURN) {
            ec = io::detail::make_eof_error_code();
        }
        else {
            ec.assign(static_cast<int>(wolfSSL_ERR_get_error()),
                      ssl_category());
        }
    }

    WOLFSSL_CTX* as_ctx(void* ctx) noexcept {
        return static_cast<WOLFSSL_CTX*>(ctx);
    }

    WOLFSSL* as_ssl(void* ssl) noexcept {
        return static_cast<WOLFSSL*>(ssl);
    }

    /*!
     * @brief Write the password to the supplied buffer.
     * @param buf Buffer to write the password to.
     * @param size Length of @p buf.
     * @param rwflag Indicates whether the callback is used for
     * reading/decryption (rwflag=0) or writing/encryption (rwflag=1).
     * @param userdata Pointer to context user provided data.
     * @return On success the number of characters, or -1 on error.
     */
    int passwd_cb_fn(char* buf, int size, int rwflag, void* userdata) noexcept {
        auto cb = static_cast<ssl::password_callback_base*>(userdata);
        if (cb == nullptr) {
            return -1;
        }
        const std::size_t buf_size = static_cast<std::size_t>(size);
        try {
            auto ret =
                cb->get_password(buffer(buf, buf_size),
                                 static_cast<ssl::password_purpose>(rwflag));
            if (!ret.has_value() || *ret > buf_size) {
                return -1;
            }
            return static_cast<int>(*ret);
        }
        catch (...) {
            return -1;
        }
    }

    /*!
     * @brief OpenSSL verify callback.
     * @param preverify_ok non-zero if the certificate was verified, zero if
     * had an error.
     * @param x509_ctx The certificate.
     * @return non-zero to continue verification, zero to abort.
     */
    int verify_cb_fn(int preverify_ok,
                     WOLFSSL_X509_STORE_CTX* x509_ctx) noexcept {
        if (!x509_ctx) {
            return 0;
        }

        // https://docs.openssl.org/master/man3/SSL_CTX_set_verify/
        // The openssl docs states that:
        // SSL_get_ex_data_X509_STORE_CTX_idx can be called to get the
        // data index of the current SSL object that is doing the
        // verification. wolfssl seems to be compatible with openssl
        // here

        const int session_index = wolfSSL_get_ex_data_X509_STORE_CTX_idx();
        if (session_index < 0) {
            return 0;
        }

        auto ssl = static_cast<WOLFSSL*>(
            wolfSSL_X509_STORE_CTX_get_ex_data(x509_ctx, session_index));
        if (ssl == nullptr) {
            return 0;
        }

        WOLFSSL_CTX* ctx = wolfSSL_get_SSL_CTX(ssl);
        if (ctx == nullptr) {
            return 0;
        }

        auto cb = reinterpret_cast<verify_callback_base*>(
            wolfSSL_CTX_get_ex_data(ctx, 0));
        if (cb != nullptr) {
            try {
                return static_cast<int>(
                    cb->verify(static_cast<bool>(preverify_ok), x509_ctx));
            }
            catch (...) {
                return 0;
            }
        }
        else {
            return 0;
        }
    }

    // custom io callbacks returns -2 to indicate no space or buffer

    int ring_io_recv(WOLFSSL* ssl, char* buf, int sz, void* ctx) noexcept {
        assert(ssl != nullptr);
        assert(ctx != nullptr);
        if (ctx == nullptr) {
            return WOLFSSL_CBIO_ERR_GENERAL;
        }
        ring_bio* rbio = static_cast<ring_bio*>(ctx);
        size_t ret = rbio->get_data(buffer(buf, static_cast<size_t>(sz)));
        if (!ret) {
            return WOLFSSL_CBIO_ERR_WANT_READ;
        }
        return static_cast<int>(ret);
    }

    int ring_io_send(WOLFSSL* ssl, char* buf, int sz, void* ctx) noexcept {
        assert(ssl != nullptr);
        assert(ctx != nullptr);
        if (ctx == nullptr || buf == nullptr || sz <= 0) {
            return WOLFSSL_CBIO_ERR_GENERAL;
        }
        ring_bio* rbio = static_cast<ring_bio*>(ctx);
        const size_t n = static_cast<size_t>(sz);
        // if (rbio->space_size() < n) {
        //  from the docs: If the cipher-send buffer isn’t big enough,
        //  the callback should return -2 this means that partial writes
        //  are not allowed ?
        //	return WOLFSSL_CBIO_ERR_WANT_WRITE;
        //}
        // partial writes seem to be supported in
        // https://github.com/wolfSSL/wolfssl/blob/master/src/internal.c
        size_t ret = rbio->put_data(buffer(buf, n));
        if (!ret) {
            return WOLFSSL_CBIO_ERR_WANT_WRITE;
        }
        return static_cast<int>(ret);
    }
} // namespace

const std::error_category& wolfssl::ssl_category() noexcept {
    return ssl_error_category_inst;
}

context::~context() {
    close();
}

void context::remove_verify_cb() noexcept {
    void* cb = wolfSSL_CTX_get_ex_data(as_ctx(ctx_), 0);
    if (cb) {
        ::wolfSSL_CTX_set_ex_data(as_ctx(ctx_), 0, nullptr);
        delete static_cast<verify_callback_base*>(cb);
    }
}

void context::remove_passwd_cb() noexcept {
    void* cb = ::wolfSSL_CTX_get_default_passwd_cb_userdata(as_ctx(ctx_));
    if (cb) {
        ::wolfSSL_CTX_set_default_passwd_cb_userdata(as_ctx(ctx_), nullptr);
        delete static_cast<password_callback_base*>(cb);
    }
}

void context::free_ctx_no_check() noexcept {
    remove_verify_cb();
    remove_passwd_cb();
    wolfSSL_CTX_free(as_ctx(ctx_));
}

void context::add_verify_certificate(const_buffer cert_buffer,
                                     file_format format,
                                     std::error_code& ec) noexcept {
    ec.clear();
    const int wformat = format == file_format::asn1 ? WOLFSSL_FILETYPE_ASN1
                                                    : WOLFSSL_FILETYPE_PEM;
    const int ret = wolfSSL_CTX_load_verify_buffer(
        as_ctx(ctx_), cert_buffer.data_as<const unsigned char>(),
        static_cast<long>(cert_buffer.size()), wformat);
    if (ret != WOLFSSL_SUCCESS) {
        ec.assign(ret, ssl_category());
    }
}

void context::add_verify_file(const std::string& file,
                              std::error_code& ec) noexcept {
    if (wolfSSL_CTX_load_verify_locations(as_ctx(ctx_), file.c_str(),
                                          nullptr) == 0) {
        return get_last_ssl_error(ec);
    }
}

void context::add_verify_path(const std::string& path,
                              std::error_code& ec) noexcept {
    if (wolfSSL_CTX_load_verify_locations(as_ctx(ctx_), nullptr,
                                          path.c_str()) == 0) {
        return get_last_ssl_error(ec);
    }
}

void context::add_verify_file_path(const std::string& path,
                                   const std::string& file,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (wolfSSL_CTX_load_verify_locations(as_ctx(ctx_), file.c_str(),
                                          path.c_str()) == 0) {
        return get_last_ssl_error(ec);
    }
}

void context::set_default_verify_paths(std::error_code& ec) noexcept {
    ec.clear();
    if (wolfSSL_CTX_set_default_verify_paths(as_ctx(ctx_)) == 0) {
        return get_last_ssl_error(ec);
    }
}

void context::use_own_certificate(const_buffer cert_buffer, file_format format,
                                  std::error_code& ec) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    ec.clear();
    const int wformat = format == file_format::asn1 ? WOLFSSL_FILETYPE_ASN1
                                                    : WOLFSSL_FILETYPE_PEM;
    const int ret = wolfSSL_CTX_use_certificate_buffer(
        as_ctx(ctx_), cert_buffer.data_as<const unsigned char>(),
        static_cast<long>(cert_buffer.size()), wformat);
    if (ret != WOLFSSL_SUCCESS) {
        get_last_ssl_error(ec);
    }
}

void context::use_own_certificate_file(const std::string& filename,
                                       file_format format,
                                       std::error_code& ec) noexcept {
    ec.clear();
    const int wformat = format == file_format::asn1 ? WOLFSSL_FILETYPE_ASN1
                                                    : WOLFSSL_FILETYPE_PEM;
    const int ret = wolfSSL_CTX_use_certificate_file(as_ctx(ctx_),
                                                     filename.c_str(), wformat);
    if (ret != WOLFSSL_SUCCESS) {
        get_last_ssl_error(ec);
    }
}

void context::use_private_key(const_buffer key_buffer, file_format format,
                              std::error_code& ec) noexcept {
    ec.clear();
    const int wformat = format == file_format::asn1 ? WOLFSSL_FILETYPE_ASN1
                                                    : WOLFSSL_FILETYPE_PEM;
    const int ret = wolfSSL_CTX_use_PrivateKey_buffer(
        as_ctx(ctx_), key_buffer.data_as<const unsigned char>(),
        static_cast<long>(key_buffer.size()), wformat);
    if (ret != WOLFSSL_SUCCESS) {
        ec.assign(ret, ssl_category());
    }
}

void context::use_private_key_file(const std::string& filename,
                                   file_format format,
                                   std::error_code& ec) noexcept {
    ec.clear();
    const int wformat = format == file_format::asn1 ? WOLFSSL_FILETYPE_ASN1
                                                    : WOLFSSL_FILETYPE_PEM;
    const int ret = wolfSSL_CTX_use_PrivateKey_file(as_ctx(ctx_),
                                                    filename.c_str(), wformat);
    if (ret != WOLFSSL_SUCCESS) {
        get_last_ssl_error(ec);
    }
}

void context::set_verify_depth(std::size_t depth) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        return;
    }
    constexpr std::size_t max_depth =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    depth = std::min(depth, max_depth);
    wolfSSL_CTX_set_verify_depth(as_ctx(ctx_), static_cast<int>(depth));
}

void context::set_verify_cb(const verify_mode* mode,
                            std::unique_ptr<verify_callback_base> cb) noexcept {
    if (ctx_ == nullptr) {
        return;
    }
    if (cb != nullptr) {
        remove_verify_cb();
        wolfSSL_CTX_set_ex_data(as_ctx(ctx_), 0, cb.release());
    }
    const int overify = mode == nullptr
                            ? wolfSSL_CTX_get_verify_mode(as_ctx(ctx_))
                        : *mode == verify_mode::none     ? WOLFSSL_VERIFY_NONE
                        : *mode == verify_mode::optional ? WOLFSSL_VERIFY_NONE
                                                         : WOLFSSL_VERIFY_PEER;

    wolfSSL_CTX_set_verify(as_ctx(ctx_), overify,
                           cb == nullptr
                               ? wolfSSL_CTX_get_verify_callback(as_ctx(ctx_))
                               : verify_cb_fn);
}

void context::set_passwd_cb(
    std::unique_ptr<password_callback_base> cb) noexcept {
    if (ctx_ == nullptr) {
        return;
    }
    remove_passwd_cb();
    wolfSSL_CTX_set_default_passwd_cb_userdata(as_ctx(ctx_), cb.release());
    wolfSSL_CTX_set_default_passwd_cb(as_ctx(ctx_), passwd_cb_fn);
}

void context::open(version v, std::error_code& ec) noexcept {
    auto method_fn = select_wolfssl_method(v);
    if (!method_fn) {
        ec = std::error_code{UNSUPPORTED_PROTO_VERSION, ssl_category()};
        return;
    }

    WOLFSSL_METHOD* meth = method_fn();
    WOLFSSL_CTX* new_ctx = wolfSSL_CTX_new(meth);
    if (!new_ctx) {
        // how to free WOLFSSL_METHOD* ?
        return get_last_ssl_error(ec);
    }
    wolfSSL_CTX_set_options(new_ctx, WOLFSSL_OP_NO_RENEGOTIATION);
    wolfSSL_CTX_set_mode(new_ctx, WOLFSSL_MODE_ENABLE_PARTIAL_WRITE);
    wolfSSL_CTX_SetIORecv(new_ctx, ring_io_recv);
    wolfSSL_CTX_SetIOSend(new_ctx, ring_io_send);
    close();
    ctx_ = new_ctx;
}

std::unique_ptr<ssl::ssl_impl_base>
context::make_generic_impl(std::error_code& ec) noexcept {
    ec.clear();
    wolfssl_impl* impl_ptr = new (std::nothrow) wolfssl_impl;
    if (impl_ptr == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return nullptr;
    }
    std::unique_ptr<ssl::ssl_impl_base> impl{impl_ptr};
    impl->open(*this, ec);
    if (ec) {
        return nullptr;
    }
    return impl;
}

context context::generic_client() {
    context ctx;
    ctx.open(version::tls_client);
    return ctx;
}

context context::generic_server() {
    context ctx;
    ctx.open(version::tls_server);
    return ctx;
}

void wolfssl_impl::free_ssl_no_check() noexcept {
    wolfSSL_free(as_ssl(ssl_));
}

void* wolfssl_impl::native_handle() const noexcept {
    return ssl_;
}

void wolfssl_impl::open(context_base& ctx, std::error_code& ec) noexcept {
    open(static_cast<context&>(ctx), ec);
}

std::string_view wolfssl_impl::version() const noexcept {
    if (ssl_ == nullptr) {
        return {};
    }
    const char* p = wolfSSL_get_version(as_ssl(ssl_));
    if (p == nullptr) {
        return {};
    }
    return std::string_view{p};
}

std::string_view wolfssl_impl::get_alpn_protocol() const noexcept {
    if (ssl_ == nullptr) {
        return {};
    }
    char* p = nullptr;
    unsigned short n = 0;
    wolfSSL_ALPN_GetProtocol(as_ssl(ssl_), &p, &n);
    if (p != nullptr && n != 0) {
        return std::string_view{p, n};
    }
    return {};
}

void wolfssl_impl::set_alpn_protocols(
    std::span<const std::string_view> protos) noexcept {
    if (ssl_ == nullptr) {
        return;
    }
    // string passed to wolfSSL_UseALPN() can't be null
    // options must either contain WOLFSSL_ALPN_CONTINUE_ON_MISMATCH or
    // WOLFSSL_ALPN_FAILED_ON_MISMATCH
    char empty_protos[] = {'\0'};
    if (protos.empty()) {
        wolfSSL_UseALPN(as_ssl(ssl_), empty_protos, 0,
                        WOLFSSL_ALPN_CONTINUE_ON_MISMATCH);
    }
    size_t list_size = 0;
    for (const auto proto : protos) {
        list_size += proto.size();
        if (!proto.empty()) {
            list_size += 1;
        }
    }
    if (list_size <= 1) {
        return;
    }
    char* p = static_cast<char*>(malloc(list_size));
    if (p == nullptr) {
        return;
    }
    auto on_exit = scope_exit([p] { free(p); });
    char* dst = p;
    for (const auto proto : protos) {
        if (proto.empty()) {
            continue;
        }
        dst = std::copy(proto.data(), proto.data() + proto.size(), dst);
        *dst++ = ',';
    }
    // don't include the last comma (list_size - 1)
    const int ret = wolfSSL_UseALPN(as_ssl(ssl_), p,
                                    static_cast<unsigned int>(list_size - 1),
                                    WOLFSSL_ALPN_CONTINUE_ON_MISMATCH);
    std::ignore = ret;
    assert(ret == WOLFSSL_SUCCESS);
}

void wolfssl_impl::open(context& ctx, std::error_code& ec) {
    auto new_ssl = wolfSSL_new(as_ctx(ctx.native_handle()));
    if (!new_ssl) {
        return get_last_ssl_error(ec);
    }
    close();
    ssl_ = new_ssl;
}

void wolfssl_impl::reopen(std::error_code& ec) noexcept {
    if (!ssl_) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    auto ctx = wolfSSL_get_SSL_CTX(as_ssl(ssl_));
    assert(ctx != nullptr);

    auto new_ssl = wolfSSL_new(ctx);
    if (!new_ssl) {
        return get_last_ssl_error(ec);
    }

    destroy_ssl();
    ssl_ = new_ssl;
}

void wolfssl_impl::set_fd(uintptr_t sock_fd, std::error_code& ec) noexcept {
    ec.clear();
    int ret = wolfSSL_set_fd(as_ssl(ssl_), static_cast<int>(sock_fd));
    if (ret != SSL_SUCCESS) {
        ec.assign(ret, ssl_category());
    }
}

void wolfssl_impl::set_bios(ring_bio& rbio, ring_bio& wbio) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        return;
    }
    wolfSSL_SetIOReadCtx(as_ssl(ssl_), &rbio);
    wolfSSL_SetIOWriteCtx(as_ssl(ssl_), &wbio);
}

void wolfssl_impl::set_hostname(std::string_view host) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        return;
    }
    wolfSSL_UseSNI(as_ssl(ssl_), WOLFSSL_SNI_HOST_NAME, host.data(),
                   static_cast<unsigned short>(host.size()));
}

void wolfssl_impl::set_mode(bool is_client) noexcept {
    if (is_client) {
        wolfSSL_set_connect_state(as_ssl(ssl_));
    }
    else {
        wolfSSL_set_accept_state(as_ssl(ssl_));
    }
}

void wolfssl_impl::handshake(engine_state& state,
                             std::error_code& ec) noexcept {
    ec.clear();
    state = engine_state::done;
    int ret = wolfSSL_negotiate(as_ssl(ssl_));
    if (ret != SSL_SUCCESS) {
        ec = last_error(ret, state);
    }
}

bool wolfssl_impl::is_handshake_done() const noexcept {
    if (ssl_ == nullptr) {
        return false;
    }
    return wolfSSL_is_init_finished(as_ssl(ssl_));
}

std::size_t wolfssl_impl::write(const_buffer buff, engine_state& state,
                                std::error_code& ec) noexcept {
    ec.clear();
    state = engine_state::done;
    if (buff.empty()) {
        return 0;
    }
    std::size_t total_size = buff.size();
    while (!ec && !buff.empty()) {
        int n = wolfSSL_write(as_ssl(ssl_), buff.data(),
                              static_cast<int>(buff.size()));
        if (n <= 0) {
            ec = last_error(n, state);
            return total_size - buff.size();
        }
        buff += static_cast<std::size_t>(n);
    }
    return total_size - buff.size();
}

std::size_t wolfssl_impl::read(mutable_buffer buff, engine_state& state,
                               std::error_code& ec) noexcept {
    ec.clear();
    state = engine_state::done;
    if (buff.empty()) {
        return 0;
    }
    std::size_t total_size = buff.size();
    while (!ec && !buff.empty()) {
        int n = wolfSSL_read(as_ssl(ssl_), buff.data(),
                             static_cast<int>(buff.size()));
        if (n <= 0) {
            ec = last_error(n, state);
            return total_size - buff.size();
        }
        buff += static_cast<std::size_t>(n);
    }
    return total_size - buff.size();
}

void wolfssl_impl::shutdown(engine_state& state, std::error_code& ec) noexcept {
    ec.clear();
    state = engine_state::done;
    int ret = wolfSSL_shutdown(as_ssl(ssl_));
    if (ret == WOLFSSL_SUCCESS) {
        return;
    }
    ec = last_error(ret, state);
}

std::error_code wolfssl_impl::last_error(int ret,
                                         engine_state& state) noexcept {
    std::error_code ec;
    int error_code = wolfSSL_get_error(as_ssl(ssl_), ret);
    if (!error_code) {
        state = ssl::engine_state::done;
    }
    else if (error_code == WOLFSSL_ERROR_ZERO_RETURN) {
        ec = io::detail::make_eof_error_code();
        state = ssl::engine_state::error;
    }
    else {
        if (error_code == WOLFSSL_ERROR_WANT_READ) {
            state = ssl::engine_state::want_read;
        }
        else if (error_code == WOLFSSL_ERROR_WANT_WRITE) {
            state = ssl::engine_state::want_write;
        }
        else {
            state = ssl::engine_state::error;
        }
        ec.assign(error_code, ssl_category());
    }
    return ec;
}