#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <rad/async/io_executor.h> // For eof error
#include <rad/net/ssl/openssl_ctx.h>
#include <rad/net/types.h>
#include <rad/random.h>
#include <rad/threading/thread.h>

#include <span>
#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif // _WIN32

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace ssl;
using namespace openssl;
using ssl::engine_state;
using ssl::ring_bio;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define USE_OPENSSL_1_1_VERSION 1
#endif // OPENSSL_VERSION_NUMBER >= 0x10100000L

namespace {
#ifndef USE_OPENSSL_1_1_VERSION

// a movable mutex so that it can be stored in a vector
#ifdef _WIN32
    struct openssl_mutex : noncopyable {
    public:
        openssl_mutex() = default;

        void lock() noexcept {
            AcquireSRWLockExclusive(&mtx);
        }

        void unlock() noexcept {
            ReleaseSRWLockExclusive(&mtx);
        }

    private:
        SRWLOCK mtx = SRWLOCK_INIT;
    };
#else
    struct openssl_mutex : noncopyable {
    public:
        openssl_mutex() = default;

        void lock() noexcept {
            [[maybe_unused]] int result = ::pthread_mutex_lock(&mtx);
            assert(result == 0);
        }

        void unlock() noexcept {
            [[maybe_unused]] int result = ::pthread_mutex_unlock(&mtx);
            assert(result == 0);
        }

    private:
        pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    };
#endif // _WIN32

    void openssl_lock_callback(int mode, int index, const char*, int);

    unsigned long openssl_thread_id_callback() {
        return this_thread::get_id().id();
    }

#endif // !USE_OPENSSL_1_1_VERSION

    struct init_openssl_t {
#ifndef USE_OPENSSL_1_1_VERSION
        std::vector<openssl_mutex> openssl_locks;
#endif // !USE_OPENSSL_1_1_VERSION

        init_openssl_t() {
#ifndef USE_OPENSSL_1_1_VERSION
            SSL_library_init();
            SSL_load_error_strings();
            ERR_load_BIO_strings();
            ERR_load_crypto_strings();
            SSL_load_error_strings();
            OpenSSL_add_all_digests();

            openssl_locks.resize(::CRYPTO_num_locks());

            CRYPTO_set_locking_callback(openssl_lock_callback);
            CRYPTO_set_id_callback(openssl_thread_id_callback);
#endif // !USE_OPENSSL_1_1_VERSION
        }

        SSL_CTX* new_ctx(const SSL_METHOD* method) const noexcept {
            return SSL_CTX_new(method);
        }

        ~init_openssl_t() {
            CRYPTO_set_locking_callback(nullptr);
            CRYPTO_set_id_callback(nullptr);
#ifndef USE_OPENSSL_1_1_VERSION
            FIPS_mode_set(0);
            ENGINE_cleanup();
#endif // !USE_OPENSSL_1_1_VERSION
            CONF_modules_unload(1);
            EVP_cleanup();
            CRYPTO_cleanup_all_ex_data();
#ifdef USE_OPENSSL_1_1_VERSION
            // ERR_remove_thread_state(nullptr);
            CRYPTO_cleanup_all_ex_data();
#else
            ERR_remove_state(0);
#endif // USE_OPENSSL_1_1_VERSION
            ERR_free_strings();
        }
    };

