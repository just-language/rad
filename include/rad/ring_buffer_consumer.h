#pragma once
#include <limits>
#include <rad/buffer.h>

namespace RAD_LIB_NAMESPACE {
    namespace detail {
        RAD_EXPORT_DECL void linearize_ring_buffer_view(
            std::uint8_t* begin_ptr, std::uint8_t* end_ptr,
            std::uint8_t* start_ptr, std::uint8_t* back_ptr) noexcept;

        RAD_EXPORT_DECL void
        move_ring_buffer_view_to_start(std::uint8_t* begin_ptr,
                                       std::uint8_t* start_ptr,
                                       std::size_t size) noexcept;
    } // namespace detail

    /*!
     * @brief A ring buffer consumer view over a fixed linear buffer used to
     * read incrementally without moving the data in the underlying linear
     * buffer.
     *
     * Since the ring view only holds pointers to the linear buffer the user
     * must ensure the linear buffer remains valid as long as it is used by
     * the ring view.
     */
    class ring_consumer {
        friend class ring_consumer_producer;

        /*
        [ is the begin ptr
        ] is the end ptr
        > is the start ptr
        < is the end ptr
        ( [>, >], <], [< ) the two pointers are pointing to the
        same place

        start_ptr can't be the same as end_ptr for non empty
        buffer because increment_pointer() will wrap the pointer
        once it is equal to or past end_ptr

        the same is true for back_ptr, it can never point to or
        beyond end_ptr

        linearized:
        [>---------<] // forbiden
        [><---------] // allowed full linear
        [   >------<] // forbidden
        [<   >------] // allowed
        [>-----<    ]
        [   >---<   ]

        non linearized:
        [--<     >--]
        [--<       >] // forbidden non linear
        [>--<       ] // allowed but linear
        [-----<>----] // full non linear
        */

    public:
        /*!
         * @brief Construct an empty ring buffer.
         */
        ring_consumer() = default;

        /*!
         * @brief Consutruct a ring consumer that reads from a
         * data buffer which must outlive the ring.
         * @param data_buffer the data buffer to read.
         * @param commit_size Count of bytes to commit from the
         * data. If @p commit_size is greater than size of @p
         * data_buffer it will be truncated. By default, all
         * data is commited.
         */
        ring_consumer(const_buffer data_buffer,
                      std::size_t commit_size =
                          std::numeric_limits<std::size_t>::max()) noexcept
            : capacity_{data_buffer.size()},
              used_size{std::min(data_buffer.size(), commit_size)},
              begin_ptr{data_buffer.data_as<const uint8_t>()},
              start_ptr{begin_ptr}, back_ptr{begin_ptr + used_size} {
        }

        /*!
         * @brief Tell the ring that @p n bytes were consumed
         * from the commited data, to decrease the commited
         * size.
         * @param n Size of consumed data.
         */
        void consume(std::size_t n) noexcept {
            assert(n <= used_size);
            used_size -= n;
            if (!used_size) {
                start_ptr = back_ptr = begin_ptr;
            }
            else {
                increment_pointer(start_ptr, n);
            }
        }

        /*!
         * @brief Tell the ring that @p n bytes were consumed
         * from the commited data, to decrease the commited
         * size.
         * @param n Size of consumed data.
         */
        ring_consumer& operator+=(std::size_t n) noexcept {
            consume(n);
            return *this;
        }

        /*!
         * @brief Consume all the commited data if any.
         */
        void clear() noexcept {
            used_size = 0;
            start_ptr = back_ptr = begin_ptr;
        }

        /*!
         * @brief Get the total size of the underlying buffer.
         * The returned size includes both used and unused
         * buffers.
         * @return The total size of the underlying buffer.
         */
        std::size_t capacity() const noexcept {
            return capacity_;
        }

        /*!
         * @brief Get the size of available commited data that
         * can be read or written.
         * @return The size of available commited data that can
         * be read or written.
         */
        std::size_t size() const noexcept {
            return used_size;
        }

        /*!
         * @brief Get the size of available free buffer space
         * that can be written to.
         * @return The size of available free buffer space that
         * can be written to.
         */
        std::size_t space() const noexcept {
            return capacity() - size();
        }

        /*!
         * @brief Check if there is no commited data (size() ==
         * 0).
         * @return True is there is no commited data, otherwise
         * false.
         */
        bool empty() const noexcept {
            return !size();
        }

