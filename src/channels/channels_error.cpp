#include <rad/channels/channel_common.h>

using namespace RAD_LIB_NAMESPACE;
using namespace sync;

namespace {
    struct channel_error_category : public std::error_category {
        constexpr channel_error_category() = default;

        virtual const char* name() const noexcept override {
            return "channel";
        }

        virtual std::string message(int code) const override {
            auto channel_code = rad::as<channel_error_code>(code);
            switch (channel_code) {
            case channel_error_code::recv:
                return "Couldn't receive any "
                       "more "
                       "messages "
                       "from the "
                       "channel because all "
                       "senders were closed";
            case channel_error_code::send:
                return "Couldn't send any more "
                       "messages "
                       "through the "
                       "channel because all "
                       "receivers were closed";
            case channel_error_code::send_consumed:
                return "Couldn't send twice to "
                       "a "
                       "oneshot "
                       "channel";
            case channel_error_code::recv_consumed:
                return "Couldn't receive twice "
                       "from a "
                       "oneshot "
                       "channel";
            case channel_error_code::send_detached:
                return "Send half was already "
                       "detached "
                       "from a "
                       "single "
                       "consumer channel";
            case channel_error_code::recv_detached:
                return "Receive half was "
                       "already "
                       "detached from "
                       "a "
                       "single producer "
                       "channel";
            case channel_error_code::consumed:
                return "";
            default:
                return "";
            }
        }
    };

    const channel_error_category channel_category_inst;
} // namespace

const std::error_category& sync::channel_category() noexcept {
    return channel_category_inst;
}