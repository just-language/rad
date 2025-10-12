#pragma once
#include <rad/os_types.h>
#include <rad/string.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <span>
#include <variant>
#include <vector>

extern "C" {
struct SC_HANDLE__;
using SC_HANDLE = SC_HANDLE__*;

struct SERVICE_STATUS_HANDLE__;
using SERVICE_STATUS_HANDLE = SERVICE_STATUS_HANDLE__*;

struct _SERVICE_STATUS;
using SERVICE_STATUS = _SERVICE_STATUS;

struct _ENUM_SERVICE_STATUSW;
using ENUM_SERVICE_STATUSW = _ENUM_SERVICE_STATUSW;

struct _ENUM_SERVICE_STATUS_PROCESSW;
using ENUM_SERVICE_STATUS_PROCESSW = _ENUM_SERVICE_STATUS_PROCESSW;
}

namespace RAD_LIB_NAMESPACE::svc {

    namespace detail {
        void close_handle(SC_HANDLE h) noexcept;
    }

    /// The RAII handle used to hold a windows service handle.
    using service_handle =
        std::unique_ptr<void,
                        os::handle_deleter<SC_HANDLE, detail::close_handle>>;

    /// The windows service startup type.
    enum class startup : DWORD {
        auto_start = 0x00000002,
        boot_start = 0x00000000, // drivers only
        demand_start = 0x00000003,
        system_start = 0x00000001, // drivers only
        disabled = 0x00000004
    };

    /// The windows service error priority.
    enum class service_error : DWORD {
        critical = 0x00000003, // restart on failure
        severe = 0x00000002,   // restart on failure
        normal = 0x00000001,
        ignore = 0x00000000
    };

    /// The windows service type.
    enum class service_type : DWORD {
        none = 0,
        adapter = 0x00000004,
        filesystem_driver = 0x00000002,
        kernel_driver = 0x00000001,
        recogonizer_driver = 0x00000008,
        driver = kernel_driver | filesystem_driver | recogonizer_driver,
        win32_own_process = 0x00000010,
        win32_shared_process = 0x00000020,
        win32 = win32_own_process | win32_shared_process,
        user_own_process = 0x00000050,
        user_shared_process = 0x00000060,
        user = user_own_process | user_shared_process,
        own = win32_own_process | user_own_process,
        shared = win32_shared_process | user_shared_process,
        all = adapter | driver | win32 | user,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(service_type);

    /// The type of services to list
    enum class list_service_type : DWORD {
        none = 0,
        kernel_driver = 0x00000001,
        filesystem_driver = 0x00000002,
        driver = 0xb,
        win32_own_process = 0x00000010,
        win32_shared_process = 0x00000020,
        win32 = win32_own_process | win32_shared_process,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(list_service_type);

    /// The windows service controller access.
    enum class sc_access : DWORD {
        all = 0x10000000L,
        write = 0x40000000L,
        read = 0x80000000L,
        execute = 0x20000000L,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(sc_access);

    /// The windows service access.
    enum class service_access : DWORD {
        all = 983551,
        write = 0x40000000L,
        read = 0x80000000L,
        execute = 0x20000000L,
        change_config = 0x0002,
        query_config = 0x0001,
        query_status = 0x0004,
        start = 0x0010,
        stop = 0x0020,
        user_code = 0x0100
    };

    RAD_OVERLOAD_ENUM_OPERATORS(service_access);

    /// The windows service failure action type.
    enum class failure_action : DWORD {
        none,
        restart_service,
        reboot_computer,
        run_command,
    };

    /// The windows service protection.
    enum class protection : DWORD {
        none,
        windows,
        windows_light,
        anti_malware_light,
    };

    /// The active service state filter. Used to list services.
    enum class active_state : DWORD {
        active = 1,
        inactive,
        all,
    };

    /// The windows service state
    enum class service_state : DWORD {
        stopped = 1,
        start_pending,
        stop_pending,
        running,
        resume_pending,
        pause_pending,
        paused,
    };

    /*!
     * @brief Action taken on service failure.
     */
    struct sc_action {
        /// The action taken on service failure.
        failure_action action = failure_action::none;
        /// The action delay time in milli seconds
        DWORD delay_ms = 0;

