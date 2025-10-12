#pragma once
#include <rad/detail/string_converter.h>
#include <rad/net/detail/resolver_impl.h>

namespace RAD_LIB_NAMESPACE::net {
    namespace details = RAD_LIB_NAMESPACE::detail;

    template <class Protocol>
    class resolver {
        using impl_type = detail::resolver_impl;
        using resolver_hint = typename impl_type::resolver_hint;
        using native_string_type = typename impl_type::native_string_type;
        using alternative_string_type1 =
            typename impl_type::alternative_string_type1;
        using alternative_string_type2 =
            typename impl_type::alternative_string_type2;

        using string_converter =
            details::string_converter<native_string_type,
                                      alternative_string_type1,
                                      alternative_string_type2>;

        template <class ServiceType>
        decltype(auto) parse_service(const ServiceType& service) {
            string_converter cv;
            if constexpr (std::is_integral_v<ServiceType>) {
                return impl_type::parse_service(service);
            }
            else {
                return cv(service);
            }
        }

    public:
        template <class StringType, class ServiceType>
        void resolve(const StringType& host, const ServiceType& service,
                     const Protocol& protocol, resolver_flags flags,
                     std::vector<endpoint>& results, std::error_code& ec) {
            string_converter cv;
            resolver_hint hint{protocol};
            hint.flags = flags;
            impl.do_resolve(cv(host), parse_service(service).data(), hint,
                            results, ec);
        }

        template <class StringType, class ServiceType>
        std::vector<endpoint>
        resolve(const StringType& host, const ServiceType& service,
                const Protocol& protocol, resolver_flags flags,
                std::error_code& ec) {
            std::vector<endpoint> results;
            resolve(host, service, protocol, flags, results, ec);
            return results;
        }

        template <class StringType, class ServiceType>
        void resolve(const StringType& host, const ServiceType& service,
                     const Protocol& protocol, std::vector<endpoint>& results,
                     resolver_flags flags = resolver_flags::none) {
            std::error_code ec;
            resolve(host, service, protocol, flags, results, ec);
            check_and_throw(ec, __func__);
        }

        template <class StringType, class ServiceType>
        std::vector<endpoint>
        resolve(const StringType& host, const ServiceType& service,
                const Protocol& protocol,
                resolver_flags flags = resolver_flags::none) {
            std::vector<endpoint> results;
            resolve(host, service, protocol, results, flags);
            return results;
        }

    private:
        impl_type impl;
    };
}; // namespace RAD_LIB_NAMESPACE::net
