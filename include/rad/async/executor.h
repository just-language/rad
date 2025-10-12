#pragma once
#include <rad/stack_forward_list.h>
#include <rad/trackable.h>
#ifndef RAD_DISABLE_EXECUTOR_THREADS
#include <rad/threading/mutex.h>
#endif // !RAD_DISABLE_EXECUTOR_THREADS
#include <array>
#include <coroutine>
#include <optional>

namespace RAD_LIB_NAMESPACE {
    class async_result {
        enum class state {
            finished,
            pending,
        };

        async_result() = default;

        async_result(state st, std::size_t n) noexcept
            : result{st}, transferred_bytes{n} {
        }

        async_result(state st, std::size_t n,
                     const std::error_code& ec) noexcept
            : result{st}, transferred_bytes{n}, ec{ec} {
        }

    public:
        static async_result success(std::size_t bytes) noexcept {
            return async_result{state::finished, bytes};
        }

        static async_result pending(std::size_t bytes = 0) noexcept {
            return async_result{state::pending, bytes};
        }

        static async_result failed(const std::error_code& ec,
                                   std::size_t bytes = 0) noexcept {
            return async_result{state::finished, bytes, ec};
        }

        // check if pending
        bool is_pending() const noexcept {
            return result == state::pending;
        }

        // check if finshed with success
        bool is_finished() const noexcept {
            return result == state::finished;
        }

        std::size_t transferred() const noexcept {
            return transferred_bytes;
        }

        int fd() const noexcept {
            return static_cast<int>(transferred());
        }

        // check if finished with error
        bool has_error() const noexcept {
            return static_cast<bool>(ec);
        }

        std::error_code& error() noexcept {
            return ec;
        }

        const std::error_code& error() const noexcept {
            return ec;
        }

    private:
        state result;
        std::size_t transferred_bytes = 0;
        std::error_code ec;
    };

    // if passed to io functions it will be ignored and errors will be
    // roprted via exceptions
    RAD_EXPORT_DECL extern std::error_code no_ec;

    /*!
     * @brief Checks if the function should report errors via the passed
     * error_code or use exceptions instead
     * @param[in] ec an error code to check if it has the same address of
     * no_ec
     * @return true if exceptions should be used in case of ec is the same
     * as no_ec, false otherwise
     */
    inline bool use_exceptions(const std::error_code& ec) noexcept {
        return std::addressof(ec) == std::addressof(no_ec);
    }

    class error_storage {
    public:
        /*!
         * @brief Constructs an error storage which will report
         * errors via exceptions
         */
        error_storage() = default;

        /*!
         * @brief Constructs an error storage which will report
         * errors via the provided error code reference if
         * ec_ref does not point to no_ec. If ec_ref points to
         * no_ec it will be equivalent to the default
         * constructor
         * @param[out] ec_ref an error code reference to report
         * errors with it
         */
        error_storage(std::error_code& ec_ref) noexcept : ec_ref{ec_ref} {
        }

        /*!
         * @return The active error code which is ec_ex if
         * ec_ref points to no_ec, otherwise it is ec_ref which
         * points to a user provided error_code
         */
        std::error_code& error() noexcept {
            return use_exceptions(ec_ref) ? ec_ex : ec_ref;
        }

        /*!
         * @return The active error code which is ec_ex if
         * ec_ref points to no_ec, otherwise it is ec_ref which
         * points to a user provided error_code
         */
        const std::error_code& error() const noexcept {
            return use_exceptions(ec_ref) ? ec_ex : ec_ref;
        }

        /*!
         * @return true if an error was stored, otherwise false
         */
        bool has_error() const noexcept {
            return static_cast<bool>(error());
        }

        /*!
         * @return true if and error was stored, otherwise false
         */
        explicit operator bool() const noexcept {
            return has_error();
        }

        /*!
         * @brief Stores an error code into the active error
         * code member
         * @param[in] ec the error code to store
         */
        void store(const std::error_code& ec) noexcept {
            error() = ec;
        }

        /*!
         * @brief If using exceptions and there is a stored
         * error throw an exception of type std::system_error
         * constructed with stored error ec_ex and additional
         * msg, otherwise if exceptions are not used do nothing
         * @param[in] msg zero terminated string message to pass
         * to std::system_error constructor
         */
        void raise(const char* msg) const {
            // ec_ex is only used only if ec_ref points to
            // no_ec and in this case exceptions are enabled
            if (ec_ex) {
                throw std::system_error(ec_ex, msg);
            }
        }

    private:
        /*
        if ec_ref references no_ec then ec_ex will be used to
        store errors and exceptions will be used to report
        errors in raise otherwise it references a user provided
        error_code and it will be used to store and report
        errors while ec_ex will not be used
        */

        std::error_code& ec_ref = no_ec;
        std::error_code ec_ex;
    };

    using default_io_allocator = std::allocator<uint8_t>;

    template <class Alloc, bool Inherits>
    class compressed_allocator_storage {
    public:
        using allocator_type = Alloc;

        compressed_allocator_storage(const Alloc& alloc) noexcept
            : alloc{alloc} {
        }

        compressed_allocator_storage(Alloc&& alloc) noexcept
            : alloc{std::move(alloc)} {
        }

