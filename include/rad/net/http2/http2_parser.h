#pragma once
#include <rad/buffer.h>
#include <rad/net/http2/hpack.h>
#include <rad/string.h>

#include <array>
#include <chrono>
#include <span>

namespace RAD_LIB_NAMESPACE::net::http2 {
    /*!
     * @brief HTTP 2 error codes.
     */
    enum class error : uint16_t {
        /*!
         * @brief NO_ERROR (0x00) The associated condition is not a result of an
         * error.
         */
        no_error,
        /*!
         * @brief PROTOCOL_ERROR (0x01): The endpoint detected an unspecific
         * protocol error.
         */
        protocol_error,
        /*!
         * @brief INTERNAL_ERROR (0x02): The endpoint encountered an unexpected
         * internal error.
         */
        internal_error,
        /*!
         * @brief FLOW_CONTROL_ERROR (0x03): The endpoint detected that its peer
         * violated the flow-control protocol.
         */
        flow_control_error,
        /*!
         * @brief SETTINGS_TIMEOUT (0x04): The endpoint sent a SETTINGS frame
         * but did not receive a response in a timely manner.
         */
        settings_timeout,
        /*!
         * @brief STREAM_CLOSED (0x05): The endpoint received a frame after a
         * stream was half-closed.
         */
        stream_closed,
        /*!
         * @brief FRAME_SIZE_ERROR (0x06): The endpoint received a frame with an
         * invalid size.
         */
        frame_size_error,
        /*!
         * @brief REFUSED_STREAM (0x07): The endpoint refused the stream prior
         * to performing any application processing.
         */
        refused_stream,
        /*!
         * @brief CANCEL (0x08): The endpoint uses this error code to indicate
         * that the stream is no longer needed.
         */
        cancel,
        /*!
         * @brief COMPRESSION_ERROR (0x09): The endpoint is unable to maintain
         * the field section compression context for the connection.
         */
        compression_error,
        /*!
         * @brief CONNECT_ERROR (0x0a): The connection established in response
         * to a CONNECT request was reset or abnormally closed.
         */
        connect_error,
        /*!
         * @brief ENHANCE_YOUR_CALM (0x0b):The endpoint detected that its peer
         * is exhibiting a behavior that might be generating excessive load.
         */
        enhance_your_calm,
        /*!
         * @brief INADEQUATE_SECURITY (0x0c): The underlying transport has
         * properties that do not meet minimum security requirements.
         */
        inadequate_security,
        /*!
         * @brief HTTP_1_1_REQUIRED (0x0d): The endpoint requires that HTTP/1.1
         * be used instead of HTTP/2.
         */
        http_1_1_required,
    };

    /*!
     * @brief Get a const reference to the http 2 error category.
     * @return A const reference to the http 2 error category.
     */
    const std::error_category& http2_category() noexcept;

    /*!
     * @brief Make a `std::error_code` using the HTTP 2 error code @p ec
     * and the http2 error category.
     * @param ec The HTTP 2 error code.
     * @return A `std::error_code` using the HTTP 2 error code @p ec
     * and the http2 error category.
     */
    inline std::error_code make_error(error ec) noexcept {
        return std::error_code{static_cast<int>(ec), http2_category()};
    }

    /*!
     * @brief The minimum HTTP 2 payload size 2^14 - 1 (16384).
     */
    inline constexpr std::uint32_t min_payload_size = 1 << 14;
    /*!
     * @brief The maximum HTTP 2 payload size 2^24 - 1 (16777215).
     */
    inline constexpr std::uint32_t max_payload_size = (1 << 24) - 1;
    /*!
     * @brief The maximum HTTP 2 flow control window size 2^31 - 1.
     */
    inline constexpr std::uint32_t max_flow_control_window_size =
        (1u << 31u) - 1;
    /*!
     * @brief The maximim HTTP 2 stream id 2^31 - 1.
     */
    inline constexpr std::uint32_t max_stream_id = (1u << 31u) - 1;
    /*!
     * @brief The default value of SETTINGS_HEADER_TABLE_SIZE 4096.
     */
    inline constexpr std::uint32_t default_header_table_size = 4096;

    /*!
     * @brief The HTTP 2 ALPN protocol identifier.
     */
    inline constexpr std::string_view alpn_value = "h2";

    /*!
     * @brief The HTTP 2 connection preface sent by the client.
     */
    inline constexpr std::string_view connection_preface =
        "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

    /*!
     * @brief The HTTP 2 frames types.
     */
    enum class frame_type : uint8_t {
        data = 0x0,
        headers = 0x1,
        priority = 0x2,
        rst_stream = 0x3,
        settings = 0x4,
        push_promise = 0x5,
        ping = 0x6,
        goaway = 0x7,
        window_update = 0x8,
        continuation = 0x9,
    };

    /*!
     * @brief The HTTP 2 DATA frame allowed flags.
     */
    enum class data_frame_flags : uint8_t {
        none = 0,
        padded = 0x8,
        end_stream = 0x1,
    };

    /*!
     * @brief The HTTP 2 HEADERS frame allowed flags.
     */
    enum class headers_frame_flags : uint8_t {
        none = 0,
        end_stream = 0x1,
        end_headers = 0x4,
        padded = 0x8,
        priority = 0x20,
    };

    /*!
     * @brief The HTTP 2 SETTINGS frame allowed flags.
     */
    enum class settings_frame_flags : uint8_t {
        none = 0,
        ack = 0x1,
    };

