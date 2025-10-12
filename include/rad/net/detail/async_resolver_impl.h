#pragma once
#ifdef _WIN32
#include <rad/net/detail/windows/async_resolver_impl.h>
#else
#include <rad/net/detail/posix/async_resolver_impl.h>
#endif // _WIN32
