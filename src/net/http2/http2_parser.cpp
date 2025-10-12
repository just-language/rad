#include <rad/net/http2/http2_parser.h>

#include "http2_debug.h"

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace http2;

namespace {
    constexpr auto errors_strs = std::array{
        "No Error",

        "The endpoint detected an unspecific protocol error",
        "The endpoint encountered an unexpected internal error",
        "The endpoint detected that its peer violated the flow-control "
        "protocol",
        "The endpoint sent a SETTINGS frame but did not receive a response "
        "in a "
        "timely manner",
        "The endpoint received a frame after a stream was half-closed",
        "The endpoint received a frame with an invalid size",
        "The endpoint refused the stream prior to performing any "
        "application "
        "processing",
        "The stream is no longer needed",
        "The endpoint is unable to maintain the field section compression "
        "context "
        "for the connection",
        "The connection established in response to a CONNECT request was "
        "reset or "
        "abnormally closed",
        "The endpoint detected that its peer is exhibiting a behavior that "
        "might "
        "be generating excessive load",
        "The underlying transport has properties that do not meet minimum "
        "security "
        "requirements",
        "The endpoint requires that HTTP/1.1 be used instead of HTTP/2"};

    struct error_category_impl : public std::error_category {
        const char* name() const noexcept override {
            return "http2";
        }

        std::string message(int err) const override {
            uint32_t uerr = static_cast<uint32_t>(err);
            if (uerr < errors_strs.size()) {
                return std::string{errors_strs[uerr]};
            }
            return "unknown http2 error";
        }
    };

    const error_category_impl error_cat_inst;

    bool validate_header_field(std::string_view field) {
        if (field.empty()) {
            return false;
        }
        bool first_ch = true;
        for (auto ch : field) {
            if ((ch >= 0x0 && ch <= 0x20) || (ch >= 0x41 && ch <= 0x5a) ||
                (ch >= 0x7f)) {
                return false;
            }
            if (ch >= 'A' && ch <= 'Z') {
                return false;
            }
            if (ch == ':' && !first_ch) {
                return false;
            }
            first_ch = false;
        }
        if (field.starts_with("connection-") || field == "connection") {
            return false;
        }
        if (field == "proxy-connection" || field == "keep-alive" ||
            field == "transfer-encoding" || field == "upgrade") {
            return false;
        }
        return true;
    }

    bool validate_header_value(std::string_view value) {
        if (value.empty()) {
            return true;
        }
        if (value.front() == ' ' || value.back() == ' ') {
            return false;
        }
        if (value.front() == '\t' || value.back() == '\t') {
            return false;
        }
        for (auto ch : value) {
            if (ch == 0 || ch == 0x0a || ch == 0x0d) {
                return false;
            }
        }
        return true;
    }

    bool validate_header(std::string_view field, std::string_view value) {
        return validate_header_field(field) && validate_header_value(value);
    }
} // namespace

const std::error_category& http2::http2_category() noexcept {
    return error_cat_inst;
}

bool frame_header::is_valid_stream_id(
    bool from_client, std::uint32_t last_client_id,
    std::uint32_t last_server_id) const noexcept {
    /*
     * Streams initiated by a client MUST use odd-numbered stream
     * identifiers; those initiated by the server MUST use even-numbered
     * stream identifiers. A stream identifier of zero (0x00) is used for
     * connection control messages; the stream identifier of zero cannot be
     * used to establish a new stream.
     *
     * The identifier of a newly established stream MUST be numerically
     * greater than all streams that the initiating endpoint has opened or
     * reserved. This governs streams that are opened using a HEADERS frame
     * and streams that are reserved using PUSH_PROMISE. An endpoint that
     * receives an unexpected stream identifier MUST respond with a
     * connection error (Section 5.4.1) of type PROTOCOL_ERROR.
     */
    if (stream_id == 0) {
        if (is_stream_frame()) {
            http2_printf("(http2) Received stream-level frame with "
                         "type: %d with zero id !\n",
                         (int)type);
            return false;
        }
        return true;
    }
    if (is_connection_frame()) {
        http2_printf("(http2) Received connection-level frame with "
                     "type: %d with "
                     "non zero id: %d !\n",
                     (int)type, (int)stream_id);
        return false;
    }
    if (stream_id > max_stream_id) {
        return false;
    }

    const bool is_new_id = (from_client && type == frame_type::headers) ||
                           (!from_client && type == frame_type::push_promise);
    const std::uint32_t last_id = from_client ? last_server_id : last_client_id;
    if (is_new_id) {
        const std::uint32_t next_id = last_id + 2;
        if (next_id > stream_id || next_id < last_id) {
            return false;
        }
        if (stream_id != next_id) {
            return false;
        }
        const bool is_odd = stream_id % 2 != 0;
        if (is_odd != from_client) {
            return false;
        }
    }
    else if (stream_id > last_id) {
        return false;
    }
    return true;
}

