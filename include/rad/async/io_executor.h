#pragma once
#include <rad/async/executor.h>
#include <rad/net/types.h>
#include <rad/os_types.h>
#include <rad/trackable.h>
#ifdef _WIN32
#include <rad/ipc/pipe_endpoint.h>
#else
#include <rad/stack_list.h>
#include <rad/threading/mutex.h>
#include <rad/threading/synchronized_value.h>
#endif // _WIN32

#ifdef __unix__
#include <rad/io/posix/iovecs_buffers.h>
#endif // __unix__

#ifdef __linux__
extern "C" {
struct msghdr;
}
#endif // __linux__

namespace RAD_LIB_NAMESPACE {
    class io_executor;

#ifdef __linux__
    struct async_io_executor_backend;
#endif // __linux__

    namespace io::detail {
#ifdef _WIN32
        struct descriptor_data {};
        using descriptor_data_ptr = descriptor_data*;

        inline std::error_code make_eof_error_code() noexcept {
            constexpr int eof = 38L; // ERROR_HANDLE_EOF
            return std::error_code{eof, system_category()};
        }
#else
        struct descriptor_data;

        struct descriptor_data_deleter {
            io_executor* ex;

            descriptor_data_deleter(io_executor& ex) : ex{&ex} {
            }

            void operator()(descriptor_data* d) noexcept;
        };

        enum class descriptor_flags : uint8_t {
            none = 0,
            ready_out = 1 << 0,
            ready_in = 1 << 1,
            is_stream = 1 << 2,
            is_timer = 1 << 3,
            eof = 1 << 4,
        };

        RAD_OVERLOAD_ENUM_OPERATORS(descriptor_flags);

        struct io_op;

        struct descriptor_data_inner_t {
            io_op* out_op = nullptr;
            io_op* in_op = nullptr;
            descriptor_flags flags = descriptor_flags::none;
#ifdef RAD_HAS_IO_URING
            bool pending_delete = false;
            uint8_t pending_ops = 0;
#endif // RAD_HAS_IO_URING

#ifndef NDEBUG
            ~descriptor_data_inner_t() {
                assert(out_op == nullptr);
                assert(in_op == nullptr);
#ifdef RAD_HAS_IO_URING
                assert(pending_ops == 0);
#endif // RAD_HAS_IO_URING
            }
#endif // !NDEBUG

            bool ready_out() const noexcept {
                return flags & descriptor_flags::ready_out;
            }

            bool ready_in() const noexcept {
                return flags & descriptor_flags::ready_in;
            }

            bool is_stream() const noexcept {
                return flags & descriptor_flags::is_stream;
            }

            bool is_timer() const noexcept {
                return flags & descriptor_flags::is_timer;
            }

            void set_timer() noexcept {
                flags |= descriptor_flags::is_timer;
            }

            void set_stream() noexcept {
                flags |= descriptor_flags::is_stream;
            }

            void set_ready_out(bool ready) noexcept {
                if (ready) {
                    flags |= descriptor_flags::ready_out;
                }
                else {
                    flags &= ~descriptor_flags::ready_out;
                }
            }

            void set_ready_in(bool ready) noexcept {
                if (ready) {
                    flags |= descriptor_flags::ready_in;
                }
                else {
                    flags &= ~descriptor_flags::ready_in;
                }
            }

            bool is_eof() const noexcept {
                return flags & descriptor_flags::eof;
            }

            void set_eof() noexcept {
                flags |= descriptor_flags::eof;
            }

            bool out_pending() const noexcept {
                return out_op != nullptr;
            }

            bool in_pending() const noexcept {
                return in_op != nullptr;
            }

            void set_out(io_op* op) noexcept {
                assert(!out_op && "overwriting "
                                  "pending out "
                                  "operation !");
                out_op = op;
            }

            void set_in(io_op* op) noexcept {
                assert(!in_op && "overwriting "
                                 "pending in "
                                 "operation !");
                in_op = op;
            }

            RAD_EXPORT_DECL std::pair<io_op*, io_op*> perform(bool out,
                                                              bool in) noexcept;

            RAD_EXPORT_DECL std::pair<io_op*, io_op*> cancel() noexcept;
        };

        // this is the base of async operations for io fds
        struct io_op : public rad::detail::async_op_base {
            // if set to true the operation should fail
            // without trying because the io object may have
            // been destroyed
            bool canceled = false;

#ifdef RAD_HAS_IO_URING
            descriptor_data* descriptor = nullptr;
#endif // RAD_HAS_IO_URING

            io_op() = default;

            io_op(rad::detail::async_op_type type) noexcept
                : rad::detail::async_op_base(type) {
            }

