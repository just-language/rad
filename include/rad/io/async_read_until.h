#include <rad/buffer.h>
#include <rad/coro/task.h>

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief Start an asynchronous operation to read data into a dynamic
     * buffer until it contains a specified delimiter.
     *
     * @tparam AsyncReadStream type of async stream to read from
     * @tparam DynamicBuffer type of buffer to append data to
     * @param s async stream to read from
     * @param buff buffer to append data to
     * @param end_mark the mark bytes to stop reading after it is found in
     * the read data
     * @param step the amount of bytes to attempt read each iteration
     * @param max_size the max size which @p buff is not allowed to be
     * resized beyond
     * @return Number of bytes read up to and including @p end_mark. The
     * buffer may contain data after @p end_mark so to determine the size of
     * all read data examine the difference between @p buff size before and
     * after this async function completes
     */
    template <class AsyncReadStream, class DynamicBuffer>
    task<std::size_t> read_until(AsyncReadStream& s, DynamicBuffer& buff,
                                 std::string_view end_mark,
                                 std::size_t step = 1024,
                                 std::size_t max_size = size_t(-1)) {
        if (end_mark.empty()) {
            throw std::system_error{
                std::make_error_code(std::errc::invalid_argument)};
        }
        while (1) {
            const size_t buff_pos = buff.size();
            if (buff_pos >= max_size) {
                throw std::system_error{
                    std::make_error_code(std::errc::value_too_large)};
            }
            const std::size_t inc_step = std::min(step, max_size - buff_pos);
            assert(inc_step > 0);
            buff.resize(buff_pos + inc_step);
            auto read_buff = buffer(buff).sub_buffer(buff_pos);
            auto n = co_await s.async_read_some(read_buff);
            assert(n > 0);
            buff.resize(buff_pos + n);
            auto v = buffer(buff).template to_string_view<char>();
            auto mark_pos = v.find(end_mark);
            if (mark_pos != std::string_view::npos) {
                co_return 0 + mark_pos + end_mark.size();
            }
        }
    }
} // namespace RAD_LIB_NAMESPACE