        sc_action() = default;

        sc_action(const failure_action action, unsigned long delay_time_ms)
            : action(action), delay_ms(delay_time_ms) {
        }
    };

    /*!
     * @brief Properties of a windows service.
     */
    struct service_properties {
        /// The name of a service in the service control manager
        /// database.
        std::wstring name;
        /// A display name that can be used by service control
        /// programs.
        std::wstring display;
        /// Type of service.
        service_type type = {};
        /// The error code that the service uses to report an
        /// error that occurs when it is starting or stopping.
        DWORD win32_exit_code = 0;
        /*!
         * @brief The service-specific error code that the
         * service returns when an error occurs while the
         * service is starting or stopping.
         */
        DWORD specific_exit_code = 0;
        /*!
         * @brief The check-point value that the service
         * increments periodically to report its progress during
         * a lengthy start, stop, pause, or continue operation.
         */
        DWORD check_point = 0;
        /*!
         * @brief The estimated time required for a pending
         * start, stop, pause, or continue operation, in
         * milliseconds.
         */
        DWORD wait_hint = 0;
        /// The process identifier of the service.
        DWORD pid = 0;
        /// Whether the service is currently running or not.
        bool running = false;
        /// Whether the service is running in a system process
        /// or not.
        bool system_process = false;

    public:
        service_properties() = default;

        RAD_EXPORT_DECL
        service_properties(const ENUM_SERVICE_STATUS_PROCESSW& info);

        RAD_EXPORT_DECL
        service_properties(const ENUM_SERVICE_STATUSW& info);
    };

    /*!
     * @brief Supported service events notifications.
     */
    enum class service_notification : DWORD {
        none = 0,
        stop = 1 << 0,
        pause_resume = 1 << 1,
        shutdown = 1 << 2,
        param_change = 1 << 3,
        net_bind_change = 1 << 4,
        hardware_profile_change = 1 << 5,
        power_event = 1 << 6,
        session_change = 1 << 7,
        pre_shutdown = 1 << 8,
        time_change = 1 << 9,
        trigger_event = 1 << 10,
        user_logoff = 1 << 11,
        low_resources = 1 << 13,
        system_low_resources = 1 << 14,
        all = stop | pause_resume | shutdown | param_change | net_bind_change |
              hardware_profile_change | power_event | session_change |
              pre_shutdown | time_change | trigger_event | user_logoff |
              low_resources | system_low_resources,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(service_notification);

    /*!
     * @brief Status of a windows service.
     * Same layout as _SERVICE_STATUS.
     */
    struct service_status {
        service_type type = service_type::win32_own_process;
        service_state state = service_state::stopped;
        service_notification accepted_controls = service_notification::none;
        DWORD win32_exit_code = 0;
        DWORD specific_exit_code = 0;
        DWORD check_point = 0;
        DWORD wait_hint = 0;

        _SERVICE_STATUS& as_base() {
            return *reinterpret_cast<_SERVICE_STATUS*>(this);
        }
    };

    enum class session_change_type {
        console_connect = 0x1,        // WTS_CONSOLE_CONNECT
        console_disconnect = 0x2,     // WTS_CONSOLE_DISCONNECT
        remote_connect = 0x3,         // WTS_REMOTE_CONNECT
        remote_disconnect = 0x4,      // WTS_REMOTE_DISCONNECT
        session_logon = 0x5,          // WTS_SESSION_LOGON
        session_logoff = 0x6,         // WTS_SESSION_LOGOFF
        session_lock = 0x7,           // WTS_SESSION_LOCK
        session_unlock = 0x8,         // WTS_SESSION_UNLOCK
        session_remote_control = 0x9, // WTS_SESSION_REMOTE_CONTROL
        session_create = 0xa,         // WTS_SESSION_CREATE
        session_terminate = 0xb,      // WTS_SESSION_TERMINATE
    };

    /*!
     * @brief Interface implemented by services implementations to
     * run provide the main service entry function and receive
     * service events.
     */
    struct service_worker {
        virtual ~service_worker() = default;

