#pragma once
#include <rad/async/executor.h>
#include <rad/net/types.h>
#include <rad/os_types.h>

#include <cassert>
#include <chrono>
#include <span>

namespace RAD_LIB_NAMESPACE::io::detail {
    namespace details = RAD_LIB_NAMESPACE::detail;

    struct overlapped_result {
        overlapped_result() {
            offset32low = 0;
            offset32high = 0;
            pointer = nullptr;
        }

        overlapped_result(uintptr_t offset) {
            offset32low = 0;
            offset32high = 0;
            pointer = reinterpret_cast<void*>(offset);
        }

        uintptr_t error_code = 0;
        uintptr_t internal_high = 0;
        union {
            struct {
                unsigned long offset32low;
                unsigned long offset32high;
            };
            void* pointer;
        };
        void* event_handle = nullptr;

        LPOVERLAPPED as_winov() noexcept {
            return reinterpret_cast<LPOVERLAPPED>(this);
        }
    };

    class io_op : public details::async_op_base {
    public:
        io_op() = default;

        io_op(details::async_op_type type) noexcept
            : details::async_op_base(type) {
        }

        LPOVERLAPPED get_ov_ptr() noexcept {
            return ov.as_winov();
        }

        void set_ov_ec(DWORD dw_ec) noexcept {
            ov.error_code = dw_ec;
        }

        DWORD get_ov_ec() const noexcept {
            return static_cast<DWORD>(ov.error_code);
        }

        static io_op* from_ov_ptr(LPOVERLAPPED ov_ptr) noexcept {
#if !defined(_MSC_VER) || defined(__clang__)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif // __clang__
#endif
            // UB is not allowed in constexpr so if the following
            // line compile, the compiler has well defined behavior for this
            // offsetof.
            constexpr intptr_t ov_offset = offsetof(io_op, ov);
#if !defined(_MSC_VER) || defined(__clang__)
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif
            return reinterpret_cast<io_op*>(reinterpret_cast<char*>(ov_ptr) -
                                            ov_offset);
        }

        static io_op* from_ov_ptr(overlapped_result* ov_ptr) noexcept {
            return from_ov_ptr(ov_ptr->as_winov());
        }

    private:
        overlapped_result ov;
    };

} // namespace RAD_LIB_NAMESPACE::io::detail

namespace RAD_LIB_NAMESPACE::io::iocp {
    /*!
     * @brief Wrapper for `OVERLAPPED_ENTRY` struct.
     *
     * This struct has the same layout as `OVERLAPPED_ENTRY` struct.
     */
    struct completion_result {
        /*!
        typedef struct _OVERLAPPED_ENTRY {
            ULONG_PTR lpCompletionKey;
            LPOVERLAPPED lpOverlapped;
            ULONG_PTR Internal;
            DWORD dwNumberOfBytesTransferred;
        } OVERLAPPED_ENTRY, *LPOVERLAPPED_ENTRY;
        */

        /*!
         * @brief The key type used for overlapped entry.
         * It has `ULONG_PTR` type.
         */
        using key_t = uintptr_t;

        completion_result() noexcept = default;

        completion_result(std::uint32_t bytes, key_t key,
                          detail::overlapped_result* ov) noexcept
            : key{key}, ov{ov}, transferred{bytes} {
        }

        /*!
         * @brief Get the error code of the pointed to overlapped struct.
         * The overlapped pointer must be non null.
         * @return The error code of the pointed to overlapped struct.
         */
        std::error_code error_code() const noexcept {
            assert(ov != nullptr);
            return std::error_code{static_cast<int>(ov->error_code),
                                   system_category()};
        }

        /*
        template <class T>
        void store_payload(uintptr_t key, T& payload) noexcept {
            this->key = key;
            ov = reinterpret_cast<detail::overlapped_result*>(&payload);
        }
        */

        /*
        template <class T>
        T& get_payload() noexcept {
            return *reinterpret_cast<T*>(ov);
        }
        */

        /// The key corresponding to `lpCompletionKey`.
        key_t key = 0;
        /// The overlapped corresponding to `lpOverlapped`.
        detail::overlapped_result* ov = nullptr;

