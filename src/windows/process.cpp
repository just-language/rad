#include <Windows.h>
#include <rad/windows/env.h>
#include <rad/windows/process.h>

#include <cassert>

using namespace RAD_LIB_NAMESPACE;
using namespace ps;

namespace {
    void throw_last_system_error() {
        throw std::system_error(GetLastError(), system_category());
    }

    class process_thread_attributes {
    public:
        process_thread_attributes() = default;

        ~process_thread_attributes() {
            destroy();
        }

        LPPROC_THREAD_ATTRIBUTE_LIST get_list() const noexcept {
            return list_;
        }

        void init(uint32_t count) {
            destroy();
            if (!count) {
                return;
            }
            SIZE_T buff_size = 0;
            InitializeProcThreadAttributeList(nullptr, count, 0, &buff_size);
            assert(buff_size != 0);
            storage_.resize(buff_size);
            list_ =
                reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
            BOOL result =
                InitializeProcThreadAttributeList(list_, count, 0, &buff_size);
            if (!result) {
                throw std::system_error(GetLastError(), system_category());
            }
            capacity_ = count;
            size_ = 0;
        }

        void set_parent(HANDLE& proc) {
            BOOL result = UpdateProcThreadAttribute(
                list_, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &proc,
                sizeof(proc), nullptr, nullptr);
            if (!result) {
                throw std::system_error(GetLastError(), system_category());
            }
            ++size_;
        }

    private:
        void destroy() {
            if (list_) {
                DeleteProcThreadAttributeList(list_);
            }
            list_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }

        LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
        std::vector<uint8_t> storage_;
        uint32_t capacity_ = 0;
        uint32_t size_ = 0;
    };

    process_info spawn_process_impl(wzstring_view path, std::wstring cmdline,
                                    wzstring_view working_dir,
                                    spawn_flags flags,
                                    wzstring_view desktop_name, token* tok,
                                    LPPROC_THREAD_ATTRIBUTE_LIST attrs) {
        const wchar_t* path_ptr = path.empty() ? nullptr : path.data();
        wchar_t* cmd_ptr = cmdline.empty() ? nullptr : cmdline.data();
        const wchar_t* working_dir_ptr =
            working_dir.empty() ? nullptr : working_dir.data();

        std::wstring desk;
        desk = desktop_name;

        STARTUPINFOEXW sinfo{};
        sinfo.StartupInfo.cb =
            attrs ? sizeof(sinfo) : sizeof(sinfo.StartupInfo);
        sinfo.lpAttributeList = attrs;
        sinfo.StartupInfo.lpDesktop =
            desktop_name.empty() ? nullptr : desk.data();
        if (attrs) {
            flags |= spawn_flags::extended_support_peresent;
        }

        env::environment_block env;
        if (tok) {
            env.create(tok->native_handle());
            flags |= spawn_flags::unicode_env;
        }

        PROCESS_INFORMATION pinfo{};

        BOOL res = false;
        if (tok) {
            res = CreateProcessAsUserW(
                tok->native_handle().get(), path_ptr, cmd_ptr, nullptr, nullptr,
                FALSE, static_cast<DWORD>(flags), env.native_handle().get(),
                working_dir_ptr, &sinfo.StartupInfo, &pinfo);
        }
        else {
            res = CreateProcessW(path_ptr, cmd_ptr, nullptr, nullptr, FALSE,
                                 static_cast<DWORD>(flags), nullptr,
                                 working_dir_ptr, &sinfo.StartupInfo, &pinfo);
        }

        if (!res) {
            throw_last_system_error();
        }

        return process_info{
            .process_handle =
                process{os::process_handle{pinfo.hProcess}, pinfo.dwProcessId},
            .thread_handle =
                thread{os::thread_handle{pinfo.hThread}, pinfo.dwThreadId}};
    }
} // namespace

process process::current() {
    process proc{os::process_handle{GetCurrentProcess()}};
    proc.is_self_ = true;
    return proc;
}

void process::get_pid() {
    if (is_current_process()) {
        id_ = GetCurrentProcessId();
    }
    else {
        id_ = GetProcessId(handle_.get());
        if (!id_) {
            throw_last_system_error();
        }
    }
}

void process::terminate(uint32_t exit_code) {
    if (!TerminateProcess(handle_.get(), exit_code)) {
        throw_last_system_error();
    }
}

bool process::is_running() {
    DWORD ret = WaitForSingleObject(handle_.get(), 0);
    if (ret == WAIT_FAILED) {
        throw_last_system_error();
    }
    return ret == WAIT_TIMEOUT;
}

void process::wait() {
    if (is_self_) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur));
    }
    if (WaitForSingleObject(handle_.get(), INFINITE) == WAIT_FAILED) {
        throw_last_system_error();
    }
}