    const init_openssl_t init_openssl;

#ifndef USE_OPENSSL_1_1_VERSION
    void openssl_lock_callback(int mode, int index, const char*, int) {
        auto& mtx = init_openssl.openssl_locks[static_cast<size_t>(index)];
        if (mode & CRYPTO_LOCK) {
            mtx.lock();
        }
        else {
            mtx.unlock();
        }
    }
#endif // !USE_OPENSSL_1_1_VERSION

#ifdef USE_OPENSSL_1_1_VERSION
    constexpr int get_openssl_min_max_proto_version(version v) {
        switch (v) {
        case version::tls_client:
            return 0;
        case version::tls_server:
            return 0;

        case version::tlsv1_client:
            return TLS1_VERSION;
        case version::tlsv1_server:
            return TLS1_VERSION;

        case version::tlsv11_client:
            return TLS1_1_VERSION;
        case version::tlsv11_server:
            return TLS1_1_VERSION;

        case version::tlsv12_client:
            return TLS1_2_VERSION;
        case version::tlsv12_server:
            return TLS1_2_VERSION;

        case version::tlsv13_client:
            return TLS1_3_VERSION;
        case version::tlsv13_server:
            return TLS1_3_VERSION;

        default:
            return 0;
        }
    }
#endif // USE_OPENSSL_1_1_VERSION

    struct bio_free_on_exit : pinned {
        BIO* b = nullptr;

        bio_free_on_exit(BIO* b) : b{b} {
        }

        ~bio_free_on_exit() {
            if (b) {
                BIO_free(b);
            }
        }
    };

    struct x509_free_on_exit : pinned {
        X509* x = nullptr;

        x509_free_on_exit(X509* x) : x{x} {
        }

        ~x509_free_on_exit() {
            if (x) {
                X509_free(x);
            }
        }
    };

    struct evp_key_free_on_exit : pinned {
        EVP_PKEY* k = nullptr;

        evp_key_free_on_exit(EVP_PKEY* k) : k{k} {
        }

        ~evp_key_free_on_exit() {
            if (k) {
                EVP_PKEY_free(k);
            }
        }
    };

    inline bio_free_on_exit make_static_buffer_bio(const_buffer buffer) {
        return {
            BIO_new_mem_buf(buffer.data(), static_cast<int>(buffer.size()))};
    }

    inline std::error_code last_net_error() {
#ifdef _WIN32
        return std::error_code{::WSAGetLastError(), system_category()};
#else
        return std::error_code{errno, system_category()};
#endif // _WIN32
    }

    std::string last_error_str;

    [[maybe_unused]] inline void store_last_error(int e) noexcept {
        return;
        last_error_str.clear();
        if (!e) {
            return;
        }

        if (e == SSL_ERROR_SYSCALL) {
            last_error_str = last_net_error().message();
            return;
        }

        auto b = BIO_new(BIO_s_mem());
        if (b) {
            bio_free_on_exit deleter{b};
            ERR_print_errors(b);
            BUF_MEM buf{};
            BIO_get_mem_data(b, &buf);
            auto buff = buffer(buf.data, static_cast<std::size_t>(buf.length));
            if (buff.size() > 1) {
                last_error_str = (--buff).to_string();
            }
        }
    }

