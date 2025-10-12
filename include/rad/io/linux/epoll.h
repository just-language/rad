#pragma once
#include <rad/async/executor.h>
#include <rad/buffer.h>
#include <rad/os_types.h>
#include <rad/trackable.h>

#include <cassert>
#include <chrono>
#include <mutex>
#include <span>

namespace RAD_LIB_NAMESPACE {
    class io_loop;
}

namespace RAD_LIB_NAMESPACE::io::detail {

    enum class fd_type {
        none,
        timer,  // the fd data is for a timerfd
        socket, // the fd data is for a non blocking socket
    };
} // namespace RAD_LIB_NAMESPACE::io::detail

namespace RAD_LIB_NAMESPACE::io {
    /*!
     * @brief The operation used with `epoll_ctl`.
     */
    enum class epoll_op : int {
        /*!
         * @brief EPOLL_CTL_ADD Add an entry to the interest list of the epoll
         * file descriptor.
         */
        add = 1,
        /*!
         * @brief EPOLL_CTL_DEL Remove (deregister) the target file descriptor
         * from the the interest list.
         */
        remove,
        /*!
         * @brief EPOLL_CTL_MOD Change the settings associated with in the
         * interest list.
         */
        modify,
    };

    /*!
     * @brief Epoll events bit mask.
     */
    enum class epoll_events : uint32_t {
        /*!
         * @brief EPOLLIN The associated file is available for read()
         * operations.
         */
        in = 1,
        /*!
         * @brief EPOLLOUT The associated file is available for write()
         * operations.
         */
        out = 4,
        /*!
         * @brief EPOLLRDHUP Stream socket peer closed connection, or shut down
         * writing half of connection.
         */
        rdhup = 0x2000,
        /*!
         * @brief EPOLLPRI There is an exceptional condition on the file
         * descriptor.
         */
        pri = 2,
        /*!
         * @brief EPOLLERR Error condition happened on the associated file
         * descriptor.
         */
        error = 8,
        /*!
         * @brief EPOLLHUP Hang up happened on the associated file descriptor.
         */
        hup = 16,
        /*!
         * @brief EPOLLET Requests edge-triggered notification for the
         * associated file descriptor.
         */
        edge_triggered = 1u << 31,
        /*!
         * @brief EPOLLONESHOT Requests one-shot notification for the associated
         * file descriptor.
         */
        one_shot = 1u << 30,
        /*!
         * @brief EPOLLWAKEUP.
         */
        wakeup = 1u << 29,
        /*!
         * @brief EPOLLEXCLUSIVE.
         */
        exclusive = 1u << 28,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(epoll_events);

    /*!
     * @brief This struct has the same layout as epoll_event.
     */
    struct epoll_event_t {
        epoll_events events;
        void* ptr;

        static constexpr uintptr_t get_ident() noexcept {
            return 0;
        }

        constexpr void* get_data() const noexcept {
            return ptr;
        }

        constexpr uint32_t get_events() const noexcept {
            return static_cast<uint32_t>(events);
        }
    } __attribute__((__packed__));

    /*!
     * @brief RAII Wrapper for epoll file descriptor.
     */
    class epoll {
    public:
        /// The native handle type wrapper.
        using native_handle_type = os::handle;

        /// The milli seconds duration used by `epoll_wait`.
        using duration = std::chrono::duration<int, std::milli>;

        /*!
         * @brief Construct a closed epoll file descriptor.
         */
        epoll() = default;

        /*!
         * @brief Create a new epoll instance.
         * @param max_threads The max threads hint.
         * It is currently ignored.
         */
        epoll(uint32_t max_threads) {
            create();
        }

        /*!
         * @brief Get a reference to the native handle wrapper.
         * @return A reference to the native handle wrapper.
         */
        native_handle_type& native_handle() noexcept {
            return epoll_fd;
        }

        /*!
         * @brief Get a const reference to the native handle wrapper.
         * @return A const reference to the native handle wrapper.
         */
        const native_handle_type& native_handle() const noexcept {
            return epoll_fd;
        }

        /*!
         * @brief Create a new epoll instance.
         * On failure the current epoll instance is not changed.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void create(std::error_code& ec) noexcept;

        /*!
         * @brief Create a new epoll instance.
         * On failure the current epoll instance is not changed.
         */
        void create() {
            std::error_code ec;
            create(ec);
            check_and_throw(ec, "epoll_create");
        }

        /*!
         * @brief Close the current epoll instance, if any.
         */
        void close() noexcept {
            epoll_fd.reset();
        }

        /*!
         * @brief Control the epoll file descriptor using `epoll_ctl`.
         * @param op Spcefies whether to add, remove or change events.
         * @param fd The target file descriptor.
         * @param ev The epoll event flags and user data.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void ctl(epoll_op op, int fd, const epoll_event_t& ev,
                                 std::error_code& ec) noexcept;

        /*!
         * @brief Control the epoll file descriptor using `epoll_ctl`.
         * @param op Spcefies whether to add, remove or change events.
         * @param fd The target file descriptor.
         * @param ev The epoll event flags and user data.
         */
        void ctl(epoll_op op, int fd, const epoll_event_t& ev) {
            std::error_code ec;
            ctl(op, fd, ev, ec);
            check_and_throw(ec, "epoll_ctl");
        }