wait_result process::wait(std::chrono::milliseconds time) {
    if (is_self_) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur));
    }
    DWORD ret =
        WaitForSingleObject(handle_.get(), static_cast<DWORD>(time.count()));
    if (ret == WAIT_FAILED) {
        throw_last_system_error();
    }
    return ret == WAIT_TIMEOUT ? wait_result::timeout
                               : wait_result::not_timeout;
}

uint32_t process::exit_code() {
    if (is_self_) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur));
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(handle_.get(), &code)) {
        throw_last_system_error();
    }
    return code;
}

process_times process::times() const {
    FILETIME ctime{}, etime{}, ktime{}, utime{};
    ::GetProcessTimes(handle_.get(), &ctime, &etime, &ktime, &utime);

    auto ftime_to_duration = [](FILETIME f) {
        ULARGE_INTEGER ul{};
        ul.LowPart = f.dwLowDateTime;
        ul.HighPart = f.dwHighDateTime;
        return windows_clock::duration{ul.QuadPart};
    };

    return process_times{windows_clock::time_point{ftime_to_duration(ctime)},
                         windows_clock::time_point{ftime_to_duration(etime)},
                         ftime_to_duration(ktime), ftime_to_duration(utime)};
}

token process::open_token(token_access access) {
    HANDLE tok = nullptr;
    BOOL result =
        ::OpenProcessToken(handle_.get(), static_cast<DWORD>(access), &tok);
    if (!result) {
        throw std::system_error{os::make_system_error(::GetLastError())};
    }
    return token{os::handle{tok}};
}

thread thread::current() noexcept {
    thread thd{os::thread_handle{GetCurrentThread()}};
    thd.is_self_ = true;
    return thd;
}

void thread::get_tid() {
    if (is_current_thread()) {
        id_ = GetCurrentThreadId();
    }
    else {
        id_ = GetThreadId(handle_.get());
        if (!id_) {
            throw_last_system_error();
        }
    }
}

void thread::resume() {
    if (ResumeThread(handle_.get()) == (DWORD)-1) {
        throw_last_system_error();
    }
}

void thread::suspend() {
    if (SuspendThread(handle_.get()) == (DWORD)-1) {
        throw_last_system_error();
    }
}

void thread::terminate(uint32_t exit_code) {
    if (!TerminateThread(handle_.get(), exit_code)) {
        throw_last_system_error();
    }
}

bool thread::is_running() {
    DWORD ret = WaitForSingleObject(handle_.get(), 0);
    if (ret == WAIT_FAILED) {
        throw_last_system_error();
    }
    return ret == WAIT_TIMEOUT;
}

void thread::wait() {
    if (is_self_) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur));
    }
    if (WaitForSingleObject(handle_.get(), INFINITE) == WAIT_FAILED) {
        throw_last_system_error();
    }
}

wait_result thread::wait(std::chrono::milliseconds time) {
    if (is_self_) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur));
    }
    DWORD ret =
        WaitForSingleObject(handle_.get(), static_cast<DWORD>(time.count()));
    if (ret == WAIT_FAILED) {
        throw_last_system_error();
    }
    return ret == WAIT_TIMEOUT ? wait_result::timeout
                               : wait_result::not_timeout;
}

uint32_t thread::exit_code() {
    if (is_self_) {
        throw std::system_error(
            std::make_error_code(std::errc::resource_deadlock_would_occur));
    }
    DWORD code = 0;
    if (!GetExitCodeThread(handle_.get(), &code)) {
        throw_last_system_error();
    }
    return code;
}

token thread::open_token(token_access access, bool as_process) {
    HANDLE tok = nullptr;
    BOOL result = ::OpenThreadToken(handle_.get(), static_cast<DWORD>(access),
                                    as_process, &tok);
    if (!result) {
        throw std::system_error{os::make_system_error(::GetLastError())};
    }
    return token{os::handle{tok}};
}

process_info ps::launch_process(token& tok, wzstring_view path,
                                std::wstring cmdline, process& parent,
                                wzstring_view working_dir, spawn_flags flags,
                                wzstring_view desktop_name) {
    process_thread_attributes attrs;
    attrs.init(1);
    HANDLE parent_handle = parent.native_handle().get();
    attrs.set_parent(parent_handle);
    return spawn_process_impl(path, cmdline, working_dir, flags, desktop_name,
                              &tok, attrs.get_list());
}

process_info ps::launch_process(token& tok, wzstring_view path,
                                std::wstring cmdline, wzstring_view working_dir,
                                spawn_flags flags, wzstring_view desktop_name) {
    return spawn_process_impl(path, cmdline, working_dir, flags, desktop_name,
                              &tok, nullptr);
}

process_info ps::launch_process(wzstring_view path, std::wstring cmdline,
                                wzstring_view working_dir, spawn_flags flags,
                                wzstring_view desktop_name) {
    return spawn_process_impl(path, cmdline, working_dir, flags, desktop_name,
                              nullptr, nullptr);
}