    /*!
     * @brief The HTTP 2 CONTINUATION frame allowed flags.
     */
    enum class continuation_frame_flags : uint8_t {
        none = 0,
        end_headers = 0x4,
    };

    /*!
     * @brief The HTTP 2 defined frames flags.
     */
    enum class frame_flags : uint8_t {
        unspecified = 0,
        end_stream = 1 << 0,
        ack = end_stream,
        end_headers = 1 << 2,
        padded = 1 << 3,
        priority = 1 << 5,
    };

    inline constexpr std::uint8_t END_STREAM_FLAG = 0x1;
    inline constexpr std::uint8_t SETTINGS_ACK_FLAG = 0x1;
    inline constexpr std::uint8_t PING_ACK_FLAG = 0x1;
    inline constexpr std::uint8_t END_HEADERS_FLAG = 0x4;
    inline constexpr std::uint8_t PADDED_FLAG = 0x8;
    inline constexpr std::uint8_t PRIORITY_FLAG = 0x20;

    /*!
     * @brief The HTTP 2 defined settings.
     */
    enum class settings_id : uint16_t {
        /// SETTINGS_HEADER_TABLE_SIZE (0x01)
        header_table_size = 0x1,
        /// SETTINGS_ENABLE_PUSH (0x02)
        enable_push,
        /// SETTINGS_MAX_CONCURRENT_STREAMS (0x03)
        max_concurrent_streams,
        /// SETTINGS_INITIAL_WINDOW_SIZE (0x04)
        initial_window_size,
        /// SETTINGS_MAX_FRAME_SIZE (0x05)
        max_frame_size,
        /// SETTINGS_MAX_HEADER_LIST_SIZE (0x06)
        max_header_list_size,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(frame_flags);

    RAD_OVERLOAD_ENUM_OPERATORS(data_frame_flags);
    RAD_OVERLOAD_ENUM_OPERATORS(headers_frame_flags);
    RAD_OVERLOAD_ENUM_OPERATORS(settings_frame_flags);
    RAD_OVERLOAD_ENUM_OPERATORS(continuation_frame_flags);

    /*!
     * @brief HTTP 2 decoded frame header.
     */
    struct frame_header {
        /// The length of the frame excluding the header in host
        /// byte order.
        uint32_t length = 0;
        /// The stream id in host byte order.
        uint32_t stream_id = 0;
        /// The frame type.
        frame_type type = {};
        /// The frame flags.
        uint8_t flags = 0;

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will always be 9 bytes.
         */
        static constexpr uint32_t needed_write_size() noexcept {
            return 9;
        }

        /*!
         * @brief Write this frame header to @p out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least 9.
         * @return The count of written bytes. It will be 9
         * bytes if the buffer size is not less than 9 or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const;

        /*!
         * @brief Write this frame header to @p out_buff.
         * @param out_buff The output buffer where the encoded
         * frame header will be appended.
         * @return The count of written bytes. It will be 9
         * bytes.
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        /*!
         * @brief Get the minimum payload size for frames that
         * define one.
         * @return The minimum payload size for frames that
         * define one, or 0 for frames that have no minimum
         * size.
         */
        uint32_t min_payload_size() const noexcept {
            switch (type) {
            case frame_type::data:
                return 0;
            case frame_type::headers:
                return 0;
            case frame_type::priority:
                return 5;
            case frame_type::rst_stream:
                return 4;
            case frame_type::settings:
                return (flags & SETTINGS_ACK_FLAG) == SETTINGS_ACK_FLAG ? 0 : 6;
            case frame_type::push_promise:
                return 0;
            case frame_type::ping:
                return 8;
            case frame_type::goaway:
                return 8;
            case frame_type::window_update:
                return 4;
            case frame_type::continuation:
                return 0;
            default:
                return 0;
            }
        }

        /*!
         * @brief Check if the frame type is valid.
         * @param from_client Whether the frame is sent by a
         * client, or by a server.
         * @param push_allowed Whether PUSH_PROMISE frames are
         * allowed or not.
         * @return True if the frame type is valid, otherwise
         * false.
         */
        bool is_valid_type(bool from_client, bool push_allowed) const noexcept {
            if (type == frame_type::push_promise) {
                return !from_client && push_allowed;
            }
            const std::uint8_t t = static_cast<std::uint8_t>(type);
            // CONTINUATION frame (type=0x09)
            return t <= 0x9;
        }

        /*!
         * @brief Check if the frame size is valid.
         * @param max_frame_payload_size The maximum frame
         * payload size allowed to be received.
         * @return True if the frame size is valid, otherwise
         * false.
         */
        bool
        is_valid_size(std::uint32_t max_frame_payload_size) const noexcept {
            const bool is_padded = (flags & PADDED_FLAG) == PADDED_FLAG;
            const bool is_priority = (flags & PRIORITY_FLAG) == PRIORITY_FLAG;

            if (length > max_payload_size || length > max_frame_payload_size) {
                return false;
            }

            if (type == frame_type::settings) {
                if ((flags & SETTINGS_ACK_FLAG) == SETTINGS_ACK_FLAG) {
                    return length == 0;
                }
                return length % 6 == 0;
            }
            if (type == frame_type::priority) {
                return length == 5;
            }
            if (type == frame_type::rst_stream ||
                type == frame_type::window_update) {
                return length == 4;
            }
            if (type == frame_type::ping) {
                return length == 8;
            }
            if (type == frame_type::goaway) {
                return length >= 8;
            }
            if (type == frame_type::headers) {
                return length > static_cast<uint32_t>((is_padded ? 1 : 0) +
                                                      (is_priority ? 5 : 0));
            }
            if (type == frame_type::data) {
                return length >= static_cast<uint32_t>(is_padded ? 1 : 0);
            }
            // CONTINUATION payload may be 0 but it causes
            // security issues!
            return true;
        }