        /*!
         * @brief Get the events this service wants to subscribe
         * to. By default, all events are subscribed to.
         * @return The events this service wants to subscribe
         * to.
         */
        virtual service_notification notifications() {
            return service_notification::all;
        }

        /*!
         * @brief The main service function.
         * @param argc Count of arguments.
         * @param argv Array of UTF-16 arguments.
         */
        virtual void main([[maybe_unused]] DWORD argc,
                          [[maybe_unused]] wchar_t** argv) {
        }

        /*!
         * @brief Called on the service start.
         */
        virtual void on_start() {
        }

        /*!
         * @brief Called on the service stop.
         */
        virtual void on_stop() noexcept {
        }

        /*!
         * @brief Called on the service pause event.
         */
        virtual void on_pause() {
        }

        /*!
         * @brief Called on the service resume event.
         */
        virtual void on_resume() {
        }

        /*!
         * @brief Called on the system shutdown.
         */
        virtual void on_shutdown() {
        }

        /*!
         * @brief Called before the system shutdown.
         */
        virtual void on_pre_shutdown() {
        }

        /*!
         * @brief Called on the system reboot.
         */
        virtual void on_reboot() {
        }

        /*!
         * @brief Called on the session change.
         * @param change_type Type of session change.
         * @param session_id The id of the session.
         */
        virtual void
        on_session_change([[maybe_unused]] session_change_type change_type,
                          [[maybe_unused]] DWORD session_id) {
        }
    };

    /*!
     * @brief Instance of a windows service that will run
     * in the current process.
     * To run a service construct a service object and provide name and
     * worker then call start() which will run the service and wait until it
     * stops. Only one instance of a service can run in the process, if
     * another service is started after the first one, the behavior is
     * undefined.
     */
    class service {
    public:
        /*!
         * @brief Construct an empty service.
         * The service can't start until name and worker are
         * provided.
         */
        service() = default;

        /*!
         * @brief Construct a service with UTF-8 name and
         * worker.
         * @param service_name The UTF-8 service name.
         * @param srv_worker The service worker.
         */
        service(std::string_view service_name, service_worker& srv_worker)
            : worker_ptr_(&srv_worker), name_(to_wstring(service_name)) {
        }

        /*!
         * @brief Construct a service with UTF-16 name and
         * worker.
         * @param service_name The UTF-16 service name.
         * @param srv_worker The service worker.
         */
        service(wzstring_view service_name, service_worker& srv_worker)
            : worker_ptr_(&srv_worker), name_(service_name) {
        }

        /*!
         * @brief Start the service
         */
        RAD_EXPORT_DECL void start();

        /*!
         * @brief Set the worker of the service that provides
         * the service main function and events callbacks
         * @param srv_woker the worker to associate with the
         * service. This worker must stay valid until start()
         * method returns
         */
        void worker(service_worker& srv_woker) {
            worker_ptr_ = &srv_woker;
        }

        /*!
         * @brief Get a pointer to the attached worker.
         * @return Pointer to the attached worker.
         */
        service_worker* worker() {
            return worker_ptr_;
        }

        /*!
         * @brief Set the name of the service to start. This is
         * necessary if the service is shared process. If the
         * service is own process the name can be left empty
         * @param service_name the name of the service to start
         */
        void name(std::string_view service_name) {
            name_ = to_wstring(service_name);
        }

        /*!
         * @brief Set the name of the service to start. This is
         * necessary if the service is shared process. If the
         * service is own process the name can be left empty
         * @param service_name the name of the service to start
         */
        void name(std::wstring_view service_name) {
            name_ = service_name;
        }

        /*!
         * @brief Get the name of the service.
         * @return The name of the service.
         */
        const std::wstring& name() const noexcept {
            return name_;
        }

    private:
        service_status srv_status_;
        SERVICE_STATUS_HANDLE srv_status_handle_ = nullptr;
        service_worker* worker_ptr_ = nullptr;
        std::wstring name_;
        DWORD checkpoint_ = 0;
        std::atomic<bool> service_stopped_ = false;
        std::optional<std::exception_ptr> ex_ptr_;

        static service& inst(service* p = nullptr);

        static void __stdcall service_start_main_impl(DWORD argc,
                                                      wchar_t** argv) noexcept;

