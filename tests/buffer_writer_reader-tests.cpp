#include <rad/buffer_reader.h>
#include <rad/buffer_writer.h>

#include <iostream>
#include <random>

#include "tests.h"

using namespace LIB_NAMESPACE;

void test_buffer_writer() {
    std::random_device rd;
    std::default_random_engine rng(rd());
    std::uniform_int_distribution<uint32_t> chars_gen(32, 126);

    std::string str;
    str.resize(70 * 1024);
    for (auto& ch : str) {
        ch = static_cast<char>(chars_gen(rng));
    }

    std::vector<uint8_t> storage;

    auto test_write_int_float = [&]<class T>(T i) {
        storage.clear();
        storage.resize(sizeof(T));

        buffer_writer(buffer(storage)) << i;
        const uint8_t* i_p = reinterpret_cast<const uint8_t*>(&i);
        if (!std::equal(storage.begin(), storage.end(), i_p)) {
            throw std::runtime_error("The number wasn't streamed correctly");
        }
        if constexpr (std::is_integral_v<T>) {
            buffer_writer(buffer(storage)).write_integer(i);
        }
        else {
            buffer_writer(buffer(storage)).write_floating_point(i);
        }
        if (!std::equal(storage.begin(), storage.end(), i_p)) {
            throw std::runtime_error("The number wasn't written correctly");
        }
    };

    auto test_write_string = [&](std::string_view str) {
        storage.clear();
        storage.resize(str.size());

        buffer_writer(buffer(storage)) << str;
        if (!std::equal(storage.begin(), storage.end(), str.begin())) {
            throw std::runtime_error("The storage content doesn't match string "
                                     "content streamed into the writer");
        }
        buffer_writer(buffer(storage)).write_string(str);
        if (!std::equal(storage.begin(), storage.end(), str.begin())) {
            throw std::runtime_error("The storage content doesn't match string "
                                     "content written into the writer");
        }
    };

    auto test_write_string_with_size = [&]<class I>(I, std::string_view str) {
        I size = static_cast<I>(str.size());
        storage.resize(size + sizeof(I));

        buffer_writer(buffer(storage)) << with_size<I>(str);

        if (*reinterpret_cast<const I*>(storage.data()) != size) {
            throw std::runtime_error(
                "The string size wasn't streamed correctly into "
                "the writer");
        }
        if (!std::equal(storage.begin() + sizeof(I), storage.end(),
                        str.begin())) {
            throw std::runtime_error("The storage content doesn't match string "
                                     "content streamed into the writer");
        }

        buffer_writer(buffer(storage)).write_string_with_size<I>(str);

        if (*reinterpret_cast<const I*>(storage.data()) != size) {
            throw std::runtime_error(
                "The string size wasn't written correctly into the "
                "writer");
        }
        if (!std::equal(storage.begin() + sizeof(I), storage.end(),
                        str.begin())) {
            throw std::runtime_error("The storage content doesn't match string "
                                     "content written into the writer");
        }
    };

    auto test_write_buffer = [&](const_buffer buff) {
        auto buff_span = buff.to_span<uint8_t>();

        storage.clear();
        storage.resize(buff.size());

        buffer_writer(buffer(storage)) << buff;
        if (!std::equal(storage.begin(), storage.end(), buff_span.begin())) {
            throw std::runtime_error("The storage content doesn't match buffer "
                                     "content streamed into the writer");
        }

        buffer_writer(buffer(storage)).write_buffer(buff);
        if (!std::equal(storage.begin(), storage.end(), buff_span.begin())) {
            throw std::runtime_error("The storage content doesn't match buffer "
                                     "content written into the writer");
        }
    };

    auto test_write_buffer_with_size = [&]<class I>(I, const_buffer buff) {
        auto buff_span = buff.to_span<uint8_t>();

        I size = static_cast<I>(buff.size());
        storage.resize(size + sizeof(I));

        buffer_writer(buffer(storage)) << with_size<I>(buff);

        if (*reinterpret_cast<const I*>(storage.data()) != size) {
            throw std::runtime_error(
                "The buffer size wasn't streamed correctly into "
                "the writer");
        }
        if (!std::equal(storage.begin() + sizeof(I), storage.end(),
                        buff_span.begin())) {
            throw std::runtime_error("The storage content doesn't match buffer "
                                     "content streamed into the writer");
        }

        buffer_writer(buffer(storage)).write_buffer_with_size<I>(buff);

        if (*reinterpret_cast<const I*>(storage.data()) != size) {
            throw std::runtime_error(
                "The buffer size wasn't written correctly into the "
                "writer");
        }
        if (!std::equal(storage.begin() + sizeof(I), storage.end(),
                        buff_span.begin())) {
            throw std::runtime_error("The storage content doesn't match buffer "
                                     "content written into the writer");
        }
    };

    test_write_int_float(uint8_t{77});
    test_write_int_float(uint16_t{123});
    test_write_int_float(uint32_t{523543});
    test_write_int_float(uint64_t{651465312});
    test_write_int_float(float{3512.24});
    test_write_int_float(double{61651.578754});

    test_write_string("some str");
    test_write_string("another string to write");
    test_write_string("write this string");

    test_write_string_with_size(uint8_t{}, str);
    test_write_string_with_size(uint16_t{}, str);
    test_write_string_with_size(uint32_t{}, str);
    test_write_string_with_size(uint64_t{}, str);

    test_write_buffer(buffer(str));

    test_write_buffer_with_size(uint8_t{}, buffer(str));
    test_write_buffer_with_size(uint16_t{}, buffer(str));
    test_write_buffer_with_size(uint32_t{}, buffer(str));
    test_write_buffer_with_size(uint64_t{}, buffer(str));
}

bool do_buffer_reader_writer_tests() {
    try {
        test_buffer_writer();
    }
    catch (const std::exception& ex) {
        std::cout << "[!] buffer writer reader tests failed ! " << ex.what()
                  << "\n";
        return false;
    }

    std::cout << "[*] buffer writer reader tests passed\n";
    return true;
}