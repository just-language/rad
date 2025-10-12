#include <rad/local/tcp.h>
#include <rad/local/udp.h>
#include <rad/net/socket_options.h>
#include <rad/net/tcp.h>
#include <rad/net/udp.h>

#include <array>
#include <span>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
namespace opts = socket_options;

namespace {
    /*
    template <class Protocol>
    void test_sync_socket_fn() {
        using socket_type = typename Protocol::bsocket;
        using endpoint_type = typename Protocol::endpoint_type;
        using native_handle_type = typename socket_type::native_handle_type;
        constexpr bool is_stream = Protocol::is_stream_protocol;

        socket_type s;
        const socket_type& c_s = s;
        std::error_code ec;

        s = {Protocol{}};
        s = {native_handle_type{}};
        s = {socket_fd_t{}};
        s = {Protocol{}, endpoint_type{}};

        s.lowest_layer();
        c_s.lowest_layer();

        s.native_handle();
        (void)c_s.native_handle();

        c_s.native_fd();

        s.open(Protocol{}, ec);
        s.open(Protocol{});

        (void)c_s.is_open();

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
        std::ignore = n;

        if constexpr (is_stream) {
            s.listen(10, ec);
            s.listen(10);

            s.accept(s, ep, ec);
            s.accept(s, ep);
            s.accept(s, ec);
            s.accept(s);
            s = s.accept(ep, ec);
            s = s.accept(ep);
            s = s.accept(ec);
            s = s.accept();
        }
        else {
            s.set_destination(ep, ec);
            s.set_destination(ep);
            s.set_source(ep, ec);
            s.set_source(ep);
        }
    }

    [[maybe_unused]] void do_compile_tests() {
        test_sync_socket_fn<tcp>();
        test_sync_socket_fn<udp>();
        test_sync_socket_fn<local::tcp>();
        test_sync_socket_fn<local::udp>();
    }
    */
} // namespace