        Alloc& get_allocator() noexcept {
            return alloc;
        }

    private:
        Alloc alloc;
    };

    template <class Alloc>
    class compressed_allocator_storage<Alloc, true> : Alloc {
    public:
        using allocator_type = Alloc;

        compressed_allocator_storage(const Alloc& alloc) noexcept
            : Alloc(alloc) {
        }

        compressed_allocator_storage(Alloc&& alloc) noexcept
            : Alloc(std::move(alloc)) {
        }

        Alloc& get_allocator() noexcept {
            return *this;
        }
    };

    template <class Alloc>
    using allocator_storage =
        compressed_allocator_storage<Alloc, !std::is_final_v<Alloc>>;

    /*!
     * @brief This allocator can be used by async functions when the
     * required size is known in advance. It is not suitable for general use
     * case. The storage buffer provided in construction must be valid as
     * long as it is used or allocated by the allocator.
     * @tparam N Compile time size of allocator storage.
     */
    template <std::size_t N>
    class static_buffer_allocator {
    public:
        using value_type = uint8_t;
        using pointer = uint8_t*;
        using const_pointer = const uint8_t*;
        using reference = uint8_t&;
        using const_reference = const uint8_t&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        /*!
         * @brief Construct a static buffer allocator with
         * buffer at @p storage and size @p n. If @p n is less
         * that N, the behavior is undefined.
         * @param storage Pointer to buffer to use for
         * allocation. The buffer must remain valid as long as
         * it is used or allocated by the allocator.
         * @param n Size of buffer pointed to by @p storage in
         * bytes. It must be greater than or equal to N.
         */
        constexpr static_buffer_allocator(pointer storage, std::size_t n)
            : storage_{storage} {
            assert(n >= N);
        }

        /*!
         * @brief Construct a static buffer allocator with
         * buffer of array @p storage.
         * @param storage Array buffer to use for allocation.
         * The array must remain valid as long as it is used or
         * allocated by the allocator.
         */
        constexpr static_buffer_allocator(uint8_t (&storage)[N])
            : storage_{storage} {
        }

        /*!
         * @brief Construct a static buffer allocator with
         * buffer of array @p storage.
         * @param storage Array buffer to use for allocation.
         * The array must remain valid as long as it is used or
         * allocated by the allocator.
         */
        constexpr static_buffer_allocator(std::array<std::uint8_t, N>& storage)
            : storage_{storage.data()} {
        }

        /*!
         * @brief Get the compile time size of allocator
         * storage.
         * @return The compile time size of allocator storage.
         */
        constexpr static size_type capacity() {
            return N;
        }

        /*!
         * @brief Allocate @p n bytes of the allocator storage.
         * If @p n is larger than allocator storage size or the
         * storage is currently used by another allocation the
         * behavior is undefined. The allocator can be used
         * inside the handler invocation because its storage is
         * released before invoking the handler.
         * @param n The count of bytes to allocate.
         * @return Pointer to the allocator storage.
         */
        pointer allocate(size_type n) noexcept {
            assert((n <= capacity()) && "The allocator storage doesn't fit the "
                                        "requested size");
            assert(!in_use() && "The allocator storage is already "
                                "allocated");
            assert((allocated_size_ == 0) && "The allocator storage is already "
                                             "allocated");
            set_used(true);
            set_allocated_size(n);
            return storage_;
        }

        /*!
         * @brief Release the allocator storage and make it
         * available for allocation again.
         * @param p Pointer to the allocator storage returned by
         * allocate().
         * @param n The count of bytes that was passed to
         * allocate().
         */
        void deallocate(pointer p, size_type n) noexcept {
            assert(in_use() && "The allocator storage was not allocated");
            assert((n == allocated_size_) &&
                   "The size to deallocate mismatches the "
                   "allocated size");
            assert((p >= storage_ && p < storage_ + N && n <= N) &&
                   "Out of range deallocation");
            set_used(false);
            set_allocated_size(0);
        }

        /*!
         * @brief Get the compile time size of allocator
         * storage.
         * @return The compile time size of allocator storage.
         */
        static constexpr size_type max_size() {
            return N;
        }

    private:
#ifndef NDEBUG
        constexpr bool in_use() const noexcept {
            return used_;
        }

        void set_used(bool is_used) noexcept {
            used_ = is_used;
        }

        void set_allocated_size(size_type n) noexcept {
            allocated_size_ = n;
        }
#else
        constexpr bool in_use() const noexcept {
            return false;
        }

        constexpr void set_used(bool) noexcept {
        }

        constexpr void set_allocated_size(size_type) noexcept {
        }
#endif // !NDEBUG

        pointer storage_;
#ifndef NDEBUG
        size_type allocated_size_ = 0;
        bool used_ = false;
#endif // !NDEBUG
    };

    static_assert(
        sizeof(static_buffer_allocator<0>) ==
            sizeof(static_buffer_allocator<1>),
        "static_buffer_allocator<N> must have a constant size regardless N");

    using stateful_null_allocator = static_buffer_allocator<0>;

    template <class T>
    struct static_buffer_allocator_size {
        static constexpr std::size_t size = std::size_t(-1);
    };

