#include <rad/libbase.h>

#include <iostream>
#include <numeric>
#include <random>
#include <string>

using namespace RAD_LIB_NAMESPACE;

namespace {
    void test_range() {
        std::random_device rd;
        std::default_random_engine rng(rd());
        std::uniform_int_distribution<int> start_dist(20, 70);
        std::uniform_int_distribution<int> stop_dist(100, 150);

        int start = start_dist(rng);
        int stop = stop_dist(rng);

        {
            std::vector<int> result(stop - start);
            std::iota(result.begin(), result.end(), start);

            std::vector<int> result2;

            for (auto i : range(start, stop)) {
                result2.push_back(i);
            }

            if (result.size() != result2.size() ||
                !std::equal(result.begin(), result.end(), result2.begin())) {
                throw std::runtime_error("range(from, to) is behaving wrong");
            }
        }

        return;

        std::vector<int> result;

        for (int i = start; i < stop; i += 4) {
            result.push_back(i);
        }

        std::vector<int> result2;
        for (auto i : range(start, stop, 4)) {
            result2.push_back(i);
        }

        if (result.size() != result2.size() ||
            !std::equal(result.begin(), result.end(), result2.begin())) {
            std::string result_str = "[";
            for (auto i : result) {
                result_str += std::to_string(i) + ", ";
            }
            result_str.pop_back();
            result_str.pop_back();
            result_str += "] != [";
            for (auto i : result2) {
                result_str += std::to_string(i) + ", ";
            }
            result_str.pop_back();
            result_str.pop_back();

            throw std::runtime_error(
                "range(from, to, step) is behaving wrong ! " + result_str);
        }
    }
} // namespace

namespace tests_fn {
    bool do_test_views() {
        try {
            test_range();
        }
        catch (const std::exception& ex) {
            std::cout << "[*] views tests failed ! " << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] views tests passed\n";
        return true;
    }
} // namespace tests_fn