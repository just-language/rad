#pragma once
// #define RAD_ENABLE_HTTP2_DEBUG 1
#ifdef RAD_ENABLE_HTTP2_DEBUG
#include <inttypes.h>

#include <cstdio>
#define http2_printf(...) printf(__VA_ARGS__)
#else
#define http2_printf(...)
#endif // RAD_ENABLE_HTTP2_DEBUG
