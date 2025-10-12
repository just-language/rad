#pragma once
#include <rad/buffer.h>
#include <rad/detail/string_converter.h>
#include <rad/io/detail/file_impl.h>
#include <rad/os_types.h>
#include <rad/string.h>

#include <chrono>
#include <functional>
#include <limits>

namespace RAD_LIB_NAMESPACE::io::files {
    namespace details = RAD_LIB_NAMESPACE::detail;

    /*!
     * @brief File wrapper to read and write files in binary mode.
     */
    class file {
        using impl_type = detail::file_impl;

        using native_string_type = impl_type::native_string_type;
        using alternative_string_type1 = impl_type::alternative_string_type1;
        using alternative_string_type2 = impl_type::alternative_string_type2;
        using native_path_type = impl_type::native_path_type;

        using string_converter =
            details::string_converter<native_string_type,
                                      alternative_string_type1,
                                      alternative_string_type2>;

    public:
        using native_handle_type = impl_type::native_handle_type;

        /*!
         * @brief Construct a closed file.
         */
        file() = default;

        /*!
         * @brief Construct and create file.
         * @tparam StringType The type of path string.
         * @param path The path of the file to create.
         * @param mode The creation mode of the file.
         * @param access_rights The access rights to the file.
         * @param share The share mode of the file.
         * @param attr The attributes of the file.
         */
        template <class StringType>
        file(const StringType& path, create_mode mode,
             access access_rights = access::read_write,
             share_mode share = share_mode::read,
             attributes attr = attributes::normal) {
            create(path, mode, access_rights, share, attr);
        }

        /*!
         * @brief Construct and open file.
         * @tparam StringType The type of path string.
         * @param path The path of the file to open.
         * @param mode The open mode of the file.
         * @param access_rights The access rights to the file.
         * @param share The share mode of the file.
         * @param attr The attributes of the file.
         */
        template <class StringType>
        file(const StringType& path, open_mode mode,
             access access_rights = access::read_write,
             share_mode share = share_mode::read,
             attributes attr = attributes::normal) {
            open(path, mode, access_rights, share, attr);
        }

        /*!
         * @brief Get a reference to the file native handle.
         * @return A reference to the file native handle.
         */
        native_handle_type& native_handle() noexcept {
            return impl.native_handle();
        }

        /*!
         * @brief Get a reference to the file native handle.
         * @return A reference to the file native handle.
         */
        const native_handle_type& native_handle() const noexcept {
            return impl.native_handle();
        }

        /*!
         * @brief Check if the file is valid (open) or not.
         * @return True if the file is open, and false if
         * closed.
         */
        bool is_valid() const noexcept {
            return impl.is_valid();
        }

        /*!
         * @brief Check if the file is valid (open) or not.
         * @return True if the file is open, and false if
         * closed.
         */
        explicit operator bool() const noexcept {
            return is_valid();
        }

