#pragma once
#include <rad/libbase.h>

#include <chrono>

namespace RAD_LIB_NAMESPACE::io {

    enum class baud_rate : uint32_t {
        b110 = 110,
        b300 = 300,
        b600 = 600,
        b1200 = 1200,
        b2400 = 2400,
        b4800 = 4800,
        b9600 = 9600,
        b14400 = 14400,
        b19200 = 19200,
        b38400 = 38400,
        b57600 = 57600,
        b115200 = 115200,
        b128000 = 128000,
        b256000 = 256000,
    };

    class serial_baud_rate {
    public:
        serial_baud_rate() = default;

        serial_baud_rate(baud_rate rate) noexcept : rate_{rate} {
        }

        serial_baud_rate(uint32_t rate) noexcept {
            rate_ = static_cast<baud_rate>(rate);
            is_custom_ = true;
        }

        bool is_standard() const noexcept {
            return !is_custom_;
        }

        baud_rate rate() const noexcept {
            return rate_;
        }

    private:
        baud_rate rate_ = baud_rate::b9600;
        bool is_custom_ = false;
    };

    enum class serial_bits_per_byte : uint8_t {
        bits8 = 8,
        bits7 = 7,
        bits6 = 6,
        bits5 = 5,
    };

    enum class serial_flags : uint32_t {
        none = 0,
        parity = 1 << 0,
        two_stop_bits = 1 << 1,
        rts_flow_control = 1 << 2,
        x_output_flow_control = 1 << 3,
        x_input_flow_control = 1 << 4,
        cts_output_flow_control = 1 << 5,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(serial_flags);

    enum class serial_parity : uint8_t {
        none = 0,
        odd = 1,
        even = 2,
        mark = 3,
        space = 4,
    };

    struct serial_options {
        serial_flags flags = serial_flags::none;
        serial_baud_rate baud_rate;
        serial_bits_per_byte bits_per_byte = serial_bits_per_byte::bits8;
        serial_parity parity = serial_parity::none;
    };

    enum class serial_access : uint8_t {
        read,
        write,
        read_write,
    };
} // namespace RAD_LIB_NAMESPACE::io