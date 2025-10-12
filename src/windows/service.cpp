#include <Windows.h>
#include <rad/views/reversed.h>
#include <rad/views/zip.h>
#include <rad/windows/service.h>

#include <cassert>

using namespace RAD_LIB_NAMESPACE;
using namespace svc;

namespace {

    [[noreturn]] inline void throw_os_error() {
        throw std::system_error(os::make_system_error(GetLastError()));
    }

    struct malloc_allocator {
        template <class Ptr>
        static Ptr allocate(std::size_t n) {
            void* p = malloc(n);
            if (!p) {
                throw std::bad_alloc();
            }
            return static_cast<Ptr>(p);
        }

        void operator()(void* p) const noexcept {
            free(p);
        }
    };

    using config_ptr = std::unique_ptr<QUERY_SERVICE_CONFIGW, malloc_allocator>;

    config_ptr get_service_config(const service_handle& srv_handle) {
        DWORD needed_size = 0;
        (void)QueryServiceConfigW(srv_handle.get(), nullptr, 0, &needed_size);
        DWORD last_error = GetLastError();
        if (last_error != ERROR_INSUFFICIENT_BUFFER) {
            assert(last_error != 0);
            throw std::system_error(os::make_system_error(last_error));
        }

        assert(needed_size != 0);

        config_ptr buffer{
            malloc_allocator::allocate<LPQUERY_SERVICE_CONFIGW>(needed_size)};

        if (!QueryServiceConfigW(srv_handle.get(), buffer.get(), needed_size,
                                 &needed_size)) {
            throw_os_error();
        }

        return buffer;
    }

    std::unique_ptr<uint8_t[]>
    get_service_config2(const service_handle& srv_handle, DWORD Type) {
        DWORD needed_size = 0;
        (void)::QueryServiceConfig2W(srv_handle.get(), Type, nullptr, 0,
                                     &needed_size);

        DWORD prev_error = GetLastError();
        if (prev_error != ERROR_INSUFFICIENT_BUFFER || !needed_size) {
            throw std::system_error(os::make_system_error(prev_error));
        }

        std::unique_ptr<uint8_t[]> buff(new uint8_t[needed_size]);
        if (!::QueryServiceConfig2W(srv_handle.get(), Type, buff.get(),
                                    needed_size, &needed_size)) {
            throw_os_error();
        }

        return buff;
    }

} // namespace

void svc::detail::close_handle(SC_HANDLE h) noexcept {
    BOOL res = ::CloseServiceHandle(h);
    assert(res == TRUE);
    ((void)res);
}

void service::start() {
    inst(this);

    assert(worker() != nullptr);
    if (worker() == nullptr) {
        throw std::system_error{
            std::make_error_code(std::errc::invalid_argument)};
    }

    // name ptr can't be null. the function takes wchar_t* although const
    // wchar_t* will suffice
    wchar_t empty_name[] = {L'\0'};
    wchar_t* name_ptr = empty_name;
    if (!name_.empty()) {
        name_ptr = name_.data();
    }
    const SERVICE_TABLE_ENTRYW SrvsTable[] = {
        {name_ptr, service_start_main_impl}, {nullptr, nullptr}};

    if (!::StartServiceCtrlDispatcherW(SrvsTable)) {
        throw_os_error();
    }

    if (ex_ptr_) {
        std::rethrow_exception(*ex_ptr_);
    }
}

service& service::inst(service* p) {
    static service* service_ptr = nullptr;
    if (service_ptr == nullptr) {
        service_ptr = p;
    }
    return *service_ptr;
}

