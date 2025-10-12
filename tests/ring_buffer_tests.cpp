#include <rad/ring_buffer.h>
#include <rad/unittest/unittest.h>
#include <rad/views/zip.h>

#include <algorithm>
#include <iostream>
#include <random>

using namespace RAD_LIB_NAMESPACE;
using namespace unittest;

namespace {
    struct NonTrivial {
        inline static size_t default_ctors = 0;
        inline static size_t move_ctors = 0;
        inline static size_t copy_ctors = 0;
        inline static size_t move_assign = 0;
        inline static size_t copy_assign = 0;
        inline static size_t destroyed = 0;

        static void zero_counters() noexcept {
            default_ctors = move_ctors = copy_ctors = move_assign =
                copy_assign = destroyed = 0;
        }

        NonTrivial() {
            ++default_ctors;
        }

        ~NonTrivial() {
            ++destroyed;
        }

        NonTrivial(NonTrivial&&) noexcept {
            ++move_ctors;
        }

        NonTrivial(const NonTrivial&) {
            ++copy_ctors;
        }

        NonTrivial& operator=(NonTrivial&&) noexcept {
            ++move_assign;
            return *this;
        }

        NonTrivial& operator=(const NonTrivial&) {
            ++copy_assign;
            return *this;
        }

        bool operator==(const NonTrivial&) const noexcept {
            return true;
        }
    };

