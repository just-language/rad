#pragma once
#include <rad/libbase.h>
#include <rad/os_types.h>

namespace RAD_LIB_NAMESPACE::env {
    /*!
     * @brief RAII Wrapper for the environment block created
     * by `CreateEnvironmentBlock`.
     */
    class environment_block {
        struct env_deleter {
            using pointer = void*;
            RAD_EXPORT_DECL void operator()(pointer env_ptr) const noexcept;
        };

    public:
        /// The wrapper handle for native environment block.
        using native_handle_type = std::unique_ptr<void, env_deleter>;

        /*!
         * @brief Construct an empty environment block.
         */
        environment_block() = default;

        /*!
         * @brief Create an environment block for a specified user.
         * @param token The user token handle.
         * @param inherit Specifies whether to inherit from the current process'
         * environment.
         */
        environment_block(os::handle& token, bool inherit = false) {
            create(token, inherit);
        }

        /*!
         * @brief Get a reference to the wrapper handle.
         * @return A reference to the wrapper handle.
         */
        native_handle_type& native_handle() noexcept {
            return env_;
        }

        /*!
         * @brief Get a const reference to the wrapper handle.
         * @return A const reference to the wrapper handle.
         */
        const native_handle_type& native_handle() const noexcept {
            return env_;
        }

        /*!
         * @brief Create an environment block for a specified user.
         * On success the current environment block, if any, is destroyed
         * and replaced by the new created environment block.
         * @param token The user token handle.
         * @param inherit Specifies whether to inherit from the current process'
         * environment.
         */
        RAD_EXPORT_DECL void create(os::handle& token, bool inherit = false);

        /*!
         * @brief Destroy the current environment block, if any.
         */
        void destroy() noexcept {
            env_.reset();
        }

    private:
        native_handle_type env_;
    };
} // namespace RAD_LIB_NAMESPACE::env