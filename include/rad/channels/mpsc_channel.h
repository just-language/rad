#pragma once
#include <rad/async/async_wait_queue.h>
#include <rad/async/executor.h>
#include <rad/channels/channel_common.h>
#include <rad/threading/synchronized_value.h>

#include <deque>
#include <optional>

namespace RAD_LIB_NAMESPACE::sync::mpsc {
    namespace details {
        enum class flags_t {
            none = 0,
            recv_detached = 1 << 1,
            send_closed = 1 << 2,
            recv_closed = 1 << 3,
        };

        RAD_OVERLOAD_ENUM_OPERATORS(flags_t);

        template <class T>
        struct channel_inner {
            using op_base = detail::async_op_base;
            using queue_type = std::deque<T>;

            struct state_t {
                queue_type queue;
                single_wait_operation recv_op;
                uint32_t senders_count = 0;
                flags_t flags = flags_t::none;

                bool is_send_closed() const noexcept {
                    return flags & flags_t::send_closed;
                }

                bool is_recv_closed() const noexcept {
                    return flags & flags_t::recv_closed;
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

                void detach_recv() noexcept {
                    flags |= flags_t::recv_detached;
                }
            };

            channel_inner(any_executor& ex) : ex_{ex} {
            }

            any_executor& executor() noexcept {
                return ex_;
            }

            const any_executor& executor() const noexcept {
                return ex_;
            }

            // increase senders count. can't be called after
            // all senders were destroyed
            void on_sender_created() noexcept {
                auto state = state_.synchronize();
                state->senders_count += 1;
            }

            // decrement senders count and if 0 is reached
            // close the send half permenantly, recv half
            // will still work until queue is empty
            void on_sender_destroyed() noexcept {
                auto state = state_.synchronize();
                assert(state->senders_count > 0 &&
                       "a sender was not created but is "
                       "destroyed or it is "
                       "destroyed twice !");
                state->senders_count -= 1;
                if (state->senders_count == 0) {
                    bool was_closed = state->close_send();
                    if (!was_closed) {
                        state->recv_op.post(ex_);
                    }
                }
            }

            // close recv half permenantly, all send halves
            // can't send afterwards and stored messages
            // will not be accessible any more
            void on_receiver_destroyed() noexcept {
                auto state = state_.synchronize();
                state->close_recv();
            }

            void close_senders() noexcept {
                auto state = state_.synchronize();
                bool was_closed = state->close_send();
                if (!was_closed && state->recv_op.has_op()) {
                    state->recv_op.post(ex_);
                }
            }

            /*!
             * @brief Attempt to send a value through the
             * channel. An error should be reported in the
             * following cases:
             * -# all senders were closed (logic error !
             * send should not be callable in that case).
             * -# the receiver is closed (or not opened yet
             * ?), an exception is thrown. If the receiver
             * is currently waiting wake it up
             * @tparam U
             * @param value the value to send
             */
            template <class... U>
            void send(U&&... value) {
                auto state = state_.synchronize();
                if (state->is_recv_closed()) {
                    throw_channel_error(channel_error_code::send);
                }
                state->queue.emplace_back(std::forward<U>(value)...);
                state->recv_op.post(executor());
            }

            /*!
             * @brief Try to receive a value from the
             * channel. An error should be reported in the
             * following cases:
             * -# the receiver is closed (logic error ! recv
             * should not be callable in that case).
             * -# all senders were closed (or not opened yet
             * ?) and no value is available, an exception is
             * thrown.
             * @param op the waiter op
             * @return true if the waiter should wait or
             * false otherwise
             */
            bool should_recv_wait(op_base& op) {
                auto state = state_.synchronize();
                if (state->queue.empty()) {
                    if (state->is_send_closed()) {
                        throw_channel_error(channel_error_code::recv);
                    }
                    state->recv_op.set_op(ex_, op);
                    return true;
                }
                return false;
            }

            bool try_should_recv_wait(op_base& op) noexcept {
                auto state = state_.synchronize();
                if (state->queue.empty()) {
                    if (state->is_send_closed()) {
                        return false;
                    }
                    state->recv_op.set_op(ex_, op);
                    return true;
                }
                return false;
            }

