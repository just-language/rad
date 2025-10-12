#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/coro/when_all.h>
#include <rad/net/tcp.h>
#include <rad/unittest/unittest.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using rad::net::tcp;
using namespace unittest;

namespace {
    void test_default_ctor(io_loop& ex) {
        const char* msg = "default constructor is flawed";
        tcp::socket s{ex};
        REQUIRE(s.native_fd() == -1, msg);
        REQUIRE(s.native_handle() == tcp::socket::native_handle_type{}, msg);
        REQUIRE(std::addressof(s.executor()) == std::addressof(ex), msg);
        REQUIRE(!s.is_open(), msg);
    }

    template <class Protocol>
    void test_opening_ctor(io_loop& ex, const Protocol& proto) {
        const char* msg = "opening constructor is flawed";
        using socket_type = typename Protocol::socket;
        socket_type s{ex, proto};
        REQUIRE(s.native_fd() != -1, msg);
        REQUIRE(s.native_handle() != tcp::socket::native_handle_type{}, msg);
        REQUIRE(std::addressof(s.executor()) == std::addressof(ex), msg);
        REQUIRE(s.is_open(), msg);
    }

    template <class Protocol, class Epoint>
    void test_binding_ctor(io_loop& ex, const Protocol& proto,
                           const Epoint& epoint) {
        const char* msg = "opening constructor is flawed";
        using socket_type = typename Protocol::socket;
        socket_type s{ex, proto, epoint};
        REQUIRE(s.native_fd() != -1, msg);
        REQUIRE(s.native_handle() != tcp::socket::native_handle_type{}, msg);
        REQUIRE(std::addressof(s.executor()) == std::addressof(ex), msg);
        REQUIRE(s.is_open(), msg);
        auto local_addr =
            TRY_RETURN(s.local_endpoint(), "local_address should not throw");
        REQUIRE(local_addr == epoint,
                "requested and used local endpoints don't match");
    }

    task<tcp::socket> do_accept(tcp::acceptor& acceptor) {
        co_return std::move((co_await acceptor.async_accept()).first);
    }

    task<> do_connect(tcp::socket& s, const net::endpoint& epoint) {
        co_await s.async_connect(epoint);
    }

    task<size_t> do_write(tcp::socket& s, const_buffer buff) {
        co_return co_await s.async_write(buff);
    }

    template <std::size_t N>
    task<size_t> do_writev(tcp::socket& s,
                           const std::array<mutable_buffer, N>& buffs) {
        co_return co_await s.async_write(buffs);
    }

    task<size_t> do_read(tcp::socket& s, mutable_buffer buff) {
        co_return co_await s.async_read_some(buff);
    }

    template <std::size_t N>
    task<size_t> do_readv_all(tcp::socket& s,
                              const std::array<mutable_buffer, N>& buffs) {
        co_return co_await s.async_read(buffs);
    }

    task<size_t> do_read_all(tcp::socket& s, mutable_buffer buff) {
        co_return co_await s.async_read(buff);
    }

    task<> do_read_canceled(tcp::socket& s, mutable_buffer buff) {
        std::error_code ec;
        size_t n = co_await s.async_read_some(buff, ec);
        assert_true(as<bool>(ec), "async_read was not canceled");
        REQUIRE(ec == std::make_error_code(std::errc::operation_canceled),
                "async_read failed due to an error other than canceled");
        REQUIRE(n == 0, "n != 0 when canceled");
    }

    task<> do_cancel(tcp::socket& s) {
        s.cancel();
        co_return;
    }

