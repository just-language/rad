#pragma once
#include <rad/detail/radconfig.h>

#include <cstdint>
#include <cstring>
#include <iterator>
#include <system_error>
#include <type_traits>

#if defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_ARCH) ||        \
    defined(_M_ARM)
#define RAD_CPU_ARM_ARCH
#if defined(__aarch64__) || defined(_M_ARM64)
#define RAD_CPU_ARM64_ARCH
#endif // __aarch64__ || _M_ARM64
#else
#define RAD_CPU_X86_ARCH
#endif // __aarch64__ || _M_ARM64 || __ARM_ARCH || _M_ARM

#define RAD_OVERLOAD_ENUM_OPERATORS(x)                                         \
    class OverloadedEnumOperators_##x {                                        \
        using enumType = std::underlying_type_t<x>;                            \
        x enumVal;                                                             \
        constexpr enumType get() const noexcept {                              \
            return static_cast<enumType>(enumVal);                             \
        }                                                                      \
        constexpr OverloadedEnumOperators_##x(enumType eVal)                   \
            : enumVal(static_cast<x>(eVal)) {                                  \
        }                                                                      \
                                                                               \
    public:                                                                    \
        constexpr OverloadedEnumOperators_##x() : enumVal() {                  \
        }                                                                      \
        constexpr OverloadedEnumOperators_##x(x eVal) : enumVal(eVal) {        \
        }                                                                      \
        constexpr OverloadedEnumOperators_##x&                                 \
        operator=(const OverloadedEnumOperators_##x&) = default;               \
        constexpr OverloadedEnumOperators_##x& operator=(x eVal) {             \
            enumVal = eVal;                                                    \
            return *this;                                                      \
        }                                                                      \
        constexpr operator x() const noexcept {                                \
            return enumVal;                                                    \
        }                                                                      \
        constexpr operator bool() const noexcept {                             \
            return static_cast<bool>(get());                                   \
        }                                                                      \
        constexpr bool operator!() const noexcept {                            \
            return !static_cast<bool>(get());                                  \
        }                                                                      \
    };                                                                         \
    inline constexpr OverloadedEnumOperators_##x operator&(const x lhs,        \
                                                           const x rhs) {      \
        return x((x)((std::underlying_type_t<x>)lhs &                          \
                     (std::underlying_type_t<x>)rhs));                         \
    }                                                                          \
    inline constexpr x operator|(const x lhs, const x rhs) {                   \
        return (x)((std::underlying_type_t<x>)lhs |                            \
                   (std::underlying_type_t<x>)rhs);                            \
    }                                                                          \
    inline constexpr x operator^(const x lhs, const x rhs) {                   \
        return (x)((std::underlying_type_t<x>)lhs ^                            \
                   (std::underlying_type_t<x>)rhs);                            \
    }                                                                          \
    inline constexpr x operator~(const x lhs) {                                \
        return (x)(~(std::underlying_type_t<x>)lhs);                           \
    }                                                                          \
    inline constexpr x& operator|=(x& lhs, const x rhs) {                      \
        lhs = (x)((std::underlying_type_t<x>)lhs |                             \
                  (std::underlying_type_t<x>)rhs);                             \
        return lhs;                                                            \
    }                                                                          \
    inline constexpr x& operator&=(x& lhs, const x rhs) {                      \
        lhs = (x)((std::underlying_type_t<x>)lhs &                             \
                  (std::underlying_type_t<x>)rhs);                             \
        return lhs;                                                            \
    }                                                                          \
    inline constexpr x& operator^=(x& lhs, const x rhs) {                      \
        lhs = (x)((std::underlying_type_t<x>)lhs ^                             \
                  (std::underlying_type_t<x>)rhs);                             \
        return lhs;                                                            \
    }                                                                          \
    inline constexpr bool operator==(const x lhs, const x rhs) {               \
        return (std::underlying_type_t<x>)lhs ==                               \
               (std::underlying_type_t<x>)rhs;                                 \
    }                                                                          \
    inline constexpr bool operator!=(const x lhs, const x rhs) {               \
        return (std::underlying_type_t<x>)lhs !=                               \
               (std::underlying_type_t<x>)rhs;                                 \
    }                                                                          \
    inline constexpr bool operator>(const x lhs, const x rhs) {                \
        return (std::underlying_type_t<x>)lhs >                                \
               (std::underlying_type_t<x>)rhs;                                 \
    }                                                                          \
    inline constexpr bool operator<(const x lhs, const x rhs) {                \
        return (std::underlying_type_t<x>)lhs <                                \
               (std::underlying_type_t<x>)rhs;                                 \
    }                                                                          \
    inline constexpr bool operator>=(const x lhs, const x rhs) {               \
        return (std::underlying_type_t<x>)lhs >=                               \
               (std::underlying_type_t<x>)rhs;                                 \
    }                                                                          \
    inline constexpr bool operator<=(const x lhs, const x rhs) {               \
        return (std::underlying_type_t<x>)lhs <=                               \
               (std::underlying_type_t<x>)rhs;                                 \
    }                                                                          \
    inline constexpr bool operator!(const x lhs) {                             \
        return !static_cast<std::underlying_type_t<x>>(lhs);                   \
    }