        /*!
         * @brief Check if there is no free space buffer.
         * @return True if there is no free space buffer,
         * otherwise false.
         */
        bool full() const noexcept {
            return size() == capacity();
        }

        /*!
         * @brief Check if the ring view is currently linear.
         * A linear ring view has its content contiguous.
         * @return True if the ring view is currently linear,
         * otherwise false.
         */
        bool is_linearized() const noexcept {
            return back_ptr >= start_ptr || back_ptr == begin_ptr;
        }

        /*!
         * @brief Get the available data buffers that can be
         * read or written. Both returned buffers, one of them
         * or none of them can be empty.
         * @return The available data buffer that can be read or
         * written.
         */
        std::array<const_buffer, 2> available_buffers() const noexcept {
            if (is_linearized()) {
                return {buffer(start_ptr, size())};
            }
            else {
                return {buffer(start_ptr, end_ptr() - start_ptr),
                        buffer(begin_ptr, back_ptr - begin_ptr)};
            }
        }

        /*!
         * @brief Get @p n bytes of the available data buffers
         * that can be read or written.
         * @param n The count of bytes to get. If @p n is
         * greater than ring commit size it will be truncated to
         * the ring commit size.
         * @return The available data buffer that can be read or
         * written.
         */
        std::array<const_buffer, 2>
        available_buffers(std::size_t n) const noexcept {
            n = std::min(n, size());
            if (is_linearized()) {
                return {buffer(start_ptr, n)};
            }
            else {
                const size_t first_buff_size =
                    std::min(static_cast<size_t>(end_ptr() - start_ptr), n);
                const_buffer first_buff = buffer(start_ptr, first_buff_size);
                n -= first_buff_size;
                const size_t second_buff_size =
                    std::min(static_cast<size_t>(back_ptr - begin_ptr), n);
                const_buffer second_buff = buffer(begin_ptr, second_buff_size);
                return {first_buff, second_buff};
            }
        }

        /*!
         * @brief Attempt to write data from used data buffers
         * to output buffer @p buff. If the size of available
         * data in self buffer is less than size of @p buff,
         * only the available size is written. If the there is
         * no data in the underlying buffer 0 is returned. The
         * commited data size will not change and the same
         * written data can be retrieved again.
         * @param buff The output buffer.
         * @return Count of written bytes to @p buff.
         */
        std::size_t peek(mutable_buffer buff) const noexcept {
            if (buff.empty() || empty()) {
                return 0;
            }
            auto [buff1, buff2] = available_buffers();
            std::size_t n1 = std::min(buff1.size(), buff.size());
            if (n1 > 0) {
                memcpy(buff.data(), buff1.data(), n1);
            }
            buff += n1;
            if (buff.empty() || buff2.empty()) {
                return n1;
            }
            std::size_t n2 = std::min(buff2.size(), buff.size());
            memcpy(buff.data(), buff2.data(), n2);
            return n1 + n2;
        }

        /*!
         * @brief Attempt to write data from used data buffers
         * to output buffer @p buff. If the size of available
         * data in self buffer is less than size of @p buff,
         * only the available size is written. If the there is
         * no data in the underlying buffer 0 is returned. The
         * commited data size will be decreased by the amount of
         * bytes written.
         * @param buff The output buffer.
         * @return Count of written bytes to @p buff.
         */
        std::size_t write_to(mutable_buffer buff) noexcept {
            std::size_t consumed = peek(buff);
            consume(consumed);
            return consumed;
        }

        /*!
         * @brief Attempt to write data from used data buffers
         * to output buffer @p buff. If the size of available
         * data in self buffer is less than size of @p buff,
         * only the available size is written. If the there is
         * no data in the underlying buffer 0 is returned. The
         * commited data size will be decreased by the amount of
         * bytes written.
         * @param buff The output buffer.
         * @return Count of written bytes to @p buff.
         */
        std::size_t get_data(mutable_buffer buff) noexcept {
            return write_to(buff);
        }

