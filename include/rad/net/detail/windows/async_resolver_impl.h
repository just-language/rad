#pragma once
#include <rad/async/io_executor.h>
#include <rad/io/windows/iocp.h>
#include <rad/net/detail/windows/resolver_impl.h>

namespace RAD_LIB_NAMESPACE::net::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    class async_resolver_impl : public trackable {
        struct sync_state_t {
            details::async_op_base* handler = nullptr;
            resolver_impl::resolver_hint hint;
            std::wstring host;
            std::wstring service;
            bool canceled = false;
        };

        enum class sync_op_state {
            no_op,
            running,
            canceled,
        };

        struct resolver_sync_state_t {
            std::wstring host;
            std::wstring service;
            resolver_impl::resolver_hint hint;
            details::async_op_base& op;
            void** results;
            int& error_value;
        };

        struct results_storage_t {
            static constexpr bool by_value = true;

            std::vector<endpoint>&
            get_results(std::vector<endpoint>& results) noexcept {
                return results;
            }
        };

        struct results_ref_storage_t {
            static constexpr bool by_value = false;

            std::vector<endpoint>& results;

            std::vector<endpoint>&
            get_results(std::vector<endpoint>&) noexcept {
                return results;
            }
        };

        struct results_awaiter;

        struct results_ref_awaiter;

        template <class Handler, class Storage, class Alloc>
        struct resolve_op;

        template <class Handler, class Storage, class Alloc>
        struct resolve_sync_op;

        template <class OldOp>
        struct resolve_v_result_op final : details::replaced_op_t<OldOp> {
            using base = details::replaced_op_t<OldOp>;
            using handler_t = typename OldOp::handler_t;

            handler_t handler;
            std::error_code ec;

            template <class Alloc>
            resolve_v_result_op(handler_t&& handler, const std::error_code& ec,
                                any_executor& ex, Alloc& alloc)
                : base(ex, alloc), handler{std::move(handler)}, ec{ec} {
            }

            virtual void invoke_operation() override {
                details::invoke_reused_op_handler(this, std::error_code{ec},
                                                  std::vector<endpoint>{});
            }
        };

        friend struct results_awaiter;
        friend struct results_ref_awaiter;

    public:
        using executor_type = io_executor;
        using resolver_hint = resolver_impl::resolver_hint;
        using native_string_type = wzstring_view;
        using alternative_string_type1 = std::wstring;
        using alternative_string_type2 = std::string_view;

        static std::wstring parse_service(uint16_t port) {
            return std::to_wstring(port);
        }

        async_resolver_impl(executor_type& ex) noexcept : ex_{ex} {
        }

        executor_type& executor() noexcept {
            return *ex_;
        }

        const executor_type& executor() const noexcept {
            return *ex_;
        }

        RAD_EXPORT_DECL void cancel() noexcept;

        template <class Protocol>
        results_awaiter
        async_resolve(native_string_type host, native_string_type service,
                      const Protocol& protocol, std::error_code& ec);

        template <class Protocol>
        results_ref_awaiter
        async_resolve(native_string_type host, native_string_type service,
                      const Protocol& protocol, std::vector<endpoint>& results,
                      std::error_code& ec);

        template <class Protocol, class Handler, class Alloc>
        void async_resolve(native_string_type host, native_string_type service,
                           const Protocol& protocol, Handler&& handler,
                           const Alloc& alloc) {
            if (supports_async_operation()) {
                using op_t = resolve_op<Handler, results_storage_t, Alloc>;
                op_t* op = details::allocate_op<op_t>(
                    alloc, *this, std::forward<Handler>(handler),
                    results_storage_t{});
                auto result = do_async_resolve(
                    host, service.data(), resolver_hint{protocol}, *op,
                    &op->results, resolver_op_cb<op_t>);
                if (!result.is_pending()) {
                    post_sync_resolve_v(result, op);
                }
            }
            else {
                if (sync_op_state_.load() != false) {
                    post_early(
                        [handler = std::forward<Handler>(handler)] {
                            handler(std::make_error_code(
                                        std::errc::operation_in_progress),
                                    std::vector<endpoint>{});
                        },
                        alloc);
                    return;
                }
                using op_t = resolve_sync_op<Handler, results_storage_t, Alloc>;
                op_t* op = details::allocate_op<op_t>(
                    alloc, *this, std::forward<Handler>(handler),
                    results_storage_t{});
                emulate_async_resolve(std::wstring{host}, std::wstring{service},
                                      resolver_hint{protocol}, &op->results,
                                      op->error_value, *op);
            }
        }