    class ssl_error_category : public std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "ssl";
        }

        [[nodiscard]] std::string message(int condition) const override {
            if (!last_error_str.empty()) {
                return last_error_str;
            }

            const char* error_msg = ERR_error_string(
                static_cast<unsigned long>(condition), nullptr);
            return error_msg ? error_msg : "unkown ssl error";
        }
    };

    const ssl_error_category ssl_error_category_inst;

    SSL_CTX* as_ctx(void* ctx) noexcept {
        return static_cast<SSL_CTX*>(ctx);
    }

    SSL* as_ssl(void* ssl) noexcept {
        return static_cast<SSL*>(ssl);
    }

    inline void get_last_ctx_error(std::error_code& ec) noexcept {
        int error_code = static_cast<int>(ERR_get_error());
        if (error_code == SSL_ERROR_ZERO_RETURN) {
            ec = io::detail::make_eof_error_code();
        }
        else {
            store_last_error(error_code);
            ec.assign(error_code, ssl_category());
        }
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
    int passwd_cb_fn(char* buf, int size, int rwflag, void* userdata) {
        if (userdata == nullptr || size <= 0) {
            return -1;
        }
        const auto cb = static_cast<password_callback_base*>(userdata);
        const std::size_t buf_size = static_cast<std::size_t>(size);
        try {
            auto ret = cb->get_password(buffer(buf, buf_size),
                                        static_cast<password_purpose>(rwflag));
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
    int verify_cb_fn(int preverify_ok, X509_STORE_CTX* x509_ctx) {
        if (!x509_ctx) {
            return 0;
        }

        // https://docs.openssl.org/master/man3/SSL_CTX_set_verify/
        // The openssl docs states that:
        // SSL_get_ex_data_X509_STORE_CTX_idx can be called to get the
        // data index of the current SSL object that is doing the
        // verification.

        const int session_index = SSL_get_ex_data_X509_STORE_CTX_idx();
        if (session_index < 0) {
            return 0;
        }

        SSL* ssl = static_cast<SSL*>(
            X509_STORE_CTX_get_ex_data(x509_ctx, session_index));
        if (ssl == nullptr) {
            return 0;
        }

        SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
        if (ctx == nullptr) {
            return 0;
        }

        auto cb = reinterpret_cast<ssl::verify_callback_base*>(
            SSL_CTX_get_app_data(ctx));
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

    int ring_BIO_write(BIO* b, const char* buff, int len) noexcept {
        if (!b) {
            return 0;
        }
        BIO_clear_retry_flags(b);
        auto rbio = reinterpret_cast<ring_bio*>(BIO_get_app_data(b));
        assert(rbio != nullptr);
        if (rbio == nullptr) {
            return 0;
        }
        if (len <= 0 || !buff) {
            return 0;
        }
        size_t n = static_cast<size_t>(len);
        size_t ret = rbio->put_data(buffer(buff, n));
        if (ret == 0) {
            BIO_set_retry_write(b);
            return -1;
        }
        return static_cast<int>(ret);
    }

    int ring_BIO_read(BIO* b, char* buff, int len) noexcept {
        if (!b) {
            return 0;
        }
        BIO_clear_retry_flags(b);
        auto rbio = reinterpret_cast<ring_bio*>(BIO_get_app_data(b));
        assert(rbio != nullptr);
        if (rbio == nullptr) {
            return 0;
        }
        if (len <= 0 || !buff) {
            return 0;
        }
        size_t n = static_cast<size_t>(len);
        size_t ret = rbio->get_data(buffer(buff, n));
        if (ret == 0) {
            BIO_set_retry_read(b);
            return -1;
        }
        return static_cast<int>(ret);
    }

    int ring_BIO_puts(BIO* b, const char* buf) {
        return ring_BIO_write(b, buf, static_cast<int>(strlen(buf)));
    }

    long ring_BIO_ctrl(BIO* b, int cmd, long larg, void* parg) {
        std::ignore = b;
        std::ignore = larg;
        std::ignore = parg;
        // called with BIO_CTRL_PUSH, BIO_CTRL_POP, BIO_CTRL_FLUSH
        // (until openssl 1.1.1) if cmd is BIO_CTRL_EOF then 0 is
        // returned to indicate not eof (since openssl 3)
        return cmd == BIO_CTRL_EOF ? 0 : 1;
    }

#ifndef USE_OPENSSL_1_1_VERSION
    constexpr int bio_ring_type = 1452 | BIO_TYPE_SOURCE_SINK;

    BIO_METHOD ring_bio_method = {
        bio_ring_type,  // type
        "",             // name
        ring_BIO_write, // bwrite
        ring_BIO_read,  // bread
        ring_BIO_puts,  // bputs
        nullptr,        // bgets
        ring_BIO_ctrl,  // ctrl
        nullptr,        // create
        nullptr,        // destory,
        nullptr         // callback_ctrl
    };

    inline BIO_METHOD* BIO_s_ring() noexcept {
        return &ring_bio_method;
    }
#else

    struct bio_method_deleter {
        void operator()(BIO_METHOD* meth) const noexcept {
            assert(meth != nullptr);
            BIO_meth_free(meth);
        }
    };

    using bio_method_ptr = std::unique_ptr<BIO_METHOD, bio_method_deleter>;

    bio_method_ptr make_bio_s_ring() noexcept {
        BIO_METHOD* meth =
            BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "");
        assert(meth != nullptr);
        if (!meth) {
            std::terminate();
        }
        BIO_meth_set_write(meth, ring_BIO_write);
        BIO_meth_set_read(meth, ring_BIO_read);
        BIO_meth_set_puts(meth, ring_BIO_puts);
        BIO_meth_set_ctrl(meth, ring_BIO_ctrl);
        return bio_method_ptr{meth};
    }

    const bio_method_ptr ring_bio_method = make_bio_s_ring();

    inline const BIO_METHOD* BIO_s_ring() noexcept {
        return ring_bio_method.get();
    }
#endif
} // namespace

const std::error_category& openssl::ssl_category() noexcept {
    return ssl_error_category_inst;
}

context::~context() {
    destroy_ctx();
}

void context::add_verify_certificate(const_buffer cert_buffer,
                                     file_format format,
                                     std::error_code& ec) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    ec.clear();
    void* pass_cb = SSL_CTX_get_default_passwd_cb_userdata(as_ctx(ctx_));
    auto mb = make_static_buffer_bio(cert_buffer);
    if (mb.b) {
        std::size_t loaded = 0;
        std::size_t failed = 0;
        while (1) {
            x509_free_on_exit xcert{
                format == file_format::pem
                    ? PEM_read_bio_X509(mb.b, nullptr, passwd_cb_fn, pass_cb)
                    : d2i_X509_bio(mb.b, nullptr)};
            if (!xcert.x) {
                failed += 1;
                break;
            }
            X509_STORE* certs_store = SSL_CTX_get_cert_store(as_ctx(ctx_));
            if (certs_store == nullptr) {
                failed += 1;
                continue;
            }
            const int ret = X509_STORE_add_cert(certs_store, xcert.x);
            if (ret == 1) {
                loaded += 1;
            }
            else {
                failed += 1;
            }
        }
        if (loaded > 0) {
            return;
        }
        std::ignore = failed;
    }
    return get_last_ctx_error(ec);
}

void context::add_verify_file(const std::string& file,
                              std::error_code& ec) noexcept {
    ec.clear();
    if (SSL_CTX_load_verify_locations(as_ctx(ctx_), file.c_str(), nullptr) ==
        0) {
        return get_last_ctx_error(ec);
    }
}

void context::add_verify_path(const std::string& path,
                              std::error_code& ec) noexcept {
    ec.clear();
    if (SSL_CTX_load_verify_locations(as_ctx(ctx_), nullptr, path.c_str()) ==
        0) {
        return get_last_ctx_error(ec);
    }
}

void context::add_verify_file_path(const std::string& path,
                                   const std::string& file,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (SSL_CTX_load_verify_locations(as_ctx(ctx_), file.c_str(),
                                      path.c_str()) == 0) {
        return get_last_ctx_error(ec);
    }
}

void context::set_default_verify_paths(std::error_code& ec) noexcept {
    ec.clear();
    if (SSL_CTX_set_default_verify_paths(as_ctx(ctx_)) == 0) {
        return get_last_ctx_error(ec);
    }
}

void context::use_own_certificate(const_buffer cert_buffer, file_format format,
                                  std::error_code& ec) noexcept {
    ec.clear();
    assert(format == file_format::asn1 || format == file_format::pem);

    if (format == file_format::asn1) {
        if (SSL_CTX_use_certificate_ASN1(
                as_ctx(ctx_), static_cast<int>(cert_buffer.size()),
                reinterpret_cast<const unsigned char*>(cert_buffer.data())) !=
            1) {
            get_last_ctx_error(ec);
        }
    }
    else if (format == file_format::pem) {
        auto mb = make_static_buffer_bio(cert_buffer);
        if (mb.b) {
            void* pass_cb =
                SSL_CTX_get_default_passwd_cb_userdata(as_ctx(ctx_));
            x509_free_on_exit xcert{
                PEM_read_bio_X509(mb.b, nullptr, passwd_cb_fn, pass_cb)};
            if (xcert.x) {
                if (SSL_CTX_use_certificate(as_ctx(ctx_), xcert.x) == 1) {
                    return;
                }
            }
        }
        return get_last_ctx_error(ec);
    }
}

void context::use_own_certificate_file(const std::string& filename,
                                       file_format format,
                                       std::error_code& ec) noexcept {
    const int oformat =
        format == file_format::asn1 ? SSL_FILETYPE_ASN1 : SSL_FILETYPE_PEM;
    const int ret =
        SSL_CTX_use_certificate_file(as_ctx(ctx_), filename.c_str(), oformat);
    if (ret != 1) {
        get_last_ctx_error(ec);
    }
}

void context::use_private_key(const_buffer key_buffer, file_format format,
                              std::error_code& ec) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    ec.clear();
    if (format == file_format::asn1) {
        // pk = 0 for auto type detection
        if (SSL_CTX_use_PrivateKey_ASN1(
                0, as_ctx(ctx_), key_buffer.data_as<const unsigned char>(),
                static_cast<int>(key_buffer.size())) != 1) {
            get_last_ctx_error(ec);
        }
    }
    else {
        auto mb = make_static_buffer_bio(key_buffer);
        if (mb.b) {
            void* pass_cb =
                SSL_CTX_get_default_passwd_cb_userdata(as_ctx(ctx_));
            evp_key_free_on_exit pkey{
                PEM_read_bio_PrivateKey(mb.b, nullptr, passwd_cb_fn, pass_cb)};
            if (pkey.k) {
                if (SSL_CTX_use_PrivateKey(as_ctx(ctx_), pkey.k) == 1) {
                    return;
                }
            }
        }
        return get_last_ctx_error(ec);
    }
}

