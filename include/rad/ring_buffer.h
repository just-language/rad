#pragma once
#include <rad/stack_allocator.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#define RAD_PROVIDE_CONTAINER_TYPES(T)                                         \
    using value_type = typename T::value_type;                                 \
    using pointer = typename T::pointer;                                       \
    using const_pointer = typename T::const_pointer;                           \
    using reference = typename T::reference;                                   \
    using const_reference = typename T::const_reference;                       \
    using size_type = typename T::size_type;                                   \
    using difference_type = typename T::difference_type

namespace RAD_LIB_NAMESPACE {
    template <class T, class Allocator>
    class ring_buffer;

    template <class T, class Allocator, bool is_const>
    class ring_buffer_iterator {
        using rbf_type =
            std::conditional_t<is_const, const ring_buffer<T, Allocator>,
                               ring_buffer<T, Allocator>>;
        using rbf_ptr_type = rbf_type*;

    public:
        RAD_PROVIDE_CONTAINER_TYPES(rbf_type);
        using iterator_category = std::random_access_iterator_tag;

        using deref_type =
            std::conditional_t<is_const, const_reference, reference>;

        constexpr ring_buffer_iterator() noexcept = default;

        ring_buffer_iterator(
            rbf_ptr_type ring_buffer_ptr,
            std::conditional_t<is_const, const_pointer, pointer> ptr) noexcept
            : rbf_ptr_{ring_buffer_ptr}, ptr_{ptr} {
        }

        deref_type operator*() const noexcept;

        std::conditional_t<is_const, const_pointer, pointer>
        operator->() const noexcept {
            return &(this->operator*());
        }

        ring_buffer_iterator& operator++() noexcept;

        ring_buffer_iterator& operator--() noexcept;

        ring_buffer_iterator operator++(int) noexcept {
            auto saved_it{*this};
            ++(*this);
            return saved_it;
        }

        ring_buffer_iterator operator--(int) noexcept {
            auto saved_it{*this};
            --(*this);
            return saved_it;
        }

        ring_buffer_iterator& operator+=(difference_type n) noexcept;

        ring_buffer_iterator& operator-=(difference_type n) noexcept;

        friend ring_buffer_iterator operator+(const ring_buffer_iterator& iter,
                                              difference_type n) noexcept {
            ring_buffer_iterator it{iter};
            it += n;
            return it;
        }

        friend ring_buffer_iterator
        operator+(difference_type n,
                  const ring_buffer_iterator& iter) noexcept {
            ring_buffer_iterator it{iter};
            it += n;
            return it;
        }

        ring_buffer_iterator operator-(difference_type n) const noexcept {
            return (*this + (-n));
        }

        difference_type
        operator-(const ring_buffer_iterator& other) const noexcept;

        deref_type operator[](difference_type n) const noexcept {
            return *(*this + n);
        }

        friend bool operator==(const ring_buffer_iterator& lhs,
                               const ring_buffer_iterator& rhs) noexcept {
            assert(lhs.rbf_ptr_ == rhs.rbf_ptr_);
            return lhs.ptr_ == rhs.ptr_;
        }

        friend bool operator!=(const ring_buffer_iterator& lhs,
                               const ring_buffer_iterator& rhs) noexcept {
            assert(lhs.rbf_ptr_ == rhs.rbf_ptr_);
            return lhs.ptr_ != rhs.ptr_;
        }

        friend bool operator<(const ring_buffer_iterator& lhs,
                              const ring_buffer_iterator& rhs) noexcept {
            assert(lhs.rbf_ptr_ == rhs.rbf_ptr_);
            return lhs.ptr_ < rhs.ptr_;
        }

        friend bool operator<=(const ring_buffer_iterator& lhs,
                               const ring_buffer_iterator& rhs) noexcept {
            assert(lhs.rbf_ptr_ == rhs.rbf_ptr_);
            return lhs.ptr_ <= rhs.ptr_;
        }

        friend bool operator>(const ring_buffer_iterator& lhs,
                              const ring_buffer_iterator& rhs) noexcept {
            assert(lhs.rbf_ptr_ == rhs.rbf_ptr_);
            return lhs.ptr_ > rhs.ptr_;
        }

        friend bool operator>=(const ring_buffer_iterator& lhs,
                               const ring_buffer_iterator& rhs) noexcept {
            assert(lhs.rbf_ptr_ == rhs.rbf_ptr_);
            return lhs.ptr_ >= rhs.ptr_;
        }

    private:
        rbf_ptr_type rbf_ptr_ = nullptr;
        std::conditional_t<is_const, const_pointer, pointer> ptr_ = nullptr;
    };

    namespace detail {
        inline size_t make_sure_capacity_is_larger_than_size(size_t cap,
                                                             size_t n) {
            if (n > cap) {
                throw std::system_error{
                    std::make_error_code(std::errc::value_too_large)};
            }
            return cap;
        }

        template <class Allocator, class InputIt>
        InputIt uninitialized_move_overlap(Allocator& alloc, InputIt first,
                                           InputIt last, InputIt d_first) {
            using alloc_traits = typename std::allocator_traits<Allocator>;
            if (first >= last || d_first == first) {
                return d_first;
            }

            if (d_first >= last || d_first < first) {
                InputIt current = d_first;
                for (; first != last; ++first, ++current) {
                    alloc_traits::construct(alloc, current, std::move(*first));
                    alloc_traits::destroy(alloc, first);
                }
                return current;
            }
            else {
                InputIt d_last = d_first + std::distance(first, last);
                for (; first != last; --last, --d_last) {
                    auto prev_last = std::prev(last);
                    alloc_traits::construct(alloc, std::prev(d_last),
                                            std::move(*prev_last));
                    alloc_traits::destroy(alloc, prev_last);
                }
                return d_first + std::distance(first, last);
            }
        }

        template <class Pointer, class SizeType>
        struct moving_array_ptr_size {
            using pointer = Pointer;
            using size_type = SizeType;

            pointer p;
            size_type size;

            pointer first() const {
                return p;
            }

            pointer last() const {
                return p + size;
            }

            void advance(size_t n) {
                p += n;
            }
        };

        template <class Alloc, bool Empty>
        struct ring_buffer_allocator_storage {
            static constexpr bool replace_on_copy = std::allocator_traits<
                Alloc>::propagate_on_container_copy_assignment::value;
            static constexpr bool replace_on_move = std::allocator_traits<
                Alloc>::propagate_on_container_move_assignment::value;
            static constexpr bool replace_on_swap = std::allocator_traits<
                Alloc>::propagate_on_container_swap::value;
            static constexpr bool always_equal =
                std::allocator_traits<Alloc>::is_always_equal::value;
            static constexpr bool is_swappable =
                replace_on_swap || always_equal;

            Alloc alloc;

            ring_buffer_allocator_storage() = default;

            ring_buffer_allocator_storage(const Alloc& alloc) : alloc{alloc} {
            }

            ring_buffer_allocator_storage(
                const ring_buffer_allocator_storage& other)
                : alloc{
                      std::allocator_traits<Alloc>::
                          select_on_container_copy_construction(other.alloc)} {
            }

            // Allocator move constructor must not throw
            ring_buffer_allocator_storage(ring_buffer_allocator_storage&& other)
                : alloc{std::move(other.alloc)} {
            }