        /*!
         * @brief Check if the frame header flags are valid,
         * which means all flags are no undefined.
         * @return True if the frame header flags are valid,
         * otherwise false.
         */
        bool is_valid_flags() const noexcept {
            constexpr std::uint8_t all_frames_flags =
                END_STREAM_FLAG | END_HEADERS_FLAG | PRIORITY_FLAG |
                PADDED_FLAG | PING_ACK_FLAG;
            const std::uint8_t cleared_flags = flags & ~all_frames_flags;
            return cleared_flags == 0;
        }

        /*!
         * @brief Check if this frame is associated with the
         * connection as a whole as opposed to an individual
         * stream. Connection frames must have 0 stream
         * identifier.
         * @return True if this is a connection-level frame,
         * otherwise false.
         */
        bool is_connection_frame() const noexcept {
            return type == frame_type::settings || type == frame_type::ping ||
                   type == frame_type::goaway;
        }

        /*!
         * @brief Check if this frame is associated with an
         * individual stream as opposed to the connection as a
         * whole. Stream frames must have non zero stream
         * identifier.
         * @return True if this is a stream-level frame,
         * otherwise false.
         */
        bool is_stream_frame() const noexcept {
            return type == frame_type::data || type == frame_type::headers ||
                   type == frame_type::continuation ||
                   type == frame_type::rst_stream ||
                   type == frame_type::priority ||
                   type == frame_type::push_promise;
        }

        /*!
         * @brief Check if this frame may be associated with the
         * connection as a whole or to an individual stream. The
         * only frame that may be either a connection or a
         * stream frame is WINDOW_UPDATE frame.
         * @return True if this is WINDOW_UPDATE frame,
         * otherwise false.
         */
        bool is_connection_or_stream_frame() const noexcept {
            return type == frame_type::window_update;
        }

        /*!
         * @brief Check if this frame is allowed to be sent to
         * a closed stream.
         * @return True if this frame is allowed to be sent to a
         * closed stream, otherwise false.
         */
        bool may_be_sent_to_closed_stream() const noexcept {
            /*
             * An endpoint MUST NOT send frames other than
             * PRIORITY on a closed stream. An endpoint MAY
             * treat receipt of any other type of frame on a
             * closed stream as a connection error
             * (Section 5.4.1) of type STREAM_CLOSED, except
             * as noted below. An endpoint that sends a
             * frame with the END_STREAM flag set or a
             * RST_STREAM frame might receive a
             * WINDOW_UPDATE or RST_STREAM frame from its
             * peer in the time before the peer receives and
             * processes the frame that closes the stream.
             */
            return type == frame_type::priority ||
                   type == frame_type::window_update ||
                   type == frame_type::rst_stream;
        }

        /*!
         * @brief Check if this frame is allowed to be sent to
         * a half closed remote stream.
         * @return True if this frame is allowed to be sent to a
         * half closed remote stream, otherwise false.
         */
        bool may_be_sent_to_half_closed_remote_stream() const noexcept {
            /*
             * If an endpoint receives additional frames,
             * other than WINDOW_UPDATE, PRIORITY, or
             * RST_STREAM, for a stream that is in this
             * state, it MUST respond with a stream error
             * (Section 5.4.2) of type STREAM_CLOSED.
             */
            return type == frame_type::priority ||
                   type == frame_type::window_update ||
                   type == frame_type::rst_stream;
        }

        /*!
         * @brief Check if the stream identifier of this frame
         * is valid.
         * @param from_client Whether the frame is sent by a
         * client, or by a server.
         * @param last_client_id The last stream identifier
         * opened by the client with HEADERS frame.
         * @param last_server_id The last stream identifier
         * reserved by the server with PUSH_PROMISE frame.
         * @return True if the stream identifier of this frame
         * is valid, otherwise false.
         */
        RAD_EXPORT_DECL bool
        is_valid_stream_id(bool from_client, std::uint32_t last_client_id,
                           std::uint32_t last_server_id) const noexcept;

        /*!
         * @brief Check if this frame is valid.
         * @param previous_header The last received previous
         * header from the peer. If this is the first received
         * header, pass nullopt.
         * @param max_frame_payload_size The maximum frame
         * payload size allowed to be received.
         * @param push_allowed Whether PUSH_PROMISE frames are
         * allowed, or disabled.
         * @param from_client Whether the frame is sent by a
         * client, or by a server.
         * @param last_client_id The last stream identifier
         * opened by the client with HEADERS frame.
         * @param last_server_id The last stream identifier
         * reserved by the server with PUSH_PROMISE frame.
         * @return True if this frame is valid, otherwise false.
         */
        RAD_EXPORT_DECL bool
        is_valid_frame(const std::optional<frame_header>& previous_header,
                       std::uint32_t max_frame_payload_size, bool push_allowed,
                       bool from_client, std::uint32_t last_client_id,
                       std::uint32_t last_server_id) const noexcept;