        /*!
         * @brief Create a file. If the operation fails this
         * file retains its previous state.
         * @tparam StringType The type of path string.
         * @param path The path of the file to create.
         * @param mode The creation mode of the file.
         * @param access_rights The access rights to the file.
         * @param share The share mode of the file.
         * @param attr The attributes of the file.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        template <class StringType>
        void create(const StringType& path, create_mode mode,
                    access access_rights, share_mode share, attributes attr,
                    std::error_code& ec) {
            string_converter cv;
            impl.create(cv(path), mode, access_rights, share, attr, ec);
        }

        /*!
         * @brief Create a file. If the operation fails this
         * file retains its previous state.
         * @tparam StringType The type of path string.
         * @param path The path of the file to create.
         * @param mode The creation mode of the file.
         * @param access_rights The access rights to the file.
         * @param share The share mode of the file.
         * @param attr The attributes of the file.
         * @throws On failure `std::system_error` is thrown.
         */
        template <class StringType>
        void create(const StringType& path, create_mode mode,
                    access access_rights = access::read_write,
                    share_mode share = share_mode::read,
                    attributes attr = attributes::normal) {
            std::error_code ec;
            create(path, mode, access_rights, share, attr, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Open a file. If the operation fails this file
         * retains its previous state.
         * @tparam StringType The type of path string.
         * @param path The path of the file to open.
         * @param mode The open mode of the file.
         * @param access_rights The access rights to the file.
         * @param share The share mode of the file.
         * @param attr The attributes of the file.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        template <class StringType>
        void open(const StringType& path, open_mode mode, access access_rights,
                  share_mode share, attributes attr, std::error_code& ec) {
            string_converter cv;
            impl.open(cv(path), mode, access_rights, share, attr, ec);
        }

        /*!
         * @brief Open a file. If the operation fails this file
         * retains its previous state.
         * @tparam StringType The type of path string.
         * @param path The path of the file to open.
         * @param mode The open mode of the file.
         * @param access_rights The access rights to the file.
         * @param share The share mode of the file.
         * @param attr The attributes of the file.
         * @throws On failure `std::system_error` is thrown.
         */
        template <class StringType>
        void open(const StringType& path, open_mode mode,
                  access access_rights = access::read_write,
                  share_mode share = share_mode::read,
                  attributes attr = attributes::normal) {
            std::error_code ec;
            open(path, mode, access_rights, share, attr, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get the size in bytes of the file.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The size in bytes of the file.
         */
        uint64_t size(std::error_code& ec) const noexcept {
            return impl.size(ec);
        }

        /*!
         * @brief Get the size in bytes of the file.
         * @return The size in bytes of the file.
         * @throws On failure `std::system_error` is thrown.
         */
        uint64_t size() const {
            std::error_code ec;
            auto file_size = size(ec);
            check_and_throw(ec, __func__);
            return file_size;
        }

        /*!
         * @brief Get the creation, last write, and last access
         * times of the file. Note that not all filesystems
         * support all these file times, so only available times
         * are returned.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The times of the file.
         */
        times_t times(std::error_code& ec) const noexcept {
            return impl.times(ec);
        }

        /*!
         * @brief Get the creation, last write, and last access
         * times of the file. Note that not all filesystems
         * support all these file times, so only available times
         * are returned.
         * @return The times of the file.
         * @throws On failure `std::system_error` is thrown.
         */
        times_t times() {
            std::error_code ec;
            auto t = times(ec);
            check_and_throw(ec, __func__);
            return t;
        }

        /*!
         * @brief Set the creation, last write, and last access
         * times of the file. Note that not all filesystems
         * support all these file times, so only available times
         * are set.
         * @param times The times of the file.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        void set_times(const times_t& times, std::error_code& ec) noexcept {
            impl.set_times(times, ec);
        }

        /*!
         * @brief Set the creation, last write, and last access
         * times of the file. Note that not all filesystems
         * support all these file times, so only available times
         * are set.
         * @param times The times of the file.
         * @throws On failure `std::system_error` is thrown.
         */
        void set_times(const times_t& times) {
            std::error_code ec;
            set_times(times, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get a reference to the file path.
         * @return A reference to the file path.
         */
        const native_path_type& path() const noexcept {
            return impl.path();
        }

        /*!
         * @brief Close the file if it is open.
         */
        void close() noexcept {
            impl.close();
        }

        bool replace(const std::wstring& new_file_path,
                     const std::wstring& backup);

        bool replace(const std::string& new_file_path,
                     const std::string& backup);

        /*!
         * @brief Move the file pointer by distance according to
         * @p mode.
         * @param distance The distance to move the file pointer
         * by. This may be from the begin of the file, the end
         * of the file, or from the current position. Negative
         * distance is valid and moves the file pointer back.
         * @param mode The seek mode.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        void seek(int64_t distance, seek_mode mode,
                  std::error_code& ec) noexcept {
            impl.seek(distance, mode, ec);
        }

        /*!
         * @brief Move the file pointer by distance according to
         * @p mode.
         * @param distance The distance to move the file pointer
         * by. This may be from the begin of the file, the end
         * of the file, or from the current position. Negative
         * distance is valid and moves the file pointer back.
         * @param mode The seek mode. The default is current.
         * @throws On failure `std::system_error` is thrown.
         */
        void seek(int64_t distance, seek_mode mode = seek_mode::current) {
            std::error_code ec;
            seek(distance, mode, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get the positon of the file pointer according
         * to @p mode.
         * @param mode The mode that determines what is the
         * returned position relative to. This may be from the
         * begin of the file, the end of the file, or from the
         * current position.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The positon of the file pointer.
         */
        int64_t position(seek_mode mode, std::error_code& ec) const noexcept {
            return impl.position(mode, ec);
        }

        /*!
         * @brief Get the positon of the file pointer according
         * to @p mode.
         * @param mode The mode that determines what is the
         * returned position relative to. This may be from the
         * begin of the file, the end of the file, or from the
         * current position.
         * @return The positon of the file pointer.
         * @throws On failure `std::system_error` is thrown.
         */
        int64_t position(seek_mode mode = seek_mode::begin) const {
            std::error_code ec;
            auto pos = position(mode, ec);
            check_and_throw(ec, __func__);
            return pos;
        }

        /*!
         * @brief Set the file attributes. On filesystems that
         * don't support this operation nothing is done.
         * @param attrs The attributes to set on the file.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        void set_attributes(attributes attrs, std::error_code& ec) noexcept {
            impl.set_attributes(attrs, ec);
        }

        /*!
         * @brief Set the file attributes. On filesystems that
         * don't support this operation nothing is done.
         * @param attrs The attributes to set on the file.
         * @throws On failure `std::system_error` is thrown.
         */
        void set_attributes(attributes attrs) {
            std::error_code ec;
            set_attributes(attrs, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Get the attributes of the file.
         * @return The attributes of the file.
         */
        attributes get_attributes() const noexcept {
            return impl.get_attributes();
        }

        /*!
         * @brief Write a buffer of data to the file at current
         * file pointer. Write is done in blocking mode. The
         * method does not return until all supplied buffer is
         * written or an error occurs.
         * @param buff The data buffer to write.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of written bytes.
         */
        std::size_t write(const_buffer buff, std::error_code& ec) noexcept {
            return impl.write(buff, ec);
        }

        /*!
         * @brief Write a buffer of data to the file at current
         * file pointer. Write is done in blocking mode. The
         * method does not return until all supplied buffer is
         * written or an error occurs.
         * @param buff The data buffer to write.
         * @return The count of written bytes.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t write(const_buffer buff) {
            std::error_code ec;
            auto written = write(buff, ec);
            check_and_throw(ec, __func__);
            return written;
        }

        /*!
         * @brief Read from the file into a buffer at current
         * file pointer. Read is done in blocking mode. The
         * method will return if at least one byte is read, or
         * an error occurs.
         * @param buff The buffer to read into.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of read bytes.
         */
        std::size_t read(mutable_buffer buff, std::error_code& ec) noexcept {
            return impl.read(buff, ec);
        }

        /*!
         * @brief Read from the file into a buffer at current
         * file pointer. Read is done in blocking mode. The
         * method will return if at least one byte is read, or
         * an error occurs.
         * @param buff The buffer to read into.
         * @return The count of read bytes.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t read(mutable_buffer buff) {
            std::error_code ec;
            auto read_num = read(buff, ec);
            check_and_throw(ec, __func__);
            return read_num;
        }

        /*!
         * @brief Read from the file into a buffer at current
         * file pointer. Read is done in blocking mode. The
         * method does not return until all supplied buffer is
         * filled or an error occurs.
         * @param buff The buffer to read into.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of read bytes.
         */
        std::size_t read_all(mutable_buffer buff,
                             std::error_code& ec) noexcept {
            std::size_t total_size = buff.size();
            while (buff.size() && !ec) {
                buff += read(buff, ec);
            }
            return total_size - buff.size();
        }

        /*!
         * @brief Read from the file into a buffer at current
         * file pointer. Read is done in blocking mode. The
         * method does not return until all supplied buffer is
         * filled or an error occurs.
         * @param buff The buffer to read into.
         * @return The count of read bytes.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t read_all(mutable_buffer buff) {
            std::error_code ec;
            std::size_t n = read_all(buff, ec);
            check_and_throw(ec, __func__);
            return n;
        }

        /*!
         * @brief Write a buffer of data to the file at a
         * spcefied position from the begin of the file. Write
         * is done in blocking mode. The method does not return
         * until all supplied buffer is written or an error
         * occurs. Current file pointer is not affected by the
         * amount of written bytes.
         * @param buff The data buffer to write.
         * @param pos The position from the begin of the file to
         * write at.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of written bytes.
         */
        std::size_t write_at(const_buffer buff, uint64_t pos,
                             std::error_code& ec) noexcept {
            return impl.write_at(buff, pos, ec);
        }

        /*!
         * @brief Write a buffer of data to the file at a
         * spcefied position from the begin of the file. Write
         * is done in blocking mode. The method does not return
         * until all supplied buffer is written or an error
         * occurs. Current file pointer is not affected by the
         * amount of written bytes.
         * @param buff The data buffer to write.
         * @param pos The position from the begin of the file to
         * write at.
         * @return The count of written bytes.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t write_at(const_buffer buff, uint64_t pos) {
            std::error_code ec;
            auto written = write_at(buff, pos, ec);
            check_and_throw(ec, __func__);
            return written;
        }

        /*!
         * @brief Read from the file into a buffer at a spcefied
         * position from the begin of the file. Read is done in
         * blocking mode. The method will return if at least one
         * byte is read, or an error occurs. Current file
         * pointer is not affected by the amount of read bytes.
         * @param buff The buffer to read into.
         * @param pos The position from the begin of the file to
         * read at.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of read bytes.
         */
        std::size_t read_at(mutable_buffer buff, uint64_t pos,
                            std::error_code& ec) noexcept {
            return impl.read_at(buff, pos, ec);
        }

        /*!
         * @brief Read from the file into a buffer at a spcefied
         * position from the begin of the file. Read is done in
         * blocking mode. The method will return if at least one
         * byte is read, or an error occurs. Current file
         * pointer is not affected by the amount of read bytes.
         * @param buff The buffer to read into.
         * @param pos The position from the begin of the file to
         * read at.
         * @return The count of read bytes.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t read_at(mutable_buffer buff, uint64_t pos) {
            std::error_code ec;
            std::size_t n = read_at(buff, pos, ec);
            check_and_throw(ec, __func__);
            return n;
        }

        /*!
         * @brief Read from the file into a buffer at a spcefied
         * position from the begin of the file. Read is done in
         * blocking mode. The method does not return until all
         * supplied buffer is filled or an error occurs. Current
         * file pointer is not affected by the amount of read
         * bytes.
         * @param buff The buffer to read into.
         * @param pos The position from the begin of the file to
         * read at.
         * @param ec Cleared on success, and set to error on
         * failure.
         * @return The count of read bytes.
         */
        std::size_t read_all_at(mutable_buffer buff, uint64_t pos,
                                std::error_code& ec) noexcept {
            std::size_t total_size = buff.size();
            ec.clear();
            while (!ec && !buff.empty()) {
                std::size_t n = read_at(buff, pos, ec);
                pos += n;
                buff += n;
            }
            return total_size - buff.size();
        }

        /*!
         * @brief Read from the file into a buffer at a spcefied
         * position from the begin of the file. Read is done in
         * blocking mode. The method does not return until all
         * supplied buffer is filled or an error occurs. Current
         * file pointer is not affected by the amount of read
         * bytes.
         * @param buff The buffer to read into.
         * @param pos The position from the begin of the file to
         * read at.
         * @return The count of read bytes.
         * @throws On failure `std::system_error` is thrown.
         */
        std::size_t read_all_at(mutable_buffer buff, uint64_t pos) {
            std::error_code ec;
            std::size_t n = read_all_at(buff, pos, ec);
            check_and_throw(ec, __func__);
            return n;
        }

        /*!
         * @brief Write CRLF at the current file position.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        void write_endl(std::error_code& ec) noexcept {
            using namespace std::string_view_literals;
            write(buffer("\r\n"sv), ec);
        }

        /*!
         * @brief Write CRLF at the current file position.
         * @throws On failure `std::system_error` is thrown.
         */
        void write_endl() {
            using namespace std::string_view_literals;
            write(buffer("\r\n"sv));
        }

        /*!
         * @brief Read all the file data starting at @p pos into
         * the dynamic buffer @p buff.
         * @param buff The dynamic buffer to read into. Read
         * data is appended to the buffer.
         * @param pos The position from the begin of the file to
         * read at. If the position exceeds the file size
         * nothing is read.
         * @throws On failure `std::system_error` is thrown.
         */
        void read_all_file(dynamic_buffer buff, uint64_t pos = 0) {
            uint64_t total_size = size();
            pos = pos > total_size ? total_size : pos;
            uint64_t len = total_size - pos;
            if (len == 0) {
                return;
            }
            auto read_buff = buff.prepare(static_cast<std::size_t>(len));
            read_all_at(read_buff, pos);
        }

        /*!
         * @brief Truncate the file at the current file pointer.
         * After success the file size is equal to the current
         * file pointer.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        void truncate_here(std::error_code& ec) noexcept {
            impl.truncate_here(ec);
        }

        /*!
         * @brief Truncate the file at the current file pointer.
         * After success the file size is equal to the current
         * file pointer.
         * @throws On failure `std::system_error` is thrown.
         */
        void truncate_here() {
            std::error_code ec;
            truncate_here(ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Truncate the file at specified position.
         * This may be from the begin of the file, the end of
         * the file, or from the current position.
         * @param pos The position to truncate the file at.
         * @param mode The mode which determines what is @pos
         * relative to.
         * @param ec Cleared on success, and set to error on
         * failure.
         */
        void truncate(int64_t pos, seek_mode mode,
                      std::error_code& ec) noexcept {
            impl.truncate(pos, mode, ec);
        }

        /*!
         * @brief Truncate the file at specified position.
         * This may be from the begin of the file, the end of
         * the file, or from the current position.
         * @param pos The position to truncate the file at.
         * @param mode The mode which determines what is @pos
         * relative to.
         * @throws On failure `std::system_error` is thrown.
         */
        void truncate(int64_t pos, seek_mode mode = seek_mode::begin) {
            std::error_code ec;
            truncate(pos, mode, ec);
            check_and_throw(ec, __func__);
        }

        /*!
         * @brief Swap the file with another file.
         * @param rhs The other file to swap with.
         */
        void swap(file& rhs) noexcept {
            impl.swap(rhs.impl);
        }

    private:
        impl_type impl;
    };

    /*!
     * @brief Swap two files.
     * @param lhs The first file.
     * @param rhs The second file.
     */
    inline void swap(file& lhs, file& rhs) noexcept {
        lhs.swap(rhs);
    }

    /*!
     * @brief Read all the file data starting at @p pos into the dynamic
     * buffer @p buff.
     * @tparam StringType The type of path string.
     * @param path The path of the file to read.
     * @param buff The dynamic buffer to read into. Read data is appended to
     * the buffer.
     * @param pos The position from the begin of the file to read at. If the
     * position exceeds the file size nothing is read.
     * @throws On failure `std::system_error` is thrown.
     */
    template <class StringType>
    inline void read_all_file(const StringType& path, dynamic_buffer buff,
                              uint64_t pos = 0) {
        file f;
        f.open(path, open_mode::existing, access::read);
        f.read_all_file(buff, pos);
    }
} // namespace RAD_LIB_NAMESPACE::io::files
