#include <rad/windows/env.h>
#include <cassert>
#include <UserEnv.h>
#include <Windows.h>

using namespace RAD_LIB_NAMESPACE;
using namespace env;

void environment_block::env_deleter::operator()(
    pointer env_ptr) const noexcept {
    [[maybe_unused]] auto result = DestroyEnvironmentBlock(env_ptr);
    assert(result && "DestroyEnvironmentBlock Failed !");
}

void environment_block::create(os::handle& token, bool inherit) {
    void* ptr = nullptr;
    BOOL result = CreateEnvironmentBlock(&ptr, token.get(), inherit);
    if (!result) {
        throw std::system_error(GetLastError(), system_category());
    }
    env_.reset(ptr);
}