#pragma once
#include <rad/libbase.h>
#include <rad/os_types.h>
#include <rad/windows/process.h>
#include <rad/windows/winreg.h>

namespace RAD_LIB_NAMESPACE::wts {
    enum class session_state {
        active,        // User logged on to WinStation
        connected,     // WinStation connected to client
        connect_query, // In the process of connecting to client
        shadow,        // Shadowing another WinStation
        disconnected,  // WinStation logged on without client
        idle,          // Waiting for client to connect
        listen,        // WinStation is listening for connection
        reset,         // WinStation is being reset
        down,          // WinStation is down due to error
        init,          // WinStation in initialization
    };

    struct session_info {
        uint32_t id = 0;
        session_state state = {};
        std::wstring session_name;
        std::wstring user_name;
        std::wstring domain_name;
    };

    std::pair<bool, DWORD> is_user_logged_in();

    DWORD active_session_id();

    ps::token session_token(DWORD id);

    inline ps::token active_session_token() {
        return session_token(active_session_id());
    }

    void collect_sessions(std::vector<session_info>& sessions);

    inline std::vector<session_info> collect_sessions() {
        std::vector<session_info> sessions;
        collect_sessions(sessions);
        return sessions;
    }

    struct impersonater : noncopyable, nonmovable {
        impersonater(ps::token& token);

        ~impersonater();
    };

    std::wstring session_username(DWORD id);

    inline std::string session_username_utf8(DWORD id) {
        return to_string(session_username(id));
    }

    inline std::wstring active_session_username() {
        return session_username(active_session_id());
    }

    inline std::string active_session_username_utf8() {
        return session_username_utf8(active_session_id());
    }

    std::wstring session_domain(DWORD id);

    inline std::string session_domain_utf8(DWORD id) {
        return to_string(session_domain(id));
    }

    inline std::wstring active_session_domain() {
        return session_domain(active_session_id());
    }

    inline std::string active_session_domain_utf8() {
        return session_domain_utf8(active_session_id());
    }

    class profile_loader : noncopyable, nonmovable {
    public:
        // load the user profile identified by token and user
        // name returned from active_session_username
        profile_loader(rad::ps::token& token) : token_{token.native_handle()} {
            load_profile(active_session_username());
        }

        // load the user profile identified by token and
        // username
        profile_loader(rad::ps::token& token, std::wstring username)
            : token_{token.native_handle()} {
            load_profile(std::move(username));
        }

        ~profile_loader();

        // get the current user key for the impersonated user
        winreg::key_view user_key() {
            using namespace winreg;

            static_assert(sizeof(defined_keys) >= sizeof(profile_),
                          "size of defined keys doesn't fit for an "
                          "HKEY");

            return key_view(static_cast<defined_keys>(
                reinterpret_cast<uintptr_t>(profile_)));
        }

    private:
        void load_profile(std::wstring username);

        void* profile_ = nullptr;
        os::handle& token_;
    };

} // namespace RAD_LIB_NAMESPACE::wts