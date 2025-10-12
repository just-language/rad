#pragma once
#include <rad/libbase.h>
#include <rad/net/types.h>

namespace RAD_LIB_NAMESPACE::local {
    /*!
     * @brief The type of a UNIX domain endpoint.
     *
     * The local::endpoint class describes an endpoint
     * that may be associated with a particular UNIX socket.
     */
    class endpoint {
        struct local_addr_t;

    public:
        /// The size type of unix address.
        using size_type = socklen_t;

        /// This endpoint is resizable.
        static constexpr bool resizable = true;

        /// The maximum unix sockets path length.
        /// It's 107 instead of 108 to make room
        /// for a null terminator in the end of the path.
        static constexpr size_type max_path_len = 107;

        /*!
         * @brief Construct an empty invalid unix socket
         * address.
         */
        endpoint() = default;

        /*!
         * @brief Construct a unix socket endpoint
         * and set the path associated with the endpoint.
         * The path length must be no greater than allowed max
         * path length (107). Longer paths are truncated.
         * @param path The path associated with the endpoint.
         */
        endpoint(std::string_view path) noexcept {
            set_address(path);
        }

        /*!
         * @brief Construct a unix socket endpoint
         * Copy the unix socket address from buffer pointed
         * to by @p addr whose size is @p len.
         * The buffer must be a valid unix socket address,
         * otherwise the behavior is undefined. To check if the
         * buffer is a valid unix socket address call
         * is_valid_address().
         * @param addr Pointer to the buffer containing the unix
         * socket address.
         * @param len The size of the buffer in bytes.
         */
        endpoint(const void* addr, size_type len) noexcept {
            set_address(addr, len);
        }

        /*!
         * @brief Make an endpoint for abstract unix socket.
         * @param path The path associated with the endpoint.
         * The path length must be no greater than allowed max
         * path length (107). Longer paths are truncated.
         * @return Abstract unix socket endpoint.
         */
        static endpoint abstract(std::string_view path) noexcept {
            assert(path.size() <= max_path_len);
            path = path.substr(0, max_path_len);
            endpoint abstract_address;
            abstract_address.addr_.name[0] = '\0';
            path.copy(abstract_address.addr_.name + 1, path.size());
            abstract_address.current_length_ =
                static_cast<size_type>(path.size()) + 1;
            return abstract_address;
        }

        /*!
         * @brief Set the path associated with the endpoint.
         * The path length must be no greater than allowed max
         * path length (107). Longer paths are truncated.
         * @param path The path associated with the endpoint.
         */
        void set_address(std::string_view path) noexcept {
            assert(path.size() <= max_path_len);
            path = path.substr(0, max_path_len);
            const std::size_t n = path.copy(addr_.name, path.size());
            addr_.name[max_path_len] = '\0';
            current_length_ = static_cast<size_type>(n);
        }

        /*!
         * @brief Copy the unix socket address from buffer
         * pointed to by @p addr whose size is @p len. The
         * buffer must be a valid unix socket address, otherwise
         * the behavior is undefined. To check if the buffer is
         * a valid unix socket address call is_valid_address().
         * @param addr Pointer to the buffer containing the unix
         * socket address.
         * @param len The size of the buffer in bytes.
         */
        void set_address(const void* addr, size_type len) noexcept {
            assert(is_valid_address(addr, len));
            if (len < 2) {
                return;
            }
            if (len > sizeof(local_addr_t)) {
                len = sizeof(local_addr_t);
            }
            current_length_ = len - 2;
            if (current_length_ > max_path_len) {
                current_length_ = max_path_len;
            }
            std::string_view name{reinterpret_cast<const char*>(addr) + 2,
                                  static_cast<std::size_t>(current_length_)};
            name.copy(addr_.name, name.size());
            addr_.name[static_cast<std::size_t>(current_length_)] = '\0';
        }

        /*!
         * @brief Get the path associated with the endpoint.
         * @return The path associated with the endpoint.
         */
        std::string_view path() const noexcept {
            if (current_length_ == 0) {
                return {};
            }
            if (is_abstract()) {
                return std::string_view{
                    addr_.name, static_cast<std::size_t>(current_length_)}
                    .substr(1);
            }
            return std::string_view{addr_.name,
                                    static_cast<std::size_t>(current_length_)};
        }

        /*!
         * @brief Set the path associated with the endpoint.
         * The path length must be no greater than allowed max
         * path length (107). Longer paths are truncated.
         * @param path The path associated with the endpoint.
         */
        void path(std::string_view path) noexcept {
            set_address(path);
        }

        /*!
         * @brief Get an address of the stored socket address
         * suitable to be passed to system socket functions.
         * @return An address of the stored socket address.
         */
        void* address() noexcept {
            return &addr_;
        }

        /*!
         * @brief Get an address of the stored socket address
         * suitable to be passed to system socket functions.
         * @return An address of the stored socket address.
         */
        const void* address() const noexcept {
            return &addr_;
        }

        /*!
         * @brief Set the underlying size of the endpoint in the
         * native type. This size includes the address family
         * two bytes. The size must be not greater than max unix
         * socket address size (109)
         * @param size The size of the address in bytes.
         */
        void resize(size_type size) noexcept {
            assert(size >= 2 && size <= max_path_len + 2);
            current_length_ =
                static_cast<size_type>(size - sizeof(local_addr_t::af));
        }

        /*!
         * @brief Get the underlying size of the endpoint in the
         * native type.
         * @return The underlying size of the endpoint in the
         * native type.
         */
        size_type size() const noexcept {
            return sizeof(local_addr_t::af) + current_length_;
        }

        /*!
         * @brief Check if this is an endoint for abstract
         * socket on linux. Abstract unix sockets have '\0' byte
         * as the first byte in the address path.
         * @return True if this is an endoint for abstract
         * socket, otherwise false.
         */
        bool is_abstract() const noexcept {
            return addr_.name[0] == '\0';
        }

        /*!
         * @brief Get the family of the endpoint. It's always
         * local.
         * @return The family of the endpoint.
         */
        static constexpr net::address_family family() noexcept {
            return net::address_family::local;
        }

        /*!
         * @brief Get the maximum size of the endpoint.
         * @return The maximum size of the endpoint.
         */
        static constexpr socklen_t max_size() noexcept {
            return sizeof(local_addr_t);
        }

        /*!
         * @brief Check if buffer pointed to by @p addr with
         * length @p len constitutes a valid unix socket
         * address.
         * @param addr The address buffer pointer.
         * @param len The length of the buffer.
         * @return True if the buffer is a valid address,
         * otherwise false.
         */
        static bool is_valid_address(const void* addr, size_type len) noexcept {
            return len >= 2 && len <= sizeof(local_addr_t) &&
                   reinterpret_cast<const local_addr_t*>(addr)->af ==
                       static_cast<uint16_t>(net::address_family::local);
        }

    private:
        struct local_addr_t {
            uint16_t af = static_cast<uint16_t>(net::address_family::local);
            char name[max_path_len + 1];
        };

        local_addr_t addr_;
        // length of name not including the address family two
        // bytes
        size_type current_length_ = 0;
    };

} // namespace RAD_LIB_NAMESPACE::local