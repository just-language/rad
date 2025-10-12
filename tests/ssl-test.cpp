#include <rad/async/io_loop.h>
#include <rad/coro/forked_task.h>
#include <rad/coro/task.h>
#include <rad/local/tcp.h>
#include <rad/net/ssl/openssl_ctx.h>
#include <rad/net/ssl/stream.h>
#include <rad/net/ssl/wolfssl_ctx.h>
#include <rad/net/tcp.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace ssl;

namespace {
    /*
    template <class NextLayer, class Context>
    forked_task TestStream()
    {
            using async_stream = ssl::stream<NextLayer>;

            Context sslctx = Context::generic_client();
            io_loop loop;
            std::error_code ec;

            async_stream stream{ sslctx, loop };

            //stream.handshake(ssl::handshake_type::client);
            //stream.write(buffer(nullptr));
            //stream.read(buffer(nullptr));

            auto handler = [](auto&& ...) {};

            constexpr std::size_t max_alloc_size = []() -> std::size_t
            {
                    constexpr std::size_t alloc_sizes[] =
                    {
                            async_stream::handshake_allocator_size(),
                            async_stream::template
    write_allocator_size<std::array<mutable_buffer, 2>>(),
    async_stream::template read_allocator_size<std::array<mutable_buffer,
    2>>(),
                    };

                    return max_of(alloc_sizes);
            }();

            stack_allocator<max_alloc_size> alloc;

            stream.async_handshake(ssl::handshake_type::client, handler);
            stream.async_handshake(ssl::handshake_type::client, handler,
    alloc);

            stream.async_write(buffer(nullptr), handler);
            stream.async_write(buffer(nullptr), handler, alloc);

            stream.async_write(std::array{ buffer(nullptr), buffer(nullptr)
    }, handler); stream.async_write(std::array{ buffer(nullptr),
    buffer(nullptr) }, handler, alloc);

            stream.async_write(std::array<const_buffer, 2>{ buffer(nullptr),
    buffer(nullptr) }, handler); stream.async_write(std::array<const_buffer,
    2>{ buffer(nullptr), buffer(nullptr) }, handler, alloc);

            stream.async_read(buffer(nullptr), handler);
            stream.async_read(buffer(nullptr), handler, alloc);

            stream.async_read(std::array{ buffer(nullptr), buffer(nullptr)
    }, handler); stream.async_read(std::array{ buffer(nullptr),
    buffer(nullptr) }, handler, alloc);

            stream.async_read_all(buffer(nullptr), handler);
            stream.async_read_all(buffer(nullptr), handler, alloc);

            stream.async_read_all(std::array{ buffer(nullptr),
    buffer(nullptr) }, handler); stream.async_read_all(std::array{
    buffer(nullptr), buffer(nullptr) }, handler, alloc);

            co_await stream.async_handshake(handshake_type::client);
            co_await stream.async_handshake(handshake_type::client, ec);

            auto n = co_await stream.async_write(buffer(nullptr));
            static_assert(std::is_same_v<std::decay_t<decltype(n)>,
    std::size_t>);

            n = co_await stream.async_write(buffer(nullptr), ec);

            n = co_await stream.async_write(std::array{ buffer(nullptr),
    buffer(nullptr) }); n = co_await stream.async_write(std::array{
    buffer(nullptr), buffer(nullptr) }, ec);

            n = co_await stream.async_write(std::array<const_buffer, 2>{
    buffer(nullptr), buffer(nullptr) }); n = co_await
    stream.async_write(std::array<const_buffer, 2>{ buffer(nullptr),
    buffer(nullptr)
    }, ec);

            n = co_await stream.async_read(buffer(nullptr));
            n = co_await stream.async_read(buffer(nullptr), ec);

            n = co_await stream.async_read(std::array{ buffer(nullptr),
    buffer(nullptr) }); n = co_await stream.async_read(std::array{
    buffer(nullptr), buffer(nullptr) }, ec);

            n = co_await stream.async_read_all(buffer(nullptr));
            n = co_await stream.async_read_all(buffer(nullptr), ec);

            n = co_await stream.async_read_all(std::array{ buffer(nullptr),
    buffer(nullptr) }); n = co_await stream.async_read_all(std::array{
    buffer(nullptr), buffer(nullptr) }, ec);
    }


    [[maybe_unused]] void Test()
    {
            TestStream<tcp::socket, ssl::openssl::context>();
            TestStream<local::tcp::socket, ssl::openssl::context>();

            TestStream<tcp::socket, ssl::wolfssl::context>();
            TestStream<local::tcp::socket, ssl::wolfssl::context>();
    }
    */

}