    std::vector<int> make_random_vec(size_t n) {
        std::random_device rd;
        std::default_random_engine rng{rd()};
        std::uniform_int_distribution<int> dist;
        std::vector<int> v;
        v.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            v.push_back(dist(rng));
        }
        return v;
    }

    template <class T>
    void non_linearize(std::vector<T>& vec, ring_buffer<T>& rbuf) {
        const size_t shift_size = std::max(size_t{1}, rbuf.size() / 4);
        assert_eq(rbuf.size(), vec.size(), "non_linearize");
        assert_true(rbuf.size() > shift_size && vec.size() > shift_size &&
                        rbuf.full(),
                    "non_linearize");
        for (auto i : range(shift_size)) {
            rbuf.pop_front();
            std::ignore = i;
        }
        for (auto i : range(shift_size)) {
            rbuf.push_back(vec[i]);
        }
        std::rotate(vec.begin(), vec.begin() + shift_size, vec.end());
        assert_false(rbuf.is_linearized(), "non_linearize");
        auto array1 = rbuf.array_one();
        auto array2 = rbuf.array_two();
        assert_eq(array2.size(), shift_size, "non_linearize");
        assert_eq(array1.size(), rbuf.size() - shift_size, "non_linearize");
        assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                    "non_linearize");
    }

    template <class T>
    void non_linearize(std::vector<T>& vec, ring_buffer<T>& rbuf,
                       size_t pop_count, size_t push_count) {
        push_count = std::min(push_count, pop_count);
        size_t full_size = rbuf.size();
        assert_eq(rbuf.size(), vec.size(), "non_linearize");
        assert_true(rbuf.size() > pop_count && vec.size() > pop_count &&
                        rbuf.full(),
                    "non_linearize");
        assert_true(pop_count > 1 && push_count > 1, "non_linearize");

        for (auto i : range(pop_count)) {
            rbuf.pop_front();
            std::ignore = i;
        }
        for (auto i : range(push_count)) {
            rbuf.push_back(vec[i]);
        }

        std::rotate(vec.begin(), vec.begin() + pop_count, vec.end());
        if (push_count < pop_count) {
            size_t diff = pop_count - push_count;
            vec.erase(vec.end() - diff, vec.end());
        }
        assert_false(rbuf.is_linearized(), "non_linearize");
        auto array1 = rbuf.array_one();
        auto array2 = rbuf.array_two();
        assert_eq(array2.size(), push_count, "non_linearize");
        assert_eq(array1.size(), full_size - pop_count, "non_linearize");
        assert_eq(rbuf.size(), vec.size(), "non_linearize");
        assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                    "non_linearize");
    }

    void test_rbuf_ctors() {
        {
            // default ctor
            ring_buffer<int> rbuf;
            assert_eq(rbuf.size(), 0,
                      "Size of default constructed ring buffer is "
                      "not 0 !");
            assert_eq(rbuf.capacity(), 0,
                      "Capacity of default constructed ring buffer "
                      "is not 0 !");
            assert_eq(rbuf.reserve(), 0,
                      "Reserve of default constructed ring buffer "
                      "is not 0 !");
            assert_eq(rbuf.empty(), true,
                      "Default constructed ring buffer is not empty !");
            assert_eq(rbuf.full(), true,
                      "Default constructed ring buffer is invalid !");
            assert_eq(rbuf.is_linearized(), true,
                      "Default constructed ring buffer is invalid !");
        }

        {
            // capacity allocating ctor
            ring_buffer<int> rbuf{1024};
            assert_eq(rbuf.size(), 0,
                      "Size of capacity constructed ring buffer is "
                      "not 0 !");
            assert_eq(rbuf.capacity(), 1024,
                      "Capacity of capacity constructed ring "
                      "buffer is not "
                      "1024 !");
            assert_eq(rbuf.reserve(), 1024,
                      "Reserve of capacity constructed ring buffer "
                      "is not "
                      "1024 !");
            assert_eq(rbuf.empty(), true,
                      "Capacity constructed ring buffer is not empty !");
            assert_eq(rbuf.full(), false,
                      "Capacity constructed ring buffer is invalid !");
            assert_eq(rbuf.is_linearized(), true,
                      "Capacity constructed ring buffer is not "
                      "linearized");
        }

        {
            // capacity allocating ctor and fill with value
            ring_buffer<int> rbuf{1024, 123};
            assert_eq(rbuf.size(), 1024,
                      "Size of capacity constructed ring buffer is "
                      "not 1024 !");
            assert_eq(rbuf.capacity(), 1024,
                      "Capacity of capacity constructed ring "
                      "buffer is not "
                      "1024 !");
            assert_eq(rbuf.reserve(), 0,
                      "Reserve of capacity constructed ring buffer "
                      "is not 0 !");
            assert_eq(rbuf.empty(), false,
                      "Capacity constructed ring buffer is empty !");
            assert_eq(rbuf.full(), true,
                      "Capacity constructed ring buffer is invalid !");
            assert_eq(rbuf.is_linearized(), true,
                      "Capacity constructed ring buffer is not "
                      "linearized");
            std::vector<int> vec{rbuf.size(), 123, std::allocator<int>{}};
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "Capacity constructed ring buffer is invalid !");
        }

        {
            // capacity allocating ctor and size and fill with value
            ring_buffer<int> rbuf{1024, 512, 456};
            assert_eq(rbuf.size(), 512,
                      "Size of capacity constructed ring buffer is "
                      "not 1024 !");
            assert_eq(rbuf.capacity(), 1024,
                      "Capacity of capacity constructed ring "
                      "buffer is not "
                      "1024 !");
            assert_eq(rbuf.reserve(), 512,
                      "Reserve of capacity constructed ring buffer "
                      "is not 0 !");
            assert_eq(rbuf.empty(), false,
                      "Capacity constructed ring buffer is empty !");
            assert_eq(rbuf.full(), false,
                      "Capacity constructed ring buffer is invalid !");
            assert_eq(rbuf.is_linearized(), true,
                      "Capacity constructed ring buffer is not "
                      "linearized");
            std::vector<int> vec{rbuf.size(), 456, std::allocator<int>{}};
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "Capacity constructed ring buffer is invalid !");
        }

        {
            // input iterators ctor
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            assert_eq(rbuf.size(), vec.size(),
                      "Input iterators constructed ring buffer is "
                      "invalid !");
            assert_eq(rbuf.capacity(), vec.size(),
                      "Input iterators constructed ring buffer is "
                      "invalid !");
            assert_eq(rbuf.reserve(), 0,
                      "Input iterators constructed ring buffer is "
                      "invalid !");
            assert_eq(rbuf.empty(), false,
                      "Input iterators constructed ring buffer is "
                      "invalid !");
            assert_eq(rbuf.full(), true,
                      "Input iterators constructed ring buffer is "
                      "invalid !");
            assert_eq(rbuf.is_linearized(), true,
                      "Input iterators constructed ring buffer is "
                      "invalid !");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "Input iterators constructed ring buffer is "
                        "invalid !");
        }
    }

    void test_rbuf_copy_move() {
        {
            // copy ctor
            auto vec = make_random_vec(1024 * 2);
            ring_buffer<int> rbuf1{vec.begin(), vec.end()};
            ring_buffer<int> rbuf2{rbuf1};

            assert_eq(rbuf1.size(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.capacity(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.reserve(), 0,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.empty(), false,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.full(), true,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.is_linearized(), true,
                      "Copy constructed ring buffer is invalid !");
            assert_true(std::equal(rbuf1.begin(), rbuf1.end(), vec.begin()),
                        "Copy constructed ring buffer is invalid !");

            assert_eq(rbuf2.size(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.capacity(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.reserve(), 0,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.empty(), false,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.full(), true,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.is_linearized(), true,
                      "Copy constructed ring buffer is invalid !");
            assert_true(std::equal(rbuf2.begin(), rbuf2.end(), vec.begin()),
                        "Copy constructed ring buffer is invalid !");

            assert_neq(&*rbuf1.begin(), &*rbuf2.begin(),
                       "Copy constructed ring buffer is invalid !");
        }

        {
            // copy ctor non linear
            auto vec = make_random_vec(1024 * 2);
            ring_buffer<int> rbuf1{vec.begin(), vec.end()};
            rbuf1.pop_front();
            rbuf1.push_back(0);
            std::copy(vec.begin(), vec.end(), rbuf1.begin());
            ring_buffer<int> rbuf2{rbuf1};

            assert_eq(rbuf1.size(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.capacity(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.reserve(), 0,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.empty(), false,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.full(), true,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf1.is_linearized(), false,
                      "Copy constructed ring buffer is invalid !");
            assert_true(std::equal(rbuf1.begin(), rbuf1.end(), vec.begin()),
                        "Copy constructed ring buffer is invalid !");

            assert_eq(rbuf2.size(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.capacity(), vec.size(),
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.reserve(), 0,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.empty(), false,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.full(), true,
                      "Copy constructed ring buffer is invalid !");
            assert_eq(rbuf2.is_linearized(), true,
                      "Copy constructed ring buffer is invalid !");

            assert_true(std::equal(rbuf2.begin(), rbuf2.end(), vec.begin()),
                        "Copy constructed ring buffer is invalid !");

            assert_neq(&*rbuf1.begin(), &*rbuf2.begin(),
                       "Copy constructed ring buffer is invalid !");
        }

        {
            // move ctor
            auto vec = make_random_vec(1024 * 2);
            ring_buffer<int> rbuf1{vec.begin(), vec.end()};
            int* rbuf1_data_ptr = &*rbuf1.begin();
            ring_buffer<int> rbuf2{std::move(rbuf1)};

            assert_eq(rbuf1.size(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.capacity(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.reserve(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.empty(), true,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.full(), true,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.is_linearized(), true,
                      "Default constructed ring buffer is invalid !");

            assert_eq(rbuf2.size(), vec.size(),
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.capacity(), vec.size(),
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.reserve(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.empty(), false,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.full(), true,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.is_linearized(), true,
                      "Move constructed ring buffer is invalid !");
            assert_true(std::equal(rbuf2.begin(), rbuf2.end(), vec.begin()),
                        "Move constructed ring buffer is invalid !");

            assert_eq(&*rbuf2.begin(), rbuf1_data_ptr,
                      "Move constructed ring buffer is invalid !");
        }

        {
            // copy assign
            auto vec = make_random_vec(1024 * 2);
            ring_buffer<int> rbuf1{vec.begin(), vec.end()};
            auto vec2 = make_random_vec(1024 * 3);
            ring_buffer<int> rbuf2{vec2.begin(), vec2.end()};
            rbuf2 = rbuf1;

            assert_eq(rbuf1.size(), vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.capacity(), vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.reserve(), 0,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.empty(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.full(), true,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.is_linearized(), true,
                      "Copy assigned ring buffer is invalid !");
            assert_true(std::equal(rbuf1.begin(), rbuf1.end(), vec.begin()),
                        "Copy assigned ring buffer is invalid !");

            assert_eq(rbuf2.size(), vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.capacity(), vec2.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.reserve(), vec2.size() - vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.empty(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.full(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.is_linearized(), true,
                      "Copy assigned ring buffer is invalid !");
            assert_true(std::equal(rbuf2.begin(), rbuf2.end(), vec.begin()),
                        "Copy assigned ring buffer is invalid !");

            assert_neq(&*rbuf1.begin(), &*rbuf2.begin(),
                       "Copy assigned ring buffer is invalid !");
        }

        {
            // copy assign non linear
            auto vec = make_random_vec(1024 * 2);
            ring_buffer<int> rbuf1{vec.begin(), vec.end()};
            rbuf1.pop_front();
            rbuf1.push_back(0);
            std::copy(vec.begin(), vec.end(), rbuf1.begin());
            auto vec2 = make_random_vec(1024 * 3);
            ring_buffer<int> rbuf2{vec2.begin(), vec2.end()};
            rbuf2 = rbuf1;

            assert_eq(rbuf1.size(), vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.capacity(), vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.reserve(), 0,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.empty(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.full(), true,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf1.is_linearized(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_true(std::equal(rbuf1.begin(), rbuf1.end(), vec.begin()),
                        "Copy assigned ring buffer is invalid !");

            assert_eq(rbuf2.size(), vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.capacity(), vec2.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.reserve(), vec2.size() - vec.size(),
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.empty(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.full(), false,
                      "Copy assigned ring buffer is invalid !");
            assert_eq(rbuf2.is_linearized(), true,
                      "Copy assigned ring buffer is invalid !");
            assert_true(std::equal(rbuf2.begin(), rbuf2.end(), vec.begin()),
                        "Copy assigned ring buffer is invalid !");

            assert_neq(&*rbuf1.begin(), &*rbuf2.begin(),
                       "Copy assigned ring buffer is invalid !");
        }

        {
            // move assign
            auto vec = make_random_vec(1024 * 2);
            ring_buffer<int> rbuf1{vec.begin(), vec.end()};
            int* rbuf1_data_ptr = &*rbuf1.begin();
            auto vec2 = make_random_vec(1024 * 3);
            ring_buffer<int> rbuf2{vec2.begin(), vec2.end()};
            rbuf2 = std::move(rbuf1);

            assert_eq(rbuf1.size(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.capacity(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.reserve(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.empty(), true,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.full(), true,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf1.is_linearized(), true,
                      "Default constructed ring buffer is invalid !");

            assert_eq(rbuf2.size(), vec.size(),
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.capacity(), vec.size(),
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.reserve(), 0,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.empty(), false,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.full(), true,
                      "Move constructed ring buffer is invalid !");
            assert_eq(rbuf2.is_linearized(), true,
                      "Move constructed ring buffer is invalid !");
            assert_true(std::equal(rbuf2.begin(), rbuf2.end(), vec.begin()),
                        "Move constructed ring buffer is invalid !");

            assert_eq(&*rbuf2.begin(), rbuf1_data_ptr,
                      "Move constructed ring buffer is invalid !");
        }
    }

    void test_rbuf_emplace_push_back() {
        auto vec = make_random_vec(10);
        ring_buffer<int> rbuf{vec.size()};

        for (size_t i = 0; i < vec.size(); ++i) {
            size_t rbuf_size = rbuf.size();
            rbuf.unchecked_emplace_back(vec[i]);
            assert_eq(rbuf.size(), rbuf_size + 1, "emplace_back_no_overwrite");
            auto sub_vec = std::span{vec}.subspan(0, rbuf.size());
            assert_true(std::equal(rbuf.begin(), rbuf.end(), sub_vec.begin()),
                        "emplace_back_no_overwrite");
        }

        assert_false(rbuf.try_emplace_back(0), "try_emplace_back");
        assert_false(rbuf.try_push_back(0), "try_push_back");

        rbuf.clear();
        assert_eq(rbuf.size(), 0,
                  "Size of default constructed ring buffer is not 0 !");
        assert_eq(rbuf.capacity(), vec.size(),
                  "Capacity of default constructed ring buffer is not 0 !");
        assert_eq(rbuf.reserve(), vec.size(),
                  "Reserve of default constructed ring buffer is not 0 !");
        assert_eq(rbuf.empty(), true,
                  "Default constructed ring buffer is not empty !");
        assert_eq(rbuf.full(), false,
                  "Default constructed ring buffer is invalid !");
        assert_eq(rbuf.is_linearized(), true,
                  "Default constructed ring buffer is invalid !");

        for (size_t i = 0; i < vec.size(); ++i) {
            size_t rbuf_size = rbuf.size();
            rbuf.unchecked_push_back(vec[i]);
            assert_eq(rbuf.size(), rbuf_size + 1, "push_back_without_checks");
            auto sub_vec = std::span{vec}.subspan(0, rbuf.size());
            assert_true(std::equal(rbuf.begin(), rbuf.end(), sub_vec.begin()),
                        "push_back_without_checks");
        }

        assert_false(rbuf.try_emplace_back(0), "try_emplace_back");
        assert_false(rbuf.try_push_back(0), "try_push_back");

        for (size_t i = 0; i < vec.size(); ++i) {
            size_t rbuf_size = rbuf.size();
            rbuf.pop_back();
            assert_eq(rbuf.size(), rbuf_size - 1, "pop_back");
            assert_eq(rbuf.is_linearized(), true,
                      "Default constructed ring buffer is invalid !");
            auto sub_vec = std::span{vec}.subspan(0, rbuf.size());
            assert_true(std::equal(rbuf.begin(), rbuf.end(), sub_vec.begin()),
                        "pop_back");
        }

        assert_false(rbuf.try_pop_back(), "try_pop_back");
        assert_false(rbuf.try_pop_front(), "try_pop_front");

        assert_eq(rbuf.size(), 0,
                  "Size of default constructed ring buffer is not 0 !");
        assert_eq(rbuf.capacity(), vec.size(),
                  "Capacity of default constructed ring buffer is not 0 !");
        assert_eq(rbuf.reserve(), vec.size(),
                  "Reserve of default constructed ring buffer is not 0 !");
        assert_eq(rbuf.empty(), true,
                  "Default constructed ring buffer is not empty !");
        assert_eq(rbuf.full(), false,
                  "Default constructed ring buffer is invalid !");
        assert_eq(rbuf.is_linearized(), true,
                  "Default constructed ring buffer is invalid !");

        for (size_t i = 0; i < vec.size(); ++i) {
            size_t rbuf_size = rbuf.size();
            rbuf.unchecked_emplace_back(vec[i]);
            assert_eq(rbuf.size(), rbuf_size + 1, "emplace_back_no_overwrite");
            auto sub_vec = std::span{vec}.subspan(0, rbuf.size());
            assert_true(std::equal(rbuf.begin(), rbuf.end(), sub_vec.begin()),
                        "emplace_back_no_overwrite");
        }

        for (size_t i = 0; i < vec.size(); ++i) {
            size_t rbuf_size = rbuf.size();
            rbuf.pop_front();
            assert_eq(rbuf.size(), rbuf_size - 1, "pop_front");
            assert_eq(rbuf.is_linearized(), true,
                      "Default constructed ring buffer is invalid !");
            auto sub_vec = std::span{vec}.subspan(i + 1);
            assert_eq(sub_vec.size(), rbuf.size(), "pop_front");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), sub_vec.begin()),
                        "pop_front");
        }
    }

    void test_rbuf_iterators() {
        // index operator
        {
            // linear 1
            auto vec = make_random_vec(10);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            for (size_t i = 0; i < rbuf.size(); ++i) {
                assert_eq(vec[i], rbuf[i], "index operator");
            }
            // linear 2
            rbuf.pop_back();
            for (size_t i = 0; i < rbuf.size(); ++i) {
                assert_eq(vec[i], rbuf[i], "index operator");
            }
            // linear 3
            rbuf.pop_front();
            for (size_t i = 0; i < rbuf.size(); ++i) {
                assert_eq(vec[i + 1], rbuf[i], "index operator");
            }

            // non linear
            rbuf.pop_front();
            rbuf.push_back(vec[vec.size() - 1]);
            rbuf.push_back(10);
            rbuf.push_back(111);
            vec.erase(vec.begin());
            vec.erase(vec.begin());
            vec.push_back(10);
            vec.push_back(111);
            for (size_t i = 0; i < rbuf.size(); ++i) {
                assert_eq(vec[i], rbuf[i], "index operator");
            }
        }

        // iterators
        {
            // linear 1
            auto vec = make_random_vec(10);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};

            for (auto [rit, vit] = std::tuple{rbuf.begin(), vec.begin()};
                 rit != rbuf.end(); ++rit, ++vit) {
                assert_eq(*rit, *vit, "increment iterator");
            }
            for (auto [rit, vit] = std::tuple{rbuf.rbegin(), vec.rbegin()};
                 rit != rbuf.rend(); ++rit, ++vit) {
                assert_eq(*rit, *vit, "decrement iterator");
            }
            for (auto [rit, vit] = std::tuple{rbuf.begin(), vec.begin()};
                 rit != rbuf.end(); rit += 2, vit += 2) {
                assert_eq(*rit, *vit, "increment iterator");
            }
            for (auto [rit, vit] = std::tuple{rbuf.rbegin(), vec.rbegin()};
                 rit.base() != rbuf.begin(); vit += 2, rit += 2) {
                assert_eq(*rit, *vit, "decrement iterator");
            }
        }

        // iterators non linear
        {
            auto data1 = std::array{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            auto data2 = std::array{4, 5, 6, 7, 8, 9, 10, 1, 2, 3};
            ring_buffer<int> rbuf{data1.begin(), data1.end()};
            rbuf.pop_front();
            rbuf.pop_front();
            rbuf.pop_front();
            rbuf.push_back(1);
            rbuf.push_back(2);
            rbuf.push_back(3);
            assert_true(std::equal(rbuf.begin(), rbuf.end(), data2.begin()),
                        "nonlinear iterator");
        }

        // for range
        {
            auto vec = make_random_vec(20);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            for (auto [ri, vi] : zip(rbuf, vec)) {
                assert_eq(ri, vi, "for range");
                ri += 10;
            }
            for (auto [ri, vi] : zip(rbuf, vec)) {
                ri -= 10;
                assert_eq(ri, vi, "for range");
            }
        }
    }

    void test_rbuf_pop_front() {
        // linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_front(vec2.begin(), vec2.end());
            assert_eq(it, vec2.end(), "pop_front range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_front range");
            assert_eq(rbuf.size(), 0, "pop_front range");
            assert_true(rbuf.empty(), "pop_front range");
        }
        // linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_front(vec2.begin(), vec2.begin() + 512);
            assert_eq(it, vec2.begin() + 512, "pop_front range");
            assert_true(
                std::equal(vec2.begin(), vec2.begin() + 512, vec.begin()),
                "pop_front range");
            assert_eq(rbuf.size(), 512, "pop_front range");
            assert_false(rbuf.empty(), "pop_front range");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin() + 512),
                        "pop_front range");
        }
        // linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            std::vector<int> vec2;
            vec2.resize(vec.size() * 2);
            auto it = rbuf.pop_front(vec2.begin(), vec2.end());
            assert_eq(it, vec2.begin() + vec.size(), "pop_front range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_front range");
            assert_eq(rbuf.size(), 0, "pop_front range");
            assert_true(rbuf.empty(), "pop_front range");
        }
        // non linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_front(vec2.begin(), vec2.end());
            assert_eq(it, vec2.end(), "pop_front range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_front range");
            assert_eq(rbuf.size(), 0, "pop_front range");
            assert_true(rbuf.empty(), "pop_front range");
        }
        // non linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_front(vec2.begin(), vec2.begin() + 512);
            assert_eq(it, vec2.begin() + 512, "pop_front range");
            assert_true(
                std::equal(vec2.begin(), vec2.begin() + 512, vec.begin()),
                "pop_front range");
            assert_eq(rbuf.size(), 512, "pop_front range");
            assert_false(rbuf.empty(), "pop_front range");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin() + 512),
                        "pop_front range");
        }
        // non linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            std::vector<int> vec2;
            vec2.resize(vec.size() * 2);
            auto it = rbuf.pop_front(vec2.begin(), vec2.end());
            assert_eq(it, vec2.begin() + vec.size(), "pop_front range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_front range");
            assert_eq(rbuf.size(), 0, "pop_front range");
            assert_true(rbuf.empty(), "pop_front range");
        }
    }

    void test_rbuf_pop_back() {
        // linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_back(vec2.begin(), vec2.end());
            assert_eq(it, vec2.end(), "pop_back range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_back range");
            assert_eq(rbuf.size(), 0, "pop_back range");
            assert_true(rbuf.empty(), "pop_back range");
        }
        // linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_back(vec2.begin(), vec2.begin() + 512);
            assert_eq(it, vec2.begin() + 512, "pop_back range");
            assert_true(
                std::equal(vec2.begin(), vec2.begin() + 512, vec.begin() + 512),
                "pop_back range");
            assert_eq(rbuf.size(), 512, "pop_back range");
            assert_false(rbuf.empty(), "pop_back range");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "pop_back range");
        }
        // linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            std::vector<int> vec2;
            vec2.resize(vec.size() * 2);
            auto it = rbuf.pop_back(vec2.begin(), vec2.end());
            assert_eq(it, vec2.begin() + vec.size(), "pop_back range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_back range");
            assert_eq(rbuf.size(), 0, "pop_back range");
            assert_true(rbuf.empty(), "pop_back range");
        }
        // non linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_back(vec2.begin(), vec2.end());
            assert_eq(it, vec2.end(), "pop_back range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_back range");
            assert_eq(rbuf.size(), 0, "pop_back range");
            assert_true(rbuf.empty(), "pop_back range");
        }
        // non linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            std::vector<int> vec2;
            vec2.resize(vec.size());
            auto it = rbuf.pop_back(vec2.begin(), vec2.begin() + 512);
            assert_eq(it, vec2.begin() + 512, "pop_back range");
            assert_true(
                std::equal(vec2.begin(), vec2.begin() + 512, vec.begin() + 512),
                "pop_back range");
            assert_eq(rbuf.size(), 512, "pop_back range");
            assert_false(rbuf.empty(), "pop_back range");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "pop_back range");
        }
        // non linear
        {
            auto vec = make_random_vec(1024);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            std::vector<int> vec2;
            vec2.resize(vec.size() * 2);
            auto it = rbuf.pop_back(vec2.begin(), vec2.end());
            assert_eq(it, vec2.begin() + vec.size(), "pop_back range");
            assert_true(std::equal(vec.begin(), vec.end(), vec2.begin()),
                        "pop_back range");
            assert_eq(rbuf.size(), 0, "pop_back range");
            assert_true(rbuf.empty(), "pop_back range");
        }
    }

    void test_rbuf_clear_destructor() {
        ring_buffer<NonTrivial> rbuf;

        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.set_capacity(1024);

        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.resize(1024, NonTrivial{});

        assert_eq(NonTrivial::default_ctors, 1, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        NonTrivial::zero_counters();

        for (auto i : range(5)) {
            rbuf.emplace_back();
            std::ignore = i;
        }

        assert_eq(NonTrivial::default_ctors, 5, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 5, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        NonTrivial::zero_counters();

        rbuf.clear();
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        NonTrivial::zero_counters();

        for (auto i : range(5)) {
            rbuf.emplace_back();
            std::ignore = i;
        }

        assert_eq(NonTrivial::default_ctors, 5, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        NonTrivial::zero_counters();

        for (auto i : range(3)) {
            rbuf.pop_front();
            std::ignore = i;
        }
        for (auto i : range(2)) {
            rbuf.pop_back();
            std::ignore = i;
        }

        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 5, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.resize(1024, NonTrivial{});
        std::vector<NonTrivial> vec;
        vec.resize(rbuf.size());
        NonTrivial::zero_counters();

        rbuf.pop_front(vec.begin(), vec.end());
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.resize(1024, NonTrivial{});
        NonTrivial::zero_counters();

        rbuf.pop_back(vec.begin(), vec.end());
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.resize(1024, NonTrivial{});
        non_linearize(vec, rbuf);
        NonTrivial::zero_counters();

        rbuf.pop_front(vec.begin(), vec.end());
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.resize(1024, NonTrivial{});
        non_linearize(vec, rbuf);
        NonTrivial::zero_counters();

        rbuf.pop_back(vec.begin(), vec.end());
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.resize(1024, NonTrivial{});
        NonTrivial::zero_counters();

        {
            auto rbuf2 = std::move(rbuf);
        }
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        NonTrivial::zero_counters();

        rbuf = ring_buffer<NonTrivial>{vec.begin(), vec.end()};

        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");

        rbuf.clear();
        NonTrivial::zero_counters();

        rbuf = ring_buffer<NonTrivial>{std::make_move_iterator(vec.begin()),
                                       std::make_move_iterator(vec.end())};
        assert_eq(NonTrivial::default_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::destroyed, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_ctors, 1024, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_ctors, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::move_assign, 0, "test_rbuf_clear_destructor");
        assert_eq(NonTrivial::copy_assign, 0, "test_rbuf_clear_destructor");
    }

    void test_rbuf_linearize() {
        // empty
        {
            ring_buffer<int> rbuf{1024};
            assert_eq(rbuf.linearize(), nullptr, "test_rbuf_linearize");
        }
        // linear
        {
            ring_buffer<int> rbuf{1024, 1024, 123};
            auto array1 = rbuf.array_one();
            assert_eq(rbuf.linearize(), array1.data(), "test_rbuf_linearize");
        }
        // linear
        {
            ring_buffer<int> rbuf{1024, 512, 123};
            auto array1 = rbuf.array_one();
            assert_eq(rbuf.linearize(), array1.data(), "test_rbuf_linearize");
        }
        // linear
        {
            ring_buffer<int> rbuf{1024, 1024, 123};
            for (auto i : range(100)) {
                rbuf.pop_front();
                std::ignore = i;
            }
            auto array1 = rbuf.array_one();
            assert_eq(rbuf.linearize(), array1.data(), "test_rbuf_linearize");
        }
        // linear
        {
            ring_buffer<int> rbuf(1024, 1024, 123);
            for (auto i : range(100)) {
                rbuf.pop_front();
                std::ignore = i;
            }
            for (auto i : range(100)) {
                rbuf.pop_back();
                std::ignore = i;
            }
            auto array1 = rbuf.array_one();
            assert_eq(rbuf.linearize(), array1.data(), "test_rbuf_linearize");
        }
        // non linear not full

        {
            auto vec = make_random_vec(1027);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf, 500, 300);
            auto* p = rbuf.linearize();
            assert_eq(rbuf.size(), vec.size(), "test_rbuf_linearize");
            assert_true(rbuf.is_linearized(), "test_rbuf_linearize");
            auto array1 = rbuf.array_one();
            assert_eq(p, array1.data(), "test_rbuf_linearize");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "test_rbuf_linearize");
            assert_true(std::equal(array1.begin(), array1.end(), vec.begin()),
                        "test_rbuf_linearize");
        }

        // non linear full
        {
            auto vec = make_random_vec(1027);
            ring_buffer<int> rbuf{vec.begin(), vec.end()};
            non_linearize(vec, rbuf);
            auto* p = rbuf.linearize();
            assert_eq(rbuf.size(), vec.size(), "test_rbuf_linearize");
            assert_true(rbuf.is_linearized(), "test_rbuf_linearize");
            auto array1 = rbuf.array_one();
            assert_eq(p, array1.data(), "test_rbuf_linearize");
            assert_true(std::equal(rbuf.begin(), rbuf.end(), vec.begin()),
                        "test_rbuf_linearize");
            assert_true(std::equal(array1.begin(), array1.end(), vec.begin()),
                        "test_rbuf_linearize");
        }
    }
} // namespace

namespace tests_fn {
    bool do_ring_buffer_tests() {
        try {
            test_rbuf_ctors();
            test_rbuf_copy_move();
            test_rbuf_emplace_push_back();
            test_rbuf_iterators();
            test_rbuf_pop_front();
            test_rbuf_pop_back();
            test_rbuf_clear_destructor();
            test_rbuf_linearize();
        }
        catch (const exception& ex) {
            std::cerr << "[!] ring buffer tests failed ! " << ex.detailed()
                      << '\n';
            return false;
        }
        std::cout << "[*] ring buffer tests passed\n";
        return true;
    }
} // namespace tests_fn