        /*!
         * @brief Add a read only file descriptor to the current epoll instance
         * interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN and EPOLLRDHUB.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        void attach_handle(int fd, void* data, std::error_code& ec) noexcept {
            epoll_event_t ev;
            ev.events = epoll_events::edge_triggered | epoll_events::in |
                        epoll_events::rdhup;
            ev.ptr = data;
            ctl(epoll_op::add, fd, ev, ec);
        }

        /*!
         * @brief Add a read only file descriptor to the current epoll instance
         * interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN and EPOLLRDHUB.
         * @param data The user data.
         */
        void attach_handle(int fd, void* data) {
            std::error_code ec;
            attach_handle(fd, data, ec);
            check_and_throw(ec, "epoll_ctl(EPOLL_CTL_ADD)");
        }

        /*!
         * @brief Add a read only file descriptor to the current epoll instance
         * interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN and EPOLLRDHUB.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        void attach_handle(os::handle& fd, void* data,
                           std::error_code& ec) noexcept {
            attach_handle(fd.get(), data, ec);
        }

        /*!
         * @brief Add a read only file descriptor to the current epoll instance
         * interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN and EPOLLRDHUB.
         * @param data The user data.
         */
        void attach_handle(os::handle& fd, void* data) {
            attach_handle(fd.get(), data);
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * epoll instance interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN, EPOLLOUT and EPOLLRDHUB.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        void attach_writable_handle(int fd, void* data,
                                    std::error_code& ec) noexcept {
            epoll_event_t ev;
            ev.events = epoll_events::edge_triggered | epoll_events::out |
                        epoll_events::in | epoll_events::rdhup;
            ev.ptr = data;
            ctl(epoll_op::add, fd, ev, ec);
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * epoll instance interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN, EPOLLOUT and EPOLLRDHUB.
         * @param data The user data.
         */
        void attach_writable_handle(int fd, void* data) {
            std::error_code ec;
            attach_writable_handle(fd, data, ec);
            check_and_throw(ec, "epoll_ctl(EPOLL_CTL_ADD)");
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * epoll instance interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN, EPOLLOUT and EPOLLRDHUB.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        void attach_writable_handle(os::handle& fd, void* data,
                                    std::error_code& ec) noexcept {
            attach_writable_handle(fd.get(), data, ec);
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * epoll instance interest list.
         * @param fd The target file descriptor.
         * The events reported by epoll EPOLLIN, EPOLLOUT and EPOLLRDHUB.
         * @param data The user data.
         */
        void attach_writable_handle(os::handle& fd, void* data) {
            attach_writable_handle(fd.get(), data);
        }

        /*!
         * @brief Remove a file descriptor from the current epoll instance
         * interest list.
         * @param fd The target file descriptor.
         * @param ec Set to indicate error occured, if any.
         */
        void remove(int fd, std::error_code& ec) noexcept {
            epoll_event_t ev{};
            ctl(epoll_op::remove, fd, ev, ec);
        }

        /*!
         * @brief Remove a file descriptor from the current epoll instance
         * interest list.
         * @param fd The target file descriptor.
         */
        void remove(int fd) {
            std::error_code ec;
            remove(fd, ec);
            check_and_throw(ec, "epoll_ctl(EPOLL_CTL_DEL)");
        }

        /*!
         * @brief Wait for an I/O event on the epoll file descriptor.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param timeout The number of milliseconds that `epoll_wait()` will
         * block if no I/O event is available.
         *
         * If @p timeout is 0, then `epoll_wait()` will return immediately,
         * if no events are available.
         *
         * If @p timeout is less than 0, then `epoll_wait()` will block
         * indefinitely until an I/O event arrives, or an error occurs.
         * @param ec Set to indicate error occured, if any.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        RAD_EXPORT_DECL std::span<epoll_event_t>
        wait(std::span<epoll_event_t> evs, duration timeout,
             std::error_code& ec) noexcept;

        /*!
         * @brief Wait for an I/O event on the epoll file descriptor.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param timeout The number of milliseconds that epoll_wait() will
         * block if no I/O event is available.
         *
         * If @p timeout is 0, then `epoll_wait()` will return immediately,
         * if no events are available.
         *
         * If @p timeout is less than 0, then `epoll_wait()` will block
         * indefinitely until an I/O event arrives, or an error occurs.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        std::span<epoll_event_t> wait(std::span<epoll_event_t> evs,
                                      duration timeout) {
            std::error_code ec;
            auto out = wait(evs, timeout, ec);
            check_and_throw(ec, "epoll_wait");
            return out;
        }

        /*!
         * @brief Poll for an I/O event on the epoll file descriptor.
         * The function will return immediately, if no events are available.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param ec Set to indicate error occured, if any.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        std::span<epoll_event_t> poll(std::span<epoll_event_t> evs,
                                      std::error_code& ec) noexcept {
            return wait(evs, duration{0}, ec);
        }

        /*!
         * @brief Poll for an I/O event on the epoll file descriptor.
         * The function will return immediately, if no events are available.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param ec Set to indicate error occured, if any.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        std::span<epoll_event_t> poll(std::span<epoll_event_t> evs) noexcept {
            return wait(evs, duration{0});
        }

    private:
        native_handle_type epoll_fd;
    };
} // namespace RAD_LIB_NAMESPACE::io