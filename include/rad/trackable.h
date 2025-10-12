#pragma once
#include <rad/libbase.h>

#ifndef NDEBUG
#ifndef RAD_ENABLE_TRACKABLE
#define RAD_ENABLE_TRACKABLE
#endif // !RAD_ENABLE_TRACKABLE
#endif // !NDEBUG

#ifdef RAD_ENABLE_TRACKABLE

#include <rad/stack_list.h>
#include <rad/threading/synchronized_value.h>

namespace RAD_LIB_NAMESPACE::experimental::tracking::v1 {
    class trackable;

    template <class T>
    class pointer;

    class pointer_base : public stack_double_list_node {
        template <typename>
        friend class pointer;
        friend class trackable;

        bool is_destroyed = true;
    };

    class trackable {
        template <typename>
        friend class pointer;

    public:
        trackable() = default;

        trackable(const trackable&) noexcept {
        }

        trackable(trackable&&) noexcept : trackable() {
        }

        trackable& operator=(const trackable&) noexcept {
            return *this;
        }

        trackable& operator=(trackable&&) noexcept {
            return *this;
        }

        ~trackable() {
            // mark all registered pointers as invalid, then
            // unlink them
            auto ptrs = ptrs_.synchronize();

            for (auto& ptr : *ptrs) {
                ptr.is_destroyed = true;
            }

            ptrs->clear();
        }

    private:
        // used by pointers
        void register_pointer(pointer_base& ptr) noexcept {
            ptrs_->push_back(ptr);
        }

        // used by pointers
        void unregister_pointer(pointer_base& ptr) noexcept {
            ptrs_->erase(ptr);
        }

        sync_value<stack_list<pointer_base>> ptrs_;
    };

    template <class T>
    class pointer : public pointer_base {
        template <typename>
        friend class pointer;

    public:
        pointer() = default;

        pointer(std::nullptr_t) noexcept : ptr_{nullptr} {
        }

        pointer(T* p) noexcept : ptr_{p} {
            assert(p != nullptr);
            is_destroyed = false;
            register_self();
        }

        template <class U>
        pointer(U* p) noexcept
            requires(!std::same_as<U, T> && std::is_convertible_v<U*, T*>)
            : ptr_{p} {
            assert(p != nullptr);
            is_destroyed = false;
            register_self();
        }

    private:
        pointer(T* p, bool src_is_destroyed) noexcept : ptr_{p} {
            is_destroyed = src_is_destroyed;
            register_self();
        }

    public:
        pointer(const pointer& other) noexcept
            : pointer_base(other), ptr_{other.ptr_} {
            register_self();
        }

        template <class U>
        pointer(const pointer<U>& other) noexcept
            requires(!std::is_same_v<T, U> && std::is_convertible_v<U*, T*>)
            : pointer_base(other), ptr_{other.ptr_} {
            register_self();
        }

        pointer& operator=(std::nullptr_t) noexcept {
            destruct();
            ptr_ = nullptr;
            is_destroyed = true;
            return *this;
        }

        pointer& operator=(const pointer& other) noexcept {
            destruct();
            ptr_ = other.ptr_;
            is_destroyed = other.is_destroyed;
            register_self();
            return *this;
        }

        template <class U>
        pointer& operator=(const pointer<U>& other) noexcept
            requires(!std::is_same_v<T, U> && std::is_convertible_v<U*, T*>)
        {
            destruct();
            ptr_ = other.ptr_;
            is_destroyed = other.is_destroyed;
            register_self();
            return *this;
        }

        pointer& operator=(T* p) noexcept {
            destruct();
            ptr_ = p;
            is_destroyed = false;
            register_self();
            return *this;
        }

        template <class U>
        pointer& operator=(U* p) noexcept
            requires(!std::same_as<T, U> && std::is_convertible_v<U*, T*>)
        {
            destruct();
            ptr_ = p;
            is_destroyed = false;
            register_self();
            return *this;
        }

        ~pointer() {
            destruct();
        }

