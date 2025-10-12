#pragma once
#ifdef _WIN32
#include <rad/threading/detail/windows/mutex.h>
#else
#include <rad/threading/detail/posix/mutex.h>
#endif