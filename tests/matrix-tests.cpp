#include <rad/math/matrix.h>

#include <iostream>
#include <random>

#pragma warning(disable : 4305 4309 4838)

using namespace RAD_LIB_NAMESPACE;

namespace {

    template <class To, class From>
    constexpr To as(From from) noexcept(noexcept(static_cast<To>(from))) {
        return static_cast<To>(from);
    }

    template <class From>
    constexpr uint8_t as_u8(From from) noexcept {
        return as<uint8_t>(from);
    }

    using matrix_type = math::matrix<4, 4>;
    using matrix_storage = matrix_type::storage_type;

    matrix_type generate_random_matrix() {
        std::random_device rd;
        std::default_random_engine rng(rd());
        std::uniform_int_distribution<uint32_t> bytes_gen(0, 255);

        matrix_type m;

        for (auto& v : m.storage()) {
            v = static_cast<uint8_t>(bytes_gen(rng));
        }

        return m;
    }

    void test_copy() {
        auto m1 = generate_random_matrix();
        auto m2 = m1;

        if (m1 != m2) {
            throw std::runtime_error("copy constructor is flawed");
        }
    }

    void test_transpose() {
        matrix_storage storage = {1, 2,  3,  4,  5,  6,  7,  8,
                                  9, 10, 11, 12, 13, 14, 15, 16};

        matrix_storage transposed_storage = {1, 5, 9,  13, 2, 6, 10, 14,
                                             3, 7, 11, 15, 4, 8, 12, 16};

        matrix_type m1 = storage;
        auto m2 = m1.transpose();

        if (m2.storage() != transposed_storage) {
            throw std::runtime_error("transpose result is wrong");
        }
    }

    void test_is_identity_is_symmetric() {
        matrix_storage storage = {1, 2,  3, 4,  5,  1,  7,  8,
                                  9, 10, 1, 12, 13, 14, 15, 1};

        matrix_type m = storage;
        if (!m.is_identity()) {
            throw std::runtime_error("is_identity is flawed");
        }

        matrix_storage storage2 = {1, 2, 3, 4,  2, 7, 9,  8,
                                   3, 9, 5, 12, 4, 8, 12, 14};

        m = storage2;
        if (!m.is_symmetric()) {
            throw std::runtime_error("is_symmetric is flawed");
        }
    }

    void test_diagonal_access() {
        matrix_storage storage = {1, 2,  3, 4,  5,  1,  7,  8,
                                  9, 10, 1, 12, 13, 14, 15, 1};

        matrix_storage storage2 = {1, 2,  3, 4,  5,  2,  7,  8,
                                   9, 10, 3, 12, 13, 14, 15, 4};

        matrix_type m = storage;

        {
            auto diagonal = m.get_diagonal();

            for (auto i : range(m.rows_count)) {
                if (diagonal[i] != 1) {
                    throw std::runtime_error("diagonal view is invalid");
                }
            }
        }

        m = storage2;

        {
            auto diagonal = m.get_diagonal();

            for (auto i : range(m.rows_count)) {
                if (diagonal[i] != i + 1) {
                    throw std::runtime_error("diagonal view is invalid");
                }
            }
        }
    }

    void test_addition() {
        auto m1 = generate_random_matrix();
        auto m2 = generate_random_matrix();
        auto storage1 = m1.storage();
        auto storage2 = m2.storage();

        matrix_storage storage3;
        for (auto i : range(storage1.size())) {
            storage3[i] = storage1[i] + storage2[i];
        }

        if (m1 + m2 != storage3) {
            throw std::runtime_error("addition is flawed");
        }

        storage1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

        storage2 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

        storage3 = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32};

        m1 = storage1;
        m2 = storage2;

