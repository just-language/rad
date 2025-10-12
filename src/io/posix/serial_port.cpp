#include <rad/io/posix/serial_port_impl.h>
#ifdef __linux__
#include <asm/termbits.h>
#else
#include <termios.h>
#endif // __linux__
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

using namespace RAD_LIB_NAMESPACE;
using namespace io;
using namespace io::detail;

#ifdef __linux__
using termios_struct_t = struct termios2;
#else
using termios_struct_t = struct termios;
constexpr uint32_t CMSPAR = 0;
#endif // __linux__

namespace {
    serial_baud_rate convert_baud_rate(speed_t r) {
        switch (r) {
        case B110:
            return baud_rate::b110;
        case B300:
            return baud_rate::b300;
        case B600:
            return baud_rate::b600;
        case B1200:
            return baud_rate::b1200;
        case B2400:
            return baud_rate::b2400;
        case B4800:
            return baud_rate::b4800;
        case B9600:
            return baud_rate::b9600;
        // case B14400: return baud_rate::b14400;
        case B19200:
            return baud_rate::b19200;
        case B38400:
            return baud_rate::b38400;
        case B57600:
            return baud_rate::b57600;
        case B115200:
            return baud_rate::b115200;
        // case B128000: return baud_rate::b128000;
        // case B256000: return baud_rate::b256000;
        default:
            return serial_baud_rate{static_cast<uint32_t>(r)};
        }
    }

    void convert_baud_rate(serial_baud_rate r, termios_struct_t& tty) {
#ifdef __linux__
        auto set_brate = [&](speed_t s) {
            tty.c_ispeed = s;
            tty.c_ospeed = s;
        };

        auto set_custom_brate = [&](speed_t s) {
            tty.c_cflag &= ~CBAUD;
            tty.c_cflag |= CBAUDEX;
            tty.c_ispeed = s;
            tty.c_ospeed = s;
        };

        tty.c_cflag |= CBAUD;
        tty.c_cflag &= ~CBAUDEX;
#else
        auto set_brate = [&](speed_t s) {
            ::cfsetispeed(&tty, s);
            ::cfsetospeed(&tty, s);
        };

        auto set_custom_brate = set_brate;
#endif
        if (r.is_standard()) {
            switch (r.rate()) {
            case baud_rate::b110:
                return set_brate(B110);
            case baud_rate::b300:
                return set_brate(B300);
            case baud_rate::b600:
                return set_brate(B600);
            case baud_rate::b1200:
                return set_brate(B1200);
            case baud_rate::b2400:
                return set_brate(B2400);
            case baud_rate::b4800:
                return set_brate(B4800);
            case baud_rate::b9600:
                return set_brate(B9600);
            case baud_rate::b14400:
                return set_custom_brate(14400);
            case baud_rate::b19200:
                return set_brate(B19200);
            case baud_rate::b38400:
                return set_brate(B38400);
            case baud_rate::b57600:
                return set_brate(B57600);
            case baud_rate::b115200:
                return set_brate(B115200);
            case baud_rate::b128000:
                return set_custom_brate(128000);
            case baud_rate::b256000:
                return set_custom_brate(256000);
            default:
                set_custom_brate(static_cast<uint32_t>(r.rate()));
            }
        }
        else {
            set_custom_brate(static_cast<uint32_t>(r.rate()));
        }
    }

    serial_bits_per_byte convert_bits_per_byte(tcflag_t c_cflags) {
        c_cflags &= CSIZE;
        if (c_cflags & CS8) {
            return serial_bits_per_byte::bits8;
        }
        if (c_cflags & CS7) {
            return serial_bits_per_byte::bits7;
        }
        if (c_cflags & CS6) {
            return serial_bits_per_byte::bits6;
        }
        if (c_cflags & CS5) {
            return serial_bits_per_byte::bits5;
        }
        return serial_bits_per_byte::bits8;
    }

    tcflag_t convert_bits_per_byte(serial_bits_per_byte bpb) {
        switch (bpb) {
        case serial_bits_per_byte::bits8:
            return CS8;
        case serial_bits_per_byte::bits7:
            return CS7;
        case serial_bits_per_byte::bits6:
            return CS6;
        case serial_bits_per_byte::bits5:
            return CS5;
        default:
            return CS8;
        }
    }

    serial_parity convert_parity(tcflag_t c_cflags) {
        if ((c_cflags & PARENB) == 0) {
            return serial_parity::none;
        }
        if (c_cflags & CMSPAR) {
            if (c_cflags & PARODD) {
                return serial_parity::mark;
            }
            return serial_parity::space;
        }
        if (c_cflags & PARODD) {
            return serial_parity::odd;
        }
        return serial_parity::even;
    }

