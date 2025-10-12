#pragma once
#include <rad/buffer.h>

#include <array>
#include <variant>
#include <vector>

namespace RAD_LIB_NAMESPACE::io {
    // this struct has the same layout as iovec
    struct iovec_buff {
        void* iov_base; // this may point to const buffer
        size_t iov_len;

        iovec_buff() = default;

        iovec_buff(const const_buffer& buff) noexcept {
            iov_len = buff.size();
            iov_base = const_cast<void*>(buff.data());
        }

        void advance(size_t n) noexcept {
            assert(iov_len >= n);
            iov_len -= n;
            iov_base = static_cast<uint8_t*>(iov_base) + n;
        }
    };

    template <std::size_t N>
    struct iovecs_array_storage {
        std::array<iovec_buff, N> buffs;
        std::size_t count = N;

        void advance(std::size_t n) noexcept {
            assert(count <= N);
            while (n > 0) {
                assert(count != 0);
                auto& buff = buffs[N - count];
                std::size_t advance_n = std::min(n, buff.iov_len);
                buff.advance(advance_n);
                n -= advance_n;
                if (buff.iov_len == 0) {
                    count -= 1;
                }
            }
        }

        bool is_one() const noexcept {
            return count == 1;
        }

        bool is_empty() const noexcept {
            return count == 0;
        }

        iovec_buff* get_buffers() noexcept {
            return std::data(buffs) + (N - count);
        }

        std::size_t get_count() const noexcept {
            return count;
        }
    };

    class iovecs_heap_storage {
        std::vector<iovec_buff> buffs;
        iovec_buff* current_buff = buffs.data();

    public:
        iovecs_heap_storage(std::vector<iovec_buff>&& buffs) noexcept
            : buffs{std::move(buffs)} {
            assert(!this->buffs.empty());
        }

        void advance(std::size_t n) noexcept;

        bool is_one() const noexcept {
            return (buffs.data() + buffs.size()) - current_buff == 1;
        }

        bool is_empty() const noexcept {
            return (buffs.data() + buffs.size()) == current_buff;
        }

        iovec_buff* get_buffers() noexcept {
            return current_buff;
        }

        std::size_t get_count() const noexcept {
            const iovec_buff* start_ptr = current_buff;
            return std::distance(start_ptr, buffs.data() + buffs.size());
        }
    };

    class iovec_buffers {
    public:
        iovec_buffers() = default;

        iovec_buffers(const const_buffer* buffs, std::size_t n) {
            init(buffs, n);
        }

        iovec_buffers(const mutable_buffer* buffs, std::size_t n) {
            init(reinterpret_cast<const const_buffer*>(buffs), n);
        }

        iovec_buffers(const const_buffer& buff) : iovec_buffers(&buff, 1) {
        }

        iovec_buffers(const mutable_buffer& buff) : iovec_buffers(&buff, 1) {
        }

        template <MultiBuffers Buffers>
        iovec_buffers(const Buffers& buffers) {
            auto [p, n] = extract_buffers<true>(buffers);
            init(p, n);
        }

        bool empty() const noexcept {
            // empty buffers are not stored
            return get_count() == 0;
        }

        RAD_EXPORT_DECL void advance(std::size_t n) noexcept;

        RAD_EXPORT_DECL iovec_buff* get_buffers() noexcept;

        RAD_EXPORT_DECL std::size_t get_count() const noexcept;

    private:
        RAD_EXPORT_DECL void init(const const_buffer* buffs, std::size_t n);

        std::variant<std::monostate, iovec_buff, iovecs_array_storage<2>,
                     iovecs_heap_storage>
            buffs_;
    };
} // namespace RAD_LIB_NAMESPACE::io