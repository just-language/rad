#pragma once
#include <rad/async/async_wait_queue.h>
#include <rad/async/executor.h>
#include <rad/channels/channel_common.h>
#include <rad/libbase.h>
#include <rad/threading/synchronized_value.h>

#include <memory>
#include <optional>

namespace RAD_LIB_NAMESPACE::sync::oneshot {
    namespace details {
        enum class flags_t {
            none = 0,
            send_detached = 1 << 0,
            recv_detached = 1 << 1,
            send_closed = 1 << 2,
            recv_closed = 1 << 3,
        };

        RAD_OVERLOAD_ENUM_OPERATORS(flags_t);

        template <class T>
        struct channel_inner {
            using op_base = detail::async_op_base;

            struct state_t {
                std::optional<T> value;
                single_wait_operation recv_op;
                flags_t flags = flags_t::none;

                bool is_send_closed() const noexcept {
                    return flags & flags_t::send_closed;
                }

                bool is_recv_closed() const noexcept {
                    return flags & flags_t::recv_closed;
                }

                bool send_detached() const noexcept {
                    return flags & flags_t::send_detached;
                }

                bool recv_detached() const noexcept {
                    return flags & flags_t::recv_detached;
                }

                // returns true if send was
                // closed before
                bool close_send() noexcept {
                    bool was_closed = is_send_closed();
                    flags |= flags_t::send_closed;
                    return was_closed;
                }

                // returns true if recv was
                // closed before
                bool close_recv() noexcept {
                    bool was_closed = is_recv_closed();
                    flags |= flags_t::recv_closed;
                    return was_closed;
                }

                void detach_send() noexcept {
                    flags |= flags_t::send_detached;
                }

                void detach_recv() noexcept {
                    flags |= flags_t::recv_detached;
                }
            };

            channel_inner(any_executor& ex) : ex_{ex} {
            }

            void on_sender_destroyed() noexcept {
                auto state = state_.synchronize();
                bool was_closed = state->close_send();
                if (!was_closed) {
                    state->recv_op.post(ex_);
                }
            }

            void on_receiver_destroyed() noexcept {
                state_->close_recv();
            }

            template <class... U>
            void send(U&&... value) {
                auto state = state_.synchronize();
                if (state->is_recv_closed()) {
                    throw_channel_error(channel_error_code::send);
                }
                if (state->is_send_closed()) {
                    throw_channel_error(channel_error_code::send_consumed);
                }
                state->value.emplace(std::forward<U>(value)...);
                state->close_send();
                state->recv_op.post(ex_);
            }

            bool should_recv_wait(op_base& op, std::optional<T>& value) {
                auto state = state_.synchronize();
                if (!state->value.has_value()) {
                    if (state->is_send_closed()) {
                        throw_channel_error(channel_error_code::recv);
                    }
                    state->recv_op.set_op(ex_, op);
                    return true;
                }
                else {
                    value = std::move(state->value);
                    state->value = std::nullopt;
                    state->close_recv();
                    return false;
                }
            }

            T recv_blocking() {
                auto state = state_.synchronize();
                if (state->is_recv_closed()) {
                    throw_channel_error(channel_error_code::recv_consumed);
                }
                if (!state->value.has_value()) {
                    throw_channel_error(channel_error_code::recv);
                }
                state->close_recv();
                auto on_exit = scope_exit([&] { state->value = std::nullopt; });
                return std::move(*state->value);
            }

            any_executor& ex_;
            sync_value<state_t> state_;
        };
    } // namespace details

    /*!
     * @brief The sender half of the channel can be used to send only one
     * value and the sender is considered closed afterwards.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class sender;

    /*!
     * @brief The receiver half of the channel can be used to receive only
     * one value and the receiver is considered closed afterwards.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class receiver;

    /*!
     * @brief A one shot channel is used to send and receive one value.
     * Only one sender and receiver may be obtained from the channel.
     * The sender and receiver can be moved across threads, but two thread
     * may not access the same instance of a channel, sender or receiver.
     * The channel, the sender and the receiver may be considered as a
     * shared_ptr to an inner channel.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class channel : public trackable, noncopyable {
    public:
        /*!
         * @brief The value type the channel will send and
         * receive.
         */
        using value_type = T;
        /*!
         * @brief The type of executor used by the channel
         * (any_executor)
         */
        using executor_type = any_executor;
        /*!
         * @brief The type of sender used by the channel
         */
        using sender_type = sender<T>;
        /*!
         * @brief the type of receiver used by the channel
         */
        using receiver_type = receiver<T>;

