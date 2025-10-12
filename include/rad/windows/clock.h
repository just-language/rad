#pragma once
#include <rad/detail/radconfig.h>
#include <chrono>
#include <cstdint>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief A clock that counts since the epoch used by
     * Windows (1/1/1601) and its unit is 100 nanosecond.
     *
     * The time point of this clock is compatible with `FILETIME`.
     */
    struct windows_clock {
        /*!
         * @brief signed arithmetic type representing the number
         * of ticks in the clock's duration.
         * Typicall it is `std::int64_t`
         */
        using rep = std::int64_t;
        /*!
         * @brief a std::ratio type representing the tick period
         * of the clock, in seconds.
         *
         */
        using period = std::ratio_multiply<std::hecto, std::nano>;
        /*!
         * @brief `std::chrono::duration<rep, period>`,
         * capable of representing negative durations.
         */
        using duration = std::chrono::duration<rep, period>;
        /*!
         * @brief Time point type used by this clock.
         * Typically it is `std::chrono::time_point<windows_clock>`.
         */
        using time_point = std::chrono::time_point<windows_clock>;

        /*!
         * @brief Whether if the time between ticks is always constant,
         * i.e. calls to now() return values that increase monotonically
         * even in case of some external clock adjustment.
         *
         * It will have the same value as `std::chrono::system_clock::is_steady`
         * which is usually `false`.
         */
        static constexpr bool is_steady = std::chrono::system_clock::is_steady;

        /*!
         * @brief Convert a windows clock time to a system clock time.
         * @param t The windows clock time.
         * @return The system clock time.
         */
        template <class Duration>
        static constexpr std::chrono::sys_time<
            std::common_type_t<Duration, std::chrono::seconds>>
        to_sys(const std::chrono::time_point<windows_clock, Duration>& t) {
            using namespace std::chrono;
            constexpr auto diff =
                sys_days{January / 1 / 1601}.time_since_epoch(); // negative
                                                                 // duration
            using return_duration =
                std::common_type_t<Duration, std::chrono::seconds>;
            using return_type = std::chrono::sys_time<return_duration>;
            return return_type{
                duration_cast<return_duration>(t.time_since_epoch() + diff)};
        }

        /*!
         * @brief Convert a system clock time to a windows clock time.
         * @param t The system clock time.
         * @return The windows clock time.
         */
        template <class Duration>
        static constexpr std::chrono::time_point<
            windows_clock, std::common_type_t<Duration, std::chrono::seconds>>
        from_sys(const std::chrono::sys_time<Duration>& t) {
            using namespace std::chrono;
            constexpr auto diff =
                sys_days{January / 1 / 1601}.time_since_epoch(); // negative
                                                                 // duration
            using return_duration =
                std::common_type_t<Duration, std::chrono::seconds>;
            using return_type =
                std::chrono::time_point<windows_clock, return_duration>;
            return return_type{
                duration_cast<return_duration>(t.time_since_epoch() - diff)};
        }

        /*!
         * @brief Get a time point representing the current point in time.
         * @return A time point representing the current point in time.
         */
        static time_point now() noexcept {
            return from_sys(std::chrono::system_clock::now());
        }
    };

    static_assert(std::chrono::is_clock_v<windows_clock>);
} // namespace RAD_LIB_NAMESPACE

namespace std::chrono {
    template <>
    struct clock_time_conversion<rad::windows_clock, rad::windows_clock> {
        // convert from windows_clock time_point to windows_clock
        // time_point
        template <class Duration>
        constexpr time_point<rad::windows_clock, Duration>
        operator()(const time_point<rad::windows_clock, Duration>& t) const {
            return t;
        }
    };

    template <>
    struct clock_time_conversion<system_clock, rad::windows_clock> {
        // convert from windows_clock time_point to system_clock
        // time_point
        template <class Duration>
        constexpr sys_time<Duration>
        operator()(const time_point<rad::windows_clock, Duration>& t) const {
            return rad::windows_clock::to_sys(t);
        }
    };

    template <>
    struct clock_time_conversion<rad::windows_clock, system_clock> {
        // convert from system_clock time_point to windows_clock
        // time_point
        template <class Duration>
        constexpr time_point<rad::windows_clock, Duration>
        operator()(const sys_time<Duration>& t) const {
            return rad::windows_clock::from_sys(t);
        }
    };
} // namespace std::chrono