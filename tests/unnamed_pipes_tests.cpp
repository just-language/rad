#include <rad/async/io_loop.h>
#include <rad/async/strand.h>
#include <rad/io/unnamed_pipe.h>

#include <chrono>
#include <cstdio>

using namespace RAD_LIB_NAMESPACE;

const int iterations = 1000;
const int num_pipes = 100;

const std::string_view test_buff_text =
    "This is a test buffer text to write into and read from pipes !";

class pipes_reader_writer {
public:
    pipes_reader_writer(io_executor& ex) : ex{ex} {
        std::tie(read_end_1, write_end_1) = io::unnamed_pipe::create_pair(ex);
        std::tie(read_end_2, write_end_2) = io::unnamed_pipe::create_pair(ex);
        std::tie(read_end_3, write_end_3) = io::unnamed_pipe::create_pair(ex);
    }

    void start() {
        do_read(count, read_end_1, write_end_2, buffer(data_1));
        do_read(count, read_end_2, write_end_3, buffer(data_2));
        do_read(count, read_end_3, write_end_1, buffer(data_3));
        write_end_1.write(buffer(test_buff_text));
    }

    void validate() {
        if (count <= iterations) {
            throw std::runtime_error{"didn't run until end !"};
        }
    }

private:
    void do_read(int& count, io::unnamed_pipe& from, io::unnamed_pipe& to,
                 mutable_buffer data) {
        from.async_read_some(buffer(data), [this, &count, &from, &to,
                                            data](auto ec, std::size_t n) {
            if (!ec) {
                auto cloned_data = data;
                auto read_text =
                    cloned_data.sub_buffer(0, n).to_string_view<char>();
                if (read_text != test_buff_text) {
                    throw std::runtime_error{"Read buffer is not "
                                             "the same as "
                                             "written ! " +
                                             read_text};
                }
                do_write(count, from, to, data, n);
            }
        });
    }

    void do_write(int& count, io::unnamed_pipe& from, io::unnamed_pipe& to,
                  mutable_buffer data, std::size_t n) {
        if (++count > iterations) {
            read_end_1.close();
            write_end_1.close();
            read_end_2.close();
            write_end_2.close();
            read_end_3.close();
            write_end_3.close();
            return;
        }

        to.async_write(data.sub_buffer(0, n),
                       [this, &count, &from, &to, data](auto ec, auto) {
                           if (!ec) {
                               do_read(count, from, to, data);
                           }
                       });
    }

    io_executor& ex;
    int count = 0;
    io::unnamed_pipe read_end_1{ex};
    io::unnamed_pipe write_end_1{ex};
    io::unnamed_pipe read_end_2{ex};
    io::unnamed_pipe write_end_2{ex};
    io::unnamed_pipe read_end_3{ex};
    io::unnamed_pipe write_end_3{ex};
    char data_1[1024];
    char data_2[1024];
    char data_3[1024];
};

static void start_pipes_readers_writers_io_loop(io_loop& ex) {
    std::vector<std::unique_ptr<pipes_reader_writer>> pipes;
    pipes.reserve(num_pipes);
    for (auto i : range(num_pipes)) {
        std::ignore = i;
        auto& p = pipes.emplace_back(std::make_unique<pipes_reader_writer>(ex));
        p->start();
    }
    ex.run();
    for (auto& p : pipes) {
        p->validate();
    }
}

static void start_pipes_readers_writers_strand(io_loop& ex) {
    struct strand_with_pipe {
        strand<io_loop> s;
        pipes_reader_writer pipes{s};

        strand_with_pipe(io_loop& loop) : s{loop} {
        }

        void start() {
            pipes.start();
        }

        void validate() {
            pipes.validate();
        }
    };

    std::vector<std::unique_ptr<strand_with_pipe>> pipes;
    pipes.reserve(num_pipes);
    for (auto i : range(num_pipes)) {
        std::ignore = i;
        auto& p = pipes.emplace_back(std::make_unique<strand_with_pipe>(ex));
        p->start();
    }

    unsigned int thds_num = thread::hardware_concurrency();

    {
        std::vector<scoped_thread> loop_threads;
        loop_threads.reserve(thds_num);
        for (auto i : range(thds_num)) {
            std::ignore = i;
            loop_threads.emplace_back([&ex] { ex.run(); });
        }
        ex.run();
    }
    for (auto& p : pipes) {
        p->validate();
    }
}

namespace tests_fn {
    bool do_unnamed_pipes_test() {
        using namespace std::chrono;
        try {
            io_loop loop;
            auto now1 = steady_clock::now();
            start_pipes_readers_writers_io_loop(loop);
            auto now2 = steady_clock::now();
            auto io_loop_elapsed = duration_cast<milliseconds>(now2 - now1);

            loop.restart();
            now1 = steady_clock::now();
            start_pipes_readers_writers_strand(loop);
            now2 = steady_clock::now();
            auto strand_elapsed = duration_cast<milliseconds>(now2 - now1);

            printf("[*] unnamed pipes test passed in %zums (io_loop), "
                   "%zums (strand)\n",
                   (size_t)io_loop_elapsed.count(),
                   (size_t)strand_elapsed.count());
        }
        catch (const std::exception& ex) {
            printf("[!] unnamed pipes test failed ! %s\n", ex.what());
            return false;
        }
        return true;
    }
} // namespace tests_fn