#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/coro/forked_task.h>
#include <rad/coro/task.h>
#include <rad/io/serial_port.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;

namespace {
    struct allocator_types {
        using ops = op_alloc_type;

        using write_buffers_type = const_buffer;
        using read_buffers_type = mutable_buffer;

        static constexpr ops op_type = ops::write | ops::read | ops::read_all;

        static constexpr std::size_t max_handler_size = sizeof(void*) * 3;
    };

    [[maybe_unused]] forked_task compile_tests() {
        io_loop loop;
        strand<io_loop> st{loop};

        auto rwhandler = [](const std::error_code&, std::size_t) {};
        std::string_view port_name = "COM5";
        std::error_code ec;
        serial_options opts;

        constexpr std::size_t max_alloc_size =
            serial_port::max_allocator_size<allocator_types>();
        std::array<std::uint8_t, max_alloc_size> alloc_buff;
        static_buffer_allocator<max_alloc_size> alloc{alloc_buff};

        {
            serial_port port{loop};
            serial_port port2{st};

            serial_port{loop, "port"};
            serial_port{loop, "port", serial_access::read_write};
            serial_port{loop, L"port", serial_access::read_write};
            serial_port{loop, port_name, serial_access::read_write};

            serial_port{std::move(port)};
            port = std::move(port2);
        }

        serial_port port{loop};
        port.lowest_layer();
        port.native_handle();
        port.native_fd();
        port.path();
        port.executor();

        port.open(port_name);
        port.open(port_name, serial_access::read);
        port.open(port_name, serial_access::read, ec);
        port.open(port_name, ec);

        port.is_open();
        port.close();
        port.cancel();

        std::size_t n = co_await port.async_write(buffer(nullptr));
        n = co_await port.async_write(buffer(nullptr), ec);

        n = co_await port.async_read_some(buffer(nullptr));
        n = co_await port.async_read_some(buffer(nullptr), ec);

        n = co_await port.async_read(buffer(nullptr));
        n = co_await port.async_read(buffer(nullptr), ec);

        port.async_write(buffer(nullptr), rwhandler);
        port.async_write(buffer(nullptr), rwhandler, alloc);

        port.async_read_some(buffer(nullptr), rwhandler);
        port.async_read_some(buffer(nullptr), rwhandler, alloc);

        port.async_read(buffer(nullptr), rwhandler);
        port.async_read(buffer(nullptr), rwhandler, alloc);

        opts = port.get_options(ec);
        port.set_options(opts, ec);

        std::ignore = n;
    }
} // namespace

bool do_serial_port_tests() {
    return true;
}