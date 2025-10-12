#pragma once
#include <rad/libbase.h>

#include <array>
#include <cassert>

namespace RAD_LIB_NAMESPACE::math {
    template <std::size_t RowsN, std::size_t ColumnsN, class T = uint8_t>
    struct matrix_types {
        using value_type = T;
        static constexpr std::size_t rows_count = RowsN;
        static constexpr std::size_t columns_count = ColumnsN;
        using row_type = std::array<value_type, columns_count>;
        using column_type = std::array<value_type, rows_count>;
        using rows_storage = std::array<row_type, rows_count>;
        using storage_type = std::array<value_type, rows_count * columns_count>;

    protected:
        bool out_of_range(std::size_t row_index) const noexcept {
            return row_index >= rows_count;
        }

        bool out_of_range(std::size_t row_index,
                          std::size_t column_index) const noexcept {
            return row_index >= rows_count || column_index >= columns_count;
        }

        void assert_not_out_of_range(std::size_t row_index) const noexcept {
            assert(!out_of_range(row_index) && "matrix subscript out of range");
        }

        void assert_not_out_of_range(std::size_t row_index,
                                     std::size_t column_index) const noexcept {
            assert(!out_of_range(row_index, column_index) &&
                   "matrix subscript out of range");
        }

        void throw_if_out_of_range(std::size_t row_index) const {
            if (out_of_range(row_index)) {
                throw std::out_of_range("matrix subscript out of range");
            }
        }

        void throw_if_out_of_range(std::size_t row_index,
                                   std::size_t column_index) const {
            if (out_of_range(row_index, column_index)) {
                throw std::out_of_range("matrix subscript out of range");
            }
        }
    };

    /*
    class matrix_row
    {
            using T = uint8_t;
            static constexpr std::size_t ColumnsN = 4;

    public:

            using value_type = T;
            static constexpr std::size_t columns_count = ColumnsN;
            using row_storage_type = std::array<value_type, columns_count>;

            matrix_row() = default;



    private:

            row_storage_type storage_;
    };
    */

    class matrix_invalid_diagonal {};

    template <std::size_t RowsN, std::size_t ColumnsN, class T, bool IsConst>
    class matrix_diagonal : public matrix_types<RowsN, ColumnsN, T> {
        using base = matrix_types<RowsN, ColumnsN, T>;

    public:
        using typename base::column_type;
        using typename base::row_type;
        using typename base::rows_storage;
        using typename base::storage_type;
        using typename base::value_type;
        static constexpr std::size_t rows_count = base::rows_count;
        static constexpr std::size_t columns_count = base::columns_count;

        matrix_diagonal(rows_storage& rows) noexcept : rows_{rows} {
        }

        matrix_diagonal(const rows_storage& rows) noexcept
            requires(IsConst == true)
            : rows_{rows} {
        }

        const value_type&
        operator[](std::size_t diagonal_index) const noexcept {
            base::assert_not_out_of_range(diagonal_index, diagonal_index);
            return rows_[diagonal_index][diagonal_index];
        }

        value_type& operator[](std::size_t diagonal_index) noexcept
            requires(IsConst == false)
        {
            base::assert_not_out_of_range(diagonal_index, diagonal_index);
            return rows_[diagonal_index][diagonal_index];
        }

        const value_type& at(std::size_t diagonal_index) const noexcept {
            base::throw_if_out_of_range(diagonal_index, diagonal_index);
            return rows_[diagonal_index][diagonal_index];
        }

        value_type& at(std::size_t diagonal_index) noexcept
            requires(IsConst == false)
        {
            base::throw_if_out_of_range(diagonal_index, diagonal_index);
            return rows_[diagonal_index][diagonal_index];
        }

    private:
        std::conditional_t<IsConst, const rows_storage&, rows_storage&> rows_;
    };

    template <std::size_t RowsN, std::size_t ColumnsN, class T, bool IsConst>
    class matrix_transpose_view {};

    template <std::size_t RowsN, std::size_t ColumnsN, class T = uint8_t>
    class matrix : public matrix_types<RowsN, ColumnsN, T> {
        using base = matrix_types<RowsN, ColumnsN, T>;

        template <std::size_t, std::size_t, typename>
        friend class matrix;

    public:
        using typename base::column_type;
        using typename base::row_type;
        using typename base::rows_storage;
        using typename base::storage_type;
        using typename base::value_type;

        static constexpr std::size_t rows_count = base::rows_count;
        static constexpr std::size_t columns_count = base::columns_count;
        static constexpr bool is_square = rows_count == columns_count;

        using diagonal =
            std::conditional_t<is_square,
                               matrix_diagonal<RowsN, ColumnsN, T, false>,
                               matrix_invalid_diagonal>;
        using const_diagonal =
            std::conditional_t<is_square,
                               matrix_diagonal<RowsN, ColumnsN, T, true>,
                               matrix_invalid_diagonal>;

        matrix() = default;

        matrix(const storage_type& matrix) : storage_{matrix} {
        }

        bool operator==(const matrix& other) const noexcept {
            return storage_ == other.storage_;
        }

        const storage_type& storage() const noexcept {
            return storage_;
        }

        storage_type& storage() noexcept {
            return storage_;
        }

        const rows_storage& rows() const noexcept {
            return rows_;
        }

        rows_storage& rows() noexcept {
            return rows_;
        }

        const row_type& operator[](std::size_t row_index) const noexcept {
            base::assert_not_out_of_range(row_index);
            return rows_[row_index];
        }

        row_type& operator[](std::size_t row_index) noexcept {
            base::assert_not_out_of_range(row_index);
            return rows_[row_index];
        }

