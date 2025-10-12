#pragma once
#include <rad/libbase.h>
#include <rad/os_types.h>
#include <rad/sysinfo.h>

#include <cassert>
#include <chrono>
#include <tuple>
#include <utility>
#ifdef _WIN32
#include <intrin.h>
#include <process.h>
#else
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif // _WIN32

namespace RAD_LIB_NAMESPACE {

    namespace detail {
        template <class Func, class... Args>
        struct thread_params {
        public:
            thread_params(Func&& func, Args&&... args)
                : fn{decay_copy(std::forward<Func>(func))},
                  args_tuple{decay_copy(std::forward<Args>(args))...} {
            }

            void invoke() {
                std::apply(fn, args_tuple);
            }

        private:
            Func fn;
            std::tuple<Args...> args_tuple;

            template <class T>
            std::decay_t<T> decay_copy(T&& v) {
                return std::forward<T>(v);
            }
        };

        inline unsigned int get_cpu_cores_num() noexcept {
            return sysinfo::cores();
        }

#ifdef _WIN32
        using native_thread_handle_type = os::handle;
        using thread_id_type = unsigned int;
        using thread_fn_type = unsigned __stdcall(void*);
        using thread_sleep_duration_type =
            std::chrono::duration<uint32_t, std::milli>;

        inline constexpr bool equal_thread_ids(thread_id_type id1,
                                               thread_id_type id2) noexcept {
            return id1 == id2;
        }
#else
        struct native_thread_handle_type : noncopyable {
            pthread_t thd = {};

            native_thread_handle_type() = default;

            native_thread_handle_type(pthread_t thd) noexcept : thd{thd} {
            }

            native_thread_handle_type(
                native_thread_handle_type&& other) noexcept
                : thd{std::exchange(other.thd, pthread_t{})} {
            }

            native_thread_handle_type&
            operator=(native_thread_handle_type&& other) noexcept {
                thd = std::exchange(other.thd, pthread_t{});
                return *this;
            }

            operator bool() const noexcept {
                return thd != pthread_t{};
            }

            bool operator==(std::nullptr_t) const noexcept {
                return thd == pthread_t{};
            }

            void reset() noexcept {
                thd = {};
            }

            pthread_t get() const noexcept {
                return thd;
            }
        };

        using thread_id_type = pthread_t;
        using thread_fn_type = void*(void*);
        using thread_sleep_duration_type = std::chrono::seconds;

        inline bool equal_thread_ids(thread_id_type id1,
                                     thread_id_type id2) noexcept {
            return ::pthread_equal(id1, id2);
        }
#endif // _WIN32

        RAD_EXPORT_DECL void join_thd(native_thread_handle_type& handle);

        struct thread_start_result_t {
            native_thread_handle_type handle;
            thread_id_type id;
        };

        RAD_EXPORT_DECL thread_start_result_t start_thd(thread_fn_type* f,
                                                        void* arg) noexcept;

        RAD_EXPORT_DECL void
        detach_thd(native_thread_handle_type& handle) noexcept;

        RAD_EXPORT_DECL void sleep_thd(std::chrono::nanoseconds nanos,
                                       thread_sleep_duration_type duration);
    } // namespace detail

    struct thread_id {
        using id_type = detail::thread_id_type;

    public:
        friend class thread;

        thread_id() = default;

        thread_id(id_type id) : id_{id} {
        }

        thread_id(const thread_id&) = default;

        thread_id(thread_id&& other) noexcept : id_{other.id_} {
            other.invalidate();
        }

        thread_id& operator=(const thread_id&) = default;

        thread_id& operator=(thread_id&& other) noexcept {
            id_ = other.id_;
            other.invalidate();
            return *this;
        }

        id_type id() const noexcept {
            return id_;
        }

        friend bool operator==(const thread_id& lhs,
                               const thread_id& rhs) noexcept {
            return detail::equal_thread_ids(lhs.id(), rhs.id());
        }

        bool operator>(const thread_id& other) const {
            return id() > other.id();
        }

        bool operator>=(const thread_id& other) const {
            return id() >= other.id();
        }

        bool operator<(const thread_id& other) const {
            return id() < other.id();
        }

        bool operator<=(const thread_id& other) const {
            return id() <= other.id();
        }

    private:
        void invalidate() noexcept {
            id_ = id_type{};
        }

        id_type id_ = id_type{};
    };

    namespace this_thread {
        RAD_EXPORT_DECL thread_id get_id() noexcept;
    }

    class thread {
    public:
        using native_handle_type = detail::native_thread_handle_type;

