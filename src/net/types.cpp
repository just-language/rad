#include <cassert>
#include <rad/net/types.h>
#include <charconv>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <In6addr.h>
#include <MSWSock.h>
#include <Windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/*
ipv4 converstaion from and to string
*/

using namespace RAD_LIB_NAMESPACE;

#ifdef _WIN32
int net::detail::close_socket(socket_fd_t fd) noexcept {
    return ::closesocket(fd);
}
#endif // _WIN32

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-attributes"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif // __clang__
#endif // __GNUC__
std::string net::ipv4::to_string() const noexcept {
    // 16 chars should be enough for 15 chars of ip and a null char
    std::array<char, 16 * 2> out_buff;
    char* out_ptr = out_buff.data();
    for (uint8_t b : bytes_storage_) {
        auto res =
            std::to_chars(out_ptr, out_buff.data() + out_buff.size(), b, 10);
        assert(res.ec == std::errc{});
        assert(res.ptr != out_ptr);
        out_ptr = res.ptr;
        *out_ptr = '.';
        assert(out_ptr < out_buff.data() + out_buff.size());
        out_ptr += 1;
    }
    out_ptr -= 1;
    return std::string{out_buff.data(),
                       static_cast<size_t>(out_ptr - out_buff.data())};
}

#ifdef __GNUC__
#ifdef __clang__
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif // __clang__
#endif // __GNUC__

bool net::ipv4::from_string(std::string_view ip_str) noexcept {
    using namespace std::string_view_literals;
    std::size_t counter = 0;
    for (auto part : ip_str | split("."sv)) {
        if (counter == 4 || part.empty()) {
            return false;
        }
        int base = 10;
        if (part[0] == '0') {
            if (part.size() == 1) {
                bytes_storage_[counter++] = 0;
                continue;
            }
            base = 8;
            if (part[1] == 'x' || part[1] == 'X') {
                part.remove_prefix(2);
                if (part.empty()) {
                    return false;
                }
                base = 16;
            }
        }
        std::error_code ec;
        bytes_storage_[counter++] = to_uint8(part, base, ec);
        if (ec) {
            return false;
        }
    }
    if (counter != 4) {
        return false;
    }
    return true;
}

std::string net::ipv6::to_string() const {
    std::array<char, 100> ipbuff;
    if (::inet_ntop(AF_INET6, bytes_storage_.data(), ipbuff.data(),
                    ipbuff.size()) != nullptr) {
        return ipbuff.data();
    }
    else {
        return std::string();
    }
}

bool net::ipv6::from_string(std::string_view ip_str) noexcept {
    // this should be more than enough to store valid ipv6 text
    constexpr std::size_t max_stack_buff = 100;
    if (ip_str.size() > max_stack_buff) {
        return false;
    }
    std::array<char, max_stack_buff + 1> ip_zstr;
    ip_zstr[ip_str.size()] = 0;
    std::copy(ip_str.begin(), ip_str.end(), ip_zstr.begin());
    return ::inet_pton(AF_INET6, ip_zstr.data(), bytes_storage_.data()) == 1;
}

std::string net::host_name(std::error_code& ec) noexcept {
    constexpr std::size_t max_host_name_len = 256;
    std::string host_name_str;
    host_name_str.resize(max_host_name_len);
    if (::gethostname(host_name_str.data(),
                      static_cast<socklen_t>(max_host_name_len)) != 0) {
#ifdef _WIN32
        ec = std::error_code{::WSAGetLastError(), system_category()};
#else
        ec = std::error_code{errno, system_category()};
#endif // _WIN32
        host_name_str.clear();
    }
    else {
        host_name_str.resize(host_name_str.find('\0'));
    }
    return host_name_str;
}

bool net::is_valid_ipv4(std::string_view ip_str) noexcept {
    using namespace std::string_view_literals;
    int counter = 5;
    for (auto part : ip_str | split("."sv)) {
        --counter;
        if (part.empty() || counter == 0) {
            return false;
        }
        std::error_code ec;
        to_uint8(part, 10, ec);
        if (ec) {
            if (part.size() > 2 && part[0] == '0' && part[1] == 'x') {
                part = part.substr(2);
                to_uint8(part, 16, ec);
                if (!ec) {
                    continue;
                }
            }
            else if (part[0] == '0') {
                to_uint8(part, 8, ec);
                if (!ec) {
                    continue;
                }
            }
            return false;
        }
    }
    if (counter != 1) {
        return false;
    }
    return true;
}

bool net::is_valid_ipv6(std::string_view ip_str) noexcept {
    constexpr std::size_t max_ipv6_length = 45;
    if (ip_str.size() > max_ipv6_length) {
        return false;
    }
    std::array<char, max_ipv6_length + 1> in_buff;
    std::copy(ip_str.begin(), ip_str.end(), in_buff.begin());
    in_buff[ip_str.size()] = '\0';
    std::array<uint8_t, sizeof(endpoint)> out_buff;
    return inet_pton(AF_INET6, in_buff.data(), out_buff.data()) == 1;
}

bool net::is_valid_ip(std::string_view ip_str) noexcept {
    return is_valid_ipv4(ip_str) || is_valid_ipv6(ip_str);
}