    tcflag_t convert_parity(serial_parity p) {
        switch (p) {
        case serial_parity::none:
            return 0;
        case serial_parity::odd:
            return PARENB | PARODD;
        case serial_parity::even:
            return PARENB;
        case serial_parity::mark:
            return PARENB | PARODD | CMSPAR;
        case serial_parity::space:
            return PARENB | CMSPAR;
        default:
            return 0;
        }
    }

    void get_flags_from_termios(const termios_struct_t& tty,
                                serial_flags& flags) {
        if (tty.c_cflag & PARENB) {
            flags |= serial_flags::parity;
        }
        if (tty.c_cflag & CSTOPB) {
            flags |= serial_flags::two_stop_bits;
        }
        if (tty.c_cflag & CRTSCTS) {
            flags |= serial_flags::rts_flow_control;
            flags |= serial_flags::cts_output_flow_control;
        }
        if (tty.c_iflag & IXON) {
            flags |= serial_flags::x_output_flow_control;
        }
        if (tty.c_iflag & IXOFF) {
            flags |= serial_flags::x_input_flow_control;
        }
    }

    void set_flags_to_termios(serial_flags flags, termios_struct_t& tty) {
        if (flags & serial_flags::two_stop_bits) {
            tty.c_cflag |= CSTOPB;
        }
        else {
            tty.c_cflag &= ~CSTOPB;
        }
        if (flags & serial_flags::x_output_flow_control) {
            tty.c_cflag |= IXON;
        }
        else {
            tty.c_cflag &= ~IXON;
        }
        if (flags & serial_flags::x_input_flow_control) {
            tty.c_cflag |= IXOFF;
        }
        else {
            tty.c_cflag &= ~IXOFF;
        }
        if (flags & serial_flags::rts_flow_control &&
            flags & serial_flags::cts_output_flow_control) {
            tty.c_cflag |= CRTSCTS;
        }
        else {
            tty.c_cflag &= ~CRTSCTS;
        }
    }
} // namespace

void serial_port_impl::open(native_string_type path, serial_access access,
                            std::error_code& ec) {
    ec.clear();
    if (path.empty()) {
        ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return;
    }
    int flags = O_CLOEXEC;
    switch (access) {
    case serial_access::read:
        flags |= O_RDONLY;
        break;
    case serial_access::write:
        flags |= O_WRONLY;
        break;
    case serial_access::read_write:
        flags |= O_RDWR;
        break;
    default:
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }

    int fd = ::open(path.data(), flags);
    if (fd < 0) {
        ec = os::make_system_error(errno);
        return;
    }

    os::handle serial_handle{fd};
    std::string serial_path{path};
    set_handle_path(serial_handle, serial_path, ec);
}

serial_options serial_port_impl::get_options(std::error_code& ec) noexcept {
    ec.clear();
    termios_struct_t tty;
#ifdef __linux__
    const int res = ::ioctl(native_handle().get(), TCGETS2, &tty);
#else
    const int res = ::tcgetattr(native_handle().get(), &tty);
#endif // __linux__
    if (res == -1) {
        ec = os::make_system_error(errno);
        return {};
    }

    serial_options opts;
#ifdef __linux__
    if (tty.c_cflag & CBAUDEX) {
        opts.baud_rate = serial_baud_rate{tty.c_ispeed};
    }
    else {
#endif // __linux__
        opts.baud_rate = convert_baud_rate(tty.c_ispeed);
#ifdef __linux__
    }
#endif // __linux__
    opts.bits_per_byte = convert_bits_per_byte(tty.c_cflag);
    opts.parity = convert_parity(tty.c_cflag);
    get_flags_from_termios(tty, opts.flags);

    return opts;
}

void serial_port_impl::set_options(const serial_options& opts,
                                   std::error_code& ec) noexcept {
    ec.clear();
    termios_struct_t tty;
#ifdef __linux__
    int res = ::ioctl(native_handle().get(), TCGETS2, &tty);
#else
    int res = ::tcgetattr(native_handle().get(), &tty);
#endif // __linux__
    if (res == -1) {
        ec = os::make_system_error(errno);
        return;
    }

    convert_baud_rate(opts.baud_rate, tty);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= convert_bits_per_byte(opts.bits_per_byte);
    tty.c_cflag &= ~(PARENB | CMSPAR);
    if (opts.flags & serial_flags::parity) {
        tty.c_cflag |= convert_parity(opts.parity);
    }
    set_flags_to_termios(opts.flags, tty);

    // set binary mode
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~(OPOST | ONLCR);

#ifdef __linux__
    res = ::ioctl(native_handle().get(), TCSETS2, &tty);
#else
    res = ::tcsetattr(native_handle().get(), TCSANOW, &tty);
#endif // __linux__
    if (res == -1) {
        ec = os::make_system_error(errno);
        return;
    }
}