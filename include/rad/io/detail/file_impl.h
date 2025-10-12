#pragma once
#include <rad/io/detail/file_common.h>
#ifdef _WIN32
#include <rad/io/detail/windows/file_impl.h>
#else
#include <rad/io/detail/posix/file_impl.h>
#endif