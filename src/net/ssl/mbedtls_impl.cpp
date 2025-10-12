#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <rad/async/io_executor.h> // For eof error
#include <rad/function_view.h>
#include <rad/io/files.h>
#include <rad/net/ssl/mbedtls_ctx.h>

#include <algorithm>
#include <filesystem>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincrypt.h>
#endif // _WIN32

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace ssl;
using namespace mbedtls;

namespace {
    class ssl_error_category : public std::error_category {
        const char* name() const noexcept override {
            return "ssl";
        }

        std::string message(int condition) const override {
            std::array<char, 1024> error_str;
            ::mbedtls_strerror(condition, error_str.data(), error_str.size());
            return std::string{error_str.data()};
        }
    };

    const ssl_error_category ssl_error_category_inst;

    const char* null_alpn = nullptr;

    struct free_mbedtls_x509_crt {
        void operator()(mbedtls_x509_crt* crt) const noexcept {
            ::mbedtls_x509_crt_free(crt);
            ::operator delete(crt);
        }
    };

    struct free_mbedtls_pk_context {
        void operator()(mbedtls_pk_context* ctx) const noexcept {
            ::mbedtls_pk_free(ctx);
            ::operator delete(ctx);
        }
    };

    struct malloc_deleter {
        void operator()(uint8_t* p) const noexcept {
            ::free(p);
        }
    };

    void get_last_ssl_error(int ret, engine_state& state,
                            std::error_code& ec) noexcept {
        if (ret == 0) {
            state = engine_state::done;
            return;
        }
        else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            state = engine_state::want_write;
        }
        else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
            state = engine_state::want_read;
        }
        else {
            state = engine_state::error;
        }
        ec.assign(ret, ssl_error_category_inst);
    }

    bool maybe_pem(const_buffer buff) {
        if (buff.empty()) {
            return false;
        }
        std::string_view buff_str = buff.to_string_view();
        if (buff_str.find("-----") != std::string_view::npos) {
            return true;
        }
        return false;
    }

    bool add_null_if_required(
        const_buffer& buff,
        std::unique_ptr<uint8_t, malloc_deleter>& heap_buff) noexcept {
        if (!maybe_pem(buff)) {
            return true;
        }
        auto view = buff.to_span<const uint8_t>();
        if (view.back() == '\0') {
            return true;
        }
        uint8_t* buff_ptr = static_cast<uint8_t*>(::malloc(view.size() + 1));
        if (buff_ptr == nullptr) {
            return false;
        }
        buff_ptr[view.size()] = 0;
        heap_buff.reset(buff_ptr);
        std::copy(view.begin(), view.end(), heap_buff.get());
        buff = buffer(heap_buff.get(), view.size() + 1);
        return true;
    }

    void load_path_certificates(const std::string& path,
                                function_view<void(const_buffer)> on_cert) {
        namespace fs = std::filesystem;
        namespace files = io::files;
        if (!fs::exists(path)) {
            return;
        }
        for (auto&& filepath : fs::directory_iterator{path}) {
            std::error_code fec;
            files::file cert_file;
            cert_file.open(filepath.path().native(), files::open_mode::existing,
                           files::access::read, files::share_mode::read,
                           files::attributes::normal, fec);
            if (fec) {
                continue;
            }
            uint64_t file_size_u64 = cert_file.size(fec);
            if (fec || file_size_u64 >= std::numeric_limits<size_t>::max()) {
                continue;
            }
            std::size_t file_size = static_cast<std::size_t>(file_size_u64);
            uint8_t* buff_ptr = static_cast<uint8_t*>(::malloc(file_size + 1));
            if (buff_ptr == nullptr) {
                fec = std::make_error_code(std::errc::not_enough_memory);
                continue;
            }
            // null terminate!
            buff_ptr[file_size] = 0;
            std::unique_ptr<uint8_t, malloc_deleter> buff{buff_ptr};
            cert_file.read_all(buffer(buff.get(), file_size), fec);
            if (fec) {
                continue;
            }
            cert_file.close();
            if (maybe_pem(buffer(buff.get(), file_size))) {
                // include the null terminate for PEM!
                file_size += 1;
            }
            on_cert(buffer(buff.get(), file_size));
        }
    }

