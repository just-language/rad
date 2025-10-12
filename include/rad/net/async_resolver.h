#pragma once
#include <rad/detail/string_converter.h>
#include <rad/net/detail/async_resolver_impl.h>

namespace RAD_LIB_NAMESPACE::net {
    namespace details = RAD_LIB_NAMESPACE::detail;

    template <class Epoint, class T>
    concept ResolveHandler =
        requires(T handler, const std::vector<Epoint>& epoints) {
            requires BasicHandler<T>;
            handler(std::error_code{}, epoints);
        };

    /*!
     * @brief Provides endpoint async resolution functionality using the os
     * native resolver. If the os resolver does not support async
     * operations, the executor's thread pool is used to emulate the async
     * resolve operations.
     * @tparam Protocol The protocol type.
     */
    template <class Protocol>
    class async_resolver {
        using impl_type = detail::async_resolver_impl;

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
        /*!
         * @brief The type of executor associated with the
         * resolver. Typically this is io_executor.
         */
        using executor_type = typename impl_type::executor_type;

        /*!
         * @brief The protocol type.
         */
        using protocol_type = Protocol;

        /*!
         * @brief The endpoint type.
         */
        using endpoint_type = typename Protocol::endpoint_type;

        /*!
         * @brief Construct an async resolver and provide an
         * executor.
         * @param ex The executor which will be used to dispatch
         * handlers.
         */
        async_resolver(executor_type& ex) noexcept : impl{ex} {
        }

        /*!
         * @brief Destroy the resolver and cancel all pending
         * resolve operations. Note that is undefined behavior
         * to destroy the resolver while it has an outstanding
         * async operation.
         */
        ~async_resolver() {
            cancel();
        }

        /*!
         * @brief Get a reference to the timer executor used by
         * the timer
         * @return a reference to timer the executor used by the
         * timer
         */
        executor_type& executor() noexcept {
            return impl.executor();
        }

        /*!
         * @brief Get a reference to the timer executor used by
         * the timer
         * @return a reference to timer the executor used by the
         * timer
         */
        const executor_type& executor() const noexcept {
            return impl.executor();
        }

        /*!
         * @brief Cancel all pending async resolve operations.
         * Canceled operations are passed an error_code that
         * indicates cancelation. Note that not all operations
         * can be canceled. Operations that have completed and
         * are scheduled for invocation can no longer be
         * canceled.
         */
        void cancel() noexcept {
            impl.cancel();
        }

        /*!
         * @brief Async resolve a host. Note that the resolve
         * operation will not start until the returned awaitable
         * is awaited. Only one resolve operation may be active
         * at a time.
         * @tparam StringType The type of string of host.
         * @tparam ServiceType The type of service, either
         * numerical type or a type convertible to
         * std::string_view or std::wstring_view.
         * @param host The domain name to resolve to ip
         * addresses.
         * @param service This is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is not used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol This is used to get the family of the
         * resolved ip addresses.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the resolve operation and result in a vector of
         * endpoints associated with host.
         * @throw On failure `std::system_error` is thrown.
         */
        template <class StringType, class ServiceType>
        auto async_resolve(const StringType& host, const ServiceType& service,
                           const Protocol& protocol,
                           std::error_code& ec = no_ec) {
            string_converter cv;
            return impl.async_resolve(cv(host), parse_service(service),
                                      protocol, ec);
        }

        /*!
         * @brief Async resolve a host. Note that the resolve
         * operation will not start until the returned awaitable
         * is awaited. Only one resolve operation may be active
         * at a time.
         * @tparam StringType The type of string of host.
         * @tparam ServiceType The type of service, either
         * numerical type or a type convertible to
         * std::string_view or std::wstring_view.
         * @param host The domain name to resolve to ip
         * addresses.
         * @param service This is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is not used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol This is used to get the family of the
         * resolved ip addresses.
         * @param results The result resolved endpoints are
         * appended to this list.
         * @param ec If set to no_ec then errors are reported
         * via exceptions, otherwise errors are reported via @p
         * ec.
         * @return An awaitable that is when awaited will start
         * the resolve operation. The result endpoints are
         * appended to @p results.
         * @throw On failure `std::system_error` is thrown.
         */
        template <class StringType, class ServiceType>
        auto async_resolve(const StringType& host, const ServiceType& service,
                           const Protocol& protocol,
                           std::vector<endpoint>& results,
                           std::error_code& ec = no_ec) {
            string_converter cv;
            return impl.async_resolve(cv(host), parse_service(service),
                                      protocol, results, ec);
        }

        /*!
         * @brief Start an async resolve operation and call the
         * handler when the operation is done. Only one resolve
         * operation may be active at a time.
         * @tparam StringType The type of string of host.
         * @tparam ServiceType The type of service, either
         * numerical type or a type convertible to
         * std::string_view or std::wstring_view.
         * @tparam Handler The type of the handler which must
         * satisfy ResolveHandler<endpoint_type>.
         * @tparam Alloc The type of the allocator which must
         * satisfy HandlerAllocator.
         * @param host The domain name to resolve to ip
         * addresses.
         * @param service This is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is not used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol This is used to get the family of the
         * resolved ip addresses.
         * @param handler The handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed and the resolved endpoints. The handler
         * must be either copyable or movable and the following
         * expression must be valid: handler(std::error_code{},
         * std::vector<endpoint>{})
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t.
         */
        template <class StringType, class ServiceType,
                  ResolveHandler<endpoint_type> Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_resolve(const StringType& host, const ServiceType& service,
                           const Protocol& protocol, Handler&& handler,
                           const Alloc& alloc = Alloc()) {
            string_converter cv;
            impl.async_resolve(cv(host), parse_service(service), protocol,
                               std::forward<Handler>(handler), alloc);
        }

        /*!
         * @brief Start an async resolve operation and call the
         * handler when the operation is done. Only one resolve
         * operation may be active at a time.
         * @tparam StringType The type of string of host.
         * @tparam ServiceType The type of service, either
         * numerical type or a type convertible to
         * std::string_view or std::wstring_view.
         * @tparam Handler The type of the handler which must
         * satisfy std::invocable<std::error_code>.
         * @tparam Alloc The type of the allocator which must
         * satisfy HandlerAllocator.
         * @param host The domain name to resolve to ip
         * addresses.
         * @param service This is either the numeric port number
         * in host byte order, or a uri scheme that is used to
         * get the port number. This port number is not used to
         * resolve the host ip addresses but it is appened to
         * the result endpoints after resolving has been done.
         * @param protocol This is used to get the family of the
         * resolved ip addresses.
         * @param handler he handler to invoke when the
         * operation is done and will be passed an error_code
         * that determines whether the operation has succeeded
         * or failed. The handler must be either copyable or
         * movable and the following expression must be valid:
         * handler(std::error_code{})
         * @param results The result resolved endpoints are
         * appended to this list.
         * @param alloc An allocator used to allocate the
         * operation. The size passed to its allocate method is
         * in bytes and the alignment of the allocator must be
         * suitable for std::max_align_t.
         */
        template <class StringType, class ServiceType,
                  std::invocable<std::error_code> Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_resolve(const StringType& host, const ServiceType& service,
                           const Protocol& protocol, Handler&& handler,
                           std::vector<endpoint>& results,
                           const Alloc& alloc = Alloc()) {
            string_converter cv;
            impl.async_resolve(cv(host), parse_service(service), protocol,
                               std::forward<Handler>(handler), results, alloc);
        }

    private:
        impl_type impl;
    };
} // namespace RAD_LIB_NAMESPACE::net