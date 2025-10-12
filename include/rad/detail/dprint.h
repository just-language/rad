#pragma once
#include <cstdio>
#include <cstring>

#ifndef NDEBUG
#define dprint(...) printf(__VA_ARGS__)
#else
#define dprint(...) ((void)0)
#endif // !NDEBUG