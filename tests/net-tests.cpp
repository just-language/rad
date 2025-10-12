#include <rad/net/types.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <In6addr.h>
#include <MSWSock.h>
#elif __unix__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#include <iostream>
#include <vector>

using namespace RAD_LIB_NAMESPACE;
using namespace net;

namespace {

    void validate_endpoint(const endpoint& epoint, const std::string& ipstr) {
        auto copy_of_epoint = epoint;
        if (copy_of_epoint.size() != epoint.size() ||
            memcmp(&epoint, &copy_of_epoint, epoint.size())) {
            throw std::runtime_error("endpoint copy ctor behaved wrong");
        }

        sockaddr_in os_ipv4{};
        sockaddr_in6 os_ipv6{};

        os_ipv4.sin_port = htons(epoint.port());
        os_ipv4.sin_family = AF_INET;
        os_ipv6.sin6_port = htons(epoint.port());
        os_ipv6.sin6_family = AF_INET6;

        if (epoint.is_v6()) {
            inet_pton(AF_INET6, ipstr.c_str(), &os_ipv6.sin6_addr);
            if (epoint.size() != sizeof(os_ipv6)) {
                throw std::runtime_error(
                    "endpoint test failed due to ipv6 size "
                    "mismatch");
            }
            if (memcmp(&epoint, &os_ipv6, sizeof(os_ipv6))) {
                throw std::runtime_error(
                    "endpoint test failed due to ipv6 memcmp "
                    "mismatch");
            }
        }
        else {
            inet_pton(AF_INET, ipstr.c_str(), &os_ipv4.sin_addr);
            if (epoint.size() != sizeof(os_ipv4)) {
                throw std::runtime_error(
                    "endpoint test failed due to ipv4 size "
                    "mismatch");
            }
            if (memcmp(&epoint, &os_ipv4, sizeof(os_ipv4))) {
                throw std::runtime_error(
                    "endpoint test failed due to ipv4 memcmp "
                    "mismatch");
            }
        }
    }

    void do_internal_tests() {
        // ipv4
        {
            std::string ipstr = "246.237.20.212";
            uint32_t ipnum = 415314689;

            ipv4 ip{ipstr};
            in_addr os_ip;

            static_assert(sizeof(ip) == sizeof(os_ip));

            inet_pton(AF_INET, ipstr.c_str(), &os_ip);

            if (memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv4 test failed");
            }

            ip = ipv4{ipnum};
#ifdef _WIN32
            os_ip.S_un.S_addr = htonl(ipnum);
#else
            os_ip.s_addr = htonl(ipnum);
#endif
            if (memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv4 test failed");
            }

            ip = ipv4::loopback();
            inet_pton(AF_INET, "127.0.0.1", &os_ip);

            if (!ip.is_loopback() || memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv4 test failed");
            }

            ip = ipv4::any();
#ifdef _WIN32
            os_ip.S_un.S_addr = htonl(INADDR_ANY);
#else
            os_ip.s_addr = htonl(INADDR_ANY);
#endif

            if (!ip.is_any() || memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv4 test failed");
            }
        }

        // ipv6
        {
            std::string ipstr = "735f:22e1:0154:410b:89d9:f0da:6286:ae23";

            ipv6 ip{ipstr};
            in6_addr os_ip;

            static_assert(sizeof(ip) == sizeof(os_ip));

            inet_pton(AF_INET6, ipstr.c_str(), &os_ip);

            if (memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv6 test failed");
            }

            ip = ipv6::loopback();
            inet_pton(AF_INET6, "::1", &os_ip);

            if (!ip.is_loopback() || memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv6 test failed");
            }
        }

        // socket_port
        {
            uint16_t portn = 6518;
            socket_port port = portn;
            uint16_t os_prt = htons(portn);

            if (memcmp(&port, &os_prt, sizeof(os_prt)) != 0) {
                throw std::runtime_error("socket_port test failed");
            }

            if (socket_port::any() != 0) {
                throw std::runtime_error("socket_port test failed");
            }
        }

        // ipv4_endpoint
        {
            std::string ipstr = "65.43.241.169";
            uint32_t ipnum = 654651485;
            uint16_t portn = 54122;
            uint16_t portn2 = 21435;

            ipv4_endpoint ip{ipstr, portn};
            sockaddr_in os_ip{};

            static_assert(sizeof(ip) == sizeof(os_ip));

            os_ip.sin_family = AF_INET;
            os_ip.sin_port = htons(portn);
            inet_pton(AF_INET, ipstr.c_str(), &os_ip.sin_addr);

            if (memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv4_endpoint test failed");
            }

            ip = {ipnum, portn2};
            os_ip.sin_port = htons(portn2);
#ifdef _WIN32
            os_ip.sin_addr.S_un.S_addr = htonl(ipnum);
#else
            os_ip.sin_addr.s_addr = htonl(ipnum);
#endif

            if (memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error("ipv4 test failed");
            }
        }

        // ipv6_endpoint
        {
            std::string ipstr = "8cf0:7047:bf87:b1a3:bf3c:e379:2cf0:7533";
            uint16_t portn = 5122;

            ipv6_endpoint ip{ipv6{ipstr}, portn};
            sockaddr_in6 os_ip{};

            static_assert(sizeof(ip) == sizeof(os_ip));

            os_ip.sin6_family = AF_INET6;
            os_ip.sin6_port = htons(portn);
            if (inet_pton(AF_INET6, ipstr.c_str(), &os_ip.sin6_addr) != 1) {
                printf("inet_pton failed !\n");
            }

            if (memcmp(&ip, &os_ip, sizeof(os_ip)) != 0) {
                throw std::runtime_error(
                    "ipv6_endpoint test failed due to memcmp "
                    "mismatch");
            }
        }

        // endpoint
        {
            std::vector<std::string> ips = {
                "796e:3355:404b:4ee4:e58d:5b21:a8cd:4d46",
                "a923:fafe:092d:b5fb:ebb4:11af:62c9:10d7",
                "91db:94fd:e9f3:19fd:a302:ebcc:791c:abc3",
                "4900:c5ee:8add:bd20:3112:6a3a:581a:0596",
                "319e:0701:d01b:0469:7e46:06aa:34dc:05a2",
                "124.4.149.119",
                "242.11.229.191",
                "6.170.200.75",
                "170.187.180.152",
                "7.161.232.40"};

            std::vector<uint16_t> ports = {53214, 931, 561, 53,  845,
                                           6357,  80,  443, 100, 468};

            for (size_t i = 0; i < ips.size(); ++i) {
                endpoint epoint(ips[i], ports[i]);
                validate_endpoint(epoint, ips[i]);
            }
        }
    }

} // namespace

namespace tests_fn {
    bool do_net_tests() {
        try {
            do_internal_tests();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] net tests failed ! " << ex.what() << "\n";
            return false;
        }

        std::cout << "[*] net tests passed\n";
        return true;
    }
} // namespace tests_fn