        /*!
         * @brief Attempt to insert @p n bytes of data from used
         * data buffers to output buffer @p buff. If the size of
         * available data in self buffer is less than size of @p
         * buff, only the available size is written. If the
         * there is no data in the underlying buffer 0 is
         * returned. Data is appended to @p buff using its
         * insert() method which may be called at most twice.
         * The commited data size will be decreased by the
         * amount of bytes written.
         * @param buff The output dynamic buffer.
         * @param n Count of bytes to append to @p buff.
         * @return Count of written bytes to @p buff.
         */
        std::size_t
        write_to(dynamic_buffer buff,
                 std::size_t n = std::numeric_limits<std::size_t>::max()) {
            if (empty() || n == 0) {
                return 0;
            }
            std::size_t consumed_n = 0;
            auto [buff1, buff2] = available_buffers(n);
            buff.reserve(buff1.size() + buff2.size());
            if (!buff1.empty()) {
                buff.insert(buff1.data(), buff1.size());
                consumed_n += buff1.size();
            }
            if (!buff2.empty()) {
                buff.insert(buff2.data(), buff2.size());
                consumed_n += buff2.size();
            }
            consume(consumed_n);
            return consumed_n;
        }

        /*!
         * @brief Attempt to insert @p n bytes of data from used
         * data buffers to output buffer @p buff. If the size of
         * available data in self buffer is less than size of @p
         * buff, only the available size is written. If the
         * there is no data in the underlying buffer 0 is
         * returned. Data is appended to @p buff using its
         * insert() method which may be called at most twice.
         * The commited data size will be decreased by the
         * amount of bytes written.
         * @param buff The output dynamic buffer.
         * @param n Count of bytes to append to @p buff.
         * @return Count of written bytes to @p buff.
         */
        std::size_t get_data(dynamic_buffer buff, std::size_t n) {
            return write_to(buff, n);
        }

    private:
        const uint8_t* end_ptr() const noexcept {
            return begin_ptr + capacity_;
        }

        void increment_pointer(const uint8_t*& p, std::size_t n) noexcept {
            p += n;
            const uint8_t* end_p = end_ptr();
            if (p >= end_p) {
                p = begin_ptr + (p - end_p);
            }
        }

        // the total buffer space size
        std::size_t capacity_ = 0;
        // the used (commited, available to consume) size in the
        // buffer space
        std::size_t used_size = 0;
        // the begin of the buffer space
        const uint8_t* begin_ptr = nullptr;
        // the start of available (commited) data in the buffer
        // space, this pointer is derefrencable
        const uint8_t* start_ptr = nullptr;
        // the end of available (commited) data in the buffer
        // space, this pointer is not derefrencable
        const uint8_t* back_ptr = nullptr;
    };

    /*!
     * @brief A ring buffer view over a fixed linear buffer used to read and
     * write data incrementally without moving the data in the underlying
     * linear buffer.
     *
     * Since the ring view only holds pointers to the linear buffer the user
     * must ensure the linear buffer remains valid as long as it is used by
     * the ring view.
     */
    class ring_consumer_producer : public ring_consumer {
    public:
        /*!
         * @brief Construct an empty ring buffer.
         */
        ring_consumer_producer() = default;

        /*!
         * @brief Consutruct a ring consumer producer that works
         * on a space buffer which must outlive the ring
         * @param space_buffer the space buffer to manage.
         * @param commit_size Count of bytes to commit from the
         * data. If @p commit_size is greater than size of @p
         * data_buffer it will be truncated. By default, no data
         * is commited.
         */
        ring_consumer_producer(mutable_buffer space_buffer,
                               std::size_t commit_size = 0) noexcept
            : ring_consumer(space_buffer, commit_size) {
            assert(commit_size <= space_buffer.size());
        }

        /*!
         * @brief Tell the ring that @p n bytes were written
         * into the free space, to increase the commited data
         * size.
         * @param n Size of commited data.
         */
        void commit(std::size_t n) noexcept {
            assert(n + used_size <= capacity_);
            increment_pointer(back_ptr, n);
            used_size += n;
        }

        /*!
         * @brief Tell the ring that @p n bytes were consumed
         * from the commited data, to decrease the commited
         * size.
         * @param n Size of consumed data.
         */
        ring_consumer_producer& operator+=(std::size_t n) noexcept {
            consume(n);
            return *this;
        }

        /*!
         * @brief Get the available free space buffers that can
         * be read into or written to. Both returned buffers,
         * one of them or none of them can be empty.
         * @return The available free space buffers that can be
         * read into or written to.
         */
        std::array<mutable_buffer, 2> available_space() noexcept {
            std::uint8_t* mut_begin_ptr = const_cast<std::uint8_t*>(begin_ptr);
            std::uint8_t* mut_back_ptr = const_cast<std::uint8_t*>(back_ptr);
            if (is_linearized()) {
                return {buffer(mut_back_ptr, end_ptr() - back_ptr),
                        buffer(mut_begin_ptr, start_ptr - begin_ptr)};
            }
            else {
                return {buffer(mut_back_ptr, space())};
            }
        }

