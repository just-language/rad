#include <rad/async/work_guard.h>
#include <rad/detail/dprint.h>
#include <rad/net/detail/windows/async_resolver_impl.h>
#include <rad/net/tcp.h>
#include <rad/net/udp.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <In6addr.h>
#include <MSWSock.h>
#include <Windows.h>
#include <VersionHelpers.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using namespace net::detail;
using namespace io;

static_assert(AI_PASSIVE == ienum(resolver_flags::passive));
static_assert(AI_CANONNAME == ienum(resolver_flags::canon_name));
static_assert(AI_NUMERICHOST == ienum(resolver_flags::numeric_host));
static_assert(AI_V4MAPPED == ienum(resolver_flags::ipv4_mapped));
static_assert(AI_ALL == ienum(resolver_flags::all));
static_assert(AI_ADDRCONFIG == ienum(resolver_flags::addr_config));
static_assert(AI_NON_AUTHORITATIVE == ienum(resolver_flags::non_authoritative));
static_assert(AI_SECURE == ienum(resolver_flags::secure));

namespace {
    inline const bool is_windows8_or_greater = IsWindows8OrGreater();

    using getaddrinfoex_cancel_fn_t = int(__stdcall*)(LPHANDLE);

    getaddrinfoex_cancel_fn_t load_getaddrinfoex_cancel_fn() noexcept {
        if (!is_windows8_or_greater) {
            return nullptr;
        }
        HMODULE ws_mod = ::GetModuleHandleW(L"ws2_32.dll");
        if (!ws_mod) {
            dprint("GetModuleHandleW(ws2_32.dll) failed ! : %d\n",
                   (int)GetLastError());
            return nullptr;
        }
        FARPROC fn_ptr = ::GetProcAddress(ws_mod, "GetAddrInfoExCancel");
        if (!fn_ptr) {
            dprint("GetProcAddress(GetAddrInfoExCancel) failed ! : "
                   "%d\n",
                   (int)GetLastError());
            return nullptr;
        }
        return reinterpret_cast<getaddrinfoex_cancel_fn_t>(fn_ptr);
    }

    const getaddrinfoex_cancel_fn_t getaddrinfoex_cancel_fn =
        load_getaddrinfoex_cancel_fn();

    using resolver_hint = resolver_impl::resolver_hint;

    struct addrinfoW_deleter {
        addrinfoW* addrs;

        ~addrinfoW_deleter() {
            assert(addrs != nullptr);
            FreeAddrInfoW(addrs);
        }
    };

    struct addrinfoExW_deleter {
        ADDRINFOEXW* addrs;

        ~addrinfoExW_deleter() {
            assert(addrs != nullptr);
            FreeAddrInfoExW(addrs);
        }
    };

    addrinfoW copy_hint(const resolver_hint& hint) noexcept {
        addrinfoW whint{};
        whint.ai_flags = static_cast<int>(hint.flags);
        whint.ai_family = static_cast<int>(hint.family);
        whint.ai_socktype = static_cast<int>(hint.sock_type);
        whint.ai_protocol = static_cast<int>(hint.protocol);
        return whint;
    }

    ADDRINFOEXW copy_hint_ex(const resolver_hint& hint) noexcept {
        ADDRINFOEXW whint{};
        whint.ai_flags = static_cast<int>(hint.flags);
        whint.ai_family = static_cast<int>(hint.family);
        whint.ai_socktype = static_cast<int>(hint.sock_type);
        whint.ai_protocol = static_cast<int>(hint.protocol);
        return whint;
    }

    template <class AddrType>
    void insert_ip_results(std::vector<endpoint>& results,
                           const AddrType* addresses) {
        std::size_t results_count = 0;
        for (const AddrType* curr = addresses; curr != nullptr;
             curr = curr->ai_next, ++results_count) {
        }
        assert(results_count != 0);

        results.reserve(results.size() + results_count);
        for (const AddrType* curr = addresses; curr != nullptr;
             curr = curr->ai_next) {
            results.emplace_back(init_sockaddr, curr->ai_addr,
                                 static_cast<int>(curr->ai_addrlen));
        }
    }
}; // namespace

void resolver_impl::do_resolve(native_string_type host, const wchar_t* service,
                               const resolver_hint& hint,
                               std::vector<endpoint>& results,
                               std::error_code& ec) noexcept {
    ec.clear();
    addrinfoW whint = copy_hint(hint);
    addrinfoW* wresults = nullptr;
    // GetAddrInfoW does not set WSAGetLastError!
    const int iec = ::GetAddrInfoW(host.data(), service, &whint, &wresults);
    if (iec != 0) {
        assert(wresults == nullptr);
        if (iec == ERROR_OPERATION_ABORTED) {
            ec = std::make_error_code(std::errc::operation_canceled);
        }
        else {
            ec = os::make_system_error(iec);
        }
        return;
    }

    addrinfoW_deleter deleter{wresults};
    insert_ip_results(results, wresults);
}