#ifdef _WIN32
#define RAD_EXPORT_COMPILER_DEF __declspec(dllexport)
#define RAD_IMPORT_COMPILER_DEF __declspec(dllimport)
#define RAD_EXPORT_VTABLE_DEF
#elif defined(__GNUC__)
#define RAD_EXPORT_COMPILER_DEF __attribute__((visibility("default")))
#define RAD_IMPORT_COMPILER_DEF
#define RAD_EXPORT_VTABLE_DEF RAD_EXPORT_COMPILER_DEF
#endif // _WIN32

#if defined(RAD_LIB_IS_BEING_BUILT) && defined(RAD_IS_BUILT_AS_SHARED_LIB)
#define RAD_EXPORT_DECL RAD_EXPORT_COMPILER_DEF
#define RAD_EXPORT_VTABLE RAD_EXPORT_VTABLE_DEF
#elif defined(RAD_IS_BUILT_AS_SHARED_LIB)
#define RAD_EXPORT_DECL RAD_IMPORT_COMPILER_DEF
#define RAD_EXPORT_VTABLE
#else
#define RAD_EXPORT_DECL
#define RAD_EXPORT_VTABLE
#endif // RAD_LIB_IS_BEING_BUILT

namespace RAD_LIB_NAMESPACE {
    // short name for static_cast<T>(U)
    template <class T, class U>
    constexpr T
    as(U&& u) noexcept(noexcept(static_cast<T>(std::forward<U>(u)))) {
        return static_cast<T>(std::forward<U>(u));
    }

    // convert an enum to its underlying integral type
    template <class T>
    constexpr std::underlying_type_t<T> ienum(T e) noexcept {
        return static_cast<std::underlying_type_t<T>>((e));
    }

    template <class, template <class, class...> class>
    struct is_instance_of : public std::false_type {};

    template <class... Ts, template <class, class...> class U>
    struct is_instance_of<U<Ts...>, U> : public std::true_type {};

    template <class Base, class Derived>
    inline constexpr bool is_public_base_of =
        !std::is_same_v<Base, Derived> &&
        std::is_convertible_v<Derived*, Base*>;

    namespace detail {
        // comp returns true if first is less than second to find max
        template <class Iter, class Sential, class Comp>
        constexpr Iter max_or_less(Iter first, Sential last, Comp comp) {
            Iter found = first;
            if (first != last) {
                while (++first != last) {
                    if (comp(*found, *first)) {
                        found = first;
                    }
                }
            }
            return found;
        }
    } // namespace detail

    template <class T>
    constexpr T min(const T right,
                    const std::type_identity_t<T> left) noexcept {
        return right < left ? right : left;
    }

    template <class T>
    constexpr T max(const T right,
                    const std::type_identity_t<T> left) noexcept {
        return right > left ? right : left;
    }

    template <class RetType, class... T>
    constexpr RetType max_of(T... args) {
        constexpr RetType ints[] = {args...};
        return *detail::max_or_less(
            std::begin(ints), std::end(ints),
            [](auto first, auto second) { return first < second; });
    }

    template <class T, std::size_t N>
    constexpr T max_of(const T (&arr)[N]) {
        return *detail::max_or_less(
            std::begin(arr), std::end(arr),
            [](auto first, auto second) { return first < second; });
    }

