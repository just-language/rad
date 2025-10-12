#include <rad/crypto/base64.h>

#include <algorithm>

using namespace RAD_LIB_NAMESPACE;

namespace {
    constexpr std::size_t decoded_group_size = 3;
    constexpr std::size_t encoded_group_size = 4;
    constexpr std::size_t base64_table_size = 64;

    using encoded_group_t = std::array<uint8_t, encoded_group_size>;
    using decoded_group_t = std::array<uint8_t, decoded_group_size>;
} // namespace

void base64::encode(const_buffer input, dynamic_buffer output) {
    std::size_t groups_n = input.size() / decoded_group_size;
    std::size_t last_group_size = input.size() - groups_n * decoded_group_size;
    std::size_t last_padded_group_size =
        last_group_size ? encoded_group_size : 0;

    if (!groups_n && !last_group_size) {
        return;
    }

    std::size_t encoding_size =
        groups_n * encoded_group_size + last_padded_group_size;
    auto encoding_buffer = output.prepare(encoding_size);

    auto raw_groups = input.to_span<const decoded_group_t>(groups_n);
    auto encoding_groups = encoding_buffer.to_span<encoded_group_t>(groups_n);

    constexpr std::array<uint8_t, base64_table_size> base64_table = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

    for (auto i : range(groups_n)) {
        encoding_groups[i][0] = base64_table[raw_groups[i][0] >> 2];
        encoding_groups[i][1] =
            base64_table[(raw_groups[i][1] >> 4) |
                         ((raw_groups[i][0] & 0b00000011) << 4)];
        encoding_groups[i][2] =
            base64_table[(raw_groups[i][2] >> 6) |
                         ((raw_groups[i][1] & 0b00001111) << 2)];
        encoding_groups[i][3] = base64_table[raw_groups[i][2] & 0b00111111];
    }

    if (!last_group_size) {
        return;
    }

    auto last_encoding_group =
        encoding_buffer
            .sub_buffer(encoding_buffer.size() - last_padded_group_size)
            .to_span<uint8_t>();
    auto last_raw_group =
        input.sub_buffer(input.size() - last_group_size).to_span<uint8_t>();

    if (last_group_size == 1) {
        last_encoding_group[0] = base64_table[last_raw_group[0] >> 2u];
        last_encoding_group[1] =
            base64_table[((size_t)last_raw_group[0] & 0b00000011u) << 4u];
        last_encoding_group[2] = '=';
        last_encoding_group[3] = '=';
    }
    else {
        last_encoding_group[0] = base64_table[last_raw_group[0] >> 2u];
        last_encoding_group[1] =
            base64_table[((last_raw_group[0] & 0b00000011u) << 4u) |
                         (last_raw_group[1] >> 4u)];
        last_encoding_group[2] =
            base64_table[((size_t)last_raw_group[1] & 0b00001111u) << 2u];
        last_encoding_group[3] = '=';
    }
}

void base64::decode(const_buffer encoded, dynamic_buffer output) {
    if (encoded.empty()) {
        return;
    }

    if (encoded.size() % encoded_group_size != 0) {
        throw std::system_error(
            std::make_error_code(std::errc::invalid_argument));
    }

    std::size_t groups_n = encoded.size() / encoded_group_size;

    std::size_t old_out_size = output.size();
    std::size_t decoded_size = groups_n * decoded_group_size;
    auto out_buff = buffer(output.increase_size(decoded_size), decoded_size);

    auto encoded_groups = encoded.to_span<const encoded_group_t>(groups_n);
    auto decoded_groups = out_buff.to_span<decoded_group_t>(groups_n);

    // the size of output (not input) processed size
    std::size_t processed_size = 0;

    auto index_of = [&](uint8_t ch) -> uint8_t {
        constexpr uint8_t a_index = 26;
        constexpr uint8_t zero_index = 52;

        if (ch >= 'A' && ch <= 'Z') {
            return ch - 'A';
        }
        if (ch >= 'a' && ch <= 'z') {
            return ch - 'a' + a_index;
        }
        if (ch >= '0' && ch <= '9') {
            return ch - '0' + zero_index;
        }
        if (ch == '+') {
            return 62;
        }
        if (ch == '/') {
            return 63;
        }
        if (ch == '=') {
            return 0;
        }

        throw std::system_error(
            std::make_error_code(std::errc::invalid_argument));
    };

    for (auto i : range(groups_n)) {
        auto& dec_group = decoded_groups[i];
        auto& enc_group = encoded_groups[i];

        encoded_group_t base64_indexes;
        base64_indexes[0] = index_of(enc_group[0]);
        base64_indexes[1] = index_of(enc_group[1]);
        base64_indexes[2] = index_of(enc_group[2]);
        base64_indexes[3] = index_of(enc_group[3]);

        // if there is '=' consider this is the end of the input and
        // stop here
        auto it = std::find(enc_group.begin(), enc_group.end(), '=');
        if (it != enc_group.end()) {
            std::size_t encoded_bytes =
                static_cast<std::size_t>(std::distance(enc_group.begin(), it));

            switch (encoded_bytes) {
            case 0:
                break;
            case 1:
                dec_group[0] = base64_indexes[0] << 2u;
                ++processed_size;
                break;
            case 2:
                dec_group[0] = (base64_indexes[0] << 2u) |
                               ((base64_indexes[1] & 0b00110000u) >> 4u);
                ++processed_size;
                break;
            case 3:
                dec_group[0] = (base64_indexes[0] << 2u) |
                               ((base64_indexes[1] & 0b00110000u) >> 4u);
                dec_group[1] = (base64_indexes[1] << 4u) |
                               ((base64_indexes[2] & 0b00111100u) >> 2u);
                processed_size += 2;
                break;
            default:
                break;
            }
            // break from the loop
            break;
        }

        processed_size += dec_group.size();

        dec_group[0] = (base64_indexes[0] << 2u) |
                       ((base64_indexes[1] & 0b00110000u) >> 4u);
        dec_group[1] = (base64_indexes[1] << 4u) |
                       ((base64_indexes[2] & 0b00111100u) >> 2u);
        dec_group[2] = (base64_indexes[2] << 6u) |
                       ((base64_indexes[3] & 0b00111111u) >> 0u);
    }

    output.resize(old_out_size + processed_size);
}