        friend constexpr bool
        operator==(const frame_header& lhs,
                   const frame_header& rhs) noexcept = default;
    };

    /*!
     * @brief HTTP 2 encoded frame header.
     */
    class frame_header_buffer {
    public:
        /*!
         * @brief Make an invalid encoded frame buffer.
         */
        frame_header_buffer() = default;

        /*!
         * @brief Encode the frame header @p fh.
         * @param fh The decoded frame header to encode.
         */
        frame_header_buffer(const frame_header& fh) noexcept {
            set_length(fh.length);
            set_stream_id(fh.stream_id);
            set_type(fh.type);
            set_flags(fh.flags);
        }

        /*!
         * @brief Decode the frame header.
         * @return The decoded frame header.
         */
        frame_header get_frame_header() const noexcept {
            frame_header fh;
            fh.length = length();
            fh.stream_id = stream_id();
            fh.type = type();
            fh.flags = flags();
            return fh;
        }

        /*!
         * @brief Get the length of the payload in host byte
         * order.
         * @return The length of the payload in host byte order.
         */
        uint32_t length() const noexcept {
            beu32 b_size{};
            std::memcpy(&b_size, buff_, sizeof(beu32));
            uint32_t size = b_size;
            return size >> 8;
        }

        /*!
         * @brief Set the length of the payload in host byte
         * order. The length must not be greater than max
         * payload size 2^24 - 1 (16777215).
         * @param len The length of the payload in host byte
         * order.
         */
        void set_length(uint32_t len) noexcept {
            assert(len <= max_payload_size);
            len <<= 8;
            const beu32 b_size = len;
            std::memcpy(buff_, &b_size, sizeof(beu32));
        }

        /*!
         * @brief Get the frame type.
         * @return The frame type.
         */
        frame_type type() const noexcept {
            return static_cast<frame_type>(buff_[3]);
        }

        /*!
         * @brief Set the frame type.
         * @param t The frame type.
         */
        void set_type(frame_type t) {
            buff_[3] = static_cast<uint8_t>(t);
        }

        /*!
         * @brief Get the frame flags.
         * @return The frame flags.
         */
        uint8_t flags() const {
            return buff_[4];
        }

        /*!
         * @brief Set the frame flags.
         * @param f The frame flags.
         */
        void set_flags(uint8_t f) {
            buff_[4] = f;
        }

        /*!
         * @brief Check if the reserved bit is set.
         * @return True if the reserved bit is set, otherwise
         * false.
         */
        bool reserved_bit() const {
            beu32 b_id{};
            std::memcpy(&b_id, &buff_[5], sizeof(beu32));
            uint32_t id = b_id;
            return bits::check<31>(id);
        }

        /*!
         * @brief Get the stream id in host byte order.
         * @return The stream id in host byte order.
         */
        std::uint32_t stream_id() const {
            beu32 b_id{};
            std::memcpy(&b_id, &buff_[5], sizeof(beu32));
            uint32_t id = b_id;
            bits::clear<31>(id);
            return id;
        }

        /*!
         * @brief Set the stream id in host byte order.
         * @param id The stream id in host byte order.
         */
        void set_stream_id(uint32_t id) {
            bits::clear<31>(id);
            beu32 b_id = id;
            std::memcpy(&buff_[5], &b_id, sizeof(beu32));
        }

        /*!
         * @brief Get the internal buffer to write the frame
         * into.
         * @return The internal buffer to write the frame into.
         * The size of the returned buffer is 9.
         */
        mutable_buffer get_buffer() {
            return buffer(buff_);
        }

        /*!
         * @brief Get the internal buffer to read the frame
         * from.
         * @return The internal buffer to read the frame from.
         * The size of the returned buffer is 9.
         */
        const_buffer get_buffer() const {
            return buffer(buff_);
        }

    private:
        uint8_t buff_[9] = {};
    };

    inline size_t frame_header::write_to_buffer(mutable_buffer out_buff) const {
        if (out_buff.size() < needed_write_size()) {
            return 0;
        }
        frame_header_buffer hbuff{*this};
        const_buffer in_buff = hbuff.get_buffer();
        std::memcpy(out_buff.data(), in_buff.data(), in_buff.size());
        return needed_write_size();
    }

    /*!
     * @brief HTTP 2 decoded RST_STREAM frame header and payload.
     *
     * Only stream_id and error_code members may be set.
     * Other members must not be changed.
     */
    struct rst_stream_frame : frame_header {
        /// The error code in host byte order.
        uint32_t error_code = 0;

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will always be 13 bytes.
         */
        static constexpr uint32_t needed_write_size() noexcept {
            return frame_header::needed_write_size() + sizeof(uint32_t);
        }

