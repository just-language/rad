#pragma once
#include <rad/async/executor.h>
#include <rad/buffer.h>
#include <rad/os_types.h>
#include <rad/trackable.h>

#include <cassert>
#include <chrono>
#include <mutex>
#include <span>

#include <sys/event.h>

namespace RAD_LIB_NAMESPACE::io {
    /*!
     * @brief This struct has the same layout as `struct kevent`.
     */
    struct kqueue_event : public kevent {
        constexpr uintptr_t get_ident() const noexcept {
            return ident;
        }

        void* get_data() const noexcept {
            return reinterpret_cast<void*>(udata);
        }

        constexpr uint32_t get_events() const noexcept {
            return static_cast<uint32_t>(filter);
        }
    };

    static_assert(sizeof(kqueue_event) == sizeof(struct kevent));

    /*!
     * @brief A wrapper for `kqueue` handle.
     */
    class kqueue_handle {
    public:
        /// The native handle type wrapper.
        using native_handle_type = os::handle;

        /*!
         * @brief Construct a closed kqueue file descriptor.
         */
        kqueue_handle() = default;

        /*!
         * @brief Create a new kqueue instance.
         * @param max_threads The max threads hint.
         * It is currently ignored.
         */
        kqueue_handle(uint32_t max_threads) {
            std::ignore = max_threads;
            create();
        }

        /*!
         * @brief Get a reference to the native handle wrapper.
         * @return A reference to the native handle wrapper.
         */
        native_handle_type& native_handle() noexcept {
            return kq_fd_;
        }

        /*!
         * @brief Get a const reference to the native handle wrapper.
         * @return A const reference to the native handle wrapper.
         */
        const native_handle_type& native_handle() const noexcept {
            return kq_fd_;
        }

        /*!
         * @brief Create a new kqueue instance.
         * On failure the current kqueue instance is not changed.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void create(std::error_code& ec) noexcept;

        /*!
         * @brief Create a new kqueue instance.
         * On failure the current kqueue instance is not changed.
         */
        void create() {
            std::error_code ec;
            create(ec);
            check_and_throw(ec, "kqueue");
        }

        /*!
         * @brief Close the current kqueue instance, if any.
         */
        void close() noexcept {
            kq_fd_.reset();
        }

        /*!
         * @brief Check if the current kqueue instance is open.
         * @return True if the current kqueue instance is open,
         * otherwise false.
         */
        bool is_open() const noexcept {
            return static_cast<bool>(kq_fd_);
        }

        /*!
         * @brief Add a read only file descriptor to the current kqueue instance
         * interest list.
         * @param fd The target file descriptor.
         * The event reported by kqueue is EVFILT_READ.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void attach_handle(int fd, void* data,
                                           std::error_code& ec) noexcept;

        /*!
         * @brief Add a read only file descriptor to the current kqueue instance
         * interest list.
         * @param fd The target file descriptor.
         * The event reported by kqueue is EVFILT_READ.
         * @param data The user data.
         */
        void attach_handle(int fd, void* data) {
            std::error_code ec;
            attach_handle(fd, data, ec);
            check_and_throw(ec, "kevent");
        }

