#pragma once
#include <rad/string.h>

#include <optional>
#include <string>
#include <system_error>

#if defined(_WIN32) && !defined(__cpp_consteval) && defined(__clang__)
namespace std::experimental::fundamentals_v2 {
    struct source_location {
        _NODISCARD static constexpr source_location
        current(const uint_least32_t _Line_ = __builtin_LINE(),
                const uint_least32_t _Column_ = __builtin_COLUMN(),
                const char* const _File_ = __builtin_FILE(),
                const char* const _Function_ = __builtin_FUNCTION()) noexcept {
            source_location _Result;
            _Result._Line = _Line_;
            _Result._Column = _Column_;
            _Result._File = _File_;
            _Result._Function = _Function_;
            return _Result;
        }

        _NODISCARD_CTOR constexpr source_location() noexcept = default;

        _NODISCARD constexpr uint_least32_t line() const noexcept {
            return _Line;
        }
        _NODISCARD constexpr uint_least32_t column() const noexcept {
            return _Column;
        }
        _NODISCARD constexpr const char* file_name() const noexcept {
            return _File;
        }
        _NODISCARD constexpr const char* function_name() const noexcept {
            return _Function;
        }

    private:
        uint_least32_t _Line{};
        uint_least32_t _Column{};
        const char* _File = "";
        const char* _Function = "";
    };
} // namespace std::experimental::fundamentals_v2
#endif // !defined(__cpp_consteval) && defined(__clang__)

#if __has_include(<source_location>) && defined(__cpp_consteval)
#include <source_location>
#else
#define UNITTEST_HAS_EXPER_FUND_SOURCE_LOCATION
#if __has_include(<experimental/source_location>)
#include <experimental/source_location>
#endif
#endif
#include <rad/coro/awaitable_traits.h>
#include <rad/coro/task.h>

namespace RAD_LIB_NAMESPACE::unittest {
#ifdef UNITTEST_HAS_EXPER_FUND_SOURCE_LOCATION
    using source_location = std::experimental::fundamentals_v2::source_location;
#else
    using source_location = std::source_location;
#endif // UNITTEST_HAS_EXPER_SOURCE_LOCATION

    class exception : public std::runtime_error {
    public:
        exception(const char* msg, const char* file, uint32_t line)
            : std::runtime_error(msg), filename_{file}, line_{line} {
        }

        exception(const char* msg, const char* file, uint32_t line,
                  const char* fn)
            : std::runtime_error(msg), filename_{file}, fn_name_{fn},
              line_{line} {
        }

        exception(const std::string& msg, const char* file, uint32_t line)
            : std::runtime_error(msg), filename_{file}, line_{line} {
        }

        exception(const char* msg, const char* orig_msg, const char* file,
                  uint32_t line)
            : std::runtime_error(std::string{msg} + " : " + orig_msg),
              filename_{file}, line_{line} {
        }

        exception(const char* msg, const char* orig_msg, const char* file,
                  uint32_t line, const char* fn)
            : std::runtime_error(std::string{msg} + " : " + orig_msg),
              filename_{file}, fn_name_{fn}, line_{line} {
        }

        std::string_view filename() const noexcept {
            return filename_;
        }

        uint32_t line() const noexcept {
            return line_;
        }

        std::string detailed() const {
            return what() + std::string{" in file \""} + filename_ +
                   std::string{" ("} + std::to_string(line_) +
                   std::string{")\" in function \""} + fn_name_;
        }

    private:
        std::string_view filename_;
        std::string_view fn_name_;
        uint32_t line_ = 0;
    };

    [[noreturn]] inline void
    throw_test_error(const char* msg,
                     const source_location& loc = source_location::current()) {
        throw exception{msg, loc.file_name(), loc.line(), loc.function_name()};
    }

    [[noreturn]] inline void
    throw_test_error(const char* msg, const char* orig_msg,
                     const source_location& loc = source_location::current()) {
        throw exception{msg, orig_msg, loc.file_name(), loc.line(),
                        loc.function_name()};
    }

    template <class T1, class T2>
    inline void
    assert_eq(T1&& t1, T2&& t2, const char* msg,
              const source_location& loc = source_location::current()) {
        if (std::forward<T1>(t1) != std::forward<T2>(t2)) {
            throw_test_error(msg, loc);
        }
    }

    template <class T1, class T2>
    inline void
    assert_eq(T1&& t1, T2&& t2, const char* name1, const char* name2,
              const source_location& loc = source_location::current()) {
        if (std::forward<T1>(t1) != std::forward<T2>(t2)) {
            throw_test_error(
                (name1 + std::string{" doesn't equal "} + name2).c_str(), loc);
        }
    }

    template <class T1, class T2>
    inline void
    assert_neq(T1&& t1, T2&& t2, const char* msg,
               const source_location& loc = source_location::current()) {
        if (std::forward<T1>(t1) == std::forward<T2>(t2)) {
            throw_test_error(msg, loc);
        }
    }

    template <class T1, class T2>
    inline void
    assert_neq(T1&& t1, T2&& t2, const char* name1, const char* name2,
               const source_location& loc = source_location::current()) {
        if (std::forward<T1>(t1) == std::forward<T2>(t2)) {
            throw_test_error((name1 + std::string{" equals "} + name2).c_str(),
                             loc);
        }
    }

    inline void
    assert_true(bool b, const char* msg,
                const source_location& loc = source_location::current()) {
        if (!b) {
            throw_test_error(msg, loc);
        }
    }