        /*!
         * @brief Write this RST_STREAM frame header and payload
         * to @p out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least 13.
         * @return The count of written bytes. It will be 13
         * bytes if the buffer size is not less than 13 or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const {
            if (needed_write_size() > out_buff.size()) {
                return 0;
            }
            const size_t total_size = out_buff.size();
            frame_header mut_header;
            mut_header.type = frame_type::rst_stream;
            mut_header.length = sizeof(std::uint32_t);
            mut_header.stream_id = stream_id;
            out_buff += mut_header.write_to_buffer(out_buff);
            beu32 berror = error_code;
            std::memcpy(out_buff.data(), &berror, sizeof(beu32));
            out_buff += sizeof(beu32);
            return total_size - out_buff.size();
        }

        /*!
         * @brief Write this RST_STREAM frame header and payload
         * to @p out_buff.
         * @param out_buff The output buffer where the encoded
         * frame will be appended.
         * @return The count of written bytes. It will be 13
         * bytes.
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        friend constexpr bool
        operator==(const rst_stream_frame& lhs,
                   const rst_stream_frame& rhs) noexcept = default;
    };

    /*!
     * @brief HTTP 2 decoded setting id and value.
     */
    struct setting_value {
        // The setting id (2 bytes) in host byte order.
        settings_id id = {};
        // The setting value (4 bytes) in host byte order.
        uint32_t value = 0;

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will always be 6 bytes.
         */
        static constexpr uint32_t needed_write_size() noexcept {
            return 6;
        }

        /*!
         * @brief Write this setting to @p out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least 6.
         * @return The count of written bytes. It will be 6
         * bytes if the buffer size is not less than 6 or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const;

        /*!
         * @brief Write this setting to @p out_buff.
         * @param out_buff The output buffer where the setting
         * will be appended.
         * @return The count of written bytes. It will be 6
         * bytes.
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        friend constexpr bool
        operator==(const setting_value& lhs,
                   const setting_value& rhs) noexcept = default;
    };

    /*!
     * @brief HTTP 2 encoded setting id and value.
     */
    class setting_value_buffer {
    public:
        /*!
         * @brief Make invalid setting id and value.
         */
        setting_value_buffer() = default;

        /*!
         * @brief Encode setting id and value.
         * @param s_id The setting id in host byte order.
         * @param val The setting value in host byte order.
         */
        setting_value_buffer(uint16_t s_id, uint32_t val) {
            set_id(s_id);
            set_value(val);
        }

        /*!
         * @brief Encode setting id and value.
         * @param svalue The setting id and value to encode.
         */
        setting_value_buffer(const setting_value& svalue)
            : setting_value_buffer(static_cast<uint16_t>(svalue.id),
                                   svalue.value) {
        }

        /*!
         * @brief Decode setting id and value.
         * @return The decoded setting id and value.
         */
        setting_value get_setting_value() const {
            return setting_value{
                static_cast<settings_id>(id()),
                value(),
            };
        }

        /*!
         * @brief Get the setting id in host byte order.
         * @return The setting id in host byte order.
         */
        uint16_t id() const {
            beu16 b_id{};
            std::memcpy(&b_id, &buff_[0], sizeof(beu16));
            return static_cast<uint16_t>(b_id);
        }

        /*!
         * @brief Set the setting id in host byte order.
         * @param s_id The setting id in host byte order.
         */
        void set_id(uint16_t s_id) {
            beu16 b_id = s_id;
            std::memcpy(&buff_[0], &b_id, sizeof(beu16));
        }

        /*!
         * @brief Get the setting value in host byte order.
         * @return The setting value in host byte order.
         */
        uint32_t value() const {
            beu32 b_val{};
            std::memcpy(&b_val, &buff_[2], sizeof(beu32));
            return b_val;
        }

        /*!
         * @brief Set the setting value in host byte order.
         * @param val The setting value in host byte order.
         */
        void set_value(uint32_t val) {
            beu32 b_val = val;
            std::memcpy(&buff_[2], &b_val, sizeof(beu32));
        }

        /*!
         * @brief Get the internal buffer to write the setting
         * into.
         * @return The internal buffer to write the setting
         * into. The size of the returned buffer is 6.
         */
        mutable_buffer get_buffer() {
            return buffer(buff_);
        }

        /*!
         * @brief Get the internal buffer to read the setting
         * from.
         * @return The internal buffer to read the setting from.
         * The size of the returned buffer is 6.
         */
        const_buffer get_buffer() const {
            return buffer(buff_);
        }

    private:
        uint8_t buff_[sizeof(uint16_t) + sizeof(uint32_t)];
    };

    inline size_t
    setting_value::write_to_buffer(mutable_buffer out_buff) const {
        if (out_buff.size() < 6) {
            return 0;
        }
        setting_value_buffer sbuff{static_cast<uint16_t>(id), value};
        const_buffer in_buff = sbuff.get_buffer();
        std::memcpy(out_buff.data(), in_buff.data(), 6);
        return 6;
    }

    /*!
     * @brief HTTP 2 decoded SETTINGS frame header and payload.
     *
     * Only settings_values member may be set.
     * Other members must not be changed.
     *
     * The settings payload is only considered for non ACK settings.
     */
    struct settings_frame : frame_header {
        /// The settings payload if ACK flag is not set.
        std::vector<setting_value> settings_values;

        /*!
         * @brief Check if this settings frame has ACK flag set.
         * @return True if this settings frame has ACK flag set,
         * otherwise false.
         */
        bool ack() const noexcept {
            return flags & 0x1;
        }

        /*!
         * @brief Set the ACK flag.
         */
        void set_ack() {
            flags |= 0x1;
        }

        /*!
         * @brief Clear the ACK flag.
         */
        void clear_ack() {
            flags &= ~0x1;
        }

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will always be 9 bytes if ACK flag is set.
         * Otherwise it will be 9 + 6 * settings_values.size().
         */
        uint32_t needed_write_size() const noexcept {
            if (ack()) {
                return frame_header::needed_write_size();
            }
            return frame_header::needed_write_size() +
                   6 * static_cast<uint32_t>(settings_values.size());
        }