void context::use_private_key_file(const std::string& filename,
                                   file_format format,
                                   std::error_code& ec) noexcept {
    const int oformat =
        format == file_format::asn1 ? SSL_FILETYPE_ASN1 : SSL_FILETYPE_PEM;
    const int ret =
        SSL_CTX_use_PrivateKey_file(as_ctx(ctx_), filename.c_str(), oformat);
    if (ret != 1) {
        get_last_ctx_error(ec);
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
    SSL_CTX_set_verify_depth(as_ctx(ctx_), static_cast<int>(depth));
}

void context::set_verify_cb(const verify_mode* mode,
                            std::unique_ptr<verify_callback_base> cb) noexcept {
    if (ctx_ == nullptr) {
        return;
    }
    if (cb != nullptr) {
        remove_verify_cb();
        SSL_CTX_set_app_data(as_ctx(ctx_), cb.release());
    }
    const int overify = mode == nullptr ? SSL_CTX_get_verify_mode(as_ctx(ctx_))
                        : *mode == verify_mode::none ? SSL_VERIFY_NONE
                        : *mode == verify_mode::optional
                            ? SSL_VERIFY_NONE
                            : SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;

    SSL_CTX_set_verify(as_ctx(ctx_), overify,
                       cb == nullptr ? SSL_CTX_get_verify_callback(as_ctx(ctx_))
                                     : verify_cb_fn);
}

void context::set_passwd_cb(
    std::unique_ptr<password_callback_base> cb) noexcept {
    if (ctx_ == nullptr) {
        return;
    }
    remove_passwd_cb();
    SSL_CTX_set_default_passwd_cb(as_ctx(ctx_), passwd_cb_fn);
    SSL_CTX_set_default_passwd_cb_userdata(as_ctx(ctx_), cb.release());
}

void context::remove_verify_cb() noexcept {
    assert(ctx_ != nullptr);
    void* cb = SSL_CTX_get_app_data(as_ctx(ctx_));
    if (cb != nullptr) {
        SSL_CTX_set_app_data(as_ctx(ctx_), nullptr);
        delete static_cast<verify_callback_base*>(cb);
    }
}

void context::remove_passwd_cb() noexcept {
    assert(ctx_ != nullptr);
#ifdef USE_OPENSSL_1_1_VERSION
    void* cb = ::SSL_CTX_get_default_passwd_cb_userdata(as_ctx(ctx_));
#else
    void* cb = as_ctx(ctx_)->default_passwd_callback_userdata;
#endif // USE_OPENSSL_1_1_VERSION
    if (cb != nullptr) {
        ::SSL_CTX_set_default_passwd_cb_userdata(as_ctx(ctx_), nullptr);
        delete static_cast<password_callback_base*>(cb);
    }
}

void context::open(version v, std::error_code& ec) noexcept {
    const SSL_METHOD* ssl_method = nullptr;
    if (is_client_version(v)) {
        ssl_method = SSLv23_client_method();
    }
    else {
        ssl_method = SSLv23_server_method();
    }

    SSL_CTX* new_ctx = init_openssl.new_ctx(ssl_method);
    if (new_ctx) {
        if (ctx_) {
            SSL_CTX_free(as_ctx(ctx_));
        }
        ctx_ = new_ctx;
#ifdef USE_OPENSSL_1_1_VERSION
        const int openssl_protocol_verion =
            get_openssl_min_max_proto_version(v);
        if (openssl_protocol_verion != 0) {
            SSL_CTX_set_min_proto_version(new_ctx, openssl_protocol_verion);
            SSL_CTX_set_max_proto_version(new_ctx, openssl_protocol_verion);
        }
#endif // USE_OPENSSL_1_1_VERSION
       // free buffers for idle sessions
        SSL_CTX_set_mode(new_ctx, SSL_MODE_RELEASE_BUFFERS);
        return;
    }

    get_last_ctx_error(ec);
}

std::unique_ptr<ssl::ssl_impl_base>
context::make_generic_impl(std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return nullptr;
    }
    openssl_impl* impl_ptr = new (std::nothrow) openssl_impl;
    if (impl_ptr == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return nullptr;
    }
    std::unique_ptr<openssl_impl> impl{impl_ptr};
    impl->open(*this, ec);
    if (ec) {
        return nullptr;
    }
    return impl;
}