#ifdef _WIN32
    void load_windows_certificates(
        function_view<void(const_buffer)> on_cert) noexcept {
        const wchar_t* stores_names[2] = {L"ROOT", L"CA"};
        for (const wchar_t* name : stores_names) {
            HCERTSTORE store = ::CertOpenSystemStoreW(0, name);
            if (store == nullptr) {
                continue;
            }
            auto on_exit = scope_exit([store] {
                ::CertCloseStore(store, CERT_CLOSE_STORE_FORCE_FLAG);
            });
            PCCERT_CONTEXT cert_ctx = nullptr;
            while (1) {
                cert_ctx = ::CertEnumCertificatesInStore(store, cert_ctx);
                if (cert_ctx == nullptr) {
                    break;
                }
                if (cert_ctx->dwCertEncodingType == X509_ASN_ENCODING) {
                    on_cert(buffer(cert_ctx->pbCertEncoded,
                                   cert_ctx->cbCertEncoded));
                }
            }
        }
    }
#endif // _WIN32
} // namespace

const std::error_category& mbedtls::ssl_category() noexcept {
    return ssl_error_category_inst;
}

struct context::context_inner : noncopyable, public trackable {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_ssl_config cfg;
    std::unique_ptr<verify_callback_base> verify_cb;
    std::unique_ptr<password_callback_base> password_cb;
    std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt> trusted_certs_;
    std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt> own_cert_;
    std::unique_ptr<mbedtls_pk_context, free_mbedtls_pk_context> own_key_;
    std::vector<std::string> alpn_vec;
    std::vector<const char*> alpn_ptrs;
    std::string password_buff;
    std::size_t max_depth = std::numeric_limits<int>::max();