        template <class Protocol, class Handler, class Alloc>
        void async_resolve(native_string_type host, native_string_type service,
                           const Protocol& protocol, Handler&& handler,
                           std::vector<endpoint>& results, const Alloc& alloc) {
            if (supports_async_operation()) {
                using op_t = resolve_op<Handler, results_ref_storage_t, Alloc>;
                op_t* op = details::allocate_op<op_t>(
                    alloc, *this, std::forward<Handler>(handler),
                    results_ref_storage_t{results});
                auto result = do_async_resolve(
                    host, service.data(), resolver_hint{protocol}, *op,
                    &op->results, resolver_op_cb<op_t>);
                if (!result.is_pending()) {
                    rad::detail::post_sync_ec(any_ex(), result, op);
                }
            }
            else {
                if (sync_op_state_.load() != false) {
                    post_early(
                        [handler = std::forward<Handler>(handler)] {
                            handler(std::make_error_code(
                                std::errc::operation_in_progress));
                        },
                        alloc);
                    return;
                }
                using op_t =
                    resolve_sync_op<Handler, results_ref_storage_t, Alloc>;
                op_t* op = details::allocate_op<op_t>(
                    alloc, *this, std::forward<Handler>(handler),
                    results_ref_storage_t{results});
                emulate_async_resolve(std::wstring{host}, std::wstring{service},
                                      resolver_hint{protocol}, &op->results,
                                      op->error_value, *op);
            }
        }

    private:
        template <class Op>
        static void __stdcall
        resolver_op_cb(DWORD dwError, DWORD dwBytes,
                       LPOVERLAPPED lpOverlapped) noexcept {
            Op* op = Op::from_ov_ptr(lpOverlapped);
            op->schedule_op(dwError);
        }

        using resolver_callback =
            void(__stdcall*)(DWORD dwError, DWORD dwBytes,
                             LPWSAOVERLAPPED lpOverlapped) noexcept;

        RAD_EXPORT_DECL static void __stdcall
        results_awaiter_cb(DWORD dwError, DWORD dwBytes,
                           LPWSAOVERLAPPED lpOverlapped) noexcept;

        RAD_EXPORT_DECL static void __stdcall
        results_ref_awaiter_cb(DWORD dwError, DWORD dwBytes,
                               LPWSAOVERLAPPED lpOverlapped) noexcept;

        RAD_EXPORT_DECL static bool supports_async_operation() noexcept;

        RAD_EXPORT_DECL async_result
        do_async_resolve(native_string_type host, const wchar_t* service,
                         const resolver_hint& hint, io::detail::io_op& op,
                         void** results, resolver_callback callback) noexcept;

        RAD_EXPORT_DECL void
        emulate_async_resolve(std::wstring host, std::wstring service,
                              const resolver_hint& hint, void** results,
                              int& error_value, details::async_op_base& op);

        RAD_EXPORT_DECL void resolve_on_thread_pool();

        RAD_EXPORT_DECL static void
        results_ptr_to_endpoints(void* results, std::vector<endpoint>& epoints);

        RAD_EXPORT_DECL static void free_results(void* results) noexcept;

        template <class Handler, class Alloc>
        void post_early(Handler&& handler, const Alloc& alloc) {
            post(any_ex(), std::forward<Handler>(handler), alloc);
        }

        any_executor& any_ex() noexcept {
            return ex_->as_any_executor();
        }

        template <class OldOp>
        void post_sync_resolve_v(const async_result& result, OldOp* old_op) {
            using op_t = resolve_v_result_op<OldOp>;
            auto op_ptr =
                details::reuse_op<op_t>(old_op, result.error(), any_ex());
            ex_->as_any_executor().post(*op_ptr);
        }

        inline static constexpr int error_io_aborted = 995;

