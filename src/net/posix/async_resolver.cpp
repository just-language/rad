#include <rad/net/detail/posix/async_resolver_impl.h>
#include <rad/threading/thread_pool.h>
#include <rad/async/work_guard.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace RAD_LIB_NAMESPACE;
using namespace net;
using net::detail::async_resolver_impl;

static_assert(AI_PASSIVE == ienum(resolver_flags::passive));
static_assert(AI_CANONNAME == ienum(resolver_flags::canon_name));
static_assert(AI_NUMERICHOST == ienum(resolver_flags::numeric_host));
static_assert(AI_V4MAPPED == ienum(resolver_flags::ipv4_mapped));
static_assert(AI_ALL == ienum(resolver_flags::all));
static_assert(AI_ADDRCONFIG == ienum(resolver_flags::addr_config));

auto async_resolver_impl::do_async_resolve() noexcept -> resolver_op_base* {
    auto state = state_.synchronize();
    if (state->canceled) {
        state->handler->store_error(
            std::make_error_code(std::errc::operation_canceled));
        return std::exchange(state->handler, nullptr);
    }

    resolver_impl resolver;
    std::error_code ec;
    resolver.do_resolve(state->host, state->service.c_str(), state->hint,
                        state->handler->get_results(), ec);
    if (ec) {
        state->handler->store_error(ec);
    }
    return std::exchange(state->handler, nullptr);
}

void async_resolver_impl::async_resolve() noexcept {
    auto& rex = ex_->thread_pool_executor();
    work_guard wguard{any_ex()};
    post(
        rex,
        [self = pointer<async_resolver_impl>{this}] {
            resolver_op_base* handler = self->do_async_resolve();
            assert(handler != nullptr);
            self->any_ex().post_finished(*handler);
        },
        static_buffer_allocator(io_alloc_buff_));
    wguard.release();
}