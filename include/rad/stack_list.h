#pragma once
#include <rad/libbase.h>

#include <cassert>
#include <iterator>
#include <utility>

namespace RAD_LIB_NAMESPACE {

    template <class T>
    class stack_list;

    class stack_double_list_node {
        friend class stack_list_base;

    public:
        stack_double_list_node() = default;

        stack_double_list_node(const stack_double_list_node&) noexcept
            : stack_double_list_node() {
        }

        stack_double_list_node&
        operator=(const stack_double_list_node&) noexcept {
            return *this;
        }

        stack_double_list_node(stack_double_list_node&&) noexcept
            : stack_double_list_node() {
        }

        stack_double_list_node& operator=(stack_double_list_node&&) noexcept {
            return *this;
        }

#ifndef NDEBUG
        ~stack_double_list_node() {
            assert(container == nullptr &&
                   "double list node destoryed while being "
                   "linked "
                   "to a list");
        }
#endif // !NDEBUG

        friend constexpr stack_double_list_node*
        get_next_node(stack_double_list_node& node) noexcept {
            return node.next_node;
        }

        friend constexpr const stack_double_list_node*
        get_next_node(const stack_double_list_node& node) noexcept {
            return node.next_node;
        }

        friend constexpr stack_double_list_node*
        get_previous_node(stack_double_list_node& node) noexcept {
            return node.prev_node;
        }

        friend constexpr const stack_double_list_node*
        get_previous_node(const stack_double_list_node& node) noexcept {
            return node.prev_node;
        }

    private:
        void next(stack_double_list_node* node) noexcept {
            next_node = node;
        }

        stack_double_list_node* next() noexcept {
            return next_node;
        }

        const stack_double_list_node* next() const noexcept {
            return next_node;
        }

        void previous(stack_double_list_node* node) noexcept {
            prev_node = node;
        }

        stack_double_list_node* previous() noexcept {
            return prev_node;
        }

        const stack_double_list_node* previous() const noexcept {
            return prev_node;
        }

#ifndef NDEBUG
        void unlink_container() noexcept {
            container = nullptr;
        }

        void link_container(void* c) noexcept {
            assert((c == nullptr || container != c) &&
                   "double list node already linked to "
                   "this list");
            assert(container == nullptr && "double list node already linked to "
                                           "another list");
            container = c;
        }

        bool is_linked_to(const void* c) const noexcept {
            return c == container;
        }
#else
        constexpr void unlink_container() const noexcept {
        }

        constexpr void link_container(const void*) const noexcept {
        }

        constexpr bool is_linked_to(const void*) const noexcept {
            return false;
        }
#endif // !NDEBUG

    private:
#ifndef NDEBUG
        void* container = nullptr;
#endif // !NDEBUG

        stack_double_list_node* next_node = nullptr;
        stack_double_list_node* prev_node = nullptr;
    };

    class stack_list_base : noncopyable {
    public:
        using node_type = stack_double_list_node;
        using size_type = std::size_t;

        constexpr stack_list_base() noexcept = default;

        stack_list_base(stack_list_base&& other) noexcept
            : nodes_count(std::exchange(other.nodes_count, 0)),
              head(std::exchange(other.head, nullptr)),
              tail(std::exchange(other.tail, nullptr)) {
            unlink_all_nodes(this);
            other.assert_list_is_valid();
            assert_list_is_valid();
        }

        stack_list_base& operator=(stack_list_base&& other) noexcept {
            if (this == std::addressof(other)) {
                return *this;
            }
            unlink_all_nodes();
            other.unlink_all_nodes(this);
            nodes_count = std::exchange(other.nodes_count, 0);
            head = std::exchange(other.head, nullptr);
            tail = std::exchange(other.tail, nullptr);
            other.assert_list_is_valid();
            assert_list_is_valid();
            return *this;
        }

        ~stack_list_base() {
            unlink_all_nodes();
        }

        constexpr bool empty() const noexcept {
            return nodes_count == 0;
        }

        constexpr size_type size() const noexcept {
            return nodes_count;
        }

        void clear() noexcept {
            unlink_all_nodes();
            nodes_count = 0;
            head = tail = nullptr;
            assert_list_is_valid();
        }

    protected:
        node_type* front_node() noexcept {
            return head;
        }

        const node_type* front_node() const noexcept {
            return head;
        }

        node_type* back_node() noexcept {
            return tail;
        }

