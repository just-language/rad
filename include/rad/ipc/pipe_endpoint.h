#pragma once
#include <rad/libbase.h>
#include <rad/string.h>

namespace RAD_LIB_NAMESPACE::pipe::detail {
    /// Prefix for windows pipes names.
    inline constexpr std::wstring_view pipe_name_prefix = LR"(\\.\pipe\)";

    inline constexpr std::string_view pipe_name_prefix_utf8 = R"(\\.\pipe\)";

    inline void convert_pipe_name(std::string_view input,
                                  std::wstring& output) {
        output.clear();
        if (input.starts_with(pipe_name_prefix_utf8)) {
            input.remove_prefix(pipe_name_prefix_utf8.size());
        }
        if (input.empty()) {
            return;
        }
        to_wstring(input, output);
    }

    inline void convert_pipe_name(std::wstring_view input,
                                  std::wstring& output) {
        output.clear();
        if (input.starts_with(pipe_name_prefix)) {
            input.remove_prefix(pipe_name_prefix.size());
        }
        if (input.empty()) {
            return;
        }
        output = input;
    }

    inline std::wstring convert_pipe_name(std::string_view input) {
        std::wstring output;
        convert_pipe_name(input, output);
        return output;
    }

    inline std::wstring convert_pipe_name(std::wstring_view input) {
        std::wstring output;
        convert_pipe_name(input, output);
        return output;
    }
} // namespace RAD_LIB_NAMESPACE::pipe::detail

namespace RAD_LIB_NAMESPACE::pipe {
    /*!
     * @brief The pipe open mode.
     * It specifies the data flow direction and
     * how many instances of the pipe can be created.
     */
    enum class open_mode : uint32_t {
        client_to_server = 0x00000001,
        server_to_client = 0x00000002,
        duplex = 0x00000003,
        overlapped = 0x40000000,
        one_instance = 0x00080000,
    };

    /*!
     * @brief The pipe transfer flags.
     * It specifies whether to write and read data
     * as a stream of bytes (like tcp) or as messages (like udp).
     */
    enum class transfer_flags : uint32_t {
        write_bytes = 0x00000000,
        read_bytes = write_bytes,
        write_messages = 0x00000004,
        read_messages = 0x00000002,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(open_mode);

    RAD_OVERLOAD_ENUM_OPERATORS(transfer_flags);

    /*!
     * @brief A pipe endpoint contains the pipe name and settings to create
     * and open the pipe. Settings include open mode, transfer mode, max
     * instances, rserved input and output sizes, timeout duration and
     * security attributes.
     */
    class endpoint {
    public:
        /*!
         * @brief Construct an empty pipe endpoint with no name
         * and with the default settings.
         * The endpoint wihtout name can't be used to create or
         * open pipes.
         *
         * The default settings are:
         * duplex data flow, write and read bytes, only one
         * allowed instance of the pipe, reserved input and
         * output buffers size is 1024 bytes, no timeout and no
         * security attributes.
         */
        endpoint() = default;

        /*!
         * @brief Construct a pipe endpoint with UTF-8 pipe name
         * and other settings.
         * @param name The pipe name in UTF-8 with or without
         * the pipe name prefix.
         * @param flow_dir The pipe data flow direction (open
         * mode).
         * @param flags The pipe transfer flags.
         * @param max_instances The maximum allowed pipe
         * instances.
         * @param input_size The pipe reserved input buffer
         * size.
         * @param output_size The pipe reserved output buffer
         * size.
         * @param timeout_ms The pipe timeout in milli seconds.
         * @param sec_attributes The pipe security attributes.
         */
        endpoint(std::string_view name, open_mode flow_dir,
                 transfer_flags flags, uint32_t max_instances,
                 uint32_t input_size, uint32_t output_size, uint32_t timeout_ms,
                 void* sec_attributes)
            : name_{detail::convert_pipe_name(name)}, flow_dir_{flow_dir},
              flow_mode_{flags}, max_instances_{max_instances},
              in_buff_size{input_size}, out_buff_size{output_size},
              timeout_ms{timeout_ms}, sec_attributes{sec_attributes} {
        }

        /*!
         * @brief Construct a pipe endpoint with UTF-16 pipe
         * name and other settings.
         * @param name The pipe name in UTF-16 with or without
         * the pipe name prefix.
         * @param flow_dir The pipe data flow direction (open
         * mode).
         * @param flags The pipe transfer flags.
         * @param max_instances The maximum allowed pipe
         * instances.
         * @param input_size The pipe reserved input buffer
         * size.
         * @param output_size The pipe reserved output buffer
         * size.
         * @param timeout_ms The pipe timeout in milli seconds.
         * @param sec_attributes The pipe security attributes.
         */
        endpoint(std::wstring_view name, open_mode flow_dir,
                 transfer_flags flags, uint32_t max_instances,
                 uint32_t input_size, uint32_t output_size, uint32_t timeout_ms,
                 void* sec_attributes)
            : name_{detail::convert_pipe_name(name)}, flow_dir_{flow_dir},
              flow_mode_{flags}, max_instances_{max_instances},
              in_buff_size{input_size}, out_buff_size{output_size},
              timeout_ms{timeout_ms}, sec_attributes{sec_attributes} {
        }

        /*!
         * @brief Construct a pipe endpoint with pipe name and
         * default settings.
         *
         * The default settings are:
         * duplex data flow, write and read bytes, only one
         * allowed instance of the pipe, reserved input and
         * output buffers size is 1024 bytes, no timeout and no
         * security attributes.
         * @param name The pipe name in UTF-8 with or without
         * the pipe name prefix.
         */
        endpoint(std::string_view name)
            : name_{detail::convert_pipe_name(name)} {
        }