        static DWORD __stdcall service_ctrl_handler_impl(DWORD ctrl,
                                                         DWORD ev_type,
                                                         void* ev_data,
                                                         void* ctx) noexcept;

        void main_impl(DWORD argc, wchar_t** argv);

        DWORD ctrl_handler_impl(DWORD ctrl, DWORD ev_type, void* ev_data);

        void pause();

        void resume();

        void stop();

        void shutdown();

        void pre_shutdown();

        void session_changed(DWORD wParam, DWORD SessionId);

        void init_status(service_state current_status, DWORD wait_hint = 0,
                         DWORD error = 0);
    };

    /*!
     * @brief A dependency service that another service is depending on.
     */
    struct service_dependency {
        /// The UTF-16 name of the dependency service.
        std::wstring name;
    };

    /*!
     * @brief A dependency load ordering group that another service is
     * depending on.
     */
    struct group_dependency {
        /// The UTF-16 name of the dependency load ordering
        /// group.
        std::wstring name;
    };

    /// A UTF-16 dependency service or load ordering group.
    class dependency {
    public:
        dependency() = default;

        /*!
         * @brief Construct a dependency.
         * @param name The UTF-16 name of the dependency.
         * @param is_group Whether this is a service or load
         * ordering group.
         */
        dependency(std::wstring_view name, bool is_group = false)
            : name_{name}, is_group_{is_group} {
        }

        /*!
         * @brief Get the UTF-16 name of the dependency.
         * @return The UTF-16 name of the dependency.
         */
        const std::wstring& name() const noexcept {
            return name_;
        }

        /*!
         * @brief Check if this dependency is a service.
         * @return True if this dependency is a service,
         * otherwise false.
         */
        bool is_service() const noexcept {
            return !is_group_;
        }

        /*!
         * @brief Check if this dependency is a load ordering
         * group.
         * @return True if this dependency is a load ordering
         * group, otherwise false.
         */
        bool is_group() const noexcept {
            return is_group_;
        }

    private:
        std::wstring name_;
        bool is_group_ = false;
    };

    /// A UTF-8 dependency service or load ordering group.
    class dependency_utf8 {
    public:
        dependency_utf8() = default;

        /*!
         * @brief Construct a dependency.
         * @param name The UTF-8 name of the dependency.
         * @param is_group Whether this is a service or load
         * ordering group.
         */
        dependency_utf8(std::string_view name, bool is_group = false)
            : name_{name}, is_group_{is_group} {
        }

        /*!
         * @brief Convert a UTF-16 dependency to a UTF-8
         * dependency.
         * @param d The UTF-16 dependency.
         */
        explicit dependency_utf8(const dependency& d)
            : name_{to_string(d.name())}, is_group_{d.is_group()} {
        }

        /*!
         * @brief Get the UTF-8 name of the dependency.
         * @return The UTF-8 name of the dependency.
         */
        const std::string& name() const noexcept {
            return name_;
        }

        /*!
         * @brief Check if this dependency is a service.
         * @return True if this dependency is a service,
         * otherwise false.
         */
        bool is_service() const noexcept {
            return !is_group_;
        }

        /*!
         * @brief Check if this dependency is a load ordering
         * group.
         * @return True if this dependency is a load ordering
         * group, otherwise false.
         */
        bool is_group() const noexcept {
            return is_group_;
        }

    private:
        std::string name_;
        bool is_group_ = false;
    };

    /*!
     * @brief A service controller using a service handle.
     */
    class service_controller {
    public:
        /// The type of native handle.
        using native_handle_type = service_handle;

        /*!
         * @brief Construct an invalid service controller.
         */
        service_controller() = default;

        /*!
         * @brief Open a service controller by name and specify
         * the desired access.
         * @param name Name of service to open a handle for.
         * @param access The desired access to the service.
         */
        template <class StringType>
        service_controller(const StringType& name,
                           service_access access = service_access::all) {
            open(name, access);
        }

        /*!
         * @brief Use an existing open service handle.
         * @param handle Handle of open service.
         */
        service_controller(native_handle_type handle) noexcept
            : srv_handle{std::move(handle)} {
        }

