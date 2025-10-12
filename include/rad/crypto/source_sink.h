#pragma once
#include <rad/buffer.h>
#include <rad/dynamic_buffer.h>

#include <cassert>

namespace RAD_LIB_NAMESPACE {
    class source_base {
    public:
        virtual ~source_base() = default;

        std::size_t read(mutable_buffer buff) {
            auto n = do_read(buff);
            assert(n <= buff.size());
            return n;
        }

    private:
        virtual std::size_t do_read(mutable_buffer buff) = 0;
    };

    class sink_base {
    public:
        virtual ~sink_base() = default;

        void write(const_buffer buff) {
            do_write(buff);
        }

    private:
        virtual void do_write(const_buffer buff) = 0;
    };
} // namespace RAD_LIB_NAMESPACE