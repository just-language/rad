#pragma once
#ifdef _WIN32
#include <rad/net/detail/windows/async_socket_impl.h>
#else
#include <rad/net/detail/posix/async_socket_impl.h>
#endif // _WIN32