            // returns true if the operation has finished
            // and the handler is ready to be executed note
            // that canceled can't be true inside this
            // method since the operation is canceled only
            // after it is moved from the descriptor_data
            // and then pushed to the finished operations
            // queue in the executor and perform is called
            // only for pending operations
            virtual bool perform() noexcept = 0;

#ifdef RAD_HAS_IO_URING
            virtual void submit(descriptor_data_inner_t& inner,
                                std::error_code& ec) noexcept = 0;

            /*!
             * @brief Try to complete the async operation.
             *
             * This function must not call any other io uring functions
             * since it is called with the io uring lock held.
             * @param ec Set to indicate error occured, if any.
             * @param result Either 0 or positive number.
             * @return True if the operation has completed.
             * If the operation was canceled or ec was set to error
             * the operation will be considered completed anyway.
             */
            virtual bool complete(const std::error_code& ec,
                                  int result) noexcept = 0;
#endif // RAD_HAS_IO_URING

        protected:
            ~io_op() = default;
        };

        // the base of fd data associated with epoll, kqueue and io_uring
        // inherit from stack_double_list_node to allow executor
        // delete_later to be noexcept and enable erase
        struct descriptor_data : public stack_double_list_node {
            os::file_handle handle;
            sync_value<descriptor_data_inner_t, executor_mutex> inner;
#ifdef RAD_HAS_IO_URING
            async_io_executor_backend* const uring_backend;

            descriptor_data(
                async_io_executor_backend* uring_backend = nullptr) noexcept
                : uring_backend{uring_backend} {
            }

            constexpr bool is_io_uring() const noexcept {
                return uring_backend != nullptr;
            }
#endif // RAD_HAS_IO_URING

            RAD_EXPORT_DECL void cancel(any_executor& ex) noexcept;
        };

        using descriptor_data_ptr =
            std::unique_ptr<descriptor_data, descriptor_data_deleter>;

        RAD_EXPORT_DECL const std::error_category& eof_category() noexcept;

        inline std::error_code make_eof_error_code() noexcept {
            return std::error_code(1, eof_category());
        }
#endif // _WIN32
    } // namespace io::detail

#ifdef RAD_HAS_IO_URING
    struct async_io_executor_backend {
        using descriptor_data = io::detail::descriptor_data;

        virtual ~async_io_executor_backend() = default;

        virtual void cancel_descriptor(descriptor_data& p) noexcept = 0;

        virtual void submit_writev(descriptor_data& d,
                                   io::detail::descriptor_data_inner_t& inner,
                                   int fd, const io::iovec_buff* iovecs,
                                   unsigned n_vecs, uint64_t offset,
                                   std::error_code& ec) noexcept = 0;

        virtual void submit_send(descriptor_data& d,
                                 io::detail::descriptor_data_inner_t& inner,
                                 int fd, const void* buff, std::size_t n,
                                 std::error_code& ec) noexcept = 0;

        virtual void submit_sendmsg(descriptor_data& d,
                                    io::detail::descriptor_data_inner_t& inner,
                                    int fd, msghdr* msg,
                                    std::error_code& ec) noexcept = 0;

        virtual void submit_sendto(descriptor_data& d,
                                   io::detail::descriptor_data_inner_t& inner,
                                   int fd, const void* buff, std::size_t n,
                                   const void* addr, socklen_t addrlen,
                                   std::error_code& ec) noexcept = 0;

        virtual void submit_readv(descriptor_data& d,
                                  io::detail::descriptor_data_inner_t& inner,
                                  int fd, const io::iovec_buff* iovecs,
                                  unsigned n_vecs, uint64_t offset,
                                  std::error_code& ec) = 0;

        virtual void submit_recv(descriptor_data& d,
                                 io::detail::descriptor_data_inner_t& inner,
                                 int fd, void* buff, std::size_t n,
                                 std::error_code& ec) noexcept = 0;

        virtual void submit_recvmsg(descriptor_data& d,
                                    io::detail::descriptor_data_inner_t& inner,
                                    int fd, msghdr* msg,
                                    std::error_code& ec) noexcept = 0;

        virtual void submit_connect(descriptor_data& d,
                                    io::detail::descriptor_data_inner_t& inner,
                                    int fd, const void* addr, size_t addr_len,
                                    std::error_code& ec) noexcept = 0;

        virtual void submit_accept(descriptor_data& d,
                                   io::detail::descriptor_data_inner_t& inner,
                                   int fd, void* addr, socklen_t* addr_len,
                                   std::error_code& ec) noexcept = 0;
    };
#endif // RAD_HAS_IO_URING

