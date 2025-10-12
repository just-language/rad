#include <rad/local/tcp.h>
#include <rad/local/udp.h>
#include <rad/net/tcp.h>
#include <rad/net/udp.h>

using namespace LIB_NAMESPACE;
using namespace io;
using namespace net;

namespace {

    template <class Protocol, bool stream>
    io::detached_task Test() {
        using endpoint_type = typename Protocol::endpoint_type;
        using endpoints_type = typename Protocol::endpoints_type;
        using socket_type = typename Protocol::async_socket;

        io_loop loop;
        typename Protocol::async_socket sock{loop};
        endpoint_type epoint = sock.local_endpoint();
        epoint = sock.remote_endpoint();
        sock.bind(epoint);
        sock.close();

        sock.open(Protocol{});

        if constexpr (stream) {
            sock.listen(10);
            sock.connect(endpoint_type{});
            sock.connect(endpoints_type{});
            auto _s = sock.accept();
            sock.accept(_s);
            sock.accept(_s, epoint);
        }

        sock.write(io::buffer(nullptr));
        sock.read(io::buffer(nullptr));
        sock.read_all(io::buffer(nullptr));
        sock.send_to(io::buffer(nullptr), epoint);
        sock.receive_from(io::buffer(nullptr), epoint);

        constexpr std::size_t max_alloc_size = []() -> std::size_t {
            if constexpr (stream) {
                constexpr std::size_t alloc_sizes[] = {
                    socket_type::accept_allocator_size(),
                    socket_type::connect_allocator_size(),
                    socket_type::connect_range_allocator_size(),
                    socket_type::write_allocator_size(),
                    socket_type::read_allocator_size(),
                };
                return max_of(alloc_sizes);
            }
            else {
                constexpr std::size_t alloc_sizes[] = {
                    socket_type::write_allocator_size(),
                    socket_type::read_allocator_size(),
                };
                return max_of(alloc_sizes);
            }
        }();

        io::stack_allocator<max_alloc_size> alloc;

        auto handler = [](auto&&...) {};

        if constexpr (stream) {
            sock.async_connect(endpoint_type{}, handler);
            sock.async_connect(endpoint_type{}, handler, alloc);
            sock.async_connect(endpoints_type{}, handler);
            sock.async_connect(endpoints_type{}, handler, alloc);
            sock.async_accept(sock, handler);
            sock.async_accept(sock, handler, alloc);
        }

        sock.async_write(io::buffer(nullptr), handler);
        sock.async_write(io::buffer(nullptr), handler, alloc);
        sock.async_read(io::buffer(nullptr), handler);
        sock.async_read(io::buffer(nullptr), handler, alloc);
        sock.async_send_to(io::buffer(nullptr), endpoint_type{}, handler);
        sock.async_send_to(io::buffer(nullptr), endpoint_type{}, handler,
                           alloc);
        sock.async_receive_from(io::buffer(nullptr), epoint, handler);
        sock.async_receive_from(io::buffer(nullptr), epoint, handler, alloc);

        if constexpr (stream) {
            co_await sock.coro_connect(endpoint_type{});
            epoint = co_await sock.coro_connect(endpoints_type{});
            auto [s, addr] = co_await sock.coro_accept();
        }

        auto n = co_await sock.coro_write(io::buffer(nullptr));
        n = co_await sock.coro_read(io::buffer(nullptr));
        n = co_await sock.coro_send_to(io::buffer(nullptr), epoint);
        std::tie(n, epoint) =
            co_await sock.coro_receive_from(io::buffer(nullptr));
    }

    template <class Protocol>
    io::detached_task TestAcceptor() {
        io_loop loop;
        using endpoint_type = typename Protocol::endpoint_type;
        using acceptor = typename Protocol::async_acceptor;

        endpoint_type epoint;
        auto handler = [](const std::error_code& ec,
                          const endpoint_type& epoint) {};
        io::stack_allocator<200> alloc;

        acceptor server{loop, Protocol{}, epoint};

        auto s = server.accept();
        server.async_accept(s, handler);
        server.async_accept(s, handler, alloc);

        std::tie(s, epoint) = co_await server.coro_accept();
    }

    [[maybe_unused]] void TestAll() {
        Test<tcp, true>();
        Test<udp, false>();
        Test<local::tcp, true>();
        Test<local::udp, false>();

        TestAcceptor<tcp>();
        TestAcceptor<local::tcp>();
    }

} // namespace