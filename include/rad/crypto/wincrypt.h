#pragma once
#include <rad/buffer.h>
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>
#include <rad/string.h>

#include <memory>

namespace RAD_LIB_NAMESPACE::crypto {

    /*!
     * @brief Flags used by `CryptProtectData`.
     */
    enum class protect_data_flags {
        /// No flags.
        none = 0,
        /*!
         * @brief When this flag is set, it associates the data encrypted with
         * the current computer instead of with an individual user. Any user on
         * the computer on which `CryptProtectData` is called can use
         * `CryptUnprotectData` to decrypt the data.
         */
        local_machine = 0x4,
        /*
         * When this flag is set and UI is specified for either the protect or
         * unprotect operation, the operation fails
         */
        ui_forbidden = 0x1,
        /*!
         * @brief This flag generates an audit on protect and unprotect
         * operations. Audit log entries are recorded only if the description is
         * not empty.
         */
        audit = 0x10,
    };

    /*!
     * @brief Flags used by `CryptUnprotectData`.
     */
    enum class unprotect_data_flags {
        /// No flags.
        none = 0,
        /*
         * When this flag is set and UI is specified for either the protect or
         * unprotect operation, the operation fails
         */
        ui_forbidden = 0x1,
        /*!
         * @brief This flag verifies the protection of a protected BLOB.
         */
        verify_protection = 0x40,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(protect_data_flags);
    RAD_OVERLOAD_ENUM_OPERATORS(unprotect_data_flags);

    /*!
     * @brief Encrypt data at @p input using `CryptProtectData`.
     *
     * Typically, only a user with the same logon credential as the user who
     * encrypted the data can decrypt the data.
     * @param input The plaintext to be encrypted.
     * @param entropy An optional password or other additional entropy used to
     * encrypt the data. If non empty, the same entropy must also be used in the
     * decryption phase.
     * @param flags The flags used for encryption.
     * @param description A string with a readable description of the data to be
     * encrypted. This description string is included with the encrypted data.
     * The decryption phase restores this description.
     * @param output The dynamic buffer where encrypted data will be appended.
     */
    RAD_EXPORT_DECL void protect_data(const_buffer input, const_buffer entropy,
                                      protect_data_flags flags,
                                      wzstring_view description,
                                      dynamic_buffer output);

    /*!
     * @brief Encrypt data at @p input using `CryptProtectData`.
     *
     * Typically, only a user with the same logon credential as the user who
     * encrypted the data can decrypt the data.
     * @param input The plaintext to be encrypted.
     * @param output The dynamic buffer where encrypted data will be appended.
     */
    inline void protect_data(const_buffer input, dynamic_buffer output) {
        protect_data(input, buffer(nullptr), protect_data_flags::none, {},
                     output);
    }

    /*!
     * @brief Decrypt data at @p input using `CryptUnprotectData`.
     * @param input The encrypted data to be decrypted.
     * @param entropy An optional password or other additional entropy that was
     * used to encrypt the data. The same entropy used in encryption must also
     * be used in the decryption phase.
     * @param flags The flags used for decryption.
     * @param description The description string included with the encrypted
     * data.
     * @param output The dynamic buffer where decrypted plaintext data will be
     * appended.
     */
    RAD_EXPORT_DECL void unprotect_data(const_buffer input,
                                        const_buffer entropy,
                                        unprotect_data_flags flags,
                                        std::wstring& description,
                                        dynamic_buffer output);

    /*!
     * @brief Decrypt data at @p input using `CryptUnprotectData`.
     * @param input The encrypted data to be decrypted.
     * @param output The dynamic buffer where decrypted plaintext data will be
     * appended.
     */
    inline void unprotect_data(const_buffer input, dynamic_buffer output) {
        std::wstring description;
        unprotect_data(input, buffer(nullptr), unprotect_data_flags::none,
                       description, output);
    }

    class aes_gcm {
        struct BcryptCloser {
            using pointer = void*;
            RAD_EXPORT_DECL void operator()(pointer handle) noexcept;
        };

    public:
        using native_handle_type = std::unique_ptr<void, BcryptCloser>;

        static constexpr uint32_t nonce_size = 12;
        static constexpr uint32_t tag_size = 16;

        aes_gcm();

        aes_gcm(const_buffer key) : aes_gcm() {
            set_key(key);
        }

        native_handle_type& native_handle() {
            return key_handle_;
        }

        const native_handle_type& native_handle() const {
            return crypt_handle_;
        }

        RAD_EXPORT_DECL void set_key(const_buffer key);

        void decrypt(const_buffer key, const_buffer iv, const_buffer tag,
                     const_buffer input, dynamic_buffer output) {
            set_key(key);
            decrypt(iv, tag, input, output);
        }

        RAD_EXPORT_DECL void decrypt(const_buffer iv, const_buffer tag,
                                     const_buffer input, dynamic_buffer output);

        void encrypt(const_buffer key, const_buffer iv,
                     std::array<uint8_t, tag_size>& tag, const_buffer input,
                     dynamic_buffer output) {
            set_key(key);
            encrypt(iv, tag, input, output);
        }

        RAD_EXPORT_DECL void encrypt(const_buffer iv,
                                     std::array<uint8_t, tag_size>& tag,
                                     const_buffer input, dynamic_buffer output);

        RAD_EXPORT_DECL static std::array<uint8_t, nonce_size> generate_iv();

    private:
        native_handle_type crypt_handle_;
        native_handle_type key_handle_;
    };

} // namespace RAD_LIB_NAMESPACE::crypto