    template <class RetType, class... T>
    constexpr RetType min_of(T... args) {
        constexpr RetType ints[] = {args...};
        return *detail::max_or_less(
            std::begin(ints), std::end(ints),
            [](auto first, auto second) { return first > second; });
    }

    template <class T, std::size_t N>
    constexpr T min_of(const T (&arr)[N]) {
        return *detail::max_or_less(
            std::begin(arr), std::end(arr),
            [](auto first, auto second) { return first > second; });
    }

    inline void check_and_throw(bool checked, const std::error_code& ec,
                                const char* func_name) {
        if (checked || ec) {
            throw std::system_error(ec, func_name);
        }
    }

    inline void check_and_throw(const std::error_code& ec,
                                const char* func_name) {
        check_and_throw(false, ec, func_name);
    }

    struct noncopyable {
        noncopyable() = default;

        noncopyable(const noncopyable&) = delete;

        noncopyable& operator=(const noncopyable&) = delete;

        noncopyable(noncopyable&&) noexcept {
        }

        noncopyable& operator=(noncopyable&&) noexcept {
            return *this;
        }
    };

    struct nonmovable {
        nonmovable() = default;

        nonmovable(nonmovable&&) = delete;

        nonmovable& operator=(nonmovable&&) = delete;
    };

    struct pinned : noncopyable, nonmovable {};

    // used to always fail static_assert
    template <class T>
    inline constexpr bool always_false = false;

    // used with std::conditional to avoid unnecessary inheritance and to
    // accept any argument
    struct empty_base {
        template <class... Args>
        constexpr empty_base(Args&&... args) noexcept {
        }
    };

    struct any_invcable_t {
        template <class... Args>
        void operator()(Args&&...) const noexcept {
        }
    };

    inline constexpr any_invcable_t any_invcable{};

    template <class Fn>
    class scope_exit {
    public:
        scope_exit(const scope_exit&) = delete;

        scope_exit& operator=(const scope_exit&) = delete;

        scope_exit(Fn&& fn) : fn{std::forward<Fn>(fn)} {
        }

        void release() noexcept {
            released = true;
        }

        ~scope_exit() {
            if (!released) {
                fn();
            }
        }