            /*!
             * @brief Try to extract value from the queue.
             * An error should be reported in the following
             * cases: the queue is empty so the awaitable
             * was resumed because all senders were closed,
             * an exception is thrown.
             * @return the value extracted from the queue
             */
            T recv_blocking() {
                auto state = state_.synchronize();
                if (state->queue.empty()) {
                    throw_channel_error(channel_error_code::recv);
                }
                auto on_exit =
                    scope_exit([&state] { state->queue.pop_front(); });
                return std::move(state->queue.front());
            }

            std::optional<T> try_recv_blocking() noexcept {
                auto state = state_.synchronize();
                if (state->queue.empty()) {
                    return std::nullopt;
                }
                auto on_exit =
                    scope_exit([&state] { state->queue.pop_front(); });
                return std::move(state->queue.front());
            }

            any_executor& ex_;
            sync_value<state_t> state_;
        };
    } // namespace details

    /*!
     * @brief Sender is used to send values through the channel to
     * receivers. Since this is multi producer channels senders can be
     * copied and moved and the channel will remain open until the receiver
     * is destroyed or receiver's close method. After any of both no
     * messages can be further sent and upon sending an exception will be
     * thrown. Also note that once senders count drops to 0 after the first
     * sender is created (all senders were closed) no more senders can be
     * created and the receiver will only be able to consume the existing
     * messages in the channel and after that the channel is closed in both
     * directions.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class sender;

    /*!
     * @brief Receiver half is used to receive values from the senders.
     * Only one receiver may be obtained from the channel and once it is
     * closed the channel is closed.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class receiver;

    /*!
     * @brief A multi producer single consumer channel is used to send and
     * receive multiple values.
     * Multiple senders and only one receiver may be obtained from the
     * channel. The sender and receiver can be moved across threads (the
     * sender may also be copied), but two thread may not access the same
     * instance of a channel, sender or receiver. The channel, the sender
     * and the receiver may be considered as a shared_ptr to an inner
     * channel.
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
         * Must not be called more than one time. Sender and
         * Receiver are pointer like types and their lifetimes
         * must not exceed that of the channel itself
         * @return a pair of sender and receiver
         */
        std::pair<sender<T>, receiver<T>> split();

        /*!
         * @brief Get a sender half for the channel. If the
         * count of senders drops to 0 after the first sender
         * was created this method can't be called any more and
         * an exception will be thrown
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

        /*!
         * @brief Get the open senders count. This method is
         * used mainly for testing, don't depend on the value
         * returned since it may have changed by another thread
         * @return open senders count
         */
        std::size_t senders_count() const noexcept {
            return inner_->state_->senders_count;
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
            return channel_->should_recv_wait(*this);
        }

        T await_resume() {
            return channel_->recv_blocking();
        }
    };

    template <class T>
    class [[nodiscard]] try_receive_awaitable : public detail::async_op_base,
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
        try_receive_awaitable(
            std::shared_ptr<details::channel_inner<T>> channel) noexcept
            : channel_{std::move(channel)} {
        }

        constexpr bool await_ready() const noexcept {
            return false;
        }

        bool await_suspend(std::coroutine_handle<> coro) noexcept {
            waiter_ = coro;
            return channel_->try_should_recv_wait(*this);
        }

