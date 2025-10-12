#pragma once
#include <rad/net/ssl/sslctx.h>

namespace RAD_LIB_NAMESPACE::net::ssl {

    template <class NextLayer>
    class bstream {
    public:
        using engine_type = SSLEngine;
        using native_handle_type = typename engine_type::native_handle_type;
        using context_type = typename engine_type::context_type;
        using next_layer_type = NextLayer;
        using lowest_layer_type = typename next_layer_type::lowest_layer_type;

        template <class... Args>
        bstream(context_type& ctx, Args&&... args)
            : sock{std::forward<Args>(args)...},
              engine{ctx, this->sock.native_handle().get()} {
        }

        stream(stream&&) = default;

        stream& operator=(stream&&) = default;

        ~stream() = default;

        next_layer_type& next_layer() noexcept {
            return sock;
        }

        const next_layer_type& next_layer() const noexcept {
            return sock;
        }

        lowest_layer_type& lowest_layer() noexcept {
            return sock.lowest_layer();
        }

        const lowest_layer_type& lowest_layer() const noexcept {
            return sock.lowest_layer();
        }

        engine_type& ssl_engine() noexcept {
            return engine;
        }

        const engine_type& ssl_engine() const noexcept {
            return engine;
        }

        void handshake(handshake_type type, std::error_code& ec) noexcept {
            ssl_engine().native_handle().SetFd(
                this->sock.native_handle().get());
            if (type == handshake_type::client) {
                engine.set_client_mode();
            }
            else {
                engine.set_server_mode();
            };
            engine.do_handshake(ec);
        }

        void handshake(handshake_type type) {
            std::error_code ec;
            handshake(type, ec);
            check_and_throw(ec, __func__);
        }

        void handshake(std::error_code& ec) noexcept {
            handshake(handshake_type::client, ec);
        }

        void handshake() {
            handshake(handshake_type::client);
        }

        void accept(std::error_code& ec) noexcept {
            handshake(handshake_type::server, ec);
        }

        void accept() {
            handshake(handshake_type::server);
        }

        uint32_t write(const_buffer buffer, std::error_code& ec) {
            ssl_engine().native_handle().SetFd(
                this->sock.native_handle().get());
            return engine.write(buffer, ec);
        }

        uint32_t send(const_buffer buffer, std::error_code& ec) noexcept {
            return write(buffer, ec);
        }

        uint32_t write(const_buffer buffer) {
            std::error_code ec;
            auto written = write(buffer, ec);
            check_and_throw(ec, __func__);
            return written;
        }

        uint32_t send(const_buffer buffer) {
            return write(buffer);
        }

        uint32_t read(mutable_buffer buffer, std::error_code& ec) noexcept {
            ssl_engine().native_handle().SetFd(
                this->sock.native_handle().get());
            return engine.read(buffer, ec);
        }

        uint32_t receive(mutable_buffer buffer, std::error_code& ec) noexcept {
            return read(buffer, ec);
        }

        uint32_t read(mutable_buffer buffer) {
            std::error_code ec;
            auto was_read = read(buffer, ec);
            check_and_throw(ec, __func__);
            return was_read;
        }

        std::size_t receive(mutable_buffer buffer) {
            return read(buffer);
        }

        void read_all(mutable_buffer buffer, std::error_code& ec) noexcept {
            while (buffer.size()) {
                buffer += read(buffer, ec);
                if (ec) {
                    return;
                }
            }
        }

        void receive_all(mutable_buffer buffer, std::error_code& ec) noexcept {
            read_all(buffer, ec);
        }

        void read_all(mutable_buffer buffer) {
            std::error_code ec;
            read_all(buffer, ec);
            check_and_throw(ec, __func__);
        }

        void receive_all(mutable_buffer buffer) {
            return read_all(buffer);
        }

    private:
        next_layer_type sock;
        engine_type engine;
    };

    namespace openssl {
        template <class NextLayer>
        using stream = ssl::stream<NextLayer, openssl::blocking_engine>;
    }

    namespace wolfssl {
        template <class NextLayer>
        using stream = ssl::stream<NextLayer, wolfssl::blocking_engine>;
    }

}; // namespace RAD_LIB_NAMESPACE::net::ssl