        ref<executor_type> ex_;
        void* cancel_handle_ = nullptr;
        bool supports_async_ = supports_async_operation();
        std::atomic<bool> sync_op_state_ = {false};
    };

    struct [[nodiscard]] async_resolver_impl::results_awaiter
        : public io::detail::io_op,
          error_storage {
        ref<async_resolver_impl> impl;
        std::coroutine_handle<> waiter;
        resolver_hint hint;
        std::wstring host;
        std::wstring service;
        void* results = nullptr;
        int error_value = 0;

        results_awaiter(async_resolver_impl& impl, const resolver_hint& hint,
                        std::wstring host, std::wstring service,
                        std::error_code& ec)
            : error_storage(ec), impl{impl}, hint{hint}, host{std::move(host)},
              service{std::move(service)} {
        }

        void map_and_store_error(int ecode) noexcept {
            if (ecode == error_io_aborted) {
                store(std::make_error_code(std::errc::operation_canceled));
            }
            else if (ecode != 0) {
                store(std::error_code(static_cast<int>(ecode),
                                      system_category()));
            }
        }

        void schedule_op(DWORD ecode) noexcept {
            impl->cancel_handle_ = nullptr;
            map_and_store_error(static_cast<int>(ecode));
            impl->any_ex().post_finished(*this);
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        bool await_suspend(std::coroutine_handle<> coro) noexcept {
            waiter = coro;
            if (impl->supports_async_operation()) {
                auto result =
                    impl->do_async_resolve(host, service.data(), hint, *this,
                                           &results, results_awaiter_cb);
                if (result.is_pending()) {
                    return true;
                }
                store(result.error());
                return false;
            }
            else {
                impl->emulate_async_resolve(std::move(host), std::move(service),
                                            hint, &results, error_value, *this);
                return true;
            }
        }

        std::vector<endpoint> await_resume() {
            if (!impl->supports_async_operation()) {
                map_and_store_error(error_value);
            }
            // if an exception is thrown then results must
            // be null ?
            if (results != nullptr && has_error()) {
                impl->free_results(results);
            }
            raise("async_resolve");
            std::vector<endpoint> epoints;
            results_ptr_to_endpoints(results, epoints);
            return epoints;
        }

        virtual void invoke_operation() override {
            waiter.resume();
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    struct [[nodiscard]] async_resolver_impl::results_ref_awaiter
        : async_resolver_impl::results_awaiter {
        std::vector<endpoint>& epoints;

        results_ref_awaiter(async_resolver_impl& impl,
                            const resolver_hint& hint, std::wstring host,
                            std::wstring service,
                            std::vector<endpoint>& results, std::error_code& ec)
            : results_awaiter(impl, hint, std::move(host), std::move(service),
                              ec),
              epoints{results} {
        }

        void await_resume() {
            if (!impl->supports_async_operation()) {
                map_and_store_error(error_value);
            }
            // if an exception is thrown then results must
            // be null ?
            if (results != nullptr && has_error()) {
                impl->free_results(results);
            }
            raise("async_resolve");
            results_ptr_to_endpoints(results, epoints);
        }
    };

    template <class Handler, class Storage, class Alloc>
    struct async_resolver_impl::resolve_op : io::detail::io_op,
                                             allocator_storage<Alloc> {
        using base = io::detail::io_op;
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::decay_t<Handler>;

        ref<async_resolver_impl> impl;
        void* results = nullptr;
        handler_t handler;
        Storage storage;

        template <class H>
        resolve_op(async_resolver_impl& impl, H&& handler, Storage s,
                   const Alloc& alloc)
            : alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              storage{s} {
        }

        static resolve_op* from_ov_ptr(LPOVERLAPPED ov_ptr) noexcept {
            return static_cast<resolve_op*>(
                io::detail::io_op::from_ov_ptr(ov_ptr));
        }

        virtual void invoke_operation() override {
            const int op_error_code = static_cast<int>(base::get_ov_ec());
            std::error_code ec;
            if (op_error_code == error_io_aborted) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            else if (op_error_code != 0) {
                ec = os::make_system_error(op_error_code);
            }

            std::vector<endpoint> temp_results;
            auto& results_ref = storage.get_results(temp_results);
            if (!ec) {
                results_ptr_to_endpoints(results, results_ref);
            }

            if constexpr (Storage::by_value) {
                std::vector<endpoint> results = std::move(results_ref);
                details::invoke_handler(this, std::error_code{ec},
                                        std::move(results));
            }
            else {
                details::invoke_handler(this, std::error_code{ec});
            }
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }

        void schedule_op(DWORD ecode) noexcept {
            impl->cancel_handle_ = nullptr;
            base::set_ov_ec(ecode);
            impl->any_ex().post_finished(*this);
        }
    };

    template <class Handler, class Storage, class Alloc>
    struct async_resolver_impl::resolve_sync_op : details::async_op_base,
                                                  allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::decay_t<Handler>;

        ref<async_resolver_impl> impl;
        void* results = nullptr;
        handler_t handler;
        Storage storage;
        int error_value = 0;

        template <class H>
        resolve_sync_op(async_resolver_impl& impl, H&& handler, Storage s,
                        const Alloc& alloc)
            : alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              storage{s} {
        }

        virtual void invoke_operation() override {
            std::error_code ec;
            if (error_value == error_io_aborted) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            else if (error_value != 0) {
                ec = os::make_system_error(error_value);
            }

            std::vector<endpoint> temp_results;
            auto& results_ref = storage.get_results(temp_results);
            if (!ec) {
                results_ptr_to_endpoints(results, results_ref);
            }

            if constexpr (Storage::by_value) {
                std::vector<endpoint> results = std::move(results_ref);
                details::invoke_handler(this, std::error_code{ec},
                                        std::move(results));
            }
            else {
                details::invoke_handler(this, std::error_code{ec});
            }
        }

        virtual any_executor& associated_executor() const noexcept override {
            return impl->any_ex();
        }
    };

    template <class Protocol>
    auto async_resolver_impl::async_resolve(native_string_type host,
                                            native_string_type service,
                                            const Protocol& protocol,
                                            std::error_code& ec)
        -> results_awaiter {
        return {*this, resolver_hint{protocol}, std::wstring{host},
                std::wstring{service}, ec};
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
                std::wstring{host},
                std::wstring{service},
                results,
                ec};
    }

} // namespace RAD_LIB_NAMESPACE::net::detail