    template <std::size_t N>
    struct static_buffer_allocator_size<static_buffer_allocator<N>> {
        static constexpr std::size_t size = N;
    };

    /*!
     * @brief All handlers must satisfy these requirements:
     * They must be copy constructible or move constructible and they must
     * be destructible.
     */
    template <class Handler>
    concept BasicHandler = (std::is_copy_constructible_v<Handler> ||
                            std::is_move_constructible_v<Handler>) &&
                           std::is_destructible_v<Handler>;

    /*!
     * @brief The allocator used by async functions to allocate async
     * operations data must satisfy these requirements. The standard
     * allocator std::allocator<uint8_t> meets the requirements and is used
     * by default.
     */
    template <class Alloc>
    concept HandlerAllocator = requires(Alloc alloc, void* p, std::size_t n) {
        typename Alloc::value_type;
        typename std::allocator_traits<Alloc>::pointer;
        p = alloc.allocate(n);
        alloc.deallocate(
            reinterpret_cast<typename std::allocator_traits<Alloc>::pointer>(p),
            n);
    };

    /*!
     * @brief The handler submitted for execution on an executor using
     * post() must satisfy these requirements. It must satisfy BasicHandler,
     * and it must be callable with no arguments.
     */
    template <class Handler>
    concept JobHandler = requires(Handler handler) {
        requires BasicHandler<Handler>;
        handler();
    };

    /*!
     * @brief The handler passed to async write functions must satisfy these
     * requirements. It must satisfy BasicHandler, and it must be callable:
     * @code handler(std::error_code{}, std::size_t{}) @endcode
     */
    template <class Handler>
    concept WriteHandler = requires(Handler handler, const std::error_code& ec,
                                    std::size_t transferred) {
        requires BasicHandler<Handler>;
        handler(ec, transferred);
    };

    /*!
     * @brief The handler passed to async read functions must satisfy these
     * requirements. It must satisfy BasicHandler, and it must be callable:
     * @code handler(std::error_code{}, std::size_t{}) @endcode
     */
    template <class Handler>
    concept ReadHandler = requires(Handler handler, const std::error_code& ec,
                                   std::size_t transferred) {
        requires BasicHandler<Handler>;
        handler(ec, transferred);
    };

    /*!
     * @brief Dummy handler type used to aid calculation of required
     * allocator buffer size.
     * @tparam HandlerSize The size of the handler function stored in the
     * operation.
     */
    template <std::size_t HandlerSize>
    struct alignas(16) handler_allocator_size_calculator {
        alignas(16) std::uint8_t handler_buff[HandlerSize];

        template <class... Args>
        void operator()(Args&&... args) {
        }
    };

    // forward declare any_executor to use a pointer to any_executor in
    // async_op_base
    class any_executor;

#ifdef RAD_DISABLE_EXECUTOR_THREADS
    /*!
     * @brief The mutex type used by multithreaded executors.
     * If the macro RAD_DISABLE_EXECUTOR_THREADS is defined then
     * the lock and unlock operations of this mutex reduce to no op
     * to save the cost of mutex lock and unlock.
     * Otherwise it wraps a mutex and forwards lock and unlock to its
     * underlying mutex.
     */
    struct executor_mutex {
        template <class... Args>
        constexpr executor_mutex(Args&&...) noexcept {
        }

        constexpr void lock() const noexcept {
        }

        constexpr void unlock() const noexcept {
        }
    };
#else
    /*!
     * @brief The mutex type used by multithreaded executors.
     * If the macro RAD_DISABLE_EXECUTOR_THREADS is defined then
     * the lock and unlock operations of this mutex reduce to no op
     * to save the cost of mutex lock and unlock.
     * Otherwise it wraps a mutex and forwards lock and unlock to its
     * underlying mutex.
     */
    class executor_mutex {
    public:
        template <class... Args>
        executor_mutex(Args&&...) noexcept {
        }

        void lock() noexcept {
            mtx.lock();
        }

        void unlock() noexcept {
            mtx.unlock();
        }