        std::optional<T> await_resume() noexcept {
            return channel_->try_recv_blocking();
        }
    };

    /*!
     * @brief Sender half of a multi producer single consumer channel.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class sender {
        friend class channel<T>;

        explicit sender(
            std::shared_ptr<details::channel_inner<T>> channel) noexcept
            : channel_{std::move(channel)} {
        }

    public:
        using executor_type = typename channel<T>::executor_type;

        sender(const sender& other) noexcept : channel_{other.channel_} {
            if (other.channel_) {
                other.channel_->on_sender_created();
            }
        }

        sender& operator=(const sender& other) noexcept {
            notify_sender_closed();
            channel_ = other.channel_;
            if (other.channel_) {
                other.channel_->on_sender_created();
            }
            return *this;
        }

        sender(sender&& other) noexcept = default;

        sender& operator=(sender&& other) noexcept {
            notify_sender_closed();
            channel_ = std::move(other.channel_);
            return *this;
        }

        ~sender() {
            notify_sender_closed();
        }

        /*!
         * @brief Get the executor of the channel.
         * @return The executor of the channel.
         */
        executor_type& executor() noexcept {
            return channel_->executor();
        }

        /*!
         * @brief Get the executor of the channel.
         * @return The executor of the channel.
         */
        const executor_type& executor() const noexcept {
            return channel_->executor();
        }

        /*!
         * @brief Send a value through the channel
         * @param value the value to send
         */
        void send(const T& value) {
            assert(is_valid());
            channel_->send(value);
        }

        /*!
         * @brief Send a value through the channel
         * @param value the value to send
         */
        void send(T&& value) {
            assert(is_valid());
            channel_->send(std::move(value));
        }

        /*!
         * @brief Emplace send a value through the channel
         * @param args the arguments to construct the value in
         * place with
         */
        template <class... Args>
        void send(Args&&... args)
            requires(std::constructible_from<T, Args...>)
        {
            assert(is_valid());
            channel_->send(std::forward<Args>(args)...);
        }

        /*!
         * @brief Close this sender. This sender must not be
         * used after it is closed
         */
        void close() noexcept {
            notify_sender_closed();
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
        void notify_sender_closed() noexcept {
            if (channel_) {
                channel_->on_sender_destroyed();
            }
        }

        std::shared_ptr<details::channel_inner<T>> channel_;
    };

    /*!
     * @brief Receiver half of a multi producer single consumer channel.
     * @tparam T The value type the channel will send and receive.
     */
    template <class T>
    class receiver : noncopyable {
        friend class channel<T>;

        explicit receiver(
            std::shared_ptr<details::channel_inner<T>> channel) noexcept
            : channel_{std::move(channel)} {
        }

    public:
        using executor_type = typename channel<T>::executor_type;

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
         * @param other the moved receiver which must not be
         * used afterwards
         * @return the receiver itself
         */
        receiver& operator=(receiver&& other) noexcept {
            notify_receiver_closed();
            channel_ = std::move(other.channel_);
            return *this;
        }

        ~receiver() {
            notify_receiver_closed();
        }

        /*!
         * @brief Get the executor of the channel.
         * @return The executor of the channel.
         */
        executor_type& executor() noexcept {
            return channel_->executor();
        }

        /*!
         * @brief Get the executor of the channel.
         * @return The executor of the channel.
         */
        const executor_type& executor() const noexcept {
            return channel_->executor();
        }

        /*!
         * @brief Async Receive a value from the channel. If
         * there is no message in the channel but there are open
         * senders around the coroutine will suspend unitl there
         * is a message in the channel or all senders are closed
         * @return An awaitable type that is when awaited will
         * return the received value or throw an exception if
         * all senders were closed and all messages in the
         * channel were consumed
         */
        receive_awaitable<T> receive() noexcept {
            return receive_awaitable<T>{channel_};
        }

        /*!
         * @brief Async Receive a value from the channel. If
         * there is no message in the channel but there are open
         * senders around the coroutine will suspend unitl there
         * is a message in the channel or all senders are closed
         * @return An awaitable type that is when awaited will
         * return the received value or return nullopt if all
         * senders were closed and all messages in the channel
         * were consumed
         */
        try_receive_awaitable<T> try_receive() noexcept {
            return try_receive_awaitable<T>{channel_};
        }

        /*!
         * @brief Closes all senders so no further messages can
         * be sent.
         */
        void close() noexcept {
            notify_receiver_closed();
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
        void notify_receiver_closed() noexcept {
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
        if (state->recv_detached()) {
            throw_channel_error(channel_error_code::recv_detached);
        }
        state->senders_count += 1;
        state->detach_recv();
        return std::pair{sender<T>{inner_}, receiver<T>{inner_}};
    }

    template <class T>
    sender<T> channel<T>::make_sender() {
        auto state = inner_->state_.synchronize();
        if (state->is_send_closed()) {
            throw_channel_error(channel_error_code::send_detached);
        }
        state->senders_count += 1;
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
} // namespace RAD_LIB_NAMESPACE::sync::mpsc