bool frame_header::is_valid_frame(
    const std::optional<frame_header>& previous_header,
    std::uint32_t max_frame_payload_size, bool push_allowed, bool from_client,
    std::uint32_t last_client_id, std::uint32_t last_server_id) const noexcept {
    if (!is_valid_type(from_client, push_allowed)) {
        http2_printf("(http2) Received invalid frame type: %d !\n", type);
        return false;
    }
    if (!is_valid_flags()) {
        http2_printf("(http2) Received frame type: %d with id: %d with "
                     "undefined "
                     "flags: %d!\n",
                     (int)type, (int)stream_id, (int)flags);
        return false;
    }
    if (!is_valid_size(max_frame_payload_size)) {
        http2_printf("(http2) Received frame type: %d with id: %d with "
                     "invalid size: %d!\n",
                     (int)type, (int)stream_id, (int)length);
        return false;
    }
    if (!is_valid_stream_id(from_client, last_client_id, last_server_id)) {
        return false;
    }
    // the first frame must be settings frame
    if (!previous_header.has_value()) {
        return type == frame_type::settings;
    }
    // check if expected continuation frame
    const bool was_end_headers =
        (previous_header->flags & END_HEADERS_FLAG) == END_HEADERS_FLAG;
    if (!was_end_headers &&
        (previous_header->type == frame_type::headers ||
         previous_header->type == frame_type::continuation)) {
        if (type != frame_type::continuation ||
            stream_id != previous_header->stream_id) {
            http2_printf("(http2) Expected CONTINUATION with id: %d, "
                         "but received frame type: %d with id: %d !\n",
                         (int)previous_header->stream_id, (int)type,
                         (int)stream_id);
            return false;
        }
    }
    else if (type == frame_type::continuation) {
        http2_printf("(http2) Received unexpected CONTINUATION frame "
                     "with id %d !\n",
                     (int)stream_id);
        return false;
    }
    return true;
}

bool http2::validate_headers(const headers& hdrs) {
    for (const auto& [name, value] : hdrs) {
        if (!validate_header(name, value)) {
            return false;
        }
    }
    return true;
}

bool http2::validate_headers(const headers_view& hdrs) {
    for (const auto& [name, value] : hdrs) {
        if (!validate_header(name, value)) {
            return false;
        }
    }
    return true;
}

std::uint32_t http2::get_header_list_size(const headers& hdrs) {
    std::size_t n = hdrs.size() * 32;
    for (const auto& [k, v] : hdrs) {
        n += k.size() + v.size();
    }
    return static_cast<uint32_t>(n);
}

std::uint32_t http2::get_header_list_size(const headers_view& hdrs) {
    std::size_t n = hdrs.size() * 32;
    for (const auto& [k, v] : hdrs) {
        n += k.size() + v.size();
    }
    return static_cast<uint32_t>(n);
}

