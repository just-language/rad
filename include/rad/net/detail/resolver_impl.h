#pragma once
#ifdef _WIN32
#include <rad/net/detail/windows/resolver_impl.h>
#else
#include <rad/net/detail/posix/resolver_impl.h>
#endif // _WIN32