        thread() = default;

        template <class Fn, class... Args>
        thread(Fn&& f, Args&&... args) {
            using params_t = detail::thread_params<Fn, Args...>;
            params_t* params =
                new params_t{std::forward<Fn>(f), std::forward<Args>(args)...};

            auto res = detail::start_thd(&thread::thread_fn<params_t>, params);

            if (!res.handle) {
                delete params;
                throw std::system_error(errno, system_category());
            }

            handle_ = std::move(res.handle);
            id_ = std::move(res.id);
        }

        thread(thread&&) = default;

        thread& operator=(thread&& other) noexcept {
            if (joinable()) {
                std::terminate();
            }

            handle_ = std::move(other.handle_);
            id_ = std::move(other.id_);
            return *this;
        }

        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        thread_id get_id() const noexcept {
            return id_;
        }

        void detach() noexcept {
            assert(handle_ != nullptr && id_.id() != 0);
            detail::detach_thd(handle_);
            handle_.reset();
            id_.invalidate();
        }

        bool joinable() const noexcept {
            return get_id() != thread_id{};
        }

        void join() {
            check_if_valid_join();
            detail::join_thd(handle_);
            detach();
        }

        static unsigned int hardware_concurrency() noexcept {
            return detail::get_cpu_cores_num();
        }

        ~thread() {
            if (joinable()) {
                std::terminate();
            }
        }

    private:
        template <class ParamsType>
#ifdef _WIN32
        static unsigned __stdcall thread_fn(void* args)
#else
        static void* thread_fn(void* args)
#endif
        {
            class delete_on_exit {
            public:
                delete_on_exit(ParamsType* ptr) : ptr{ptr} {
                }

                ParamsType* operator->() {
                    return ptr;
                }

                ~delete_on_exit() {
                    delete ptr;
                }

            private:
                ParamsType* ptr;
            };

            delete_on_exit params{reinterpret_cast<ParamsType*>(args)};
            params->invoke();
            return 0;
        }

        void check_if_valid_join() {
            if (!handle_) {
                throw std::system_error(
                    std::make_error_code(std::errc::no_such_process));
            }
            if (get_id() == this_thread::get_id()) {
                throw std::system_error(std::make_error_code(
                    std::errc::resource_deadlock_would_occur));
            }
            if (!joinable()) {
                throw std::system_error(
                    std::make_error_code(std::errc::invalid_argument));
            }
        }

        native_handle_type handle_ = {};
        thread_id id_;
    };

    class scoped_thread {
    public:
        using native_handle_type = thread::native_handle_type;

        scoped_thread() = default;

        template <class Fn, class... Args>
        scoped_thread(Fn&& f, Args&&... args)
            : thd_(std::forward<Fn>(f), std::forward<Args>(args)...) {
        }

        scoped_thread(scoped_thread&&) noexcept = default;

        scoped_thread& operator=(scoped_thread&& other) noexcept {
            if (joinable()) {
                join();
            }
            thd_ = std::move(other.thd_);
            return *this;
        }

        ~scoped_thread() {
            if (thd_.joinable()) {
                thd_.join();
            }
        }

        native_handle_type& native_handle() noexcept {
            return thd_.native_handle();
        }

        const native_handle_type& native_handle() const noexcept {
            return thd_.native_handle();
        }

        thread_id get_id() const noexcept {
            return thd_.get_id();
        }

        void detach() noexcept {
            thd_.detach();
        }

        bool joinable() const noexcept {
            return thd_.joinable();
        }

        void join() {
            thd_.join();
        }

        static unsigned int hardware_concurrency() noexcept {
            return thread::hardware_concurrency();
        }

    private:
        thread thd_;
    };

    namespace this_thread {
        RAD_EXPORT_DECL void yield() noexcept;

        template <class Rep, class Period>
        void sleep_for(
            const std::chrono::duration<Rep, Period>& sleep_duration) noexcept {
            using namespace std::chrono;
            using detail::thread_sleep_duration_type;
            detail::sleep_thd(
                duration_cast<nanoseconds>(sleep_duration),
                duration_cast<thread_sleep_duration_type>(sleep_duration));
        }

        template <class Clock, class Duration>
        void sleep_until(const std::chrono::time_point<Clock, Duration>&
                             sleep_time) noexcept {
            while (1) {
                auto now = Clock::now();
                if (now >= sleep_time) {
                    break;
                }
                sleep_for(sleep_time - now);
            }
        }
    } // namespace this_thread

}; // namespace RAD_LIB_NAMESPACE
