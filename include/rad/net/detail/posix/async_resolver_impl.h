#pragma once
#include <rad/async/io_executor.h>
#include <rad/net/detail/posix/resolver_impl.h>
#include <string>
#include <vector>

namespace RAD_LIB_NAMESPACE::net::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    class async_resolver_impl : public trackable {
        struct resolver_op_base;

        struct state_t {
            resolver_op_base* handler = nullptr;
            resolver_impl::resolver_hint hint;
            std::string host;
            std::string service;
            bool canceled = false;
        };

        struct results_storage_t {
            static constexpr bool by_value = true;

            std::vector<endpoint> results;

            std::vector<endpoint>& get_results() noexcept {
                return results;
            }
        };

        struct results_ref_storage_t {
            static constexpr bool by_value = false;

            std::vector<endpoint>& results;

            std::vector<endpoint>& get_results() noexcept {
                return results;
            }
        };

        struct results_awaiter;

        struct results_ref_awaiter;

        template <class Handler, class Storage, class Alloc>
        struct resolve_op;

        friend struct results_awaiter;
        friend struct results_ref_awaiter;

    public:
        using executor_type = io_executor;
        using resolver_hint = resolver_impl::resolver_hint;
        using native_string_type = zstring_view;
        using alternative_string_type1 = std::string;
        using alternative_string_type2 = std::wstring_view;

        static std::string parse_service(uint16_t port) {
            return std::to_string(port);
        }

        async_resolver_impl(executor_type& ex) noexcept : ex_{ex} {
        }

        executor_type& executor() noexcept {
            return ex_;
        }

        const executor_type& executor() const noexcept {
            return ex_;
        }

        template <class Protocol>
        results_awaiter
        async_resolve(native_string_type host, native_string_type service,
                      const Protocol& protocol, std::error_code& ec = no_ec);

        template <class Protocol>
        results_ref_awaiter
        async_resolve(native_string_type host, native_string_type service,
                      const Protocol& protocol, std::vector<endpoint>& results,
                      std::error_code& ec = no_ec);

