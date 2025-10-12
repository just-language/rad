#pragma once
#include <rad/libbase.h>

#include <cassert>
#include <iterator>
#include <utility>

namespace RAD_LIB_NAMESPACE {
    template <class T>
    class stack_forward_list;

    class stack_forward_list_node {
        friend class stack_forward_list_base;

    public:
        stack_forward_list_node() noexcept {};

        stack_forward_list_node(const stack_forward_list_node&) noexcept {
        }

        stack_forward_list_node(stack_forward_list_node&&) noexcept {
        }

        stack_forward_list_node&
        operator=(const stack_forward_list_node&) noexcept {
            return *this;
        }

        stack_forward_list_node& operator=(stack_forward_list_node&&) noexcept {
            return *this;
        }

#ifndef NDEBUG
        ~stack_forward_list_node() {
            assert(container == nullptr && "forward list node destoryed while "
                                           "being linked "
                                           "to a list");
        }
#endif // !NDEBUG

        friend constexpr stack_forward_list_node*
        get_next_node(stack_forward_list_node& node) noexcept {
            return node.next_node;
        }

    private:
        stack_forward_list_node* next() noexcept {
            return next_node;
        }

        const stack_forward_list_node* next() const noexcept {
            return next_node;
        }

        void next(stack_forward_list_node* node) noexcept {
            next_node = node;
        }

#ifndef NDEBUG
        void unlink_container() {
            container = nullptr;
        }

        void link_container(void* c) {
            assert(container == nullptr &&
                   "forward list node already linked to "
                   "another list");
            container = c;
        }

        void* container = nullptr;
#else
        constexpr void unlink_container() const noexcept {
        }

        constexpr void
        link_container([[maybe_unused]] const void* c) const noexcept {
        }
#endif // !NDEBUG

        stack_forward_list_node* next_node = nullptr;
    };

    class stack_forward_list_base : noncopyable {
    public:
        using size_type = std::size_t;
        using node_type = stack_forward_list_node;

        constexpr stack_forward_list_base() = default;

        stack_forward_list_base(stack_forward_list_base&& other) noexcept
            : nodes_count(std::exchange(other.nodes_count, 0)),
              head(std::exchange(other.head, nullptr)),
              tail(std::exchange(other.tail, nullptr)) {
            unlink_all_nodes(this);
            other.assert_list_is_valid();
            assert_list_is_valid();
        }

        stack_forward_list_base&
        operator=(stack_forward_list_base&& other) noexcept {
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

#ifndef NDEBUG
        ~stack_forward_list_base() {
            unlink_all_nodes();
        }
#endif // !NDEBUG

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
            assert(!empty() && "front() called on empty list");
            assert_list_is_valid();
            return head;
        }

        const node_type* front_node() const noexcept {
            assert(!empty() && "front() called on empty list");
            assert_list_is_valid();
            return head;
        }

        node_type* back_node() noexcept {
            assert(!empty() && "back() called on empty list");
            assert_list_is_valid();
            return tail;
        }

        const node_type* back_node() const noexcept {
            assert(!empty() && "back() called on empty list");
            assert_list_is_valid();
            return tail;
        }

        void set_head_tail_count(node_type* h, node_type* t,
                                 size_type n) noexcept {
            head = h;
            tail = t;
            nodes_count = n;
            assert_list_is_valid();
        }

        void merge_back_nodes(stack_forward_list_base& other) noexcept {
            if (this == std::addressof(other)) {
                return;
            }
            if (other.empty()) {
                return;
            }

            if (empty()) {
                *this = std::move(other);
                other.assert_list_is_valid();
                assert_list_is_valid();
                return;
            }

            other.unlink_all_nodes(this);

            nodes_count += std::exchange(other.nodes_count, 0);

            // | tail [x] | o-head [] | o-tail |

            tail->next(std::exchange(other.head, nullptr));
            tail = std::exchange(other.tail, nullptr);

            other.assert_list_is_valid();
            assert_list_is_valid();
        }

