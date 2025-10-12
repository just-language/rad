#pragma once
#include <rad/net/types.h>
#include <rad/string.h>

#include <vector>

namespace RAD_LIB_NAMESPACE::net::detail {
    struct resolver_impl {
        struct resolver_hint {
            resolver_flags flags = resolver_flags::none;
            address_family family = address_family::unspecified;
            socket_type sock_type;
            protocol_type protocol;

            resolver_hint() = default;

            template <class Protocol>
            resolver_hint(const Protocol& protocol) noexcept
                : family{protocol.family()}, sock_type{protocol.type()},
                  protocol{protocol.protocol()} {
            }
        };

        using native_string_type = wzstring_view;
        using alternative_string_type1 = std::wstring;
        using alternative_string_type2 = std::string_view;

        static std::wstring parse_service(uint16_t port) {
            return std::to_wstring(port);
        }

        void do_resolve(native_string_type host, const wchar_t* service,
                        const resolver_hint& hint,
                        std::vector<endpoint>& results,
                        std::error_code& ec) noexcept;
    };
} // namespace RAD_LIB_NAMESPACE::net::detail