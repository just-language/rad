#include <rad/async/io_loop.h>
#include <rad/channels/mpsc_channel.h>
#include <rad/channels/oneshot_channel.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/threading/thread_pool.h>
#include <rad/unittest/unittest.h>

#include <iostream>
#include <string>

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;

namespace {
    task<> test_oneshot(any_executor& ex) {
        ((void)ex);
        co_return;

        sync::oneshot::channel<std::string> channel{ex};
        static_assert(
            std::is_same_v<decltype(channel)::value_type, std::string>,
            "incorrect channel value_type");
        static_assert(
            std::is_same_v<decltype(channel)::executor_type, any_executor>,
            "incorrect channel executor_type");

        static_assert(
            !std::is_copy_constructible_v<decltype(channel)::sender_type>);
        static_assert(
            !std::is_copy_assignable_v<decltype(channel)::sender_type>);
        static_assert(
            std::is_move_constructible_v<decltype(channel)::sender_type>);
        static_assert(
            std::is_move_assignable_v<decltype(channel)::sender_type>);

        static_assert(
            !std::is_copy_constructible_v<decltype(channel)::receiver_type>);
        static_assert(
            !std::is_copy_assignable_v<decltype(channel)::receiver_type>);
        static_assert(
            std::is_move_constructible_v<decltype(channel)::receiver_type>);
        static_assert(
            std::is_move_assignable_v<decltype(channel)::receiver_type>);

        REQUIRE(&ex == &channel.executor(),
                "provided executor is not the same as channel executor");

        auto [s, r] = assert_returns([&] { return channel.split(); },
                                     "split must not throw");
        assert_false(channel.is_send_closed(), "send half must not be closed");
        assert_false(channel.is_receive_closed(),
                     "receive half must not be closed");

        assert_throws<std::system_error>(
            [&] { channel.split(); }, "split didn't throw when called twice");
        assert_throws<std::system_error>(
            [&] { channel.make_sender(); },
            "make_sender didn't throw when called twice");
        assert_throws<std::system_error>(
            [&] { channel.make_receiver(); },
            "make_receiver didn't throw when called twice");

        {
            sync::oneshot::channel<std::string> ch{ex};
            auto s = assert_returns([&] { return ch.make_sender(); },
                                    "make_sender must not throw");
            auto r = assert_returns([&] { return ch.make_receiver(); },
                                    "make_receiver must not throw");

            assert_throws<std::system_error>(
                [&] { ch.split(); }, "split didn't throw when called twice");
            assert_throws<std::system_error>(
                [&] { ch.make_sender(); },
                "make_sender didn't throw when called twice");
            assert_throws<std::system_error>(
                [&] { ch.make_receiver(); },
                "make_receiver didn't throw when called twice");
        }

        // send, receive

        std::string value = "some message";
        assert_returns([&, &s = s] { s.send(value); }, "send must not throw");
        assert_true(channel.is_send_closed(),
                    "send was not closed after first send");

        std::string msg = co_await assert_returns(
            [&r = r]() -> task<std::string> { co_return co_await r.receive(); },
            "receive must not throw");
        assert_eq(value, msg, "the value sent", " the value received");
        assert_true(channel.is_receive_closed(),
                    "receive was not closed after first receive");

        // send, close send, receive
        {
            sync::oneshot::channel<std::string> ch{ex};
            auto [s, r] = assert_returns([&] { return ch.split(); },
                                         "split must not throw");
            assert_returns([&, &s = s] { s.send(value); },
                           "send must not throw");
            assert_true(channel.is_send_closed(),
                        "send was not closed after first send");
            s.close();

            std::string msg = co_await assert_returns(
                [&r = r]() -> task<std::string> {
                    co_return co_await r.receive();
                },
                "receive must not throw");
            assert_eq(value, msg, "the value sent", " the value received");
            assert_true(channel.is_receive_closed(),
                        "receive was not closed after first receive");
        }

        // close receive, send

        {
            sync::oneshot::channel<std::string> ch{ex};
            auto [s, r] = assert_returns([&] { return ch.split(); },
                                         "split must not throw");
            r.close();
            assert_throws<std::system_error>(
                [&s = s] { s.send(""); },
                "send didn't throw after receiver was closed");
        }
    }