        /*!
         * @brief Get a reference to the native handle.
         * @return A reference to the native handle.
         */
        native_handle_type& native_handle() noexcept {
            return srv_handle;
        }

        /*!
         * @brief Get a const reference to the native handle.
         * @return A const reference to the native handle.
         */
        const native_handle_type& native_handle() const noexcept {
            return srv_handle;
        }

    private:
        RAD_EXPORT_DECL void
        open_impl(wzstring_view name,
                  service_access access = service_access::all);

    public:
        /*!
         * @brief Open a service controller by name and specify
         * the desired access.
         * @param name Name of service to open a handle for.
         * @param access The desired access to the service.
         */
        template <class StringType>
        void open(const StringType& name,
                  service_access access = service_access::all) {
            open_impl(os::get_wstring(name), access);
        }

        /*!
         * @brief Close the service handle if it is open.
         */
        void close() noexcept {
            srv_handle.reset();
        }

    private:
        RAD_EXPORT_DECL void set_description_impl(wzstring_view description);

        RAD_EXPORT_DECL void set_display_impl(wzstring_view display_name);

        RAD_EXPORT_DECL void set_bin_path_impl(wzstring_view path);

        RAD_EXPORT_DECL void
        failure_actions_impl(std::span<const sc_action> actions,
                             bool use_timeout, DWORD reset_period_seconds,
                             bool use_strings, wzstring_view command,
                             wzstring_view reboot_msg);

    public:
        /*!
         * @brief Set the service description text.
         * @param description The service description text.
         */
        template <class StringType>
        void description(const StringType& description) {
            set_description_impl(os::get_wstring(description));
        }

        /*!
         * @brief Set the service display name.
         * @param display_name The service display name.
         */
        template <class StringType>
        void display(const StringType& display_name) {
            set_display_impl(os::get_wstring(display_name));
        }

        /*!
         * @brief Set the service type.
         * @param service_type The service type.
         */
        RAD_EXPORT_DECL void type(const service_type service_type);

        /*!
         * @brief Set the service startup type.
         * @param startup_type The service startup type.
         */
        RAD_EXPORT_DECL void startup_type(const startup startup_type);

        /*!
         * @brief Set the service error priority.
         * @param error_priority The service error priority.
         */
        RAD_EXPORT_DECL void error_priority(service_error error_priority);

        /*!
         * @brief Set the service binary path.
         * @param path The service binary path.
         */
        template <class StringType>
        void binary_path(const StringType& path) {
            set_bin_path_impl(os::get_wstring(path));
        }

        /*!
         * @brief Set the service dependencies.
         * @param dependencies The service dependencies.
         */
        RAD_EXPORT_DECL void
        set_dependencies(std::span<const dependency> dependencies);

        /*!
         * @brief Set the service failure actions.
         * Process command line and reboot messages are not
         * changed. The reset period will be never reset.
         * @param actions The service failure actions.
         * @param reset_period The time after which to reset the
         * failure count to zero if there are no failures.
         * Specify negative value to indicate that this value
         * should never be reset.
         */
        template <class Rep, class Period>
        void failure_actions(std::span<const sc_action> actions) {
            failure_actions_impl(actions, false, 0, false, L"", L"");
        }

        /*!
         * @brief Set the service failure actions.
         * Process command line and reboot messages are not
         * changed.
         * @param actions The service failure actions.
         * @param reset_period The time after which to reset the
         * failure count to zero if there are no failures.
         * Specify negative value to indicate that this value
         * should never be reset.
         */
        template <class Rep, class Period>
        void failure_actions(std::span<const sc_action> actions,
                             std::chrono::duration<Rep, Period> reset_period) {
            const bool use_timeout =
                reset_period.count() >= 0 &&
                reset_period.count() < std::numeric_limits<DWORD>::max();
            failure_actions_impl(actions, use_timeout,
                                 static_cast<DWORD>(reset_period.count()),
                                 false, L"", L"");
        }