    context_inner() {
        psa_status_t status = ::psa_crypto_init();
        assert(status == PSA_SUCCESS);
        std::ignore = status;
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_ssl_config_init(&cfg);
        int ret = ::mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                          nullptr, 0);
        assert(ret == 0);
        std::ignore = ret;
        ::mbedtls_ssl_conf_rng(&cfg, mbedtls_ctr_drbg_random, &drbg);
    }

    ~context_inner() {
        mbedtls_ssl_config_free(&cfg);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }

    void set_alpn_protocols(std::span<const std::string_view> protos) {
        ::mbedtls_ssl_conf_alpn_protocols(&cfg, &null_alpn);
        alpn_ptrs.clear();
        alpn_vec.clear();
        if (protos.empty()) {
            return;
        }
        alpn_vec.reserve(protos.size());
        for (std::string_view proto : protos) {
            if (proto.empty()) {
                continue;
            }
            if (std::find(alpn_vec.begin(), alpn_vec.end(), proto) !=
                alpn_vec.end()) {
                continue;
            }
            alpn_vec.push_back(std::string{proto});
        }
        if (alpn_vec.empty()) {
            return;
        }
        alpn_ptrs.reserve(alpn_vec.size() + 1);
        for (const auto& proto : alpn_vec) {
            alpn_ptrs.push_back(proto.c_str());
        }
        alpn_ptrs.push_back(nullptr);
        ::mbedtls_ssl_conf_alpn_protocols(&cfg, alpn_ptrs.data());
    }

    /*!
     * @brief MbedTLS verify callback.
     * @param data user context pointer (points to context_inner).
     * @param crt The certificate.
     * @param depth Depth in the chain (0 = leaf certificate).
     * @param flags Pointer to verification flags (bitmask of
     * MBEDTLS_X509_BADCERT_* codes).
     * @return 0 to continue verification, non-zero to abort
     */
    static int verify_callback(void* data, mbedtls_x509_crt* crt, int depth,
                               uint32_t* flags) noexcept {
        if (data == nullptr) {
            return -1;
        }
        context_inner* ctx = static_cast<context_inner*>(data);
        if (depth > static_cast<int>(ctx->max_depth)) {
            *flags |= MBEDTLS_X509_BADCERT_OTHER;
            return -1;
        }
        if (ctx->verify_cb == nullptr) {
            return -1;
        }
        bool ok = false;
        try {
            ok = ctx->verify_cb->verify(*flags == 0, crt);
        }
        catch (...) {
            ok = false;
        }
        if (ok) {
            *flags = 0;
            return 0;
        }
        return -1;
    }

    const_buffer get_password_buffer(password_purpose purpose) {
        if (password_cb == nullptr) {
            return {};
        }
        password_buff.resize(1024);
        auto ret =
            password_cb->get_password(buffer(password_buff) - 1, purpose);
        if (!ret.has_value() || *ret >= password_buff.size() || *ret == 0) {
            password_buff.clear();
            return {};
        }
        password_buff.resize(*ret + 1);
        password_buff.back() = '\0';
        return buffer(password_buff) - 1;
    }

    const char* get_password_null_terminated(password_purpose purpose) {
        return get_password_buffer(purpose).data_as<const char>();
    }

    void clear_password() noexcept {
        std::memset(password_buff.data(), 0, password_buff.size());
        password_buff.clear();
    }

    static std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt>
    load_cert(const_buffer cert_buffer, std::error_code& ec) noexcept {
        ec.clear();
        std::unique_ptr<uint8_t, malloc_deleter> heap_buff;
        if (!add_null_if_required(cert_buffer, heap_buff)) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        mbedtls_x509_crt* cert_ptr = new (std::nothrow) mbedtls_x509_crt;
        if (cert_ptr == nullptr) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt> cert{cert_ptr};
        ::mbedtls_x509_crt_init(cert.get());
        const int ret = ::mbedtls_x509_crt_parse(
            cert_ptr, cert_buffer.data_as<const unsigned char>(),
            cert_buffer.size());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return nullptr;
        }
        return cert;
    }

    static std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt>
    load_cert(const std::string& filename, std::error_code& ec) noexcept {
        ec.clear();
        mbedtls_x509_crt* cert_ptr = new (std::nothrow) mbedtls_x509_crt;
        if (cert_ptr == nullptr) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt> cert{cert_ptr};
        ::mbedtls_x509_crt_init(cert.get());
        const int ret =
            ::mbedtls_x509_crt_parse_file(cert_ptr, filename.c_str());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return nullptr;
        }
        return cert;
    }

    std::unique_ptr<mbedtls_pk_context, free_mbedtls_pk_context>
    load_key(const_buffer key_buffer, std::error_code& ec) noexcept {
        ec.clear();
        std::unique_ptr<uint8_t, malloc_deleter> heap_buff;
        if (!add_null_if_required(key_buffer, heap_buff)) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        mbedtls_pk_context* pk_ctx_ptr = new (std::nothrow) mbedtls_pk_context;
        if (pk_ctx_ptr == nullptr) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        ::mbedtls_pk_init(pk_ctx_ptr);
        std::unique_ptr<mbedtls_pk_context, free_mbedtls_pk_context> pk_ctx{
            pk_ctx_ptr};

        auto password_buff = get_password_buffer(password_purpose::decrypting);

        int ret = ::mbedtls_pk_parse_key(
            pk_ctx.get(), key_buffer.data_as<const unsigned char>(),
            key_buffer.size(), password_buff.data_as<const unsigned char>(),
            password_buff.size(), mbedtls_ctr_drbg_random, &drbg);

        clear_password();

        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return nullptr;
        }
        return pk_ctx;
    }

    std::unique_ptr<mbedtls_pk_context, free_mbedtls_pk_context>
    load_key(const std::string& filename, std::error_code& ec) noexcept {
        ec.clear();
        mbedtls_pk_context* pk_ctx_ptr = new (std::nothrow) mbedtls_pk_context;
        if (pk_ctx_ptr == nullptr) {
            ec = std::make_error_code(std::errc::not_enough_memory);
            return nullptr;
        }
        ::mbedtls_pk_init(pk_ctx_ptr);
        std::unique_ptr<mbedtls_pk_context, free_mbedtls_pk_context> pk_ctx{
            pk_ctx_ptr};

        const char* password_buff =
            get_password_null_terminated(password_purpose::decrypting);

        int ret = ::mbedtls_pk_parse_keyfile(pk_ctx.get(), filename.c_str(),
                                             password_buff,
                                             mbedtls_ctr_drbg_random, &drbg);

        clear_password();

        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return nullptr;
        }
        return pk_ctx;
    }

    static void add_trusted_certificate(
        std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt>& root,
        const_buffer cert_buffer, std::error_code& ec) noexcept {
        ec.clear();
        mbedtls_x509_crt* cert_ptr = nullptr;
        std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt> new_cert;
        if (root == nullptr) {
            cert_ptr = new (std::nothrow) mbedtls_x509_crt;
            if (cert_ptr == nullptr) {
                ec = std::make_error_code(std::errc::not_enough_memory);
                return;
            }
            ::mbedtls_x509_crt_init(cert_ptr);
            new_cert.reset(cert_ptr);
        }
        else {
            cert_ptr = root.get();
        }
        const int ret = ::mbedtls_x509_crt_parse(
            cert_ptr, cert_buffer.data_as<const unsigned char>(),
            cert_buffer.size());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return;
        }
        if (root == nullptr) {
            root = std::move(new_cert);
        }
    }

    static void add_trusted_certificate(
        std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt>& root,
        const std::string& filename, std::error_code& ec) noexcept {
        ec.clear();
        mbedtls_x509_crt* cert_ptr = nullptr;
        std::unique_ptr<mbedtls_x509_crt, free_mbedtls_x509_crt> new_cert;
        if (root == nullptr) {
            cert_ptr = new (std::nothrow) mbedtls_x509_crt;
            if (cert_ptr == nullptr) {
                ec = std::make_error_code(std::errc::not_enough_memory);
                return;
            }
            ::mbedtls_x509_crt_init(cert_ptr);
            new_cert.reset(cert_ptr);
        }
        else {
            cert_ptr = root.get();
        }
        const int ret =
            ::mbedtls_x509_crt_parse_file(cert_ptr, filename.c_str());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return;
        }
        if (root == nullptr) {
            root = std::move(new_cert);
        }
    }
};

