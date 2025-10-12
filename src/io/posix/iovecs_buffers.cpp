#include <rad/io/posix/iovecs_buffers.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;

void iovecs_heap_storage::advance(std::size_t n) noexcept {
    const iovec_buff* last_buff = buffs.data() + buffs.size();
    while (n > 0) {
        assert(current_buff < last_buff);
        std::size_t advance_n = std::min(n, current_buff->iov_len);
        current_buff->advance(advance_n);
        n -= advance_n;
        if (current_buff->iov_len == 0) {
            current_buff += 1;
        }
    }
}

void iovec_buffers::init(const const_buffer* buffs, std::size_t n) {
    if (n == 0) {
    }
    else if (n == 1) {
        if (!buffs->empty()) {
            buffs_ = iovec_buff{*buffs};
        }
    }
    else if (n == 2) {
        if (!buffs[0].empty() && !buffs[1].empty()) {
            auto& array_buffs = buffs_.emplace<iovecs_array_storage<2>>();
            array_buffs.buffs[0] = iovec_buff{buffs[0]};
            array_buffs.buffs[1] = iovec_buff{buffs[1]};
        }
        else if (!buffs[0].empty() && buffs[1].empty()) {
            buffs_ = iovec_buff{*buffs};
        }
        else if (buffs[0].empty() && !buffs[1].empty()) {
            buffs_ = iovec_buff{buffs[1]};
        }
    }
    else {
        std::vector<iovec_buff> heap_buffs;
        heap_buffs.reserve(n);
        for (auto i : range(n)) {
            if (buffs[i].empty()) {
                continue;
            }
            heap_buffs.emplace_back(buffs[i]);
        }
        if (!heap_buffs.empty()) {
            buffs_.emplace<iovecs_heap_storage>(std::move(heap_buffs));
        }
    }
}

void iovec_buffers::advance(std::size_t n) noexcept {
    if (std::holds_alternative<iovec_buff>(buffs_)) {
        auto& buff = std::get<iovec_buff>(buffs_);
        buff.advance(n);
        if (buff.iov_len == 0) {
            buffs_ = std::monostate{};
        }
    }
    else if (std::holds_alternative<iovecs_array_storage<2>>(buffs_)) {
        auto& buffs = std::get<iovecs_array_storage<2>>(buffs_);
        buffs.advance(n);
        if (buffs.is_one()) {
            buffs_ = buffs.get_buffers()[0];
        }
        else if (buffs.is_empty()) {
            buffs_ = std::monostate{};
        }
    }
    else if (std::holds_alternative<iovecs_heap_storage>(buffs_)) {
        auto& buffs = std::get<iovecs_heap_storage>(buffs_);
        buffs.advance(n);
        if (buffs.is_one()) {
            buffs_ = buffs.get_buffers()[0];
        }
        else if (buffs.is_empty()) {
            buffs_ = std::monostate{};
        }
    }
    else {
        assert(false && "advancing empty iovec buffers !");
    }
}

iovec_buff* iovec_buffers::get_buffers() noexcept {
    if (std::holds_alternative<iovec_buff>(buffs_)) {
        return &std::get<iovec_buff>(buffs_);
    }
    else if (std::holds_alternative<iovecs_array_storage<2>>(buffs_)) {
        return std::get<iovecs_array_storage<2>>(buffs_).get_buffers();
    }
    else if (std::holds_alternative<iovecs_heap_storage>(buffs_)) {
        return std::get<iovecs_heap_storage>(buffs_).get_buffers();
    }
    else {
        return nullptr;
    }
}

std::size_t iovec_buffers::get_count() const noexcept {
    if (std::holds_alternative<iovec_buff>(buffs_)) {
        return 1;
    }
    else if (std::holds_alternative<iovecs_array_storage<2>>(buffs_)) {
        return std::get<iovecs_array_storage<2>>(buffs_).get_count();
    }
    else if (std::holds_alternative<iovecs_heap_storage>(buffs_)) {
        return std::get<iovecs_heap_storage>(buffs_).get_count();
    }
    else {
        return 0;
    }
}