void context::free_ctx_no_check() noexcept {
    assert(ctx_ != nullptr);
    remove_verify_cb();
    remove_passwd_cb();
    SSL_CTX_free(as_ctx(ctx_));
}

options context::set_options(options opts) noexcept {
#ifdef USE_OPENSSL_1_1_VERSION
    unsigned long o = static_cast<unsigned long>(
        opts & ~(options::single_dh_use | options::no_sslv2));
#else
    long o = static_cast<long>(opts);
#endif // USE_OPENSSL_1_1_VERSION
    return static_cast<options>(SSL_CTX_set_options(as_ctx(ctx_), o));
}

options context::clear_options(options opts) noexcept {
#ifdef USE_OPENSSL_1_1_VERSION
    unsigned long o = static_cast<unsigned long>(opts);
#else
    long o = static_cast<long>(opts);
#endif // USE_OPENSSL_1_1_VERSION
    return static_cast<options>(SSL_CTX_clear_options(as_ctx(ctx_), o));
}

options context::get_options() const noexcept {
    return static_cast<options>(SSL_CTX_get_options(as_ctx(ctx_)));
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

void* openssl_impl::native_handle() const noexcept {
    return ssl_;
}

std::string_view openssl_impl::version() const noexcept {
    if (ssl_ == nullptr) {
        return "";
    }
    const char* v = SSL_get_version(as_ssl(ssl_));
    return v != nullptr ? std::string_view{v} : std::string_view{};
}

void openssl_impl::set_alpn_protocols(
    std::span<const std::string_view> protos) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        return;
    }
    if (protos.empty()) {
        SSL_set_alpn_protos(as_ssl(ssl_), nullptr, 0);
        return;
    }
    size_t list_size = 0;
    for (const auto proto : protos) {
        if (proto.size() > std::numeric_limits<uint8_t>::max()) {
            SSL_set_alpn_protos(as_ssl(ssl_), nullptr, 0);
            return;
        }
        list_size += proto.size();
        if (!proto.empty()) {
            list_size += 1;
        }
    }
    if (list_size <= 1) {
        SSL_set_alpn_protos(as_ssl(ssl_), nullptr, 0);
        return;
    }
    uint8_t* p = new (std::nothrow) uint8_t[list_size];
    if (p == nullptr) {
        return;
    }
    auto on_exit = scope_exit([p] { delete[] p; });
    uint8_t* dst = p;
    for (const auto proto : protos) {
        if (proto.empty()) {
            continue;
        }
        *dst++ = static_cast<uint8_t>(proto.size());
        dst = std::copy(proto.data(), proto.data() + proto.size(), dst);
    }
    int ret = SSL_set_alpn_protos(as_ssl(ssl_), p,
                                  static_cast<unsigned int>(list_size));
    if (ret != 0) {
        assert(false && "SSL_set_alpn_protos failed !!!\n");
    }
}