        /*!
         * @brief Set the service failure actions.
         * The reset period will be never reset.
         * @param actions The service failure actions.
         * @param command The command line of the process for
         * the CreateProcess function to execute in response to
         * the SC_ACTION_RUN_COMMAND service controller action.
         * If an empty string is passed, the existing command is
         * deleted. This process runs under the same account as
         * the service.
         * @param reboot_msg The message to be broadcast to
         * server users before rebooting in response to the
         * SC_ACTION_REBOOT service controller action. If an
         * empty string is passed, the existing reboot message
         * is deleted.
         */
        template <class StringType1, class StringType2>
        void failure_actions(std::span<const sc_action> actions,
                             const StringType1& command,
                             const StringType2& reboot_msg) {
            failure_actions_impl(actions, false, 0, true,
                                 os::get_wstring(command),
                                 os::get_wstring(reboot_msg));
        }

        /*!
         * @brief Set the service failure actions.
         * @param actions The service failure actions.
         * @param reset_period The time after which to reset the
         * failure count to zero if there are no failures.
         * Specify negative value to indicate that this value
         * should never be reset.
         * @param command The command line of the process for
         * the CreateProcess function to execute in response to
         * the SC_ACTION_RUN_COMMAND service controller action.
         * If an empty string is passed, the existing command is
         * deleted. This process runs under the same account as
         * the service.
         * @param reboot_msg The message to be broadcast to
         * server users before rebooting in response to the
         * SC_ACTION_REBOOT service controller action. If an
         * empty string is passed, the existing reboot message
         * is deleted.
         */
        template <class Rep, class Period, class StringType1, class StringType2>
        void failure_actions(std::span<const sc_action> actions,
                             std::chrono::duration<Rep, Period> reset_period,
                             const StringType1& command,
                             const StringType2& reboot_msg) {
            const bool use_timeout =
                reset_period.count() >= 0 &&
                reset_period.count() < std::numeric_limits<DWORD>::max();
            failure_actions_impl(
                actions, use_timeout, static_cast<DWORD>(reset_period.count()),
                true, os::get_wstring(command), os::get_wstring(reboot_msg));
        }

    private:
        RAD_EXPORT_DECL void pre_shutdown_time_impl(DWORD time_ms);

    public:
        /*!
         * @brief Set the pre shutdown timeout.
         * @param timeout The timeout to for the system to wait
         * before shutdown.
         */
        template <class Rep, class Period>
        void pre_shutdown_timeout(std::chrono::duration<Rep, Period> timeout) {
            using namespace std::chrono;
            pre_shutdown_time_impl(
                duration_cast<milliseconds>(timeout).count());
        }

        /*!
         * @brief Set the service launch protection.
         * @param protection The service launch protection.
         */
        RAD_EXPORT_DECL void protection(protection protection);

        /*!
         * @brief Start the controlled service with ANSI
         * arguments.
         * @param argc Count of arguments.
         * @param argv Array of pointers to ANSI null terminated
         * strings.
         */
        RAD_EXPORT_DECL void start(DWORD argc, const char** argv);

        /*!
         * @brief Start the controlled service with UTF-16
         * arguments.
         * @param argc Count of arguments.
         * @param argv Array of pointers to UTF-16 null
         * terminated strings.
         */
        RAD_EXPORT_DECL void start(DWORD argc, const wchar_t** argv);

        /*!
         * @brief Start the controlled service without
         * arguments.
         */
        void start(std::nullptr_t) {
            start(0, static_cast<const wchar_t**>(nullptr));
        }

        /*!
         * @brief Start the controlled service without
         * arguments.
         */
        void start() {
            start(nullptr);
        }

        /*!
         * @brief Start the controlled service with ANSI
         * arguments.
         * @param args The arguments to pass to the service main
         * function.
         */
        RAD_EXPORT_DECL void start(const std::vector<std::string>& args);

        /*!
         * @brief Start the controlled service with UTF-16
         * arguments.
         * @param args The arguments to pass to the service main
         * function.
         */
        RAD_EXPORT_DECL void start(const std::vector<std::wstring>& args);

        /*!
         * @brief Stop the controlled service by sending a stop
         * command.
         */
        RAD_EXPORT_DECL void stop();

        /*!
         * @brief Resume the controlled service by sending a
         * resume command.
         */
        RAD_EXPORT_DECL void resume();