    /*!
     * @brief The base of all io executors. Currently only io_loop
     * implements io_executor.
     */
    class io_executor : public trackable {
    public:
        using descriptor_data = io::detail::descriptor_data;
        using descriptor_data_ptr = io::detail::descriptor_data_ptr;

        virtual ~io_executor() = default;

        /*!
         * @brief Casts this io_executor to any_executor
         * @return A reference to this executor casted to
         * any_executor
         */
        virtual any_executor& as_any_executor() noexcept = 0;

        /*!
         * @brief Check if this io_executor is backed by an
         * async os api like IOCP and io_uring, or a readiness
         * api like epoll and kqueue. In the latter case the
         * file descriptors attached to the executor need to be
         * in non-blocking mode.
         * @return true if this io_executor is backed by an
         * async os api, and false otherwise.
         */
        virtual bool is_async() const noexcept = 0;

        /*!
         * @brief Attach a socket handle to the io executor.
         * @param handle The socket handle.
         * @param data The io descriptor data associated with
         * the socket. This data is allocated using
         * allocate_descriptor_data()
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        virtual void attach_handle(net::socket_handle& handle,
                                   descriptor_data& data,
                                   std::error_code& ec) noexcept = 0;

#ifdef _WIN32
        /*!
         * @brief Attach a file handle to the io executor.
         * @param handle The file handle.
         * @param data The io descriptor data associated with
         * the file. This data is allocated using
         * allocate_descriptor_data()
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        virtual void attach_handle(os::file_handle& handle,
                                   descriptor_data& data,
                                   std::error_code& ec) noexcept = 0;
#endif // _WIN32

        /*!
         * @brief Get the executor used to emulate async
         * operations where it is not supported by the os. This
         * may include dns and file async operations
         * @return The executor used to emulate async dns
         * resolving.
         */
        virtual any_executor& thread_pool_executor() = 0;

        /*!
         * @brief Allocate the data associated with the io
         * descriptor. This data is shared between the io
         * executor and the attached io descriptor.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return An owning pointer to the allocated io
         * descriptor data on success, or nullptr on failure.
         */
        virtual descriptor_data_ptr
        allocate_descriptor_data(std::error_code& ec) noexcept = 0;

        /*!
         * @brief Delete the data associated with an attached io
         * descriptor. If there is any os file descriptors in
         * the io descriptor data the executor will take the
         * responsibility of closing them.
         * @param p Pointer to the attached io descriptor data.
         */
        virtual void delete_descriptor_data(descriptor_data* p) noexcept = 0;
    };

    /*!
     * @brief The type implementing io executor must satisfy these
     * requirements. It must derive from io_executor and derive from
     * any_executor to satisfy Executor requirements.
     */
    template <class Exec>
    concept IoExecutor =
        Executor<Exec> && std::is_convertible_v<Exec*, io_executor*>;

    /*!
     * @brief The type implementing both io executor and timer executor must
     * satisfy these requirements. It must derive from io_executor to
     * satisfy IoExecutor, derive from timer_executor to satisfy
     * TimerExecutor, and derive from any_executor to satisfy Executor
     * requirements.
     */
    template <class Exec>
    concept IoTimerExecutor = IoExecutor<Exec> && TimerExecutor<Exec>;

    /*!
     * @brief The type implementing proxy io executors (like strand over
     * io_loop) must satisfy these requirements. It must derive from
     * io_executor to satisfy IoExecutor, and satisfy the requirements of
     * ProxyExecutor.
     */
    template <class Exec>
    concept ProxyIoExecutor =
        ProxyExecutor<Exec> && IoExecutor<typename Exec::inner_executor_type>;

    /*!
     * @brief The type implementing proxy io timer executors (like strand
     * over io_loop) must satisfy these requirements. It must derive from
     * io_executor to satisfy IoExecutor, derive from timer_executor to
     * satisfy TimerExecutor, and satisfy the requirements of ProxyExecutor.
     */
    template <class Exec>
    concept ProxyIoTimerExecutor =
        ProxyExecutor<Exec> &&
        IoTimerExecutor<typename Exec::inner_executor_type>;

    enum class op_alloc_type {
        read = 1 << 0,
        write = 1 << 1,
        sendto = 1 << 2,
        recvfrom = 1 << 3,
        connect = 1 << 4,
        connect_range = 1 << 5,
        accept_peer_ref = 1 << 6,
        accept_no_peer_ref = 1 << 7,
        accept_pipe = 1 << 8,
        read_all = 1 << 9,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(op_alloc_type);

#ifndef _WIN32
    namespace io::detail {
        inline void
        descriptor_data_deleter::operator()(descriptor_data* d) noexcept {
            ex->delete_descriptor_data(d);
        }
    } // namespace io::detail
#endif // !_WIN32

} // namespace RAD_LIB_NAMESPACE