std::string_view openssl_impl::get_alpn_protocol() const noexcept {
    if (ssl_ == nullptr) {
        return "";
    }
    const unsigned char* data = nullptr;
    unsigned int len = 0;
    SSL_get0_alpn_selected(as_ssl(ssl_), &data, &len);
    if (len == 0 || data == nullptr) {
        return {};
    }
    return std::string_view{reinterpret_cast<const char*>(data), len};
}

void openssl_impl::open(context_base& ctx, std::error_code& ec) noexcept {
    open(static_cast<context&>(ctx), ec);
}

void openssl_impl::open(context_type& ctx, std::error_code& ec) noexcept {
    ec.clear();
    auto new_ssl = SSL_new(as_ctx(ctx.native_handle()));
    if (!new_ssl) {
        return get_last_ctx_error(ec);
    }
    destroy_ssl();
#ifndef USE_OPENSSL_1_1_VERSION
    new_ssl->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
#else
    SSL_set_options(new_ssl, SSL_OP_NO_RENEGOTIATION);
#endif
    SSL_set_mode(new_ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(new_ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    ssl_ = new_ssl;
}

void openssl_impl::reopen(std::error_code& ec) noexcept {
    if (!ssl_) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    SSL_CTX* ctx = SSL_get_SSL_CTX(as_ssl(ssl_));
    assert(ctx != nullptr);

    auto new_ssl = SSL_new(ctx);
    if (!new_ssl) {
        return get_last_ctx_error(ec);
    }
#ifndef USE_OPENSSL_1_1_VERSION
    new_ssl->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
#else
    SSL_set_options(new_ssl, SSL_OP_NO_RENEGOTIATION);
#endif
    SSL_set_mode(new_ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(new_ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    destroy_ssl();
    ssl_ = new_ssl;
}

void openssl_impl::set_fd(uintptr_t sock, std::error_code& ec) noexcept {
    if (SSL_set_fd(as_ssl(ssl_), static_cast<int>(sock)) == 0) {
        /*return get_last_ssl_error(ec)*/ ((void)0);
    }
}

void openssl_impl::set_bios(ring_bio& rbio, ring_bio& wbio) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        return;
    }
    BIO* rb = BIO_new(BIO_s_ring());
    BIO* wb = BIO_new(BIO_s_ring());
    assert(rb != nullptr && wb != nullptr);
#ifndef USE_OPENSSL_1_1_VERSION
    rb->init = wb->init = 1;
#endif // !USE_OPENSSL_1_1_VERSION
    BIO_set_app_data(rb, &rbio);
    BIO_set_app_data(wb, &wbio);
    SSL_set_bio(as_ssl(ssl_), rb, wb);
}

void openssl_impl::set_hostname(std::string_view host) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        return;
    }
    char zhost_buff[256 + 1];
    std::string zhost_string;
    const char* host_ptr = zhost_buff;
    if (host.size() < sizeof(zhost_buff)) {
        memcpy(zhost_buff, host.data(), host.size());
        zhost_buff[host.size()] = 0;
    }
    else {
        zhost_string = host;
        host_ptr = zhost_string.c_str();
    }
    SSL_set_tlsext_host_name(as_ssl(ssl_), host_ptr);
}