    private:
        mutex mtx;
    };
#endif // RAD_DISABLE_EXECUTOR_THREADS

} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail {
    enum class async_op_type {
        unknown,
        strand,
        write,
        read,
        sendto,
        recvfrom,
        read_all,
        accept,
        connect,
        connect_range,
        wait,
        spawn,
        post,
        schedule,
        yield,
        lock,
        resolve,
    };

    enum class executor_type {
        unknown,
        io_loop,
        thread_pool,
        strand,
    };

    struct async_op_base : public stack_forward_list_node {
#ifdef RAD_ENABLE_ASYNC_DEBUG
    private:
        async_op_type type_ = async_op_type::unknown;

    public:
        const char* op_type_string() const noexcept {
            switch (type_) {
            case async_op_type::unknown:
                return "unknown";
            case async_op_type::strand:
                return "strand";
            case async_op_type::write:
                return "write";
            case async_op_type::read:
                return "read";
            case async_op_type::sendto:
                return "sendto";
            case async_op_type::recvfrom:
                return "recvfrom";
            case async_op_type::read_all:
                return "read_all";
            case async_op_type::accept:
                return "accept";
            case async_op_type::connect:
                return "connect";
            case async_op_type::connect_range:
                return "connect_range";
            case async_op_type::wait:
                return "wait";
            case async_op_type::spawn:
                return "spawn";
            case async_op_type::post:
                return "post";
            case async_op_type::schedule:
                return "schedule";
            case async_op_type::yield:
                return "yield";
            case async_op_type::lock:
                return "lock";
            case async_op_type::resolve:
                return "resolve";
            default:
                return "unkown";
            }
        }

        async_op_base(async_op_type type) : type_{type} {
            printf("** operation (%s:%zu) is constructed\n", op_type_string(),
                   get_op_id());
        }
#else
        async_op_base(async_op_type) noexcept {
        }
#endif // RAD_ENABLE_ASYNC_DEBUG

        async_op_base() = default;

        /*!
         * @brief Used by executors to invoke this operation, if
         * the executing executor is the one associated with
         * this operation then it will delegate to
         * invoke_operation immediately and decrease the work
         * count after invocation, otherwise it will post this
         * operation to its associated executor using
         * post_finished without decreasing the work count
         * because it will be decreased by the proxy executor.
         * Typically only io operations may be redirected from
         * io_loop to strand
         * @param current_executor the executor that is
         * currently executing this operation
         */
        template <class Exec>
        void invoke(Exec& current_executor);

        /*!
         * @brief Get a unique id for this operation used only
         * for testing purposes
         */
        std::size_t get_op_id() const noexcept {
            return reinterpret_cast<const std::size_t>(this);
        }

    protected:
        ~async_op_base() = default;

        /*!
         * @brief Implemented by derived operations to invoke
         * handlers, resume coroutines, ...
         */
        virtual void invoke_operation() = 0;

        /*!
         * @brief Get the operation's associated executor
         * @return A reference to the associated executor
         */
        virtual any_executor& associated_executor() const noexcept = 0;
    };

    struct timer_op_base : public async_op_base {
        bool canceled = false;

        timer_op_base() noexcept : async_op_base(async_op_type::wait) {
        }

    protected:
        ~timer_op_base() = default;
    };

    template <class T, class Allocator, std::size_t N>
    struct op_memory_holder {
        T* ptr;
        Allocator& alloc;

        op_memory_holder(T* ptr, Allocator& alloc) noexcept
            : ptr{ptr}, alloc{alloc} {
        }

        void release() noexcept {
            ptr = nullptr;
        }

        ~op_memory_holder() {
            using alloc_traits = std::allocator_traits<Allocator>;
            using pointer = typename alloc_traits::pointer;
            if (ptr) {
                alloc_traits::deallocate(alloc, reinterpret_cast<pointer>(ptr),
                                         N);
            }
        }
    };

    template <class OriginalOp>
    class replaced_op_t : public async_op_base, public OriginalOp::alloc_base {
        any_executor& ex_;

    public:
        using alloc_base = typename OriginalOp::alloc_base;
        using old_operation_type = OriginalOp;

        template <class Alloc>
        replaced_op_t(any_executor& ex, Alloc&& alloc) noexcept
            : alloc_base(std::move(alloc)), ex_{ex} {
        }

        any_executor& associated_executor() const noexcept override {
            return ex_;
        }

    protected:
        ~replaced_op_t() = default;
    };

    template <class Op, class Alloc, class... Args>
    inline Op* allocate_op(const Alloc& const_alloc, Args&&... args) {
        static_assert(static_buffer_allocator_size<Alloc>::size >= sizeof(Op),
                      "the stack allocator storage does not fit the operation");
        using alloc_traits = std::allocator_traits<Alloc>;
        using memory_holder = op_memory_holder<Op, Alloc, sizeof(Op)>;

        // make copy of the allocator
        auto alloc = const_alloc;
        Op* op =
            reinterpret_cast<Op*>(alloc_traits::allocate(alloc, sizeof(Op)));

        memory_holder op_holder{op, alloc};
        alloc_traits::construct(alloc, op, std::forward<Args>(args)..., alloc);
        op_holder.release();

        op = std::launder(op);
        return op;
    }

    /*
    destroy and free the object (op) using the allocator returned from
    op->get_allocator()
    */
    template <class Op>
    inline void free_op(Op* op) noexcept {
        using allocator_type = typename Op::allocator_type;
        using alloc_traits = std::allocator_traits<allocator_type>;
        using pointer = typename alloc_traits::pointer;

        // move the allocator outside of op
        auto alloc = std::move(op->get_allocator());
        // destroy the op, destroying the allocator inside it
        std::destroy_at(op);
        // deallocate using the allocator copy
        alloc_traits::deallocate(alloc, reinterpret_cast<pointer>(op),
                                 sizeof(Op));
    }

    template <class NewOp, class OldOp, class... Args>
    inline NewOp* reuse_op(OldOp* old_op, Args&&... args) {
        static_assert(is_public_base_of<replaced_op_t<OldOp>, NewOp>,
                      "NewOp must be derived from replaced_op_t<OldOp>");
        static_assert(!std::is_same_v<NewOp, OldOp>,
                      "NewOp can't be the same type as OldOp, copy or "
                      "move instead");
        static_assert(sizeof(NewOp) <= sizeof(OldOp),
                      "sizeof(NewOp) must be less than or equal to "
                      "sizeof(OldOp)");

        using allocator_type = typename OldOp::allocator_type;
        using alloc_traits = std::allocator_traits<allocator_type>;
        using pointer = typename alloc_traits::pointer;

        static_assert(
            static_buffer_allocator_size<allocator_type>::size >= sizeof(NewOp),
            "the stack allocator storage does not fit the new operation");

        auto alloc = std::move(old_op->get_allocator());
        auto handler = std::move(old_op->handler);
        std::destroy_at(old_op);

        auto on_failure = scope_exit([&] {
            alloc_traits::deallocate(alloc, reinterpret_cast<pointer>(old_op),
                                     sizeof(OldOp));
        });

        NewOp* new_op = reinterpret_cast<NewOp*>(old_op);
        alloc_traits::construct(alloc, new_op, std::move(handler),
                                std::forward<Args>(args)..., std::move(alloc));

        on_failure.release();

        new_op = std::launder(new_op);
        return new_op;
    }

    template <class NewOp>
    inline void free_reused_op(NewOp* op) {
        using Op = typename NewOp::old_operation_type;

        using allocator_type = typename Op::allocator_type;
        using alloc_traits = std::allocator_traits<allocator_type>;
        using pointer = typename alloc_traits::pointer;

        auto alloc = std::move(op->get_allocator());
        std::destroy_at(op);

        alloc_traits::deallocate(alloc, reinterpret_cast<pointer>(op),
                                 sizeof(Op));
    }

    /*
    moves the handler to the stack, free the object (ctx) with free_ctx and
    invokes the handler with the given arguments. the object (ctx) is
    destoryed and freed before the handler is invoked so the memory area
    occupied by the object (ctx) may be used again inside the handler. since
    the object (ctx) is desroyed and freed before the handler is invoked,
    arguments passed to the handler must be non members of the object (ctx)
    ! if a member is to be passed as an argument it must be copied first !
    */
    template <class Op, class... Args>
    inline void invoke_handler(Op* op, Args&&... args) {
        // move the handler outside of op
        auto handler2 = std::move(op->handler);
        // destroy and free op destroying the handler inside it
        free_op(op);
        // invoke the handler
        handler2(std::forward<Args>(args)...);
    }

    template <class Op, class... Args>
    inline void invoke_reused_op_handler(Op* op, Args&&... args) {
        auto handler2 = std::move(op->handler);
        free_reused_op(op);
        handler2(std::forward<Args>(args)...);
    }

    /*!
     * @brief A succeeded read or write operation to be used with reuse_op
     * @tparam OldOp the original allocated operation
     */
    template <class OldOp>
    struct rw_success_op final : replaced_op_t<OldOp> {
        using base = replaced_op_t<OldOp>;
        using handler_t =
            std::remove_cvref_t<decltype(std::declval<OldOp>().handler)>;

        handler_t handler;
        std::size_t transferred = 0;

        template <class Alloc>
        rw_success_op(handler_t&& handler, std::size_t transferred,
                      any_executor& ex, Alloc&& alloc)
            : base(ex, std::move(alloc)), handler{std::move(handler)},
              transferred{transferred} {
        }

        void invoke_operation() override {
            invoke_reused_op_handler(this, std::error_code{},
                                     std::size_t{transferred});
        }
    };

    /*!
     * @brief A failed read or write operation to be used with reuse_op
     * @tparam OldOp the original allocated operation
     */
    template <class OldOp>
    struct rw_failure_op final : replaced_op_t<OldOp> {
        using base = replaced_op_t<OldOp>;
        using handler_t =
            std::remove_cvref_t<decltype(std::declval<OldOp>().handler)>;

        handler_t handler;
        std::error_code ec;

        template <class Alloc>
        rw_failure_op(handler_t&& handler, const std::error_code& ec,
                      any_executor& ex, Alloc&& alloc)
            : base(ex, std::move(alloc)), handler{std::move(handler)}, ec{ec} {
        }

        void invoke_operation() override {
            invoke_reused_op_handler(this, std::error_code{ec}, std::size_t{0});
        }
    };

    /*!
     * @brief A succeeded operation whose handler takes a parameter of type
     * error_code
     * @tparam OldOp the original allocated operation
     */
    template <class OldOp>
    struct ec_success_op final : replaced_op_t<OldOp> {
        using base = replaced_op_t<OldOp>;
        using handler_t =
            std::remove_cvref_t<decltype(std::declval<OldOp>().handler)>;

        handler_t handler;

        template <class Alloc>
        ec_success_op(handler_t&& handler, any_executor& ex, Alloc&& alloc)
            : base(ex, std::move(alloc)), handler{std::move(handler)} {
        }

        void invoke_operation() override {
            invoke_reused_op_handler(this, std::error_code{});
        }
    };

    /*!
     * @brief A failed operation whose handler takes a parameter of type
     * error_code
     * @tparam OldOp the original allocated operation
     */
    template <class OldOp>
    struct ec_failure_op final : replaced_op_t<OldOp> {
        using base = replaced_op_t<OldOp>;
        using handler_t =
            std::remove_cvref_t<decltype(std::declval<OldOp>().handler)>;

        handler_t handler;
        std::error_code ec;

        template <class Alloc>
        ec_failure_op(handler_t&& handler, const std::error_code& ec,
                      any_executor& ex, Alloc&& alloc)
            : base(ex, std::move(alloc)), handler{std::move(handler)}, ec{ec} {
        }

        void invoke_operation() override {
            invoke_reused_op_handler(this, std::error_code{ec});
        }
    };

    template <class Exec, class Handler, class Alloc>
    class job_op final : public async_op_base, public allocator_storage<Alloc> {
        Exec& ex_;

    public:
        using alloc_base = allocator_storage<Alloc>;

        Handler handler;

        template <class H>
        job_op(Exec& ex, H&& handler, const Alloc& alloc) noexcept
            : async_op_base(async_op_type::post), alloc_base(alloc), ex_{ex},
              handler{std::forward<H>(handler)} {
        }

        void invoke_operation() override {
            invoke_handler(this);
        }

        any_executor& associated_executor() const noexcept override {
            return ex_;
        }
    };

    struct timer_impl;
} // namespace RAD_LIB_NAMESPACE::detail