        operator pointer<const T>() const noexcept
            requires(!std::is_const_v<T>)
        {
            return pointer<const T>{ptr_, is_destroyed};
        }

        T& operator*() const noexcept {
            assert_is_valid();
            return *ptr_;
        }

        T* operator->() const noexcept {
            assert_is_valid();
            return ptr_;
        }

        constexpr bool operator==(std::nullptr_t) const noexcept {
            return ptr_ == nullptr;
        }

        constexpr bool operator==(const T* other) const noexcept {
            return ptr_ == other;
        }

        constexpr bool operator==(const pointer& other) const noexcept {
            return ptr_ == other.ptr_;
        }

        constexpr operator bool() const noexcept {
            return ptr_ != nullptr;
        }

    private:
        void register_self() noexcept {
            if constexpr (is_public_base_of<trackable, T>) {
                if (ptr_ && !is_destroyed) {
                    static_cast<trackable*>(ptr_)->register_pointer(*this);
                }
            }
        }

        void destruct() noexcept {
            if constexpr (is_public_base_of<trackable, T>) {
                if (ptr_ && !is_destroyed) {
                    static_cast<trackable*>(ptr_)->unregister_pointer(*this);
                }
            }
        }

        void assert_is_valid() const noexcept {
            assert(ptr_ != nullptr && "derefrencing a null pointer !");
            assert(!is_destroyed && "pointer is used after the pointed to was "
                                    "destructed !");

            if (!ptr_) {
                printf("!!! derefrencing a null pointer "
                       "!\n");
                std::terminate();
            }

            if (is_destroyed) {
                printf("!!! pointer is used after the "
                       "pointed to "
                       "was "
                       "destructed !\n");
                std::terminate();
            }
        }

        T* ptr_;
    };

    template <class T>
    class ref {
    public:
        ref(T& r) noexcept : ptr_{std::addressof(r)} {
        }

        T& operator*() const noexcept {
            return *ptr_;
        }

        T* operator->() const noexcept {
            return ptr_.operator->();
        }

        T& get() const noexcept {
            return *ptr_;
        }

        operator T&() const noexcept {
            return get();
        }

        T* operator&() const noexcept {
            return operator->();
        }

        template <class... Args>
        constexpr std::invoke_result_t<T&, Args...>
        operator()(Args&&... args) const
            requires(std::is_invocable_v<T&, Args...>)
        {
            return get()(std::forward<Args>(args)...);
        }

    private:
        pointer<T> ptr_;
    };

    template <class T>
    class cref : public ref<const T> {
        using base = ref<const T>;

    public:
        cref(const T& r) noexcept : base(r) {
        }
    };
} // namespace RAD_LIB_NAMESPACE::experimental::tracking::v1

#include <rad/threading/mutex.h>

#include <cstdio>
#include <memory>
#if __cpp_lib_source_location == 201907L
#include <source_location>
#endif

namespace RAD_LIB_NAMESPACE::experimental::tracking::v2 {
    class trackable;

    template <class T>
    class pointer_with_lock;

    struct tracking_state {
        friend class trackable;
        template <typename>
        friend class pointer_with_lock;

        mutable mutex mtx;
#if __cpp_lib_source_location == 201907L
        std::string destruction_place;
        std::string usage_place;
#endif // __cpp_lib_source_location
        bool destroyed = false;
        size_t usage_count = 0;

        bool is_destroyed() const noexcept {
            auto lock = std::lock_guard<mutex>{mtx};
            return destroyed;
        }

        bool in_use() const noexcept {
            auto lock = std::lock_guard<mutex>{mtx};
            return usage_count > 0;
        }

        void assert_not_destroyed() {
            if (is_destroyed()) {
                auto lock = std::lock_guard<mutex>{mtx};
#if __cpp_lib_source_location == 201907L
                printf("[!] used after destoyed at %s\n",
                       destruction_place.c_str());
#else
                printf("[!] used after destoyed !\n");
#endif // __cpp_lib_source_location
                assert(false);
            }
        }

