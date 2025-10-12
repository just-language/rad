#pragma once
#include <rad/os_types.h>
#include <rad/string.h>

#include <rad/windows/clock.h>

namespace RAD_LIB_NAMESPACE::ps {
    enum class spawn_flags : uint32_t {
        none = 0,
        debug = 0x00000001,
        debug_first = 0x00000002,
        suspended = 0x00000004,
        detached = 0x00000008,
        new_console = 0x00000010,
        new_group = 0x00000200,
        unicode_env = 0x00000400,
        extended_support_peresent = 0x00080000,
        no_window = 0x08000000,
        protected_process = 0x00040000,
        secure_process = 0x00400000,
    };

    enum class process_access {
        terminate = 1 << 0,
        create_thread = 1 << 1,
        set_session_id = 1 << 2,
        vm_operation = 1 << 3,
        vm_read = 1 << 4,
        vm_write = 1 << 5,
        duplicate_handles = 1 << 6,
        create_process = 1 << 7,
        set_quota = 1 << 8,
        set_information = 1 << 9,
        query_information = 1 << 10,
        suspend_resume = 1 << 11,
        query_limited_information = 1 << 12,
        set_limited_information = 1 << 13,
        synchronize = 0x00100000L,
        all = 0x000F0000L | 0x00100000L | 0xFFFF,
    };

    enum class thread_access {
        terminate = 1 << 0,
        suspend_resume = 1 << 1,
        get_context = 1 << 3,
        set_context = 1 << 4,
        set_information = 1 << 5,
        query_information = 1 << 6,
        set_token = 1 << 7,
        impersonate = 1 << 8,
        direct_impersonation = 1 << 9,
        set_limited_information = 1 << 10,
        query_limited_information = 1 << 11,
        resume = 1 << 12,
        synchronize = 0x00100000L,
        all = 2097151,
    };

    enum class token_access {
        assign_primary = 1 << 0,
        duplicate = 1 << 1,
        impersonate = 1 << 2,
        query = 1 << 3,
        query_source = 1 << 4,
        adjust_privileges = 1 << 5,
        adjust_groups = 1 << 6,
        adjust_default = 1 << 7,
        adjust_session_id = 1 << 8,
        execute = 0x00020000L,
        read = execute | query,
        write = execute | adjust_privileges | adjust_groups | adjust_default,
        all = 983551,
        max_allowed = 0x02000000,
    };

    enum class impersonation_level {
        anonymous,
        identification,
        impersonation,
        delegation,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(spawn_flags);

    RAD_OVERLOAD_ENUM_OPERATORS(process_access);

    RAD_OVERLOAD_ENUM_OPERATORS(thread_access);

    RAD_OVERLOAD_ENUM_OPERATORS(token_access);

    enum class wait_result {
        timeout,
        not_timeout,
    };

    class token {
    public:
        using native_handle_type = os::handle;

        token() = default;

        explicit token(native_handle_type tok) noexcept : tok_{std::move(tok)} {
        }

        native_handle_type& native_handle() noexcept {
            return tok_;
        }

        const native_handle_type& native_handle() const noexcept {
            return tok_;
        }

        bool is_valid() const noexcept {
            return static_cast<bool>(tok_);
        }

        explicit operator bool() const {
            return is_valid();
        }

        RAD_EXPORT_DECL token duplicate(token_access access,
                                        impersonation_level level,
                                        bool for_spawn);

        RAD_EXPORT_DECL auto username_domain() const
            -> std::pair<std::wstring, std::wstring>;

        std::wstring username() const {
            auto [user, domain] = username_domain();
            return user;
        }

        std::wstring domain() const {
            auto [user, domain] = username_domain();
            return domain;
        }

        RAD_EXPORT_DECL bool elevated() const;

        RAD_EXPORT_DECL void set_session_id(uint32_t id);

        RAD_EXPORT_DECL void set_ui_access(bool enable);

    private:
        native_handle_type tok_;
    };

    struct process_times {
        windows_clock::time_point sart_time = {};
        windows_clock::time_point exit_time = {};
        windows_clock::duration kernel_duration = {};
        windows_clock::duration user_curation = {};
    };

    class process {
    public:
        using native_handle_type = os::process_handle;

        process() = default;

        explicit process(native_handle_type proc, uint32_t id = 0) noexcept
            : handle_{std::move(proc)}, id_{id} {
        }

        RAD_EXPORT_DECL static process current();

        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        bool is_valid() const noexcept {
            return static_cast<bool>(handle_);
        }