        if (m1 + m2 != storage3) {
            throw std::runtime_error("addition is flawed");
        }
    }

    void test_multiplication() {
        matrix_storage storage1 = {1, 2,  3,  4,  5,  6,  7,  8,
                                   9, 10, 11, 12, 13, 14, 15, 16};

        matrix_storage storage2 = {5,  6,  7,  8,  12, 13, 18, 19,
                                   45, 26, 78, 14, 17, 56, 48, 4};

        matrix_storage storage3 = {2,  4,  6,  8,  10, 12, 14, 16,
                                   18, 20, 22, 24, 26, 28, 30, 32};

        // values are truncated to fit into uint8_t
        constexpr matrix_storage storage4 = {
            232,         as_u8(334),  as_u8(469),  104,
            as_u8(548),  as_u8(738),  as_u8(1073), as_u8(284),
            as_u8(864),  as_u8(1142), as_u8(1677), as_u8(464),
            as_u8(1180), as_u8(1546), as_u8(2281), as_u8(644)};

        std::array<matrix_type::value_type, 4 * 2> storage5 = {4,  5, 10, 11,
                                                               15, 8, 22, 17};

        std::array<matrix_type::value_type, 4 * 2> storage6 = {
            157,        119,        as_u8(361), as_u8(283),
            as_u8(565), as_u8(447), as_u8(769), as_u8(611)};

        matrix_type m1 = storage1;

        if (m1 * 2 != storage3) {
            throw std::runtime_error("scalar multiplication is flawed");
        }

        m1 *= 2;
        if (m1 != storage3) {
            throw std::runtime_error("scalar multiplication is flawed");
        }

        m1 = storage1;
        matrix_type m2 = storage2;

        if (m1 * m2 != storage4) {
            throw std::runtime_error("multiplication is flawed");
        }

        math::matrix<4, 2, matrix_storage::value_type> m3 = storage5;
        if (m1 * m3 != storage6) {
            throw std::runtime_error("multiplication is flawed");
        }
    }

    void test_invertible_singular() {
        matrix_storage storage1 = {1, 2,  3,  4,  5,  6,  7,  8,
                                   9, 10, 11, 12, 15, 30, 45, 60};

        matrix_type m = storage1;

        if (!m.has_dependent_rows()) {
            throw std::runtime_error("has_dependent_rows is flawed");
        }

        if (m.has_dependent_columns()) {
            throw std::runtime_error("has_dependent_columns is flawed");
        }

        if (m.is_invertible()) {
            throw std::runtime_error("is_invertible is flawed");
        }

        if (!m.is_singular()) {
            throw std::runtime_error("is_singular is flawed");
        }

        matrix_storage storage2 = {1, 2,  7,  4,  5,  6,  35, 8,
                                   9, 10, 63, 12, 13, 14, 91, 16};

        m = storage2;

        if (m.has_dependent_rows()) {
            throw std::runtime_error("has_dependent_rows is flawed");
        }

        if (!m.has_dependent_columns()) {
            throw std::runtime_error("has_dependent_columns is flawed");
        }

        if (m.is_invertible()) {
            throw std::runtime_error("is_invertible is flawed");
        }

        if (!m.is_singular()) {
            throw std::runtime_error("is_singular is flawed");
        }

        matrix_storage storage3 = {1, 2,  3,  4,  5,  6,  7,  8,
                                   9, 10, 11, 12, 13, 14, 15, 16};

        m = storage3;

        if (m.has_dependent_rows()) {
            throw std::runtime_error("has_dependent_rows is flawed");
        }

        if (m.has_dependent_columns()) {
            throw std::runtime_error("has_dependent_columns is flawed");
        }

        if (!m.is_invertible()) {
            throw std::runtime_error("is_invertible is flawed");
        }

        if (m.is_singular()) {
            throw std::runtime_error("is_singular is flawed");
        }
    }

    void test_assosiativety() {
        matrix_type M = {{1, 2, 50, 4, 12, 23, 53, 21, 12, 23, 53, 21, 1 * 6,
                          2 * 6, 50, 4 * 6}};

        auto A = generate_random_matrix();
        auto B = generate_random_matrix();

        if (!M.is_singular() || M.is_invertible()) {
            throw std::runtime_error("shared matrix is invertible");
        }

        auto PUB1 = A * M;
        auto PUB2 = M * B;

        auto K1 = A * PUB2;
        auto K2 = PUB1 * B;

        if (K1 != K2) {
            throw std::runtime_error("the final results are not the same");
        }
    }

} // namespace

namespace tests_fn {
    bool do_matrix_tests() {
        try {
            test_copy();
            test_transpose();
            test_is_identity_is_symmetric();
            test_diagonal_access();
            test_addition();
            test_invertible_singular();
            test_multiplication();
            test_assosiativety();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] matrix tests failed ! " << ex.what() << " !\n";
            return false;
        }

        std::cout << "[*] matrix tests passed\n";
        return true;
    }
} // namespace tests_fn