        void merge_front_nodes(stack_forward_list_base& other) noexcept {
            if (this == std::addressof(other)) {
                return;
            }
            if (other.empty()) {
                return;
            }

            if (empty()) {
                *this = std::move(other);
                other.assert_list_is_valid();
                assert_list_is_valid();
                return;
            }

            other.unlink_all_nodes(this);
            nodes_count += std::exchange(other.nodes_count, 0);

            node_type* new_head = std::exchange(other.head, nullptr);
            other.tail->next(head);
            other.tail = nullptr;
            head = new_head;
            other.assert_list_is_valid();
            assert_list_is_valid();
        }

        void push_back_node(node_type* node) noexcept {
            validate_node(node, false);
            node->link_container(this);

            ++nodes_count;

            node->next(nullptr);

            if (nodes_count == 1) {
                // | node [x] |
                head = tail = node;
                assert_list_is_valid();
                return;
            }

            // | tail [x] | node [x] |
            tail->next(node);
            tail = node;
            assert_list_is_valid();
        }

        void push_front_node(node_type* node) noexcept {
            validate_node(node, false);
            node->link_container(this);

            ++nodes_count;

            if (nodes_count == 1) {
                // | node [x] |
                node->next(nullptr);
                head = tail = node;
                assert_list_is_valid();
                return;
            }

            // | node [x] | head [] |
            auto old_head = std::exchange(head, node);
            head->next(old_head);
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

            assert_list_is_valid();
            return node;
        }

        node_type* try_pop_back_node() noexcept {
            if (nodes_count == 0) {
                return nullptr;
            }
            nodes_count -= 1;

            // unlink the last node (tail)
            node_type *last = head, *prev_last = nullptr;
            while (last->next() != nullptr) {
                prev_last = last;
                last = last->next();
            }

            last->unlink_container();

            if (prev_last) {
                prev_last->next(nullptr);
            }

            // set a new tail and return the last node
            tail = prev_last;
            assert_list_is_valid();
            return last;
        }

    private:
#ifndef NDEBUG
        void validate_node(node_type* node, bool should_exist) const noexcept {
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
                       "empty forward list must have null head");
                assert(tail == nullptr &&
                       "empty forward list must have null tail");
            }
            else {
                assert(head != nullptr &&
                       "non empty forward list must have non null head");
                assert(tail != nullptr &&
                       "non empty forward list must have non null tail");
                assert(tail->next() == nullptr &&
                       "non empty forward list must have tail with null next");
                if (nodes_count == 1) {
                    assert(head == tail && "forward list with one item must "
                                           "have same head and tail");
                }
                else {
                    assert(head != tail &&
                           "forward list with more than one item must have "
                           "different head and tail");
                }
            }
        }
#else
        constexpr void validate_node(const void*, bool) const noexcept {
        }

        constexpr void unlink_all_nodes(const void*) const noexcept {
        }

        constexpr void unlink_all_nodes() const noexcept {
        }

        constexpr void assert_list_is_valid() const noexcept {
        }