            // Allocator copy assign must not throw
            void
            copy_alloc(const ring_buffer_allocator_storage& other) noexcept {
                if constexpr (replace_on_copy) {
                    alloc = other.alloc;
                }
            }

            // Allocator move assign must not throw
            void move_alloc(ring_buffer_allocator_storage&& other) noexcept {
                if constexpr (replace_on_move) {
                    alloc = std::move(other.alloc);
                }
            }

            // Allocator swap must not throw
            void swap_alloc(ring_buffer_allocator_storage& other) noexcept {
                if constexpr (replace_on_swap) {
                    using std::swap;
                    swap(alloc, other.alloc);
                }
                else if (!always_equal) {
                    if (get_allocator() != other.get_allocator()) {
                        assert(false && "Swapping two "
                                        "unequal "
                                        "allocators "
                                        "that has "
                                        "propagate_on_"
                                        "container_"
                                        "move_"
                                        "assignment as "
                                        "false "
                                        "(default) is "
                                        "undefined "
                                        "behavior!");
                    }
                }
            }

            Alloc& get_allocator() {
                return alloc;
            }

            const Alloc& get_allocator() const {
                return alloc;
            }
        };

        template <class Allocator>
        class ring_buffer_mem_storage
            : public ring_buffer_allocator_storage<
                  Allocator,
                  std::is_empty_v<Allocator> && !std::is_final_v<Allocator>> {
            using base =
                ring_buffer_allocator_storage<Allocator,
                                              std::is_empty_v<Allocator> &&
                                                  !std::is_final_v<Allocator>>;

        public:
            using allocator_type = Allocator;
            using alloc_traits = typename std::allocator_traits<allocator_type>;
            using pointer = typename alloc_traits::pointer;
            using const_pointer = typename alloc_traits::const_pointer;
            using size_type = typename alloc_traits::size_type;

            using base::always_equal;
            using base::copy_alloc;
            using base::get_allocator;
            using base::move_alloc;
            using base::replace_on_move;
            using base::swap_alloc;

            ring_buffer_mem_storage() = default;

            ring_buffer_mem_storage(const allocator_type& alloc) : base(alloc) {
            }

            ring_buffer_mem_storage(const ring_buffer_mem_storage& other)
                : base(other) {
            }

            ring_buffer_mem_storage(ring_buffer_mem_storage&& other)
                : base(std::move(other)),
                  start_ptr_(std::exchange(other.start_ptr_, nullptr)),
                  end_ptr_(std::exchange(other.end_ptr_, nullptr)) {
            }

            ring_buffer_mem_storage&
            operator=(const ring_buffer_mem_storage&) = delete;

            ring_buffer_mem_storage&
            operator=(ring_buffer_mem_storage&&) noexcept = delete;

            ring_buffer_mem_storage(size_type n) {
                start_ptr_ = alloc_traits::allocate(get_allocator(), n);
                end_ptr_ = start_ptr_ + n;
            }

            ring_buffer_mem_storage(size_type n, const allocator_type& alloc)
                : base(alloc) {
                start_ptr_ = alloc_traits::allocate(get_allocator(), n);
                end_ptr_ = start_ptr_ + n;
            }

            ~ring_buffer_mem_storage() {
                if (start_ptr_ != nullptr) {
                    alloc_traits::deallocate(get_allocator(), start_ptr_,
                                             capacity());
                }
            }

            void copy_assign(const ring_buffer_mem_storage& other) {
                copy_alloc(other);
                if (capacity() < other.capacity()) {
                    allocate(other.capacity());
                }
            }

            bool move_assign(ring_buffer_mem_storage&& other) {
                move_alloc(std::move(other));
                if (replace_on_move || always_equal ||
                    get_allocator() == other.get_allocator()) {
                    start_ptr_ = std::exchange(other.start_ptr_, nullptr);
                    end_ptr_ = std::exchange(other.end_ptr_, nullptr);
                    return true;
                }
                else {
                    if (capacity() < other.capacity()) {
                        allocate(other.capacity());
                    }
                    return false;
                }
            }

            constexpr size_type capacity() const noexcept {
                return static_cast<size_type>(end_ptr_ - start_ptr_);
            }

            pointer allocate(size_type n) {
                pointer new_start = alloc_traits::allocate(get_allocator(), n);
                if (start_ptr_) {
                    alloc_traits::deallocate(get_allocator(), start_ptr_,
                                             capacity());
                }
                start_ptr_ = new_start;
                end_ptr_ = start_ptr_ + n;
                return start_ptr_;
            }

            void free() noexcept {
                if (start_ptr_) {
                    alloc_traits::deallocate(get_allocator(), start_ptr_,
                                             capacity());
                }
                start_ptr_ = nullptr;
                end_ptr_ = nullptr;
            }

            template <class... Args>
            void construct(pointer p, Args&&... args) {
                alloc_traits::construct(get_allocator(), p,
                                        std::forward<Args>(args)...);
            }

            void destroy(pointer p) noexcept {
                alloc_traits::destroy(get_allocator(), p);
            }

            template <class First, class Last>
            void destroy(First first, Last last) noexcept {
                while (first != last) {
                    alloc_traits::destroy(get_allocator(),
                                          std::addressof(*first));
                    ++first;
                }
            }

            pointer start_ptr() const noexcept {
                return start_ptr_;
            }

            pointer end_ptr() const noexcept {
                return end_ptr_;
            }

            void swap(ring_buffer_mem_storage& other) noexcept {
                using std::swap;
                swap_alloc(other);
                swap(start_ptr_, other.start_ptr_);
                swap(end_ptr_, other.end_ptr_);
            }

        private:
            pointer start_ptr_ = nullptr;
            pointer end_ptr_ = nullptr;
        };
    } // namespace detail

    template <class T, class Allocator = std::allocator<T>>
    class ring_buffer {
        using alloc_traits = typename std::allocator_traits<Allocator>;
        static constexpr bool is_move_assign_noexcept =
            (alloc_traits::propagate_on_container_move_assignment::value &&
             std::is_nothrow_move_assignable_v<Allocator>) ||
            (!alloc_traits::propagate_on_container_move_assignment::value &&
             alloc_traits::is_always_equal::value);

    public:
        using allocator_type = Allocator;
        using value_type = typename alloc_traits::value_type;
        using pointer = typename alloc_traits::pointer;
        using const_pointer = typename alloc_traits::const_pointer;
        using reference = T&;
        using const_reference = const T&;
        using size_type = typename alloc_traits::size_type;
        using difference_type = typename alloc_traits::difference_type;

        using iterator = ring_buffer_iterator<value_type, Allocator, false>;
        using const_iterator =
            ring_buffer_iterator<value_type, Allocator, true>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        ring_buffer() = default;

        explicit ring_buffer(const allocator_type& alloc) : storage_(alloc) {
        }

        ring_buffer(size_type capacity_size)
            : storage_(capacity_size), front_ptr_(storage_.start_ptr()),
              back_ptr_(storage_.start_ptr()) {
        }

        ring_buffer(size_type capacity_size, const allocator_type& alloc)
            : storage_(capacity_size, alloc), front_ptr_{storage_.start_ptr()},
              back_ptr_{storage_.start_ptr()} {
        }

