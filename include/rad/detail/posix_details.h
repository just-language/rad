#pragma once

#if !defined(__unix__)
#error "this file is to be compiled only on posix os"
#endif

#include <cstdint>

extern "C" {

using socket_fd_t = int;

using socklen_t = unsigned int;

extern int close(int __fd);
}