void service::main_impl(DWORD argc, wchar_t** argv) {
    srv_status_handle_ = ::RegisterServiceCtrlHandlerExW(
        name_.c_str(), service_ctrl_handler_impl, this);

    if (!srv_status_handle_) {
        throw_os_error();
    }

    static_assert(sizeof(service_status) == sizeof(SERVICE_STATUS));
    RtlSecureZeroMemory(&srv_status_, sizeof(SERVICE_STATUS));
    srv_status_.type = service_type::win32_own_process;
    srv_status_.state = service_state::start_pending;

    srv_status_.accepted_controls = worker()->notifications();

    if (!::SetServiceStatus(srv_status_handle_, &(srv_status_.as_base()))) {
        throw_os_error();
    }

    worker()->on_start();

    init_status(service_state::running);

    try {
        worker()->main(argc, argv);
    }
    catch (...) {
        ex_ptr_ = std::current_exception();
    }

    if (!service_stopped_) {
        service_ctrl_handler_impl(SERVICE_CONTROL_STOP, 0, nullptr, nullptr);
    }
}

DWORD service::ctrl_handler_impl(DWORD ctrl, DWORD ev_type, void* ev_data) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
        inst().service_stopped_ = true;
        inst().stop();
        return NO_ERROR;

    case SERVICE_CONTROL_PAUSE:
        inst().pause();
        return NO_ERROR;

    case SERVICE_CONTROL_CONTINUE:
        inst().resume();
        return NO_ERROR;

    case SERVICE_CONTROL_SHUTDOWN:
        inst().service_stopped_ = true;
        inst().shutdown();
        return NO_ERROR;

    case SERVICE_CONTROL_PRESHUTDOWN:
        inst().service_stopped_ = true;
        inst().pre_shutdown();
        return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
        inst().session_changed(
            ev_type,
            reinterpret_cast<PWTSSESSION_NOTIFICATION>(ev_data)->dwSessionId);
        return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;

    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

// static
void __stdcall service::service_start_main_impl(DWORD argc,
                                                wchar_t** argv) noexcept {
    try {
        inst().main_impl(argc, argv);
    }
    catch (...) {
        inst().ex_ptr_ = std::current_exception();
    }
}

// static
DWORD __stdcall service::service_ctrl_handler_impl(DWORD ctrl, DWORD ev_type,
                                                   void* ev_data,
                                                   void* ctx) noexcept {
    assert(ctx != nullptr);
    service* srv_ptr = ctx != nullptr ? static_cast<service*>(ctx) : &inst();
    try {
        return srv_ptr->ctrl_handler_impl(ctrl, ev_type, ev_data);
    }
    catch (...) {
        if (!inst().ex_ptr_) {
            inst().ex_ptr_ = std::current_exception();
        }
    }
    return NO_ERROR;
}

void service::pause() {
    init_status(service_state::pause_pending);
    worker()->on_pause();
    init_status(service_state::paused);
}

void service::resume() {
    init_status(service_state::resume_pending);
    worker()->on_resume();
    init_status(service_state::running);
}

void service::stop() {
    init_status(service_state::stop_pending);
    worker()->on_stop();
    init_status(service_state::stopped);
}

void service::shutdown() {
    worker()->on_shutdown();
    init_status(service_state::stopped);
}

void service::pre_shutdown() {
    init_status(service_state::stop_pending);
    worker()->on_pre_shutdown();
    init_status(service_state::stopped);
}

void service::session_changed(DWORD wParam, DWORD SessionId) {
    worker()->on_session_change(static_cast<session_change_type>(wParam),
                                SessionId);
}

void service::init_status(service_state current_status, DWORD wait_hint,
                          DWORD error) {
    static_assert(sizeof(service_status) == sizeof(SERVICE_STATUS),
                  "The size of service_status mismatches SERVICE_STATUS");

    srv_status_.state = current_status;
    srv_status_.wait_hint = wait_hint;
    srv_status_.win32_exit_code = error;
    srv_status_.check_point = checkpoint_++;

    if (current_status == service_state::stopped) {
        srv_status_.check_point = 0;
    }

    if (!::SetServiceStatus(srv_status_handle_, &(srv_status_.as_base()))) {
        throw_os_error();
    }
}