        value_type& operator()(std::size_t row_index,
                               std::size_t column_index) noexcept {
            base::assert_not_out_of_range(row_index, column_index);
            return rows_[row_index][column_index];
        }

        const value_type& operator()(std::size_t row_index,
                                     std::size_t column_index) const noexcept {
            base::assert_not_out_of_range(row_index, column_index);
            return rows_[row_index][column_index];
        }

        const row_type& at(std::size_t row_index) const {
            base::throw_if_out_of_range(row_index);
            return rows_[row_index];
        }

        row_type& at(std::size_t row_index) {
            base::throw_if_out_of_range(row_index);
            return rows_[row_index];
        }

        value_type& at(std::size_t row_index, std::size_t column_index) {
            base::throw_if_out_of_range(row_index, column_index);
            return rows_[row_index][column_index];
        }

        const value_type& at(std::size_t row_index,
                             std::size_t column_index) const {
            base::throw_if_out_of_range(row_index, column_index);
            return rows_[row_index][column_index];
        }

        diagonal get_diagonal() noexcept
            requires(is_square)
        {
            return diagonal{rows_};
        }

        const_diagonal get_diagonal() const noexcept
            requires(is_square)
        {
            return const_diagonal{rows_};
        }

        matrix& operator+=(const matrix& other) noexcept {
            for (auto i : range(storage_.size())) {
                storage_[i] += other.storage_[i];
            }
        }

        matrix operator+(const matrix& other) const noexcept {
            matrix result;
            for (auto i : range(storage_.size())) {
                result.storage_[i] = storage_[i] + other.storage_[i];
            }
            return result;
        }

        matrix& operator-=(const matrix& other) noexcept {
            for (auto i : range(storage_.size())) {
                storage_[i] -= other.storage_[i];
            }
        }

        matrix operator-(const matrix& other) const noexcept {
            matrix result;
            for (auto i : range(storage_.size())) {
                result.storage_[i] = storage_[i] - other.storage_[i];
            }
            return result;
        }

        matrix& operator*=(value_type scalar) noexcept {
            for (auto& elem : storage_) {
                elem *= scalar;
            }
            return *this;
        }

        matrix operator*(value_type scalar) const noexcept {
            matrix result;
            for (auto i : range(storage_.size())) {
                result.storage_[i] = storage_[i] * scalar;
            }
            return result;
        }

        matrix& operator*=(const matrix& other) noexcept {
            *this = *this * other;
            return *this;
        }

        template <std::size_t CN>
        matrix<RowsN, CN, value_type> operator*(
            const matrix<ColumnsN, CN, value_type>& other) const noexcept {
            constexpr std::size_t m = rows_count;
            constexpr std::size_t n = columns_count;
            constexpr std::size_t p = CN;
            // constexpr std::size_t p =
            // other.columns_count;

            /*
            c00      c01      c02      c0(p-1)
            c10      c11      c12      c1(p-1)
            c20      c21      c21      c2(p-1)
            c(n-1)0  c(m-1)1  c(m-1)2  c(m-1)(p-1)
            */

            matrix<RowsN, CN, value_type> c;
            for (auto& v : c.storage_) {
                v = 0;
            }

            for (auto i : range(m)) {
                for (auto j : range(p)) {
                    for (auto k : range(n)) {
                        c[i][j] += (*this)[i][k] * other[k][j];
                    }
                }
            }

            return c;
        }

        void tanspose(std::in_place_t) noexcept {
        }

        matrix transpose() const noexcept {
            matrix result;

            for (auto i : range(rows_count)) {
                for (auto j : range(columns_count)) {
                    result(i, j) = (*this)(j, i);
                }
            }

            return result;
        }

        bool has_dependent_rows() const noexcept {
            auto are_dependednt_rows = [](const row_type& row1,
                                          const row_type& row2) {
                // row1[0] > row2[0]
                if (row2[0] == 0) {
                    return row1 == row2;
                }

                auto factor = row1[0] / row2[0];
                if (!factor) {
                    return false;
                }

                for (auto i : range(row1.size())) {
                    if (row2[i] * factor != row1[i]) {
                        return false;
                    }
                }

                return true;
            };

            for (auto i : range(rows_.size())) {
                for (auto j : range(rows_.size())) {
                    if (i == j) {
                        continue;
                    }
                    bool result = false;
                    if (rows_[i][0] > rows_[j][0]) {
                        result = are_dependednt_rows(rows_[i], rows_[j]);
                    }
                    else {
                        result = are_dependednt_rows(rows_[j], rows_[i]);
                    }
                    if (result) {
                        return true;
                    }
                }
            }

            return false;
        }

        bool has_dependent_columns() const noexcept {
            return transpose().has_dependent_rows();
        }

        bool is_invertible() const noexcept {
            return is_square && !has_dependent_rows() &&
                   !has_dependent_columns();
        }

        bool is_singular() const noexcept {
            return !is_square || has_dependent_rows() ||
                   has_dependent_columns();
        }

        bool is_symmetric() const noexcept {
            if (!is_square) {
                return false;
            }

            for (auto i : range(rows_count)) {
                for (auto j : range(columns_count)) {
                    if (rows_[i][j] != rows_[j][i]) {
                        return false;
                    }
                }
            }

            return true;
        }

        bool is_identity() const noexcept {
            if (!is_square) {
                return false;
            }

            for (auto i : range(rows_.size())) {
                if (rows_[i][i] != 1) {
                    return false;
                }
            }

            return true;
        }

    private:
        union {
            rows_storage rows_;
            storage_type storage_;
        };
    };
} // namespace RAD_LIB_NAMESPACE::math