        ring_buffer(size_type capacity_size_and_count, const value_type& value)
            : storage_(capacity_size_and_count),
              front_ptr_{storage_.start_ptr()}, back_ptr_{storage_.start_ptr()},
              count_{capacity_size_and_count} {
            std::uninitialized_fill(front_ptr_, front_ptr_ + size(), value);
        }

        ring_buffer(size_type capacity_size_and_count, const value_type& value,
                    const allocator_type& alloc)
            : storage_(capacity_size_and_count, alloc),
              front_ptr_{storage_.start_ptr()}, back_ptr_{storage_.end_ptr()},
              count_{capacity_size_and_count} {
            std::uninitialized_fill(front_ptr_, front_ptr_ + size(), value);
        }

        ring_buffer(size_type capacity_size, size_type buffer_size,
                    const value_type& value)
            : storage_{detail::make_sure_capacity_is_larger_than_size(
                  capacity_size, buffer_size)},
              front_ptr_{storage_.start_ptr()},
              back_ptr_{storage_.start_ptr() + buffer_size},
              count_{buffer_size} {
            if (back_ptr_ == storage_.end_ptr()) {
                back_ptr_ = storage_.start_ptr();
            }
            std::uninitialized_fill(front_ptr_, front_ptr_ + size(), value);
        }

        ring_buffer(size_type capacity_size, size_type buffer_size,
                    const value_type& value, const allocator_type& alloc)
            : storage_{detail::make_sure_capacity_is_larger_than_size(
                           capacity_size, buffer_size),
                       alloc},
              front_ptr_{storage_.start_ptr()},
              back_ptr_{storage_.start_ptr() + buffer_size},
              count_{buffer_size} {
            if (back_ptr_ == storage_.end_ptr()) {
                back_ptr_ = storage_.start_ptr();
            }
            std::uninitialized_fill(front_ptr_, front_ptr_ + size(), value);
        }

        template <class InputIterator,
                  std::enable_if_t<!std::is_integral_v<InputIterator>, int> = 0>
        ring_buffer(InputIterator first, InputIterator last)
            : ring_buffer(udistance(first, last)) {
            std::uninitialized_copy(first, last, front_ptr_);
            count_ = capacity();
        }

        template <class InputIterator>
        ring_buffer(InputIterator first, InputIterator last,
                    const allocator_type& alloc)
            : ring_buffer(udistance(first, last), alloc) {
            std::uninitialized_copy(first, last, front_ptr_);
            count_ = capacity();
        }

        ring_buffer(const ring_buffer& other) : storage_{other.storage_} {
            if (!other.empty()) {
                copy_other_items(other);
            }
        }

        ring_buffer(ring_buffer&& other) noexcept
            : storage_{std::move(other.storage_)}, front_ptr_{other.front_ptr_},
              back_ptr_{other.back_ptr_}, count_{other.count_} {
            other.front_ptr_ = nullptr;
            other.back_ptr_ = nullptr;
            other.count_ = 0;
        }

        ~ring_buffer() {
            if (storage_.start_ptr() != nullptr) {
                destroy_all();
            }
        }

        ring_buffer& operator=(const ring_buffer& other) {
            if (this == std::addressof(other)) {
                return *this;
            }
            clear();
            copy_another_ring_buffer(other);
            return *this;
        }

        ring_buffer&
        operator=(ring_buffer&& other) noexcept(is_move_assign_noexcept) {
            if (this == std::addressof(other)) {
                return *this;
            }
            clear();
            move_another_ring_buffer(std::move(other));
            return *this;
        }

        /*
        emplace, push and pop
        for a FIFO order use emplace_back/push_back with
        pop_front for a LIFO order use emplace_back/push_back
        with pop_back
        */

        /*
        emplace_back and push_back

        ----------------------------------------------------------
        | =>1 | 2 | 3 | 4 | 5 | => |  |  |  |  |  |  |  |  |  |
        |
        ---^--------------------^---------------------------------
           ^                    ^
          front				   back

        -----------------------------------------------------------
        | =>1 | 2 | 3 | 4 | 5 | 6 | => |  |  |  |  |  |  |  |  |
        |
        ---^------------------------^------------------------------
           ^					    ^
          front				      new back
        */

        template <class... Args>
        void unchecked_emplace_back(Args&&... args) {
            assert(storage_.start_ptr() != nullptr &&
                   "void "
                   "ring_buffer<T>::unchecked_emplace_back("
                   "Args&& ... "
                   "args) the "
                   "ring buffer has no capacity");
            assert(!full() && "void "
                              "ring_buffer<T>::unchecked_emplace_back("
                              "Args&& ... "
                              "args) can't "
                              "append to a full ring buffer without "
                              "overwriting");

            storage_.construct(back_ptr_, std::forward<Args>(args)...);
            increment_back();
        }

        template <class... Args>
        void emplace_back(Args&&... args) {
            if (full()) {
                pop_front();
            }
            unchecked_emplace_back(std::forward<Args>(args)...);
        }

        template <class... Args>
        bool try_emplace_back(Args&&... args) {
            if (full()) {
                return false;
            }
            unchecked_emplace_back(std::forward<Args>(args)...);
            return true;
        }

        void unchecked_push_back(value_type&& value) {
            unchecked_emplace_back(std::move_if_noexcept(value));
        }

        void unchecked_push_back(const value_type& value) {
            unchecked_emplace_back(value);
        }

        void push_back(value_type&& value) {
            emplace_back(std::move_if_noexcept(value));
        }

        void push_back(const value_type& value) {
            emplace_back(value);
        }

        bool try_push_back(value_type&& value) {
            return try_emplace_back(std::move_if_noexcept(value));
        }

        bool try_push_back(const value_type& value) {
            return try_emplace_back(value);
        }