void service_controller::open_impl(wzstring_view name, service_access access) {
    service_handle sc_handle(
        ::OpenSCManagerW(nullptr, nullptr, static_cast<DWORD>(sc_access::all)));
    if (!sc_handle) {
        throw_os_error();
    }

    service_handle new_srv_handle(
        OpenServiceW(sc_handle.get(), name.data(), static_cast<DWORD>(access)));
    if (!new_srv_handle) {
        throw_os_error();
    }

    srv_handle = std::move(new_srv_handle);
    srv_name = name;
}

void service_controller::set_description_impl(wzstring_view description) {
    SERVICE_DESCRIPTIONW info;
    info.lpDescription = const_cast<wchar_t*>(description.data());
    if (!::ChangeServiceConfig2W(srv_handle.get(), SERVICE_CONFIG_DESCRIPTION,
                                 &info)) {
        throw_os_error();
    }
}

void service_controller::set_display_impl(wzstring_view display_name) {
    if (!::ChangeServiceConfigW(srv_handle.get(), SERVICE_NO_CHANGE,
                                SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr,
                                nullptr, nullptr, nullptr, nullptr, nullptr,
                                display_name.data())) {
        throw_os_error();
    }
}

void service_controller::type(const service_type service_type) {
    if (!::ChangeServiceConfigW(
            srv_handle.get(), static_cast<DWORD>(service_type),
            SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr)) {
        throw_os_error();
    }
}

void service_controller::startup_type(startup startup_type) {
    if (!::ChangeServiceConfigW(srv_handle.get(), SERVICE_NO_CHANGE,
                                static_cast<DWORD>(startup_type),
                                SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr)) {
        throw_os_error();
    }
}

void service_controller::error_priority(service_error error_priority) {
    if (!::ChangeServiceConfigW(
            srv_handle.get(), SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
            static_cast<DWORD>(error_priority), nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr)) {
        throw_os_error();
    }
}

void service_controller::set_bin_path_impl(wzstring_view path) {
    if (!ChangeServiceConfigW(srv_handle.get(), SERVICE_NO_CHANGE,
                              SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, path.data(),
                              nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr)) {
        throw_os_error();
    }
}

void service_controller::set_dependencies(
    std::span<const dependency> dependencies) {
    std::wstring deps;
    std::wstring_view wnull(L"\0", 1);
    if (!dependencies.empty()) {
        std::size_t string_size = 0;
        for (const auto& dep : dependencies) {
            if (dep.name().empty()) {
                continue;
            }
            if (dep.is_group()) {
                string_size += 1;
            }
            string_size += dep.name().size();
            string_size += wnull.size();
        }

        deps.reserve(string_size);

        for (const auto& dep : dependencies) {
            if (dep.name().empty()) {
                continue;
            }
            if (dep.is_group()) {
                deps += SC_GROUP_IDENTIFIERW;
            }
            deps += dep.name();
            deps += wnull;
        }
    }

    if (!ChangeServiceConfigW(srv_handle.get(), SERVICE_NO_CHANGE,
                              SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr,
                              nullptr, nullptr, deps.c_str(), nullptr, nullptr,
                              nullptr)) {
        throw_os_error();
    }
}

void service_controller::failure_actions_impl(
    std::span<const sc_action> failure_actions, bool use_timeout,
    DWORD reset_period_seconds, bool use_strings, wzstring_view command,
    wzstring_view reboot_msg) {
    wchar_t empty_string[] = {L'\0'};
    SERVICE_FAILURE_ACTIONSW info{};
    info.dwResetPeriod = use_timeout ? reset_period_seconds : INFINITE;
    info.cActions = static_cast<DWORD>(failure_actions.size());
    info.lpsaActions = reinterpret_cast<SC_ACTION*>(
        const_cast<sc_action*>(failure_actions.data()));
    if (use_strings) {
        info.lpCommand = command.empty() ? empty_string
                                         : const_cast<wchar_t*>(command.data());
        info.lpRebootMsg = reboot_msg.empty()
                               ? empty_string
                               : const_cast<wchar_t*>(reboot_msg.data());
    }
    if (!ChangeServiceConfig2W(srv_handle.get(), SERVICE_CONFIG_FAILURE_ACTIONS,
                               &info)) {
        throw_os_error();
    }
}