context::context(version v) {
    open(v);
}

void context::open(version v, std::error_code& ec) noexcept {
    ec.clear();
    int ctx_direction =
        is_client_version(v) ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER;
    if (ec) {
        return;
    }
    context_inner* new_ctx_ptr = new (std::nothrow) context_inner;
    if (new_ctx_ptr == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return;
    }
    std::unique_ptr<context_inner> ctx{new_ctx_ptr};
    ::mbedtls_ssl_config_defaults(&ctx->cfg, ctx_direction,
                                  MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT);
    ::mbedtls_ssl_conf_renegotiation(&ctx->cfg,
                                     MBEDTLS_SSL_RENEGOTIATION_DISABLED);
    ctx_ = std::move(ctx);
}

context::~context() = default;

void context::close() noexcept {
    ctx_.reset();
}

// static
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

std::unique_ptr<ssl_impl_base>
context::make_generic_impl(std::error_code& ec) noexcept {
    return std::make_unique<mbedtls_impl>(*this);
}

void* context::native_handle() const noexcept {
    if (ctx_ != nullptr) {
        return &ctx_->cfg;
    }
    return nullptr;
}

void context::add_verify_certificate(const_buffer cert_buffer,
                                     file_format format,
                                     std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    std::unique_ptr<uint8_t, malloc_deleter> heap_buff;
    if (!add_null_if_required(cert_buffer, heap_buff)) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return;
    }
    const bool had_chain = ctx_->trusted_certs_ != nullptr;
    context_inner::add_trusted_certificate(ctx_->trusted_certs_, cert_buffer,
                                           ec);
    if (!ec && !had_chain) {
        ::mbedtls_ssl_conf_ca_chain(&ctx_->cfg, ctx_->trusted_certs_.get(),
                                    nullptr);
    }
}

void context::add_verify_file(const std::string& file,
                              std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    const bool had_chain = ctx_->trusted_certs_ != nullptr;
    context_inner::add_trusted_certificate(ctx_->trusted_certs_, file, ec);
    if (!ec && !had_chain) {
        ::mbedtls_ssl_conf_ca_chain(&ctx_->cfg, ctx_->trusted_certs_.get(),
                                    nullptr);
    }
}

std::size_t context::add_verify_path(const std::string& path,
                                     std::error_code& ec) {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    const bool had_chain = ctx_->trusted_certs_ != nullptr;
    size_t parsed = 0;
    size_t total_files = 0;
    std::error_code last_parse_ec;
    load_path_certificates(path, [&](const_buffer cert_buffer) {
        total_files += 1;
        std::error_code parse_ec;
        context_inner::add_trusted_certificate(ctx_->trusted_certs_,
                                               cert_buffer, parse_ec);
        if (!parse_ec) {
            parsed += 1;
        }
        else {
            last_parse_ec = parse_ec;
        }
    });
    if (parsed == 0 && total_files > 0) {
        ec = last_parse_ec;
        return 0;
    }
    if (!ec && !had_chain) {
        ::mbedtls_ssl_conf_ca_chain(&ctx_->cfg, ctx_->trusted_certs_.get(),
                                    nullptr);
    }
    return parsed;
}

