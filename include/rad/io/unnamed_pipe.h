#pragma once
#ifdef _WIN32
#include <rad/io/windows/unnamed_pipe_impl.h>
#elif defined(__unix__)
#include <rad/io/posix/unnamed_pipe_impl.h>
#endif
#include <rad/detail/string_converter.h>

namespace RAD_LIB_NAMESPACE::io {
    class unnamed_pipe {
        using implementation_type = io::detail::unnamed_pipe_impl;

        using string_converter = rad::detail::string_converter<
            typename implementation_type::native_string_type,
            typename implementation_type::alternative_string_type1,
            typename implementation_type::alternative_string_type2>;

        unnamed_pipe(implementation_type&& impl) noexcept
            : impl_{std::move(impl)} {
        }

    public:
        using executor_type = typename implementation_type::executor_type;
        using lowest_layer_type = unnamed_pipe;
        using native_handle_type =
            typename implementation_type::native_handle_type;
        using native_fd_type = typename implementation_type::native_fd_type;

        unnamed_pipe(executor_type& ex) noexcept : impl_{ex} {
        }

        unnamed_pipe(executor_type& ex, native_handle_type& handle) noexcept
            : impl_{ex, handle} {
        }

        static std::pair<unnamed_pipe, unnamed_pipe>
        create_pair(executor_type& ex, std::error_code& ec) noexcept {
            auto [impl1, impl2] = implementation_type::create_pair(ex, ec);
            return {unnamed_pipe{std::move(impl1)},
                    unnamed_pipe{std::move(impl2)}};
        }

        static std::pair<unnamed_pipe, unnamed_pipe>
        create_pair(executor_type& ex) {
            std::error_code ec;
            auto pipes = create_pair(ex, ec);
            check_and_throw(ec, __func__);
            return pipes;
        }

        lowest_layer_type& lowest_layer() noexcept {
            return *this;
        }

        const lowest_layer_type& lowest_layer() const noexcept {
            return *this;
        }

        native_handle_type& native_handle() noexcept {
            return impl_.native_handle();
        }

        const native_handle_type& native_handle() const noexcept {
            return impl_.native_handle();
        }

        native_fd_type native_fd() const noexcept {
            return native_handle().get();
        }

        executor_type& executor() noexcept {
            return impl_.executor();
        }

        const executor_type& executor() const noexcept {
            return impl_.executor();
        }

        bool is_open() const noexcept {
            return impl_.is_open();
        }

        explicit operator bool() const noexcept {
            return is_open();
        }

        void close() noexcept {
            impl_.close();
        }

        void cancel() noexcept {
            impl_.cancel();
        }

        auto async_write(const_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_write(buff, ec);
        }

        auto async_read_some(mutable_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_read(buff, ec);
        }

        auto async_read(mutable_buffer buff, std::error_code& ec = no_ec) {
            return impl_.async_read_all(buff, ec);
        }

        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            return impl_.write(buff, ec);
        }

        std::size_t write(const const_buffer& buff) {
            std::error_code ec;
            auto written = write(buff, ec);
            check_and_throw(ec, __func__);
            return written;
        }

        template <WriteHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_write(const_buffer buffer, Handler&& handler,
                         const Alloc& alloc = Alloc()) {
            impl_.async_write(buffer, std::forward<Handler>(handler), alloc);
        }

        std::size_t read_some(const mutable_buffer& buff,
                              std::error_code& ec) noexcept {
            return impl_.read(buff, ec);
        }

        std::size_t read_some(const mutable_buffer& buff) {
            std::error_code ec;
            auto read_num = read_some(buff, ec);
            check_and_throw(ec, __func__);
            return read_num;
        }

        void read(mutable_buffer buff, std::error_code& ec) noexcept {
            do {
                buff += read_some(buff, ec);
            } while (!buff.empty() && !ec);
        }

        void read(const mutable_buffer& buff) {
            std::error_code ec;
            read(buff, ec);
            check_and_throw(ec, __func__);
        }

        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read_some(mutable_buffer buffer, Handler&& handler,
                             const Alloc& alloc = Alloc()) {
            impl_.async_read(buffer, std::forward<Handler>(handler), alloc);
        }

        template <ReadHandler Handler,
                  HandlerAllocator Alloc = default_io_allocator>
        void async_read(mutable_buffer buffer, Handler&& handler,
                        const Alloc& alloc = Alloc()) {
            impl_.async_read_all(buffer, std::forward<Handler>(handler), alloc);
        }

    private:
        implementation_type impl_;
    };

    struct two_pipe_ends {
        unnamed_pipe read_end;
        unnamed_pipe write_end;
    };
} // namespace RAD_LIB_NAMESPACE::io
