#pragma once
#include <rad/io/detail/file_common.h>
#include <rad/io/detail/serial_common.h>
#include <rad/io/posix/async_file_impl.h>

#include <chrono>

namespace RAD_LIB_NAMESPACE::io::detail {

    class serial_port_impl : public async_file_impl {
        using msecs = std::chrono::milliseconds;

    public:
        static constexpr native_string_type path_prefix = "/dev/";

        RAD_EXPORT_DECL void open(native_string_type path, serial_access access,
                                  std::error_code& ec);

        RAD_EXPORT_DECL serial_options
        get_options(std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void set_options(const serial_options& opts,
                                         std::error_code& ec) noexcept;
    };
} // namespace RAD_LIB_NAMESPACE::io::detail