#endif

        size_type nodes_count = 0;
        node_type* head = nullptr;
        node_type* tail = nullptr; // for fast back access and insertion
    };

    template <class T>
    class stack_forward_list : public stack_forward_list_base {
        template <typename>
        friend class stack_forward_list;

        stack_forward_list(stack_forward_list_base&& other) noexcept
            : stack_forward_list_base(std::move(other)) {
        }

        template <class Derived>
        static stack_forward_list
        derived_list_to_base(stack_forward_list<Derived>&& other) {
            return stack_forward_list{
                std::move(static_cast<stack_forward_list_base&>(other))};
        }

    public:
        static_assert(is_public_base_of<stack_forward_list_node, T>,
                      "stack_forward_list_node must be a public base of "
                      "T");

        class iterator;

        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator_type = iterator;

        constexpr stack_forward_list() noexcept = default;

        stack_forward_list(stack_forward_list&& other) noexcept
            : stack_forward_list_base(std::move(other)) {
        }

        template <class Derived,
                  std::enable_if_t<std::is_base_of_v<T, Derived>, int> = 0>
        stack_forward_list(stack_forward_list<Derived>&& other) noexcept
            : stack_forward_list(derived_list_to_base(std::move(other))) {
        }

        template <class BeginIter, class EndIter>
        stack_forward_list(BeginIter first, EndIter last) {
            if (first == last) {
                return;
            }

            node_type* h = std::addressof(*first);
            node_type* t = std::addressof(*std::prev(last));
            size_type count = 0;
            while (first != last) {
                first->link_container(this);
                auto next_it = std::next(first);
                first->next(std::addressof(*next_it));
                ++count;
                first = next_it;
            }

            set_head_tail_count(h, t, count);
        }

        stack_forward_list& operator=(stack_forward_list&& other) noexcept {
            stack_forward_list_base::operator=(std::move(other));
            return *this;
        }

        template <class Derived,
                  std::enable_if_t<std::is_base_of_v<T, Derived>, int> = 0>
        stack_forward_list&
        operator=(stack_forward_list<Derived>&& other) noexcept {
            return operator=(derived_list_to_base(std::move(other)));
        }

        reference front() noexcept {
            return *static_cast<pointer>(front_node());
        }

        const_reference front() const noexcept {
            return *static_cast<const_pointer>(front_node());
        }

        reference back() noexcept {
            return *static_cast<pointer>(back_node());
        }

        const_reference back() const noexcept {
            return *static_cast<const_pointer>(back_node());
        }

        void merge_back(stack_forward_list& other) noexcept {
            merge_back_nodes(other);
        }

        void merge_front(stack_forward_list& other) noexcept {
            merge_front_nodes(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_back(stack_forward_list<Derived>& other) noexcept {
            merge_back(derived_list_to_base(std::move(other)));
        }

        void merge_back(stack_forward_list&& other) noexcept {
            merge_back(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_back(stack_forward_list<Derived>&& other) noexcept {
            merge_back(derived_list_to_base(std::move(other)));
        }

        void merge_front(stack_forward_list&& other) noexcept {
            merge_front(other);
        }

        template <class Derived,
                  std::enable_if_t<is_public_base_of<T, Derived>, int> = 0>
        void merge_front(stack_forward_list<Derived>&& other) noexcept {
            merge_front(derived_list_to_base(std::move(other)));
        }

        template <class BeginIter, class EndIter>
        void insert_back(BeginIter first, EndIter last) {
            merge_back(stack_forward_list{first, last});
        }

        // using pointers

        void push_back(pointer node) noexcept {
            push_back_node(node);
        }

        void push_front(pointer node) noexcept {
            push_front_node(node);
        }

        // using references

        void push_back(reference node) noexcept {
            push_back_node(&node);
        }

        void push_front(reference node) noexcept {
            push_front_node(&node);
        }

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

        iterator begin() noexcept {
            return iterator{front_node()};
        }

        iterator end() noexcept {
            return iterator{};
        }
    };

    template <class T>
    class stack_forward_list<T>::iterator {
    public:
        using list_type = stack_forward_list<T>;
        using node_type = typename list_type::node_type;
        using value_type = typename list_type::value_type;
        using pointer = typename list_type::pointer;
        using const_pointer = typename list_type::const_pointer;
        using reference = typename list_type::reference;
        using const_reference = typename list_type::const_reference;
        using size_type = typename list_type::size_type;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        iterator() = default;

        iterator(node_type* ptr) noexcept : node_ptr(ptr) {
        }

        reference operator*() const noexcept {
            assert(node_ptr != nullptr &&
                   "dereferencing stack_forward_list end "
                   "iterator");
            return *static_cast<pointer>(node_ptr);
        }

        pointer operator->() const noexcept {
            assert(node_ptr != nullptr &&
                   "dereferencing stack_forward_list end "
                   "iterator");
            return static_cast<pointer>(node_ptr);
        }

        iterator& operator++() noexcept {
            assert(node_ptr != nullptr &&
                   "incrementing past stack_forward_list end "
                   "iterator");
            node_ptr = get_next_node(*node_ptr);
            return *this;
        }

        iterator operator++(int) noexcept {
            auto saved_it{*this};
            ++(*this);
            return saved_it;
        }

        bool operator==(const iterator& rhs) const noexcept {
            return node_ptr == rhs.node_ptr;
        }

        bool operator!=(const iterator& rhs) const noexcept {
            return node_ptr != rhs.node_ptr;
        }

    private:
        node_type* node_ptr = nullptr;
    };

} // namespace RAD_LIB_NAMESPACE
