#include <rad/windows/wts.h>
#include <cassert>
#include <Windows.h>
#include <LM.h>
#include <userenv.h>
#include <wtsapi32.h>

using namespace RAD_LIB_NAMESPACE;
using namespace wts;

namespace {
    class wst_buffer : noncopyable {
    public:
        wst_buffer() = default;

        wst_buffer(wst_buffer&& other) noexcept
            : m_ptr(std::exchange(other.m_ptr, nullptr)) {
        }

        wst_buffer& operator=(wst_buffer&& other) noexcept {
            free_buff();
            m_ptr = std::exchange(other.m_ptr, nullptr);
            m_size = std::exchange(other.m_size, 0);
            return *this;
        }

        ~wst_buffer() {
            free_buff();
        }

        wchar_t* ptr() noexcept {
            return m_ptr;
        }

        const wchar_t* ptr() const noexcept {
            return m_ptr;
        }

        // the string is null terminated
        DWORD size() const noexcept {
            return (m_size / sizeof(wchar_t)) - 1;
        }

        std::pair<wchar_t**, DWORD*> get_refs() {
            return {&m_ptr, &m_size};
        }

        std::wstring to_wstring() const noexcept {
            if (!m_ptr || !m_size) {
                return std::wstring();
            }
            return std::wstring(m_ptr, size());
        }

    private:
        void free_buff() noexcept {
            if (m_ptr) {
                WTSFreeMemory(m_ptr);
            }
            m_ptr = nullptr;
            m_size = 0;
        }

        wchar_t* m_ptr = nullptr;
        DWORD m_size = 0;
    };

    class netapi_buffer : noncopyable {
    public:
        netapi_buffer() = default;

        netapi_buffer(netapi_buffer&& other) noexcept
            : mptr(std::exchange(other.mptr, nullptr)) {
        }

        netapi_buffer& operator=(netapi_buffer&& other) noexcept {
            free_buff();
            mptr = std::exchange(other.mptr, nullptr);
            return *this;
        }

        uint8_t** ptr() noexcept {
            free_buff();
            mptr = nullptr;
            return &mptr;
        }

        uint8_t* data() {
            return mptr;
        }

        ~netapi_buffer() {
            free_buff();
        }

    private:
        void free_buff() noexcept {
            if (mptr) {
                NetApiBufferFree(mptr);
            }
            mptr = nullptr;
        }

        uint8_t* mptr = nullptr;
    };

    std::wstring wts_query_string(DWORD id, WTS_INFO_CLASS info) {
        wst_buffer buff;
        auto [buff_ptr, len_ptr] = buff.get_refs();
        BOOL result = WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, id,
                                                  info, buff_ptr, len_ptr);
        if (!result) {
            throw std::system_error(GetLastError(), system_category(),
                                    "WTSQuerySessionInformationW");
        }

        assert(buff_ptr != nullptr && len_ptr != 0);

        return buff.to_wstring();
    }
} // namespace

std::pair<bool, DWORD> wts::is_user_logged_in() {
    std::pair<bool, DWORD> ret;
    auto& [logged_in, id] = ret;
    id = WTSGetActiveConsoleSessionId();
    logged_in = id != 0xFFFFFFFF;
    return ret;
}

DWORD wts::active_session_id() {
    DWORD id = WTSGetActiveConsoleSessionId();
    if (id == 0xFFFFFFFF) {
        throw std::system_error(ERROR_LOGON_NOT_GRANTED, system_category());
    }
    return id;
}

ps::token wts::session_token(DWORD id) {
    HANDLE tok = nullptr;
    BOOL result = WTSQueryUserToken(id, &tok);
    if (!result) {
        throw std::system_error(GetLastError(), system_category(),
                                "WTSQueryUserToken");
    }
    return ps::token{os::handle{tok}};
}

std::wstring wts::session_username(DWORD id) {
    return wts_query_string(id, WTSUserName);
}

std::wstring wts::session_domain(DWORD id) {
    return wts_query_string(id, WTSDomainName);
}

void wts::collect_sessions(std::vector<session_info>& sessions) {
    DWORD level = 1, count = 0;
    const DWORD filter = 0;
    PWTS_SESSION_INFO_1W infos = nullptr;
    BOOL result = ::WTSEnumerateSessionsExW(WTS_CURRENT_SERVER_HANDLE, &level,
                                            filter, &infos, &count);
    if (!result) {
        throw std::system_error{os::make_system_error(::GetLastError())};
    }
    if (!infos) {
        return;
    }
    auto on_exit = scope_exit([infos, count] {
        ::WTSFreeMemoryExW(WTSTypeSessionInfoLevel1, infos, count);
    });
    if (!count) {
        return;
    }

    sessions.reserve(sessions.size() + count);
    for (auto i : range(count)) {
        session_info info;
        info.id = infos[i].SessionId;
        info.state = static_cast<session_state>(infos[i].State);
        info.session_name = infos[i].pSessionName ? infos[i].pSessionName : L"";
        info.user_name = infos[i].pUserName ? infos[i].pUserName : L"";
        info.domain_name = infos[i].pDomainName ? infos[i].pDomainName : L"";
        sessions.emplace_back(std::move(info));
    }
}

impersonater::impersonater(ps::token& token) {
    BOOL result = ImpersonateLoggedOnUser(token.native_handle().get());
    if (!result) {
        throw std::system_error(GetLastError(), system_category(),
                                "ImpersonateLoggedOnUser");
    }
}

impersonater::~impersonater() {
    BOOL result = RevertToSelf();
    assert(result && "Failed to cancel impersonation !");
    if (!result) {
        std::terminate();
    }
}

void profile_loader::load_profile(std::wstring username) {
    netapi_buffer netbuff;
    wchar_t* roaming_profile_path = nullptr;
    DWORD ret = NetUserGetInfo(nullptr, username.c_str(), 4, netbuff.ptr());
    if (ret != NERR_Success || !netbuff.data()) {
        // throw std::system_error(ret, system_category(),
        // "NetUserGetInfo");
        //  try without the romaing profile
    }
    else {
        roaming_profile_path =
            reinterpret_cast<USER_INFO_4*>(netbuff.data())->usri4_profile;
    }
    PROFILEINFOW profile_info{};
    profile_info.dwSize = sizeof(profile_info);
    profile_info.lpUserName = username.data();
    profile_info.lpProfilePath = roaming_profile_path;
    BOOL result = LoadUserProfileW(token_.get(), &profile_info);
    if (!result) {
        throw std::system_error(GetLastError(), system_category(),
                                "LoadUserProfileW");
    }
    profile_ = profile_info.hProfile;
}

profile_loader::~profile_loader() {
    if (profile_) {
        BOOL result = UnloadUserProfile(token_.get(), profile_);
        assert(result && "Failed to unload the user profile !");
        if (!result) {
            std::terminate();
        }
    }
}
