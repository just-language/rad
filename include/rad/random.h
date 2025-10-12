#pragma once
#include <rad/string.h>

#include <random>

namespace RAD_LIB_NAMESPACE {
    template <class T>
    inline T random_number(T minimum, T maximum) {
        std::random_device rnd;
        std::default_random_engine rng(rnd());
        std::uniform_int_distribution<T> dist(minimum, maximum);
        return dist(rng);
    }

    inline char random_char(char minimum = 0, char maximum = -1) {
        return static_cast<char>(random_number<int>(static_cast<int>(minimum),
                                                    static_cast<int>(maximum)));
    }

    inline wchar_t random_wchar(wchar_t minimum, wchar_t maximum) {
        return static_cast<wchar_t>(random_number<int>(
            static_cast<int>(minimum), static_cast<int>(maximum)));
    }

    inline unsigned char random_byte(unsigned char minimum = 0,
                                     unsigned char maximum = 255) {
        return static_cast<uint8_t>(random_number<uint32_t>(
            static_cast<uint32_t>(minimum), static_cast<uint32_t>(maximum)));
    }

    inline int random_int(int minimum, int maximum) {
        return random_number(minimum, maximum);
    }

    inline long random_long(long minimum, long maximum) {
        return random_number(minimum, maximum);
    }

    inline long long random_longlong(long long minimum, long long maximum) {
        return random_number(minimum, maximum);
    }

    inline unsigned int random_uint(unsigned int minimum,
                                    unsigned int maximum) {
        return random_number(minimum, maximum);
    }

    inline unsigned long random_ulong(unsigned long minimum,
                                      unsigned long maximum) {
        return random_number(minimum, maximum);
    }

    inline unsigned long long random_ulonglong(unsigned long long minimum,
                                               unsigned long long maximum) {
        return random_number(minimum, maximum);
    }

    inline std::string random_string(size_t length, bool upper_only = false,
                                     bool lower_only = false) {
        std::string random_str;
        random_str.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            if (!upper_only && !lower_only) {
                int upper = random_int(0, 1);
                if (upper) {
                    random_str.push_back(random_char(65, 90));
                }
                else {
                    random_str.push_back(random_char(97, 122));
                }
            }
            else if (upper_only) {
                random_str.push_back(random_char(65, 90));
            }
            else if (lower_only) {
                random_str.push_back(random_char(97, 122));
            }
        }
        return random_str;
    }

} // namespace RAD_LIB_NAMESPACE