        template <class Protocol, class Handler, class Alloc>
        void async_resolve(native_string_type host, native_string_type service,
                           const Protocol& protocol, Handler&& handler,
                           const Alloc& alloc) {
            auto state = state_.synchronize();
            if (state->handler) {
                post_early(
                    [handler = std::forward<Handler>(handler)] {
                        handler(std::make_error_code(
                                    std::errc::operation_in_progress),
                                std::vector<endpoint>{});
                    },
                    alloc);
                return;
            }

            using op_t = resolve_op<Handler, results_storage_t, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, std::forward<Handler>(handler), results_storage_t{},
                any_ex());
            state->handler = op;
            state->canceled = false;
            state->hint = resolver_hint{protocol};
            state->host = host;
            state->service = service;
            async_resolve();
        }

        template <class Protocol, class Handler, class Alloc>
        void async_resolve(native_string_type host, native_string_type service,
                           const Protocol& protocol, Handler&& handler,
                           std::vector<endpoint>& results, Alloc& alloc) {
            auto state = state_.synchronize();
            if (state->handler) {
                post_early(
                    [handler = std::forward<Handler>(handler)] {
                        handler(std::make_error_code(
                            std::errc::operation_in_progress));
                    },
                    alloc);
                return;
            }

            using op_t = resolve_op<Handler, results_ref_storage_t, Alloc>;
            op_t* op = details::allocate_op<op_t>(
                alloc, std::forward<Handler>(handler),
                results_ref_storage_t{results}, any_ex());
            state->handler = op;
            state->canceled = false;
            state->hint = resolver_hint{protocol};
            state->host = host;
            state->service = service;
            async_resolve();
        }

        void cancel() noexcept {
            state_->canceled = true;
        }

    private:
        async_result start_coro(std::string host, std::string service,
                                const resolver_hint& hint,
                                resolver_op_base* op) noexcept {
            auto state = state_.synchronize();
            if (state->handler) {
                return async_result::failed(
                    std::make_error_code(std::errc::operation_in_progress));
            }

            state->handler = op;
            state->hint = hint;
            state->host = std::move(host);
            state->service = std::move(service);
            state->canceled = false;

            async_resolve();

            return async_result::pending();
        }

        RAD_EXPORT_DECL void async_resolve() noexcept;

        resolver_op_base* do_async_resolve() noexcept;

        template <class Handler, class Alloc>
        void post_early(Handler&& handler, const Alloc& alloc) {
            post(any_ex(), std::forward<Handler>(handler), alloc);
        }

        any_executor& any_ex() noexcept {
            return ex_->as_any_executor();
        }

        ref<executor_type> ex_;

        static constexpr std::size_t alloc_size =
            details::async_job_size<sizeof(pointer<async_resolver_impl>),
                                    false>();

        alignas(16) std::array<std::uint8_t, alloc_size> io_alloc_buff_;

        sync_value<state_t> state_;
    };

    struct async_resolver_impl::resolver_op_base : details::async_op_base {
        virtual void store_error(const std::error_code& ec) noexcept = 0;

        virtual std::vector<endpoint>& get_results() noexcept = 0;

    protected:
        ~resolver_op_base() = default;
    };

    struct [[nodiscard]] async_resolver_impl::results_awaiter final
        : async_resolver_impl::resolver_op_base,
          error_storage {
        ref<async_resolver_impl> impl;
        std::coroutine_handle<> waiter;
        resolver_hint hint;
        std::string host;
        std::string service;
        std::vector<endpoint> results;

        results_awaiter(async_resolver_impl& impl, const resolver_hint& hint,
                        std::string host, std::string service,
                        std::error_code& ec)
            : error_storage(ec), impl{impl}, hint{hint}, host{std::move(host)},
              service{std::move(service)} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) noexcept {
            waiter = coro;
            auto result = impl->start_coro(std::move(host), std::move(service),
                                           hint, this);
            if (result.is_pending()) {
                return true;
            }
            store(result.error());
            return false;
        }

        std::vector<endpoint> await_resume() {
            raise("async_resolve");
            return std::move(results);
        }

        virtual void store_error(const std::error_code& ec) noexcept override {
            store(ec);
        }

        virtual std::vector<endpoint>& get_results() noexcept override {
            return results;
        }

        virtual void invoke_operation() override {
            waiter.resume();
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    struct [[nodiscard]] async_resolver_impl::results_ref_awaiter final
        : async_resolver_impl::resolver_op_base,
          error_storage {
        ref<async_resolver_impl> impl;
        std::coroutine_handle<> waiter;
        resolver_impl::resolver_hint hint;
        std::string host;
        std::string service;
        std::vector<endpoint>& results;

        results_ref_awaiter(async_resolver_impl& impl,
                            const resolver_hint& hint, std::string host,
                            std::string service, std::vector<endpoint>& results,
                            std::error_code& ec)
            : error_storage(ec), impl{impl}, hint{hint}, host{std::move(host)},
              service{std::move(service)}, results{results} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) noexcept {
            waiter = coro;
            auto result = impl->start_coro(std::move(host), std::move(service),
                                           hint, this);
            if (result.is_pending()) {
                return true;
            }
            store(result.error());
            return false;
        }

        void await_resume() {
            raise("async_resolve");
        }

        virtual void store_error(const std::error_code& ec) noexcept override {
            store(ec);
        }

        virtual std::vector<endpoint>& get_results() noexcept override {
            return results;
        }

        virtual void invoke_operation() override {
            waiter.resume();
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Handler, class Storage, class Alloc>
    struct async_resolver_impl::resolve_op final
        : async_resolver_impl::resolver_op_base,
          allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::decay_t<Handler>;

        handler_t handler;
        any_executor& ex;
        Storage storage;
        std::error_code ec;

        template <class H>
        resolve_op(H&& handler, const Storage& storage, any_executor& ex,
                   Alloc& alloc) noexcept
            : alloc_base(alloc), handler{std::forward<H>(handler)}, ex{ex},
              storage{storage} {
        }

        virtual void store_error(const std::error_code& e) noexcept override {
            ec = e;
        }

        virtual std::vector<endpoint>& get_results() noexcept override {
            return storage.get_results();
        }

        virtual void invoke_operation() override {
            if constexpr (Storage::by_value) {
                std::vector<endpoint> results =
                    std::move(storage.get_results());
                details::invoke_handler(this, std::error_code{ec},
                                        std::move(results));
            }
            else {
                details::invoke_handler(this, std::error_code{ec});
            }
        }

        virtual any_executor& associated_executor() const noexcept override {
            return ex;
        }
    };

    template <class Protocol>
    auto async_resolver_impl::async_resolve(native_string_type host,
                                            native_string_type service,
                                            const Protocol& protocol,
                                            std::error_code& ec)
        -> results_awaiter {
        return {*this, resolver_hint{protocol}, std::string{host},
                std::string{service}, ec};
    }

    template <class Protocol>
    auto async_resolver_impl::async_resolve(native_string_type host,
                                            native_string_type service,
                                            const Protocol& protocol,
                                            std::vector<endpoint>& results,
                                            std::error_code& ec)
        -> results_ref_awaiter {
        return {*this,
                resolver_hint{protocol},
                std::string{host},
                std::string{service},
                results,
                ec};
    }
} // namespace RAD_LIB_NAMESPACE::net::detail