        template <class... Args>
        static void format_string(std::string& out, const char* fmt,
                                  const Args&... args) {
            int len = std::snprintf(nullptr, 0, fmt, args...);
            assert(len > 0);
            out.clear();
            out.resize(static_cast<size_t>(len) + 1);
            std::snprintf(out.data(), out.size(), fmt, args...);
            out.pop_back();
        }

    private:
        void destroy() noexcept {
            auto lock = std::lock_guard<mutex>{mtx};
            destroyed = true;
        }

        void register_usage() {
            auto lock = std::lock_guard<mutex>{mtx};
            ++usage_count;
        }

        void unregister_usage() {
            auto lock = std::lock_guard<mutex>{mtx};
            --usage_count;
        }
    };

    template <class T>
    class pointer_with_lock {
    public:
        pointer_with_lock(T* p,
                          std::shared_ptr<tracking_state> state_ptr) noexcept
            : p{p}, state_ptr{std::move(state_ptr)} {
            if (this->state_ptr) {
                this->state_ptr->register_usage();
#if __cpp_lib_source_location == 201907L
                const std::source_location& loc =
                    std::source_location::current();
                tracking_state::format_string(
                    this->state_ptr->usage_place, "%s : %s (%d, %d)",
                    loc.file_name(), loc.function_name(), loc.line(),
                    loc.column());
#endif
            }
        }

        ~pointer_with_lock() {
            if (state_ptr) {
                state_ptr->unregister_usage();
            }
        }

        T* operator->() const noexcept {
            return p;
        }

    private:
        T* p;
        std::shared_ptr<tracking_state> state_ptr;
    };

    class trackable {
        template <typename>
        friend class pointer;
        template <typename>
        friend class reference;

    public:
        trackable() noexcept : state_ptr{std::make_shared<tracking_state>()} {
        }

        ~trackable() {
            auto lock = std::lock_guard<mutex>{state_ptr->mtx};

            // if is in use. Not reliable yet due to some
            // bugs arised in asymmetric execution transfer:
            /*
            struct delete_self : trackable
            {
                    void do_delete()
                    {
                    delete this;
                    }
            };

            void fn()
            {
                    pointer<delete_self> ptr = new
            delete_self(); ptr->do_delete(); // the
            trackable is destroyed while it is being used
            although this code should work
            }
            */
            if (state_ptr->usage_count) {
#if __cpp_lib_source_location == 201907L
                printf("[!] destoyed while being used at "
                       "%s!\n",
                       state_ptr->usage_place.c_str());
#else
                printf("[!] destoyed while being used "
                       "!\n");
#endif
                assert(false);
            }

#if __cpp_lib_source_location == 201907L
            const std::source_location& loc = std::source_location::current();
            tracking_state::format_string(
                state_ptr->destruction_place, "%s : %s (%d, %d)",
                loc.file_name(), loc.function_name(), loc.line(), loc.column());
#endif // __cpp_lib_source_location

            state_ptr->destroyed = true;
        }

        trackable(const trackable&) noexcept : trackable() {
        }

        trackable(trackable&&) noexcept : trackable() {
        }

        constexpr trackable& operator=(const trackable&) noexcept {
            return *this;
        }

        constexpr trackable& operator=(trackable&&) noexcept {
            return *this;
        }

    private:
        std::shared_ptr<tracking_state> clone_state() const noexcept {
            return state_ptr;
        }

        std::shared_ptr<tracking_state> state_ptr;
    };

    template <class T>
    class pointer {
    public:
        pointer() = default;

        pointer(std::nullptr_t) noexcept {
        }

        pointer(T* p) noexcept : p{p} {
            if constexpr (std::is_convertible_v<T*, trackable*>) {
                state_ptr = p->trackable::clone_state();
            }
        }

        operator pointer<const T>() const noexcept {
            return pointer<const T>{p};
        }

        pointer_with_lock<T> operator->() const noexcept {
            assert(p != nullptr && "derefrencing a null pointer");
            if (state_ptr) {
                state_ptr->assert_not_destroyed();
            }
            return {p, state_ptr};
        }