        /*!
         * @brief Construct the channel with the given executor
         * to dispatch wait operation onto
         * @param ex an executor to dispatch wait operation onto
         */
        explicit channel(executor_type& ex)
            : inner_{std::make_shared<details::channel_inner<T>>(ex)} {
        }

        /*!
         * @brief Split the channel into send and receive halfs.
         * Must not be called more than one time.
         * @return a pair of sender and receiver
         */
        std::pair<sender<T>, receiver<T>> split();

        /*!
         * @brief Get the sender half of the channel. The sender
         * can be retrived only one time using make_sender() or
         * split() and attempting to retrive it twice will raise
         * an exception
         * @return sender half of the channel
         */
        sender<T> make_sender();

        /*!
         * @brief Get the receiver half of the channel. The
         * receiver can be retrived only one time using
         * make_receiver() or split() and attempting to retrive
         * it twice will raise an exception
         * @return receiver half of the channel
         */
        receiver<T> make_receiver();

        /*!
         * @brief Acess the channel's executor
         * @return Reference to the channel executor
         */
        executor_type& executor() noexcept {
            return inner_->ex_;
        }

        /*!
         * @brief Acess the channel's executor
         * @return Reference to the channel executor
         */
        const executor_type& executor() const noexcept {
            return inner_->ex_;
        }

        /*!
         * @brief Check if the sender half is closed or consumed
         * after it was detached
         * @return true if the sender half is closed or
         * consumed, otherwise false
         */
        bool is_send_closed() const noexcept {
            return inner_->state_->is_send_closed();
        }

        /*!
         * @brief Check if the receiver half is closed or
         * consumed after it was detached
         * @return true if the receiver half is closed or
         * consumed, otherwise false
         */
        bool is_receive_closed() const noexcept {
            return inner_->state_->is_recv_closed();
        }