        /*!
         * @brief Add a read only file descriptor to the current kqueue instance
         * interest list.
         * @param fd The target file descriptor.
         * The event reported by kqueue is EVFILT_READ.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        void attach_handle(os::handle& fd, void* data,
                           std::error_code& ec) noexcept {
            attach_handle(fd.get(), data, ec);
        }

        /*!
         * @brief Add a read only file descriptor to the current kqueue instance
         * interest list.
         * @param fd The target file descriptor.
         * The event reported by kqueue is EVFILT_READ.
         * @param data The user data.
         */
        void attach_handle(os::handle& fd, void* data) {
            attach_handle(fd.get(), data);
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * kqueue instance interest list.
         * @param fd The target file descriptor.
         * The events reported by kqueue are EVFILT_READ and EVFILT_WRITE.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void
        attach_writable_handle(int fd, void* data,
                               std::error_code& ec) noexcept;

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * kqueue instance interest list.
         * @param fd The target file descriptor.
         * The events reported by kqueue are EVFILT_READ and EVFILT_WRITE.
         * @param data The user data.
         */
        void attach_writable_handle(int fd, void* data) {
            std::error_code ec;
            attach_writable_handle(fd, data, ec);
            check_and_throw(ec, "kevent");
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * kqueue instance interest list.
         * @param fd The target file descriptor.
         * The events reported by kqueue are EVFILT_READ and EVFILT_WRITE.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        void attach_writable_handle(os::handle& fd, void* data,
                                    std::error_code& ec) noexcept {
            attach_writable_handle(fd.get(), data, ec);
        }

        /*!
         * @brief Add a writable and readable file descriptor to the current
         * kqueue instance interest list.
         * @param fd The target file descriptor.
         * The events reported by kqueue are EVFILT_READ and EVFILT_WRITE.
         * @param data The user data.
         */
        void attach_writable_handle(os::handle& fd, void* data) {
            attach_writable_handle(fd.get(), data);
        }

        /*!
         * @brief Trigger a user event.
         * This may be used to unblock a call to `kevent`.
         *
         * The event will stay triggered and reported by
         * `kevent` until it is disabled.
         * @param ident The event identifier to use.
         * When the event is received the user may check for (ident,
         * EVFILT_USER) to check if it is the triggered event.
         * @param data The user data.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void trigger_event(uintptr_t ident, void* data,
                                           std::error_code& ec) noexcept;

        /*!
         * @brief Disable a triggered event.
         * @param ident The event identifier to use.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void disable_event(uintptr_t ident,
                                           std::error_code& ec) noexcept;

        /*!
         * @brief Disable a triggered event.
         * @param ident The event identifier to use.
         */
        void disable_event(uintptr_t ident) {
            std::error_code ec;
            disable_event(ident, ec);
            check_and_throw(ec, "kevent(EVFILT_USER, EV_DISABLE)");
        }

        /*!
         * @brief Remove a file descriptor from the current kqueue instance
         * interest list.
         * @param fd The target file descriptor.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void remove(int fd, std::error_code& ec) noexcept;

        /*!
         * @brief Remove a file descriptor from the current kqueue instance
         * interest list.
         * @param fd The target file descriptor.
         */
        void remove(int fd) {
            std::error_code ec;
            remove(fd, ec);
            check_and_throw(ec, "kevent(EPOLL_CTL_DEL)");
        }

        /*!
         * @brief Wait for an I/O event on the kqueue file descriptor.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param timeout The number of milliseconds that `kqueue_wait()` will
         * block if no I/O event is available.
         *
         * If @p timeout is 0, then `kqueue_wait()` will return immediately,
         * if no events are available.
         *
         * If @p timeout is less than 0, then `kqueue_wait()` will block
         * indefinitely until an I/O event arrives, or an error occurs.
         * @param ec Set to indicate error occured, if any.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        RAD_EXPORT_DECL std::span<kqueue_event>
        wait(std::span<kqueue_event> evs, std::chrono::nanoseconds timeout,
             std::error_code& ec) noexcept;

        /*!
         * @brief Wait for an I/O event on the kqueue file descriptor.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param timeout The number of milliseconds that kqueue_wait() will
         * block if no I/O event is available.
         *
         * If @p timeout is 0, then `kqueue_wait()` will return immediately,
         * if no events are available.
         *
         * If @p timeout is less than 0, then `kqueue_wait()` will block
         * indefinitely until an I/O event arrives, or an error occurs.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        std::span<kqueue_event> wait(std::span<kqueue_event> evs,
                                     std::chrono::nanoseconds timeout) {
            std::error_code ec;
            auto out = wait(evs, timeout, ec);
            check_and_throw(ec, "kqueue_wait");
            return out;
        }

        /*!
         * @brief Poll for an I/O event on the kqueue file descriptor.
         * The function will return immediately, if no events are available.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param ec Set to indicate error occured, if any.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        std::span<kqueue_event> poll(std::span<kqueue_event> evs,
                                     std::error_code& ec) noexcept {
            return wait(evs, std::chrono::nanoseconds{0}, ec);
        }

        /*!
         * @brief Poll for an I/O event on the kqueue file descriptor.
         * The function will return immediately, if no events are available.
         * @param evs The events buffer where output events will be stored.
         * If this buffer is empty, then this function is a no op.
         * @param ec Set to indicate error occured, if any.
         * @return A sub span of the input events buffer containing the output
         * events.
         */
        std::span<kqueue_event> poll(std::span<kqueue_event> evs) noexcept {
            return wait(evs, std::chrono::nanoseconds{0});
        }

    private:
        native_handle_type kq_fd_;
    };
} // namespace RAD_LIB_NAMESPACE::io