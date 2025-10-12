#include <rad/sysinfo.h>
#include <rad/windows/winreg.h>

using namespace RAD_LIB_NAMESPACE;

std::string sysinfo::os_name() {
    using namespace winreg;

    std::wstring win_name;

    key hKey = local_machine.create_subkey(
        LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", access::read);
    win_name += hKey.wstring_value(L"ProductName");
    win_name += L' ';
    win_name += L'v';
    try {
        win_name += hKey.wstring_value(L"ReleaseId");
    }
    catch (...) {
        win_name += hKey.wstring_value(L"CurrentVersion");
    }
    win_name += L" build " + hKey.wstring_value(L"CurrentBuild");
    return to_string(win_name);
}

std::string sysinfo::os_installation_date() {
    using namespace winreg;

    // "%H:%M:%S %d/%m/%Y" 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 4 = 16

    std::string install_date;

    key hKey = local_machine.create_subkey(
        LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", access::read);
    auto install_sec = hKey.dword_value(L"InstallDate");
    time_t install_time = install_sec;
    tm tmstruct;
    localtime_s(&tmstruct, &install_time);
    install_date.resize(20);
    size_t size = strftime(install_date.data(), install_date.size(),
                           "%H:%M:%S %d/%m/%Y", &tmstruct);
    if (!size) {
        return "";
    }

    install_date.resize(size);

    return install_date;
}