        const node_type* back_node() const noexcept {
            return tail;
        }

        void merge_back_nodes(stack_list_base& other) noexcept {
            if (this == std::addressof(other)) {
                return;
            }
            // no nodes will be moved
            if (other.empty()) {
                return;
            }

            // if no already list, just move the other list
            if (empty()) {
                *this = std::move(other);
                assert_list_is_valid();
                other.assert_list_is_valid();
                return;
            }

            other.unlink_all_nodes(this);

            nodes_count += std::exchange(other.nodes_count, 0);

            // --- | [] tail [x] | [x] o-head [] | ---- | []
            // o-tail [null] |
            tail->next(other.head);
            other.head->previous(tail);
            tail = std::exchange(other.tail, nullptr);
            other.head = nullptr;

            assert_list_is_valid();
            other.assert_list_is_valid();
        }

        void merge_front_nodes(stack_list_base& other) noexcept {
            if (this == std::addressof(other)) {
                return;
            }
            // no nodes will be moved
            if (other.empty()) {
                return;
            }

            // if no already list, just move the other list
            if (empty()) {
                *this = std::move(other);
                assert_list_is_valid();
                other.assert_list_is_valid();
                return;
            }

            other.unlink_all_nodes(this);

            nodes_count += std::exchange(other.nodes_count, 0);

            // --- | [] o-tail [x] | [x] head [] | ---- | []
            // tail [null] |
            other.tail->next(head);
            head->previous(other.tail);
            head = std::exchange(other.head, nullptr);
            other.tail = nullptr;

            assert_list_is_valid();
            other.assert_list_is_valid();
        }

        void push_front_node(node_type* node) noexcept {
            validate_node(node, false);
            node->link_container(this);

            ++nodes_count;

            if (nodes_count == 1) {
                // | [x] node [x] |
                node->previous(nullptr);
                node->next(nullptr);
                head = tail = node;
                assert_list_is_valid();
                return;
            }

            // | [x] node [x] | [x] head [] |

            node->previous(nullptr);
            node->next(head);
            head->previous(node);

            head = node;
            assert_list_is_valid();
        }

        void push_back_node(node_type* node) noexcept {
            validate_node(node, false);
            node->link_container(this);

            ++nodes_count;

            if (nodes_count == 1) {
                // | [x] node [x] |
                node->previous(nullptr);
                node->next(nullptr);
                head = tail = node;
                assert_list_is_valid();
                return;
            }

            // | [] tail [x] | [x] node [x] |

            tail->next(node);
            node->previous(tail);
            node->next(nullptr);

            tail = node;
            assert_list_is_valid();
        }

        void push_after_node(node_type* prev, node_type* node) noexcept {
            // insertion at end
            if (!prev) {
                return push_back_node(node);
            }

            validate_node(prev, true);
            validate_node(node, false);
            node->link_container(this);

            ++nodes_count;

            // | [] prev [x] | [x] node [x] | [x] next [] |
            // | [] prev [x] | [x] node [x] | null |

            auto next = prev->next();

            prev->next(node);

            node->previous(prev);
            node->next(next);

            if (next != nullptr) {
                next->previous(node);
            }
            else {
                tail = node;
            }
            assert_list_is_valid();
        }

        void push_before_node(node_type* next, node_type* node) noexcept {
            validate_node(next, true);
            validate_node(node, false);
            node->link_container(this);

            ++nodes_count;

            // | [] prev [x] | [x] node [x] | [x] next [] |
            // | null | [x] node [x] | [x] next [] |

            auto prev = next->previous();

            if (prev != nullptr) {
                prev->next(node);
            }
            else {
                head = node;
            }

            node->previous(prev);
            node->next(next);

            next->previous(node);
            assert_list_is_valid();
        }

        node_type* try_pop_front_node() noexcept {
            if (nodes_count == 0) {
                return nullptr;
            }
            nodes_count -= 1;

            auto node = head;
            node->unlink_container();

            head = head->next();
            if (!head) {
                tail = nullptr;
            }
            else {
                head->previous(nullptr);
            }

            node->next(nullptr);
            assert_list_is_valid();
            return node;
        }

        node_type* try_pop_back_node() noexcept {
            if (nodes_count == 0) {
                return nullptr;
            }
            nodes_count -= 1;

            auto node = tail;
            node->unlink_container();

            tail = tail->previous();
            if (tail == nullptr) {
                head = nullptr;
            }
            else {
                tail->next(nullptr);
            }

            node->previous(nullptr);
            assert_list_is_valid();
            return node;
        }

