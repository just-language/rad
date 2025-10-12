#include <Windows.h>
#include <bcrypt.h>
#include <rad/crypto/wincrypt.h>
#include <rad/random.h>
#include <rad/system_error.h>
#include <winternl.h>

using namespace RAD_LIB_NAMESPACE;
using namespace crypto;

namespace {
    void throw_if_os_error(NTSTATUS status, const char* str) {
        if (status) {
            throw std::system_error(std::error_code(status, system_category()),
                                    str);
        }
    }
} // namespace

void aes_gcm::BcryptCloser::operator()(pointer handle) noexcept {
    [[maybe_unused]] auto result = ::BCryptCloseAlgorithmProvider(handle, 0);
    assert(NT_SUCCESS(result) && "invalid bcrypt handle");
}

aes_gcm::aes_gcm() {
    BCRYPT_HANDLE handle = nullptr;
    NTSTATUS status =
        ::BCryptOpenAlgorithmProvider(&handle, BCRYPT_AES_ALGORITHM, 0, 0);
    throw_if_os_error(status, "BCryptOpenAlgorithmProvider");
    native_handle_type new_handle(handle);
    status = ::BCryptSetProperty(new_handle.get(), BCRYPT_CHAINING_MODE,
                                 (PUCHAR)(BCRYPT_CHAIN_MODE_GCM),
                                 sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    throw_if_os_error(status, "BCryptSetProperty");
    crypt_handle_ = std::move(new_handle);
}

void aes_gcm::set_key(const_buffer key) {
    std::vector<uint8_t> keyVec = key.to_vector<uint8_t>();
    BCRYPT_HANDLE handle = nullptr;
    NTSTATUS status = ::BCryptGenerateSymmetricKey(
        native_handle().get(), &handle, nullptr, 0, keyVec.data(),
        static_cast<uint32_t>(keyVec.size()), 0);
    throw_if_os_error(status, "BCryptGenerateSymmetricKey");
    key_handle_.reset(handle);
}

void aes_gcm::decrypt(const_buffer iv, const_buffer tag, const_buffer input,
                      dynamic_buffer output) {
    std::array<uint8_t, nonce_size> nonce;
    memcpy(nonce.data(), iv.data(), nonce_size);

    std::array<uint8_t, tag_size> tagVec;
    memcpy(tagVec.data(), tag.data(), std::min(tagVec.size(), tag.size()));

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = nonce.data();
    auth_info.cbNonce = static_cast<uint32_t>(nonce.size());
    auth_info.pbTag = tagVec.data();
    auth_info.cbTag = static_cast<uint32_t>(tagVec.size());

    ULONG required_size = 0;
    NTSTATUS status =
        ::BCryptDecrypt(key_handle_.get(), input.data_as<uint8_t>(),
                        static_cast<ULONG>(input.size()), &auth_info, nullptr,
                        0, nullptr, 0, &required_size, 0);
    throw_if_os_error(status, "BCryptDecrypt");

    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = nonce.data();
    auth_info.cbNonce = static_cast<uint32_t>(nonce.size());
    auth_info.pbTag = tagVec.data();
    auth_info.cbTag = static_cast<uint32_t>(tagVec.size());

    size_t old_size = output.size();
    output.resize(required_size + old_size);

    status = ::BCryptDecrypt(key_handle_.get(), input.data_as<uint8_t>(),
                             static_cast<ULONG>(input.size()), &auth_info,
                             nullptr, 0, output.data_as<uint8_t>() + old_size,
                             required_size, &required_size, 0);

    throw_if_os_error(status, "BCryptDecrypt");
}

void aes_gcm::encrypt(const_buffer iv, std::array<uint8_t, tag_size>& tag,
                      const_buffer input, dynamic_buffer output) {
    std::array<uint8_t, nonce_size> nonce;
    memcpy(nonce.data(), iv.data(), nonce_size);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = static_cast<uint32_t>(nonce.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<uint32_t>(tag.size());

    ULONG required_size = 0;
    NTSTATUS status =
        ::BCryptEncrypt(key_handle_.get(), input.data_as<uint8_t>(),
                        static_cast<ULONG>(input.size()), &authInfo, nullptr, 0,
                        nullptr, 0, &required_size, 0);
    throw_if_os_error(status, "BCryptEncrypt");

    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = nonce.data();
    authInfo.cbNonce = static_cast<uint32_t>(nonce.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<uint32_t>(tag.size());

    size_t old_size = output.size();
    output.resize(required_size + old_size);

    status = ::BCryptEncrypt(key_handle_.get(), input.data_as<uint8_t>(),
                             static_cast<ULONG>(input.size()), &authInfo,
                             nullptr, 0, input.data_as<uint8_t>() + old_size,
                             required_size, &required_size, 0);

    throw_if_os_error(status, "BCryptEncrypt");
}

auto aes_gcm::generate_iv() -> std::array<uint8_t, nonce_size> {
    std::array<uint8_t, nonce_size> ret;
    for (auto& byte : ret) {
        byte = random_byte(0, 255);
    }
    return ret;
}