namespace RAD_LIB_NAMESPACE {
    /*!
     * @brief An awaitable type used to schedule the awaiting coroutine on
     * one of the threads of the executor.
     * @tparam Scheduler The executor type to schedule the coroutine on.
     */
    template <class Scheduler>
    class [[nodiscard]] schedule_op final : noncopyable,
                                            public detail::async_op_base {
    public:
        /*!
         * @brief Construct the schedule operation with an
         * executor.
         * @param scheduler The executor to schedule the
         * coroutine on.
         */
        schedule_op(Scheduler& scheduler)
            : detail::async_op_base(detail::async_op_type::schedule),
              ex_{scheduler} {
        }

        /*!
         * @brief Check if the coroutine should continue without
         * suspending.
         * @return Always returns false because checking is done
         * in await_suspend().
         */
        constexpr bool await_ready() const noexcept {
            return false;
        }

        /*!
         * @brief Check if the coroutine is already running on
         * the scheduler (executor). If the coroutine is already
         * running on the scheduler then resume without suspend.
         * Otherwise suspend and schedule the coroutine to be
         * resumed on one of the scheduler threads.
         * @param waiter The coroutine handle.
         * @return If the coroutine is to be suspended return
         * true, and false otherwise.
         */
        bool await_suspend(std::coroutine_handle<> waiter) noexcept {
            waiter_ = waiter;
            return ex_.try_schedule(*this);
        }

        /*!
         * @brief Do nothing after the coroutine is resumed.
         */
        constexpr void await_resume() const noexcept {
        }

    private:
        void invoke_operation() override {
            waiter_.resume();
        }

        any_executor& associated_executor() const noexcept override {
            return ex_;
        }

        Scheduler& ex_;
        std::coroutine_handle<> waiter_;
    };

