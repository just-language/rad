#include <rad/net/detail/posix/resolver_impl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using net::detail::resolver_impl;

static_assert(AI_PASSIVE == ienum(resolver_flags::passive));
static_assert(AI_CANONNAME == ienum(resolver_flags::canon_name));
static_assert(AI_NUMERICHOST == ienum(resolver_flags::numeric_host));
static_assert(AI_V4MAPPED == ienum(resolver_flags::ipv4_mapped));
static_assert(AI_ADDRCONFIG == ienum(resolver_flags::addr_config));
static_assert(AI_ALL == ienum(resolver_flags::all));

namespace {
    class getaddrinfo_error_category_type : public std::error_category {
        virtual const char* name() const noexcept override {
            return "getaddrinfo";
        }

        virtual std::string message(int ec) const override {
            const char* p = ::gai_strerror(ec);
            return p == nullptr ? std::string{} : std::string{p};
        }
    };

    inline getaddrinfo_error_category_type getaddrinfo_error_category;
} // namespace

const std::error_category& net::detail::getaddrinfo_category() noexcept {
    return getaddrinfo_error_category;
}

void resolver_impl::do_resolve(native_string_type host, const char* service,
                               const resolver_hint& hint,
                               std::vector<endpoint>& results,
                               std::error_code& ec) noexcept {
    struct addrinfo hints{};
    hints.ai_family = as<int>(hint.flags);
    hints.ai_family = as<int>(hint.family);
    hints.ai_socktype = as<int>(hint.sock_type);
    hints.ai_protocol = as<int>(hint.protocol);

    struct addrinfo* res = nullptr;
    int ret = ::getaddrinfo(host.data(), service, &hints, &res);

    if (ret != 0) {
        if (ret == EAI_SYSTEM) {
            ec = os::make_system_error(errno);
        }
        else {
            ec = std::error_code{ret, getaddrinfo_error_category};
        }
        return;
    }

    assert(res != nullptr);
    auto on_exit = scope_exit([res] { ::freeaddrinfo(res); });

    while (res) {
        results.emplace_back(init_sockaddr, res->ai_addr, res->ai_addrlen);
        res = res->ai_next;
    }
}