void context::set_default_verify_paths(std::error_code& ec) {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    std::size_t loaded_count = 0;
    std::error_code last_load_ec;
    const bool had_chain = ctx_->trusted_certs_ != nullptr;
#ifdef _WIN32
    load_windows_certificates([&](const_buffer cert_buffer) {
        if (cert_buffer.empty()) {
            return;
        }
        std::error_code load_ec;
        context_inner::add_trusted_certificate(ctx_->trusted_certs_,
                                               cert_buffer, load_ec);
        if (load_ec) {
            last_load_ec = load_ec;
        }
        else {
            loaded_count += 1;
        }
    });
#elif defined(__APPLE__)
    // not implemented yet.
#elif defined(__unix__)
    // This list is taken from wolfssl
    const char* system_ca_dirs[] = {
#if defined(__ANDROID__) || defined(ANDROID)
        "/system/etc/security/cacerts" /* Android */
#else
        "/etc/ssl/certs",                   /* Debian, Ubuntu, Gentoo, others */
        "/etc/pki/ca-trust/source/anchors", /* Fedora, RHEL */
        "/etc/pki/tls/certs"                /* Older RHEL */
#endif
    };
    for (const char* path : system_ca_dirs) {
        load_path_certificates(path, [&](const_buffer cert_buffer) {
            std::error_code load_ec;
            context_inner::add_trusted_certificate(ctx_->trusted_certs_,
                                                   cert_buffer, load_ec);
            if (load_ec) {
                last_load_ec = load_ec;
            }
            else {
                loaded_count += 1;
            }
        });
    }
#endif // _WIN32
    if (loaded_count == 0 && last_load_ec) {
        ec = last_load_ec;
    }
    if (!had_chain && ctx_->trusted_certs_ != nullptr) {
        ::mbedtls_ssl_conf_ca_chain(&ctx_->cfg, ctx_->trusted_certs_.get(),
                                    nullptr);
    }
}

void context::use_own_certificate_private_key(const_buffer cert_buffer,
                                              const_buffer key_buffer,
                                              std::error_code& ec) {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto cert = context_inner::load_cert(cert_buffer, ec);
    if (cert == nullptr) {
        return;
    }
    auto key = ctx_->load_key(key_buffer, ec);
    if (key == nullptr) {
        return;
    }
    const int ret =
        ::mbedtls_ssl_conf_own_cert(&ctx_->cfg, cert.get(), key.get());
    if (ret != 0) {
        ec.assign(ret, ssl_category());
        return;
    }
    ctx_->own_cert_ = std::move(cert);
    ctx_->own_key_ = std::move(key);
}

void context::use_own_certificate_private_key(const std::string& cert_file,
                                              const_buffer key_buffer,
                                              std::error_code& ec) {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto cert = context_inner::load_cert(cert_file, ec);
    if (cert == nullptr) {
        return;
    }
    auto key = ctx_->load_key(key_buffer, ec);
    if (key == nullptr) {
        return;
    }
    const int ret =
        ::mbedtls_ssl_conf_own_cert(&ctx_->cfg, cert.get(), key.get());
    if (ret != 0) {
        ec.assign(ret, ssl_category());
        return;
    }
    ctx_->own_cert_ = std::move(cert);
    ctx_->own_key_ = std::move(key);
}

void context::use_own_certificate_private_key(const_buffer cert_buffer,
                                              const std::string& key_file,
                                              std::error_code& ec) {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto cert = context_inner::load_cert(cert_buffer, ec);
    if (cert == nullptr) {
        return;
    }
    auto key = ctx_->load_key(key_file, ec);
    if (key == nullptr) {
        return;
    }
    const int ret =
        ::mbedtls_ssl_conf_own_cert(&ctx_->cfg, cert.get(), key.get());
    if (ret != 0) {
        ec.assign(ret, ssl_category());
        return;
    }
    ctx_->own_cert_ = std::move(cert);
    ctx_->own_key_ = std::move(key);
}