        /*!
         * @brief Write this SETTINGS frame header and payload
         * to @p out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least this->needed_write_size().
         * @return The count of written bytes. It will be
         * this->needed_write_size() bytes if the buffer size is
         * not less than this->needed_write_size() or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const noexcept {
            if (needed_write_size() > out_buff.size()) {
                return 0;
            }
            const size_t total_size = out_buff.size();
            frame_header mut_header;
            mut_header.type = frame_type::settings;
            mut_header.length =
                needed_write_size() - frame_header::needed_write_size();
            mut_header.flags = flags & 0x1;
            out_buff += mut_header.write_to_buffer(out_buff);
            for (const auto& svalue : settings_values) {
                size_t n = svalue.write_to_buffer(out_buff);
                if (n == 0) {
                    return 0;
                }
                out_buff += n;
            }
            return total_size - out_buff.size();
        }

        /*!
         * @brief Write this SETTINGS frame header and payload
         * to @p out_buff.
         * @param out_buff The output buffer where the encoded
         * frame will be appended.
         * @return The count of written bytes. It will be
         * needed_write_size().
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        friend constexpr bool
        operator==(const settings_frame& lhs,
                   const settings_frame& rhs) noexcept = default;
    };

    /*!
     * @brief HTTP 2 decoded PING frame header and payload.
     *
     * Only opaque_data member may be set.
     * Other members must not be changed.
     */
    struct ping_frame : frame_header {
        /// The ping opaque data (8 bytes) in host byte order.
        std::uint64_t opaque_data = 0;

        /*!
         * @brief Check if this settings frame has ACK flag set.
         * @return True if this settings frame has ACK flag set,
         * otherwise false.
         */
        bool ack() const noexcept {
            return flags & 0x1;
        }

        /*!
         * @brief Set the ACK flag.
         */
        void set_ack() {
            flags |= 0x1;
        }

        /*!
         * @brief Clear the ACK flag.
         */
        void clear_ack() {
            flags &= ~0x1;
        }

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will always be 17 bytes.
         */
        static constexpr uint32_t needed_write_size() noexcept {
            return frame_header::needed_write_size() + sizeof(std::uint64_t);
        }

        /*!
         * @brief Write this PING frame header and payload to @p
         * out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least 17.
         * @return The count of written bytes. It will be 17
         * bytes if the buffer size is not less than 17 or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const {
            if (needed_write_size() > out_buff.size()) {
                return 0;
            }
            size_t total_size = out_buff.size();
            frame_header mut_header;
            mut_header.type = frame_type::ping;
            mut_header.length = sizeof(std::uint64_t);
            mut_header.flags = flags & 0x1;
            out_buff += mut_header.write_to_buffer(out_buff);
            beu64 bopaque_data = opaque_data;
            std::memcpy(out_buff.data(), &bopaque_data, sizeof(beu64));
            out_buff += sizeof(uint64_t);
            return total_size - out_buff.size();
        }

        /*!
         * @brief Write this PING frame header and payload to @p
         * out_buff.
         * @param out_buff The output buffer where the encoded
         * frame will be appended.
         * @return The count of written bytes. It will be 17
         * bytes.
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        friend constexpr bool
        operator==(const ping_frame& lhs,
                   const ping_frame& rhs) noexcept = default;
    };

    /*!
     * @brief HTTP 2 decoded GOAWAY frame header and payload.
     *
     * Only last_stream_id, error_code and debug_data members may be set.
     * Other members must not be changed.
     */
    struct goaway_frame : frame_header {
        /// The last stream id.
        std::uint32_t last_stream_id = 0;
        /// The error code.
        error error_code = error::no_error;
        /// The debug message.
        std::string debug_data;

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will be 9 + 8 + debug_data.size().
         */
        uint32_t needed_write_size() const {
            return frame_header::needed_write_size() +
                   (sizeof(std::uint32_t) * 2) +
                   static_cast<std::uint32_t>(debug_data.size());
        }

        /*!
         * @brief Write this GOAWAY frame header and payload to
         * @p out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least this->needed_write_size().
         * @return The count of written bytes. It will be
         * this->needed_write_size() bytes if the buffer size is
         * not less than this->needed_write_size() or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const {
            if (needed_write_size() > out_buff.size()) {
                return 0;
            }
            const size_t total_size = out_buff.size();
            frame_header mut_header;
            mut_header.type = frame_type::goaway;
            mut_header.length =
                needed_write_size() - frame_header::needed_write_size();
            out_buff += mut_header.write_to_buffer(out_buff);
            beu32 bid = last_stream_id;
            std::memcpy(out_buff.data(), &bid, sizeof(beu32));
            out_buff += sizeof(beu32);
            beu32 berror = static_cast<uint32_t>(error_code);
            std::memcpy(out_buff.data(), &berror, sizeof(beu32));
            out_buff += sizeof(beu32);
            if (!debug_data.empty()) {
                std::memcpy(out_buff.data(), debug_data.data(),
                            debug_data.size());
                out_buff += debug_data.size();
            }
            return total_size - out_buff.size();
        }

        /*!
         * @brief Write this GOAWAY frame header and payload to
         * @p out_buff.
         * @param out_buff The output buffer where the encoded
         * frame will be appended.
         * @return The count of written bytes. It will be
         * needed_write_size().
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        friend constexpr bool
        operator==(const goaway_frame& lhs,
                   const goaway_frame& rhs) noexcept = default;
    };

    /*!
     * @brief HTTP 2 decoded WINDOW_UPDATE frame header and payload.
     *
     * Only stream_id and window_size_increment members may be set.
     * Other members must not be changed.
     */
    struct window_update_frame : frame_header {
        std::uint32_t window_size_increment = 0;