        /*!
         * @brief Pause the controlled service by sending a
         * pause command.
         */
        RAD_EXPORT_DECL void pause();

        /*!
         * @brief Send a control code to the controlled service
         * using ControlService function.
         * @param code The code to send to the service.
         */
        RAD_EXPORT_DECL void execute_code(DWORD code);

        /*!
         * @brief Get the service type.
         * @return The service type.
         */
        RAD_EXPORT_DECL service_type type() const;

        /*!
         * @brief Get the service startup type.
         * @return The service startup type.
         */
        RAD_EXPORT_DECL startup startup_type() const;

        /*!
         * @brief Get the service error priority.
         * @return The service error priority.
         */
        RAD_EXPORT_DECL service_error error_priority() const;

        /*!
         * @brief Get the service description as UTF-16 string.
         * @return The service description as UTF-16 string.
         */
        RAD_EXPORT_DECL std::wstring description() const;

        /*!
         * @brief Get the service description as UTF-8 string.
         * @return The service description as UTF-8 string.
         */
        std::string description_utf8() const {
            return to_string(description());
        }

        /*!
         * @brief Get the service binary path as UTF-16 string.
         * @return The service binary path as UTF-16 string.
         */
        RAD_EXPORT_DECL std::wstring binary_path() const;

        /*!
         * @brief Get the service binary path as UTF-8 string.
         * @return The service binary path as UTF-8 string.
         */
        std::string binary_path_utf8() const {
            return to_string(binary_path());
        }

        /*!
         * @brief Get the service dependencies as UTF-16
         * strings.
         * @param deps Dependencies will be appended to this
         * vector.
         */
        RAD_EXPORT_DECL void
        get_dependencies(std::vector<dependency>& deps) const;

        /*!
         * @brief Get the service dependencies as UTF-8 strings.
         * @param deps Dependencies will be appended to this
         * vector.
         */
        void get_dependencies(std::vector<dependency_utf8>& deps) const {
            std::vector<dependency> wdeps;
            get_dependencies(wdeps);
            deps.reserve(wdeps.size());
            for (const auto& dep : wdeps) {
                deps.emplace_back(dependency_utf8{dep});
            }
        }

        /*!
         * @brief Get the service dependencies as UTF-16
         * strings.
         * @return The service dependencies as UTF-16 strings.
         */
        std::vector<dependency> dependencies() const {
            std::vector<dependency> deps;
            get_dependencies(deps);
            return deps;
        }

        /*!
         * @brief Get the service dependencies as UTF-8 strings.
         * @return The service dependencies as UTF-8 strings.
         */
        std::vector<dependency_utf8> dependencies_utf8() const {
            std::vector<dependency_utf8> deps;
            get_dependencies(deps);
            return deps;
        }

        /*!
         * @brief Get UTF-16 name and display of services that
         * depend on the current controlled service.
         * @param deps The services that depend on the current
         * controlled service.
         * @param state The active state filter.
         */
        RAD_EXPORT_DECL void
        get_dependents(std::vector<service_properties>& deps,
                       active_state state = active_state::all) const;

        /*!
         * @brief Get UTF-16 name and display of services that
         * depend on the current controlled service.
         * @param state The active state filter.
         * @return The services that depend on the current
         * controlled service.
         */
        std::vector<service_properties>
        dependents(active_state state = active_state::all) const {
            std::vector<service_properties> deps;
            get_dependents(deps, state);
            return deps;
        }

        /*!
         * @brief Get the service display name as UTF-16 string.
         * @return The service display name as UTF-16 string.
         */
        RAD_EXPORT_DECL std::wstring display() const;

        /*!
         * @brief Get the service display name as UTF-8 string.
         * @return The service display name as UTF-8 string.
         */
        std::string display_utf8() const {
            return to_string(display());
        }

        /*!
         * @brief Get the status of this controlled service.
         * @return The status of this controlled service.
         */
        RAD_EXPORT_DECL service_status status() const;

        /*!
         * @brief Check if this controlled service is in the
         * start pending state.
         * @return True if this controlled service is in the
         * start pending state, otherwise false.
         */
        bool start_pending() const {
            return status().state == service_state::start_pending;
        }

