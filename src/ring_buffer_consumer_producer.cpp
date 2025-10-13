#include <rad/ring_buffer_consumer.h>

#include <algorithm>

using namespace RAD_LIB_NAMESPACE;

namespace {
    struct rbuf_slice {
        std::uint8_t* start = nullptr;
        std::uint8_t* end = nullptr;

        bool empty() const noexcept {
            return start == end;
        }

        std::size_t size() const noexcept {
            return end - start;
        }
    };

    std::uint8_t* move_overlap(std::uint8_t* first, std::uint8_t* last,
                               std::uint8_t* d_first) noexcept {
        if (first >= last || d_first == first) {
            return d_first;
        }
        std::uint8_t* d_last = d_first + (last - first);
        if (d_first >= last || d_last <= first) {
            // no overlap
            std::memcpy(d_first, first, (last - first));
        }
        else {
            // overlap
            std::memmove(d_first, first, last - first);
        }
        return d_last;
    }
} // namespace

void detail::linearize_ring_buffer_view(std::uint8_t* begin_ptr,
                                        std::uint8_t* end_ptr,
                                        std::uint8_t* start_ptr,
                                        std::uint8_t* back_ptr) noexcept {
    /*
    non linearized:
    [--<     >--]
    [-----<>----]
    */
    assert(begin_ptr < end_ptr);
    assert(start_ptr != end_ptr && back_ptr != end_ptr);
    assert(start_ptr >= back_ptr);
    assert(back_ptr > begin_ptr);

    const std::size_t reserve_size = start_ptr - back_ptr;
    std::span<std::uint8_t> array1{start_ptr, end_ptr};
    std::span<std::uint8_t> array2{begin_ptr, back_ptr};

    if (reserve_size == 0) {
        std::rotate(begin_ptr, start_ptr, end_ptr);
        return;
    }

    struct moving_array_ptr_size {
        std::uint8_t* p;
        std::size_t size;

        std::uint8_t* first() const {
            return p;
        }

        std::uint8_t* last() const {
            return p + size;
        }

        void advance(std::size_t n) {
            p += n;
        }
    };

    moving_array_ptr_size moving_array2{array2.data(), array2.size()};
    std::uint8_t* new_array1 = start_ptr;

    while (!array1.empty()) {
        // shift array 2
        const std::size_t shift_distance =
            std::min(reserve_size, array1.size());
        move_overlap(moving_array2.first(), moving_array2.last(),
                     moving_array2.first() + shift_distance);
        moving_array2.advance(shift_distance);
        // move array1
        new_array1 = std::copy(array1.begin(), array1.begin() + shift_distance,
                               new_array1);
        array1 = array1.subspan(shift_distance);
    }
}

void detail::move_ring_buffer_view_to_start(std::uint8_t* begin_ptr,
                                            std::uint8_t* start_ptr,
                                            std::size_t size) noexcept {
    assert(start_ptr > begin_ptr && size > 0);
    move_overlap(start_ptr, start_ptr + size, begin_ptr);
}