        node_type* erase_node(node_type* node) noexcept {
            validate_node(node, true);

            if (node == head) {
                try_pop_front_node();
                return head;
            }
            else if (node == tail) {
                try_pop_back_node();
                return nullptr;
            }

            --nodes_count;

            node->unlink_container();

            auto next_node = node->next();
            auto prev_node = node->previous();

            prev_node->next(next_node);
            next_node->previous(prev_node);
            assert_list_is_valid();
            return next_node;
        }

        template <class BeginIter, class EndIter>
        void insert_back_nodes(BeginIter first, EndIter last) noexcept {
            if (first == last) {
                return;
            }

            last = std::prev(last);

            // | [] tail [x] | [x] first [x] | ----- | [x]
            // last [x] | |     null    | [x] first [x] |
            // ----- | [x] last [x] |

            ++nodes_count;

            if (tail) {
                tail->next(std::addressof(*first));
            }
            else {
                head = std::addressof(*first);
            }

            static_cast<node_type*>(std::addressof(*first))->previous(tail);
            static_cast<node_type*>(std::addressof(*last))->next(nullptr);

            tail = std::addressof(*last);

            while (first != last) {
                first->link_container(this);
                auto next = std::next(first);
                static_cast<node_type*>(std::addressof(*first))
                    ->next(std::addressof(*next));
                next->previous(std::addressof(*first));
                ++nodes_count;
                first = std::next(first);
            }

            assert_list_is_valid();
        }

    private:
#ifndef NDEBUG
        void validate_node(node_type* node, bool should_exist) noexcept {
            assert(node != nullptr && "node is null");
            bool node_exists = node->container == this;

            if (should_exist) {
                assert(node_exists && "the node is not linked to this "
                                      "list");
            }
            else {
                assert(!node_exists && "the node is already linked to "
                                       "this list");
            }
        }

        void unlink_all_nodes(void* new_list = nullptr) noexcept {
            auto first = head;
            while (first) {
                first->unlink_container();
                first->link_container(new_list);
                first = first->next();
            }
        }

        void assert_list_is_valid() const noexcept {
            if (nodes_count == 0) {
                assert(head == nullptr &&
                       "empty double list must have null head");
                assert(tail == nullptr &&
                       "empty double list must have null tail");
            }
            else {
                assert(head != nullptr &&
                       "non empty double list must have non null head");
                assert(tail != nullptr &&
                       "non empty double list must have non null tail");
                assert(tail->next() == nullptr &&
                       "non empty double list must have tail with null next");
                assert(
                    head->previous() == nullptr &&
                    "non empty double list must have head with null previous");
                if (nodes_count == 1) {
                    assert(head == tail && "double list with one item must "
                                           "have same head and tail");
                }
                else {
                    assert(head != tail &&
                           "double list with more than one item must have "
                           "different head and tail");
                }
            }
        }
#else
        constexpr void validate_node(const void*, bool) const noexcept {
        }

        constexpr void unlink_all_nodes(const void* = nullptr) const noexcept {
        }

        constexpr void assert_list_is_valid() const noexcept {
        }
#endif // !NDEBUG

        size_type nodes_count = 0;
        node_type* head = nullptr;
        node_type* tail = nullptr; // for fast back access and insertion
    };

    struct dummy_node : stack_double_list_node {};

    template <class T>
    class stack_list : public stack_list_base {
        template <bool is_const>
        class iterator_base;

        template <typename>
        friend class stack_list;

        template <bool>
        friend class iterator_base;

        template <class Derived>
        static stack_list derived_list_to_base(stack_list<Derived>&& other) {
            return stack_list{std::move(static_cast<stack_list_base&>(other))};
        }

    public:
        static_assert(is_public_base_of<stack_double_list_node, T>,
                      "stack_double_list_node must be a public base of "
                      "T");

        using iterator = iterator_base<false>;
        using const_iterator = iterator_base<true>;
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator_type = iterator;

        constexpr stack_list() noexcept = default;

