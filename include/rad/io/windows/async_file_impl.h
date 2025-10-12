#pragma once
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/io/windows/iocp.h>
#include <rad/os_types.h>
#include <rad/string.h>

namespace RAD_LIB_NAMESPACE::io::detail {
    class async_file_impl {
        class write_awaiter;
        class read_awaiter;

        template <class Handler, class Alloc>
        struct write_op;
        template <class Handler, class Alloc>
        struct read_op;
        template <class Handler, class Alloc>
        struct read_all_op;

    public:
        template <class AllocatorTypes>
        static constexpr std::size_t read_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::read)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(read_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t read_all_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type &
                            op_alloc_type::read_all)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(read_all_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t write_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::write)) {
                return 0;
            }
            else {
                using handler = handler_allocator_size_calculator<
                    AllocatorTypes::max_handler_size>;
                return sizeof(read_op<handler, stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t max_allocator_size() noexcept {
            constexpr std::size_t sizes[] = {
                read_allocator_size<AllocatorTypes>(),
                read_all_allocator_size<AllocatorTypes>(),
                write_allocator_size<AllocatorTypes>(),
            };
            return max_of(sizes);
        }

        using executor_type = io_executor;
        using native_string_type = wzstring_view;
        using alternative_string_type1 = std::wstring;
        using alternative_string_type2 = std::string_view;

        using native_handle_type = os::file_handle;
        using native_fd_type = HANDLE;
        using native_path_type = std::wstring;

        async_file_impl(executor_type& ex) : ex_{ex} {
        }

        async_file_impl(executor_type& ex, native_handle_type& handle) noexcept
            : ex_{ex} {
            native_path_type p;
            std::error_code ec;
            set_handle_path(handle, p, ec);
            check_and_throw(ec, "");
        }

        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        executor_type& executor() noexcept {
            return ex_;
        }

        const executor_type& executor() const noexcept {
            return ex_;
        }

        any_executor& get_any_exuector() noexcept {
            return ex_->as_any_executor();
        }

        const any_executor& get_any_exuector() const noexcept {
            return ex_->as_any_executor();
        }

        bool is_valid() const noexcept {
            return static_cast<bool>(handle_);
        }

        bool is_open() const noexcept {
            return static_cast<bool>(handle_);
        }

        explicit operator bool() const noexcept {
            return is_valid();
        }

        const native_path_type& path() const noexcept {
            return path_;
        }

        void close() noexcept {
            handle_.reset();
            path_.clear();
        }

        RAD_EXPORT_DECL void set_handle_path(native_handle_type& handle,
                                             native_path_type& path,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t write(const_buffer buff,
                                          std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t read(mutable_buffer buff,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void cancel() noexcept;

        RAD_EXPORT_DECL async_result do_async_write(
            const const_buffer& buff, io::detail::io_op& ctx) noexcept;

        RAD_EXPORT_DECL async_result do_async_read(
            const mutable_buffer& buff, io::detail::io_op& ctx) noexcept;

        RAD_EXPORT_DECL std::size_t
        get_write_result(io::detail::io_op& op, std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t
        get_read_result(io::detail::io_op& op, std::error_code& ec) noexcept;

        template <WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(const_buffer buffer, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            using op_t = write_op<Handler, Alloc>;
            op_t* op = rad::detail::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));
            auto result = do_async_write(buffer, *op);
            if (!result.is_pending()) {
                rad::detail::post_sync_rw(get_any_exuector(), result, op);
            }
        }

        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(mutable_buffer buffer, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            using op_t = read_op<Handler, Alloc>;
            op_t* op = rad::detail::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler));
            auto result = do_async_read(buffer, *op);
            if (!result.is_pending()) {
                rad::detail::post_sync_rw(get_any_exuector(), result, op);
            }
        }

        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_all(mutable_buffer buffer, Handler&& handler,
                            const Alloc& alloc = Alloc()) {
            using op_t = read_all_op<Handler, Alloc>;
            op_t* op = rad::detail::allocate_op<op_t>(
                alloc, *this, std::forward<Handler>(handler), buffer);

            std::error_code ec;
            std::size_t total_size = buffer.size();
            do {
                auto result = do_async_read(op->buff, *op);
                if (result.is_pending()) {
                    return;
                }
                op->buff += result.transferred();
                ec = result.error();
            } while (!op->buff.empty() && !ec);

            std::size_t transferred = total_size - op->buff.size();
            rad::detail::post_sync_rw(get_any_exuector(),
                                      ec ? async_result::failed(ec, transferred)
                                         : async_result::success(transferred),
                                      op);
        }

        write_awaiter async_write(const_buffer buff,
                                  std::error_code& ec = no_ec);

        read_awaiter async_read(mutable_buffer buff,
                                std::error_code& ec = no_ec);

        read_awaiter async_read_all(mutable_buffer buff,
                                    std::error_code& ec = no_ec);

    private:
        ref<io_executor> ex_;
        native_handle_type handle_;
        native_path_type path_;
    };

    class [[nodiscard]] async_file_impl::write_awaiter final
        : noncopyable,
          io::detail::io_op,
          error_storage {
        ref<async_file_impl> impl;
        std::coroutine_handle<> waiter;
        const_buffer buff;
        std::size_t transferred = 0;

    public:
        write_awaiter(async_file_impl& impl, const_buffer buff,
                      std::error_code& ec) noexcept
            : io::detail::io_op(rad::detail::async_op_type::write),
              error_storage(ec), impl{impl}, buff{buff} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;
    };

    class [[nodiscard]] async_file_impl::read_awaiter final : noncopyable,
                                                              io::detail::io_op,
                                                              error_storage {
        ref<async_file_impl> impl;
        std::coroutine_handle<> waiter;
        mutable_buffer buff;
        uint32_t total_size = 0;
        bool read_all = false;

    public:
        read_awaiter(async_file_impl& impl, mutable_buffer buff, bool read_all,
                     std::error_code& ec) noexcept
            : io::detail::io_op(rad::detail::async_op_type::read),
              error_storage(ec), impl{impl}, buff{buff},
              total_size{static_cast<uint32_t>(buff.size())},
              read_all{read_all} {
        }

        bool want_more_read() const noexcept {
            return read_all ? (!buff.empty() && !has_error()) : false;
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;
    };

    template <class Handler, class Alloc>
    struct async_file_impl::write_op final : io::detail::io_op,
                                             allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_file_impl> impl;
        handler_t handler;

        template <class H>
        write_op(async_file_impl& impl, H&& handler, const Alloc& alloc)
            : io::detail::io_op(rad::detail::async_op_type::write),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)} {
        }

        void invoke_operation() override {
            std::error_code ec;
            std::size_t transferred = impl->get_write_result(*this, ec);
            rad::detail::invoke_handler(this, ec, transferred);
        }

        any_executor& associated_executor() const noexcept override {
            return impl->get_any_exuector();
        }
    };

    template <class Handler, class Alloc>
    struct async_file_impl::read_op final : io::detail::io_op,
                                            allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_file_impl> impl;
        handler_t handler;

        template <class H>
        read_op(async_file_impl& impl, H&& handler, const Alloc& alloc)
            : io::detail::io_op(rad::detail::async_op_type::read),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)} {
        }

        void invoke_operation() override {
            std::error_code ec;
            std::size_t transferred = impl->get_read_result(*this, ec);
            rad::detail::invoke_handler(this, ec, transferred);
        }

        any_executor& associated_executor() const noexcept override {
            return impl->get_any_exuector();
        }
    };

    template <class Handler, class Alloc>
    struct async_file_impl::read_all_op final : io::detail::io_op,
                                                allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;
        using handler_t = std::remove_cvref_t<Handler>;

        ref<async_file_impl> impl;
        handler_t handler;
        mutable_buffer buff;
        std::size_t total_size = buff.size();

        template <class H>
        read_all_op(async_file_impl& impl, H&& handler, mutable_buffer buff,
                    Alloc& alloc)
            : io::detail::io_op(rad::detail::async_op_type::read_all),
              alloc_base(alloc), impl{impl}, handler{std::forward<H>(handler)},
              buff{buff} {
        }

        void invoke_operation() override {
            std::error_code ec;
            buff += impl->get_read_result(*this, ec);

            while (!buff.empty() && !ec) {
                auto result = impl->do_async_read(buff, *this);
                if (result.is_pending()) {
                    return;
                }
                buff += result.transferred();
                ec = result.error();
            }
            // copy total_size to the stack before it is
            // destructed
            rad::detail::invoke_handler(this, ec, total_size - buff.size());
        }

        any_executor& associated_executor() const noexcept override {
            return impl->get_any_exuector();
        }
    };

    inline auto async_file_impl::async_write(const_buffer buff,
                                             std::error_code& ec)
        -> write_awaiter {
        return {*this, buff, ec};
    }

    inline auto async_file_impl::async_read(mutable_buffer buff,
                                            std::error_code& ec)
        -> read_awaiter {
        return {*this, buff, false, ec};
    }

    inline auto async_file_impl::async_read_all(mutable_buffer buff,
                                                std::error_code& ec)
        -> read_awaiter {
        return {*this, buff, true, ec};
    }
} // namespace RAD_LIB_NAMESPACE::io::detail