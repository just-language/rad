#include <rad/detail/radconfig.h>

#include <cstdint>
#include <cstring>
#include <tuple>

namespace RAD_LIB_NAMESPACE {
    template <class T>
    struct function_traits : public function_traits<decltype(&T::operator())> {
    };

    // free functions
    template <class Ret, class... Args>
    struct function_traits<Ret (*)(Args...)> {
        static constexpr bool is_const = false;
        static constexpr bool is_noexcept = false;
        static constexpr std::size_t args_n = sizeof...(Args);
        using result_type = Ret;
        using args_tuple = std::tuple<Args...>;
        template <std::size_t I>
        using nth_arg = std::tuple_element_t<I, args_tuple>;
    };

    // free noexcept functions
    template <class Ret, class... Args>
    struct function_traits<Ret (*)(Args...) noexcept> {
        static constexpr bool is_const = false;
        static constexpr bool is_noexcept = true;
        static constexpr std::size_t args_n = sizeof...(Args);
        using result_type = Ret;
        using args_tuple = std::tuple<Args...>;
        template <std::size_t I>
        using nth_arg = std::tuple_element_t<I, args_tuple>;
    };

#define RAD_DEFINE_METHOD_TRAITS(CV, Const, Noexcept, Rvalue, Lvalue)          \
    template <class Class, class Ret, class... Args>                           \
    struct function_traits<Ret (Class::*)(Args...) CV> {                       \
        static constexpr bool is_const = Const;                                \
        static constexpr bool is_noexcept = Noexcept;                          \
        static constexpr std::size_t args_n = sizeof...(Args);                 \
        using result_type = Ret;                                               \
        using args_tuple = std::tuple<Args...>;                                \
        template <std::size_t I>                                               \
        using nth_arg = std::tuple_element_t<I, args_tuple>;                   \
    };

    RAD_DEFINE_METHOD_TRAITS(, false, false, false, false)
    RAD_DEFINE_METHOD_TRAITS(const, true, false, false, false)
    RAD_DEFINE_METHOD_TRAITS(noexcept, false, true, false, false)
    RAD_DEFINE_METHOD_TRAITS(const noexcept, true, true, false, false)

} // namespace RAD_LIB_NAMESPACE