        /*!
         * @brief Get the needed buffer size for serialization.
         * @return The needed buffer size for serialization.
         * The return will always be 13 bytes.
         */
        static constexpr uint32_t needed_write_size() noexcept {
            return frame_header::needed_write_size() + sizeof(uint32_t);
        }

        /*!
         * @brief Write this WINDOW_UPDATE frame header and
         * payload to
         * @p out_buff.
         * @param out_buff The output buffer. Its size must be
         * at least 13.
         * @return The count of written bytes. It will be 13
         * bytes if the buffer size is not less than 13 or 0 to
         * indicate not enough buffer space.
         */
        size_t write_to_buffer(mutable_buffer out_buff) const noexcept {
            if (needed_write_size() > out_buff.size()) {
                return 0;
            }
            const size_t total_size = out_buff.size();
            frame_header mut_header;
            mut_header.type = frame_type::window_update;
            mut_header.stream_id = stream_id;
            mut_header.length = sizeof(std::uint32_t);
            out_buff += mut_header.write_to_buffer(out_buff);
            beu32 bsize = window_size_increment;
            std::memcpy(out_buff.data(), &bsize, sizeof(beu32));
            out_buff += sizeof(beu32);
            return total_size - out_buff.size();
        }

        /*!
         * @brief Write this WINDOW_UPDATE frame header and
         * payload to
         * @p out_buff.
         * @param out_buff The output buffer where the encoded
         * frame will be appended.
         * @return The count of written bytes. It will be 13
         * bytes.
         */
        std::size_t write_to_buffer(dynamic_buffer out_buff) const {
            auto write_buff = out_buff.prepare(needed_write_size());
            return write_to_buffer(write_buff);
        }

        friend constexpr bool
        operator==(const window_update_frame& lhs,
                   const window_update_frame& rhs) noexcept = default;
    };

    /*!
     * @brief The configuration options for HTTP 2 endpoints.
     */
    struct endpoint_config {
        /*!
         * @brief The value of SETTINGS_HEADER_TABLE_SIZE
         * setting (default is 4096). It applies to the dynamic
         * table used by the HPACK. Sender must set its decoder
         * max dynamic table size to this value. Receiver must
         * set its encoder max dynamic table size to a value
         * less than or equal to this value.
         */
        std::uint32_t header_table_size = default_header_table_size;

        /*!
         * @brief The value of SETTINGS_MAX_CONCURRENT_STREAMS
         * setting (default is unlimited). It specifies the max
         * concurrent streams the sender will allow. Sender may
         * refuse streams above this value by sending a
         * RST_STREAM frame. Receiver must not open concurrent
         * streams more than this value.
         */
        std::uint32_t max_concurrent_streams =
            std::numeric_limits<std::uint32_t>::max();

        /*!
         * @brief The value of SETTINGS_INITIAL_WINDOW_SIZE
         * setting (default is 2^16 - 1 = 65535). It indicates
         * the sender's initial window size for stream-level
         * flow control. This setting affects the window size of
         * all streams. The maximum value is 2^31 - 1 and values
         * greater than this must be treated as a connection
         * error of type FLOW_CONTROL_ERROR.
         */
        std::uint32_t initial_window_size =
            std::numeric_limits<std::uint16_t>::max();

        /*!
         * @brief The value of SETTINGS_MAX_FRAME_SIZE setting
         * (default is 2^14 = 16384). The minimum value is 2^14
         * (16384)  and the maximum value is 2^24 - 1 (16777215)
         * and values outside this range are treated as a
         * connection error of type PROTOCOL_ERROR. Sender must
         * treat payloads with size larger than this value as a
         * connection error of type PROTOCOL_ERROR. Recevier
         * must not send frame payloads larger than this value.
         */
        std::uint32_t max_frame_size = min_payload_size;

        /*!
         * @brief The value of SETTINGS_MAX_HEADER_LIST_SIZE
         * setting (default is unlimited). This value specifies
         * the maximum size of headers after decompression the
         * sender will accept. The size includes the name and
         * value plus 32 for each line. Sender should treat
         * decompressed headers larger than this value as a
         * stream error of type PROTOCOL_ERROR. Receiver should
         * not send headers larger than this value.
         */
        std::uint32_t max_header_list_size =
            std::numeric_limits<std::uint32_t>::max();

        /*!
         * @brief The value of SETTINGS_ENABLE_PUSH setting
         * (default is true). The server must not send this
         * setting, and after receiving false enable_push must
         * not send PUSH_PROMISE frames to the client. If a
         * client sent false enable_push to the server and had
         * it acknowledged, then it received a PUSH_PROMISE
         * frame, then it must treat it as a connection error of
         * type PROTOCOL_ERROR.
         */
        bool enable_push = true;

        friend constexpr bool
        operator==(const endpoint_config& lhs,
                   const endpoint_config& rhs) noexcept = default;
    };

