#include <rad/windows/service.h>

#include <iostream>

using namespace RAD_LIB_NAMESPACE;

namespace {
    void list_services() {
        std::vector<svc::service_properties> services;
        const svc::list_service_type type =
            svc::list_service_type::win32 | svc::list_service_type::driver;
        const svc::active_state state = svc::active_state::all;
        svc::list_services(services, type, state);
        std::cout << "[*] all installed services count: " << services.size()
                  << "\n";
    }
} // namespace

namespace tests_fn {
    bool do_win_services_tests() {
        try {
            list_services();
        }
        catch (const std::exception& ex) {
            std::cout << "[!] windows services tests failed ! " << ex.what()
                      << "\n";
            return false;
        }
        return true;
    }
} // namespace tests_fn