    /*!
     * @brief The base of all executors.
     */
    class any_executor : public trackable {
        using op_type = detail::async_op_base;
        using op_list_type = stack_forward_list<detail::async_op_base>;

#ifdef RAD_ENABLE_ASYNC_DEBUG
        detail::executor_type type_ = detail::executor_type::unknown;

    public:
        const char* executor_type_string() const noexcept {
            using detail::executor_type;
            switch (type_) {
            case executor_type::unknown:
                return "unknown";
            case executor_type::io_loop:
                return "io_loop";
            case executor_type::thread_pool:
                return "thread_pool";
            case executor_type::strand:
                return "strand";
            default:
                return "unknown";
            }
        }

        any_executor(detail::executor_type type) noexcept : type_{type} {
            printf("** executor (%s:%zu) is constructed\n",
                   executor_type_string(), get_ex_id());
        }
#else
    public:
        any_executor(detail::executor_type) noexcept {
        }
#endif // RAD_ENABLE_ASYNC_DEBUG

    public:
        any_executor() = default;

        virtual ~any_executor() = default;

        /*!
         * @brief Get a unique id for this executor used only
         * for testing purposes
         */
        std::size_t get_ex_id() const noexcept {
            return reinterpret_cast<const std::size_t>(this);
        }

        /*!
         * @brief Post an io operation to be invoked in the
         * loop. No additional allocation for the operation is
         * performed
         * @param op the operation to post
         */
        virtual void post(op_type& op) noexcept = 0;

        /*!
         * @brief Post one finished or canceled operation
         * without incrementing the work counter. This comes
         * after add_work. Should be used only by library io
         * objects
         * @param op an operation to post
         */
        virtual void post_finished(op_type& op) noexcept = 0;

        /*!
         * @brief Post finished or canceled operations without
         * inrementing the work counter. This comes after
         * add_work. Should be used only by library io objects
         * @param ready_ops operations to post
         */
        virtual void post_finished(op_list_type ready_ops) noexcept = 0;

        /*!
         * @brief Inform the executor that a work (operation) is
         * now pending and increases the work counter by n
         * (default 1). This should be used only by the library
         * async objects. Each call to add_work must be balanced
         * by a call to post_finished or cancel_work
         * @param n number of pending operations
         * @return
         */
        virtual void add_work(std::size_t n = 1) noexcept = 0;

        /*!
         * @brief Inform the executor that n operations have
         * been invoked. This should be used only by
         * async_op_base and the library proxy executors
         * @param n number of invoked operations
         */
        virtual void consume_work(std::size_t n) noexcept = 0;

