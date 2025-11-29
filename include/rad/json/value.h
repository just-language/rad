#pragma once
#include <rad/json/error.h>
#include <rad/libbase.h>

#include <cstdint>
#include <exception>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace rad::json {
    class value;

    /*!
     * @brief A dynamically sized array of JSON values.
     *
     * This is the type used to represent a JSON array as a modifiable
     * container. The interface and performance characteristics are modeled
     * after std::vector<value>.
     *
     * Elements are stored contiguously, which means that they can be
     * accessed not only through iterators, but also using offsets to
     * regular pointers to elements.
     *
     * A pointer to an element of an array may be passed to any function
     * that expects a pointer to value.
     */
    class array {
        using storage_type = std::vector<value>;

    public:
        /*!
         * @brief The type of each element.
         */
        using value_type = value;
        /*!
         * @brief A random access iterator to an element.
         */
        using iterator = storage_type::iterator;
        /*!
         * @brief A random access const iterator to an element.
         */
        using const_iterator = storage_type::const_iterator;
        /*!
         * @brief A reverse random access iterator to an
         * element.
         */
        using reverse_iterator = storage_type::reverse_iterator;
        /*!
         * @brief A reverse random access const iterator to an
         * element.
         */
        using const_reverse_iterator = storage_type::const_reverse_iterator;
        /*!
         * @brief The type used to represent unsigned integers.
         */
        using size_type = storage_type::size_type;
        /*!
         * @brief The type used to represent signed integers.
         */
        using difference_type = storage_type::difference_type;
        /*!
         * @brief A reference to an element.
         */
        using reference = storage_type::reference;
        /*!
         * @brief A const reference to an element.
         */
        using const_reference = storage_type::const_reference;
        /*!
         * @brief A pointer to an element.
         */
        using pointer = storage_type::pointer;
        /*!
         * @brief A const pointer to an element.
         */
        using const_pointer = storage_type::const_pointer;

        /*!
         * @brief Construct an empty array with zero capacity.
         */
        array() = default;

        /*!
         * @brief Construct an array and fill it with @p count
         * copies of v.
         * @param count The number of copies to insert.
         * @param v The value to be inserted.
         */
        RAD_EXPORT_DECL array(std::size_t count, const value& v);

        /*!
         * @brief Construct an array and fill it with @p count
         * of null values.
         * @param count The number of null values to insert.
         */
        RAD_EXPORT_DECL array(std::size_t count);

        /*!
         * @brief Construct an array and fill it with values in
         * the range [first, last), preserving order.
         * @tparam InputIt Type of input iterator.
         * @param first An input iterator pointing to the first
         * element to insert, or pointing to the end of the
         * range.
         * @param last An input iterator pointing to the end of
         * the range.
         */
        template <class InputIt>
        array(InputIt first, InputIt last);

        /*!
         * @brief Construct an array and fill it with copies of
         * the values in @p init, preserving order.
         * @param init The initializer list with elements to
         * insert.
         */
        RAD_EXPORT_DECL
        array(std::initializer_list<value> init);

        ~array();

        /*!
         * @brief Access an element, with bounds checking.
         * Returns a reference to the element specified at
         * location pos, with bounds checking. If pos is not
         * within the range of the container, an exception of
         * type std::system_error is thrown.
         * @param pos A zero-based index.
         * @return Reference to the value at @p pos.
         */
        RAD_EXPORT_DECL value& at(std::size_t pos) &;

        /*!
         * @brief Access an element, with bounds checking.
         * Returns a reference to the element specified at
         * location pos, with bounds checking. If pos is not
         * within the range of the container, an exception of
         * type std::system_error is thrown.
         * @param pos A zero-based index.
         * @return Reference to the value at @p pos.
         */
        RAD_EXPORT_DECL const value& at(std::size_t pos) const&;

        /*!
         * @brief Access an element, with bounds checking.
         * Returns a reference to the element specified at
         * location pos, with bounds checking. If pos is not
         * within the range of the container, an exception of
         * type std::system_error is thrown.
         * @param pos A zero-based index.
         * @return Reference to the value at @p pos.
         */
        RAD_EXPORT_DECL value&& at(std::size_t pos) &&;

        /*!
         * @brief Access an element.
         * Returns a reference to the element specified at
         * location pos. No bounds checking is performed. If @p
         * pos is not less than the array size the behavior is
         * undefined.
         * @param pos A zero-based index.
         * @return Reference to the value at @p pos.
         */
        RAD_EXPORT_DECL value& operator[](std::size_t pos) &;

        /*!
         * @brief Access an element.
         * Returns a reference to the element specified at
         * location pos. No bounds checking is performed. If @p
         * pos is not less than the array size the behavior is
         * undefined.
         * @param pos A zero-based index.
         * @return Reference to the value at @p pos.
         */
        RAD_EXPORT_DECL const value& operator[](std::size_t pos) const&;

        /*!
         * @brief Access an element.
         * Returns a reference to the element specified at
         * location pos. No bounds checking is performed. If @p
         * pos is not less than the array size the behavior is
         * undefined.
         * @param pos A zero-based index.
         * @return Reference to the value at @p pos.
         */
        RAD_EXPORT_DECL value&& operator[](std::size_t pos) &&;

        /*!
         * @brief Access the first element.
         * If the array is empty the behavior is undefined.
         * @return Reference to the first element.
         */
        RAD_EXPORT_DECL value& front() & noexcept;

        /*!
         * @brief Access the first element.
         * If the array is empty the behavior is undefined.
         * @return Reference to the first element.
         */
        RAD_EXPORT_DECL const value& front() const& noexcept;

        /*!
         * @brief Access the first element.
         * If the array is empty the behavior is undefined.
         * @return Reference to the first element.
         */
        RAD_EXPORT_DECL value&& front() && noexcept;

        /*!
         * @brief Access the last element.
         * If the array is empty the behavior is undefined.
         * @return Reference to the last element.
         */
        RAD_EXPORT_DECL value& back() & noexcept;

        /*!
         * @brief Access the last element.
         * If the array is empty the behavior is undefined.
         * @return Reference to the last element.
         */
        RAD_EXPORT_DECL const value& back() const& noexcept;

        /*!
         * @brief Access the last element.
         * If the array is empty the behavior is undefined.
         * @return Reference to the last element.
         */
        RAD_EXPORT_DECL value&& back() && noexcept;

        /*!
         * @brief Return a pointer to an element if it exists.
         * This function returns a pointer to the element at
         * index @p pos when the index is less then the size of
         * the container. Otherwise it returns null.
         * @param pos A zero-based index.
         * @return Pointer to the element at pos if it exists,
         * otherwise null.
         */
        RAD_EXPORT_DECL value* if_contains(std::size_t pos) noexcept;

        /*!
         * @brief Return a pointer to an element if it exists.
         * This function returns a pointer to the element at
         * index @p pos when the index is less then the size of
         * the container. Otherwise it returns null.
         * @param pos A zero-based index.
         * @return Pointer to the element at pos if it exists,
         * otherwise null.
         */
        RAD_EXPORT_DECL const value*
        if_contains(std::size_t pos) const noexcept;

        /*!
         * @brief Return the number of elements that can be held
         * in currently allocated memory.
         * @return The number of elements that can be held in
         * currently allocated memory.
         */
        std::size_t capacity() const noexcept;

        /*!
         * @brief Return the number of elements in the array.
         * The value returned may be different from the number
         * returned from capacity.
         * @return The number of elements in the array.
         */
        std::size_t size() const noexcept;

        /*!
         * @brief Access the underlying array directly.
         * @return Pointer to the underlying array serving as
         * element storage.
         */
        value* data() noexcept;

        /*!
         * @brief Access the underlying array directly.
         * @return Pointer to the underlying array serving as
         * element storage.
         */
        const value* data() const noexcept;

        /*!
         * @brief Check if the array has no elements.
         * @return True if there are no elements in the array,
         * i.e. size() returns 0
         */
        bool empty() const noexcept {
            return values_.empty();
        }

        /*!
         * @brief Clear the contents.
         * Erases all elements from the container. After this
         * call, size() returns zero but capacity() is
         * unchanged. All references, pointers, and iterators
         * are invalidated.
         */
        RAD_EXPORT_DECL void clear() noexcept;

        /*!
         * @brief Increase the capacity to at least a certain
         * amount. This increases the capacity() to a value that
         * is greater than or equal to
         * @p count. If @p count > capacity(), new memory is
         * allocated. Otherwise, the call has no effect. The
         * number of elements and therefore the size() of the
         * container is not changed. If new memory is allocated,
         * all iterators including any past-the-end iterators,
         * and all references to the elements are invalidated.
         * Otherwise, no iterators or references are
         * invalidated.
         * @param count The new capacity of the array.
         */
        RAD_EXPORT_DECL void reserve(std::size_t count);

        /*!
         * @brief Change the number of elements stored.
         * Resizes the container to contain count elements.
         * - If size() > @p count, the container is reduced to
         * its first count elements.
         * - If size() < @p count, additional null values are
         * appended. If capacity() < @p count, a reallocation
         * occurs first, and all iterators and references are
         * invalidated. Any past-the-end iterators are always
         * invalidated
         * @param count The new size of the container.
         */
        RAD_EXPORT_DECL void resize(std::size_t count);

        /*!
         * @brief Change the number of elements stored.
         * Resizes the container to contain count elements.
         * - If size() > @p count, the container is reduced to
         * its first count elements.
         * - If size() < @p count, additional copies of @p v are
         * appended. If capacity() < @p count, a reallocation
         * occurs first, and all iterators and references are
         * invalidated. Any past-the-end iterators are always
         * invalidated
         * @param count The new size of the container.
         * @param v The value to copy into the new elements.
         */
        RAD_EXPORT_DECL void resize(std::size_t count, const value& v);

        /*!
         * @brief Request the removal of unused capacity.
         * This performs a non-binding request to reduce the
         * capacity to the current size. The request may or may
         * not be fulfilled. If reallocation occurs, all
         * iterators including any past-the-end iterators, and
         * all references to the elements are invalidated.
         * Otherwise, no iterators or references are
         * invalidated.
         */
        RAD_EXPORT_DECL void shrink_to_fit();

        /*!
         * @brief Remove element from the array. The element at
         * pos is removed. If @p pos does not point to a valid
         * element in this array the behavior is undefined.
         * @param pos Iterator to the element to remove.
         * @return Iterator following the last removed element.
         * If that was the last element of the array, the end()
         * iterator is returned.
         * @return Iterator following the last removed element.
         * If that was the last element of the array, the end()
         * iterator is returned.
         */
        RAD_EXPORT_DECL iterator erase(const_iterator pos) noexcept;

        /*!
         * @brief Remove elements from the array. The elements
         * in the range [first, last) are removed. If @p first
         * does not point to a valid element in this array and
         * is not the end iterator the behavior is undefined. If
         * @p last is not the end iterator and is not an
         * iterator equal to or past @p first the behavior is
         * undefined.
         * @param first An iterator pointing to the first
         * element to erase, or pointing to the end of the
         * range.
         * @param last An iterator pointing to one past the last
         * element to erase, or pointing to the end of the
         * range.
         * @return Iterator following the last removed element.
         * If that was the last element of the array, the end()
         * iterator is returned.
         */
        RAD_EXPORT_DECL iterator erase(const_iterator first,
                                       const_iterator last) noexcept;

        /*!
         * @brief Add an element to the end.
         * @param v The value to insert.
         */
        RAD_EXPORT_DECL void push_back(const value& v);

        /*!
         * @brief Add an element to the end.
         * @param v The value to insert.
         */
        RAD_EXPORT_DECL void push_back(value&& v);

        /*!
         * @brief Remove the last element.
         */
        RAD_EXPORT_DECL void pop_back() noexcept;

        /*!
         * @brief Append a constructed element in-place.
         * @tparam ...Args The arguments types.
         * @param ...args The arguments to forward to the value
         * constructor.
         * @return A reference to the inserted element.
         */
        template <class... Args>
        value& emplace_back(Args&&... args);

        /*!
         * @brief Insert a constructed element in-place.
         * @tparam ...Args The arguments types.
         * @param pos Iterator before which the element will be
         * inserted. This may be the end() iterator.
         * @param ...args The arguments to forward to the value
         * constructor.
         * @return An iterator to the inserted element.
         */
        template <class... Args>
        iterator emplace(const_iterator pos, Args&&... args);

        /*!
         * @brief Insert element before the specified location.
         * @param pos Iterator before which the new element will
         * be inserted. This may be the end() iterator.
         * @param v The value to insert.
         * @return An iterator to the inserted element.
         */
        RAD_EXPORT_DECL iterator insert(const_iterator pos, const value& v);

        /*!
         * @brief Insert element before the specified location.
         * @param pos Iterator before which the new element will
         * be inserted. This may be the end() iterator.
         * @param v The value to insert.
         * @return An iterator to the inserted element.
         */
        RAD_EXPORT_DECL iterator insert(const_iterator pos, value&& v);

        /*!
         * @brief Insert elements before the specified location.
         * @param pos Iterator before which the new elements
         * will be inserted. This may be the end() iterator.
         * @param n The count of copies to insert.
         * @param v The value to insert copies from.
         * @return An iterator to the first inserted value, or
         * pos if no values were inserted.
         */
        RAD_EXPORT_DECL iterator insert(const_iterator pos, std::size_t count,
                                        const value& v);

        /*!
         * @brief Insert elements before the specified location.
         * @tparam InputIt Type of input iterator.
         * @param pos Iterator before which the new elements
         * will be inserted. This may be the end() iterator.
         * @param first An input iterator pointing to the first
         * element to insert, or pointing to the end of the
         * range to insert.
         * @param last An input iterator pointing to the end of
         * the range to insert.
         * @return An iterator to the first inserted value, or
         * pos if no values were inserted.
         */
        template <class InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last);

        /*!
         * @brief Insert elements before the specified location.
         * @param pos Iterator before which the new elements
         * will be inserted. This may be the end() iterator.
         * @param values The initializer list to insert.
         * @return An iterator to the first inserted value, or
         * pos if no values were inserted.
         */
        RAD_EXPORT_DECL iterator insert(const_iterator pos,
                                        std::initializer_list<value> values);

        /*!
         * @brief Return an iterator to the first element.
         * If the container is empty, end() is returned.
         * @return Iterator to the first element.
         */
        iterator begin();

        /*!
         * @brief Return a iterator past the last element.
         * The returned iterator only acts as a sentinel.
         * Dereferencing it results in undefined behavior.
         * @return Iterator past the last element.
         */
        iterator end();

        /*!
         * @brief Return a const iterator to the first element.
         * If the container is empty, end() is returned.
         * @return Iterator to the first element.
         */
        const_iterator begin() const;

        /*!
         * @brief Return a const iterator past the last element.
         * The returned iterator only acts as a sentinel.
         * Dereferencing it results in undefined behavior.
         * @return Iterator past the last element.
         */
        const_iterator end() const;

        /*!
         * @brief Return a const iterator to the first element.
         * If the container is empty, cend() is returned.
         * @return Iterator to the first element.
         */
        const_iterator cbegin() const;

        /*!
         * @brief Return a const iterator past the last element.
         * The returned iterator only acts as a sentinel.
         * Dereferencing it results in undefined behavior.
         * @return Iterator past the last element.
         */
        const_iterator cend() const;

        /*!
         * @brief Return a reverse iterator to the first element
         * of the reversed container. The pointed-to element
         * corresponds to the last element of the non-reversed
         * container. If the container is empty, rend() is
         * returned.
         * @return Reverse iterator to the first element of the
         * reversed container.
         */
        reverse_iterator rbegin();

        /*!
         * @brief Return a reverse iterator to the element
         * following the last element of the reversed container.
         * The pointed-to element corresponds to the element
         * preceding the first element of the non-reversed
         * container. The returned iterator only acts as a
         * sentinel. Dereferencing it results in undefined
         * behavior.
         * @return Reverse iterator to the element following the
         * last element of the reversed container.
         */
        reverse_iterator rend();

        /*!
         * @brief Return a reverse iterator to the first element
         * of the reversed container. The pointed-to element
         * corresponds to the last element of the non-reversed
         * container. If the container is empty, rend() is
         * returned.
         * @return Reverse iterator to the first element of the
         * reversed container.
         */
        const_reverse_iterator rbegin() const;

        /*!
         * @brief Return a reverse iterator to the element
         * following the last element of the reversed container.
         * The pointed-to element corresponds to the element
         * preceding the first element of the non-reversed
         * container. The returned iterator only acts as a
         * sentinel. Dereferencing it results in undefined
         * behavior.
         * @return Reverse iterator to the element following the
         * last element of the reversed container.
         */
        const_reverse_iterator rend() const;

        /*!
         * @brief Return a const reverse iterator to the first
         * element of the reversed container. The pointed-to
         * element corresponds to the last element of the
         * non-reversed container. If the container is empty,
         * rend() is returned.
         * @return Reverse iterator to the first element of the
         * reversed container.
         */
        const_reverse_iterator crbegin() const;

        /*!
         * @brief Return a const reverse iterator to the element
         * following the last element of the reversed container.
         * The pointed-to element corresponds to the element
         * preceding the first element of the non-reversed
         * container. The returned iterator only acts as a
         * sentinel. Dereferencing it results in undefined
         * behavior.
         * @return Reverse iterator to the element following the
         * last element of the reversed container.
         */
        const_reverse_iterator crend() const;

        /*!
         * @brief Swap two arrays.
         * Exchanges the contents of this array with another
         * array. If this == &other, this function call has no
         * effect.
         * @param other The value to swap with.
         */
        void swap(array& other) noexcept {
            values_.swap(other.values_);
        }

        /*!
         * @brief Swap two arrays.
         * Exchanges the contents of the array lhs with another
         * array rhs. If &lhs == &rhs, this function call has no
         * effect.
         * @param lhs The array to exchange.
         * @param rhs The array to exchange.
         */
        friend void swap(array& lhs, array& rhs) noexcept {
            lhs.swap(rhs);
        }

        /*!
         * @brief Compare two arrays for equality.
         * @param lhs The first array to compare for equality.
         * @param rhs The second array to compare for equality.
         * @return True if the two arrays are equal, otherwise
         * false.
         */
        friend bool operator==(const array& lhs,
                               const array& rhs) noexcept = default;

        /*!
         * @brief Serialize to an output stream.
         * @param os The output stream to serialize to.
         * @param arr The value to serialize.
         * @return Reference to os.
         */
        friend std::ostream& operator<<(std::ostream& os, const array& arr) {
            return arr.serialize_to_ostream(os);
        }

    private:
        RAD_EXPORT_DECL std::ostream&
        serialize_to_ostream(std::ostream& os) const;

        std::vector<value> values_;
    };

    class key_value_pair;

    /*!
     * @brief A dynamically sized associative container of JSON key/value
     * pairs.
     *
     * This is an associative container whose elements are key/value pairs
     * with unique keys.
     *
     * The elements are stored contiguously; iterators are ordinary
     * pointers, allowing random access pointer arithmetic for retrieving
     * elements.
     */
    class object {
        using storage_type = std::vector<key_value_pair>;

    public:
        /// The type of keys.
        using key_type = std::string_view;
        /// The type of mapped values.
        using mapped_type = value;
        /// The element type.
        using value_type = key_value_pair;
        /// A random access iterator to an element.
        using iterator = storage_type::iterator;
        /// A const random access iterator to an element.
        using const_iterator = storage_type::const_iterator;
        /// A reverse random access iterator to an element.
        using reverse_iterator = storage_type::reverse_iterator;
        /// A const reverse random access iterator to an
        /// element.
        using const_reverse_iterator = storage_type::const_reverse_iterator;
        /// The type used to represent unsigned integers.
        using size_type = storage_type::size_type;
        /// The type used to represent signed integers.
        using difference_type = storage_type::difference_type;

        /*!
         * @brief Construct an empty object.
         */
        object() noexcept;

        /*!
         * @brief Construct an empty object and reserve memory
         * for @p min_capacity elements.
         * @param min_capacity 	The minimum number of elements
         * for which capacity is guaranteed without a subsequent
         * reallocation.
         */
        RAD_EXPORT_DECL object(std::size_t min_capacity);

        /*!
         * @brief Construct an object and, reserve memory for @p
         * min_capacity or init.size() elements, whichever is
         * greater, and fill it with copies of values in @p
         * init.
         * @param init The initializer list to insert.
         * @param min_capacity The minimum number of elements
         * for which capacity is guaranteed without a subsequent
         * reallocation.
         */
        RAD_EXPORT_DECL
        object(std::initializer_list<std::pair<std::string_view, value>> init,
               std::size_t min_capacity = 0);

        ~object();

        /*!
         * @brief Find an element with a specific key.
         *
         * This function returns an iterator to the element
         * matching key if it exists, otherwise returns end().
         * @param key The key of the element to find.
         * @return Iterator to the element matching key if it
         * exists, otherwise returns end().
         */
        RAD_EXPORT_DECL iterator find(std::string_view key) noexcept;

        /*!
         * @brief Find an element with a specific key.
         *
         * This function returns an iterator to the element
         * matching key if it exists, otherwise returns end().
         * @param key The key of the element to find.
         * @return Iterator to the element matching key if it
         * exists, otherwise returns end().
         */
        RAD_EXPORT_DECL const_iterator
        find(std::string_view key) const noexcept;

        /*!
         * @brief Access the specified element, with bounds
         * checking.
         *
         * Returns a reference to the mapped value of the
         * element that matches key, otherwise throws.
         * @param key The key of the element to find.
         * @return A reference to the mapped value.
         */
        RAD_EXPORT_DECL value& at(std::string_view key) &;

        /*!
         * @brief Access the specified element, with bounds
         * checking.
         *
         * Returns a reference to the mapped value of the
         * element that matches key, otherwise throws.
         * @param key The key of the element to find.
         * @return A reference to the mapped value.
         */
        RAD_EXPORT_DECL const value& at(std::string_view key) const&;

        /*!
         * @brief Access the specified element, with bounds
         * checking.
         *
         * Returns a reference to the mapped value of the
         * element that matches key, otherwise throws.
         * @param key The key of the element to find.
         * @return A reference to the mapped value.
         */
        RAD_EXPORT_DECL value&& at(std::string_view key) &&;

        /*!
         * @brief Return an iterator to the first element.
         *
         * If the container is empty, end() is returned.
         * @return Iterator to the first element, or end() if
         * the object is empty.
         */
        iterator begin();

        /*!
         * @brief Return an iterator to the element following
         * the last element.
         *
         * The element acts as a placeholder; attempting to
         * access it results in undefined behavior.
         * @return Iterator to the element following the last
         * element.
         */
        iterator end();

        /*!
         * @brief Return a const iterator to the first element.
         *
         * If the container is empty, end() is returned.
         * @return Iterator to the first element, or end() if
         * the object is empty.
         */
        const_iterator begin() const;

        /*!
         * @brief Return an iterator to the element following
         * the last element.
         *
         * The element acts as a placeholder; attempting to
         * access it results in undefined behavior.
         * @return Iterator to the element following the last
         * element.
         */
        const_iterator end() const;

        /*!
         * @brief Return a const iterator to the first element.
         *
         * If the container is empty, end() is returned.
         * @return Iterator to the first element, or end() if
         * the object is empty.
         */
        const_iterator cbegin() const;

        /*!
         * @brief Return an iterator to the element following
         * the last element.
         *
         * The element acts as a placeholder; attempting to
         * access it results in undefined behavior.
         * @return Iterator to the element following the last
         * element.
         */
        const_iterator cend() const;

        /*!
         * @brief Return a reverse iterator to the first element
         * of the reversed container.
         *
         * The pointed-to element corresponds to the last
         * element of the non-reversed container. If the
         * container is empty, rend() is returned.
         * @return Reverse iterator to the first element of the
         * reversed container.
         */
        reverse_iterator rbegin();

        /*!
         * @brief Return a reverse iterator to the element
         * following the last element of the reversed container.
         *
         * The pointed-to element corresponds to the element
         * preceding the first element of the non-reversed
         * container. The returned iterator only acts as a
         * sentinel. Dereferencing it results in undefined
         * behavior.
         * @return Reverse iterator to the element following the
         * last element of the reversed container.
         */
        reverse_iterator rend();

        /*!
         * @brief Return a const reverse iterator to the first
         * element of the reversed container.
         *
         * The pointed-to element corresponds to the last
         * element of the non-reversed container. If the
         * container is empty, rend() is returned.
         * @return Reverse iterator to the first element of the
         * reversed container.
         */
        const_reverse_iterator rbegin() const;

        /*!
         * @brief Return a const reverse iterator to the element
         * following the last element of the reversed container.
         *
         * The pointed-to element corresponds to the element
         * preceding the first element of the non-reversed
         * container. The returned iterator only acts as a
         * sentinel. Dereferencing it results in undefined
         * behavior.
         * @return Reverse iterator to the element following the
         * last element of the reversed container.
         */
        const_reverse_iterator rend() const;

        /*!
         * @brief Return a const reverse iterator to the first
         * element of the reversed container.
         *
         * The pointed-to element corresponds to the last
         * element of the non-reversed container. If the
         * container is empty, rend() is returned.
         * @return Reverse iterator to the first element of the
         * reversed container.
         */
        const_reverse_iterator crbegin() const;

        /*!
         * @brief Return a const reverse iterator to the element
         * following the last element of the reversed container.
         *
         * The pointed-to element corresponds to the element
         * preceding the first element of the non-reversed
         * container. The returned iterator only acts as a
         * sentinel. Dereferencing it results in undefined
         * behavior.
         * @return Reverse iterator to the element following the
         * last element of the reversed container.
         */
        const_reverse_iterator crend() const;

        /*!
         * @brief Return the number of elements that can be held
         * in currently allocated memory.
         *
         * Returns the number of elements that the container has
         * currently allocated space for. This number is never
         * smaller than the value returned by size().
         * @return Number of elements that can be held in
         * currently allocated memory.
         */
        std::size_t capacity() const noexcept;

        /*!
         * @brief Return the number of elements.
         * @return Number of elements in the container.
         */
        std::size_t size() const noexcept;

        /*!
         * @brief Erase all elements.
         *
         * Erases all elements from the container. After this
         * call, size() returns zero but capacity() is
         * unchanged. All references, pointers, and iterators
         * are invalidated.
         */
        RAD_EXPORT_DECL void clear() noexcept;

        /*!
         * @brief Checks if there is an element with key equal
         * to key.
         * @param key The key of the element to find.
         * @return True if the key is found, otherwise false.
         */
        bool contains(std::string_view key) const noexcept {
            return find(key) != end();
        }

        /*!
         * @brief Count the number of elements with a specific
         * key.
         * @param key The key of the element to find.
         * @return Number of elements with keys equal to key.
         * The only possible return values are 0 and 1.
         */
        std::size_t count(std::string_view key) const noexcept {
            return static_cast<std::size_t>(contains(key));
        }

        /*!
         * @brief Construct an element in-place.
         *
         * Inserts a new element into the container constructed
         * in-place with the given key and given arguments for
         * value constructor, if there is no element with the
         * key in the container.
         * @tparam ...Args Type of arguments.
         * @param key The key of the element to insert.
         * @param ...arg Arguments to value constructor.
         * @return A std::pair where first is an iterator to the
         * existing or inserted element, and second is true if
         * the insertion took place or false otherwise.
         */
        template <class... Args>
        std::pair<iterator, bool> emplace(std::string_view key, Args&&... arg);

        /*!
         * @brief Return whether there are no elements.
         * @return True if there are no elements in the
         * container, i.e. size() returns 0.
         */
        bool empty() const noexcept {
            return keys_values_.empty();
        }

        /*!
         * @brief Remove an element at @p pos.
         *
         * @p pos must be valid and dereferenceable and can't be
         * the end iterator.
         * @param pos An iterator pointing to the element to be
         * removed.
         * @return Iterator following the removed element.
         */
        RAD_EXPORT_DECL iterator erase(const_iterator pos) noexcept;

        /*!
         * @brief Remove an element whose key is equal to @p
         * key.
         * @param key The key to match.
         * @return Number of elements removed, which will be
         * either 0 or 1.
         */
        RAD_EXPORT_DECL std::size_t erase(std::string_view key) noexcept;

        /*!
         * @brief Erase an element preserving order.
         *
         * Remove the element which matches key, if it exists.
         * All references and iterators are invalidated.
         * @param key The key to match.
         * @return The number of elements removed, which will be
         * either 0 or 1.
         */
        RAD_EXPORT_DECL std::size_t stable_erase(std::string_view key) noexcept;

        /*!
         * @brief Erase an element preserving order.
         *
         * Remove the element pointed to by pos, which must be
         * valid and dereferenceable. References and iterators
         * from pos to end(), both included, are invalidated.
         * Other iterators and references are not invalidated.
         * @param pos An iterator pointing to the element to be
         * removed.
         * @return An iterator following the removed element.
         */
        RAD_EXPORT_DECL iterator stable_erase(const_iterator pos) noexcept;

        /*!
         * @brief Return a pointer to the value if the key is
         * found, or null.
         * @param key The key of the element to find.
         * @return Pointer to the value if the key is found, or
         * null.
         */
        RAD_EXPORT_DECL value* if_contains(std::string_view key) noexcept;

        /*!
         * @brief Return a pointer to the value if the key is
         * found, or null.
         * @param key The key of the element to find.
         * @return Pointer to the value if the key is found, or
         * null.
         */
        RAD_EXPORT_DECL const value*
        if_contains(std::string_view key) const noexcept;

        /*!
         * @brief Inserts a new element constructed as if via
         * value_type( std::forward<P>(p) ). If an element with
         * the same key already exists no insertion will occur.
         * @tparam P Type of pair argument
         * @param p The value to insert.
         * @return A std::pair where first is an iterator to the
         * existing or inserted element, and second is true if
         * the insertion took place or false otherwise.
         */
        template <class P>
        std::pair<iterator, bool> insert(P&& p);

        /*!
         * @brief Insert the elements in the range [first, last)
         * one at a time, in order.
         *
         * Any element with key that is a duplicate of a key
         * already present in container will be skipped. This
         * also means, that if there are two keys within the
         * inserted range that are equal to each other, only the
         * first will be inserted.
         * @tparam InputIt Type of input iterator.
         * @param first An input iterator pointing to the first
         * element to insert, or pointing to the end of the
         * range.
         * @param last An input iterator pointing to the end of
         * the range.
         */
        template <class InputIt>
        void insert(InputIt first, InputIt last);

        /*!
         * @brief Insert the elements in the initializer list
         * one at a time, in order.
         *
         * Any element with key that is a duplicate of a key
         * already present in container will be skipped. This
         * also means, that if there are two keys within the
         * inserted range that are equal to each other, only the
         * first will be inserted.
         * @param init The initializer list to insert.
         */
        RAD_EXPORT_DECL void
        insert(std::initializer_list<std::pair<std::string_view, value>> init);

        /*!
         * @brief Insert an element or assign to an existing
         * element.
         * @tparam ...Args Type of arguments.
         * @param key The key used for lookup and insertion.
         * @param ...args The arguments passed to the value
         * constructor.
         * @return A std::pair where first is an iterator to the
         * existing or inserted element, and second is true if
         * the insertion took place or false if the assignment
         * took place.
         */
        template <class... Args>
        std::pair<iterator, bool> insert_or_assign(std::string_view key,
                                                   Args&&... args);

        /*!
         * @brief Access or insert an element.
         *
         * Returns a reference to the value that is mapped to
         * key. If such value does not already exist, performs
         * an insertion of a null value.
         * @param key The key of the element to find.
         * @return A reference to the mapped value.
         */
        RAD_EXPORT_DECL value& operator[](std::string_view key);

        /*!
         * @brief Increase the capacity to at least a certain
         * amount.
         *
         * This increases the capacity() to a value that is
         * greater than or equal to
         * @p new_capacity. If @p new_capacity > capacity(), new
         * memory is allocated. Otherwise, the call has no
         * effect. The number of elements and therefore the
         * size() of the container is not changed.
         *
         * If new memory is allocated, all iterators including
         * any past-the-end iterators, and all references to the
         * elements are invalidated. Otherwise, no iterators or
         * references are invalidated.
         * @param new_capacity The new minimum capacity.
         */
        RAD_EXPORT_DECL void reserve(std::size_t new_capacity);

        /*!
         * @brief Swap two objects.
         *
         * Exchanges the contents of this object with another
         * object.
         * @param other The object to swap with.
         */
        void swap(object& other) noexcept {
            keys_values_.swap(other.keys_values_);
        }

        /*!
         * @brief Swap two objects.
         *
         * Exchanges the contents of the object lhs with another
         * object rhs.
         * @param lhs The object to exchange.
         * @param rhs The object to exchange.
         */
        friend void swap(object& lhs, object& rhs) noexcept {
            lhs.swap(rhs);
        }

        /*!
         * @brief Compare two objects for equality.
         *
         * Objects are equal when their sizes are the same,
         * and when for each key in lhs there is a matching key
         * in rhs with the same value.
         * @param lhs The object to compare.
         * @param rhs The object to compare.
         * @return True if the two objects are equal, otherwise
         * false.
         */
        friend bool operator==(const object& lhs, const object& rhs) noexcept {
            return lhs.equal(rhs);
        }

        /*!
         * @brief Serialize to an output stream.
         * @param os The output stream to serialize to.
         * @param obj The value to serialize.
         * @return Reference to os.
         */
        friend std::ostream& operator<<(std::ostream& os, const object& obj) {
            return obj.serialize_to_ostream(os);
        }

    private:
        // objects are not ordered
        RAD_EXPORT_DECL bool equal(const object& other) const noexcept;

        RAD_EXPORT_DECL std::ostream&
        serialize_to_ostream(std::ostream& os) const;

        storage_type keys_values_;
    };

    /*!
     * @brief A tag type used to select the value constructor that creates
     * an empty string value.
     */
    struct string_kind_t {};
    /*!
     * @brief A tag type used to select the value constructor that creates
     * an empty array value.
     */
    struct array_kind_t {};
    /*!
     * @brief A tag type used to select the value constructor that creates
     * an empty object value.
     */
    struct object_kind_t {};

    /*!
     * @brief A constant used to select the value constructor that creates
     * an empty string value.
     */
    inline constexpr string_kind_t string_kind;
    /*!
     * @brief A constant used to select the value constructor that creates
     * an empty array value.
     */
    inline constexpr array_kind_t array_kind;
    /*!
     * @brief A constant used to select the value constructor that creates
     * an empty object value.
     */
    inline constexpr object_kind_t object_kind;

    /*!
     * @brief The type used to represent any JSON value.
     *
     * This is a Regular type which works like a variant of the basic JSON
     * data types: array, object, string, number, boolean, and null.
     */
    class value {
    public:
        /*!
         * @brief Construct a null value. Same as
         * value(nullptr).
         */
        value() = default;

        /*!
         * @brief Construct a null value. Same as the default
         * constructor.
         */
        value(std::nullptr_t) noexcept;

        /*!
         * @brief Construct a boolean value where the stored
         * bool is equal to @p b.
         * @param b The boolean to construct with.
         */
        value(bool b) noexcept;

        /*!
         * @brief Construct a signed integer value where the
         * stored number is equal to
         * @p i.
         * @param i The number to construct with.
         */
        value(std::int64_t i) noexcept;

        /*!
         * @brief Construct an unsigned integer value where the
         * stored number is equal to @p u.
         * @param u The number to construct with.
         */
        value(std::uint64_t u) noexcept;

        /*!
         * @brief Construct a signed integer value where the
         * stored number is equal to
         * @p i.
         * @param i The number to construct with.
         */
        value(std::int8_t i) noexcept;

        /*!
         * @brief Construct a signed integer value where the
         * stored number is equal to
         * @p i.
         * @param i The number to construct with.
         */
        value(std::int16_t i) noexcept;

        /*!
         * @brief Construct a signed integer value where the
         * stored number is equal to
         * @p i.
         * @param i The number to construct with.
         */
        value(std::int32_t i) noexcept;

        /*!
         * @brief Construct an unsigned integer value where the
         * stored number is equal to @p u.
         * @param u The number to construct with.
         */
        value(std::uint8_t i) noexcept;

        /*!
         * @brief Construct an unsigned integer value where the
         * stored number is equal to @p u.
         * @param u The number to construct with.
         */
        value(std::uint16_t i) noexcept;

        /*!
         * @brief Construct an unsigned integer value where the
         * stored number is equal to @p u.
         * @param u The number to construct with.
         */
        value(std::uint32_t i) noexcept;

        /*!
         * @brief Construct a floating point value where the
         * stored number is equal to
         * @p d.
         * @param d The number to construct with.
         */
        value(double d) noexcept;

        /*!
         * @brief Construct an empty string value.
         */
        value(string_kind_t) noexcept;

        /*!
         * @brief Construct a string value by copying @p s.
         * @param s The string to construct with.
         */
        value(std::string_view s);

        /*!
         * @brief Construct a string value by copying @p s.
         * @param s The string to construct with.
         */
        value(const std::string& s) noexcept;

        /*!
         * @brief Construct a string value by moving @p s.
         * @param s The string to construct with.
         */
        value(std::string&& s) noexcept;

        /*!
         * @brief Construct a string value by copying null
         * terminated string @p s.
         * @param s The null terminated string to construct
         * with.
         */
        value(const char* s);

        /*!
         * @brief Construct a string value by copying @p s up to
         * @p n bytes.
         * @param s The string to construct with, does not need
         * to be null terminated.
         * @param n The count of bytes to copy from @p s.
         */
        value(const char* s, std::size_t n);

        /*!
         * @brief Construct a string value by copying null
         * terminated utf-8 string @p s.
         * @param s The null terminated utf-8 string to
         * construct with.
         */
        value(const char8_t* s);

        /*!
         * @brief Construct a string value by copying @p s up to
         * @p n bytes.
         * @param s The utf-8 string to construct with, does not
         * need to be null terminated.
         * @param n The count of bytes to copy from @p s.
         */
        value(const char8_t* s, std::size_t n);

        /*!
         * @brief Construct an empty array value.
         */
        value(array_kind_t) noexcept;

        /*!
         * @brief Construct an array value by copying @p a.
         * @param a The array to construct with.
         */
        value(const array& a);

        /*!
         * @brief Construct an array value by moving @p a.
         * @param a The array to construct with.
         */
        value(array&& a) noexcept;

        /*!
         * @brief Construct an empty object value.
         */
        value(object_kind_t) noexcept;

        /*!
         * @brief Construct an object value by copying @p o.
         * @param o The object to construct with.
         */
        value(const object& o);

        /*!
         * @brief Construct an object value by moving @p o.
         * @param o The object to construct with.
         */
        value(object&& o) noexcept;

        ~value();

        /*!
         * @brief Check if this is a null value.
         * @return True if the value is null, otherwise false.
         */
        constexpr bool is_null() const noexcept {
            return std::holds_alternative<std::nullptr_t>(storage_);
        }

        /*!
         * @brief Check if this is a bool value.
         * @return True if the value is bool, otherwise false.
         */
        constexpr bool is_bool() const noexcept {
            return std::holds_alternative<bool>(storage_);
        }

        /*!
         * @brief Check if this is a signed integer value.
         * @return True if the value is signed integer,
         * otherwise false.
         */
        constexpr bool is_int64() const noexcept {
            return std::holds_alternative<std::int64_t>(storage_);
        }

        /*!
         * @brief Check if this is an unsigned integer value.
         * @return True if the value is unsigned integer,
         * otherwise false.
         */
        constexpr bool is_uint64() const noexcept {
            return std::holds_alternative<std::uint64_t>(storage_);
        }

        /*!
         * @brief Check if this is a floating point value.
         * @return True if the value is floating point,
         * otherwise false.
         */
        constexpr bool is_double() const noexcept {
            return std::holds_alternative<double>(storage_);
        }

        /*!
         * @brief Check if this is a number.
         * @return True if the value is an integer (signed or
         * unsigned) or a floating point.
         */
        constexpr bool is_number() const noexcept {
            return is_int64() || is_uint64() || is_double();
        }

        /*!
         * @brief Check if this is a string value.
         * @return True if the value is string, otherwise false.
         */
        constexpr bool is_string() const noexcept {
            return std::holds_alternative<std::string>(storage_);
        }

        /*!
         * @brief Check if this is an object value.
         * @return True if the value is object, otherwise false.
         */
        constexpr bool is_object() const noexcept {
            return std::holds_alternative<object>(storage_);
        }

        /*!
         * @brief Check if this is an array value.
         * @return True if the value is array, otherwise false.
         */
        constexpr bool is_array() const noexcept {
            return std::holds_alternative<array>(storage_);
        }

        /*!
         * @brief Check if this is an object or array value.
         * @return True if the value is object or array,
         * otherwise false.
         */
        constexpr bool is_structured() const noexcept {
            return is_array() || is_object();
        }

        /*!
         * @brief Check if this is not an object or array value.
         * @return True if the value is not object or array,
         * otherwise false.
         */
        constexpr bool is_primitive() const noexcept {
            return !is_structured();
        }

        /*!
         * @brief Return a pointer to the underlying bool.
         * @return Pointer to the underlying bool if this is a
         * bool value, otherwise nullptr.
         */
        bool* if_bool() noexcept {
            return std::get_if<bool>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying bool.
         * @return Pointer to the underlying bool if this is a
         * bool value, otherwise nullptr.
         */
        const bool* if_bool() const noexcept {
            return std::get_if<bool>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying
         * std::int64_t.
         * @return Pointer to the underlying std::int64_t if
         * this is a signed number value, otherwise nullptr.
         */
        std::int64_t* if_int64() noexcept {
            return std::get_if<std::int64_t>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying
         * std::int64_t.
         * @return Pointer to the underlying std::int64_t if
         * this is a signed number value, otherwise nullptr.
         */
        const std::int64_t* if_int64() const noexcept {
            return std::get_if<std::int64_t>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying
         * std::uint64_t.
         * @return Pointer to the underlying std::uint64_t if
         * this is an usigned number value, otherwise nullptr.
         */
        std::uint64_t* if_uint64() noexcept {
            return std::get_if<std::uint64_t>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying
         * std::uint64_t.
         * @return Pointer to the underlying std::uint64_t if
         * this is an usigned number value, otherwise nullptr.
         */
        const std::uint64_t* if_uint64() const noexcept {
            return std::get_if<std::uint64_t>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying double.
         * @return Pointer to the underlying double if this is a
         * floating point value, otherwise nullptr.
         */
        double* if_double() noexcept {
            return std::get_if<double>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying double.
         * @return Pointer to the underlying double if this is a
         * floating point value, otherwise nullptr.
         */
        const double* if_double() const noexcept {
            return std::get_if<double>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying
         * std::string.
         * @return Pointer to the underlying std::string if this
         * is a string value, otherwise nullptr.
         */
        std::string* if_string() noexcept {
            return std::get_if<std::string>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying
         * std::string.
         * @return Pointer to the underlying std::string if this
         * is a string value, otherwise nullptr.
         */
        const std::string* if_string() const noexcept {
            return std::get_if<std::string>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying object.
         * @return Pointer to the underlying object if this is
         * an object value, otherwise nullptr.
         */
        object* if_object() noexcept {
            return std::get_if<object>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying object.
         * @return Pointer to the underlying object if this is
         * an object value, otherwise nullptr.
         */
        const object* if_object() const noexcept {
            return std::get_if<object>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying array.
         * @return Pointer to the underlying array if this is an
         * array value, otherwise nullptr.
         */
        array* if_array() noexcept {
            return std::get_if<array>(&storage_);
        }

        /*!
         * @brief Return a pointer to the underlying array.
         * @return Pointer to the underlying array if this is an
         * array value, otherwise nullptr.
         */
        const array* if_array() const noexcept {
            return std::get_if<array>(&storage_);
        }

        /*!
         * @brief Return the underlying bool, without checking.
         * If this is not a bool value, the behavior is
         * undefined.
         * @return Reference to the underlying bool.
         */
        bool& get_bool() noexcept {
            return *std::get_if<bool>(&storage_);
        }

        /*!
         * @brief Return the underlying bool, without checking.
         * If this is not a bool value, the behavior is
         * undefined.
         * @return Copy of the underlying bool.
         */
        bool get_bool() const noexcept {
            return *std::get_if<bool>(&storage_);
        }

        /*!
         * @brief Return the underlying std::int64_t, without
         * checking. If this is not a signed integer value, the
         * behavior is undefined.
         * @return Reference to the underlying std::int64_t.
         */
        std::int64_t& get_int64() noexcept {
            return *std::get_if<std::int64_t>(&storage_);
        }

        /*!
         * @brief Return the underlying std::int64_t, without
         * checking. If this is not a signed integer value, the
         * behavior is undefined.
         * @return Copy of the underlying std::int64_t.
         */
        std::int64_t get_int64() const noexcept {
            return *std::get_if<std::int64_t>(&storage_);
        }

        /*!
         * @brief Return the underlying std::uint64_t, without
         * checking. If this is not an unsigned integer value,
         * the behavior is undefined.
         * @return Reference to the underlying std::uint64_t.
         */
        std::uint64_t& get_uint64() noexcept {
            return *std::get_if<std::uint64_t>(&storage_);
        }

        /*!
         * @brief Return the underlying std::uint64_t, without
         * checking. If this is not an unsigned integer value,
         * the behavior is undefined.
         * @return Copy of the underlying std::uint64_t.
         */
        std::uint64_t get_uint64() const noexcept {
            return *std::get_if<std::uint64_t>(&storage_);
        }

        /*!
         * @brief Return the underlying double, without
         * checking. If this is not a floating point value, the
         * behavior is undefined.
         * @return Reference to the underlying double.
         */
        double& get_double() noexcept {
            return *std::get_if<double>(&storage_);
        }

        /*!
         * @brief Return the underlying double, without
         * checking. If this is not a floating point value, the
         * behavior is undefined.
         * @return Copy of the underlying double.
         */
        double get_double() const noexcept {
            return *std::get_if<double>(&storage_);
        }

        /*!
         * @brief Return the underlying std::string, without
         * checking. If this is not a string value, the behavior
         * is undefined.
         * @return Reference to the underlying std::string.
         */
        std::string& get_string() & noexcept {
            return *std::get_if<std::string>(&storage_);
        }

        /*!
         * @brief Return the underlying std::string, without
         * checking. If this is not a string value, the behavior
         * is undefined.
         * @return Reference to the underlying std::string.
         */
        const std::string& get_string() const& noexcept {
            return *std::get_if<std::string>(&storage_);
        }

        /*!
         * @brief Return the underlying std::string, without
         * checking. If this is not a string value, the behavior
         * is undefined.
         * @return Reference to the underlying std::string.
         */
        std::string&& get_string() && noexcept {
            return std::move(*std::get_if<std::string>(&storage_));
        }

        /*!
         * @brief Return the underlying object, without
         * checking. If this is not an object value, the
         * behavior is undefined.
         * @return Reference to the underlying object.
         */
        object& get_object() & noexcept {
            return *std::get_if<object>(&storage_);
        }

        /*!
         * @brief Return the underlying object, without
         * checking. If this is not an object value, the
         * behavior is undefined.
         * @return Reference to the underlying object.
         */
        const object& get_object() const& noexcept {
            return *std::get_if<object>(&storage_);
        }

        /*!
         * @brief Return the underlying object, without
         * checking. If this is not an object value, the
         * behavior is undefined.
         * @return Reference to the underlying object.
         */
        object&& get_object() && noexcept {
            return std::move(*std::get_if<object>(&storage_));
        }

        /*!
         * @brief Return the underlying array, without checking.
         * If this is not an array value, the behavior is
         * undefined.
         * @return Reference to the underlying array.
         */
        array& get_array() & noexcept {
            return *std::get_if<array>(&storage_);
        }

        /*!
         * @brief Return the underlying array, without checking.
         * If this is not an array value, the behavior is
         * undefined.
         * @return Reference to the underlying array.
         */
        const array& get_array() const& noexcept {
            return *std::get_if<array>(&storage_);
        }

        /*!
         * @brief Return the underlying array, without checking.
         * If this is not an array value, the behavior is
         * undefined.
         * @return Reference to the underlying array.
         */
        array&& get_array() && noexcept {
            return std::move(*std::get_if<array>(&storage_));
        }

        /*!
         * @brief Return the stored number cast to an arithmetic
         * type.
         *
         * This function attempts to return the stored value
         * converted to the arithmetic type T which may not be
         * bool:
         *
         * - If T is an integral type and the stored value is a
         * number which can be losslessly converted, the
         * conversion is performed without error and the
         * converted number is returned.
         *
         * - If T is an integral type and the stored value is a
         * number which cannot be losslessly converted, then the
         * operation fails with an error.
         *
         * - If T is a floating point type and the stored value
         * is a number, the conversion is performed without
         * error. The converted number is returned, with a
         * possible loss of precision.
         *
         * - Otherwise, if the stored value is not a number;
         * that is, if is_number() returns false, then the
         * operation fails with an error.
         * @tparam T Type of number.
         * @param ec Set to the error, if any occurred.
         * @return The converted number.
         */
        template <class T, std::enable_if_t<std::is_arithmetic_v<T> &&
                                            !std::is_same_v<T, bool>>>
        T to_number(std::error_code& ec) const noexcept {
            ec.clear();
            error e{};
            T t{};
            if constexpr (std::is_signed_v<T>) {
                t = to_signed_number<T>(e);
            }
            else if constexpr (std::is_unsigned_v<T>) {
                t = to_unsigned_number<T>(e);
            }
            else if constexpr (std::is_floating_point_v<T>) {
                t = to_floating_number<T>(e);
            }
            else {
                static_assert(always_false<T>, "T must be integral or "
                                               "floating point");
            }
            if (e != error{}) {
                ec = make_error(e);
            }
            return t;
        }

        /*!
         * @brief Return the stored number cast to an arithmetic
         * type. Exception is thrown on error.
         *
         * This function attempts to return the stored value
         * converted to the arithmetic type T which may not be
         * bool:
         *
         * - If T is an integral type and the stored value is a
         * number which can be losslessly converted, the
         * conversion is performed without error and the
         * converted number is returned.
         *
         * - If T is an integral type and the stored value is a
         * number which cannot be losslessly converted, then the
         * operation fails with an error.
         *
         * - If T is a floating point type and the stored value
         * is a number, the conversion is performed without
         * error. The converted number is returned, with a
         * possible loss of precision.
         *
         * - Otherwise, if the stored value is not a number;
         * that is, if is_number() returns false, then the
         * operation fails with an error.
         * @tparam T Type of number.
         * @return The converted number.
         */
        template <class T, std::enable_if_t<std::is_arithmetic_v<T> &&
                                            !std::is_same_v<T, bool>>>
        T to_number() const {
            std::error_code ec;
            T t = to_number<T>(ec);
            if (ec) {
                throw std::system_error{ec};
            }
            return t;
        }

        /*!
         * @brief Return the stored number cast to an arithmetic
         * type.
         *
         * This function attempts to return the stored value
         * converted to the arithmetic type T which may not be
         * bool:
         *
         * - If T is an integral type and the stored value is a
         * number which can be losslessly converted, the
         * conversion is performed without error and the
         * converted number is returned.
         *
         * - If T is an integral type and the stored value is a
         * number which cannot be losslessly converted, then the
         * operation fails with an error.
         *
         * - If T is a floating point type and the stored value
         * is a number, the conversion is performed without
         * error. The converted number is returned, with a
         * possible loss of precision.
         *
         * - Otherwise, if the stored value is not a number;
         * that is, if is_number() returns false, then the
         * operation fails with an error.
         * @tparam T Type of number.
         * @param v Assigned the converted number on success,
         * and left unchanged on failure.
         * @return True if conversion is successful, otherwise
         * false.
         */
        template <class T, std::enable_if_t<std::is_arithmetic_v<T> &&
                                            !std::is_same_v<T, bool>>>
        bool to_number(T& v) const noexcept {
            std::error_code ec;
            T t = to_number<T>(ec);
            if (ec) {
                return false;
            }
            v = t;
            return true;
        }

        /*!
         * @brief Return the underlying bool, or throw an
         * exception if not a bool value.
         * @return Reference to the underlying bool.
         */
        RAD_EXPORT_DECL bool& as_bool();

        /*!
         * @brief Return the underlying bool, or throw an
         * exception if not a bool value.
         * @return Copy of the underlying bool.
         */
        RAD_EXPORT_DECL bool as_bool() const;

        /*!
         * @brief Return the underlying std::int64_t, or throw
         * an exception if not a signed integer value.
         * @return Reference to the underlying std::int64_t.
         */
        RAD_EXPORT_DECL std::int64_t& as_int64();

        /*!
         * @brief Return the underlying std::int64_t, or throw
         * an exception if not a signed integer value.
         * @return Copy of the underlying std::int64_t.
         */
        RAD_EXPORT_DECL std::int64_t as_int64() const;

        /*!
         * @brief Return the underlying std::uint64_t, or throw
         * an exception if not an unsigned integer value.
         * @return Reference to the underlying std::uint64_t.
         */
        RAD_EXPORT_DECL std::uint64_t& as_uint64();

        /*!
         * @brief Return the underlying std::uint64_t, or throw
         * an exception if not an unsigned integer value.
         * @return Copy of the underlying std::uint64_t.
         */
        RAD_EXPORT_DECL std::uint64_t as_uint64() const;

        /*!
         * @brief Return the underlying double, or throw an
         * exception if not a floating point value.
         * @return Reference to the underlying double.
         */
        RAD_EXPORT_DECL double& as_double();

        /*!
         * @brief Return the underlying double, or throw an
         * exception if not a floating point value.
         * @return Copy of the underlying double.
         */
        RAD_EXPORT_DECL double as_double() const;

        /*!
         * @brief Return the underlying std::string, or throw an
         * exception if not a string value.
         * @return Reference to the underlying std::string.
         */
        RAD_EXPORT_DECL std::string& as_string() &;

        /*!
         * @brief Return the underlying std::string, or throw an
         * exception if not a string value.
         * @return Reference to the underlying std::string.
         */
        RAD_EXPORT_DECL const std::string& as_string() const&;

        /*!
         * @brief Return the underlying std::string, or throw an
         * exception if not a string value.
         * @return Reference to the underlying std::string.
         */
        RAD_EXPORT_DECL std::string&& as_string() &&;

        /*!
         * @brief Return the underlying object, or throw an
         * exception if not an object value.
         * @return Reference to the underlying object.
         */
        RAD_EXPORT_DECL object& as_object() &;

        /*!
         * @brief Return the underlying object, or throw an
         * exception if not an object value.
         * @return Reference to the underlying object.
         */
        RAD_EXPORT_DECL const object& as_object() const&;

        /*!
         * @brief Return the underlying object, or throw an
         * exception if not an object value.
         * @return Reference to the underlying object.
         */
        RAD_EXPORT_DECL object&& as_object() &&;

        /*!
         * @brief Return the underlying array, or throw an
         * exception if not an array value.
         * @return Reference to the underlying array.
         */
        RAD_EXPORT_DECL array& as_array() &;

        /*!
         * @brief Return the underlying array, or throw an
         * exception if not an array value.
         * @return Reference to the underlying array.
         */
        RAD_EXPORT_DECL const array& as_array() const&;

        /*!
         * @brief Return the underlying array, or throw an
         * exception if not an array value.
         * @return Reference to the underlying array.
         */
        RAD_EXPORT_DECL array&& as_array() &&;

        /*!
         * @brief Access an element, with bounds checking.
         * Used to access the element of the underlying object.
         * If this is not an object value, or the object does
         * not contain an element with this key, an exception is
         * thrown. Same as this->as_object().at(key).
         * @param key The key of the element to find.
         * @return Reference to the element.
         */
        value& at(std::string_view key) & {
            return as_object().at(key);
        }

        /*!
         * @brief Access an element, with bounds checking.
         * Used to access the element of the underlying object.
         * If this is not an object value, or the object does
         * not contain an element with this key, an exception is
         * thrown. Same as this->as_object().at(key).
         * @param key The key of the element to find.
         * @return Reference to the element.
         */
        const value& at(std::string_view key) const& {
            return as_object().at(key);
        }

        /*!
         * @brief Access an element, with bounds checking.
         * Used to access the element of the underlying object.
         * If this is not an object value, or the object does
         * not contain an element with this key, an exception is
         * thrown. Same as this->as_object().at(key).
         * @param key The key of the element to find.
         * @return Reference to the element.
         */
        value&& at(std::string_view key) && {
            return std::move(as_object()).at(key);
        }

        /*!
         * @brief Access an element, with bounds checking.
         * Used to access the element of the underlying array.
         * If this is not an array value, or the array size is
         * equal to or smaller than @p pos, an exception is
         * thrown. Same as this->as_array().at(pos).
         * @param pos A zero-based array index.
         * @return Reference to the element.
         */
        value& at(std::size_t pos) & {
            return as_array().at(pos);
        }

        /*!
         * @brief Access an element, with bounds checking.
         * Used to access the element of the underlying array.
         * If this is not an array value, or the array size is
         * equal to or smaller than @p pos, an exception is
         * thrown. Same as this->as_array().at(pos).
         * @param pos A zero-based array index.
         * @return Reference to the element.
         */
        const value& at(std::size_t pos) const& {
            return as_array().at(pos);
        }

        /*!
         * @brief Access an element, with bounds checking.
         * Used to access the element of the underlying array.
         * If this is not an array value, or the array size is
         * equal to or smaller than @p pos, an exception is
         * thrown. Same as this->as_array().at(pos).
         * @param pos A zero-based array index.
         * @return Reference to the element.
         */
        value&& at(std::size_t pos) && {
            return std::move(as_array()).at(pos);
        }

        /*!
         * @brief Replace with a null value.
         * The current value is destroyed and replaced with
         * null.
         */
        void emplace_null() noexcept {
            storage_.emplace<std::nullptr_t>();
        }

        /*!
         * @brief Replace with a bool value.
         * The value is replaced with a bool equal to @p b.
         * @param b The boolean to assign to the underlying bool
         * value.
         * @return Reference to the new underlying bool value.
         */
        bool& emplace_bool(bool b = false) noexcept {
            return storage_.emplace<bool>(b);
        }

        /*!
         * @brief Replace with a std::uint64_t value.
         * The value is replaced with a std::uint64_t equal to
         * @p u.
         * @param u The std::uint64_t to assign to the
         * underlying std::uint64_t value.
         * @return Reference to the new underlying std::uint64_t
         * value.
         */
        std::uint64_t& emplace_uint64(std::uint64_t u = 0) noexcept {
            return storage_.emplace<std::uint64_t>(u);
        }

        /*!
         * @brief Replace with a std::int64_t value.
         * The value is replaced with a std::int64_t equal to @p
         * i.
         * @param i The std::int64_t to assign to the underlying
         * std::int64_t value.
         * @return Reference to the new underlying std::int64_t
         * value.
         */
        std::int64_t& emplace_int64(std::int64_t i = 0) noexcept {
            return storage_.emplace<std::int64_t>(i);
        }

        /*!
         * @brief Replace with a double value.
         * The value is replaced with a double equal to @p d.
         * @param d The double to assign to the underlying
         * double value.
         * @return Reference to the new underlying double value.
         */
        double& emplace_double(double d = 0) noexcept {
            return storage_.emplace<double>(d);
        }

        /*!
         * @brief Replace with a string value.
         * The value is replaced with a std::string constructed
         * as by std::string(std::forward<Args>(args)...).
         * @tparam ...Args Type of arguments.
         * @param ...args Arguments to forward to the
         * std::string constructor.
         * @return Reference to the new underlying std::string
         * value.
         */
        template <class... Args>
        std::string& emplace_string(Args&&... args) {
            return storage_.emplace<std::string>(std::forward<Args>(args)...);
        }

        /*!
         * @brief Replace with an object value.
         * The value is replaced with an object constructed as
         * by object(std::forward<Args>(args)...).
         * @tparam ...Args Type of arguments.
         * @param ...args Arguments to forward to the object
         * constructor.
         * @return Reference to the new underlying object value.
         */
        template <class... Args>
        object& emplace_object(Args&&... args) {
            return storage_.emplace<object>(std::forward<Args>(args)...);
        }

        /*!
         * @brief Replace with an array value.
         * The value is replaced with an array constructed as by
         * array(std::forward<Args>(args)...).
         * @tparam ...Args Type of arguments.
         * @param ...args Arguments to forward to the array
         * constructor.
         * @return Reference to the new underlying array value.
         */
        template <class... Args>
        array& emplace_array(Args&&... args) {
            return storage_.emplace<array>(std::forward<Args>(args)...);
        }

        /*!
         * @brief Swap the given values.
         *
         * Exchanges the contents of this value with another
         * value.
         * @param other The value to swap with.
         */
        void swap(value& other) noexcept;

        /*!
         * @brief Swap the given values.
         *
         * Exchanges the contents of value lhs with another
         * value rhs.
         * @param lhs The value to exchange.
         * @param rhs The value to exchange.
         */
        friend void swap(value& lhs, value& rhs) noexcept {
            lhs.swap(rhs);
        }

        /*!
         * @brief Check if two values are equal.
         * @param lhs The value to compare for equality.
         * @param rhs The value to compare for equality.
         * @return True if the two values are equal, otherwise
         * false.
         */
        friend bool operator==(const value& lhs, const value& rhs) noexcept {
            return lhs.equal(rhs);
        }

        /*!
         * @brief Serialize value as JSON text to an output
         * stream.
         * @param os The output stream to serialize to.
         * @param v The value to serialize.
         * @return Reference to os.
         */
        friend std::ostream& operator<<(std::ostream& os, const value& v) {
            return v.serialize_to_ostream(os);
        }

    private:
        // objects are not ordered
        RAD_EXPORT_DECL bool equal(const value& other) const noexcept;

        RAD_EXPORT_DECL std::ostream&
        serialize_to_ostream(std::ostream& os) const;

        template <class T>
        T to_signed_number(error& e) const noexcept {
            constexpr T min_t = std::numeric_limits<T>::min();
            constexpr T max_t = std::numeric_limits<T>::max();

            if (const std::int64_t* i = if_int64()) {
                if (*i >= min_t && *i <= max_t) {
                    return static_cast<T>(*i);
                }
            }
            else if (const std::uint64_t* u = if_uint64()) {
                if (*u <= static_cast<std::uint64_t>(max_t)) {
                    return static_cast<T>(*u);
                }
            }
            else if (const double* d = if_double()) {
                if (*d >= min_t && *d <= max_t && static_cast<T>(*d) == *d) {
                    return static_cast<T>(*d);
                }
            }
            else {
                e = error::not_number;
                return T{};
            }
            e = error::not_exact;
            return T{};
        }

        template <class T>
        T to_unsigned_number(error& e) const noexcept {
            constexpr T max_t = std::numeric_limits<T>::max();

            if (const std::int64_t* i = if_int64()) {
                if (*i >= 0 && static_cast<std::uint64_t>(*i) <= max_t) {
                    return static_cast<T>(*i);
                }
            }
            else if (const std::uint64_t* u = if_uint64()) {
                if (*u <= max_t) {
                    return static_cast<T>(*u);
                }
            }
            else if (const double* d = if_double()) {
                if (*d >= 0 && *d <= max_t && static_cast<T>(*d) == *d) {
                    return static_cast<T>(*d);
                }
            }
            else {
                e = error::not_number;
                return T{};
            }
            e = error::not_exact;
            return T{};
        }

        template <class T>
        T to_floating_number(error& e) const noexcept {
            if (const std::int64_t* i = if_int64()) {
                return static_cast<T>(*i);
            }
            else if (const std::uint64_t* u = if_uint64()) {
                return static_cast<T>(*u);
            }
            else if (const double* d = if_double()) {
                return static_cast<T>(*d);
            }
            e = error::not_number;
            return T{};
        }

        std::variant<std::nullptr_t, bool, uint64_t, int64_t, double,
                     std::string, object, array>
            storage_;
    };

    /*!
     * @brief A key/value pair.
     *
     *This is the type of element used by the object container.
     */
    class key_value_pair {
        std::string key_;
        json::value val_;

    public:
        /*!
         * @brief Construct a key/value pair. Copy the
         * characters of key and construct the value as if by
         * value(std::forward<Args>(args)...).
         * @tparam ...Args Type of arguments.
         * @param key The key string to use.
         * @param ...args Optional arguments forwarded to the
         * value constructor.
         */
        template <class... Args>
        key_value_pair(std::string_view key, Args&&... args)
            : key_{key}, val_{std::forward<Args>(args)...} {
        }

        /*!
         * @brief Construct a key/value pair. Copy the
         * characters of key and construct the value by copying
         * the second element of the pair.
         * @param p A std::pair with the key string and value to
         * construct with.
         */
        key_value_pair(const std::pair<std::string_view, json::value>& p)
            : key_{p.first}, val_{p.second} {
        }

        /*!
         * @brief Construct a key/value pair. Copy the
         * characters of key and construct the value by moving
         * the second element of the pair.
         * @param p A std::pair with the key string and value to
         * construct with.
         */
        key_value_pair(std::pair<std::string_view, json::value>&& p)
            : key_{p.first}, val_{std::move(p.second)} {
        }

        /*!
         * @brief Get a view of the pair's key.
         * @return Constant view of the pair's key.
         */
        std::string_view key() const noexcept {
            return key_;
        }

        /*!
         * @brief The pair's key as a null-terminated string.
         * @return Constant null-terminated string.
         */
        const char* key_c_str() const noexcept {
            return key_.c_str();
        }

        /*!
         * @brief Access the pair's value.
         * @return Reference to the pair's value.
         */
        const json::value& value() const& {
            return val_;
        }

        /*!
         * @brief Access the pair's value.
         * @return Reference to the pair's value.
         */
        class json::value& value() & {
            return val_;
        }

        /*!
         * @brief Access the pair's value.
         * @return Reference to the pair's value.
         */
        class json::value&& value() && {
            return std::move(val_);
        }

        friend bool operator==(const key_value_pair& lhs,
                               const key_value_pair& rhs) noexcept = default;
    };

    /*!
     * @brief A stack of value elements, for building a document.
     *
     * This stack of value allows iterative construction of a JSON document
     * in memory.
     */
    class value_stack {
        using stack_item = std::variant<value, std::string>;

    public:
        /*!
         * @brief Push a bool onto the stack.
         * @param b The boolean to insert.
         */
        RAD_EXPORT_DECL void push_bool(bool b);

        /*!
         * @brief Push a std::int64_t onto the stack.
         * @param i The number to insert.
         */
        RAD_EXPORT_DECL void push_int64(std::int64_t i);

        /*!
         * @brief Push a std::uint64_t onto the stack.
         * @param u The number to insert.
         */
        RAD_EXPORT_DECL void push_uint64(std::uint64_t u);

        /*!
         * @brief Push a double onto the stack.
         * @param d The number to insert.
         */
        RAD_EXPORT_DECL void push_double(double d);

        /*!
         * @brief Push a null onto the stack.
         */
        RAD_EXPORT_DECL void push_null();

        /*!
         * @brief Push a part of a key or a string onto the
         * stack.
         *
         * This function pushes the characters in s onto the
         * stack, appending to any existing characters or
         * creating new characters as needed. Once a string part
         * is placed onto the stack, the only valid stack
         * operations are:
         *
         * - push_chars to append additional characters to the
         * key or string being built,
         *
         * - push_key or push_string to finish building the key
         * or string and place the value onto the stack.
         * @param s The characters to append. This may be empty.
         */
        RAD_EXPORT_DECL void push_chars(std::string_view s);

        /*!
         * @brief Place a string value onto the stack.
         *
         * This function notionally removes all the characters
         * currently on top of the stack, then pushes a value
         * containing a std::string onto the stack formed by
         * appending @p s to the removed characters.
         * @param s The characters to append. This may be empty.
         */
        RAD_EXPORT_DECL void push_string(std::string_view s);

        /*!
         * @brief Push an object key onto the stack.
         *
         * This function notionally pops all of the characters
         * currently on top of the stack, then pushes a value
         * containing a key onto the stack formed by appending
         * @p s to the removed characters.
         * @param s The characters to append. This may be empty.
         */
        RAD_EXPORT_DECL void push_key(std::string_view s);

        /*!
         * @brief Push an array onto the stack.
         *
         * This function pushes an array value onto the stack.
         * The array is formed by first popping the top @p n
         * values from the stack. If the stack contains fewer
         * than n values, or if any of the top @p n values on
         * the stack is a key, the behavior is undefined.
         * @param n The number of values to pop from the top of
         * the stack to form the array.
         */
        RAD_EXPORT_DECL void push_array(std::size_t n);

        /*!
         * @brief Push an object onto the stack.
         *
         * This function pushes an object value onto the stack.
         * The object is formed by first popping the top n
         * key/value pairs from the stack. If the stack contains
         * fewer than @p n key/value pairs, or if any of the top
         * @p n key/value pairs on the stack does not consist of
         * exactly one key followed by one value, the behavior
         * is undefined.
         *
         * If there are object elements with duplicate keys;
         * that is, if multiple elements in an object have keys
         * that compare equal, only the last equivalent element
         * will be inserted.
         * @param n The number of key/value pairs to pop from
         * the top of the stack to form the array.
         */
        RAD_EXPORT_DECL void push_object(std::size_t n);

        /*!
         * @brief Return the top-level value.
         *
         * This function transfers ownership of the constructed
         * top-level value to the caller. The behavior is
         * undefined if there is not a single, top-level
         * element.
         * @return A value holding the result. Ownership of this
         * value is transferred to the caller.
         */
        RAD_EXPORT_DECL value release() noexcept;

        /*!
         * @brief Prepare to build a new document.
         *
         * This function must be called before constructing a
         * new top-level value. Any previously existing partial
         * or complete elements are destroyed, but internal
         * dynamically allocated memory is preserved which may
         * be reused to build new values.
         */
        RAD_EXPORT_DECL void reset() noexcept;

    private:
        bool expects_string_or_key() const noexcept {
            return has_pending_chars_;
        }

        std::string stack_chars_;
        std::vector<stack_item> stack_;
        bool has_pending_chars_ = false;
    };

    template <class InputIt>
    array::array(InputIt first, InputIt last) : values_{first, last} {
    }

    inline array::~array() = default;

    inline std::size_t array::capacity() const noexcept {
        return values_.capacity();
    }

    inline std::size_t array::size() const noexcept {
        return values_.size();
    }

    inline value* array::data() noexcept {
        return values_.data();
    }

    inline const value* array::data() const noexcept {
        return values_.data();
    }

    template <class... Args>
    value& array::emplace_back(Args&&... args) {
        return values_.emplace_back(std::forward<Args>(args)...);
    }

    template <class... Args>
    auto array::emplace(const_iterator pos, Args&&... args) -> iterator {
        return values_.emplace(pos, std::forward<Args>(args)...);
    }

    template <class InputIt>
    auto array::insert(const_iterator pos, InputIt first, InputIt last)
        -> iterator {
        return values_.insert(pos, first, last);
    }

    inline auto array::begin() -> iterator {
        return values_.begin();
    }

    inline auto array::end() -> iterator {
        return values_.end();
    }

    inline auto array::begin() const -> const_iterator {
        return values_.begin();
    }

    inline auto array::end() const -> const_iterator {
        return values_.end();
    }

    inline auto array::cbegin() const -> const_iterator {
        return values_.cbegin();
    }

    inline auto array::cend() const -> const_iterator {
        return values_.cend();
    }

    inline auto array::rbegin() -> reverse_iterator {
        return values_.rbegin();
    }

    inline auto array::rend() -> reverse_iterator {
        return values_.rend();
    }

    inline auto array::rbegin() const -> const_reverse_iterator {
        return values_.rbegin();
    }

    inline auto array::rend() const -> const_reverse_iterator {
        return values_.rend();
    }

    inline auto array::crbegin() const -> const_reverse_iterator {
        return values_.crbegin();
    }

    inline auto array::crend() const -> const_reverse_iterator {
        return values_.crend();
    }

    inline object::object() noexcept = default;

    inline object::~object() = default;

    inline std::size_t object::capacity() const noexcept {
        return keys_values_.capacity();
    }

    inline std::size_t object::size() const noexcept {
        return keys_values_.size();
    }

    template <class... Args>
    auto object::emplace(std::string_view key, Args&&... args)
        -> std::pair<iterator, bool> {
        auto it = find(key);
        if (it != end()) {
            return std::pair{it, false};
        }
        it = keys_values_.insert(
            it, key_value_pair{key, std::forward<Args>(args)...});
        return std::pair{it, true};
    }

    template <class P>
    auto object::insert(P&& p) -> std::pair<iterator, bool> {
        key_value_pair k_v{std::forward<P>(p)};
        auto it = find(k_v.key());
        const bool insert_new = it == end();
        if (insert_new) {
            it = keys_values_.insert(it, std::move(k_v));
        }
        return std::pair{it, insert_new};
    }

    template <class InputIt>
    void object::insert(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            key_value_pair k_v{*first};
            auto it = find(k_v.key());
            if (it == end()) {
                keys_values_.emplace_back(std::move(k_v));
            }
        }
    }

    template <class... Args>
    auto object::insert_or_assign(std::string_view key, Args&&... args)
        -> std::pair<iterator, bool> {
        auto it = find(key);
        const bool insert_new = it == end();
        if (insert_new) {
            it = keys_values_.insert(
                it, key_value_pair{key, std::forward<Args>(args)...});
        }
        else {
            it->value() = value{std::forward<Args>(args)...};
        }
        return std::pair{it, insert_new};
    }

    inline auto object::begin() -> iterator {
        return keys_values_.begin();
    }

    inline auto object::end() -> iterator {
        return keys_values_.end();
    }

    inline auto object::begin() const -> const_iterator {
        return keys_values_.begin();
    }

    inline auto object::end() const -> const_iterator {
        return keys_values_.end();
    }

    inline auto object::cbegin() const -> const_iterator {
        return keys_values_.cbegin();
    }

    inline auto object::cend() const -> const_iterator {
        return keys_values_.cend();
    }

    inline auto object::rbegin() -> reverse_iterator {
        return keys_values_.rbegin();
    }

    inline auto object::rend() -> reverse_iterator {
        return keys_values_.rend();
    }

    inline auto object::rbegin() const -> const_reverse_iterator {
        return keys_values_.rbegin();
    }

    inline auto object::rend() const -> const_reverse_iterator {
        return keys_values_.rend();
    }

    inline auto object::crbegin() const -> const_reverse_iterator {
        return keys_values_.crbegin();
    }

    inline auto object::crend() const -> const_reverse_iterator {
        return keys_values_.crend();
    }

    inline value::value(std::nullptr_t) noexcept : storage_{nullptr} {
    }

    inline value::value(bool b) noexcept : storage_{b} {
    }

    inline value::value(std::int64_t i) noexcept : storage_{i} {
    }

    inline value::value(std::uint64_t u) noexcept : storage_{u} {
    }

    inline value::value(std::int8_t i) noexcept
        : value(static_cast<std::int64_t>(i)) {
    }

    inline value::value(std::int16_t i) noexcept
        : value(static_cast<std::int64_t>(i)) {
    }

    inline value::value(std::int32_t i) noexcept
        : value(static_cast<std::int64_t>(i)) {
    }

    inline value::value(std::uint8_t i) noexcept
        : value(static_cast<std::uint64_t>(i)) {
    }

    inline value::value(std::uint16_t i) noexcept
        : value(static_cast<std::uint64_t>(i)) {
    }

    inline value::value(std::uint32_t i) noexcept
        : value(static_cast<std::uint64_t>(i)) {
    }

    inline value::value(double d) noexcept : storage_{d} {
    }

    inline value::value(string_kind_t) noexcept : storage_{std::string{}} {
    }

    inline value::value(std::string_view s)
        : storage_{std::in_place_type<std::string>, s} {
    }

    inline value::value(const std::string& s) noexcept : storage_{s} {
    }

    inline value::value(std::string&& s) noexcept : storage_{std::move(s)} {
    }

    inline value::value(const char* s) : value(std::string_view{s}) {
    }

    inline value::value(const char* s, std::size_t n)
        : value(std::string_view{s, n}) {
    }

    inline value::value(const char8_t* s)
        : value(std::string_view{reinterpret_cast<const char*>(s)}) {
    }

    inline value::value(const char8_t* s, std::size_t n)
        : value(std::string_view{reinterpret_cast<const char*>(s), n}) {
    }

    inline value::value(array_kind_t) noexcept : storage_{array{}} {
    }

    inline value::value(const array& a) : storage_{a} {
    }

    inline value::value(array&& a) noexcept : storage_{std::move(a)} {
    }

    inline value::value(object_kind_t) noexcept : storage_{object{}} {
    }

    inline value::value(const object& o) : storage_{o} {
    }

    inline value::value(object&& o) noexcept : storage_{std::move(o)} {
    }

    inline value::~value() = default;

    inline void value::swap(value& other) noexcept {
        storage_.swap(other.storage_);
    }
} // namespace rad::json