#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/coro/forked_task.h>
#include <rad/coro/task.h>
#include <rad/local/tcp.h>
#include <rad/local/udp.h>
#include <rad/net/socket_options.h>
#include <rad/net/tcp.h>
#include <rad/net/tcp_factories.h>
#include <rad/net/udp.h>

#include <array>
#include <span>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace io;
namespace opts = socket_options;

namespace {

    template <class Protocol>
    forked_task test_async_socket() {
        using socket_type = typename Protocol::socket;
        using endpoint_type = typename Protocol::endpoint_type;
        using native_handle_type = typename socket_type::native_handle_type;
        constexpr bool is_stream = Protocol::is_stream_protocol;

        io_loop loop;
        strand<io_loop> st{loop};
        static_assert(Executor<strand<io_loop>>);
        static_assert(TimerExecutor<strand<io_loop>>);
        static_assert(IoExecutor<strand<io_loop>>);
        socket_type s{loop};
        const socket_type& c_s = s;
        std::error_code ec;
        std::allocator<uint32_t> alloc;
        native_handle_type sock_handle;

        s = {loop, Protocol{}};
        s = {loop, sock_handle};
        s = {loop, socket_fd_t{}};
        s = {loop, Protocol{}, endpoint_type{}};
        s = socket_type{st};
        s = {st, Protocol{}};
        s = {st, sock_handle};
        s = {st, socket_fd_t{}};
        s = {st, Protocol{}, endpoint_type{}};

        s = rebind_executor(st, std::move(s));

        s.next_layer();
        c_s.next_layer();

        s.lowest_layer();
        c_s.lowest_layer();

        s.native_handle();
        c_s.native_handle();

        c_s.native_fd();

        s.executor();
        c_s.executor();

        s.open(Protocol{}, ec);
        s.open(Protocol{});

        c_s.is_open();

        s.close();

        s.shutdown(socket_shutdown::both, ec);
        s.shutdown(socket_shutdown::both);
        s.shutdown();

        s.set_option(opts::reuse_address{true}, ec);
        s.set_option(opts::receive_buffer{10});
        opts::send_buffer opt;
        s.get_option(opt, ec);
        s.get_option(opt);
        opts::keep_alive kopt = s.template get_option<opts::keep_alive>();

        endpoint_type ep = c_s.local_endpoint(ec);
        ep = c_s.local_endpoint();

        ep = c_s.remote_endpoint(ec);
        ep = c_s.remote_endpoint();

        s.bind(ep, ec);
        s.bind(ep);

        s.connect(ep, ec);
        s.connect(ep);

        std::vector<endpoint_type> eprng1;
        std::array<endpoint_type, 5> eprng2;
        std::span<endpoint_type> eprng3;

        s.connect(eprng1, ec);
        s.connect(eprng1);

        s.connect(eprng2, ec);
        s.connect(eprng2);

        s.connect(eprng3, ec);
        s.connect(eprng3);

        const_buffer cbuff;
        mutable_buffer mbuff;
        std::vector<const_buffer> cbuffs1;
        std::array<const_buffer, 5> cbuffs2;
        std::span<const_buffer> cbuffs3;
        const_buffer cbuffs4[10];
        std::vector<mutable_buffer> mbuffs1;
        std::array<mutable_buffer, 5> mbuffs2;
        std::span<mutable_buffer> mbuffs3;
        mutable_buffer mbuffs4[10];

        std::size_t n = s.send(cbuff);
        n = s.send(mbuff);
        n = s.send(cbuffs1);
        n = s.send(cbuffs2);
        n = s.send(cbuffs3);
        n = s.send(cbuffs4);
        n = s.send(mbuffs1);
        n = s.send(mbuffs2);
        n = s.send(mbuffs3);
        n = s.send(mbuffs4);

        n = s.receive(mbuff);
        n = s.receive(mbuffs1);
        n = s.receive(mbuffs2);
        n = s.receive(mbuffs3);
        n = s.receive(mbuffs4);

        if constexpr (!is_stream) {
            n = s.send_to(cbuff, ep);
            n = s.send_to(mbuff, ep);
            n = s.send_to(cbuffs1, ep);
            n = s.send_to(cbuffs2, ep);
            n = s.send_to(cbuffs3, ep);
            n = s.send_to(cbuffs4, ep);
            n = s.send_to(mbuffs1, ep);
            n = s.send_to(mbuffs2, ep);
            n = s.send_to(mbuffs3, ep);
            n = s.send_to(mbuffs4, ep);

            n = s.send_to(cbuff, ep, ec);
            n = s.send_to(mbuff, ep, ec);
            n = s.send_to(cbuffs1, ep, ec);
            n = s.send_to(cbuffs2, ep, ec);
            n = s.send_to(cbuffs3, ep, ec);
            n = s.send_to(cbuffs4, ep, ec);
            n = s.send_to(mbuffs1, ep, ec);
            n = s.send_to(mbuffs2, ep, ec);
            n = s.send_to(mbuffs3, ep, ec);
            n = s.send_to(mbuffs4, ep, ec);

            n = s.send_to(cbuff, ep, transfer_flags::none, ec);
            n = s.send_to(mbuff, ep, transfer_flags::none, ec);
            n = s.send_to(cbuffs1, ep, transfer_flags::none, ec);
            n = s.send_to(cbuffs2, ep, transfer_flags::none, ec);
            n = s.send_to(cbuffs3, ep, transfer_flags::none, ec);
            n = s.send_to(cbuffs4, ep, transfer_flags::none, ec);
            n = s.send_to(mbuffs1, ep, transfer_flags::none, ec);
            n = s.send_to(mbuffs2, ep, transfer_flags::none, ec);
            n = s.send_to(mbuffs3, ep, transfer_flags::none, ec);
            n = s.send_to(mbuffs4, ep, transfer_flags::none, ec);

            n = s.receive_from(mbuff, ep);
            n = s.receive_from(mbuffs1, ep);
            n = s.receive_from(mbuffs2, ep);
            n = s.receive_from(mbuffs3, ep);
            n = s.receive_from(mbuffs4, ep);

            n = s.receive_from(mbuff, ep, ec);
            n = s.receive_from(mbuffs1, ep, ec);
            n = s.receive_from(mbuffs2, ep, ec);
            n = s.receive_from(mbuffs3, ep, ec);
            n = s.receive_from(mbuffs4, ep, ec);

            n = s.receive_from(mbuff, ep, transfer_flags::none, ec);
            n = s.receive_from(mbuffs1, ep, transfer_flags::none, ec);
            n = s.receive_from(mbuffs2, ep, transfer_flags::none, ec);
            n = s.receive_from(mbuffs3, ep, transfer_flags::none, ec);
            n = s.receive_from(mbuffs4, ep, transfer_flags::none, ec);

            s.set_destination(ep, ec);
            s.set_destination(ep);
            s.set_source(ep, ec);
            s.set_source(ep);
        }

        std::ignore = n;

        if constexpr (is_stream) {
            co_await s.async_connect(endpoint_type{});
            co_await s.async_connect(endpoint_type{}, ec);
            auto endpoints = std::array<endpoint_type, 2>{};
            auto se = co_await s.async_connect(endpoints);
            se = co_await s.async_connect(endpoints, ec);
            s.connect(endpoints);
            s.connect(endpoints, ec);
        }

        if constexpr (is_stream) {
            s.async_connect(
                endpoint_type{}, [](const std::error_code&) {}, alloc);
            s.async_connect(endpoint_type{}, [](const std::error_code&) {});
            auto endpoints = std::array<endpoint_type, 2>{};
            s.async_connect(
                endpoints, [](const std::error_code&, const endpoint_type&) {},
                alloc);
            s.async_connect(
                endpoints, [](const std::error_code&, const endpoint_type&) {});
        }

        if constexpr (is_stream) {
            auto write_handler = [](const std::error_code&, std::size_t) {};

            s.async_write(buffer(nullptr), write_handler);
            s.async_write(buffer(nullptr), write_handler, alloc);

            s.async_write(buffer(nullptr), write_handler, transfer_flags::none);
            s.async_write(buffer(nullptr), write_handler, transfer_flags::none,
                          alloc);

            std::vector<const_buffer> buffs;
            s.async_write(buffs, write_handler);
            s.async_write(buffs, write_handler, alloc);

            s.async_write(buffs, write_handler, transfer_flags::none);
            s.async_write(buffs, write_handler, transfer_flags::none, alloc);

            std::vector<mutable_buffer> mbuffs;
            s.async_write(mbuffs, write_handler);
            s.async_write(mbuffs, write_handler, alloc);

            s.async_write(mbuffs, write_handler, transfer_flags::none);
            s.async_write(mbuffs, write_handler, transfer_flags::none, alloc);

            s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler);
            s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler, alloc);