        /*!
         * @brief Inform the executor that an outstanding
         * operation has been canceled and will not go through
         * the executor
         */
        virtual void cancel_work() noexcept = 0;

        /*!
         * @brief Get the work count. This method is only used
         * for testing purposes
         */
        virtual std::size_t work_count() const noexcept = 0;

        /*!
         * @brief Check if the executor is currently executing
         * operations on the current thread.
         * @return True if the executor is running on the
         * current thread, and false otherwise.
         */
        virtual bool running_on_current_thread() const noexcept = 0;
    };

    /*!
     * @brief The base of all timer executors.
     */
    class timer_executor : public trackable {
        using timer_t = detail::timer_impl;

    public:
        using timer_implementation_type = detail::timer_impl;
        using timer_operation_type = detail::timer_op_base;

        virtual ~timer_executor() = default;

        /*!
         * @brief Casts this timer executor to any_executor
         * @return A reference to this executor casted to
         * any_executor
         */
        virtual any_executor& as_any_executor() noexcept = 0;

        /*!
         * @brief Adds a timer to wait queue. Adding a timer to
         * a wait queue is not an operation by itself
         * @param t the timer to add the handlers to
         */
        virtual void schedule_timer(timer_implementation_type& t) noexcept = 0;

        /*!
         * @brief Removes a timer from wait queue. Removing a
         * timer from a wait queue is not a consumed operation
         * by itself
         * @param t the timer to add the handlers to
         */
        virtual void cancel_timer(timer_implementation_type& t) noexcept = 0;

        /*!
         * @brief Move the state of old timer to a new one.
         * Handlers of the old timer are moved to the new timer,
         * but if any of them holds a reference to the old timer
         * it will keep pointing to the old timer so care must
         * be taken!
         * @param old_timer the old timer
         * @param new_timer the new timer
         */
        virtual void move_timers(timer_implementation_type& old_t,
                                 timer_implementation_type& new_t) noexcept = 0;

        /*!
         * @brief Adds a handler to the timer state under the
         * waite queue lock. The executor increments the work
         * counter so no need to call add_work by the timer
         * @param t the timer to add the handlers to
         * @param handler the handler
         */
        virtual void
        add_timer_handler(timer_implementation_type& t,
                          timer_operation_type& handler) noexcept = 0;
    };

    /*!
     * @brief The type implementing an executor must satisfy these
     * requirements. It must derive from and implement any_executor.
     */
    template <class Exec>
    concept Executor = std::is_convertible_v<Exec*, any_executor*>;

    /*!
     * @brief The type implementing timer executor must satisfy these
     * requirements. It must derive from and implement timer_executor, and
     * derive from any_executor to satisfy Executor.
     */
    template <class Exec>
    concept TimerExecutor =
        Executor<Exec> && std::convertible_to<Exec*, timer_executor*>;

    /*!
     * @brief The type implementing proxy executor (like strand) must
     * satisfy these requirements. It must derive from any_executor to
     * satisfy Executor. It must define nested executor type:
     * inner_executor_type. It must have inner_executor() method that
     * returns a reference to the inner executor.
     */
    template <class Exec>
    concept ProxyExecutor =
        Executor<Exec> && requires(const Exec& cex, Exec& mex) {
            requires Executor<typename Exec::inner_executor_type>;
            {
                cex.inner_executor()
            } -> std::same_as<const typename Exec::inner_executor_type&>;
            {
                mex.inner_executor()
            } -> std::same_as<typename Exec::inner_executor_type&>;
        };

    /*!
     * @brief The type implementing proxy timer executor (like strand over
     * io_loop or thread_pool) must satisfy these requirements. It must
     * satisfy ProxyExecutor. Its inner executor must satisfy TimerExecutor.
     */
    template <class Exec>
    concept ProxyTimerExecutor =
        ProxyExecutor<Exec> &&
        TimerExecutor<typename Exec::inner_executor_type>;

    /*!
     * @brief The type implementing async object attached to an executor
     * must satisfy these requirements. It must define nested executor type:
     * @code executor_type @endcode. It must have @code executor() @endcode
     * method that returns a reference to the executor.
     */
    template <class Object>
    concept AsyncObject = requires(const Object& cobj, Object& mobj) {
        typename Object::executor_type;
        {
            cobj.executor()
        } -> std::same_as<typename Object::executor_type const&>;
        { mobj.executor() } -> std::same_as<typename Object::executor_type&>;
    };

    /*!
     * @brief Traits used to change the executor of an async object to a new
     * executor. Not all objects support changing its executor to any
     * executor. Io objects in general only allow changing its io executor
     * to a proxy over the same io executor. Timers will cancel its pending
     * wait operations when its executor is changed. To implement this trait
     * this static method must be defined:
     * @code Object rebind(Exec& ex, Object&& object) @endcode
     * @tparam Exec The type of the new executor
     * @tparam Object The type of async object
     */
    template <class Exec, class Object>
    struct rebind_executor_helper;

    template <class Object, class Exec>
    concept RebindableAsyncObject = requires(Exec& ex, Object&& robj) {
        requires Executor<Exec>;
        requires AsyncObject<Object>;
        {
            rebind_executor_helper<Exec, Object>::rebind(ex, std::move(robj))
        } -> std::same_as<Object>;
    };

