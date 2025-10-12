#pragma once
#include <rad/libbase.h>

#include <cassert>
#include <cstdio>
#include <iterator>
#include <vector>

namespace RAD_LIB_NAMESPACE {
    template <class T>
    struct null_allocator {
        template <class U>
        struct rebind {
            using other = null_allocator<U>;
        };

        using pointer = T*;
        using const_pointer = const T*;
        using void_pointer = void*;
        using const_void_pointer = const void*;
        using reference = T&;
        using const_reference = const T&;
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using is_always_equal = std::true_type;

        constexpr null_allocator() = default;

        pointer allocate(size_type) {
            throw std::bad_alloc();
        }

        void deallocate(pointer, size_type) {
            throw std::bad_alloc();
        }
    };

    template <class T, std::size_t N>
    class stack_source {
    public:
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = std::size_t;

        bool can_allocate(size_type n) const noexcept {
            check_overwrite();
            return !is_used && n <= N;
        }

        bool can_deallocate(pointer p, size_type n) const noexcept {
            check_overwrite();
            bool points_to_storage =
                p == reinterpret_cast<const_pointer>(&storage[0]);
            assert((points_to_storage && n <= N) || !points_to_storage);
            return points_to_storage;
        }

        pointer allocate(size_type n) noexcept {
            check_overwrite();
            assert(can_allocate(n));
            is_used = true;
            return reinterpret_cast<pointer>(&storage[0]);
        }

        void deallocate(pointer p, size_type n) noexcept {
            check_overwrite();
            assert(can_deallocate(p, n));
            is_used = false;
        }

        bool in_use() const noexcept {
            check_overwrite();
            return is_used;
        }

    private:
        void check_overwrite() const noexcept {
            assert(debug_first_tag == this && debug_last_tag == this &&
                   "data was overwritten");
        }

#ifndef NDEBUG
        void* debug_first_tag = this;
#endif // !NDEBUG
        std::aligned_storage_t<sizeof(T), alignof(T)> storage[N];
#ifndef NDEBUG
        void* debug_last_tag = this;
#endif // !NDEBUG
        bool is_used = false;
    };

    namespace experimental {
        template <class T, std::size_t N,
                  class FallbackAllocator = std::allocator<T>>
        class stack_allocator : private FallbackAllocator {
            static constexpr bool fallback_dont_allocate =
                std::is_same_v<FallbackAllocator, null_allocator<T>>;

            using alloc_traits = std::allocator_traits<FallbackAllocator>;

            template <typename, std::size_t, typename>
            friend class stack_allocator;

        public:
            template <class U>
            struct rebind {
                using other = stack_allocator<
                    U, N, typename alloc_traits::template rebind_alloc<U>>;
            };

            using pointer = T*;
            using const_pointer = const T*;
            using void_pointer = void*;
            using const_void_pointer = const void*;
            using reference = T&;
            using const_reference = const T&;
            using value_type = T;
            using size_type = std::size_t;
            using difference_type = std::ptrdiff_t;
            using is_always_equal = std::false_type;
            using fallback_allocator = FallbackAllocator;
            using source_type = stack_source<T, N>;

            stack_allocator() = default;

            stack_allocator(
                source_type* source,
                const fallback_allocator& alloc = fallback_allocator())
                : fallback_allocator(alloc), source{source} {
            }

            stack_allocator(
                const fallback_allocator& alloc = fallback_allocator())
                : fallback_allocator(alloc) {
            }

            stack_allocator(const stack_allocator& other)
                : source{const_cast<source_type*>(other.source)} {
            }

            template <class U,
                      std::enable_if_t<sizeof(U) != sizeof(T), int> = 0>
            stack_allocator(const stack_allocator<U, N>&) {
            }

            template <class U,
                      std::enable_if_t<sizeof(U) == sizeof(T), int> = 0>
            stack_allocator(const stack_allocator<U, N>& other)
                : source{reinterpret_cast<source_type*>(
                      const_cast<typename stack_allocator<U, N>::source_type*>(
                          other.source))} {
            }

            pointer address(reference ref) const noexcept {
                return std::addressof(ref);
            }

            const_pointer address(const_reference ref) const noexcept {
                return std::addressof(ref);
            }

            pointer allocate(size_type n) {
                if (!source || !source->can_allocate(n)) {
                    return fallback_allocator::allocate(n);
                }
                return source->allocate(n);
            }

            void deallocate(pointer p, size_type n) noexcept {
                if (source && source->can_deallocate(p, n)) {
                    source->deallocate(p, n);
                }
                else {
                    fallback_allocator::deallocate(p, n);
                }
            }

            bool in_use() const noexcept {
                return source && source->in_use();
            }

        private:
            source_type* source = nullptr;
        };
    } // namespace experimental

    template <class T, size_t N, class FallbackAllocator = std::allocator<T>>
    class stack_vector
        : public std::vector<
              T, experimental::stack_allocator<T, N, FallbackAllocator>> {
        using container_type =
            std::vector<T,
                        experimental::stack_allocator<T, N, FallbackAllocator>>;
        using stack_allocator_type = typename container_type::allocator_type;
        using source_type = typename stack_allocator_type::source_type;

    public:
        using size_type = typename container_type::size_type;
        using fallback_allocator =
            typename stack_allocator_type::fallback_allocator;

        stack_vector(const fallback_allocator& fb_alloc = fallback_allocator{})
            : container_type(stack_allocator_type(&source, fb_alloc)) {
            container_type::reserve(N);
        }

        stack_vector(size_type count, const T& value = T()) : stack_vector() {
            for (size_type i = 0; i < count; ++i) {
                container_type::push_back(value);
            }
        }

        stack_vector(const stack_vector& other) : stack_vector() {
            container_type::insert(container_type::end(), other.begin(),
                                   other.end());
        }

        template <size_t S, class Alloc>
        stack_vector(const stack_vector<T, S, Alloc>& other) : stack_vector() {
            container_type::insert(container_type::end(), other.begin(),
                                   other.end());
        }

        stack_vector(stack_vector&& other) : stack_vector() {
            std::move(other.begin(), other.end(), std::back_inserter(*this));
        }

        template <size_t S, class Alloc>
        stack_vector(stack_vector<T, S, Alloc>&& other) noexcept
            : stack_vector() {
            std::move(other.begin(), other.end(), std::back_inserter(*this));
        }

        stack_vector& operator=(stack_vector&& other) noexcept {
            this->~stack_vector();
            new (this) stack_vector(std::move(other));
            return *this;
        }

        template <size_t S, class Alloc>
        stack_vector& operator=(stack_vector<T, S, Alloc>&& other) noexcept {
            this->~stack_vector();
            new (this) stack_vector(std::move(other));
            return *this;
        }

        stack_vector& operator=(const stack_vector& other) {
            auto new_vec(other);
            this->~stack_vector();
            new (this) stack_vector(std::move(new_vec));
            return *this;
        }

        template <size_t S, class Alloc>
        stack_vector& operator=(const stack_vector<T, S, Alloc>& other) {
            auto new_vec(other);
            this->~stack_vector();
            new (this) stack_vector(std::move(new_vec));
            return *this;
        }

    private:
        source_type source;
    };

} // namespace RAD_LIB_NAMESPACE