    private:
        std::shared_ptr<details::channel_inner<T>> inner_;
    };

    template <class T>
    class [[nodiscard]] receive_awaitable final : public detail::async_op_base,
                                                  noncopyable {
        std::shared_ptr<details::channel_inner<T>> channel_;
        std::coroutine_handle<> waiter_;
        std::optional<T> value_;

        void invoke_operation() override {
            waiter_.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return channel_->ex_;
        }

    public:
        receive_awaitable(
            std::shared_ptr<details::channel_inner<T>> channel) noexcept
            : channel_{std::move(channel)} {
        }

        constexpr bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(std::coroutine_handle<> coro) {
            waiter_ = coro;
            return channel_->should_recv_wait(*this, value_);
        }

        T await_resume() {
            if (value_.has_value()) {
                return std::move(*value_);
            }
            return channel_->recv_blocking();
        }
    };

    /*!
     * @brief Sender half of a one shot channel.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class sender : noncopyable {
        friend class channel<T>;
        friend struct details::channel_inner<T>;

        explicit sender(
            std::shared_ptr<details::channel_inner<T>> channel) noexcept
            : channel_{std::move(channel)} {
        }

    public:
        /*!
         * @brief The value type the channel will send and
         * receive.
         */
        using value_type = T;

        /*!
         * @brief Move construct the sender from another sender.
         * @param other the moved sender which must not be used
         * afterwards
         */
        sender(sender&& other) noexcept : channel_{std::move(other.channel_)} {
        }

        /*!
         * @brief Move assign the sender to another sender. If
         * this sender is open it will be closed before move.
         * @param other the moved sender which must not be used
         * afterwards.
         * @return the sender itself.
         */
        sender& operator=(sender&& other) noexcept {
            notify_close_if_connected();
            channel_ = std::move(other.channel_);
            return *this;
        }

        ~sender() {
            notify_close_if_connected();
        }

        /*!
         * @brief Send a value through the channel. Only one
         * value can be sent and the send half is considered
         * closed afterwards. Accessing a closed oneshot channel
         * half is undefined behavior
         * !
         * @param value the value to send
         */
        void send(const T& value) {
            assert(is_valid() && "oneshot sender is used after being "
                                 "closed");
            std::exchange(channel_, nullptr)->send(value);
        }

        /*!
         * @brief Send a value through the channel. Only one
         * value can be sent and the send half is considered
         * closed afterwards. Accessing a closed oneshot channel
         * half is undefined behavior
         * !
         * @param value the value to send
         */
        void send(T&& value) {
            assert(is_valid() && "oneshot sender is used after being "
                                 "closed");
            std::exchange(channel_, nullptr)->send(std::move(value));
        }

        /*!
         * @brief Emplace send a value through the channel. Only
         * one value can be sent and the send half is considered
         * closed afterwards. Accessing a closed oneshot channel
         * half is undefined behavior !
         * @param args the arguments to construct the value in
         * place with
         */
        template <class... Args>
        void send(Args&&... args)
            requires(std::constructible_from<T, Args...>)
        {
            assert(is_valid() && "oneshot sender is used after being "
                                 "closed");
            std::exchange(channel_, nullptr)->send(std::forward<Args>(args)...);
        }

        /*!
         * @brief Destroys this sender. This sender must not be
         * used after it is closed
         */
        void close() noexcept {
            notify_close_if_connected();
            channel_ = nullptr;
        }

        /*!
         * @brief Check whether the sender is closed or not
         * @return true if the sender is not closed, otherwise
         * false
         */
        bool is_valid() const noexcept {
            return channel_ != nullptr;
        }

    private:
        void notify_close_if_connected() noexcept {
            if (channel_) {
                channel_->on_sender_destroyed();
            }
        }

        std::shared_ptr<details::channel_inner<T>> channel_;
    };

    /*!
     * @brief Receiver half of a one shot channel.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class receiver : noncopyable {
        friend class channel<T>;
        friend struct details::channel_inner<T>;

        explicit receiver(
            std::shared_ptr<details::channel_inner<T>> channel) noexcept
            : channel_{std::move(channel)} {
        }

    public:
        using value_type = T;

        /*!
         * @brief Move construct the receiver from another
         * receiver.
         * @param other the moved receiver which must not be
         * used afterwards
         */
        receiver(receiver&& other) noexcept
            : channel_{std::move(other.channel_)} {
        }

        /*!
         * @brief Move assign the receiver to another receiver.
         * If this receiver is open it will be closed before
         * move.
         * @param other the moved receiver which must not be
         * used afterwards.
         * @return the receiver itself.
         */
        receiver& operator=(receiver&& other) noexcept {
            notify_close_if_connected();
            channel_ = std::move(other.channel_);
            return *this;
        }

        ~receiver() {
            notify_close_if_connected();
        }

        /*!
         * @brief Async Receive a value from the message. If
         * there is no message in the channel but the sender is
         * still open the coroutine will suspend unitl there is
         * a message in the channel or the sender is closed.
         * Only one receive on the channel is allowed and the
         * receiver half is closed afterwards. Accessing a
         * closed oneshot channel half is undefined behavior !
         * @return An awaitable type that is when awaited will
         * return the received value or throw an exception if
         * the sender was closed and the message in the channel
         * was consumed
         */
        receive_awaitable<T> receive() {
            assert(is_valid() && "oneshot receiver is used after being "
                                 "closed");
            return receive_awaitable<T>{channel_};
        }

        /*!
         * @brief Closes the receiver. The receiver must not be
         * used after it is closed
         */
        void close() noexcept {
            notify_close_if_connected();
            channel_ = nullptr;
        }

        /*!
         * @brief Check whether the receiver is closed or not
         * @return true if the receiver is not closed, otherwise
         * false
         */
        bool is_valid() const noexcept {
            return channel_ != nullptr;
        }

    private:
        void notify_close_if_connected() {
            if (channel_) {
                channel_->on_receiver_destroyed();
            }
        }

        std::shared_ptr<details::channel_inner<T>> channel_;
    };

    /*!
     * @brief Get a sender and receiver pair associated with an executor
     * without explicitly creating the channel first.
     * @tparam T The value type the channel will send and receive.
     * @param ex An executor to dispatch wait operation onto.
     * @return A pair of sender and receiver.
     */
    template <class T>
    std::pair<sender<T>, receiver<T>> make_sender_receiver(any_executor& ex) {
        channel<T> c{ex};
        return c.split();
    }

    template <class T>
    std::pair<sender<T>, receiver<T>> channel<T>::split() {
        auto state = inner_->state_.synchronize();
        if (state->send_detached()) {
            throw_channel_error(channel_error_code::send_detached);
        }
        if (state->recv_detached()) {
            throw_channel_error(channel_error_code::recv_detached);
        }
        state->detach_send();
        state->detach_recv();
        return std::pair{sender<T>{inner_}, receiver<T>{inner_}};
    }

    template <class T>
    sender<T> channel<T>::make_sender() {
        auto state = inner_->state_.synchronize();
        if (state->send_detached()) {
            throw_channel_error(channel_error_code::send_detached);
        }
        state->detach_send();
        return sender<T>{inner_};
    }

    template <class T>
    receiver<T> channel<T>::make_receiver() {
        auto state = inner_->state_.synchronize();
        if (state->recv_detached()) {
            throw_channel_error(channel_error_code::recv_detached);
        }
        state->detach_recv();
        return receiver<T>{inner_};
    }
} // namespace RAD_LIB_NAMESPACE::sync::oneshot