    inline void
    assert_false(bool b, const char* msg,
                 const source_location& loc = source_location::current()) {
        if (b) {
            throw_test_error(msg, loc);
        }
    }

    template <NonAwaitableFunctor Fn>
    std::invoke_result_t<Fn>
    assert_returns(Fn fn, const char* msg,
                   const source_location& loc = source_location::current()) {
        try {
            return fn();
        }
        catch (const std::exception& ex) {
            throw_test_error(msg, ex.what(), loc);
        }
        catch (...) {
            throw_test_error(msg, loc);
        }
        if constexpr (!std::is_same_v<std::invoke_result_t<Fn>, void>) {
            throw; // get rid of gcc Wreturn-type
        }
    }

    template <AwaitableFunctor<int> Fn>
    auto assert_returns(Fn fn, const char* msg,
                        const source_location& loc = source_location::current())
        -> task<awaitable_result<std::invoke_result_t<Fn>>> {
        try {
            co_return co_await fn();
        }
        catch (const std::exception& ex) {
            throw_test_error(msg, ex.what(), loc);
        }
        catch (...) {
            throw_test_error(msg, loc);
        }
    }

    template <NonAwaitableFunctor Fn>
    void
    assert_throws(Fn fn, const char* msg,
                  const source_location& loc = source_location::current()) {
        bool caught = false;
        try {
            (void)fn();
        }
        catch (...) {
            caught = true;
        }
        assert_true(caught, msg, loc);
    }

    template <class Exception, NonAwaitableFunctor Fn>
    void
    assert_throws(Fn fn, const char* msg,
                  const source_location& loc = source_location::current()) {
        bool caught = false;
        try {
            (void)fn();
        }
        catch (const Exception&) {
            caught = true;
        }
        assert_true(caught, msg, loc);
    }

    template <AwaitableFunctor<int> Fn>
    task<>
    assert_throws(Fn fn, const char* msg,
                  const source_location& loc = source_location::current()) {
        bool caught = false;
        try {
            (void)(co_await fn());
        }
        catch (...) {
            caught = true;
        }
        assert_true(caught, msg, loc);
    }

    template <class Exception, AwaitableFunctor<int> Fn>
    task<>
    assert_throws(Fn fn, const char* msg,
                  const source_location& loc = source_location::current()) {
        bool caught = false;
        try {
            (void)(co_await fn());
        }
        catch (const Exception&) {
            caught = true;
        }
        assert_true(caught, msg, loc);
    }
} // namespace RAD_LIB_NAMESPACE::unittest

#define THROW_TEST_ERROR(msg)                                                  \
    throw RAD_LIB_NAMESPACE::unittest::exception {                             \
        msg, __FILE__, __LINE__                                                \
    }

#define THROW_TEST_ERROR_ORIG(msg, orig_msg)                                   \
    throw RAD_LIB_NAMESPACE::unittest::exception {                             \
        msg, orig_msg, __FILE__, __LINE__                                      \
    }

#define REQUIRE(e, msg)                                                        \
    if (!(e))                                                                  \
    THROW_TEST_ERROR(msg)

#define TEST_EQ_MSG(a, b, msg)                                                 \
    if ((a) != (b))                                                            \
    THROW_TEST_ERROR(msg)

#define TEST_EQ(a, b, name1, name2)                                            \
    if ((a) != (b))                                                            \
    THROW_TEST_ERROR(name1 + std::string{" doesn't equal "} + name2)

#define TEST_TRUE(b, msg)                                                      \
    if (!b)                                                                    \
    THROW_TEST_ERROR(msg)

#define TEST_FALSE(b, msg)                                                     \
    if (b)                                                                     \
    THROW_TEST_ERROR(msg)

#define TRY_RETURN(e, msg)                                                     \
    [&] {                                                                      \
        try {                                                                  \
            return (e);                                                        \
        }                                                                      \
        catch (const std::exception& ex) {                                     \
            THROW_TEST_ERROR_ORIG(msg, ex.what());                             \
        }                                                                      \
        catch (...) {                                                          \
            THROW_TEST_ERROR(msg);                                             \
        }                                                                      \
    }()

#define TRY_AWAIT_RETURN(e, ret, msg)                                          \
    co_await [&]() -> rad::task<ret> {                                         \
        try {                                                                  \
            co_return co_await (e);                                            \
        }                                                                      \
        catch (const std::exception& ex) {                                     \
            THROW_TEST_ERROR_ORIG(msg, ex.what());                             \
        }                                                                      \
        catch (...) {                                                          \
            THROW_TEST_ERROR(msg);                                             \
        }                                                                      \
    }()

#define TRY_AWAIT(e, msg) TRY_AWAIT_RETURN(e, void, msg)

#define TEST_THROW(e, msg)                                                     \
    {                                                                          \
        bool caught = false;                                                   \
        try {                                                                  \
            (void)(e);                                                         \
        }                                                                      \
        catch (...) {                                                          \
            caught = true;                                                     \
        }                                                                      \
        TEST_TRUE(caught, msg);                                                \
    }

#define TEST_THROW_EX(e, ex, msg)                                              \
    {                                                                          \
        bool caught = false;                                                   \
        try {                                                                  \
            (void)(e);                                                         \
        }                                                                      \
        catch (const ex&) {                                                    \
            caught = true;                                                     \
        }                                                                      \
        catch (...) {                                                          \
        }                                                                      \
        TEST_TRUE(caught, msg);                                                \
    }