        template <class InputIter>
        void insert_back(InputIter first, InputIter last) {
            assert(storage_.start_ptr() != nullptr &&
                   "void "
                   "ring_buffer<T>::insert_back(InputIter "
                   "first, "
                   "InputIter last) "
                   "the ring buffer has no capacity");

            size_type range_size = udistance(first, last);
            bool linearized = is_linearized();
            size_type available_size = reserve();
            size_type capacity_size = capacity();

            if (available_size >= range_size) {
                if (front_ptr_ == storage_.start_ptr()) {
                    back_ptr_ = std::uninitialized_copy(first, last, back_ptr_);
                    if (back_ptr_ == storage_.end_ptr()) {
                        back_ptr_ = storage_.start_ptr();
                    }
                }
                else if (back_ptr_ == storage_.start_ptr() || !linearized) {
                    back_ptr_ = std::uninitialized_copy(first, last, back_ptr_);
                }
                else {
                    auto end1 =
                        first + std::min(range_size,
                                         static_cast<size_type>(
                                             storage_.end_ptr() - back_ptr_));
                    back_ptr_ = std::uninitialized_copy(first, end1, back_ptr_);
                    if (end1 != last) {
                        back_ptr_ = std::uninitialized_copy(
                            end1, last, storage_.start_ptr());
                    }
                    else if (back_ptr_ == storage_.end_ptr()) {
                        back_ptr_ = storage_.start_ptr();
                    }
                }
                count_ += range_size;
            }

            else if (range_size < capacity_size) {
                // fill the reverse then overwrite
                // starting from front ptr

                if (front_ptr_ == storage_.start_ptr()) {
                    /*
                    ----------------------------------------------------
                    | >>1 | 2 | 3 | 4 | 5 | => |  |
                    |  |  | |  | |  |
                    ----------------------------------------------------

                    */

                    storage_.destroy(front_ptr_,
                                     front_ptr_ + range_size - available_size);
                    auto end1 = first + available_size;
                    std::uninitialized_copy(first, end1, back_ptr_);
                    front_ptr_ = back_ptr_ = std::uninitialized_copy(
                        end1, last, storage_.start_ptr());
                }

                else if (back_ptr_ == storage_.start_ptr()) {
                    /*
                    ----------------------------------------------------
                    | => |  |  |  |  |  |  |  |  |
                    >>1 | 2 | 3 | 4 | 5 |
                    ----------------------------------------------------
                    */

                    storage_.destroy(front_ptr_,
                                     front_ptr_ + range_size - available_size);
                    front_ptr_ = back_ptr_ =
                        std::uninitialized_copy(first, last, back_ptr_);
                }

                else if (!linearized) {
                    /*
                    ----------------------------------------
                    | 4 | 5 | => |  |  |  |  | >>1 |
                    2 | 3 |
                    ----------------------------------------
                    */

                    size_type array1_size = storage_.end_ptr() - front_ptr_;
                    range_size -= available_size;
                    size_type to_copy = std::min(range_size, array1_size);
                    range_size -= to_copy;

                    storage_.destroy(front_ptr_, front_ptr_ + to_copy);
                    auto end1 = first + available_size + to_copy;
                    back_ptr_ = front_ptr_ =
                        std::uninitialized_copy(first, end1, back_ptr_);

                    if (range_size) {
                        storage_.destroy(storage_.start_ptr(),
                                         storage_.start_ptr() + range_size);
                        back_ptr_ = front_ptr_ = std::uninitialized_copy(
                            end1, last, storage_.start_ptr());
                    }

                    else if (back_ptr_ == storage_.end_ptr()) {
                        back_ptr_ = front_ptr_ = storage_.start_ptr();
                    }
                }

                else {
                    /*
                    -------------------------------------------------
                    |  |  |  | >>1 | 2 | 3 | 4 | 5 |
                    => |  | |  | |
                    -------------------------------------------------
                    */

                    range_size -= available_size;
                    storage_.destroy(front_ptr_, front_ptr_ + range_size);

                    auto end1 = first + (storage_.end_ptr() - back_ptr_);
                    std::uninitialized_copy(first, end1, back_ptr_);
                    front_ptr_ = back_ptr_ = std::uninitialized_copy(
                        end1, last, storage_.start_ptr());
                }

                count_ = capacity_size;
            }

            else {
                clear();
                std::advance(first, range_size - capacity_size);
                std::uninitialized_copy(first, last, storage_.start_ptr());
                count_ = capacity_size;
            }
        }

        /*
        pop front : removes elements from the begin of the
        buffer then advance the the front pointer before the pop
        the front ptr points to the front object after the pop
        the front ptr moves towards the back ptr to point to the
        next object which becomes the new front object
        =====>
         front                                back
           ^                                   ^
           |                                   |
        ------------------------------------------------------------------------------
        | >1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | < |  |  |  |  |
        |  |  | |  |  | |  |
        ------------------------------------------------------------------------------
        ------>
           new front                        back
                  ^                               ^
                  |                               |
        --------------------------------------------------------------------------
        |  | >2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | < |  |  |  |  |  |
        |  | |  |  |
        |
        --------------------------------------------------------------------------
        */

        void pop_front(value_type& value) {
            assert(storage_.start_ptr() != nullptr &&
                   "void "
                   "ring_buffer<T>::pop_front(value_type& "
                   "value) the "
                   "ring buffer "
                   "has no capacity");
            assert(!empty() && "void "
                               "ring_buffer<T>::pop_front(value_type& "
                               "value) the "
                               "ring buffer is empty");

            value = std::move_if_noexcept(*front_ptr_);
            storage_.destroy(front_ptr_);
            increment_front();
            if (count_ == 0) {
                front_ptr_ = back_ptr_ = storage_.start_ptr();
            }
        }

        void pop_front() noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "void ring_buffer<T>::pop_front() the ring "
                   "buffer has "
                   "no capacity");
            assert(!empty() && "void ring_buffer<T>::pop_front() the ring "
                               "buffer is empty");

