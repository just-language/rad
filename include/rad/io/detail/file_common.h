#pragma once
#include <rad/libbase.h>

#include <chrono>
#include <limits>

namespace RAD_LIB_NAMESPACE::io::files {
    // constants here are equivalent to their windows values

    enum class access : unsigned long {
        write = 0x40000000L,
        read = 0x80000000L,
        read_write = 0x40000000L | 0x80000000L,
    };

    enum class share_mode : unsigned long {
        none = 0,
        write = 0x00000002,
        read = 0x00000001,
        share_delete = 0x00000004,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(share_mode);

    enum class create_mode {
        if_new_file_or_fail =
            1,         // create only if the file doesn't exist otherwise fails
        overwrite = 2, // create even if the file already exists , this
                       // overwites the file if it exist
        if_new_file_or_open =
            4, // // opens if exists otherwise creates a new file
    };

    enum class open_mode {
        existing = 3,            // opens only if exists otherwise fails
        create_if_not_exist = 4, // opens if exists otherwise creates a new file
    };

    enum class attributes : unsigned long {
        archive = 0x00000020,
        encrypted = 0x00004000,
        hidden = 0x00000002,
        normal = 0x00000080,
        offline = 0x00001000,
        read_only = 0x00000001,
        system = 0x00000004,
        temp = 0x00000100,
        backup = 0x02000000,
        delete_on_close = 0x04000000,
        no_buffering = 0x20000000,
        overlapped = 0x40000000,
        posix = 0x01000000,
        random_access = 0x10000000,
        write_direct = 0x80000000,
        sequential = 0x08000000,
        invalid = static_cast<unsigned long>(-1),
    };

    RAD_OVERLOAD_ENUM_OPERATORS(attributes);

    enum class seek_mode : unsigned long {
        begin = 0,
        end = 2,
        current = 1,
    };

    struct times_t {
        std::chrono::file_clock::time_point creation = {};
        std::chrono::file_clock::time_point last_write = {};
        std::chrono::file_clock::time_point last_access = {};
    };

} // namespace RAD_LIB_NAMESPACE::io::files