        /*!
         * @brief Get @p n bytes of the available free space
         * buffers that can be read into or written to. Both
         * returned buffers, one of them or none of them can be
         * empty.
         * @param n The count of bytes to get. If @p n is
         * greater than ring space size it will be truncated to
         * the ring space size.
         * @return The available free space buffers that can be
         * read into or written to.
         */
        std::array<mutable_buffer, 2> available_space(std::size_t n) noexcept {
            auto space_buffs = available_space();
            if (n < space_buffs[0].size()) {
                space_buffs[0] = space_buffs[0].sub_buffer(0, n);
                space_buffs[1] = buffer(nullptr);
                return space_buffs;
            }
            n -= space_buffs[0].size();
            if (n < space_buffs[1].size()) {
                space_buffs[1] = space_buffs[1].sub_buffer(0, n);
            }
            return space_buffs;
        }

        /*!
         * @brief Write @p buff into unused space. Partial
         * writes are allowed. The commited data size will be
         * increased by the amount of bytes written.
         * @param buff The buffer to write
         * @return The number of bytes written wich may be less
         * than buff.size() and will be 0 if no available space.
         */
        std::size_t read_from(const_buffer buff) noexcept {
            if (buff.empty() || full()) {
                return 0;
            }

            auto [buff1, buff2] = available_space();

            std::size_t n1 = min(buff1.size(), buff.size());
            if (n1) {
                memcpy(buff1.data(), buff.data(), n1);
            }
            buff += n1;

            if (buff.empty() || buff2.empty()) {
                commit(n1);
                return n1;
            }

            std::size_t n2 = min(buff2.size(), buff.size());
            memcpy(buff2.data(), buff.data(), n2);
            commit(n1 + n2);
            return n1 + n2;
        }

        /*!
         * @brief Write @p buff into unused space. Partial
         * writes are allowed. The commited data size will be
         * increased by the amount of bytes written.
         * @param buff The buffer to write
         * @return The number of bytes written wich may be less
         * than buff.size() and will be 0 if no available space.
         */
        std::size_t put_data(const_buffer buff) noexcept {
            return read_from(buff);
        }

        /*!
         * @brief Linearize the underlying buffer to make it
         * possible to pass to functions expecting contiguous
         * buffer. If the buffer is currently linear or empty,
         * this operation is a no op.
         * @return The contiguous buffer view which is the first
         * buffer of the ring after linearization.
         */
        mutable_buffer linearize() noexcept {
            std::uint8_t* mut_begin_ptr = const_cast<std::uint8_t*>(begin_ptr);
            std::uint8_t* mut_start_ptr = const_cast<std::uint8_t*>(start_ptr);
            std::uint8_t* mut_back_ptr = const_cast<std::uint8_t*>(back_ptr);
            std::uint8_t* mut_end_ptr = const_cast<std::uint8_t*>(end_ptr());
            if (is_linearized()) {
                return buffer(mut_start_ptr, size());
            }
            assert(!empty());
            if (empty()) {
                return buffer(mut_start_ptr, size());
            }
            detail::linearize_ring_buffer_view(mut_begin_ptr, mut_end_ptr,
                                               mut_start_ptr, mut_back_ptr);
            start_ptr = begin_ptr;
            back_ptr = begin_ptr;
            increment_pointer(back_ptr, size());
            return buffer(mut_begin_ptr, size());
        }

        /*!
         * @brief Move the first buffer to the start of the internal linear
         * buffer. The ring buffer must be linear prior to this call. If the
         * begin pointer is currently pointing to the start of the buffer, then
         * this operation is a no op.
         * @return The contiguous buffer view which is the first buffer of the
         * ring starting at the start of the linear buffer.
         */
        mutable_buffer move_to_start() noexcept {
            std::uint8_t* mut_begin_ptr = const_cast<std::uint8_t*>(begin_ptr);
            std::uint8_t* mut_start_ptr = const_cast<std::uint8_t*>(start_ptr);
            assert(is_linearized());
            if (empty() || start_ptr == begin_ptr) {
                return buffer(mut_start_ptr, size());
            }
            detail::move_ring_buffer_view_to_start(mut_begin_ptr, mut_start_ptr,
                                                   size());
            start_ptr = begin_ptr;
            back_ptr = begin_ptr;
            increment_pointer(back_ptr, size());
            return buffer(mut_begin_ptr, size());
        }
    };
} // namespace RAD_LIB_NAMESPACE