void http2::make_headers_continuations_frames(
    uint32_t id, verb method, std::string_view path, const headers& hdrs,
    bool empty_body, uint32_t max_headers_block_size, hpack_encoder& hencoder,
    std::vector<uint8_t>& write_buff, std::vector<const_buffer>& frames_buffs) {
    frames_buffs.clear();
    write_buff.clear();
    max_headers_block_size = std::max(max_headers_block_size, uint32_t{1});

    hencoder.encode(method, path, hdrs, dynamic_buffer(write_buff), true, true);
    if (write_buff.size() < max_headers_block_size) {
        // only one headers frame
        std::uint8_t flags = END_HEADERS_FLAG;
        if (empty_body) {
            flags |= END_STREAM_FLAG;
        }
        frame_header fh;
        fh.stream_id = id;
        fh.type = frame_type::headers;
        fh.flags = flags;
        fh.length = static_cast<std::uint32_t>(write_buff.size());
        const frame_header_buffer hbuff{fh};
        auto encoded_header_buff = hbuff.get_buffer().to_span<const uint8_t>();
        write_buff.insert(write_buff.begin(), encoded_header_buff.begin(),
                          encoded_header_buff.end());

        frames_buffs.clear();
        frames_buffs.reserve(2);
        frames_buffs.emplace_back(
            buffer(write_buff, frame_header::needed_write_size()));
        frames_buffs.emplace_back(buffer(write_buff) +
                                  frame_header::needed_write_size());
    }
    else {
        // a headers frame followed by continuation frames
        const std::size_t frames_count =
            (write_buff.size() / max_headers_block_size) +
            (write_buff.size() % max_headers_block_size != 0);
        assert(frames_count > 1);
        http2_printf("(http2) Dividing the request of stream (%d) "
                     "into headers multiple %d continuation frames\n",
                     (int)id, (int)frames_count);

        // make space for the frames headers
        const std::size_t frames_headers_size =
            frames_count * frame_header::needed_write_size();
        write_buff.insert(write_buff.begin(), frames_headers_size, 0);

        // write headers and split buffers
        mutable_buffer headers_buff = buffer(write_buff, frames_headers_size);
        const_buffer hpack_buff = buffer(write_buff) + frames_headers_size;

        auto insert_header_and_payload = [&](const frame_header& fh) {
            const std::size_t n = fh.write_to_buffer(headers_buff);
            assert(n > 0);
            frames_buffs.emplace_back(headers_buff.sub_buffer(0, n));
            frames_buffs.emplace_back(hpack_buff.sub_buffer(0, fh.length));
            headers_buff += n;
            hpack_buff += fh.length;
        };

        // first headers frame
        {
            frame_header fh;
            fh.stream_id = id;
            fh.type = frame_type::headers;
            fh.flags =
                empty_body
                    ? static_cast<uint8_t>(headers_frame_flags::end_stream)
                    : 0;
            fh.length = max_headers_block_size;
            insert_header_and_payload(fh);
        }

        // continuation frames
        while (!hpack_buff.empty()) {
            const size_t write_size =
                std::min(size_t{max_headers_block_size}, hpack_buff.size());
            const bool last_continuation = write_size == hpack_buff.size();
            frame_header fh;
            fh.stream_id = id;
            fh.type = frame_type::continuation;
            fh.flags = last_continuation ? END_HEADERS_FLAG : 0;
            fh.length = static_cast<uint32_t>(write_size);
            insert_header_and_payload(fh);
        }
    }
}

void http2::make_frames_buffers(
    std::span<const std::pair<frame_header, const_buffer>> data_frames,
    std::vector<uint8_t>& headers_write_buff,
    std::vector<const_buffer>& data_frames_buffs) {
    data_frames_buffs.clear();
    headers_write_buff.resize(data_frames.size() *
                              frame_header::needed_write_size());
    if (data_frames.empty()) {
        return;
    }
    mutable_buffer headers_buff = buffer(headers_write_buff);
    for (const auto& [fh, buff] : data_frames) {
        data_frames_buffs.push_back(
            headers_buff.sub_buffer(0, frame_header::needed_write_size()));
        headers_buff += fh.write_to_buffer(headers_buff);
        data_frames_buffs.push_back(buff);
    }
}

settings_frame http2::make_settings_frame(const endpoint_config& cfg,
                                          const endpoint_config& old_cfg) {
    settings_frame frame;
    frame.clear_ack();
    if (old_cfg.header_table_size != cfg.header_table_size) {
        frame.settings_values.emplace_back(setting_value{
            settings_id::header_table_size, cfg.header_table_size});
    }
    if (old_cfg.initial_window_size != cfg.initial_window_size) {
        frame.settings_values.emplace_back(setting_value{
            settings_id::initial_window_size, cfg.initial_window_size});
    }
    if (old_cfg.max_concurrent_streams != cfg.max_concurrent_streams) {
        frame.settings_values.emplace_back(setting_value{
            settings_id::max_concurrent_streams, cfg.max_concurrent_streams});
    }
    if (old_cfg.max_frame_size != cfg.max_frame_size) {
        frame.settings_values.emplace_back(
            setting_value{settings_id::max_frame_size, cfg.max_frame_size});
    }
    if (old_cfg.max_header_list_size != cfg.max_header_list_size) {
        frame.settings_values.emplace_back(setting_value{
            settings_id::max_header_list_size, cfg.max_header_list_size});
    }
    if (old_cfg.enable_push != cfg.enable_push) {
        frame.settings_values.emplace_back(setting_value{
            settings_id::enable_push, static_cast<uint32_t>(cfg.enable_push)});
    }
    return frame;
}