        /*!
         * @brief Construct a pipe endpoint with pipe name and
         * default settings.
         *
         * The default settings are:
         * duplex data flow, write and read bytes, only one
         * allowed instance of the pipe, reserved input and
         * output buffers size is 1024 bytes, no timeout and no
         * security attributes.
         * @param name The pipe name in UTF-16 with or without
         * the pipe name prefix.
         */
        endpoint(std::wstring_view name)
            : name_{detail::convert_pipe_name(name)} {
        }

        /*!
         * @brief Set the pipe name.
         * @param name The pipe name in UTF-8 with or without
         * the pipe name prefix.
         */
        void pipe_name(std::string_view name) {
            detail::convert_pipe_name(name, name_);
        }

        /*!
         * @brief Set the pipe name.
         * @param name The pipe name in UTF-16 with or without
         * the pipe name prefix.
         */
        void pipe_name(std::wstring_view name) {
            detail::convert_pipe_name(name, name_);
        }

        /*!
         * @brief Get the pipe name without prefix in UTF-16.
         * @return The pipe name without prefix in UTF-16.
         */
        const std::wstring& pipe_name() const {
            return name_;
        }

        /*!
         * @brief Get the pipe name without prefix in UTF-8.
         * @return The pipe name without prefix in UTF-8.
         */
        std::string pipe_name_utf8() const {
            return to_string(name_);
        }

        /*!
         * @brief Get the pipe name with prefix in UTF-16.
         * @return The pipe name with prefix in UTF-16.
         */
        std::wstring name_with_prefix() const {
            return detail::pipe_name_prefix + name_;
        }

        /*!
         * @brief Get the pipe name with prefix in UTF-8.
         * @return The pipe name with prefix in UTF-8.
         */
        std::string name_with_prefix_utf8() const {
            return to_string(name_with_prefix());
        }

        /*!
         * @brief Set the pipe data flow direction (open mode).
         * @param mode The pipe data flow direction (open mode).
         */
        void flow_dir(open_mode mode) noexcept {
            flow_dir_ = mode;
        }

        /*!
         * @brief Set the pipe data flow direction (open mode).
         * @return The pipe data flow direction (open mode).
         */
        constexpr open_mode flow_dir() const noexcept {
            return flow_dir_ | open_mode::overlapped;
        }

        /*!
         * @brief Set the pipe transfer flags.
         * @param flags The pipe transfer flags.
         */
        void transfer(transfer_flags flags) noexcept {
            flow_mode_ = flags;
        }

        /*!
         * @brief Get the pipe transfer flags.
         * @return The pipe transfer flags.
         */
        constexpr transfer_flags transfer() const noexcept {
            return flow_mode_;
        }

        /*!
         * @brief Set the the maximum allowed pipe instances.
         * @param count The maximum allowed pipe instances.
         */
        void instances(uint32_t count) noexcept {
            max_instances_ = count;
        }

        /*!
         * @brief Get the the maximum allowed pipe instances.
         * @return The maximum allowed pipe instances.
         */
        constexpr uint32_t instances() const noexcept {
            return max_instances_;
        }

        /*!
         * @brief Set the pipe reserved input buffer size.
         * @param size The pipe reserved input buffer size.
         */
        void input_size(uint32_t size) noexcept {
            in_buff_size = size;
        }

        /*!
         * @brief Get the pipe reserved input buffer size.
         * @return The pipe reserved input buffer size.
         */
        constexpr uint32_t input_size() const noexcept {
            return in_buff_size;
        }

        /*!
         * @brief Set the pipe reserved output buffer size.
         * @param size The pipe reserved output buffer size.
         */
        void output_size(uint32_t size) noexcept {
            out_buff_size = size;
        }

        /*!
         * @brief Get the pipe reserved output buffer size.
         * @return The pipe reserved output buffer size.
         */
        constexpr uint32_t output_size() const noexcept {
            return out_buff_size;
        }

        /*!
         * @brief Set the pipe timeout in milli seconds.
         * @param time_ms The pipe timeout in milli seconds.
         */
        void timeout(uint32_t time_ms) noexcept {
            timeout_ms = time_ms;
        }

        /*!
         * @brief Get the pipe timeout in milli seconds.
         * @return The pipe timeout in milli seconds.
         */
        constexpr uint32_t timeout() const noexcept {
            return timeout_ms;
        }

        /*!
         * @brief Set the pipe security attributes.
         * @param sec The pipe security attributes.
         */
        void security_attributes(void* sec) noexcept {
            sec_attributes = sec;
        }

        /*!
         * @brief Get the pipe security attributes.
         * @return The pipe security attributes.
         */
        void* security_attributes() const noexcept {
            return sec_attributes;
        }

        /*!
         * @brief Get the maximum allowed instances for any
         * pipe.
         * @return The maximum allowed instances for any pipe.
         */
        static constexpr uint32_t max_instances() noexcept {
            return 255;
        }

    private:
        std::wstring name_;
        open_mode flow_dir_ = open_mode::duplex | open_mode::overlapped;
        transfer_flags flow_mode_ = transfer_flags::write_bytes;
        uint32_t max_instances_ = 1;
        uint32_t in_buff_size = 1024;
        uint32_t out_buff_size = 1024;
        uint32_t timeout_ms = 0;
        void* sec_attributes = nullptr;
    };

} // namespace RAD_LIB_NAMESPACE::pipe