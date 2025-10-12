#include <Windows.h>
#include <rad/io/windows/serial_port_impl.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

namespace {
    serial_baud_rate convert_baud_rate(DWORD r) {
        switch (r) {
        case CBR_110:
            return baud_rate::b110;
        case CBR_300:
            return baud_rate::b300;
        case CBR_600:
            return baud_rate::b600;
        case CBR_1200:
            return baud_rate::b1200;
        case CBR_2400:
            return baud_rate::b2400;
        case CBR_4800:
            return baud_rate::b4800;
        case CBR_9600:
            return baud_rate::b9600;
        case CBR_14400:
            return baud_rate::b14400;
        case CBR_19200:
            return baud_rate::b19200;
        case CBR_38400:
            return baud_rate::b38400;
        case CBR_57600:
            return baud_rate::b57600;
        case CBR_115200:
            return baud_rate::b115200;
        case CBR_128000:
            return baud_rate::b128000;
        case CBR_256000:
            return baud_rate::b256000;
        default:
            return serial_baud_rate{r};
        }
    }

    DWORD convert_baud_rate(const serial_baud_rate& r) {
        return static_cast<uint32_t>(r.rate());
    }

    serial_bits_per_byte convert_bits_per_byte(BYTE bpb) {
        return static_cast<serial_bits_per_byte>(bpb);
    }

    BYTE convert_bits_per_byte(serial_bits_per_byte bpb) {
        return static_cast<BYTE>(bpb);
    }

    void get_flags_from_dcb(const DCB& dcb, serial_flags& flags) {
        flags = serial_flags::none;
        if (dcb.fParity) {
            flags |= serial_flags::parity;
        }
        if (dcb.StopBits != ONESTOPBIT) {
            flags |= serial_flags::two_stop_bits;
        }
        if (dcb.fRtsControl == RTS_CONTROL_ENABLE) {
            flags |= serial_flags::rts_flow_control;
        }
        if (dcb.fInX) {
            flags |= serial_flags::x_input_flow_control;
        }
        if (dcb.fOutX) {
            flags |= serial_flags::x_output_flow_control;
        }
        if (dcb.fOutxCtsFlow) {
            flags |= serial_flags::cts_output_flow_control;
        }
    }

    void set_flags_from_opts(DCB& dcb, serial_flags flags) {
        dcb.fParity = static_cast<bool>(flags & serial_flags::parity);
        dcb.StopBits =
            (flags & serial_flags::two_stop_bits) ? TWOSTOPBITS : ONESTOPBIT;
        dcb.fRtsControl = (flags & serial_flags::rts_flow_control)
                              ? RTS_CONTROL_ENABLE
                              : RTS_CONTROL_DISABLE;
        dcb.fInX =
            static_cast<bool>(flags & serial_flags::x_input_flow_control);
        dcb.fOutX =
            static_cast<bool>(flags & serial_flags::x_output_flow_control);
        dcb.fOutxCtsFlow =
            static_cast<bool>(flags & serial_flags::cts_output_flow_control);
    }

    DWORD convert_access(serial_access access) {
        switch (access) {
        case serial_access::read:
            return GENERIC_READ;
        case serial_access::write:
            return GENERIC_WRITE;
        case serial_access::read_write:
            return GENERIC_READ | GENERIC_WRITE;
        default:
            return GENERIC_READ | GENERIC_WRITE;
        }
    }
} // namespace

void serial_port_impl::open(native_string_type path, serial_access access,
                            std::error_code& ec) {
    ec.clear();
    if (path.empty()) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
    }

    std::wstring prefixed_path;
    if (!path.starts_with(path_prefix)) {
        prefixed_path = path_prefix + path;
        path = prefixed_path;
    }

    HANDLE handle =
        ::CreateFileW(path.data(), convert_access(access), 0, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        ec = os::make_system_error(::GetLastError());
        return;
    }

    assert(path.starts_with(path_prefix));
    path.remove_prefix(path_prefix.size());

    auto serial_handle = native_handle_type{handle};
    auto serial_path = std::wstring{path};
    set_handle_path(serial_handle, serial_path, ec);
}

serial_options serial_port_impl::get_options(std::error_code& ec) noexcept {
    ec.clear();
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!::GetCommState(native_handle().get(), &dcb)) {
        ec = os::make_system_error(::GetLastError());
        return {};
    }
    serial_options opts;
    opts.baud_rate = convert_baud_rate(dcb.BaudRate);
    opts.bits_per_byte = convert_bits_per_byte(dcb.ByteSize);
    opts.parity = static_cast<serial_parity>(dcb.Parity);
    get_flags_from_dcb(dcb, opts.flags);
    return opts;
}

void serial_port_impl::set_options(const serial_options& opts,
                                   std::error_code& ec) noexcept {
    ec.clear();
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!::GetCommState(native_handle().get(), &dcb)) {
        ec = os::make_system_error(::GetLastError());
        return;
    }
    dcb.BaudRate = convert_baud_rate(opts.baud_rate);
    dcb.ByteSize = convert_bits_per_byte(opts.bits_per_byte);
    dcb.Parity = static_cast<uint8_t>(opts.parity);
    set_flags_from_opts(dcb, opts.flags);
    if (!::SetCommState(native_handle().get(), &dcb)) {
        ec = os::make_system_error(::GetLastError());
    }
}