        /*!
         * @brief Check if this controlled service is in the
         * running state.
         * @return True if this controlled service is in the
         * running state, otherwise false.
         */
        bool running() const {
            return status().state == service_state::running;
        }

        /*!
         * @brief Check if this controlled service is in the
         * pause state.
         * @return True if this controlled service is in the
         * pause state, otherwise false.
         */
        bool paused() const {
            return status().state == service_state::paused;
        }

        /*!
         * @brief Check if this controlled service is in the
         * stop state.
         * @return True if this controlled service is in the
         * stop state, otherwise false.
         */
        bool stopped() const {
            return status().state == service_state::stopped;
        }

        /*!
         * @brief Check if this controlled service is in the
         * resume pending state.
         * @return True if this controlled service is in the
         * resume pending state, otherwise false.
         */
        bool resume_pending() const {
            return status().state == service_state::resume_pending;
        }

        /*!
         * @brief Check if this controlled service is in the
         * pause pending state.
         * @return True if this controlled service is in the
         * pause pending state, otherwise false.
         */
        bool pause_pending() const {
            return status().state == service_state::pause_pending;
        }

        /*!
         * @brief Check if this controlled service is in the
         * stop pending state.
         * @return True if this controlled service is in the
         * stop pending state, otherwise false.
         */
        bool stop_pending() const {
            return status().state == service_state::stop_pending;
        }

        /*!
         * @brief Check if the service controller handle is
         * valid.
         * @return True if the service controller handle is
         * valid, otherwise false.
         */
        bool valid() const {
            return static_cast<bool>(srv_handle);
        };

        /*!
         * @brief Check if the service controller handle is
         * valid.
         */
        explicit operator bool() const noexcept {
            return valid();
        }

    private:
        native_handle_type srv_handle;
        std::wstring srv_name;
    };

    namespace detail {
        RAD_EXPORT_DECL service_controller install_service_impl(
            wzstring_view path, wzstring_view service_name,
            wzstring_view service_display_name, startup startup_type,
            service_type service_type, service_error error_priority);

        RAD_EXPORT_DECL void uninstall_service_impl(wzstring_view service_name);
    } // namespace detail

    /*!
     * @brief Install a windows service.
     * @param path The path to the service executable.
     * @param service_name The name of a service in the service control
     * manager database.
     * @param service_display_name A display name that can be used by
     * service control programs.
     * @param startup_type The service startup type.
     * @param service_type The service type.
     * @param error_priority The service error priority.
     * @return Handle for the newly installed service to control.
     */
    template <class StringType, class StringType2, class StringType3>
    service_controller
    install_service(const StringType& path, const StringType2& service_name,
                    const StringType3& service_display_name,
                    startup startup_type = startup::auto_start,
                    service_type service_type = service_type::win32_own_process,
                    service_error error_priority = service_error::ignore) {
        return detail::install_service_impl(
            os::get_wstring(path), os::get_wstring(service_name),
            os::get_wstring(service_display_name), startup_type, service_type,
            error_priority);
    }

    /*!
     * @brief Uninstall a windows service.
     * @tparam StringType Type of string.
     * @param service_name The service name to uninstall.
     */
    template <class StringType>
    inline void uninstall_service(const StringType& service_name) {
        detail::uninstall_service_impl(os::get_wstring(service_name));
    }

    /*!
     * @brief Get a list of windows services with filter by active state and
     * service type.
     * @param services The vector where new services properties will be
     * appended.
     * @param type The type filter of services to list.
     * @param state The state filter of services to list.
     */
    RAD_EXPORT_DECL void
    list_services(std::vector<service_properties>& services,
                  list_service_type type, active_state state);

    /*!
     * @brief Get a list of windows services with filter by active state and
     * service type.
     * @param type The type filter of services to list.
     * @param state The state filter of services to list.
     * @return A vector of services properties.
     */
    inline std::vector<service_properties> list_services(list_service_type type,
                                                         active_state state) {
        std::vector<service_properties> services;
        list_services(services, type, state);
        return services;
    }

}; // namespace RAD_LIB_NAMESPACE::svc
