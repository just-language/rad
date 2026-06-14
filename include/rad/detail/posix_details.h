#pragma once

#if !defined(__unix__) && !defined(__APPLE__)
#error "this file is to be compiled only on posix os"
#endif

#include <cstdint>

extern "C" {

using socket_fd_t = int;

using socket_len_t = unsigned int;

extern int close(int fd);
}