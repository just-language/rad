#pragma once
#include <rad/libbase.h>

#include <array>
#include <cstring>

namespace RAD_LIB_NAMESPACE {

    namespace detail {
        struct dynamic_buffer_vtable {
            void (*reserve)(void*, std::size_t);
            void (*resize)(void*, std::size_t);
            void (*resize_with_val)(void*, std::size_t, uint8_t);
            uint8_t* (*increase_size)(void*, std::size_t);
            void* (*mut_data)(void*);
            const void* (*const_data)(const void*);
            std::size_t (*size)(const void*);
            void (*insert)(void*, const void*, std::size_t);
        };

        template <class T>
        struct dynamic_buffer_fns {
            static void reserve(void* ptr, std::size_t size) {
                static_cast<T*>(ptr)->reserve(size);
            }

            static void resize(void* ptr, std::size_t size) {
                static_cast<T*>(ptr)->resize(size);
            }

            static void resize_with_val(void* ptr, std::size_t size,
                                        uint8_t val) {
                static_cast<T*>(ptr)->resize(
                    size, static_cast<typename T::value_type>(val));
            }

            static uint8_t* increase_size(void* ptr, std::size_t size) {
                T& c = *static_cast<T*>(ptr);
                std::size_t old_size = c.size();
                c.resize(old_size + size);
                return reinterpret_cast<uint8_t*>(c.data()) + old_size;
            }

            static void* mut_data(void* ptr) {
                return static_cast<void*>(static_cast<T*>(ptr)->data());
            }

            static const void* const_data(const void* ptr) {
                return static_cast<const void*>(
                    static_cast<const T*>(ptr)->data());
            }

            static std::size_t size(const void* ptr) {
                return static_cast<const T*>(ptr)->size();
            }

            static void insert(void* ptr, const void* data, std::size_t n) {
                using pointer_type = typename T::const_pointer;
                auto first = static_cast<pointer_type>(data);
                auto last = first + n;
                T& c = *static_cast<T*>(ptr);
                c.insert(c.end(), first, last);
            }

            static constexpr dynamic_buffer_vtable make_vtable() noexcept {
                return dynamic_buffer_vtable{
                    &dynamic_buffer_fns::reserve,
                    &dynamic_buffer_fns::resize,
                    &dynamic_buffer_fns::resize_with_val,
                    &dynamic_buffer_fns::increase_size,
                    &dynamic_buffer_fns::mut_data,
                    &dynamic_buffer_fns::const_data,
                    &dynamic_buffer_fns::size,
                    &dynamic_buffer_fns::insert};
            }

            inline static dynamic_buffer_vtable vtable = make_vtable();
        };

        class dynamic_buffer_impl {
        public:
            dynamic_buffer_impl(void* container, dynamic_buffer_vtable* vtable)
                : container_{container}, vtable_{vtable} {
            }

            void reserve(size_t size) {
                vtable_->reserve(container_, size);
            }

            void resize(size_t size) {
                vtable_->resize(container_, size);
            }

            void resize(size_t size, uint8_t val) {
                vtable_->resize_with_val(container_, size, val);
            }

            uint8_t* increase_size(size_t size) {
                return vtable_->increase_size(container_, size);
            }

            const void* data() const noexcept {
                return vtable_->const_data(container_);
            }

            void* data() noexcept {
                return vtable_->mut_data(container_);
            }

            size_t size() const noexcept {
                return vtable_->size(container_);
            }

            void insert(const void* p, std::size_t s) {
                vtable_->insert(container_, p, s);
            }

        private:
            void* container_;
            dynamic_buffer_vtable* vtable_;
        };
    } // namespace detail

    class mutable_buffer;

    class dynamic_buffer {
    public:
        dynamic_buffer(const dynamic_buffer&) = default;

        /*!
         * @brief construct a dyncamic_buffer using a container
         * of pod type
         * @tparam Container type of container
         * @param c a container of pod type
         */
        template <class Container,
                  std::enable_if_t<!std::is_same_v<Container, dynamic_buffer>,
                                   int> = 0>
        explicit dynamic_buffer(Container& c)
            : impl_{&c, &detail::dynamic_buffer_fns<Container>::vtable} {
        }

        /*!
         * @brief calls container reserve method with the passed
         * size parameter
         * @param size passed to the container reserve method
         */
        void reserve(size_t size) {
            impl_.reserve(size);
        }

        /*!
         * @brief calls container resize method with the passed
         * size parameter
         * @param size passed to the container resize method
         */
        void resize(size_t size) {
            impl_.resize(size);
        }

        /*!
         * @brief calls container resize method with the passed
         * size and val byte parameters
         * @param size passed to the container resize method as
         * first argument
         * @param val passed to the container resize method as
         * second argumnet after being casted to
         * Container::value_type
         */
        void resize(size_t size, uint8_t val) {
            impl_.resize(size, val);
        }

        /*!
         * @brief increases the size of the container by @p size
         * as if by resize(size() + size)
         * @param size passed to the container resize method as
         * first argument
         * @return pointer to the start of new appended data
         * (data() + old_size)
         */
        uint8_t* increase_size(size_t size) {
            return impl_.increase_size(size);
        }

        /*!
         * @brief increases the size of the container by @p size
         * as if by resize(size() + size)
         * @param size passed to the container resize method as
         * first argument
         * @return mutable buffer view of new appended data
         * (data() + old_size)
         */
        mutable_buffer prepare(size_t size);

        /*!
         * @brief returns pointer to the start of container data
         * as if by container data() method
         * @return a pointer returned by container data() method
         */
        const void* data() const noexcept {
            return impl_.data();
        }

        /*!
         * @brief returns pointer to the start of container data
         * as if by container data() method
         * @return a pointer returned by container data() method
         */
        void* data() noexcept {
            return impl_.data();
        }

        /*!
         * @brief calls data() and casts the result to a pointer
         * of T as if by static_cast<T*>(data())
         * @tparam T type to cast to a pointer of it
         * @return a pointer returned by container data() method
         * and casted to T*
         */
        template <class T>
        const T* data_as() const noexcept {
            return static_cast<const T*>(data());
        }

        /*!
         * @brief calls data() and casts the result to a pointer
         * of T as if by static_cast<T*>(data())
         * @tparam T type to cast to a pointer of it
         * @return a pointer returned by container data() method
         * and casted to T*
         */
        template <class T>
        T* data_as() noexcept {
            return static_cast<T*>(data());
        }

        /*!
         * @brief calls the container size() method of the
         * container
         * @return the size of the container returned by the
         * container size() method
         */
        size_t size() const noexcept {
            return impl_.size();
        }

        /*!
         * @brief Check if the container is empty as by size()
         * == 0.
         * @return True if the container is empty, otherwise
         * false.
         */
        bool empty() const noexcept {
            return size() != 0;
        }

        /*!
         * @brief inserts bytes in the container as if:
         * @code
         * using pointer_type = typename
         * Container::const_pointer; auto first =
         * static_cast<pointer_type>(ptr); auto last = first +
         * n; c.insert(c.end(), first, last);
         * @endcode
         * @param ptr pointer to data to insert
         * @param n number of bytes pointed to by ptr
         */
        void insert(const void* ptr, size_t n) {
            impl_.insert(ptr, n);
        }

        /*!
         * @brief inserts a pod object into the container
         * @param value a pod object to insert as if by
         * insert(&value, sizeof(value))
         */
        template <class T>
        void push_back(const T& value) {
            insert(&value, sizeof(value));
        }

    private:
        detail::dynamic_buffer_impl impl_;
    };

}; // namespace RAD_LIB_NAMESPACE
