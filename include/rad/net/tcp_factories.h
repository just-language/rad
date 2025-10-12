#pragma once
#include <rad/coro/task.h>
#include <rad/net/tcp.h>

namespace RAD_LIB_NAMESPACE::net {
    inline auto tcp::connect(io_executor& ex, const endpoint_type& epoint)
        -> task<socket> {
        socket s{ex, tcp{epoint.family()}};
        co_await s.async_connect(epoint);
        co_return std::move(s);
    }

    template <EndpointSequence<endpoint> EndpointRange>
    inline auto tcp::connect(io_executor& ex, const EndpointRange& endpoints)
        -> task<socket> {
        socket s{ex};
        co_await s.async_connect(endpoints);
        co_return std::move(s);
    }
}; // namespace RAD_LIB_NAMESPACE::net