    task<> test_write_read(io_executor& ex) {
        tcp::acceptor acceptor{ex, tcp::ipv4(),
                               net::endpoint{net::ipv4::loopback(), 0}};
        acceptor.listen();
        tcp::socket s1{ex};
        auto [s2, _unused] = co_await (
            do_accept(acceptor) && do_connect(s1, acceptor.local_endpoint()));

        std::vector<uint8_t> buff1(1024);
        std::vector<uint8_t> buff2(buff1.size());
        std::size_t n1 = 0, n2 = 0;

        for (auto i : range(buff1.size())) {
            buff1[i] = static_cast<uint8_t>(i % 255);
        }

        std::tie(n1, n2) = co_await assert_returns(
            [&, &s1 = s1, &s2 = s2]() -> task<std::tuple<size_t, size_t>> {
                co_return co_await (do_write(s1, buffer(buff1)) &&
                                    do_read(s2, buffer(buff2)));
            },
            "async_write and async_read should not throw");
        REQUIRE(n1 == buff1.size(),
                "async_write didn't transfer the whole message");
        REQUIRE(n1 == n2, "write size != read size");
        assert_eq(buff1, buff2, "buff1 != buff2");

        std::memset(buff2.data(), 0, buff1.size());
        std::tie(n1, n2) = co_await assert_returns(
            [&, &s1 = s1, &s2 = s2]() -> task<std::tuple<size_t, size_t>> {
                co_return co_await (do_read(s2, buffer(buff2)) &&
                                    do_write(s1, buffer(buff1)));
            },
            "async_write and async_read should not throw");
        REQUIRE(n1 == buff1.size(),
                "async_write didn't transfer the whole message");
        REQUIRE(n1 == n2, "write size != read size");
        assert_eq(buff1, buff2, "buff1 != buff2");

        co_await assert_returns(
            [&]() -> task<> {
                co_await (do_read_canceled(s1, buffer(buff1)) && do_cancel(s1));
            },
            "async_read and cancel should not throw");

        buff2.resize(buff1.size() * 2);
        std::memset(buff2.data(), 0, buff1.size());

        std::tie(n1, n2) = co_await assert_returns(
            [&, &s1 = s1, &s2 = s2]() -> task<std::tuple<size_t, size_t>> {
                // ensure first write completes before second write!
                std::size_t n1 = co_await do_write(s1, buffer(buff1));
                auto [n2, n3] = co_await (do_read_all(s2, buffer(buff2)) &&
                                          do_write(s1, buffer(buff1)));
                co_return std::tuple{n1 + n3, n2};
            },
            "async_write and async_read_all should not throw");
        REQUIRE(n1 == buff1.size() * 2,
                "async_write didn't transfer the whole message");
        REQUIRE(n1 == n2, "write size != read size");
        std::span sp{buff2};
        REQUIRE(std::equal(buff1.begin(), buff1.end(), sp.begin()),
                "buff1 != buff2");
        REQUIRE(
            std::equal(buff1.begin(), buff1.end(), sp.begin() + buff1.size()),
            "buff1 != buff2");

        std::memset(buff2.data(), 0, buff1.size());
        std::vector<uint8_t> buff3(buff1.size());
        buff2.resize(buff1.size());

        std::tie(n1, n2) = co_await assert_returns(
            [&, &s1 = s1, &s2 = s2]() -> task<std::tuple<size_t, size_t>> {
                // ensure first write completes before second write!
                std::size_t n1 = co_await do_write(s1, buffer(buff1));
                auto [n2, n3] =
                    co_await (do_readv_all(s2, std::array{buffer(buff2),
                                                          buffer(buff3)}) &&
                              do_write(s1, buffer(buff1)));
                co_return std::tuple{n1 + n3, n2};
            },
            "async_write and async_read_all should not throw");
        REQUIRE(n1 == buff1.size() * 2,
                "async_write didn't transfer the whole message");
        REQUIRE(n1 == n2, "write size != read size");
        sp = {buff2};
        REQUIRE(std::equal(buff1.begin(), buff1.end(), sp.begin()),
                "buff1 != buff2");
        sp = {buff3};
        REQUIRE(std::equal(buff1.begin(), buff1.end(), sp.begin()),
                "buff1 != buff2");

        std::memset(buff2.data(), 0, buff2.size());
        std::memset(buff3.data(), 0, buff3.size());

        std::tie(n1, n2) = co_await assert_returns(
            [&, &s1 = s1, &s2 = s2]() -> task<std::tuple<size_t, size_t>> {
                auto [n1, n2] = co_await (
                    do_readv_all(s2,
                                 std::array{buffer(buff2), buffer(buff3)}) &&
                    do_writev(s1, std::array{buffer(buff1), buffer(buff1)}));
                co_return std::tuple{n2, n1};
            },
            "async_write and async_read_all should not throw");
        REQUIRE(n1 == buff1.size() * 2,
                "async_write didn't transfer the whole message");
        REQUIRE(n1 == n2, "write size != read size");
        sp = {buff2};
        REQUIRE(std::equal(buff1.begin(), buff1.end(), sp.begin()),
                "buff1 != buff2");
        sp = {buff3};
        REQUIRE(std::equal(buff1.begin(), buff1.end(), sp.begin()),
                "buff1 != buff2");
    }
} // namespace

namespace tests_fn {
    bool do_socket_tests() {
        int failed_n = 0;
        auto ex_handler = [&failed_n](std::exception_ptr ex_ptr) {
            try {
                std::rethrow_exception(ex_ptr);
            }
            catch (const exception& ex) {
                std::cerr << "[!] socket tests failed ! " << ex.detailed()
                          << '\n';
                ++failed_n;
            }
        };

        try {
            io_loop loop;
            strand<io_loop> st{loop};
            test_default_ctor(loop);
            test_opening_ctor(loop, tcp::ipv4());
            test_opening_ctor(loop, tcp::ipv6());
            tcp::endpoint_type epoint4{net::ipv4{{127, 0, 0, 1}}, 8644};
            test_binding_ctor(loop, tcp::ipv4(), epoint4);
            spawn(loop, test_write_read(loop), ex_handler);
            // spawn(loop, test_write_read(st), ex_handler);
            loop.run();
            std::cout << "[*] socket tests passed\n";
        }
        catch (const exception& ex) {
            std::cerr << "[!] socket tests failed ! " << ex.detailed() << '\n';
            return false;
        }
        catch (const std::exception& ex) {
            std::cerr << "[!] socket tests failed ! " << ex.what() << '\n';
            return false;
        }
        return !failed_n;
    }
} // namespace tests_fn