async_result async_resolver_impl::do_async_resolve(
    native_string_type host, const wchar_t* service, const resolver_hint& hint,
    io::detail::io_op& op, void** results,
    resolver_callback callback) noexcept {
    assert(supports_async_operation());

    ADDRINFOEXW whint = copy_hint_ex(hint);
    auto wresults = reinterpret_cast<addrinfoexW**>(results);
    void** cancel_handle_ptr = &cancel_handle_;

    work_guard wguard{any_ex()};

    LPOVERLAPPED ov_ptr = op.get_ov_ptr();
    // GetAddrInfoExW does not set WSAGetLastError!
    int iec = GetAddrInfoExW(host.data(), service, 0, nullptr, &whint, wresults,
                             nullptr, ov_ptr, callback, cancel_handle_ptr);

    if (iec == WSA_IO_PENDING) {
        wguard.release();
        return async_result::pending();
    }

    // no callback will be scheduled so wguard will cancel the outstanding
    // work
    if (!iec) {
        return async_result::success(0);
    }
    return iec == ERROR_OPERATION_ABORTED
               ? async_result::failed(
                     std::make_error_code(std::errc::operation_canceled))
               : async_result::failed(os::make_system_error(iec));
}

void async_resolver_impl::emulate_async_resolve(
    std::wstring host, std::wstring service, const resolver_hint& hint,
    void** results, int& error_value, details::async_op_base& op) {
    assert(!supports_async_operation());
    auto state = resolver_sync_state_t{
        .host = std::move(host),
        .service = std::move(service),
        .hint = hint,
        .op = op,
        .results = results,
        .error_value = error_value,
    };

    work_guard wguard{any_ex()};
    sync_op_state_ = true;
    auto on_exit = scope_exit([this] { sync_op_state_ = false; });

    post(ex_->thread_pool_executor(), [state = std::move(state), this] {
        bool was_canceled = sync_op_state_.exchange(false) != true;
        if (was_canceled) {
            state.error_value = ERROR_OPERATION_ABORTED;
            any_ex().post_finished(state.op);
            return;
        }
        ADDRINFOEXW whint = copy_hint_ex(state.hint);
        auto wresults = reinterpret_cast<addrinfoexW**>(state.results);
        state.error_value = ::GetAddrInfoExW(
            state.host.c_str(), state.service.c_str(), 0, nullptr, &whint,
            wresults, nullptr, nullptr, nullptr, nullptr);
        any_ex().post_finished(state.op);
    });

    on_exit.release();
    wguard.release();
}

void async_resolver_impl::cancel() noexcept {
    if (cancel_handle_ && getaddrinfoex_cancel_fn) {
        (void)getaddrinfoex_cancel_fn(&cancel_handle_);
        cancel_handle_ = nullptr;
    }
    else {
        sync_op_state_ = false;
    }
}

bool async_resolver_impl::supports_async_operation() noexcept {
    return getaddrinfoex_cancel_fn != nullptr;
}

void __stdcall async_resolver_impl::results_awaiter_cb(
    DWORD dwError, DWORD dwBytes, LPWSAOVERLAPPED lpOverlapped) noexcept {
    auto* awaiter = static_cast<results_awaiter*>(
        io::detail::io_op::from_ov_ptr(lpOverlapped));
    awaiter->schedule_op(dwError);
}

void __stdcall async_resolver_impl::results_ref_awaiter_cb(
    DWORD dwError, DWORD dwBytes, LPWSAOVERLAPPED lpOverlapped) noexcept {
    auto* awaiter = static_cast<results_ref_awaiter*>(
        io::detail::io_op::from_ov_ptr(lpOverlapped));
    awaiter->schedule_op(dwError);
}

void async_resolver_impl::results_ptr_to_endpoints(
    void* results, std::vector<endpoint>& epoints) {
    assert(results != nullptr);
    ADDRINFOEXW* addrs = static_cast<ADDRINFOEXW*>(results);
    addrinfoExW_deleter deleter{addrs};
    insert_ip_results(epoints, addrs);
}

void async_resolver_impl::free_results(void* results) noexcept {
    if (results == nullptr) {
        return;
    }
    ADDRINFOEXW* addrs = static_cast<ADDRINFOEXW*>(results);
    ::FreeAddrInfoExW(addrs);
}