        T& operator*() const noexcept {
            assert(p != nullptr && "derefrencing a null pointer");
            if (state_ptr) {
                state_ptr->assert_not_destroyed();
            }
            return *p;
        }

        constexpr bool operator==(std::nullptr_t) const noexcept {
            return p == nullptr;
        }

        constexpr bool operator==(const pointer& other) const noexcept {
            return p == other.p;
        }

        constexpr operator bool() const noexcept {
            return p != nullptr;
        }

    private:
        T* p = nullptr;
        std::shared_ptr<tracking_state> state_ptr;
    };

    template <class T>
    class ref {
    public:
        ref(T& r) noexcept : r{r} {
            if constexpr (std::is_convertible_v<T*, trackable*>) {
                state_ptr = r.trackable::clone_state();
            }
        }

        T* operator->() const noexcept {
            if (state_ptr) {
                state_ptr->assert_not_destroyed();
            }
            return std::addressof(r);
        }

        constexpr T* operator&() const noexcept {
            if (state_ptr) {
                state_ptr->assert_not_destroyed();
            }
            return std::addressof(r);
        }

        constexpr T& operator*() const noexcept {
            return get();
        }

        T& get() const noexcept {
            if (state_ptr) {
                state_ptr->assert_not_destroyed();
            }
            return r;
        }

        operator T&() const noexcept {
            return get();
        }

        template <class... Args>
        constexpr std::invoke_result_t<T&, Args...>
        operator()(Args&&... args) const {
            return get()(std::forward<Args>(args)...);
        }

    private:
        T& r;
        std::shared_ptr<tracking_state> state_ptr;
    };

    template <class T>
    class cref : public ref<const T> {
        using base = ref<const T>;

    public:
        cref(const T& r) noexcept : base(r) {
        }
    };

} // namespace RAD_LIB_NAMESPACE::experimental::tracking::v2

#endif // RAD_ENABLE_TRACKABLE

namespace RAD_LIB_NAMESPACE {
#ifdef RAD_ENABLE_TRACKABLE

    using trackable = experimental::tracking::v1::trackable;

    template <class T>
    using pointer = experimental::tracking::v1::pointer<T>;

    template <class T>
    using ref = experimental::tracking::v1::ref<T>;

    template <class T>
    using cref = experimental::tracking::v1::cref<T>;

#else

    class trackable {};

    template <class T>
    class pointer {
    public:
        constexpr pointer() noexcept = default;

        constexpr pointer(std::nullptr_t) noexcept : p{nullptr} {
        }

        constexpr pointer(T* p) noexcept : p{p} {
        }

        constexpr T* operator->() const noexcept {
            return p;
        }

        constexpr T& operator*() const noexcept {
            return *p;
        }

        constexpr bool operator==(std::nullptr_t) const noexcept {
            return p == nullptr;
        }

        constexpr bool operator==(const pointer& other) const noexcept {
            return p == other.p;
        }

        constexpr operator bool() const noexcept {
            return p != nullptr;
        }

        operator pointer<const T>() const noexcept {
            return pointer<const T>{p};
        }

    private:
        T* p;
    };

    template <class T>
    class ref {
    public:
        constexpr ref(T& r) noexcept : ptr_{std::addressof(r)} {
        }

        constexpr T* operator->() const noexcept {
            return ptr_;
        }

        constexpr T* operator&() const noexcept {
            return ptr_;
        }

        constexpr T& operator*() const noexcept {
            return *ptr_;
        }

        constexpr T& get() const noexcept {
            return *ptr_;
        }

        constexpr operator T&() const noexcept {
            return get();
        }

        template <class... Args>
        constexpr std::invoke_result_t<T&, Args...>
        operator()(Args&&... args) const {
            return get()(std::forward<Args>(args)...);
        }

    private:
        T* ptr_;
    };

    template <class T>
    class cref : public ref<const T> {
        using base = ref<const T>;

    public:
        constexpr cref(const T& r) noexcept : base(r) {
        }
    };

#endif // RAD_ENABLE_TRACKABLE
} // namespace RAD_LIB_NAMESPACE