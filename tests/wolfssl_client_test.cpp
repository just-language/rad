#include <iostream>
#include <cstring>
#include "wolfssl_options.h"
#include <wolfssl/ssl.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif // _WIN32

namespace {
    void close_socket(int sockfd) noexcept {
        if (sockfd != -1) {
#ifdef _WIN32
            ::closesocket(sockfd);
#else
            ::close(sockfd);
#endif // _WIN32
        }
    }

    int last_socket_error() noexcept {
#ifdef _WIN32
        return ::WSAGetLastError();
#else
        return errno;
#endif // _WIN32
    }

    char wolfssl_error_buff[1024];

    const char* get_wolfssl_error_msg(int c) {
        std::memset(wolfssl_error_buff, 0, sizeof(wolfssl_error_buff));
        wolfSSL_ERR_error_string_n(c, wolfssl_error_buff,
                                   sizeof(wolfssl_error_buff) - 1);
        return wolfssl_error_buff;
    }

    struct clean_on_exit {
        WOLFSSL_CTX* ctx = nullptr;
        WOLFSSL* ssl = nullptr;
        int sockfd = -1;

        ~clean_on_exit() {
            if (ssl != nullptr) {
                ::wolfSSL_Free(ssl);
            }
            if (ctx != nullptr) {
                ::wolfSSL_CTX_free(ctx);
            }
            close_socket(sockfd);
            sockfd = -1;
            wolfSSL_Debugging_OFF();
            wolfSSL_Cleanup();
        }
    };

    int connect_socket(const char* domain_name) {
        int sockfd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
        if (sockfd == -1) {
            return -1;
        }

        struct addrinfo hint{};
        hint.ai_family = AF_INET;
        hint.ai_socktype = SOCK_STREAM;

        struct addrinfo* results = nullptr;
        const int dns_res =
            ::getaddrinfo(domain_name, "https", &hint, &results);
        if (dns_res != 0 || results == nullptr) {
            std::cout << "[!] wolfssl client: getaddrinfo failed with error: "
                      << dns_res << " !\n";
            close_socket(sockfd);
            return -1;
        }

        bool connected = false;
        for (struct addrinfo* p = results; p != nullptr; p = p->ai_next) {
            char buff[100];
            std::memset(buff, 0, sizeof(buff));
            const char* str = ::inet_ntop(
                p->ai_family, &(((sockaddr_in*)(p->ai_addr))->sin_addr), buff,
                sizeof(buff) - 1);
            if (str != nullptr) {
                std::cout << "[*] wolfssl client: trying to connect to "
                          << domain_name << " (" << buff << ") ...\n";
            }
            const int connect_res =
                ::connect(sockfd, p->ai_addr, static_cast<int>(p->ai_addrlen));
            if (connect_res == 0) {
                std::cout << "[*] wolfssl client: connected to " << domain_name
                          << " (" << buff << ")\n";
                connected = true;
                break;
            }
            else {
                std::cout << "[!] wolfssl client: failed to connect to "
                          << domain_name << " (" << buff
                          << ")"
                             " with error: "
                          << last_socket_error() << "\n";
            }
        }

        ::freeaddrinfo(results);
        if (!connected) {
            std::cout << "[!] wolfssl client: couldn't connect to "
                      << domain_name << " !\n";
            close_socket(sockfd);
            return -1;
        }

        return sockfd;
    }

    bool do_ssl_handshake(const char* domain_name, bool use_tls13) {
        wolfSSL_Init();
        // wolfSSL_Debugging_ON();
        clean_on_exit on_exit;
        on_exit.sockfd = connect_socket(domain_name);
        if (on_exit.sockfd == -1) {
            return false;
        }
        WOLFSSL_METHOD* method =
            use_tls13 ? wolfTLS_client_method() : wolfTLSv1_2_client_method();
        on_exit.ctx = wolfSSL_CTX_new(method);
        if (on_exit.ctx == nullptr) {
            std::cout << "[!] wolfssl client: failed to create a new wolfssl "
                         "context !\n";
            return false;
        }

        wolfSSL_Debugging_OFF();
        wolfSSL_CTX_set_default_verify_paths(on_exit.ctx);
        // wolfSSL_Debugging_ON();

        on_exit.ssl = wolfSSL_new(on_exit.ctx);
        if (on_exit.ssl == nullptr) {
            std::cout << "[!] wolfssl client: failed to create a new wolfssl "
                         "session !\n";
            return false;
        }

        wolfSSL_set_fd(on_exit.ssl, on_exit.sockfd);
        wolfSSL_UseSNI(on_exit.ssl, WOLFSSL_SNI_HOST_NAME, domain_name,
                       static_cast<unsigned short>(strlen(domain_name)));

        const int connect_ret = wolfSSL_connect(on_exit.ssl);
        wolfSSL_Debugging_OFF();
        if (connect_ret != SSL_SUCCESS) {
            // fails here when use_tls13 is true!
            const char* error_msg = get_wolfssl_error_msg(
                wolfSSL_get_error(on_exit.ssl, connect_ret));
            std::cout << "[!] wolfssl client: failed to do handshake : "
                      << error_msg << "\n";
            return false;
        }
        else {
            const char* version = wolfSSL_get_version(on_exit.ssl);
            std::cout
                << "[*] wolfssl client: done the handshake, protocol version: "
                << version << "\n";
            wolfSSL_shutdown(on_exit.ssl);
            return true;
        }
    }
} // namespace

namespace tests_fn {
    bool do_wolfssl_client_test() {
        const bool res1 = do_ssl_handshake("github.com", true);
        const bool res2 = do_ssl_handshake("github.com", false);
        if (!res1 || !res2) {
            std::cout << "[!] wolfssl client test failed !\n";
        }
        else {
            std::cout << "[*] wolfssl client test passed\n";
        }
        return res1 && res2;
    }
} // namespace tests_fn