void openssl_impl::handshake(engine_state& state,
                             std::error_code& ec) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    ec.clear();
    state = engine_state::done;
    int ret = SSL_do_handshake(as_ssl(ssl_));
    if (ret != 1) {
        ec = last_error(ret, state);
    }
}

bool openssl_impl::is_handshake_done() const noexcept {
    return SSL_is_init_finished(as_ssl(ssl_));
}

std::size_t openssl_impl::write(const_buffer buff, engine_state& state,
                                std::error_code& ec) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    ec.clear();
    state = engine_state::done;
    if (buff.empty()) {
        return 0;
    }
    std::size_t total_size = buff.size();
    while (!ec && !buff.empty()) {
        int n =
            SSL_write(as_ssl(ssl_), buff.data(), static_cast<int>(buff.size()));
        if (n <= 0) {
            ec = last_error(n, state);
            return total_size - buff.size();
        }
        buff += static_cast<std::size_t>(n);
    }
    return total_size - buff.size();
}

std::size_t openssl_impl::read(mutable_buffer buff, engine_state& state,
                               std::error_code& ec) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    ec.clear();
    state = engine_state::done;
    if (buff.empty()) {
        return 0;
    }
    std::size_t total_size = buff.size();
    while (!ec && !buff.empty()) {
        int n =
            SSL_read(as_ssl(ssl_), buff.data(), static_cast<int>(buff.size()));
        if (n <= 0) {
            ec = last_error(n, state);
            return total_size - buff.size();
        }
        buff += static_cast<std::size_t>(n);
    }
    return total_size - buff.size();
}

