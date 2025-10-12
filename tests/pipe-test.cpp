#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/coro/forked_task.h>
#include <rad/coro/spawn.h>
#include <rad/coro/task.h>
#include <rad/coro/when_all.h>
#include <rad/ipc/async_pipe.h>
#include <rad/unittest/unittest.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;
using namespace pipe;

namespace {
    struct allocator_types {
        using ops = op_alloc_type;

        using write_buffers_type = const_buffer;
        using read_buffers_type = mutable_buffer;

        static constexpr ops op_type =
            ops::write | ops::read | ops::read_all | ops::accept_pipe;

        static constexpr std::size_t max_handler_size = sizeof(void*) * 2;
    };

    [[maybe_unused]] forked_task compile_tests() {
        io_loop loop;
        strand<io_loop> st{loop};

        auto ahandler = [](const std::error_code&) {};
        auto rwhandler = [](const std::error_code&, std::size_t) {};
        endpoint epoint{"some_pipe"};

        constexpr std::size_t max_alloc_size =
            async_pipe::max_allocator_size<allocator_types>();
        std::array<std::uint8_t, max_alloc_size> alloc_buff;
        static_buffer_allocator<max_alloc_size> alloc{alloc_buff};

        async_pipe pipe{loop};
        async_pipe pipe2{st};

        pipe = std::move(pipe2);

        pipe.open(epoint);
        pipe.connect(epoint);
        pipe.close();

        pipe.accept();
        pipe.write(buffer(nullptr));
        pipe.read_some(buffer(nullptr));
        pipe.read(buffer(nullptr));

        pipe.async_accept(ahandler);
        pipe.async_accept(ahandler, alloc);

        pipe.async_write(buffer(nullptr), rwhandler);
        pipe.async_write(buffer(nullptr), rwhandler, alloc);

        pipe.async_read_some(buffer(nullptr), rwhandler);
        pipe.async_read_some(buffer(nullptr), rwhandler, alloc);

        pipe.async_read(buffer(nullptr), rwhandler);
        pipe.async_read(buffer(nullptr), rwhandler, alloc);

        pipe.flush();

        co_await pipe.async_accept();
        std::size_t n = co_await pipe.async_write(buffer(nullptr));
        n = co_await pipe.async_read_some(buffer(nullptr));
        n = co_await pipe.async_read(buffer(nullptr));
        ((void)n);
    }

    task<> test_pipe(io_executor& io_ex, any_executor& any_ex) {
        async_pipe p{io_ex};
        TEST_EQ_MSG(p.is_open(), false, "pipe is constrcted closed");
        TEST_EQ(&io_ex, &p.executor(), "pipe executor", "provided executor");

        pipe::endpoint epoint;
        epoint.flow_dir(pipe::open_mode::duplex);
        epoint.transfer(pipe::transfer_flags::write_bytes |
                        pipe::transfer_flags::read_bytes);
        constexpr std::string_view pipe_name = "test-pipe-5313";
        epoint.pipe_name(pipe_name);

        TEST_THROW_EX(p.connect(epoint), std::system_error,
                      "connect to non existing pipe didn't throw");
        TRY_RETURN(p.open(epoint), "open must not throw");
        TEST_TRUE(p.is_open(), "pipe is not open after open");
        TEST_EQ_MSG(p.name(), pipe_name, "pipe name is flawed");

        auto pipe_fd = p.native_fd();
        auto p2 = std::move(p);
        TEST_TRUE(p2.is_open(), "moved to pipe is not open");
        TEST_TRUE(!p.is_open(), "moved from pipe is open");
        TEST_EQ_MSG(p2.name(), pipe_name, "move constructor is flawed");
        TEST_TRUE(p.name().empty(), "move constructor is flawed");
        TEST_EQ_MSG(pipe_fd, p2.native_fd(), "move constructor is flawed");

        p = std::move(p2);
        TEST_TRUE(p.is_open(), "moved to pipe is not open");
        TEST_TRUE(!p2.is_open(), "moved from pipe is open");
        TEST_EQ_MSG(p.name(), pipe_name, "move assign is flawed");
        TEST_TRUE(p2.name().empty(), "move assign is flawed");
        TEST_EQ_MSG(pipe_fd, p.native_fd(), "move assign is flawed");

        TRY_RETURN(p2.connect(epoint), "connect must not throw");
        TEST_TRUE(p2.is_open(), "pipe is not open after connect");
        TEST_EQ_MSG(p2.name(), pipe_name,
                    "pipe name is not the same as connected to pipe");

        std::string msg = "some message";
        auto wn = TRY_AWAIT_RETURN(p.async_write(buffer(msg)), std::size_t,
                                   "async_write must not throw");
        TEST_EQ(wn, msg.size(), "written size", "message size");
        std::string rmsg;
        rmsg.resize(msg.size());
        auto rn = TRY_AWAIT_RETURN(p2.async_read(buffer(rmsg)), std::size_t,
                                   "async_read must not throw");
        TEST_EQ(wn, rn, "written size", "read size");
        TEST_EQ(msg, rmsg, "written message", "read message");

        msg = "another message";
        rmsg.resize(msg.size());

        auto do_read = [&p, &rmsg]() -> task<std::size_t> {
            auto rn = TRY_AWAIT_RETURN(p.async_read(buffer(rmsg)), std::size_t,
                                       "async_read must not throw");
            co_return rn;
        };

        auto do_write = [&p2, &msg]() -> task<std::size_t> {
            std::size_t n = msg.size() / 2;
            auto wn =
                TRY_AWAIT_RETURN(p2.async_write(buffer(msg, n)), std::size_t,
                                 "async_write must not throw");
            wn += TRY_AWAIT_RETURN(p2.async_write(buffer(msg) + n), std::size_t,
                                   "async_write must not throw");
            ;
            co_return wn;
        };

        std::tie(rn, wn) = co_await (do_read() && do_write());

        TEST_EQ(wn, msg.size(), "written size", "message size");
        TEST_EQ(wn, rn, "written size", "read size");
        TEST_EQ(msg, rmsg, "written message", "read message");
    }
} // namespace

namespace tests_fn {
    bool do_pipe_tests() {
        int failed_tests = 0;
        auto ex_handler = [&failed_tests](std::string_view test_name,
                                          std::exception_ptr ex_ptr) {
            ++failed_tests;
            try {
                std::rethrow_exception(ex_ptr);
            }
            catch (const unittest::exception& ex) {
                std::cout << "[!] pipe " << test_name << " test failed ! "
                          << ex.detailed() << "\n";
            }
        };

        io_loop ex;
        strand<io_loop> st{ex};
        spawn(
            ex, test_pipe(ex, ex),
            [&] {
                spawn(ex, test_pipe(st, st),
                      std::bind_front(ex_handler, "strand"));
            },
            std::bind_front(ex_handler, "io_loop"));
        ex.run();

        if (!failed_tests) {
            std::cout << "[*] pipe tests passed\n";
        }
        return failed_tests == 0;
    }
} // namespace tests_fn
