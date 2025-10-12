#pragma once
#include <rad/libbase.h>

#include <cassert>

namespace RAD_LIB_NAMESPACE {
    template <class>
    class function_view;

    template <class R, class... Args>
    class function_view<R(Args...)> {
        using free_fn_ptr_type = R (*)(Args...);

    public:
        using fn_ptr_type = R (*)(void*, Args...);
        using result_type = R;

        function_view() = default;

        function_view(std::nullptr_t) noexcept {
        }

        function_view(free_fn_ptr_type fn) noexcept
            : instance{reinterpret_cast<void*>(fn)} {
            fn_ptr = [](void* instance, Args... args) -> result_type {
                return static_cast<result_type>(
                    (reinterpret_cast<free_fn_ptr_type>(instance))(
                        std::forward<Args>(args)...));
            };
        }

        template <class Fn, std::enable_if_t<
                                std::is_invocable_r_v<R, Fn, Args...>, int> = 0>
        function_view(Fn&& fn) noexcept : instance{(void*)std::addressof(fn)} {
            fn_ptr = [](void* instance, Args... args) -> result_type {
                auto& fn = *reinterpret_cast<std::add_pointer_t<Fn>>(instance);
                return static_cast<result_type>(
                    fn(std::forward<Args>(args)...));
            };
        }

        result_type operator()(Args... args) const
            noexcept(noexcept(fn_ptr(instance, std::forward<Args>(args)...))) {
            assert(instance != nullptr && fn_ptr != nullptr);
            return fn_ptr(instance, std::forward<Args>(args)...);
        }

        explicit operator bool() const noexcept {
            return instance != nullptr;
        }

    private:
        void* instance = nullptr;
        fn_ptr_type fn_ptr = nullptr;
    };
} // namespace RAD_LIB_NAMESPACE