void context::use_own_certificate_private_key(const std::string& cert_file,
                                              const std::string& key_file,
                                              std::error_code& ec) {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto cert = context_inner::load_cert(cert_file, ec);
    if (cert == nullptr) {
        return;
    }
    auto key = ctx_->load_key(key_file, ec);
    if (key == nullptr) {
        return;
    }
    const int ret =
        ::mbedtls_ssl_conf_own_cert(&ctx_->cfg, cert.get(), key.get());
    if (ret != 0) {
        ec.assign(ret, ssl_category());
        return;
    }
    ctx_->own_cert_ = std::move(cert);
    ctx_->own_key_ = std::move(key);
}

void context::use_own_certificate(const_buffer cert_buffer, file_format format,
                                  std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto cert = context_inner::load_cert(cert_buffer, ec);
    if (cert == nullptr) {
        return;
    }
    if (ctx_->own_key_ != nullptr) {
        const int ret = ::mbedtls_ssl_conf_own_cert(&ctx_->cfg, cert.get(),
                                                    ctx_->own_key_.get());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return;
        }
    }
    ctx_->own_cert_ = std::move(cert);
}

void context::use_own_certificate_file(const std::string& filename,
                                       file_format format,
                                       std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto cert = context_inner::load_cert(filename, ec);
    if (cert == nullptr) {
        return;
    }
    if (ctx_->own_key_ != nullptr) {
        const int ret = ::mbedtls_ssl_conf_own_cert(&ctx_->cfg, cert.get(),
                                                    ctx_->own_key_.get());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return;
        }
    }
    ctx_->own_cert_ = std::move(cert);
}

void context::use_private_key(const_buffer key_buffer, file_format format,
                              std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto key = ctx_->load_key(key_buffer, ec);
    if (key == nullptr) {
        return;
    }
    if (ctx_->own_cert_ != nullptr) {
        const int ret = ::mbedtls_ssl_conf_own_cert(
            &ctx_->cfg, ctx_->own_cert_.get(), key.get());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return;
        }
    }
    ctx_->own_key_ = std::move(key);
}

void context::use_private_key_file(const std::string& filename,
                                   file_format format,
                                   std::error_code& ec) noexcept {
    ec.clear();
    if (ctx_ == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    auto key = ctx_->load_key(filename, ec);
    if (key == nullptr) {
        return;
    }
    if (ctx_->own_cert_ != nullptr) {
        const int ret = ::mbedtls_ssl_conf_own_cert(
            &ctx_->cfg, ctx_->own_cert_.get(), key.get());
        if (ret != 0) {
            ec.assign(ret, ssl_category());
            return;
        }
    }
    ctx_->own_key_ = std::move(key);
}

void context::set_verify_depth(std::size_t depth) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        return;
    }
    ctx_->max_depth = static_cast<int>(depth);
}

void context::set_verify_cb(const verify_mode* mode,
                            std::unique_ptr<verify_callback_base> cb) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        return;
    }
    if (mode != nullptr) {
        const int authmode =
            *mode == verify_mode::none       ? MBEDTLS_SSL_VERIFY_NONE
            : *mode == verify_mode::optional ? MBEDTLS_SSL_VERIFY_OPTIONAL
                                             : MBEDTLS_SSL_VERIFY_REQUIRED;
        ::mbedtls_ssl_conf_authmode(&ctx_->cfg, authmode);
    }
    if (cb != nullptr) {
        ctx_->verify_cb = std::move(cb);
        ::mbedtls_ssl_conf_verify(&ctx_->cfg, context_inner::verify_callback,
                                  ctx_.get());
    }
}

void context::set_passwd_cb(
    std::unique_ptr<password_callback_base> cb) noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ != nullptr) {
        ctx_->password_cb = std::move(cb);
    }
}

void context::remove_verify_cb() noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ == nullptr) {
        return;
    }
    ::mbedtls_ssl_conf_verify(&ctx_->cfg, nullptr, nullptr);
    ctx_->verify_cb = nullptr;
}

void context::remove_passwd_cb() noexcept {
    assert(ctx_ != nullptr);
    if (ctx_ != nullptr) {
        ctx_->password_cb = nullptr;
    }
}

struct mbedtls_impl::inner : noncopyable {
    mbedtls_ssl_context ssl;
    ring_bio* send_bio = nullptr;
    ring_bio* recv_bio = nullptr;
    ref<context::context_inner> ctx_inner;
    const_buffer last_write_buff;

    inner(context::context_inner& ctx) noexcept : ctx_inner{ctx} {
        ::mbedtls_ssl_init(&ssl);
    }

    ~inner() {
        ::mbedtls_ssl_free(&ssl);
    }
};