    namespace detail {
#ifdef RAD_ENABLE_ASYNC_DEBUG
        template <class Exec>
        inline void async_op_base::invoke(Exec& current_executor) {
            printf("** operation (%s:%zu) is being executed on "
                   "executor (%s:%zu) work "
                   "count: %zu\n",
                   op_type_string(), get_op_id(),
                   current_executor.executor_type_string(),
                   current_executor.get_ex_id(), current_executor.work_count());
            any_executor& assoc_exec = associated_executor();
            if (&current_executor == &assoc_exec) {
                auto on_exit =
                    scope_exit([&] { current_executor.consume_work(1); });
                invoke_operation();
            }
            else {
                printf("** operation (%s:%zu) is scheduled to be "
                       "executed on its "
                       "associated executor (%s:%zu) work count: "
                       "%zu\n",
                       op_type_string(), get_op_id(),
                       assoc_exec.executor_type_string(),
                       assoc_exec.get_ex_id(), assoc_exec.work_count());
                assoc_exec.post_finished(*this);
            }
        }
#else
        template <class Exec>
        inline void async_op_base::invoke(Exec& current_executor) {
            any_executor& assoc_exec = associated_executor();
            if (std::addressof(current_executor) ==
                std::addressof(assoc_exec)) {
                auto consumer =
                    scope_exit([&] { current_executor.consume_work(1); });
                invoke_operation();
            }
            else {
                // don't consume the operations's work here
                // because it will be consumed by the proxy
                // executor
                assoc_exec.post_finished(*this);
            }
        }
#endif // RAD_ENABLE_ASYNC_DEBUG
    } // namespace detail

    /*!
     * @brief Submits a handler for execution on an executor.
     * @tparam Exec The type of executor which must satisfy Executor
     * @tparam Handler The type of handler which must satisfy JobHandler
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to submit and execute the handler on.
     * @param handler The handler to submit.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, JobHandler Handler,
              HandlerAllocator Alloc = default_io_allocator>
    void post(Exec& ex, Handler&& handler, const Alloc& alloc = Alloc()) {
        using op_t = detail::job_op<Exec, std::remove_cvref_t<Handler>, Alloc>;
        op_t* op = detail::allocate_op<op_t>(alloc, ex,
                                             std::forward<Handler>(handler));
        ex.post(static_cast<detail::async_op_base&>(*op));
    }

    /*!
     * @brief Submits a handler for execution on an executor.
     * If the executor is currently running on the current thread the
     * handler is not submitted but instead it is executed directly before
     * returning from dispatch(). In this case the allocator passed is not
     * used and no switch context will occur.
     * @tparam Exec The type of executor which must satisfy Executor
     * @tparam Handler The type of handler which must satisfy JobHandler
     * @tparam Alloc The type of allocator which must satisfy
     * HandlerAllocator
     * @param ex The executor to submit and execute the handler on.
     * @param handler The handler to dispatch or submit.
     * @param alloc An allocator used to allocate the operation. The size
     * passed to its allocate method is in bytes and the alignment of the
     * allocator must be suitable for std::max_align_t
     */
    template <Executor Exec, JobHandler Handler,
              HandlerAllocator Alloc = default_io_allocator>
    void dispatch(Exec& ex, Handler&& handler, const Alloc& alloc = Alloc()) {
        if (ex.running_on_current_thread()) {
            handler();
        }
        else {
            post(ex, std::forward<Handler>(handler), alloc);
        }
    }
} // namespace RAD_LIB_NAMESPACE

namespace RAD_LIB_NAMESPACE::detail {
    template <std::size_t handler_size, bool stateless_allocator>
    inline constexpr std::size_t async_job_size() noexcept {
        using handler_type = handler_allocator_size_calculator<handler_size>;
        if constexpr (stateless_allocator) {
            return sizeof(
                job_op<any_executor, handler_type, default_io_allocator>);
        }
        else {
            return sizeof(
                job_op<any_executor, handler_type, stateful_null_allocator>);
        }
    }

    template <class OldOp>
    void post_sync_rw(any_executor& ex, const async_result& result,
                      OldOp* old_op) {
        async_op_base* op_ptr;
        if (!result.has_error()) {
            using op_t = rw_success_op<OldOp>;
            op_ptr = reuse_op<op_t>(old_op, result.transferred(), ex);
        }
        else {
            using op_t = rw_failure_op<OldOp>;
            op_ptr = reuse_op<op_t>(old_op, result.error(), ex);
        }
        ex.post(*op_ptr);
    }

    template <class OldOp>
    void post_sync_ec(any_executor& ex, const async_result& result,
                      OldOp* old_op) {
        async_op_base* op_ptr;
        if (!result.has_error()) {
            using op_t = ec_success_op<OldOp>;
            op_ptr = reuse_op<op_t>(old_op, ex);
        }
        else {
            using op_t = ec_failure_op<OldOp>;
            op_ptr = reuse_op<op_t>(old_op, result.error(), ex);
        }
        ex.post(*op_ptr);
    }
} // namespace RAD_LIB_NAMESPACE::detail