    /*!
     * @brief The timeout options for HTTP 2 endpoints.
     */
    struct endpoint_timeout {
        /*!
         * @brief This is the amount of time after which a handshake will time
         * out.
         *
         * The default handshake timeout is 5 seconds.
         */
        std::chrono::milliseconds handshake_timeout = std::chrono::seconds{5};
        /*!
         * @brief If no data is received from the peer for a time equal
         * to the idle timeout, then the connection will time out.
         *
         * The default idle timeout is 30 seconds.
         */
        std::chrono::milliseconds idle_timeout = std::chrono::seconds{30};
        /*!
         * @brief If the idle timeout is enabled, then the value of this setting
         * controls whether or not a ping frame will be sent to the peer if no
         * data is received for half of the idle timeout interval.
         *
         * This option is enabled by default.
         */
        bool keep_alive_pings = true;
    };

    /*!
     * @brief Check if HTTP 2 headers contain forbidden characters.
     * If any pseudo header exists in @p hdrs, false will be returned
     * because ':' is not allowed in headers names.
     * @param hdrs The headers to check.
     * @return True if HTTP 2 headers don't contain forbidden characters,
     * otherwise false.
     */
    RAD_EXPORT_DECL bool validate_headers(const headers& hdrs);

    /*!
     * @brief Check if HTTP 2 headers contain forbidden characters.
     * If any pseudo header exists in @p hdrs, false will be returned
     * because ':' is not allowed in headers names.
     * @param hdrs The headers to check.
     * @return True if HTTP 2 headers don't contain forbidden characters,
     * otherwise false.
     */
    RAD_EXPORT_DECL bool validate_headers(const headers_view& hdrs);

    /*!
     * @brief Encode HTTP request using HPACK encoder, and write the result
     * to @p write_buff, then prepare @p frames_buffs so that it contains
     * successive pairs of frame header buffer followed by payload buffer.
     *
     * If the entire headers block data size doesn't exceed @p
     * max_headers_block_size then a single HEADERS frame will be produced
     * and @p frames_buffs will contain two buffers: the HEADERS frame
     * header buffer followed by payload buffer.
     *
     * Otherwise, as many CONTINUATION frames as necessary are created to
     * carry the headers block data with the last CONTINUATION frame having
     * END_HEADERS flag set. In this case, @p frames_buffs will contain
     * after the two HEADERS buffers a successive pairs of CONTINUATION
     * frame header buffer followed by payload buffer.
     * @param id The stream id.
     * @param method The request method.
     * @param path The request path.
     * @param hdrs The request headers.
     * @param empty_body If true, the created HEADERS frame will have
     * END_STREAM flag set.
     * @param max_headers_block_size The maximum size of headers block per
     * frame.
     * @param hencoder The HPACK encoder.
     * @param write_buff The out buffer where frames headers and encoded
     * payloads will be written. Initial content of this buffer is cleared.
     * @param frames_buffs The successive pairs of frame header buffer
     * followed by payload buffer. All the buffers in this list is pointing
     * to @p write_buff internal storage and will be invalidated if @p
     * write_buff storage is cleared, resized or destroyed.
     */
    RAD_EXPORT_DECL void make_headers_continuations_frames(
        uint32_t id, verb method, std::string_view path, const headers& hdrs,
        bool empty_body, uint32_t max_headers_block_size,
        hpack_encoder& hencoder, std::vector<uint8_t>& write_buff,
        std::vector<const_buffer>& frames_buffs);

    /*!
     * @brief Write frame headers from @p data_frames in order to @p
     * headers_write_buff, then prepare @p frames_buffs so that it contains
     * successive pairs of frame header buffer followed by payload buffer.
     * @param data_frames The input pairs of frame header and payload.
     * @param headers_write_buff The out buffer where frames headers will be
     * written.
     * @param data_frames_buffs The successive pairs of frame header buffer
     * followed by payload buffer. Frame headers buffers in this list is
     * pointing to @p headers_write_buff internal storage, While payload
     * buffers are the same payload buffers in @p data_frames. Initial
     * content of this buffer is cleared.
     */
    RAD_EXPORT_DECL void make_frames_buffers(
        std::span<const std::pair<frame_header, const_buffer>> data_frames,
        std::vector<uint8_t>& headers_write_buff,
        std::vector<const_buffer>& data_frames_buffs);

    /*!
     * @brief Get the header list size according to the rules of
     * SETTINGS_MAX_HEADER_LIST_SIZE value calculation.
     * @param hdrs The header list.
     * @return The header list size.
     */
    RAD_EXPORT_DECL std::uint32_t get_header_list_size(const headers& hdrs);

    /*!
     * @brief Get the header list size according to the rules of
     * SETTINGS_MAX_HEADER_LIST_SIZE value calculation.
     * @param hdrs The header list.
     * @return The header list size.
     */
    RAD_EXPORT_DECL std::uint32_t
    get_header_list_size(const headers_view& hdrs);

    /*!
     * @brief Compare the settings in @p cfg with the settings in @p old_cfg
     * and different settings will be appended to the result SETTINGS frame.
     * @param cfg The new settings.
     * @param old_cfg The old settings.
     * @return The SETTINGS frame containing the changed settings.
     * If not changes, the result frame will be empty.
     */
    RAD_EXPORT_DECL settings_frame make_settings_frame(
        const endpoint_config& cfg, const endpoint_config& old_cfg);
} // namespace RAD_LIB_NAMESPACE::net::http2