    private:
        /// Internal.
        [[maybe_unused]] uintptr_t internal = 0;

    public:
        /// The transferred bytes corresponding to `dwNumberOfBytesTransferred`.
        uint32_t transferred = 0;
    };

    /*!
     * @brief RAII wrapper for (I/O) Completion Port.
     */
    class io_port {
    public:
        /*!
         * @brief Construct closed (I/O) Completion Port.
         */
        io_port() = default;

        /*!
         * @brief Create a new (I/O) Completion Port.
         * @param max_threads The maximum number of threads that the operating
         * system can allow to concurrently process I/O completion packets for
         * the I/O completion port.
         *
         * If this parameter is zero, the system allows as many concurrently
         * running threads as there are processors in the system.
         * @param ec Set to indicate error occured, if any.
         * On failure, the current (I/O) Completion Port is not changed.
         */
        RAD_EXPORT_DECL void create(uint32_t max_threads,
                                    std::error_code& ec) noexcept;

        /*!
         * @brief Create a new (I/O) Completion Port.
         * @param max_threads The maximum number of threads that the operating
         * system can allow to concurrently process I/O completion packets for
         * the I/O completion port.
         *
         * If this parameter is zero, the system allows as many concurrently
         * running threads as there are processors in the system.
         */
        void create(uint32_t max_threads = 0) {
            std::error_code ec;
            create(max_threads, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Associate a pipe handle to the (I/O) Completion Port.
         * @param pipe The pipe handle.
         * @param key The per-handle user-defined completion key that is
         * included in every I/O completion packet for the specified handle.
         * @param ec Set to indicate error occured, if any.
         */
        void add_handle(os::handle& pipe, uintptr_t key,
                        std::error_code& ec) noexcept {
            add_handle(pipe.get(), key, ec);
        }

        /*!
         * @brief Associate a pipe handle to the (I/O) Completion Port.
         * @param pipe The pipe handle.
         * @param key The per-handle user-defined completion key that is
         * included in every I/O completion packet for the specified handle.
         */
        void add_handle(os::handle& pipe, uintptr_t key) {
            std::error_code ec;
            add_handle(pipe, key, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Associate a file handle to the (I/O) Completion Port.
         * @param file The file handle.
         * @param key The per-handle user-defined completion key that is
         * included in every I/O completion packet for the specified handle.
         * @param ec Set to indicate error occured, if any.
         */
        void add_handle(os::file_handle& file, uintptr_t key,
                        std::error_code& ec) noexcept {
            add_handle(file.get(), key, ec);
        }

        /*!
         * @brief Associate a file handle to the (I/O) Completion Port.
         * @param file The file handle.
         * @param key The per-handle user-defined completion key that is
         * included in every I/O completion packet for the specified handle.
         */
        void add_handle(os::file_handle& file, uintptr_t key) {
            std::error_code ec;
            add_handle(file, key, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Associate a socket handle to the (I/O) Completion Port.
         * @param sock The socket handle.
         * @param key The per-handle user-defined completion key that is
         * included in every I/O completion packet for the specified handle.
         * @param ec Set to indicate error occured, if any.
         */
        void add_handle(net::socket_handle& sock, uintptr_t key,
                        std::error_code& ec) noexcept {
            add_handle(
                reinterpret_cast<void*>(static_cast<socket_fd_t>(sock.get())),
                key, ec);
        }

        /*!
         * @brief Associate a socket handle to the (I/O) Completion Port.
         * @param sock The socket handle.
         * @param key The per-handle user-defined completion key that is
         * included in every I/O completion packet for the specified handle.
         */
        void add_handle(net::socket_handle& sock, uintptr_t key) {
            std::error_code ec;
            add_handle(sock, key, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get one completion entry from the (I/O) Completion Port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @param result The completion entry.
         * @param wait_time_ms The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time_ms is the max uint32_t value, the function will never
         * time out.
         *
         * If @p wait_time_ms is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void get_result(completion_result& result,
                                        uint32_t wait_time_ms,
                                        std::error_code& ec) noexcept;

        /*!
         * @brief Get one completion entry from the (I/O) completion port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @param result The completion entry.
         * @param wait_time The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time in milliseconds is the max uint32_t value, the
         * function will never time out.
         *
         * If @p wait_time is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param ec Set to indicate error occured, if any.
         */
        template <class Rep, class Period>
        void get_result(completion_result& result,
                        const std::chrono::duration<Rep, Period>& wait_time,
                        std::error_code& ec) {
            using namespace std::chrono;
            using duration_type = duration<uint32_t, std::milli>;
            auto wait_timeout_ms = duration_cast<duration_type>(wait_time);
            get_result(result, wait_timeout_ms.count(), ec);
        }

        /*!
         * @brief Get multiple completion entries from the (I/O) completion
         * port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @param results The pointer to completion entries.
         * @param results_count The count of entries pointed to by @p results.
         * @param results_removed On return it will contain the number of
         * removed completion entries from the (I/O) completion port.
         * @param wait_time_ms The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time in milliseconds is the max uint32_t value, the
         * function will never time out.
         *
         * If @p wait_time is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param alterable If this parameter is false, the function does not
         * return until the time-out period has elapsed or an entry is
         * retrieved.
         *
         * If the parameter is TRUE and there are no available entries, the
         * function performs an alertable wait. The thread returns when the
         * system queues an I/O completion routine or APC to the thread and the
         * thread executes the function.
         * @param ec Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void get_results(completion_result* results,
                                         uint32_t results_count,
                                         uint32_t& results_removed,
                                         uint32_t wait_time_ms, bool alterable,
                                         std::error_code& ec) noexcept;

        /*!
         * @brief Get multiple completion entries from the (I/O) completion
         * port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @param results The pointer to completion entries.
         * @param results_count The count of entries pointed to by @p results.
         * @param results_removed On return it will contain the number of
         * removed completion entries from the (I/O) completion port.
         * @param wait_time_ms The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time in milliseconds is the max uint32_t value, the
         * function will never time out.
         *
         * If @p wait_time is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param alterable If this parameter is false, the function does not
         * return until the time-out period has elapsed or an entry is
         * retrieved.
         *
         * If the parameter is TRUE and there are no available entries, the
         * function performs an alertable wait. The thread returns when the
         * system queues an I/O completion routine or APC to the thread and the
         * thread executes the function.
         */
        void get_results(completion_result* results, uint32_t results_count,
                         uint32_t& results_removed, uint32_t wait_time_ms,
                         bool alterable = false) {
            std::error_code ec;
            get_results(results, results_count, results_removed, wait_time_ms,
                        alterable, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get multiple completion entries from the (I/O) completion
         * port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @tparam N The count of entries in @p results.
         * @param results The array of completion entries.
         * @param results_removed On return it will contain the number of
         * removed completion entries from the (I/O) completion port.
         * @param wait_time_ms The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time in milliseconds is the max uint32_t value, the
         * function will never time out.
         *
         * If @p wait_time is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param alterable If this parameter is false, the function does not
         * return until the time-out period has elapsed or an entry is
         * retrieved.
         *
         * If the parameter is TRUE and there are no available entries, the
         * function performs an alertable wait. The thread returns when the
         * system queues an I/O completion routine or APC to the thread and the
         * thread executes the function.
         * @param ec Set to indicate error occured, if any.
         */
        template <std::size_t N>
        void get_results(completion_result (&results)[N],
                         uint32_t& results_removed, uint32_t wait_time_ms,
                         bool alterable, std::error_code& ec) noexcept {
            return get_results(results, N, results_removed, wait_time_ms,
                               alterable, ec);
        }

        /*!
         * @brief Get multiple completion entries from the (I/O) completion
         * port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @tparam N The count of entries in @p results.
         * @param results The array of completion entries.
         * @param results_removed On return it will contain the number of
         * removed completion entries from the (I/O) completion port.
         * @param wait_time_ms The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time in milliseconds is the max uint32_t value, the
         * function will never time out.
         *
         * If @p wait_time is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param alterable If this parameter is false, the function does not
         * return until the time-out period has elapsed or an entry is
         * retrieved.
         *
         * If the parameter is TRUE and there are no available entries, the
         * function performs an alertable wait. The thread returns when the
         * system queues an I/O completion routine or APC to the thread and the
         * thread executes the function.
         */
        template <std::size_t N>
        void get_results(completion_result (&results)[N],
                         uint32_t& results_removed, uint32_t wait_time_ms,
                         bool alterable = false) {
            std::error_code ec;
            get_results(results, results_removed, wait_time_ms, alterable, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get multiple completion entries from the (I/O) completion
         * port.
         *
         * If there is no completion packet queued, the function waits for a
         * pending I/O operation associated with the completion port to
         * complete.
         * @tparam N The count of entries in @p results.
         * @param results The array of completion entries.
         * @param wait_time_ms The number of milliseconds that the caller is
         * willing to wait for a completion packet to appear at the completion
         * port.
         *
         * If a completion packet does not appear within the specified time, the
         * function times out, and sets @p ec to error.
         *
         * If @p wait_time in milliseconds is the max uint32_t value, the
         * function will never time out.
         *
         * If @p wait_time is 0 and there is no I/O operation to dequeue, the
         * function will time out immediately.
         * @param alterable If this parameter is false, the function does not
         * return until the time-out period has elapsed or an entry is
         * retrieved.
         *
         * If the parameter is TRUE and there are no available entries, the
         * function performs an alertable wait. The thread returns when the
         * system queues an I/O completion routine or APC to the thread and the
         * thread executes the function.
         * @param ec Set to indicate error occured, if any.
         * @return A span pointing to @p results and its size is the count of
         * the removed entries of the (I/O) completion port.
         */
        template <std::size_t N>
        std::span<completion_result>
        get_results(std::array<completion_result, N>& results,
                    uint32_t wait_time_ms, bool alertable,
                    std::error_code& ec) noexcept {
            if (!N) {
                return {};
            }
            uint32_t extracted = 0;
            get_results(results.data(), as<uint32_t>(results.size()), extracted,
                        wait_time_ms, alertable, ec);
            return std::span<completion_result>{results}.subspan(0, extracted);
        }

        /*!
         * @brief Posts an I/O completion packet to an I/O completion port.
         * @param result The I/O completion packet.
         * @param ec  Set to indicate error occured, if any.
         */
        RAD_EXPORT_DECL void post(const completion_result& result,
                                  std::error_code& ec) noexcept;

        /*!
         * @brief Posts an I/O completion packet to an I/O completion port.
         * @param result The I/O completion packet.
         */
        void post(const completion_result& result) {
            std::error_code ec;
            post(result, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Close the I/O completion port if it is open.
         */
        void close() noexcept {
            port_.reset();
        }

        /*!
         * @brief Skip posting a completion packet when an async operation
         * on the handle completes synchronously.
         * @param handle The handle associated with the IOCP.
         * @param ec Set to indicate error occured, if any.
         */
        static void skip_iocp_on_sucess(os::file_handle& handle,
                                        std::error_code& ec) noexcept {
            skip_iocp_on_sucess(handle.get(), ec);
        }

        /*!
         * @brief Skip posting a completion packet when an async operation
         * on the handle completes synchronously.
         * @param handle The handle associated with the IOCP.
         * @param ec Set to indicate error occured, if any.
         */
        static void skip_iocp_on_sucess(os::handle& handle,
                                        std::error_code& ec) noexcept {
            skip_iocp_on_sucess(handle.get(), ec);
        }

        /*!
         * @brief Skip posting a completion packet when an async operation
         * on the handle completes synchronously.
         * @param handle The handle associated with the IOCP.
         * @param ec Set to indicate error occured, if any.
         */
        static void skip_iocp_on_sucess(net::socket_handle& handle,
                                        std::error_code& ec) noexcept {
            skip_iocp_on_sucess(
                reinterpret_cast<void*>(static_cast<socket_fd_t>(handle.get())),
                ec);
        }

    private:
        RAD_EXPORT_DECL void add_handle(void* handle, uintptr_t key,
                                        std::error_code& ec) noexcept;

        RAD_EXPORT_DECL static void
        skip_iocp_on_sucess(void* handle, std::error_code& ec) noexcept;

        os::handle port_;
    };
}; // namespace RAD_LIB_NAMESPACE::io::iocp