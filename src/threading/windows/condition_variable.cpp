#include <Windows.h>
#include <rad/threading/condition_variable.h>

using namespace RAD_LIB_NAMESPACE;

static_assert(sizeof(detail::recursive_mutex_storage) ==
              sizeof(CRITICAL_SECTION));
static_assert(sizeof(detail::exclusive_mutex_storage) == sizeof(SRWLOCK));
static_assert(sizeof(detail::shared_mutex_storage) == sizeof(SRWLOCK));
static_assert(sizeof(detail::condvar_storage_type) ==
              sizeof(CONDITION_VARIABLE));

void detail::notify_one_condvar(native_condvar_type cvar) noexcept {
    ::WakeConditionVariable(cvar);
}

void detail::notify_all_condvar(native_condvar_type cvar) noexcept {
    ::WakeAllConditionVariable(cvar);
}

void detail::wait_condvar(native_condvar_type cvar, mutex* mtx) noexcept {
    ::SleepConditionVariableSRW(cvar, mtx->native_handle(), INFINITE, 0);
}

bool detail::wait_condvar_timeout(native_condvar_type cvar, mutex* mtx,
                                  condvar_time_unit timeout) noexcept {
    return ::SleepConditionVariableSRW(cvar, mtx->native_handle(),
                                       timeout.count(), 0) != 0;
}