void service_controller::pre_shutdown_time_impl(const DWORD time_ms) {
    SERVICE_PRESHUTDOWN_INFO info;
    info.dwPreshutdownTimeout = time_ms;
    if (!ChangeServiceConfig2W(srv_handle.get(),
                               SERVICE_CONFIG_PRESHUTDOWN_INFO, &info)) {
        throw_os_error();
    }
}

void service_controller::protection(svc::protection protection) {
    SERVICE_LAUNCH_PROTECTED_INFO info;
    info.dwLaunchProtected = static_cast<DWORD>(protection);
    if (!ChangeServiceConfig2W(srv_handle.get(),
                               SERVICE_CONFIG_LAUNCH_PROTECTED, &info)) {
        throw_os_error();
    }
}

void service_controller::start(DWORD argc, const char** argv) {
    if (!StartServiceA(srv_handle.get(), argc, argv)) {
        throw_os_error();
    }
}

void service_controller::start(DWORD argc, const wchar_t** argv) {
    if (!StartServiceW(srv_handle.get(), argc, argv)) {
        throw_os_error();
    }
}

void service_controller::start(const std::vector<std::string>& args) {
    DWORD argc = static_cast<DWORD>(args.size());
    std::vector<const char*> argv(argc);

    for (auto [vec_elem, argv_elem] : zip(args, argv)) {
        argv_elem = vec_elem.c_str();
    }

    if (!StartServiceA(srv_handle.get(), argc, argv.data())) {
        throw_os_error();
    }
}

void service_controller::start(const std::vector<std::wstring>& args) {
    DWORD argc = static_cast<DWORD>(args.size());
    std::vector<const wchar_t*> argv(argc);

    for (auto [vec_elem, argv_elem] : zip(args, argv)) {
        argv_elem = vec_elem.c_str();
    }

    if (!StartServiceW(srv_handle.get(), argc, argv.data())) {
        throw_os_error();
    }
}

void service_controller::stop() {
    return execute_code(SERVICE_CONTROL_STOP);
}

void service_controller::resume() {
    return execute_code(SERVICE_CONTROL_CONTINUE);
}

void service_controller::pause() {
    return execute_code(SERVICE_CONTROL_PAUSE);
}

void service_controller::execute_code(DWORD code) {
    SERVICE_STATUS srv_status;
    if (!ControlService(srv_handle.get(), code, &srv_status)) {
        throw_os_error();
    }
}

service_type service_controller::type() const {
    auto config = get_service_config(srv_handle);
    return static_cast<service_type>(config->dwServiceType);
}

startup service_controller::startup_type() const {
    auto config = get_service_config(srv_handle);
    return static_cast<startup>(config->dwStartType);
}

service_error service_controller::error_priority() const {
    auto config = get_service_config(srv_handle);
    return static_cast<service_error>(config->dwErrorControl);
}

std::wstring service_controller::description() const {
    auto buffer = get_service_config2(srv_handle, SERVICE_CONFIG_DESCRIPTION);
    LPSERVICE_DESCRIPTIONW desc = (LPSERVICE_DESCRIPTIONW)buffer.get();
    if (!desc->lpDescription) {
        desc->lpDescription = const_cast<wchar_t*>(L"");
    }
    return std::wstring(desc->lpDescription);
}

std::wstring service_controller::binary_path() const {
    auto config = get_service_config(srv_handle);

    if (!config->lpBinaryPathName) {
        config->lpBinaryPathName = const_cast<wchar_t*>(L"");
    }

    return std::wstring(config->lpBinaryPathName);
}

