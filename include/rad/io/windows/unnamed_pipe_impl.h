#pragma once
#include <rad/io/windows/async_file_impl.h>

#include <array>

namespace RAD_LIB_NAMESPACE::io::detail {

    class unnamed_pipe_impl : public async_file_impl {
    public:
        using async_file_impl::async_file_impl;

        static RAD_EXPORT_DECL std::array<unnamed_pipe_impl, 2>
        create_pair(io_executor& ex, std::error_code& ec) noexcept;
    };
} // namespace RAD_LIB_NAMESPACE::io::detail