    private:
        Fn fn;
        bool released = false;
    };
} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail {
    template <class IntType>
    struct int_range_step_1_t {
        constexpr void step_up(IntType& i) const noexcept {
            ++i;
        }

        constexpr void step_down(IntType& i) const noexcept {
            --i;
        }
    };

    template <class IntType>
    struct int_range_dynamic_step_t {
        constexpr int_range_dynamic_step_t(IntType i) noexcept : step_value{i} {
        }

        constexpr void step_up(IntType& i) const noexcept {
            i += step_value;
        }

        constexpr void step_down(IntType& i) const noexcept {
            i -= step_value;
        }

        IntType step_value;
    };

    template <class IntType, class StepType, bool IsEnd, bool Including>
    class int_range_iterator_t : StepType {
        using step = StepType;

    public:
        using value_type = IntType;
        using difference_type = IntType;
        using pointer = IntType;

        constexpr int_range_iterator_t(IntType init, step step_value) noexcept
            : step(step_value), counter(init) {
        }

        constexpr value_type operator*() const noexcept {
            return counter;
        }

        constexpr int_range_iterator_t& operator++() {
            if constexpr (!IsEnd) {
                step::step_up(counter);
            }
            else {
                step::step_down(counter);
            }
            return *this;
        }

        constexpr int_range_iterator_t operator++(int) {
            int_range_iterator_t it{*this};
            operator++();
            return it;
        }

        constexpr int_range_iterator_t& operator--() {
            if constexpr (IsEnd) {
                step::step_up(counter);
            }
            else {
                step::step_down(counter);
            }
            return *this;
        }

        constexpr int_range_iterator_t operator--(int) {
            int_range_iterator_t it{*this};
            operator--();
            return it;
        }

        template <bool IsBigger = IsEnd, std::enable_if_t<IsBigger, int> = 0>
        constexpr bool
        operator!=(const int_range_iterator_t<IntType, StepType, false,
                                              Including>& other) noexcept {
            return Including ? counter >= *other : counter > *other;
        }

        template <bool IsLess = !IsEnd, std::enable_if_t<IsLess, int> = 0>
        constexpr bool
        operator!=(const int_range_iterator_t<IntType, StepType, true,
                                              Including>& other) noexcept {
            return Including ? counter <= *other : counter < *other;
        }

    private:
        IntType counter;
    };

    template <bool Including, class IntType, class StepType>
    class int_range_t : StepType {
    public:
        constexpr int_range_t(IntType stop_value, StepType step_value) noexcept
            : StepType(step_value), stop_value(stop_value) {
        }

        constexpr int_range_t(IntType init, IntType stop_value,
                              StepType step_value) noexcept
            : StepType(step_value), start_value(init), stop_value(stop_value) {
        }

        constexpr IntType size() const noexcept {
            return stop_value - start_value + (Including ? 1 : 0);
        }

        constexpr auto begin() const noexcept {
            return int_range_iterator_t<IntType, StepType, false, Including>{
                start_value, *this};
        }

        constexpr auto end() const noexcept {
            return int_range_iterator_t<IntType, StepType, true, Including>{
                stop_value, *this};
        }

        constexpr auto rbegin() const noexcept {
            return std::make_reverse_iterator(end());
        }

        constexpr auto rend() const noexcept {
            return std::make_reverse_iterator(begin());
        }

        template <std::integral I>
        constexpr bool contains(I i) const noexcept {
            return i >= start_value &&
                   (Including ? i <= stop_value : i < stop_value);
        }

        template <std::integral I>
        constexpr IntType operator[](I i) const noexcept {
            return start_value + i;
        }

    private:
        IntType start_value = 0;
        IntType stop_value;
    };

    template <class I1, class I2>
    using select_unsigned_type =
        std::conditional_t<std::is_unsigned_v<I1>, I1, I2>;

    template <class I1, class I2>
    using wider_int_type = std::conditional_t<
        std::is_same_v<I1, I2>, I1,
        std::conditional_t<
            sizeof(I1) == sizeof(I2), select_unsigned_type<I1, I2>,
            std::conditional_t<(sizeof(I1) > sizeof(I2)), I1, I2>>>;
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    template <std::integral I>
    constexpr auto range(I stop) noexcept {
        return detail::int_range_t<false, I, detail::int_range_step_1_t<I>>{
            stop, detail::int_range_step_1_t<I>{}};
    }

    template <std::integral I1, std::integral I2>
    constexpr auto range(I1 start, I2 stop) noexcept {
        using I = detail::wider_int_type<I1, I2>;
        return detail::int_range_t<false, I, detail::int_range_step_1_t<I>>{
            static_cast<I>(start), static_cast<I>(stop),
            detail::int_range_step_1_t<I>{}};
    }

    template <std::integral I1, std::integral I2, std::integral I3>
    constexpr auto range(I1 start, I2 stop, I3 step) noexcept {
        using I4 = detail::wider_int_type<I1, I2>;
        using I = detail::wider_int_type<I3, I4>;
        return detail::int_range_t<false, I,
                                   detail::int_range_dynamic_step_t<I>>{
            static_cast<I>(start), static_cast<I>(stop),
            detail::int_range_dynamic_step_t<I>{static_cast<I>(step)}};
    }

    template <std::integral I>
    constexpr auto range_including(I stop) noexcept {
        return detail::int_range_t<true, I, detail::int_range_step_1_t<I>>{
            stop, detail::int_range_step_1_t<I>{}};
    }

    template <std::integral I1, std::integral I2>
    constexpr auto range_including(I1 start, I2 stop) noexcept {
        using I = detail::wider_int_type<I1, I2>;
        return detail::int_range_t<true, I, detail::int_range_step_1_t<I>>{
            static_cast<I>(start), static_cast<I>(stop),
            detail::int_range_step_1_t<I>{}};
    }

    template <std::integral I1, std::integral I2, std::integral I3>
    constexpr auto range_including(I1 start, I2 stop, I3 step) noexcept {
        using I4 = detail::wider_int_type<I1, I2>;
        using I = detail::wider_int_type<I3, I4>;
        return detail::int_range_t<true, I,
                                   detail::int_range_dynamic_step_t<I>>{
            static_cast<I>(start), static_cast<I>(stop),
            detail::int_range_dynamic_step_t<I>{static_cast<I>(step)}};
    }
} // namespace RAD_LIB_NAMESPACE