mbedtls_impl::mbedtls_impl(context& ctx) {
    open(ctx);
}

mbedtls_impl::~mbedtls_impl() = default;

void mbedtls_impl::open(context& ctx, std::error_code& ec) noexcept {
    ec.clear();
    if (!ctx.is_open()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    inner* new_impl_ptr = new (std::nothrow) inner{*ctx.ctx_};
    if (new_impl_ptr == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return;
    }
    std::unique_ptr<inner> impl{new_impl_ptr};
    int ret = ::mbedtls_ssl_setup(&impl->ssl, &ctx.ctx_->cfg);
    if (ret != 0) {
        ec.assign(ret, ssl_category());
        return;
    }

    impl_ = std::move(impl);
}

void* mbedtls_impl::native_handle() const noexcept {
    if (impl_ != nullptr) {
        return &impl_->ssl;
    }
    return nullptr;
}

void mbedtls_impl::reopen(std::error_code& ec) noexcept {
    ec.clear();
    if (!is_open()) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    const mbedtls_ssl_config* cfg =
        ::mbedtls_ssl_context_get_config(&impl_->ssl);
    if (cfg == nullptr) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    assert(cfg == std::addressof(impl_->ctx_inner->cfg));
    inner* new_impl_ptr = new (std::nothrow) inner{*impl_->ctx_inner};
    if (new_impl_ptr == nullptr) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return;
    }
    std::unique_ptr<inner> impl{new_impl_ptr};
    int ret = ::mbedtls_ssl_setup(&impl->ssl, cfg);
    if (ret != 0) {
        ec = std::make_error_code(std::errc::not_enough_memory);
        return;
    }

    impl_ = std::move(impl);
}

void mbedtls_impl::close() noexcept {
    impl_.reset();
}

std::string_view mbedtls_impl::version() const noexcept {
    if (impl_ == nullptr) {
        return {};
    }
    const char* p = ::mbedtls_ssl_get_version(&impl_->ssl);
    if (p == nullptr) {
        return {};
    }
    return std::string_view{p};
}

std::string_view mbedtls_impl::get_alpn_protocol() const noexcept {
    if (impl_ == nullptr) {
        return {};
    }
    const char* alpn_str = ::mbedtls_ssl_get_alpn_protocol(&impl_->ssl);
    return alpn_str != nullptr ? std::string_view{alpn_str}
                               : std::string_view{};
}

void mbedtls_impl::set_alpn_protocols(
    std::span<const std::string_view> protos) noexcept {
    assert(impl_ != nullptr);
    if (impl_ != nullptr) {
        impl_->ctx_inner->set_alpn_protocols(protos);
    }
}

// static
int mbedtls_impl::ring_io_send(void* ctx, const unsigned char* buf,
                               size_t len) noexcept {
    if (!ctx || !buf || len <= 0) {
        return 0;
    }
    auto impl = reinterpret_cast<inner*>(ctx);
    assert(impl->send_bio != nullptr);
    if (impl->send_bio == nullptr) {
        return 0;
    }
    size_t ret = impl->send_bio->put_data(buffer(buf, len));
    if (ret == 0) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    return static_cast<int>(ret);
}

