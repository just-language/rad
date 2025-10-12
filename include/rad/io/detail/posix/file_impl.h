#pragma once
#include <rad/buffer.h>
#include <rad/io/detail/file_common.h>
#include <rad/os_types.h>
#include <rad/string.h>

namespace RAD_LIB_NAMESPACE::io::files::detail {
    class file_impl {
    public:
        using native_string_type = zstring_view;
        using alternative_string_type1 = std::string;
        using alternative_string_type2 = std::wstring_view;

        using native_handle_type = os::handle;
        using native_path_type = std::string;

        file_impl() = default;

        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        bool is_valid() const noexcept {
            return static_cast<bool>(handle_);
        }

        explicit operator bool() const noexcept {
            return is_valid();
        }

        RAD_EXPORT_DECL void create(native_string_type path, create_mode mode,
                                    access access_rights, share_mode share,
                                    attributes attr,
                                    std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void open(native_string_type path, open_mode mode,
                                  access access_rights, share_mode share,
                                  attributes attr,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL uint64_t size(std::error_code& ec) const noexcept;

        RAD_EXPORT_DECL times_t times(std::error_code& ec) const noexcept;

        RAD_EXPORT_DECL void set_times(const times_t& times,
                                       std::error_code& ec) noexcept;

        const native_path_type& path() const noexcept {
            return path_;
        }

        void close() noexcept {
            handle_.reset();
            path_.clear();
        }

        RAD_EXPORT_DECL void seek(int64_t distance, seek_mode mode,
                                  std::error_code& ec) noexcept;

        RAD_EXPORT_DECL int64_t position(seek_mode mode,
                                         std::error_code& ec) const noexcept;

        RAD_EXPORT_DECL void set_attributes(attributes attributes,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL attributes get_attributes() const noexcept;

        RAD_EXPORT_DECL std::size_t write(const_buffer buff,
                                          std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t read(mutable_buffer buff,
                                         std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t write_at(const_buffer buff, uint64_t pos,
                                             std::error_code& ec) noexcept;

        RAD_EXPORT_DECL std::size_t read_at(mutable_buffer buff, uint64_t pos,
                                            std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void truncate_here(std::error_code& ec) noexcept;

        RAD_EXPORT_DECL void truncate(int64_t pos, seek_mode mode,
                                      std::error_code& ec) noexcept;

        void swap(file_impl& other) {
            std::swap(handle_, other.handle_);
            std::swap(path_, other.path_);
        }

    private:
        native_handle_type handle_;
        native_path_type path_;
    };
} // namespace RAD_LIB_NAMESPACE::io::files::detail