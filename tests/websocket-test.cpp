#include <rad/net/tcp.h>
#include <rad/net/websocket/async_websocket.h>
#ifdef RAD_ENABLE_SSL
#include <rad/net/ssl/async_sslstream.h>
#include <rad/net/ssl/sslstream.h>
#endif // RAD_ENABLE_SSL

using namespace LIB_NAMESPACE;
using namespace io;
using namespace net;
using namespace websocket;
#ifdef RAD_ENABLE_SSL
using namespace ssl;
#endif // RAD_ENABLE_SSl

namespace {
    template <class NextLayer, class... Args>
    detached_task Test(Args&&... args) {
        using stream = async_websocket<NextLayer>;
        io_loop loop;
        auto handler = [](auto&&...) {};
        io::stack_allocator<1024> alloc;

        stream wsock{std::forward<Args>(args)..., loop};

        http::request req;
        http::response res;
        std::vector<uint8_t> read_buffer;

        wsock.accept(req);
        wsock.accept();

        wsock.handshake("", "", "");
        wsock.handshake(res, "", "");

        wsock.write(io::buffer(nullptr));
        // wsock.receive_frame(dynamic_buffer(read_buffer));

        wsock.async_handshake("", "", handler);
        wsock.async_handshake("", "", handler, "", alloc);

        wsock.async_write(io::buffer(nullptr), handler);
        wsock.async_write(io::buffer(nullptr), handler, alloc);

        wsock.async_read(read_buffer, handler);
        wsock.async_read(read_buffer, handler, alloc);

        co_await wsock.coro_handshake("", "");
        auto n = co_await wsock.coro_write(io::buffer(nullptr));
        n = co_await wsock.coro_read(read_buffer);
    }

    [[maybe_unused]] void Test() {
        Test<tcp::async_socket>();
#ifdef RAD_ENABLE_SSL
        openssl::context octx{sslmethod::tlsv1};
        wolfssl::context wctx{sslmethod::tlsv1};
        Test<openssl::async_stream<tcp::async_socket>>(octx);
        Test<wolfssl::async_stream<tcp::async_socket>>(wctx);
#endif // RAD_ENABLE_SSL
    }

} // namespace