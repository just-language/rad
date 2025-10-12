#pragma once
#include <rad/libbase.h>
#include <rad/string.h>
#include <filesystem>
#include <string>

namespace RAD_LIB_NAMESPACE::sysinfo {
    /*!
     * @brief Get total size of CPU cache of all levels in bytes.
     * @return The total size of CPU cache of all levels in bytes.
     */
    RAD_EXPORT_DECL uint32_t cache_size();

    /*!
     * @brief Get count of logical CPU cores.
     * @return Count of logical CPU cores.
     */
    RAD_EXPORT_DECL uint32_t cores();

    /*!
     * @brief Get a string representing the current operating system name.
     * @return A string representing the current operating system name.
     */
    RAD_EXPORT_DECL std::string os_name();

    /*!
     * @brief Get a string representing the current operating system install
     * date.
     * @return A string representing the current operating system install date.
     */
    RAD_EXPORT_DECL std::string os_installation_date();

    /*!
     * @brief System memory information.
     */
    struct system_memory_info_t {
        /// Total system memory size in bytes.
        uint64_t total_memory = 0;
        /// Used system memory size in bytes.
        uint64_t used_memory = 0;
        /// Free system memory size in bytes.
        uint64_t free_memory = 0;
        /// Memory usage percent from 0 to 100.
        uint32_t memory_usage = 0;
    };

    /*!
     * @brief Get system memory information.
     * @return The system memory information.
     */
    RAD_EXPORT_DECL system_memory_info_t system_memory_info();

    /*!
     * @brief Get total system memory size in bytes.
     * @return The total system memory size in bytes.
     */
    inline uint64_t total_system_memory() {
        return system_memory_info().total_memory;
    }

    /*!
     * @brief Get used system memory size in bytes.
     * @return The used system memory size in bytes.
     */
    inline uint64_t used_system_memory() {
        return system_memory_info().used_memory;
    }

    /*!
     * @brief Get free system memory size in bytes.
     * @return The free system memory size in bytes.
     */
    inline uint64_t free_system_memory() {
        return system_memory_info().free_memory;
    }

    /*!
     * @brief Get system memory usage percent from 0 to 100.
     * @return The system memory usage percent from 0 to 100.
     */
    inline uint32_t system_memory_usage() {
        return system_memory_info().memory_usage;
    }

    /*!
     * @brief Get the current executable path.
     * @return The current executable path.
     */
    RAD_EXPORT_DECL std::filesystem::path executable_path();

    /*!
     * @brief Get the current executable name in native system characters.
     * @return The current executable name in native system characters.
     */
    inline std::filesystem::path::string_type native_executable_name() {
        return executable_path().filename().native();
    }

    /*!
     * @brief Get the current executable name in UTF-8 string.
     * @return The current executable name in UTF-8 string.
     */
    inline std::string executable_name() {
        return to_string(executable_path().filename().native());
    }
} // namespace RAD_LIB_NAMESPACE::sysinfo