void service_controller::get_dependencies(std::vector<dependency>& deps) const {
    auto config = get_service_config(srv_handle);

    if (!config->lpDependencies || config->lpDependencies[0] == L'\0') {
        return;
    }

    auto calc_length = [](const wchar_t* str) -> size_t {
        struct terminator {
            wchar_t firts_null;
            wchar_t second_null;

            bool is_terminator() const {
                return firts_null == L'\0' && second_null == L'\0';
            }
        };

        size_t length = 0;
        for (terminator* term = (terminator*)str; !term->is_terminator();
             ++term, length += 2)
            ;
        return length;
    };

    std::wstring_view deps_str(config->lpDependencies,
                               calc_length(config->lpDependencies));
    if (deps_str.empty()) {
        return;
    }

    constexpr std::wstring_view delim{L"\0", 1};
    split(deps_str, delim, [&](std::wstring_view str) {
        if (str.empty()) {
            return;
        }
        bool is_group = false;
        if (str.front() == SC_GROUP_IDENTIFIER) {
            str.remove_prefix(1);
            if (str.empty()) {
                return;
            }
            is_group = true;
        }
        deps.emplace_back(str, is_group);
    });
}

void service_controller::get_dependents(std::vector<service_properties>& deps,
                                        active_state state) const {
    DWORD needed_size, num;

    (void)::EnumDependentServicesW(srv_handle.get(), static_cast<DWORD>(state),
                                   nullptr, 0, &needed_size, &num);

    DWORD prev_error = GetLastError();
    if (prev_error != ERROR_MORE_DATA) {
        throw std::system_error(os::make_system_error(prev_error));
    }

    if (needed_size == 0 || num == 0) {
        return;
    }

    using ptr_type = std::unique_ptr<ENUM_SERVICE_STATUSW, malloc_allocator>;

    ptr_type buffer(
        malloc_allocator::allocate<LPENUM_SERVICE_STATUSW>(needed_size));

    if (!::EnumDependentServicesW(srv_handle.get(), static_cast<DWORD>(state),
                                  buffer.get(), needed_size, &needed_size,
                                  &num)) {
        throw_os_error();
    }
    if (num == 0) {
        return;
    }
    // from docs: The order of the services in this array is the reverse of
    // the start order of the services.
    deps.reserve(num);
    auto services_view = std::span{buffer.get(), num};
    for (const auto& srv : services_view | reversed()) {
        deps.emplace_back(srv);
    }
}

std::wstring service_controller::display() const {
    auto config = get_service_config(srv_handle);
    if (!config->lpDisplayName) {
        config->lpDisplayName = const_cast<wchar_t*>(L"");
    }
    return std::wstring(config->lpDisplayName);
}

service_status service_controller::status() const {
    service_status status;
    if (!QueryServiceStatus(srv_handle.get(), &status.as_base())) {
        throw_os_error();
    }
    return status;
}

service_properties::service_properties(
    const ENUM_SERVICE_STATUS_PROCESSW& info) {
    if (info.lpServiceName) {
        name = info.lpServiceName;
    }
    if (info.lpDisplayName) {
        display = info.lpDisplayName;
    }
    type = static_cast<service_type>(info.ServiceStatusProcess.dwServiceType);
    win32_exit_code = info.ServiceStatusProcess.dwWin32ExitCode;
    specific_exit_code = info.ServiceStatusProcess.dwServiceSpecificExitCode;
    check_point = info.ServiceStatusProcess.dwCheckPoint;
    wait_hint = info.ServiceStatusProcess.dwWaitHint;
    pid = info.ServiceStatusProcess.dwProcessId;
    system_process = info.ServiceStatusProcess.dwServiceFlags ==
                     SERVICE_RUNS_IN_SYSTEM_PROCESS;
    running = system_process || pid != 0;
}

service_properties::service_properties(const ENUM_SERVICE_STATUSW& info) {
    if (info.lpServiceName) {
        name = info.lpServiceName;
    }
    if (info.lpDisplayName) {
        name = info.lpDisplayName;
    }
    type = static_cast<service_type>(info.ServiceStatus.dwServiceType);
    win32_exit_code = info.ServiceStatus.dwWin32ExitCode;
    specific_exit_code = info.ServiceStatus.dwServiceSpecificExitCode;
    check_point = info.ServiceStatus.dwCheckPoint;
    wait_hint = info.ServiceStatus.dwWaitHint;
    pid = (-1);
    system_process = false;
    running = false;
}