        stack_list(stack_list&& other) noexcept
            : stack_list_base(std::move(other)) {
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        stack_list(stack_list<Derived>&& other) noexcept
            : stack_list(derived_list_to_base(std::move(other))) {
        }

        stack_list& operator=(stack_list&& other) noexcept {
            stack_list_base::operator=(std::move(other));
            return *this;
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        stack_list& operator=(stack_list<Derived>&& other) noexcept {
            return operator=(derived_list_to_base(std::move(other)));
        }

        reference front() noexcept {
            assert(!empty() && "front() called on empty list");
            return *static_cast<pointer>(front_node());
        }

        const_reference front() const noexcept {
            assert(!empty() && "front() called on empty list");
            return *static_cast<const_pointer>(front_node());
        }

        reference back() noexcept {
            assert(!empty() && "back() called on empty list");
            return *static_cast<pointer>(back_node());
        }

        const_reference back() const noexcept {
            assert(!empty() && "back() called on empty list");
            return *static_cast<const_pointer>(back_node());
        }

        void merge_back(stack_list& other) noexcept {
            merge_back_nodes(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_back(stack_list<Derived>& other) noexcept {
            merge_back(derived_list_to_base(other));
        }

        void merge_back(stack_list&& other) noexcept {
            merge_back(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_back(stack_list<Derived>&& other) noexcept {
            merge_back(derived_list_to_base(other));
        }

        void merge_front(stack_list& other) noexcept {
            merge_front_nodes(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_front(stack_list<Derived>& other) noexcept {
            merge_front(derived_list_to_base(other));
        }

        void merge_front(stack_list&& other) noexcept {
            merge_front(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_front(stack_list<Derived>&& other) noexcept {
            merge_front(derived_list_to_base(other));
        }

        // using pointers

        void push_front(pointer node) noexcept {
            push_front_node(node);
        }

        void push_back(pointer node) noexcept {
            push_back_node(node);
        }

        void push_after(pointer pos, pointer node) noexcept {
            push_after_node(pos, node);
        }

        void push_after(iterator pos, pointer node) noexcept;

        void push_before(pointer pos, pointer node) noexcept {
            push_before_node(pos, node);
        }

        void push_before(iterator pos, pointer node) noexcept;

        // using references

        void push_front(reference node) noexcept {
            push_front(&node);
        }

        void push_back(reference node) noexcept {
            push_back(&node);
        }

        void push_after(reference prev, reference node) noexcept {
            push_after(&prev, &node);
        }

        void push_after(iterator pos, reference node) noexcept;

        void push_before(reference next, reference node) noexcept {
            push_before(&next, &node);
        }

        void push_before(iterator pos, reference node) noexcept;

        pointer try_pop_front() noexcept {
            return static_cast<pointer>(try_pop_front_node());
        }

        reference pop_front() noexcept {
            assert(!empty() && "pop_front called on empty list");
            return *try_pop_front();
        }

        pointer try_pop_back() noexcept {
            return static_cast<pointer>(try_pop_back_node());
        }

        reference pop_back() noexcept {
            assert(!empty() && "pop_back called on empty list");
            return *try_pop_back();
        }

        template <class UnaryPredicate>
        iterator find(UnaryPredicate pred);

        iterator find(const_reference value);

        template <class UnaryPredicate>
        const_iterator find(UnaryPredicate pred) const;

        const_iterator find(const_reference value) const;

        template <class UnaryPredicate>
        bool contains(UnaryPredicate pred) const;

        bool contains(const_reference value) const;

        pointer erase(pointer node) noexcept {
            return static_cast<pointer>(erase_node(node));
        }

        void erase(reference node) noexcept {
            erase_node(std::addressof(node));
        }

        iterator erase(iterator pos) noexcept;

        template <class UnaryPredicate>
        size_type remove_if(UnaryPredicate pred) {
            size_type removed_count = 0;
            node_type* node = front_node();
            while (node != nullptr) {
                if (pred(*static_cast<const_pointer>(node))) {
                    node = erase_node(node);
                    removed_count += 1;
                }
                else {
                    node = get_next_node(*node);
                }
            }
            return removed_count;
        }

        size_type remove_if(const_reference value) {
            return remove_if(
                [&value](const auto& elem) { return elem == value; });
        }

        void insert_front(stack_list& other) noexcept {
            merge_front(other);
        }

        void insert_front(stack_list&& other) noexcept {
            insert_front(other);
        }

        void insert_back(stack_list& other) noexcept {
            merge_back(other);
        }

        void insert_back(stack_list&& other) noexcept {
            merge_back(other);
        }

        template <class BeginIter, class EndIter>
        void insert_front(BeginIter first, EndIter last) noexcept {
            stack_list front_list;
            front_list.insert_back(first, last);
            merge_front(std::move(front_list));
        }

        template <class BeginIter, class EndIter>
        void insert_back(BeginIter first, EndIter last) noexcept {
            insert_back_nodes(first, last);
        }

        iterator begin() noexcept;

        iterator end() noexcept;

        const_iterator begin() const noexcept;

        const_iterator end() const noexcept;

        const_iterator cbegin() const noexcept;

        const_iterator cend() const noexcept;

        auto rbegin() noexcept;

        auto rend() noexcept;

        auto rbegin() const noexcept;

        auto rend() const noexcept;

        auto crbegin() const noexcept;

        auto crend() const noexcept;

    private:
#ifndef NDEBUG
        iterator make_iterator(node_type* node) {
            return iterator{node, back_node(), this};
        }
#else
        iterator make_iterator(node_type* node) {
            return iterator{node, back_node()};
        }
#endif // !NDEBUG
    };

    template <class T>
    template <bool is_const>
    class stack_list<T>::iterator_base {
        template <typename>
        friend class stack_list;

    public:
        using list_type = stack_list<T>;
        using node_type = typename list_type::node_type;
        using value_type = typename list_type::value_type;
        using pointer = typename list_type::pointer;
        using const_pointer = typename list_type::const_pointer;
        using reference = typename list_type::reference;
        using const_reference = typename list_type::const_reference;
        using size_type = typename list_type::size_type;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::bidirectional_iterator_tag;

    private:
        using deref_ref =
            std::conditional_t<is_const, const_reference, reference>;

        using deref_ptr = std::conditional_t<is_const, const_pointer, pointer>;

        using node_ptr_t =
            std::conditional_t<is_const, const node_type*, node_type*>;

    public:
        iterator_base() = default;

#ifndef NDEBUG
        iterator_base(node_ptr_t node, node_ptr_t tail, list_type* c) noexcept
            : node_ptr(node), tail_ptr(tail), container(c) {
        }

        iterator_base(node_ptr_t tail, list_type* c) noexcept
            : tail_ptr(tail), container(c) {
        }
#else
        iterator_base(node_ptr_t node, node_ptr_t tail) noexcept
            : node_ptr(node), tail_ptr(tail) {
        }

        iterator_base(node_ptr_t tail) noexcept : tail_ptr(tail) {
        }
#endif // !_DEBUG

        deref_ref operator*() const noexcept {
            assert(container != nullptr &&
                   "cannot dereference value-initialized list "
                   "iterator");
            assert(node_ptr != nullptr &&
                   "cannot dereference end list iterator");

            return *get();
        }

        deref_ptr operator->() const noexcept {
            return get();
        }

        deref_ptr get() const noexcept {
            assert(container != nullptr &&
                   "cannot dereference value-initialized list "
                   "iterator");
            assert(node_ptr != nullptr &&
                   "cannot dereference end list iterator");

            return static_cast<deref_ptr>(node_ptr);
        }

        iterator_base& operator++() noexcept {
            assert(container != nullptr && "cannot increment value-initialized "
                                           "list iterator");
            assert(node_ptr != nullptr && "cannot increment end list iterator");

            node_ptr = get_next_node(*node_ptr);
            return *this;
        }

        iterator_base operator++(int) noexcept {
            auto saved_it{*this};
            ++(*this);
            return saved_it;
        }

        iterator_base& operator--() noexcept {
            assert(container != nullptr && "cannot decrement value-initialized "
                                           "list iterator");
            assert(!container->empty() && node_ptr != &container->front() &&
                   "cannot decrement begin list iterator");

            if (!node_ptr) {
                node_ptr = tail_ptr;
            }
            else {
                node_ptr = get_previous_node(*node_ptr);
            }
            return *this;
        }

        iterator_base operator--(int) noexcept {
            auto saved_it{*this};
            --(*this);
            return saved_it;
        }

        friend bool operator==(const iterator_base& lhs,
                               const iterator_base& rhs) noexcept {
            assert(lhs.container == rhs.container &&
                   "invalid list iterator comparison");
            return lhs.node_ptr == rhs.node_ptr;
        }

        friend bool operator!=(const iterator_base& lhs,
                               const iterator_base& rhs) noexcept {
            assert(lhs.container == rhs.container &&
                   "invalid list iterator comparison");
            return lhs.node_ptr != rhs.node_ptr;
        }

        operator iterator_base<true>() const noexcept
            requires(!is_const)
        {
#ifndef NDEBUG
            return iterator_base<true>{node_ptr, tail_ptr, container};
#else
            return iterator_base<true>{node_ptr, tail_ptr};
#endif // !_DEBUG
        }

    private:
        node_ptr_t node() const noexcept {
            return node_ptr;
        }

        void unlink_container() {
#ifndef NDEBUG
            container = nullptr;
#endif // !NDEBUG
        }

        node_ptr_t node_ptr = nullptr;
        node_ptr_t tail_ptr = nullptr;

#ifndef NDEBUG
        list_type* container = nullptr;
#endif // !NDEBUG
    };

    template <class T>
    void stack_list<T>::push_after(iterator pos, pointer node) noexcept {
        push_after_node(pos.node(), node);
    }

    template <class T>
    void stack_list<T>::push_after(iterator pos, reference node) noexcept {
        push_after_node(pos.node(), &node);
    }

    template <class T>
    void stack_list<T>::push_before(iterator pos, pointer node) noexcept {
        push_before_node(pos.node(), node);
    }

    template <class T>
    void stack_list<T>::push_before(iterator pos, reference node) noexcept {
        push_before_node(pos.node(), &node);
    }

    template <class T>
    template <class UnaryPredicate>
    auto stack_list<T>::find(UnaryPredicate pred) -> iterator {
        node_type* node = front_node();
        while (node != nullptr) {
            if (pred(*static_cast<const_pointer>(node))) {
                return make_iterator(node);
            }
            node = get_next_node(*node);
        }
        return end();
    }

    template <class T>
    auto stack_list<T>::find(const_reference value) -> iterator {
        return find(
            [value = &value](const auto& elem) { return elem == *value; });
    }

    template <class T>
    template <class UnaryPredicate>
    auto stack_list<T>::find(UnaryPredicate pred) const -> const_iterator {
        return const_cast<stack_list*>(this)->find(pred);
    }

    template <class T>
    auto stack_list<T>::find(const_reference value) const -> const_iterator {
        return const_cast<stack_list*>(this)->find(value);
    }

    template <class T>
    template <class UnaryPredicate>
    bool stack_list<T>::contains(UnaryPredicate pred) const {
        return find(pred) != end();
    }

    template <class T>
    bool stack_list<T>::contains(const_reference value) const {
        return find(value) != end();
    }

    template <class T>
    auto stack_list<T>::erase(iterator pos) noexcept -> iterator {
        assert(pos.container == this && "invalid list iterator");
        assert(pos.node_ptr != nullptr &&
               "cannot dereference end list iterator");

        return make_iterator(erase_node(pos.node()));
    }

    template <class T>
    auto stack_list<T>::begin() noexcept -> iterator {
        return make_iterator(front_node());
    }

    template <class T>
    auto stack_list<T>::end() noexcept -> iterator {
        return make_iterator(nullptr);
    }

    template <class T>
    auto stack_list<T>::begin() const noexcept -> const_iterator {
        return const_cast<stack_list*>(this)->begin();
    }

    template <class T>
    auto stack_list<T>::end() const noexcept -> const_iterator {
        return const_cast<stack_list*>(this)->end();
    }

    template <class T>
    auto stack_list<T>::cbegin() const noexcept -> const_iterator {
        return begin();
    }

    template <class T>
    auto stack_list<T>::cend() const noexcept -> const_iterator {
        return end();
    }

    template <class T>
    auto stack_list<T>::rbegin() noexcept {
        return std::make_reverse_iterator(end());
    }

    template <class T>
    auto stack_list<T>::rend() noexcept {
        return std::make_reverse_iterator(begin());
    }

    template <class T>
    auto stack_list<T>::rbegin() const noexcept {
        return std::make_reverse_iterator(end());
    }

    template <class T>
    auto stack_list<T>::rend() const noexcept {
        return std::make_reverse_iterator(begin());
    }

    template <class T>
    auto stack_list<T>::crbegin() const noexcept {
        return std::make_reverse_iterator(end());
    }

    template <class T>
    auto stack_list<T>::crend() const noexcept {
        return std::make_reverse_iterator(begin());
    }

} // namespace RAD_LIB_NAMESPACE