    task<> test_mpsc(any_executor& ex) {
        ((void)ex);
        co_return;

        using channel_t = sync::mpsc::channel<std::string>;

        channel_t ch{ex};
        static_assert(std::is_same_v<decltype(ch)::value_type, std::string>,
                      "incorrect channel value_type");
        static_assert(std::is_same_v<decltype(ch)::executor_type, any_executor>,
                      "incorrect channel executor_type");

        static_assert(std::is_copy_constructible_v<decltype(ch)::sender_type>);
        static_assert(std::is_copy_assignable_v<decltype(ch)::sender_type>);
        static_assert(std::is_move_constructible_v<decltype(ch)::sender_type>);
        static_assert(std::is_move_assignable_v<decltype(ch)::sender_type>);

        static_assert(
            !std::is_copy_constructible_v<decltype(ch)::receiver_type>);
        static_assert(!std::is_copy_assignable_v<decltype(ch)::receiver_type>);
        static_assert(
            std::is_move_constructible_v<decltype(ch)::receiver_type>);
        static_assert(std::is_move_assignable_v<decltype(ch)::receiver_type>);

        TEST_EQ_MSG(&ex, &ch.executor(),
                    "provided executor is not the same as channel executor");

        {
            auto [s, r] = assert_returns([&] { return ch.split(); },
                                         "split must not throw");
            TEST_EQ_MSG(ch.is_send_closed(), false,
                        "send half must not be closed");
            TEST_EQ_MSG(ch.is_receive_closed(), false,
                        "receive half must not be closed");

            assert_throws<std::system_error>(
                [&] { ch.split(); }, "split didn't throw when called twice");
            auto s2 = assert_returns([&] { return ch.make_sender(); },
                                     "make_sender threw when called twice");
            assert_throws<std::system_error>(
                [&] { ch.make_receiver(); },
                "make_receiver didn't throw when called twice");

            TEST_EQ(ch.senders_count(), 2, "senders_count()", "2");
            auto s3 = s2;
            TEST_EQ(ch.senders_count(), 3, "senders_count()", "3");
            s3.close();
            s.close();
            TEST_EQ(ch.senders_count(), 1, "senders_count()", "1");
            TEST_EQ_MSG(ch.is_send_closed(), false,
                        "send half must not be closed");
            s2.close();
            TEST_EQ(ch.senders_count(), 0, "senders_count()", "0");
            TEST_EQ_MSG(ch.is_send_closed(), true, "send half must be closed");
            assert_throws<std::system_error>(
                [&] { ch.make_sender(); },
                "make_sender didn't throw after all senders were "
                "closed");
        }

        // send, receive, close send, receive
        {
            channel_t ch{ex};
            auto [s, r] = assert_returns([&] { return ch.split(); },
                                         "split must not throw");
            std::string value = "some message in channel";
            assert_returns([&, &s = s] { s.send(value); },
                           "send must not throw");
            std::string msg = co_await assert_returns(
                [&r = r]() -> task<std::string> {
                    co_return co_await r.receive();
                },
                "receive must not throw");
            TEST_EQ(value, msg, "the value sent", " the value received");
            s.close();
            co_await assert_throws<std::system_error>(
                [&r = r]() -> task<> { co_await r.receive(); },
                "receive didn't throw after all senders were "
                "closed");
        }

        // send, send, receive, close send, close send, receive, receive
        {
            channel_t ch{ex};
            auto [s, r] = assert_returns([&] { return ch.split(); },
                                         "split must not throw");
            auto s2 = s;
            std::string value1 = "some message in channel1";
            std::string value2 = "some message in channel2";
            assert_returns([&, &s = s] { s.send(value1); },
                           "send must not throw");
            assert_returns([&] { s2.send(value2); }, "send must not throw");
            std::string msg1 = co_await assert_returns(
                [&r = r]() -> task<std::string> {
                    co_return co_await r.receive();
                },
                "receive must not throw");
            TEST_EQ(value1, msg1, "the value sent", " the value received");
            s.close();
            s2.close();
            TEST_EQ_MSG(ch.is_send_closed(), true, "send half must be closed");
            std::string msg2 = co_await assert_returns(
                [&r = r]() -> task<std::string> {
                    co_return co_await r.receive();
                },
                "receive must not throw");
            TEST_EQ(value2, msg2, "the value sent", " the value received");
            co_await assert_throws<std::system_error>(
                [&r = r]() -> task<> { co_await r.receive(); },
                "receive didn't throw after all senders were "
                "closed");
        }

        // close receive, send
        {
            channel_t ch{ex};
            auto [s, r] = TRY_RETURN(ch.split(), "split must not throw");
            r.close();
            assert_throws<std::system_error>(
                [&s = s] { s.send(""); },
                "send didn't throw after receiver was closed");
        }
    }

    task<> run_tests(any_executor& ex) {
        co_await test_oneshot(ex);
        co_await test_mpsc(ex);
    }
} // namespace

namespace tests_fn {
    bool do_channels_tests() {
        try {
            io_loop loop{1};
            std::exception_ptr ex_ptr;
            spawn(loop, run_tests(loop),
                  [&](std::exception_ptr p) { ex_ptr = p; });
            loop.run();
            if (!ex_ptr) {
                std::cout << "[*] channels tests passed\n";
                return true;
            }
            std::rethrow_exception(ex_ptr);
        }
        catch (const exception& ex) {
            std::cout << "[!] oneshot channel test failed ! " << ex.detailed()
                      << "\n";
        }
        catch (const std::exception& ex) {
            std::cout << "[!] oneshot channel test failed ! " << ex.what()
                      << "\n";
        }
        return false;
    }
} // namespace tests_fn