// static
int mbedtls_impl::ring_io_recv(void* ctx, unsigned char* buf,
                               size_t len) noexcept {
    if (!ctx || !buf || len == 0) {
        return 0;
    }
    auto impl = reinterpret_cast<inner*>(ctx);
    assert(impl->recv_bio != nullptr);
    if (impl->recv_bio == nullptr) {
        return 0;
    }
    size_t ret = impl->recv_bio->get_data(buffer(buf, len));
    if (ret == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return static_cast<int>(ret);
}

void mbedtls_impl::set_bios(ring_bio& rbio, ring_bio& wbio) noexcept {
    assert(impl_ != nullptr);
    if (impl_ == nullptr) {
        return;
    }
    impl_->send_bio = &wbio;
    impl_->recv_bio = &rbio;
    ::mbedtls_ssl_set_bio(&impl_->ssl, impl_.get(), ring_io_send, ring_io_recv,
                          nullptr);
}

void mbedtls_impl::set_hostname(std::string_view host) noexcept {
    assert(impl_ != nullptr);
    if (impl_ == nullptr) {
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
    ::mbedtls_ssl_set_hostname(&impl_->ssl, host_ptr);
}

void mbedtls_impl::set_mode(bool is_client) noexcept {
    std::ignore = is_client;
}

bool mbedtls_impl::is_handshake_done() const noexcept {
    return ::mbedtls_ssl_is_handshake_over(&impl_->ssl) == 1;
}

void mbedtls_impl::handshake(engine_state& state,
                             std::error_code& ec) noexcept {
    assert(impl_ != nullptr);
    if (impl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    ec.clear();
    state = engine_state::done;
    int ret = 0;
    while (1) {
        ret = ::mbedtls_ssl_handshake(&impl_->ssl);
        if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            continue;
        }
        break;
    }
    get_last_ssl_error(ret, state, ec);
    if (ret == 0) {
        assert(is_handshake_done());
    }
}

std::size_t mbedtls_impl::flush_output(engine_state& state,
                                       std::error_code& ec) noexcept {
    assert(!impl_->last_write_buff.empty());
    const std::size_t total_size = impl_->last_write_buff.size();
    while (!impl_->last_write_buff.empty()) {
        const int n = ::mbedtls_ssl_write(
            &impl_->ssl, impl_->last_write_buff.data_as<const uint8_t>(),
            impl_->last_write_buff.size());
        if (n <= 0) {
            if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
                continue;
            }
            get_last_ssl_error(n, state, ec);
            return total_size - impl_->last_write_buff.size();
        }
        impl_->last_write_buff += static_cast<std::size_t>(n);
    }
    return total_size - impl_->last_write_buff.size();
}

std::size_t mbedtls_impl::write(const_buffer buff, engine_state& state,
                                std::error_code& ec) noexcept {
    assert(impl_ != nullptr);
    if (impl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    ec.clear();
    state = engine_state::done;
    const std::size_t total_size = buff.size();
    std::size_t flushed = 0;
    if (!impl_->last_write_buff.empty()) {
        flushed = flush_output(state, ec);
        buff += flushed;
        if (state != engine_state::done) {
            return total_size - buff.size();
        }
    }
    if (buff.empty()) {
        return total_size - buff.size();
    }
    while (!ec && !buff.empty()) {
        int n = ::mbedtls_ssl_write(&impl_->ssl, buff.data_as<const uint8_t>(),
                                    buff.size());
        if (n <= 0) {
            if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
                continue;
            }
            get_last_ssl_error(n, state, ec);
            if (state == engine_state::want_write) {
                impl_->last_write_buff = buff;
            }
            return total_size - buff.size();
        }
        buff += static_cast<std::size_t>(n);
    }
    return total_size - buff.size();
}

std::size_t mbedtls_impl::read(mutable_buffer buff, engine_state& state,
                               std::error_code& ec) noexcept {
    assert(impl_ != nullptr);
    if (impl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
        return 0;
    }
    ec.clear();
    state = engine_state::done;
    if (buff.empty()) {
        return 0;
    }
    const std::size_t total_size = buff.size();
    while (!ec && !buff.empty()) {
        const int n =
            mbedtls_ssl_read(&impl_->ssl, buff.data_as<uint8_t>(), buff.size());
        if (n <= 0) {
            if (n == MBEDTLS_ERR_SSL_WANT_READ && impl_->recv_bio->size() > 0) {
                continue;
            }
            else if (n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
                continue;
            }
            else if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                ec = io::detail::make_eof_error_code();
                state = engine_state::error;
            }
            else {
                /*
                Returns 0 if the read end of the underlying transport was closed
                without sending a CloseNotify beforehand. Since socket is not
                used but memory buffers, 0 should never be returned!
                */
                assert(n != 0);
                if (n == 0) {
                    ec = std::make_error_code(std::errc::connection_aborted);
                }
                else {
                    get_last_ssl_error(n, state, ec);
                }
            }
            return total_size - buff.size();
        }
        buff += static_cast<std::size_t>(n);
    }
    return total_size - buff.size();
}

void mbedtls_impl::shutdown(engine_state& state, std::error_code& ec) noexcept {
    assert(impl_ != nullptr);
    if (impl_ == nullptr) {
        state = engine_state::error;
        ec = std::make_error_code(std::errc::invalid_argument);
    }
    ec.clear();
    state = engine_state::done;
    int ret = ::mbedtls_ssl_close_notify(&impl_->ssl);
    get_last_ssl_error(ret, state, ec);
}