void openssl_impl::shutdown(engine_state& state, std::error_code& ec) noexcept {
    assert(ssl_ != nullptr);
    if (ssl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    ec.clear();
    state = engine_state::done;
    int ret = SSL_shutdown(as_ssl(ssl_));
    if (ret >= 0) {
        return;
    }
    ec = last_error(ret, state);
}

void openssl_impl::set_mode(bool is_client) noexcept {
    if (is_client) {
        SSL_set_connect_state(as_ssl(ssl_));
    }
    else {
        SSL_set_accept_state(as_ssl(ssl_));
    }
}

std::error_code openssl_impl::last_error(int ret,
                                         engine_state& state) noexcept {
    std::error_code ec;
    int error_code = SSL_get_error(as_ssl(ssl_), ret);
    if (!error_code) {
        state = engine_state::done;
    }
    else if (error_code == SSL_ERROR_ZERO_RETURN) {
        ec = io::detail::make_eof_error_code();
        state = engine_state::error;
    }
    else {
        if (error_code == SSL_ERROR_WANT_READ) {
            state = engine_state::want_read;
        }
        else if (error_code == SSL_ERROR_WANT_WRITE) {
            state = engine_state::want_write;
        }
        else {
            state = engine_state::error;
        }
        store_last_error(error_code);
        ec.assign(error_code, ssl_category());
    }
    return ec;
}

void openssl_impl::free_ssl_no_check() noexcept {
    SSL_free(as_ssl(ssl_));
}