service_controller svc::detail::install_service_impl(
    wzstring_view path, wzstring_view service_name,
    wzstring_view service_display_name, startup startup_type,
    service_type service_type, service_error error_priority) {
    service_handle sc_handle(
        ::OpenSCManagerW(nullptr, nullptr, (DWORD)sc_access::write));
    if (!sc_handle) {
        throw_os_error();
    }

    std::wstring path2;
    auto get_path = [&]() -> wzstring_view {
        if (path.empty()) {
            return wzstring_view{};
        }
        if (path[0] != '"' && path.find(' ') != std::wstring_view::npos) {
            path2.reserve(path.size() + 2);
            path2 += L'\"';
            path2 += path;
            path2 += L'\"';
            return path2;
        }
        return path;
    };

    auto formatted_path = get_path();
    const wchar_t* name_ptr = service_name.empty() ? L"" : service_name.data();
    const wchar_t* display_ptr =
        service_display_name.empty() ? L"" : service_display_name.data();
    const wchar_t* path_ptr =
        formatted_path.empty() ? L"" : formatted_path.data();

    service_handle srv_handle(::CreateServiceW(
        sc_handle.get(), name_ptr, display_ptr, (DWORD)service_access::all,
        (DWORD)service_type, (DWORD)startup_type, (DWORD)error_priority,
        path_ptr, nullptr, nullptr, nullptr, nullptr, nullptr));

    if (!srv_handle) {
        throw_os_error();
    }

    return std::move(srv_handle);
}

void svc::detail::uninstall_service_impl(wzstring_view service_name) {
    service_handle sc_handle(
        OpenSCManagerW(nullptr, nullptr, static_cast<DWORD>(sc_access::write)));
    if (!sc_handle) {
        throw_os_error();
    }
    const wchar_t* name_ptr = service_name.empty() ? L"" : service_name.data();
    service_handle srv_handle(
        ::OpenServiceW(sc_handle.get(), name_ptr, DELETE));
    if (!srv_handle) {
        throw_os_error();
    }

    if (!DeleteService(srv_handle.get())) {
        throw_os_error();
    }
}

void svc::list_services(std::vector<service_properties>& services,
                        list_service_type type, active_state state) {
    service_handle sc_handle(
        ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (!sc_handle) {
        throw_os_error();
    }
    // printf("service type: %d, state: %d\n", (int)type, (int)state);
    constexpr DWORD max_iteration_buffer_size =
        256 * 1024; // according to docs this is the max size of the buffer
    DWORD bytes_needed = 0, returned = 0, resume_handle = 0;
    std::vector<uint8_t> buff;
    buff.resize(max_iteration_buffer_size);

    while (1) {
        returned = 0;
        bytes_needed = 0;

        const BOOL result = ::EnumServicesStatusExW(
            sc_handle.get(), SC_ENUM_PROCESS_INFO, static_cast<DWORD>(type),
            static_cast<DWORD>(state), buff.empty() ? nullptr : buff.data(),
            static_cast<DWORD>(buff.size()), &bytes_needed, &returned,
            &resume_handle, nullptr);

        bool needs_more_data = false;
        if (!result) {
            const DWORD prev_error = GetLastError();
            if (prev_error != ERROR_MORE_DATA) {
                throw std::system_error(os::make_system_error(prev_error));
            }
            needs_more_data = true;
        }

        if (returned) {
            services.reserve(services.size() + returned);
            const ENUM_SERVICE_STATUS_PROCESSW* svcs =
                reinterpret_cast<const ENUM_SERVICE_STATUS_PROCESSW*>(
                    buff.data());
            for (auto i : range(returned)) {
                services.emplace_back(svcs[i]);
            }
        }

        // never resize the vector down in size because the vector will
        // not reallocate anyway
        if (bytes_needed > buff.size()) {
            buff.resize(bytes_needed);
        }

        if (!needs_more_data) {
            assert(resume_handle == 0);
            assert(bytes_needed == 0);
            break;
        }
    }
}