            s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler, transfer_flags::none);
            s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler, transfer_flags::none, alloc);

            s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler);
            s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler, alloc);

            s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler, transfer_flags::none);
            s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                write_handler, transfer_flags::none, alloc);

            const_buffer buffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            s.async_write(buffs_arr, write_handler);
            s.async_write(buffs_arr, write_handler, alloc);

            s.async_write(buffs_arr, write_handler, transfer_flags::none);
            s.async_write(buffs_arr, write_handler, transfer_flags::none,
                          alloc);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            s.async_write(mbuffs_arr, write_handler);
            s.async_write(mbuffs_arr, write_handler, alloc);

            s.async_write(mbuffs_arr, write_handler, transfer_flags::none);
            s.async_write(mbuffs_arr, write_handler, transfer_flags::none,
                          alloc);
        }

        if constexpr (is_stream) {
            auto n = co_await s.async_write(buffer(nullptr));
            n = co_await s.async_write(buffer(nullptr), ec);

            n = co_await s.async_write(buffer(nullptr), transfer_flags::none);
            n = co_await s.async_write(buffer(nullptr), transfer_flags::none,
                                       ec);

            std::vector<const_buffer> buffs;
            n = co_await s.async_write(buffs);
            n = co_await s.async_write(buffs, ec);

            n = co_await s.async_write(buffs, transfer_flags::none);
            n = co_await s.async_write(buffs, transfer_flags::none, ec);

            std::vector<mutable_buffer> mbuffs;
            n = co_await s.async_write(mbuffs);
            n = co_await s.async_write(mbuffs, ec);

            n = co_await s.async_write(mbuffs, transfer_flags::none);
            n = co_await s.async_write(mbuffs, transfer_flags::none, ec);

            n = co_await s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)});
            n = co_await s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                ec);

            n = co_await s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                transfer_flags::none);
            n = co_await s.async_write(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                transfer_flags::none, ec);

            n = co_await s.async_write(std::array<mutable_buffer, 2>{
                buffer(nullptr), buffer(nullptr)});
            n = co_await s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                ec);

            n = co_await s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                transfer_flags::none);
            n = co_await s.async_write(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                transfer_flags::none, ec);

            const_buffer buffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            n = co_await s.async_write(buffs_arr);
            n = co_await s.async_write(buffs_arr, ec);

            n = co_await s.async_write(buffs_arr, transfer_flags::none);
            n = co_await s.async_write(buffs_arr, transfer_flags::none, ec);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            n = co_await s.async_write(mbuffs_arr);
            n = co_await s.async_write(mbuffs_arr, ec);

            n = co_await s.async_write(mbuffs_arr, transfer_flags::none);
            n = co_await s.async_write(mbuffs_arr, transfer_flags::none, ec);
        }

        if constexpr (is_stream) {
            auto read_handler = [](const std::error_code&, std::size_t) {};

            s.async_read_some(buffer(nullptr), read_handler);
            s.async_read_some(buffer(nullptr), read_handler, alloc);

            s.async_read_some(buffer(nullptr), read_handler,
                              transfer_flags::none);
            s.async_read_some(buffer(nullptr), read_handler,
                              transfer_flags::none, alloc);

            std::vector<mutable_buffer> mbuffs;
            s.async_read_some(mbuffs, read_handler);
            s.async_read_some(mbuffs, read_handler, alloc);

            s.async_read_some(mbuffs, read_handler, transfer_flags::none);
            s.async_read_some(mbuffs, read_handler, transfer_flags::none,
                              alloc);

            s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                read_handler);
            s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                read_handler, alloc);

            s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                read_handler, transfer_flags::none);
            s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                read_handler, transfer_flags::none, alloc);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            s.async_read_some(mbuffs_arr, read_handler);
            s.async_read_some(mbuffs_arr, read_handler, alloc);

            s.async_read_some(mbuffs_arr, read_handler, transfer_flags::none);
            s.async_read_some(mbuffs_arr, read_handler, transfer_flags::none,
                              alloc);
        }

        if constexpr (is_stream) {
            auto n = co_await s.async_read_some(buffer(nullptr));
            n = co_await s.async_read_some(buffer(nullptr), ec);

            n = co_await s.async_read_some(buffer(nullptr),
                                           transfer_flags::none);
            n = co_await s.async_read_some(buffer(nullptr),
                                           transfer_flags::none, ec);

            std::vector<mutable_buffer> mbuffs;
            n = co_await s.async_read_some(mbuffs);
            n = co_await s.async_read_some(mbuffs, ec);

            n = co_await s.async_read_some(mbuffs, transfer_flags::none);
            n = co_await s.async_read_some(mbuffs, transfer_flags::none, ec);

            n = co_await s.async_read_some(std::array<mutable_buffer, 2>{
                buffer(nullptr), buffer(nullptr)});
            n = co_await s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                ec);

            n = co_await s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                transfer_flags::none);
            n = co_await s.async_read_some(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                transfer_flags::none, ec);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            n = co_await s.async_read_some(mbuffs_arr);
            n = co_await s.async_read_some(mbuffs_arr, ec);

            n = co_await s.async_read_some(mbuffs_arr, transfer_flags::none);
            n = co_await s.async_read_some(mbuffs_arr, transfer_flags::none,
                                           ec);
        }

        if constexpr (!is_stream) {
            endpoint_type receiver;
            auto write_handler = [](const std::error_code&, std::size_t) {};

            s.async_send_to(buffer(nullptr), receiver, write_handler);
            s.async_send_to(buffer(nullptr), receiver, write_handler, alloc);

            s.async_send_to(buffer(nullptr), receiver, transfer_flags::none,
                            write_handler);
            s.async_send_to(buffer(nullptr), receiver, transfer_flags::none,
                            write_handler, alloc);

            std::vector<const_buffer> buffs;
            s.async_send_to(buffs, receiver, write_handler);
            s.async_send_to(buffs, receiver, write_handler, alloc);

            s.async_send_to(buffs, receiver, transfer_flags::none,
                            write_handler);
            s.async_send_to(buffs, receiver, transfer_flags::none,
                            write_handler, alloc);

            std::vector<mutable_buffer> mbuffs;
            s.async_send_to(mbuffs, receiver, write_handler);
            s.async_send_to(mbuffs, receiver, write_handler, alloc);

            s.async_send_to(mbuffs, receiver, transfer_flags::none,
                            write_handler);
            s.async_send_to(mbuffs, receiver, transfer_flags::none,
                            write_handler, alloc);

            s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, write_handler);
            s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, write_handler, alloc);

            s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none, write_handler);
            s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none, write_handler, alloc);

            s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, write_handler);
            s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, write_handler, alloc);

            s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none, write_handler);
            s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none, write_handler, alloc);

            const_buffer buffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            s.async_send_to(buffs_arr, receiver, write_handler);
            s.async_send_to(buffs_arr, receiver, write_handler, alloc);

            s.async_send_to(buffs_arr, receiver, transfer_flags::none,
                            write_handler);
            s.async_send_to(buffs_arr, receiver, transfer_flags::none,
                            write_handler, alloc);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            s.async_send_to(mbuffs_arr, receiver, write_handler);
            s.async_send_to(mbuffs_arr, receiver, write_handler, alloc);

            s.async_send_to(mbuffs_arr, receiver, transfer_flags::none,
                            write_handler);
            s.async_send_to(mbuffs_arr, receiver, transfer_flags::none,
                            write_handler, alloc);
        }

        if constexpr (!is_stream) {
            endpoint_type receiver;
            auto n = co_await s.async_send_to(buffer(nullptr), receiver);
            n = co_await s.async_send_to(buffer(nullptr), receiver, ec);

            n = co_await s.async_send_to(buffer(nullptr), receiver,
                                         transfer_flags::none);
            n = co_await s.async_send_to(buffer(nullptr), receiver,
                                         transfer_flags::none, ec);

            std::vector<const_buffer> buffs;
            n = co_await s.async_send_to(buffs, receiver);
            n = co_await s.async_send_to(buffs, receiver, ec);

            n = co_await s.async_send_to(buffs, receiver, transfer_flags::none);
            n = co_await s.async_send_to(buffs, receiver, transfer_flags::none,
                                         ec);

            std::vector<mutable_buffer> mbuffs;
            n = co_await s.async_send_to(mbuffs, receiver);
            n = co_await s.async_send_to(mbuffs, receiver, ec);

            n = co_await s.async_send_to(mbuffs, receiver,
                                         transfer_flags::none);
            n = co_await s.async_send_to(mbuffs, receiver, transfer_flags::none,
                                         ec);

            n = co_await s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver);
            n = co_await s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, ec);

            n = co_await s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none);
            n = co_await s.async_send_to(
                std::array<const_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none, ec);

            n = co_await s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver);
            n = co_await s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, ec);

            n = co_await s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none);
            n = co_await s.async_send_to(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                receiver, transfer_flags::none, ec);

            const_buffer buffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            n = co_await s.async_send_to(buffs_arr, receiver);
            n = co_await s.async_send_to(buffs_arr, receiver, ec);

            n = co_await s.async_send_to(buffs_arr, receiver,
                                         transfer_flags::none);
            n = co_await s.async_send_to(buffs_arr, receiver,
                                         transfer_flags::none, ec);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            n = co_await s.async_send_to(mbuffs_arr, receiver);
            n = co_await s.async_send_to(mbuffs_arr, receiver, ec);

            n = co_await s.async_send_to(mbuffs_arr, receiver,
                                         transfer_flags::none);
            n = co_await s.async_send_to(mbuffs_arr, receiver,
                                         transfer_flags::none, ec);
        }

        if constexpr (!is_stream) {
            endpoint_type sender;
            auto read_handler = [](const std::error_code&, std::size_t) {};

            s.async_receive_from(buffer(nullptr), sender, read_handler);
            s.async_receive_from(buffer(nullptr), sender, read_handler, alloc);

            s.async_receive_from(buffer(nullptr), sender, transfer_flags::none,
                                 read_handler);
            s.async_receive_from(buffer(nullptr), sender, transfer_flags::none,
                                 read_handler, alloc);

            std::vector<mutable_buffer> mbuffs;
            s.async_receive_from(mbuffs, sender, read_handler);
            s.async_receive_from(mbuffs, sender, read_handler, alloc);

            s.async_receive_from(mbuffs, sender, transfer_flags::none,
                                 read_handler);
            s.async_receive_from(mbuffs, sender, transfer_flags::none,
                                 read_handler, alloc);

            s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, read_handler);
            s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, read_handler, alloc);

            s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, transfer_flags::none, read_handler);
            s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, transfer_flags::none, read_handler, alloc);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            s.async_receive_from(mbuffs_arr, sender, read_handler);
            s.async_receive_from(mbuffs_arr, sender, read_handler, alloc);

            s.async_receive_from(mbuffs_arr, sender, transfer_flags::none,
                                 read_handler);
            s.async_receive_from(mbuffs_arr, sender, transfer_flags::none,
                                 read_handler, alloc);
        }

        if constexpr (!is_stream) {
            endpoint_type sender;

            auto n = co_await s.async_receive_from(buffer(nullptr), sender);
            n = co_await s.async_receive_from(buffer(nullptr), sender, ec);

            n = co_await s.async_receive_from(buffer(nullptr), sender,
                                              transfer_flags::none);
            n = co_await s.async_receive_from(buffer(nullptr), sender,
                                              transfer_flags::none, ec);

            std::vector<mutable_buffer> mbuffs;
            n = co_await s.async_receive_from(mbuffs, sender);
            n = co_await s.async_receive_from(mbuffs, sender, ec);

            n = co_await s.async_receive_from(mbuffs, sender,
                                              transfer_flags::none);
            n = co_await s.async_receive_from(mbuffs, sender,
                                              transfer_flags::none, ec);

            n = co_await s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender);
            n = co_await s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, ec);

            n = co_await s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, transfer_flags::none);
            n = co_await s.async_receive_from(
                std::array<mutable_buffer, 2>{buffer(nullptr), buffer(nullptr)},
                sender, transfer_flags::none, ec);

            mutable_buffer mbuffs_arr[] = {buffer(nullptr), buffer(nullptr)};
            n = co_await s.async_receive_from(mbuffs_arr, sender);
            n = co_await s.async_receive_from(mbuffs_arr, sender, ec);

            n = co_await s.async_receive_from(mbuffs_arr, sender,
                                              transfer_flags::none);
            n = co_await s.async_receive_from(mbuffs_arr, sender,
                                              transfer_flags::none, ec);
        }
    }

    template <class Protocol>
    rad::forked_task test_async_acceptor() {
        using protocol = Protocol;
        using endpoint_type = typename protocol::endpoint_type;
        using acceptor_type = typename protocol::acceptor;

        io_loop loop;
        std::error_code ec;

        {
            acceptor_type a{loop};
        }
        {
            acceptor_type a{loop, protocol{}};
        }
        {
            acceptor_type a{loop, protocol{}, endpoint_type{}};
        }

        acceptor_type a{loop};
        a.open(protocol{});
        a.open(protocol{}, ec);

        a.bind(endpoint_type{});
        a.bind(endpoint_type{}, ec);

        a.listen();
        a.listen(10);
        a.listen(10, ec);

        auto [s, e] = co_await a.async_accept();

        static_assert(std::is_same_v<std::decay_t<decltype(s)>,
                                     typename protocol::socket>);
        static_assert(std::is_same_v<std::decay_t<decltype(e)>, endpoint_type>);

        std::tie(s, e) = co_await a.async_accept(ec);
        e = co_await a.async_accept(s);
        e = co_await a.async_accept(s, ec);
        s = co_await a.async_accept(e);
        s = co_await a.async_accept(e, ec);

        co_await a.async_accept(s, e);
        co_await a.async_accept(s, e, ec);

        a.accept(s, e, ec);
        a.accept(s, e);
        a.accept(s, ec);
        a.accept(s);

        s = a.accept(loop, e, ec);
        s = a.accept(e, ec);
        s = a.accept(loop, e);
        s = a.accept(e);
        s = a.accept(loop, ec);
        s = a.accept(loop);
        s = a.accept(ec);
        s = a.accept();
    }

    struct allocator_types {
        using ops = op_alloc_type;

        using write_buffers_type = const_buffer;
        using read_buffers_type = mutable_buffer;
        using protocol_type = tcp;
        using endpoint_type = endpoint;
        using endpoints_range = std::vector<endpoint>;
        using accepted_type = tcp::socket;

        static constexpr ops op_type =
            ops::write | ops::read | ops::sendto | ops::recvfrom |
            ops::connect | ops::connect_range | ops::accept_no_peer_ref |
            ops::accept_peer_ref | ops::read_all;

        static constexpr std::size_t max_handler_size = sizeof(void*) * 2;
    };

    [[maybe_unused]] void do_compile_tests() {
        [[maybe_unused]] constexpr std::size_t alloc_size =
            tcp::socket::max_allocator_size<allocator_types>();

        test_async_socket<tcp>();
        test_async_socket<udp>();
        test_async_socket<local::tcp>();
        test_async_socket<local::udp>();

        test_async_acceptor<tcp>();
        test_async_acceptor<local::tcp>();
    }

} // namespace