        explicit operator bool() const noexcept {
            return is_valid();
        }

        bool is_current_process() const noexcept {
            return is_valid() && is_self_;
        }

        uint32_t pid() {
            if (!id_) {
                get_pid();
            }
            return id_;
        }

        RAD_EXPORT_DECL std::wstring path();

        void close() noexcept {
            if (!is_self_) {
                handle_.reset();
            }
        }

        RAD_EXPORT_DECL void terminate(uint32_t exit_code = 0);

        RAD_EXPORT_DECL bool is_running();

        RAD_EXPORT_DECL void wait();

        RAD_EXPORT_DECL wait_result wait(std::chrono::milliseconds time);

        RAD_EXPORT_DECL uint32_t exit_code();

        RAD_EXPORT_DECL process_times times() const;

        RAD_EXPORT_DECL token open_token(token_access access);

    private:
        void get_pid();

        native_handle_type handle_;
        uint32_t id_ = 0;
        bool is_self_ = false;
    };

    class thread {
    public:
        using native_handle_type = os::thread_handle;

        thread() = default;

        explicit thread(native_handle_type thd, uint32_t id = 0) noexcept
            : handle_{std::move(thd)}, id_{id} {
        }

        RAD_EXPORT_DECL static thread current() noexcept;

        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        bool is_valid() const noexcept {
            return static_cast<bool>(handle_);
        }

        explicit operator bool() const noexcept {
            return is_valid();
        }

        bool is_current_thread() const noexcept {
            return is_valid() && is_self_;
        }

        uint32_t id() {
            if (!id_) {
                get_tid();
            }
            return id_;
        }

        void close() noexcept {
            if (!is_self_) {
                handle_.reset();
            }
        }

        RAD_EXPORT_DECL void resume();

        RAD_EXPORT_DECL void suspend();

        RAD_EXPORT_DECL void terminate(uint32_t exit_code = 0);

        RAD_EXPORT_DECL bool is_running();

        RAD_EXPORT_DECL void wait();

        RAD_EXPORT_DECL wait_result wait(std::chrono::milliseconds time);

        RAD_EXPORT_DECL uint32_t exit_code();

        RAD_EXPORT_DECL token open_token(token_access access, bool as_process);

    private:
        void get_tid();

        native_handle_type handle_;
        uint32_t id_ = 0;
        bool is_self_ = false;
    };

    inline process current_process = process::current();
    inline thread current_thread = thread::current();

    struct process_info {
        process process_handle;
        thread thread_handle;
    };

    RAD_EXPORT_DECL process_info launch_process(
        token& tok, wzstring_view path, std::wstring cmdline, process& parent,
        wzstring_view working_dir = {}, spawn_flags flags = spawn_flags::none,
        wzstring_view desktop_name = {});

    RAD_EXPORT_DECL process_info launch_process(
        token& tok, wzstring_view path, std::wstring cmdline,
        wzstring_view working_dir = {}, spawn_flags flags = spawn_flags::none,
        wzstring_view desktop_name = {});

    RAD_EXPORT_DECL process_info launch_process(
        wzstring_view path, std::wstring cmdline,
        wzstring_view working_dir = {}, spawn_flags flags = spawn_flags::none,
        wzstring_view desktop_name = {});

    inline process_info launch_process(token& tok, std::string_view path,
                                       std::string cmdline, process& parent,
                                       std::string_view working_dir = {},
                                       spawn_flags flags = spawn_flags::none,
                                       std::string_view desktop_name = {}) {
        return launch_process(tok, to_wstring(path), to_wstring(cmdline),
                              parent, to_wstring(working_dir), flags,
                              to_wstring(desktop_name));
    }

    inline process_info launch_process(token& tok, std::string_view path,
                                       std::string cmdline,
                                       std::string_view working_dir = {},
                                       spawn_flags flags = spawn_flags::none,
                                       std::string_view desktop_name = {}) {
        return launch_process(tok, to_wstring(path), to_wstring(cmdline),
                              to_wstring(working_dir), flags,
                              to_wstring(desktop_name));
    }

    inline process_info launch_process(std::string_view path,
                                       std::string cmdline,
                                       std::string_view working_dir = {},
                                       spawn_flags flags = spawn_flags::none,
                                       std::string_view desktop_name = {}) {
        return launch_process(to_wstring(path), to_wstring(cmdline),
                              to_wstring(working_dir), flags,
                              to_wstring(desktop_name));
    }
} // namespace RAD_LIB_NAMESPACE::ps
