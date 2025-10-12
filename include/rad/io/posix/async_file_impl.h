#pragma once
#include <rad/async/io_executor.h>
#include <rad/buffer.h>
#include <rad/function_view.h>
#include <rad/io/posix/iovecs_buffers.h>
#include <rad/os_types.h>
#include <rad/string.h>
#include <rad/trackable.h>

namespace RAD_LIB_NAMESPACE::io::detail {
    struct write_op_base : io_op {
        std::size_t transferred = 0;

        write_op_base() : io_op(rad::detail::async_op_type::write) {
        }

    protected:
        ~write_op_base() = default;
    };

    struct read_op_base : io_op {
        std::size_t transferred = 0;
        bool read_all = false;

        read_op_base() : io_op(rad::detail::async_op_type::read) {
        }

    protected:
        ~read_op_base() = default;
    };

    class async_file_impl : public trackable {
        class write_awaiter;
        class read_awaiter;

        class writev_awaiter;

        class readv_awaiter;

        template <class Handler, class Alloc>
        struct write_op;
        template <class Handler, class Alloc>
        struct read_op;

    public:
        template <class AllocatorTypes>
        static constexpr std::size_t read_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::read)) {
                return 0;
            }
            else {
                return sizeof(read_op<handler_allocator_size_calculator<
                                          AllocatorTypes::max_handler_size>,
                                      stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t write_allocator_size() noexcept {
            if constexpr (!(AllocatorTypes::op_type & op_alloc_type::write)) {
                return 0;
            }
            else {
                return sizeof(write_op<handler_allocator_size_calculator<
                                           AllocatorTypes::max_handler_size>,
                                       stateful_null_allocator>);
            }
        }

        template <class AllocatorTypes>
        static constexpr std::size_t max_allocator_size() noexcept {
            constexpr std::size_t sizes[] = {
                read_allocator_size<AllocatorTypes>(),
                write_allocator_size<AllocatorTypes>(),
            };
            return max_of(sizes);
        }

        using executor_type = io_executor;
        using native_string_type = zstring_view;
        using alternative_string_type1 = std::string;
        using alternative_string_type2 = std::wstring_view;

        using native_handle_type = os::file_handle;
        using native_fd_type = int;
        using native_path_type = std::string;

        async_file_impl(executor_type& ex)
            : data_{nullptr, descriptor_data_deleter{ex}} {
        }

        async_file_impl(executor_type& ex, native_handle_type& handle) noexcept
            : data_{nullptr, descriptor_data_deleter{ex}} {
            init_by_handle(handle);
        }

        async_file_impl(async_file_impl&&) noexcept = default;

        async_file_impl& operator=(async_file_impl&&) noexcept = default;

        native_handle_type& native_handle() noexcept {
            return data_ != nullptr ? data_->handle : dummy_file_handle;
        }

        const native_handle_type& native_handle() const noexcept {
            return data_ != nullptr ? data_->handle : dummy_file_handle;
        }

        executor_type& executor() noexcept {
            return *data_.get_deleter().ex;
        }

        const executor_type& executor() const noexcept {
            return *data_.get_deleter().ex;
        }

        any_executor& get_any_executor() noexcept {
            return executor().as_any_executor();
        }

        bool is_valid() const noexcept {
            return data_ != nullptr;
        }

        bool is_open() const noexcept {
            return data_ != nullptr;
        }

        explicit operator bool() const noexcept {
            return is_valid();
        }

        const native_path_type& path() const noexcept {
            return path_;
        }

        void close() noexcept {
            data_.reset();
            path_.clear();
        }

        RAD_EXPORT_DECL void set_handle_path(native_handle_type& handle,
                                             native_path_type& path,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t write(const const_buffer* buffs,
                                          std::size_t n,
                                          std::error_code& ec) noexcept;

        std::size_t write(const const_buffer& buff,
                          std::error_code& ec) noexcept {
            return write(&buff, 1, ec);
        }

        RAD_EXPORT_DECL std::size_t read(const mutable_buffer* buffs,
                                         std::size_t n, bool read_all,
                                         std::error_code& ec) noexcept;

        std::size_t read(const mutable_buffer* buffs, std::size_t n,
                         std::error_code& ec) noexcept {
            return read(buffs, n, false, ec);
        }

        std::size_t read(const mutable_buffer& buff, bool read_all,
                         std::error_code& ec) noexcept {
            mutable_buffer copied_buff = buff;
            return read(&copied_buff, 1, read_all, ec);
        }

        std::size_t read(const mutable_buffer& buff,
                         std::error_code& ec) noexcept {
            return read(&buff, 1, false, ec);
        }

        RAD_EXPORT_DECL void cancel() noexcept;

        RAD_EXPORT_DECL async_result
        do_async_write(const_buffer& buff,
                       function_view<write_op_base*()> alloc_fn) noexcept;

        RAD_EXPORT_DECL async_result
        do_async_write(iovec_buffers& buffs,
                       function_view<write_op_base*()> alloc_fn) noexcept;

        RAD_EXPORT_DECL async_result
        do_async_read(mutable_buffer& buff, bool read_all,
                      function_view<read_op_base*()> alloc_fn) noexcept;

        RAD_EXPORT_DECL async_result
        do_async_read(iovec_buffers& buffs, bool read_all,
                      function_view<read_op_base*()> alloc_fn) noexcept;

        template <ConstBufferSequence Buffers, WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(const Buffers& buffers, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            using op_t = write_op<std::remove_cvref_t<Handler>, Alloc>;

            if (!is_open()) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            iovec_buffers cloned_buffs{buffers};

            auto alloc_fn = [&]() -> op_t* {
                return rad::detail::allocate_op<op_t>(
                    alloc, *this, std::move(cloned_buffs),
                    std::forward<Handler>(handler));
            };

            auto result = do_async_write(cloned_buffs, alloc_fn);
            if (result.is_pending()) {
                return;
            }
            // op was not allocated
            post_early([ec = result.error(), n = result.transferred(),
                        handler = std::forward<Handler>(
                            handler)]() mutable { handler(ec, n); },
                       alloc);
        }

        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(const Buffers& buffers, Handler&& handler,
                        bool read_all, const Alloc& alloc = Alloc()) {
            using op_t = read_op<std::remove_cvref_t<Handler>, Alloc>;

            if (!is_open()) {
                post_early(
                    [handler = std::forward<Handler>(handler)]() mutable {
                        handler(std::make_error_code(
                                    std::errc::bad_file_descriptor),
                                0);
                    },
                    alloc);
                return;
            }

            iovec_buffers cloned_buffs{buffers};

            auto alloc_fn = [&]() -> op_t* {
                return rad::detail::allocate_op<op_t>(
                    alloc, *this, std::move(cloned_buffs),
                    std::forward<Handler>(handler), read_all);
            };

            auto result = do_async_read(cloned_buffs, read_all, alloc_fn);
            if (result.is_pending()) {
                return;
            }
            // op was not allocated
            post_early([ec = result.error(), n = result.transferred(),
                        handler = std::forward<Handler>(
                            handler)]() mutable { handler(ec, n); },
                       alloc);
        }

        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(Buffers&& buffers, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            async_read(std::forward<Buffers>(buffers),
                       std::forward<Handler>(handler), false, alloc);
        }

        template <MutableBufferSequence Buffers, ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_all(Buffers&& buffers, Handler&& handler,
                            const Alloc& alloc = Alloc()) {
            async_read(std::forward<Buffers>(buffers),
                       std::forward<Handler>(handler), true, alloc);
        }

        write_awaiter async_write(const_buffer buff,
                                  std::error_code& ec = no_ec);

        read_awaiter async_read(mutable_buffer buff,
                                std::error_code& ec = no_ec);

        read_awaiter async_read_all(mutable_buffer buff,
                                    std::error_code& ec = no_ec);

        template <class Buffers>
        writev_awaiter async_writev(const Buffers& buffers,
                                    std::error_code& ec);

        template <class Buffers>
        readv_awaiter async_readv(const Buffers& buffers, std::error_code& ec);

        template <class Buffers>
        readv_awaiter async_readv_all(const Buffers& buffers,
                                      std::error_code& ec);

    private:
        RAD_EXPORT_DECL async_result perform_async_write(bool first_time,
                                                         const_buffer& buff);

        RAD_EXPORT_DECL async_result perform_async_write(bool first_time,
                                                         iovec_buffers& buffs);

        RAD_EXPORT_DECL async_result perform_async_read(mutable_buffer& buff,
                                                        bool read_all) noexcept;

        RAD_EXPORT_DECL async_result perform_async_read(iovec_buffers& buffs,
                                                        bool read_all) noexcept;

        // before this call handle_ is invalid and data_ is
        // nullptr after successful return handle_ is set to fd
        // and data_ is allocated on failure ownership of fd is
        // not taken
        RAD_EXPORT_DECL void init_by_fd(int fd, std::error_code& ec) noexcept;

        void init_by_fd(int fd) {
            std::error_code ec;
            init_by_fd(fd, ec);
            if (ec) {
                throw std::system_error{ec};
            }
        }

        void init_by_handle(native_handle_type& handle) {
            int fd = handle.get();
            init_by_fd(fd);
            // ownership of handle was taken!
            handle.release();
        }

        async_result make_pending(std::size_t n) noexcept {
            get_any_executor().add_work();
            return async_result::pending(n);
        }

        template <class Handler, class Alloc>
        void post_early(Handler&& handler, const Alloc& alloc) {
            post(get_any_executor(), std::forward<Handler>(handler), alloc);
        }

        // on destruction handle_ should be destroyed before
        // data_
        descriptor_data_ptr data_;
        native_path_type path_;
        inline static native_handle_type dummy_file_handle;
    };

    RAD_EXPORT_DECL void wait_until_writable(int fd,
                                             std::error_code& ec) noexcept;

    RAD_EXPORT_DECL void wait_until_readable(int fd,
                                             std::error_code& ec) noexcept;

    RAD_EXPORT_DECL void make_error_if_descriptor_is_closed(bool is_open,
                                                            std::error_code& ec,
                                                            const char* msg);

    // after successful return handle is set to fd and data is allocated
    // on failure ownership of fd is not taken
    RAD_EXPORT_DECL descriptor_data_ptr attach_fd_to_executor(
        int fd, io_executor& ex, std::error_code& ec) noexcept;

    RAD_EXPORT_DECL void
    set_fd_non_blocking_close_on_exec(int fd, std::error_code& ec) noexcept;

    class [[nodiscard]] RAD_EXPORT_VTABLE async_file_impl::write_awaiter final
        : noncopyable,
          write_op_base,
          error_storage {
        ref<async_file_impl> impl;
        std::coroutine_handle<> waiter;
        const_buffer buff;

    public:
        write_awaiter(async_file_impl& impl, const_buffer buff,
                      std::error_code& ec) noexcept
            : error_storage(ec), impl{impl}, buff{buff} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;

        RAD_EXPORT_DECL bool perform() noexcept override;

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL bool complete(const std::error_code& ec,
                                      int result) noexcept override;
#endif // RAD_HAS_IO_URING
    };

    class [[nodiscard]] RAD_EXPORT_VTABLE async_file_impl::read_awaiter final
        : noncopyable,
          read_op_base,
          error_storage {
        ref<async_file_impl> impl;
        std::coroutine_handle<> waiter;
        mutable_buffer buff;

    public:
        read_awaiter(async_file_impl& impl, mutable_buffer buff, bool read_all,
                     std::error_code& ec) noexcept
            : error_storage(ec), impl{impl}, buff{buff} {
            this->read_all = read_all;
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

        RAD_EXPORT_DECL bool perform() noexcept override;

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL bool complete(const std::error_code& ec,
                                      int result) noexcept override;
#endif // RAD_HAS_IO_URING
    };

    class [[nodiscard]] RAD_EXPORT_VTABLE async_file_impl::writev_awaiter final
        : noncopyable,
          write_op_base,
          error_storage {
        ref<async_file_impl> impl;
        std::coroutine_handle<> waiter;
        iovec_buffers buffers;

    public:
        template <class B>
        writev_awaiter(async_file_impl& impl, const B& buffs,
                       std::error_code& ec) noexcept
            : error_storage(ec), impl{impl}, buffers{buffs} {
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;

        RAD_EXPORT_DECL bool perform() noexcept override;

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL bool complete(const std::error_code& ec,
                                      int result) noexcept override;
#endif // RAD_HAS_IO_URING
    };

    class [[nodiscard]] RAD_EXPORT_VTABLE async_file_impl::readv_awaiter final
        : noncopyable,
          read_op_base,
          error_storage {
        ref<async_file_impl> impl;
        std::coroutine_handle<> waiter;
        iovec_buffers buffers;

    public:
        template <class B>
        readv_awaiter(async_file_impl& impl, const B& buffs, bool read_all,
                      std::error_code& ec) noexcept
            : error_storage(ec), impl{impl}, buffers{buffs} {
            this->read_all = read_all;
        }

        bool await_ready() const noexcept {
            return has_error();
        }

        RAD_EXPORT_DECL bool await_suspend(std::coroutine_handle<> coro);

        RAD_EXPORT_DECL std::size_t await_resume() const;

        RAD_EXPORT_DECL void invoke_operation() override;

        RAD_EXPORT_DECL any_executor&
        associated_executor() const noexcept override;

        RAD_EXPORT_DECL bool perform() noexcept override;

#ifdef RAD_HAS_IO_URING
        RAD_EXPORT_DECL void submit(descriptor_data_inner_t& inner,
                                    std::error_code& ec) noexcept override;

        RAD_EXPORT_DECL bool complete(const std::error_code& ec,
                                      int result) noexcept override;
#endif // RAD_HAS_IO_URING
    };

    template <class Handler, class Alloc>
    struct async_file_impl::write_op final : write_op_base,
                                             allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_file_impl> impl;
        iovec_buffers buffers;
        Handler handler;
        std::error_code ec;

        template <class H>
        write_op(async_file_impl& impl, iovec_buffers&& buffs, H&& handler,
                 const Alloc& alloc)
            : alloc_base(alloc), impl{impl}, buffers{std::move(buffs)},
              handler{std::forward<H>(handler)} {
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and transferred to stack
            rad::detail::invoke_handler(this, std::error_code{ec},
                                        std::size_t{transferred});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->get_any_executor();
        }

        bool perform() noexcept override {
            assert(!canceled);
            auto result = impl->perform_async_write(false, buffers);
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            ec = result.error();
            return true;
        }

#ifdef RAD_HAS_IO_URING
        void submit(descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            descriptor->uring_backend->submit_writev(
                *descriptor, inner, impl->native_handle().get(),
                buffers.get_buffers(), buffers.get_count(), -1, ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            this->ec = ec;
            if (result >= 0) {
                transferred += result;
                buffers.advance(result);
                if (canceled || ec || buffers.get_count() == 0) {
                    return true;
                }
                return false;
            }
            return true;
        }
#endif // RAD_HAS_IO_URING
    };

    template <class Handler, class Alloc>
    struct async_file_impl::read_op final : read_op_base,
                                            allocator_storage<Alloc> {
        using alloc_base = allocator_storage<Alloc>;

        ref<async_file_impl> impl;
        iovec_buffers buffers;
        Handler handler;
        std::error_code ec;

        template <class H>
        read_op(async_file_impl& impl, iovec_buffers&& buffs, H&& handler,
                bool read_all, const Alloc& alloc)
            : alloc_base(alloc), impl{impl}, buffers{std::move(buffs)},
              handler{std::forward<H>(handler)} {
            this->read_all = read_all;
        }

        void invoke_operation() override {
            if (canceled) {
                ec = std::make_error_code(std::errc::operation_canceled);
            }
            // copy ec and transferred to stack
            rad::detail::invoke_handler(this, std::error_code{ec},
                                        std::size_t{transferred});
        }

        any_executor& associated_executor() const noexcept override {
            return impl->get_any_executor();
        }

        bool perform() noexcept override {
            assert(!canceled);
            auto result = impl->perform_async_read(buffers, read_all);
            transferred += result.transferred();
            if (result.is_pending()) {
                return false;
            }
            ec = result.error();
            return true;
        }

#ifdef RAD_HAS_IO_URING
        void submit(descriptor_data_inner_t& inner,
                    std::error_code& ec) noexcept override {
            descriptor->uring_backend->submit_readv(
                *descriptor, inner, impl->native_handle().get(),
                buffers.get_buffers(), buffers.get_count(), -1, ec);
        }

        bool complete(const std::error_code& ec, int result) noexcept override {
            if (!buffers.empty() && result == 0 && !ec) {
                this->ec = make_eof_error_code();
                return true;
            }
            this->ec = ec;
            if (result >= 0) {
                transferred += result;
                buffers.advance(result);
                if (ec || canceled || buffers.get_count() == 0 ||
                    (!read_all && transferred > 0)) {
                    return true;
                }
                return false;
            }
            return true;
        }
#endif // RAD_HAS_IO_URING
    };

    inline auto async_file_impl::async_write(const_buffer buff,
                                             std::error_code& ec)
        -> write_awaiter {
        make_error_if_descriptor_is_closed(is_open(), ec, "async_write");
        return {*this, buff, ec};
    }

    inline auto async_file_impl::async_read(mutable_buffer buff,
                                            std::error_code& ec)
        -> read_awaiter {
        make_error_if_descriptor_is_closed(is_open(), ec, "async_read");
        return {*this, buff, false, ec};
    }

    inline auto async_file_impl::async_read_all(mutable_buffer buff,
                                                std::error_code& ec)
        -> read_awaiter {
        make_error_if_descriptor_is_closed(is_open(), ec, "async_read");
        return {*this, buff, true, ec};
    }

    template <class Buffers>
    auto async_file_impl::async_writev(const Buffers& buffers,
                                       std::error_code& ec) -> writev_awaiter {
        make_error_if_descriptor_is_closed(is_open(), ec, "async_write");
        return writev_awaiter{*this, buffers, ec};
    }

    template <class Buffers>
    auto async_file_impl::async_readv(const Buffers& buffers,
                                      std::error_code& ec) -> readv_awaiter {
        make_error_if_descriptor_is_closed(is_open(), ec, "async_read");
        return readv_awaiter{*this, buffers, false, ec};
    }

    template <class Buffers>
    auto async_file_impl::async_readv_all(const Buffers& buffers,
                                          std::error_code& ec)
        -> readv_awaiter {
        return readv_awaiter{*this, buffers, true, ec};
    }
} // namespace RAD_LIB_NAMESPACE::io::detail