            storage_.destroy(front_ptr_);
            increment_front();
            if (count_ == 0) {
                front_ptr_ = back_ptr_ = storage_.start_ptr();
            }
        }

        bool try_pop_front(value_type& value) {
            if (empty()) {
                return false;
            }
            pop_front(value);
            return true;
        }

        bool try_pop_front() noexcept {
            if (empty()) {
                return false;
            }
            pop_front();
            return true;
        }

        template <class OutputIter>
        OutputIter pop_front(OutputIter first, OutputIter last) {
            return pop_front(first, udistance(first, last));
        }

        template <class OutputIter>
        OutputIter pop_front(OutputIter dest_first, size_type num) {
            num = std::min(num, size());
            if (!num) {
                return dest_first;
            }

            OutputIter ret;
            if (is_linearized()) {
                ret = std::copy(std::make_move_iterator(front_ptr_),
                                std::make_move_iterator(front_ptr_ + num),
                                dest_first);
                storage_.destroy(front_ptr_, front_ptr_ + num);
                front_ptr_ += num;
                if (front_ptr_ == storage_.end_ptr()) {
                    front_ptr_ = storage_.start_ptr();
                }
            }
            else {
                auto array1 = get_array<1>(false);
                size_type num1 = std::min(num, array1.size());
                ret = std::copy(std::make_move_iterator(array1.begin()),
                                std::make_move_iterator(array1.begin() + num1),
                                dest_first);
                storage_.destroy(array1.begin(), array1.begin() + num1);

                front_ptr_ += num1;
                if (front_ptr_ == storage_.end_ptr()) {
                    front_ptr_ = storage_.start_ptr();
                }

                size_type rem = num - num1;
                if (rem > 0) {
                    auto array2 = get_array<2>(false);
                    ret = std::copy(
                        std::make_move_iterator(array2.begin()),
                        std::make_move_iterator(array2.begin() + rem), ret);
                    storage_.destroy(array2.begin(), array2.begin() + rem);
                    front_ptr_ += rem;
                }
            }
            count_ -= num;
            if (count_ == 0) {
                front_ptr_ = back_ptr_ = storage_.start_ptr();
            }
            return ret;
        }

        /*
        pop back : removes elements from the end of the buffer
        then decrement the back pointer before the pop the back
        pointer points to the place past the last element (the
        place in which a new element would be inserted at the
        back) after the pop the back pointer points to the place
        of the popped element

        ------------------------------------------------------------------
        | =>1 | 2 | 3 | 4 | 5 | 6 | 7 | => |  |  |  |  |  |  |
        |  |  |
        |
        --^-----------------------------^---------------------------------
          ^                             ^
         front                         back
                                        <------------------
        -----------------------------------------------------------------
        | =>1 | 2 | 3 | 4 | 5 | 6 | => |  |  |  |  |  |  |  |  |
        |  | |
        --^-------------------------^------------------------------------
          ^                         ^
         front                     new back
        */

        void pop_back(value_type& value) {
            assert(storage_.start_ptr() != nullptr &&
                   "void ring_buffer<T>::pop_back(value_type& "
                   "value) the "
                   "ring buffer "
                   "has no capacity");
            assert(!empty() && "void ring_buffer<T>::pop_back(value_type& "
                               "value) the "
                               "ring buffer is empty");

            decrement_back();
            {
                auto on_exit = scope_exit([this] { increment_back(); });
                value = std::move_if_noexcept(*back_ptr_);
                on_exit.release();
            }
            storage_.destroy(back_ptr_);
            if (count_ == 0) {
                front_ptr_ = back_ptr_ = storage_.start_ptr();
            }
        }

        void pop_back() noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "void ring_buffer<T>::pop_back() the ring "
                   "buffer has no "
                   "capacity");
            assert(!empty() && "void ring_buffer<T>::pop_back() the ring "
                               "buffer is empty");

            decrement_back();
            storage_.destroy(back_ptr_);
            if (count_ == 0) {
                front_ptr_ = back_ptr_ = storage_.start_ptr();
            }
        }

        bool try_pop_back(value_type& value) {
            if (empty()) {
                return false;
            }
            pop_back(value);
            return true;
        }

        bool try_pop_back() noexcept {
            if (empty()) {
                return false;
            }
            pop_back();
            return true;
        }

        template <class OutputIter>
        OutputIter pop_back(OutputIter first, OutputIter last) {
            return pop_back(first, udistance(first, last));
        }

        template <class OutputIter>
        OutputIter pop_back(OutputIter dest_first, size_type num) {
            num = std::min(num, size());
            if (!num) {
                return dest_first;
            }

            OutputIter ret = dest_first;

            if (is_linearized()) {
                pointer end_copy_ptr = back_ptr_ == storage_.start_ptr()
                                           ? storage_.end_ptr()
                                           : back_ptr_;
                back_ptr_ = end_copy_ptr - num;
                ret = std::copy(std::make_move_iterator(back_ptr_),
                                std::make_move_iterator(end_copy_ptr),
                                dest_first);
                storage_.destroy(back_ptr_, end_copy_ptr);
            }

            else {
                auto array2 = get_array<2>(false);
                size_type num1 = std::min(num, array2.size());
                size_type rem = num - num1;
                pointer array1_destroy_begin = nullptr;
                if (rem > 0) {
                    auto array1 = get_array<1>(false);
                    auto start_copy_ptr = std::prev(array1.end(), rem);
                    ret = std::copy(std::make_move_iterator(start_copy_ptr),
                                    std::make_move_iterator(array1.end()),
                                    dest_first);
                    array1_destroy_begin = std::addressof(*start_copy_ptr);
                }
                auto start_copy_ptr = std::prev(array2.end(), num1);
                ret = std::copy(std::make_move_iterator(start_copy_ptr),
                                std::make_move_iterator(array2.end()), ret);
                storage_.destroy(start_copy_ptr, array2.end());
                if (rem > 0) {
                    storage_.destroy(array1_destroy_begin,
                                     array1_destroy_begin + rem);
                }
                back_ptr_ = std::addressof(*start_copy_ptr);
            }
            count_ -= num;
            if (count_ == 0) {
                front_ptr_ = back_ptr_ = storage_.start_ptr();
            }
            return ret;
        }

        /*
        ranges
        */

        iterator begin() {
            return iterator{this, !empty() ? front_ptr_ : nullptr};
        }

        const_iterator begin() const {
            return const_iterator{this, !empty() ? front_ptr_ : nullptr};
        }

        const_iterator cbegin() const {
            return begin();
        }

        iterator end() {
            return iterator{this, nullptr};
        }

        const_iterator end() const {
            return const_iterator{this, nullptr};
        }

        const_iterator cend() const {
            return end();
        }

        reverse_iterator rbegin() {
            return reverse_iterator{end()};
        }

        const_reverse_iterator rbegin() const {
            return const_reverse_iterator{end()};
        }

        const_reverse_iterator crbegin() const {
            return rbegin();
        }

        reverse_iterator rend() {
            return reverse_iterator{begin()};
        }

        const_reverse_iterator rend() const {
            return const_reverse_iterator{begin()};
        }

        const_reverse_iterator crend() const {
            return rend();
        }

        /*
                ^ ==> front pointer
                > ==> back pointer
                data starts from front pointer to back pointer

                1 - linearized : there is one array starting
           from front pointer to back pointer
                --------------------------------------------------------------------------
                | =>1 | 2 | 3 | 4 | 5 | => |  |  |  |  |  |  |
           |  |  | |  |  | |  |
           |
                ---^--------------------^-------------------------------------------------
                   ^                    ^
                   front               back

                2 - not linearized :
                the first array is from front pointer until the
           end of the buffer (last index N-1) the second array
           is from the start of the buffer until the back
           pointer
                ----------------------------------------------------------------------------------------
                | 6 | 7 | 8 | => |  |  |  |  |  |  |  |  |  |  |
           |  | |  |  | | =>1 | 2 | 3 | 4 | 5 |
                --------------^-----------------------------------------------------^-------------------
                                          ^ ^ ^ ^ back front
        */

        template <size_t one_two>
        std::span<value_type> get_array(bool is_buffer_linearized) noexcept {
            static_assert((one_two == 1 || one_two == 2), " wrong array !!!");

            if constexpr (one_two == 1) {
                if (is_buffer_linearized) {
                    return {front_ptr_, front_ptr_ + size()};
                }
                else {
                    return {front_ptr_, storage_.end_ptr()};
                }
            }
            else {
                if (is_buffer_linearized) {
                    return {};
                }
                else {
                    return {storage_.start_ptr(), back_ptr_};
                }
            }
        }

        template <size_t one_two>
        std::span<const value_type>
        get_array(bool is_buffer_linearized) const noexcept {
            static_assert((one_two == 1 || one_two == 2), " wrong array !!!");

            if constexpr (one_two == 1) {
                if (is_buffer_linearized) {
                    return {front_ptr_, front_ptr_ + size()};
                }
                else {
                    return {front_ptr_, storage_.end_ptr()};
                }
            }
            else {
                if (is_buffer_linearized) {
                    return {};
                }
                else {
                    return {storage_.start_ptr(), back_ptr_};
                }
            }
        }

        std::span<value_type> array_one() noexcept {
            return get_array<1>(is_linearized());
        }

        std::span<const value_type> array_one() const noexcept {
            return get_array<1>(is_linearized());
        }

        std::span<value_type> array_two() noexcept {
            return get_array<2>(is_linearized());
        }

        std::span<const value_type> array_two() const noexcept {
            return get_array<2>(is_linearized());
        }

        /*
        accessors
        */

        reference front() noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "reference ring_buffer<T>::front() "
                   "noexcept : "
                   "the ring "
                   "buffer has "
                   "no capacity");
            assert(!empty() && "reference ring_buffer<T>::front() "
                               "noexcept : "
                               "the ring "
                               "buffer is empty()");

            return *front_ptr_;
        }

        const_reference front() const noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "const_reference "
                   "ring_buffer<T>::front() const "
                   "noexcept "
                   ": the ring "
                   "buffer has no capacity");
            assert(!empty() && "const_reference "
                               "ring_buffer<T>::front() const "
                               "noexcept "
                               ": the ring buffer is empty()");

            return *front_ptr_;
        }

        reference back() noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "reference ring_buffer<T>::back() the ring "
                   "buffer has "
                   "no capacity");
            assert(!empty() && "reference ring_buffer<T>::back() : the "
                               "ring buffer is empty()");

            pointer last_elem = back_ptr_;
            decrement(last_elem);
            return *last_elem;
        }

        const_reference back() const noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "const_reference ring_buffer<T>::back() "
                   "const "
                   "noexcept "
                   ": the ring "
                   "buffer has no capacity");
            assert(!empty() && "const_reference ring_buffer<T>::back() "
                               "const "
                               "noexcept "
                               ": the ring buffer is empty");

            const_pointer last_elem = back_ptr_;
            decrement(last_elem);
            return *last_elem;
        }

        reference operator[](size_type index) noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "reference "
                   "ring_buffer<T>::operator[](size_type "
                   "index) "
                   "noexcept : "
                   "the ring buffer has no capacity");
            assert(index < size() && "reference "
                                     "ring_buffer<T>::operator[](size_type "
                                     "index) noexcept : index out of range");
            return *index_to_pointer(index);
        }

        const_reference operator[](size_type index) const noexcept {
            assert(storage_.start_ptr() != nullptr &&
                   "const_reference "
                   "ring_buffer<T>::operator[](size_type "
                   "index) const "
                   "the ring buffer has no capacity");
            assert(index < size() && "const_reference "
                                     "ring_buffer<T>::operator[](size_type "
                                     "index) const "
                                     "noexcept : index out of range");
            return *index_to_pointer(index);
        }

        reference at(size_type index) {
            if (index >= size()) {
                throw std::out_of_range("invalid ring_buffer subscript");
            }
            return (*this)[index];
        }

        const_reference at(size_type index) const {
            if (index >= size()) {
                throw std::out_of_range("invalid ring_buffer subscript");
            }
            return (*this)[index];
        }

        /*
        modifiers
        */

        pointer linearize() {
            if (empty()) {
                return nullptr;
            }
            if (is_linearized()) {
                return front_ptr_;
            }

            const size_type reserve_size = reserve();

            if (reserve_size == 0) {
                assert(front_ptr_ == back_ptr_ &&
                       front_ptr_ > storage_.start_ptr());
                std::rotate(storage_.start_ptr(), front_ptr_,
                            storage_.end_ptr());
                front_ptr_ = back_ptr_ = storage_.start_ptr();
                return storage_.start_ptr();
            }

            std::span<T> array1 = get_array<1>(false);
            std::span<T> array2 = get_array<2>(false);
            detail::moving_array_ptr_size<pointer, size_type> moving_array2{
                array2.data(), array2.size()};
            pointer new_array1 = storage_.start_ptr();

            while (!array1.empty()) {
                // shift array 2
                const size_type shift_distance =
                    std::min(reserve_size, array1.size());
                detail::uninitialized_move_overlap(
                    get_allocator(), moving_array2.first(),
                    moving_array2.last(),
                    moving_array2.first() + shift_distance);
                moving_array2.advance(shift_distance);
                // move array1
                new_array1 = std::uninitialized_move(
                    array1.begin(), array1.begin() + shift_distance,
                    new_array1);
                storage_.destroy(array1.begin(),
                                 array1.begin() + shift_distance);
                array1 = array1.subspan(shift_distance);
            }

            front_ptr_ = storage_.start_ptr();
            // no need to wrap since buffer is not full
            back_ptr_ = storage_.start_ptr() + size();
            return storage_.start_ptr();
        }

        void swap(ring_buffer& other) noexcept {
            using std::swap;
            storage_.swap(other.storage_);
            swap(front_ptr_, other.front_ptr_);
            swap(back_ptr_, other.back_ptr_);
            swap(count_, other.count_);
        }

        friend void swap(ring_buffer& lhs, ring_buffer& rhs) {
            rhs.swap(lhs);
        }

        void clear() noexcept {
            if (!storage_.start_ptr()) {
                return;
            }
            destroy_all();
            front_ptr_ = back_ptr_ = storage_.start_ptr();
            count_ = 0;
        }

        void set_capacity(size_type capacity_size) {
            if (capacity_size == capacity()) {
                return;
            }
            auto rbuf = ring_buffer(capacity_size);
            rbuf.insert_back(begin(), end());
            *this = std::move(rbuf);
        }

        void resize(size_type new_size,
                    const value_type& value = value_type()) {
            if (new_size == size()) {
                return;
            }

            size_type current_cap = capacity();

            if (new_size > current_cap) {
                ring_buffer temp_buffer = std::move(*this);
                set_capacity(new_size);

                const bool linearized = temp_buffer.is_linearized();
                auto array1 = temp_buffer.get_array<1>(linearized);
                auto array2 = temp_buffer.get_array<2>(linearized);
                back_ptr_ = std::uninitialized_copy(
                    std::make_move_iterator(array1.begin()),
                    std::make_move_iterator(array1.end()), front_ptr_);
                back_ptr_ = std::uninitialized_copy(
                    std::make_move_iterator(array2.begin()),
                    std::make_move_iterator(array2.end()), back_ptr_);

                std::uninitialized_fill(back_ptr_, storage_.end_ptr(), value);
                back_ptr_ = storage_.start_ptr();
                count_ = new_size;
            }
            else if (new_size < current_cap) {
                if (size() > new_size) {
                    if constexpr (!std::is_trivially_destructible_v<T>) {
                        while (size() > new_size) {
                            pop_back();
                        }
                    }
                    else {
                        back_ptr_ = front_ptr_ + new_size;
                        count_ = new_size;
                        if (back_ptr_ >= storage_.end_ptr()) {
                            back_ptr_ =
                                storage_.start_ptr() +
                                udistance(storage_.end_ptr(), back_ptr_);
                        }
                    }
                }
                else {
                    while (size() < new_size) {
                        unchecked_emplace_back(value);
                    }
                }
            }
            else {
                while (!full()) {
                    unchecked_emplace_back(value);
                }
            }
        }

        template <class U = T,
                  std::enable_if_t<std::is_trivially_constructible_v<U> &&
                                       std::is_trivially_copyable_v<U>,
                                   int> = 0>
        void grow(size_type new_size) {
            if (new_size <= size()) {
                return;
            }

            auto cap = capacity();

            if (new_size < cap) {
                back_ptr_ = front_ptr_ + new_size;
                count_ = new_size;
                if (back_ptr_ >= storage_.end_ptr()) {
                    back_ptr_ = storage_.start_ptr() +
                                udistance(storage_.end_ptr(), back_ptr_);
                }
            }
            else if (new_size > cap) {
                auto temp_rbuf = std::move(*this);
                auto arr1 = temp_rbuf.array_one();
                auto arr2 = temp_rbuf.array_two();

                auto on_exit =
                    scope_exit([&]() { *this = std::move(temp_rbuf); });

                set_capacity(new_size);
                front_ptr_ = storage_.start_ptr();
                back_ptr_ = std::uninitialized_copy(arr1.begin(), arr1.end(),
                                                    front_ptr_);
                back_ptr_ = std::uninitialized_copy(arr2.begin(), arr2.end(),
                                                    back_ptr_);
                back_ptr_ = front_ptr_ = new_size;
                count_ = new_size;

                on_exit.release();
            }
            else {
                count_ = new_size;
                back_ptr_ = front_ptr_;
            }
        }

        /*
        info
        */

        constexpr size_type size() const noexcept {
            return count_;
        }

        constexpr size_type capacity() const noexcept {
            return storage_.capacity();
        }

        constexpr size_type reserve() const noexcept {
            return capacity() - size();
        }

        constexpr bool empty() const noexcept {
            return !size();
        }

        constexpr bool full() const noexcept {
            return size() == capacity();
        }

        constexpr bool is_linearized() const noexcept {
            return back_ptr_ > front_ptr_ || back_ptr_ == storage_.start_ptr();
        }

        allocator_type& get_allocator() noexcept {
            return storage_.get_allocator();
        }

        const allocator_type& get_allocator() const noexcept {
            return storage_.get_allocator();
        }

        friend bool operator==(const ring_buffer& lhs, const ring_buffer& rhs) {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            auto first1 = rhs.begin();
            auto last1 = rhs.end();
            auto first2 = lhs.begin();

            while (first1 != last1) {
                if (*first1++ != *first2++) {
                    return false;
                }
            }

            return true;
        }

        template <class UnaryPred>
        iterator find_if(UnaryPred pred) {
            bool linearized = is_linearized();
            auto array1 = get_array<1>(linearized);
            auto array2 = get_array<2>(linearized);
            auto it = std::find_if(std::begin(array1), std::end(array1), pred);
            if (it != std::end(array1)) {
                return iterator{this, std::addressof(*it)};
            }
            if (linearized) {
                return end();
            }
            it = std::find_if(std::begin(array2), std::end(array2), pred);
            if (it != std::end(array2)) {
                return iterator{this, std::addressof(*it)};
            }
            return end();
        }

        template <class UnaryPred>
        const_iterator find_if(UnaryPred pred) const {
            bool linearized = is_linearized();
            auto array1 = get_array<1>(linearized);
            auto array2 = get_array<2>(linearized);
            auto it = std::find_if(std::begin(array1), std::end(array1), pred);
            if (it != std::end(array1)) {
                return const_iterator{this, std::addressof(*it)};
            }
            if (linearized) {
                return end();
            }
            it = std::find_if(std::begin(array2), std::end(array2), pred);
            if (it != std::end(array2)) {
                return const_iterator{this, std::addressof(*it)};
            }
            return end();
        }

        template <class U>
        iterator find(const U& value) {
            return find_if([&value](const T& v) { return v == value; });
        }

        template <class U>
        const_iterator find(const U& value) const {
            return find_if([&value](const T& v) { return v == value; });
        }

        template <class UnaryPred>
        iterator find_if_not(UnaryPred pred) {
            return find_if([&pred](const T& v) { !pred(v); });
        }

        template <class UnaryPred>
        const_iterator find_if_not(UnaryPred pred) const {
            return find_if([&pred](const T& v) { !pred(v); });
        }

    private:
        detail::ring_buffer_mem_storage<Allocator> storage_;

        pointer front_ptr_ = nullptr;
        pointer back_ptr_ = nullptr;

        size_type count_ = 0;

        template <typename, typename, bool>
        friend class ring_buffer_iterator;

        void copy_other_items(const ring_buffer& other) {
            if (storage_.capacity() < other.storage_.capacity()) {
                storage_.allocate(other.storage_.capacity());
            }
            const bool other_is_linearized = other.is_linearized();
            auto array1 = other.get_array<1>(other_is_linearized);
            auto array2 = other.get_array<2>(other_is_linearized);
            pointer new_back_ptr = storage_.start_ptr();
            new_back_ptr = std::uninitialized_copy(array1.begin(), array1.end(),
                                                   new_back_ptr);
            new_back_ptr = std::uninitialized_copy(array2.begin(), array2.end(),
                                                   new_back_ptr);
            if (new_back_ptr == storage_.end_ptr()) {
                new_back_ptr = storage_.start_ptr();
            }
            front_ptr_ = storage_.start_ptr();
            back_ptr_ = new_back_ptr;
            count_ = other.count_;
        }

        void copy_another_ring_buffer(const ring_buffer& other) {
            assert(empty());
            if (other.empty()) {
                return;
            }
            // copy the allocator if propagate on copy is
            // true
            storage_.copy_assign(other.storage_);
            // copy the items
            copy_other_items(other);
        }

        void move_another_ring_buffer(ring_buffer&& other) noexcept(
            is_move_assign_noexcept) {
            assert(empty());
            // try to take the contents of other or allocate
            // buffer to move its contents if other is empty
            // but has capacity, its storage will be taken
            if (storage_.move_assign(std::move(other.storage_))) {
                front_ptr_ = std::exchange(other.front_ptr_, nullptr);
                back_ptr_ = std::exchange(other.back_ptr_, nullptr);
                count_ = std::exchange(other.count_, 0);
            }
            else {
                if (other.empty()) {
                    return;
                }
                // storage_ has room for the items of
                // other
                const bool other_is_linearized = other.is_linearized();
                auto array1 = other.get_array<1>(other_is_linearized);
                auto array2 = other.get_array<2>(other_is_linearized);
                pointer new_back_ptr = storage_.start_ptr();
                new_back_ptr = std::uninitialized_move(
                    array1.begin(), array1.end(), new_back_ptr);
                new_back_ptr = std::uninitialized_move(
                    array2.begin(), array2.end(), new_back_ptr);
                if (new_back_ptr == storage_.end_ptr()) {
                    new_back_ptr = storage_.start_ptr();
                }
                front_ptr_ = storage_.start_ptr();
                back_ptr_ = new_back_ptr;
                count_ = other.count_;
            }
        }

        void validate_pointer(const_pointer p) const noexcept {
#ifndef NDEBUG
            if (full()) {
                assert(p >= storage_.start_ptr() && p < storage_.end_ptr() &&
                       "the iterator is out of range");
            }
            else if (is_linearized()) {
                if (back_ptr_ == storage_.start_ptr()) {
                    assert(p >= front_ptr_ && p < storage_.end_ptr() &&
                           "the iterator is out of "
                           "range");
                }
                else {
                    assert(p >= front_ptr_ && p < back_ptr_ &&
                           "the iterator is out of "
                           "range");
                }
            }
            else {
                assert(((p >= front_ptr_ && p < storage_.end_ptr()) ||
                        (p >= storage_.start_ptr() && p < back_ptr_)) &&
                       "the iterator is out of range");
            }
#else
            std::ignore = p;
#endif // !NDEBUG
        }

        size_t pointer_to_index(const_pointer p) const noexcept {
            assert(p == nullptr ||
                   (p >= storage_.start_ptr() && p < storage_.end_ptr()));
            if (p == nullptr) {
                return size();
            }
            if (p >= front_ptr_) {
                return p - front_ptr_;
            }
            return (p - storage_.start_ptr()) +
                   (storage_.end_ptr() - front_ptr_);
        }

        pointer index_to_pointer(size_t i) const noexcept {
            assert(i < size());
            if (is_linearized()) {
                return front_ptr_ + i;
            }
            size_t array_1_size = storage_.end_ptr() - front_ptr_;
            if (i < array_1_size) {
                return front_ptr_ + i;
            }
            i -= array_1_size;
            return storage_.start_ptr() + i;
        }

        void increment(pointer& ptr) const noexcept {
            if (++ptr == storage_.end_ptr()) {
                ptr = storage_.start_ptr();
            }
        }

        void increment(const_pointer& ptr) const noexcept {
            if (++ptr == storage_.end_ptr()) {
                ptr = storage_.start_ptr();
            }
        }

        void decrement(pointer& ptr) const noexcept {
            if (ptr == storage_.start_ptr()) {
                ptr = storage_.end_ptr();
            }
            --ptr;
        }

        void decrement(const_pointer& ptr) const noexcept {
            if (ptr == storage_.start_ptr()) {
                ptr = storage_.end_ptr();
            }
            --ptr;
        }

        void increment_by(pointer& ptr, size_t n) const noexcept {
            assert((n <= size() - pointer_to_index(ptr)) &&
                   "can't increment ring_buffer iterator "
                   "past end");
            ptr += n;
            if (ptr >= storage_.end_ptr()) {
                ptr -= capacity();
            }
        }

        void increment_by(const_pointer& ptr, size_t n) const noexcept {
            assert((n <= size() - pointer_to_index(ptr)) &&
                   "can't increment ring_buffer iterator "
                   "past end");
            ptr += n;
            if (ptr >= storage_.end_ptr()) {
                ptr -= capacity();
            }
        }

        void decrement_by(pointer& ptr, size_t n) const noexcept {
            if (ptr == nullptr) {
                assert(size() >= n);
                ptr = storage_.end_ptr() - n;
                return;
            }
            assert((n <= pointer_to_index(ptr)) &&
                   "can't deccrement ring_buffer iterator "
                   "before "
                   "begin");
            ptr -= n;
            if (ptr < storage_.start_ptr()) {
                ptr += capacity();
            }
        }

        void decrement_by(const_pointer& ptr, size_t n) const noexcept {
            if (ptr == nullptr) {
                assert(size() >= n);
                ptr = storage_.end_ptr() - n;
                return;
            }
            assert((n <= pointer_to_index(ptr)) &&
                   "can't deccrement ring_buffer iterator "
                   "before "
                   "begin");
            ptr -= n;
            if (ptr < storage_.start_ptr()) {
                ptr += capacity();
            }
        }

        // push_back
        void increment_back() noexcept {
            increment(back_ptr_);
            ++count_;
        }

        // pop_front
        void increment_front() noexcept {
            increment(front_ptr_);
            --count_;
        }

        // pop_back
        void decrement_back() noexcept {
            decrement(back_ptr_);
            --count_;
        }

        // push_front
        void decrement_front() noexcept {
            decrement(front_ptr_);
            ++count_;
        }

        template <class Iter>
        static size_type udistance(Iter first, Iter last) {
            return static_cast<size_type>(std::distance(first, last));
        }

        void destroy_all() noexcept {
            if constexpr (!std::is_trivially_destructible_v<value_type>) {
                if (!empty()) {
                    bool linearized = is_linearized();
                    auto array1 = get_array<1>(linearized);
                    auto array2 = get_array<2>(linearized);
                    storage_.destroy(array1.begin(), array1.end());
                    storage_.destroy(array2.begin(), array2.end());
                }
            }
        }
    };

    template <class T, std::size_t N>
    class stack_ring_buffer
        : public ring_buffer<T, experimental::stack_allocator<T, N>> {
        using base = ring_buffer<T, experimental::stack_allocator<T, N>>;

    public:
        RAD_PROVIDE_CONTAINER_TYPES(base);

        stack_ring_buffer() noexcept : base(N) {
        }

        stack_ring_buffer(const stack_ring_buffer& other) : base(other) {
        }

        stack_ring_buffer(stack_ring_buffer&& other) noexcept
            : stack_ring_buffer() {
            std::uninitialized_move(other.begin(), other.end(),
                                    std::back_inserter(*this));
        }

        template <class InputIter>
        stack_ring_buffer(InputIter first, InputIter last) : base(first, last) {
        }

        stack_ring_buffer& operator=(const stack_ring_buffer& other) {
            base::operator=(other);
            return *this;
        }

        stack_ring_buffer& operator=(stack_ring_buffer&& other) noexcept {
            this->~stack_ring_buffer();
            new (this) stack_ring_buffer(std::move(other));
            return *this;
        }

        void set_capacity(size_type) = delete;

        void resize(size_type) = delete;
    };

    template <class T, class Allocator, bool is_const>
    inline auto
    ring_buffer_iterator<T, Allocator, is_const>::operator*() const noexcept
        -> deref_type {
        assert(ptr_ != nullptr &&
               "ring_buffer_iterator::operator*() derefrencing end "
               "iterator");
        rbf_ptr_->validate_pointer(ptr_);
        return *ptr_;
    }

    template <class T, class Allocator, bool is_const>
    ring_buffer_iterator<T, Allocator, is_const>&
    ring_buffer_iterator<T, Allocator, is_const>::operator++() noexcept {
        assert(ptr_ != nullptr &&
               "can't increment ring_buffer iterator past end");
        rbf_ptr_->increment(ptr_);
        if (ptr_ == rbf_ptr_->back_ptr_) {
            ptr_ = nullptr;
        }
        return *this;
    }

    template <class T, class Allocator, bool is_const>
    ring_buffer_iterator<T, Allocator, is_const>&
    ring_buffer_iterator<T, Allocator, is_const>::operator--() noexcept {
        assert(ptr_ != rbf_ptr_->front_ptr_ &&
               "can't deccrement ring_buffer iterator before begin");
        if (ptr_ == nullptr) { // for --end(), std::prev(end()) which is
                               // used with reverse iterator
            ptr_ = rbf_ptr_->back_ptr_;
        }
        // decrement does not accept null pointers
        rbf_ptr_->decrement(ptr_);
        return *this;
    }

    template <class T, class Allocator, bool is_const>
    auto ring_buffer_iterator<T, Allocator, is_const>::operator-(
        const ring_buffer_iterator& other) const noexcept -> difference_type {
        return rbf_ptr_->pointer_to_index(ptr_) -
               rbf_ptr_->pointer_to_index(other.ptr_);
    }

    template <class T, class Allocator, bool is_const>
    ring_buffer_iterator<T, Allocator, is_const>&
    ring_buffer_iterator<T, Allocator, is_const>::operator+=(
        typename ring_buffer_iterator::difference_type n) noexcept {
        if (n == 0) {
            return *this;
        }
        if (n < 0) {
            return *this -= -n;
        }
        assert(ptr_ != nullptr &&
               "can't increment ring_buffer iterator past end");
        rbf_ptr_->increment_by(ptr_, static_cast<size_t>(n));
        if (ptr_ == rbf_ptr_->back_ptr_) {
            ptr_ = nullptr;
        }
        return *this;
    }

    template <class T, class Allocator, bool is_const>
    ring_buffer_iterator<T, Allocator, is_const>&
    ring_buffer_iterator<T, Allocator, is_const>::operator-=(
        typename ring_buffer_iterator::difference_type n) noexcept {
        if (n == 0) {
            return *this;
        }
        if (n < 0) {
            return *this += -n;
        }
        assert(ptr_ != rbf_ptr_->front_ptr_ &&
               "can't deccrement ring_buffer iterator before begin");
        // decrement_by accepts null pointers to know it is the back
        // pointer
        rbf_ptr_->decrement_by(ptr_, static_cast<size_t>(n));
        return *this;
    }

}; // namespace RAD_LIB_NAMESPACE