#pragma once
#include <rad/buffer.h>
#include <rad/dynamic_buffer.h>
#include <rad/libbase.h>
#include <rad/os_types.h>
#include <rad/string.h>
#include <rad/windows/clock.h>

#include <optional>
#include <variant>
#include <vector>

extern "C" {
struct HKEY__;
using HKEY = HKEY__*;
using PHKEY = HKEY*;
}

namespace RAD_LIB_NAMESPACE::winreg {
    /*!
     * @brief Registry predefined keys values.
     */
    enum class defined_keys : uintptr_t {
        /// HKEY_CLASSES_ROOT
        classes_root = (uintptr_t)((long)0x80000000),
        /// HKEY_CURRENT_USER
        current_user = (uintptr_t)((long)0x80000001),
        /// HKEY_LOCAL_MACHINE
        local_machine = (uintptr_t)((long)0x80000002),
        /// HKEY_USERS
        users = (uintptr_t)((long)0x80000003),
        /// HKEY_PERFORMANCE_DATA
        performance_data = (uintptr_t)((long)0x80000004),
        /// HKEY_CURRENT_CONFIG
        current_config = (uintptr_t)((long)0x80000005),
        /// HKEY_CURRENT_USER_LOCAL_SETTINGS
        current_user_local_settings = (uintptr_t)((long)0x80000007),
        /// HKEY_PERFORMANCE_TEXT
        performance_text = (uintptr_t)((long)0x80000050),
        /// HKEY_PERFORMANCE_NLSTEXT
        performance_nlstext = (uintptr_t)((long)0x80000060),
    };

    /*!
     * @brief Registry access rights.
     */
    enum class access : uint32_t {
        /// KEY_ALL_ACCESS
        all = 0xF003F,
        /// KEY_CREATE_LINK (Reserved)
        create_link = 0x0020,
        /// KEY_CREATE_SUB_KEY Required to create a subkey of a registry key.
        create_subkey = 0x0004,
        /// KEY_ENUMERATE_SUB_KEYS Required to enumerate the subkeys of a
        /// registry key.
        enum_subkeys = 0x0008,
        /// KEY_EXECUTE Equivalent to KEY_READ.
        execute = 0x20019,
        /*!
         * @brief KEY_NOTIFY Required to request change notifications for a
         * registry key or for subkeys of a registry key.
         */
        notify = 0x0010,
        /// KEY_QUERY_VALUE Required to query the values of a registry key.
        query_value = 0x0001,
        /*!
         * @brief KEY_READ Combines the STANDARD_RIGHTS_READ, KEY_QUERY_VALUE,
         * KEY_ENUMERATE_SUB_KEYS, and KEY_NOTIFY values.
         */
        read = 0x20019,
        /// KEY_SET_VALUE Required to create, delete, or set a registry value.
        set_value = 0x0002,
        /// KEY_WOW64_32KEY
        wow64key64 = 0x0100,
        /// KEY_WOW64_64KEY
        wow64key32 = 0x0200,
        /*!
         * @brief KEY_WRITE Combines the STANDARD_RIGHTS_WRITE, KEY_SET_VALUE,
         * and KEY_CREATE_SUB_KEY access rights.
         */
        write = 0x20006,
    };

    /*!
     * @brief The platform-specific view of the registry.
     * Passed to `RegDeleteKeyExW`.
     */
    enum class key_arch {
        /// Delete the key from the 32-bit registry view.
        key32bit = 0x0200,
        /// Delete the key from the 64-bit registry view.
        key64bit = 0x0100,
    };

    /*!
     * @brief The options used to open or create keys.
     * Passed to `RegCreateKeyExW`.
     */
    enum class options : uint32_t {
        /*!
         * @brief REG_OPTION_NON_VOLATILE This key is not volatile; this is the
         * default. The information is stored in a file and is preserved when
         * the system is restarted. The RegSaveKey function saves keys that are
         * not volatile.
         */
        default_options = 0,
        /*!
         * @brief REG_OPTION_VOLATILE All keys created by the function will be
         * volatile. The information is stored in memory and is not preserved
         * when the corresponding registry hive is unloaded. The RegSaveKey
         * function does not save volatile keys. This flag is ignored for keys
         * that already exist.
         */
        volatile_key = 1,
        /*!
         * @brief REG_OPTION_CREATE_LINK This key is a symbolic link. The target
         * path is assigned to the L"SymbolicLinkValue" value of the key. The
         * target path must be an absolute registry path.
         */
        create_link = 2,
        /*!
         * @brief REG_OPTION_BACKUP_RESTORE If this flag is set, the function
         * ignores the samDesired parameter and attempts to open the key with
         * the access required to backup or restore the key.
         */
        backup_restore = 4,
    };

    /*!
     * @brief The key creation disposition, that indicates
     * whether a new key was created, or an existing key was opened.
     * Returned from `RegCreateKeyExW`.
     */
    enum class disposition : uint32_t {
        /*!
         * @brief REG_CREATED_NEW_KEY The key did not exist and was created.
         */
        created_new = 1,
        /*!
         * @brief REG_OPENED_EXISTING_KEY The key existed and was simply
         * opened without being changed.
         */
        opened_existing = 2,
    };

    /*!
     * @brief Registry value types.
     */
    enum class value_type : uint32_t {
        /// REG_NONE No defined value type.
        none = 0,
        /// REG_SZ A null-terminated string.
        string = 1ul,
        /*!
         * @brief REG_EXPAND_SZ A null-terminated string that contains
         * unexpanded references to environment variables, for example, %PATH%.
         */
        expand_string = 2ul,
        /// REG_BINARY Binary data in any form.
        binary = 3ul,
        /// REG_DWORD A 32-bit number in little-endian format.
        dword = 4ul,
        /// REG_DWORD A 32-bit number in big-endian format.
        dword_big_endian = 5ul,
        /*!
         * @brief REG_LINK A null-terminated Unicode string that contains the
         * target path of a symbolic link that was created by calling the
         * `RegCreateKeyEx` function with `REG_OPTION_CREATE_LINK`.
         */
        link = 6ul,
        /// REG_MULTI_SZ A sequence of null-terminated strings, terminated by an
        /// empty string (\0).
        multi_string = 7ul,
        /// A 64-bit number in little-endian format.
        qword = 11ul,
    };

    /*!
     * @brief The format of the saved key or hive.
     */
    enum class save_format : unsigned long {
        /*!
         * @brief REG_STANDARD_FORMAT The standard format is the
         * only format supported by Windows 2000.
         */
        standard = 1,
        /*!
         * @brief REG_LATEST_FORMAT The latest format is supported starting with
         * Windows XP. After the key or hive is saved in this format, it cannot
         * be loaded on an earlier system.
         */
        latest = 2,
        /*!
         * @brief REG_NO_COMPRESSION The hive is saved with no compression, for
         * faster save operations.
         */
        no_compression = 4,
    };

    enum class security_info : unsigned long {
        owner = 0x00000001,
        group = 0x00000002,
        dacl = 0x00000004,
        sacl = 0x00000008,
    };

    RAD_OVERLOAD_ENUM_OPERATORS(access);

    RAD_OVERLOAD_ENUM_OPERATORS(options);

    RAD_OVERLOAD_ENUM_OPERATORS(save_format);

    RAD_OVERLOAD_ENUM_OPERATORS(security_info);

    /*!
     * @brief The registry key information returned by `RegQueryInfoKeyW`.
     */
    struct key_info {
        /// Count of sub keys.
        uint32_t sub_keys = 0;
        /*!
         * @brief The length of the sub key name
         * with longest name in unicode characters not including
         * the terminating null character.
         */
        uint32_t max_sub_key_name = 0;
        /// Count of values associated with the key.
        uint32_t values = 0;
        /*!
         * @brief The length of the value name
         * with longest name in unicode characters not including
         * the terminating null character.
         */
        uint32_t max_value_name = 0;
        /*!
         * @brief The length of the longest data
         * component among the key's values, in bytes.
         */
        uint32_t max_value_data = 0;
        /// The last time that the key or any of its value entries is modified.
        windows_clock::time_point last_write_time = {};
    };

    template <class HandleType>
    class key_base;
} // namespace RAD_LIB_NAMESPACE::winreg

namespace RAD_LIB_NAMESPACE::winreg::detail {

    class key_view_holder {
    public:
        key_view_holder(defined_keys k) noexcept : stored_key{cast_key(k)} {
        }

        key_view_holder(void* k) noexcept : stored_key{cast_key(k)} {
        }

        HKEY get() const {
            return stored_key;
        }

        void reset() const noexcept {
        }

        void reset(HKEY) const noexcept {
        }

        explicit operator bool() const noexcept {
            return true;
        }

    private:
        static HKEY cast_key(defined_keys k) {
            return reinterpret_cast<HKEY>(k);
        }

        static HKEY cast_key(void* k) {
            return static_cast<HKEY>(k);
        }

        HKEY stored_key;
    };

    RAD_EXPORT_DECL HKEY create_key(HKEY base_key, wzstring_view sub_key,
                                    options opts, access access,
                                    disposition& disposition, void* security);

    RAD_EXPORT_DECL void close_handle(HKEY h) noexcept;

    RAD_EXPORT_DECL HKEY open_key(HKEY base_key, wzstring_view sub_key,
                                  bool symbolic_link, access access);

    RAD_EXPORT_DECL void set_dword_value(HKEY k, wzstring_view name,
                                         uint32_t val);

    RAD_EXPORT_DECL void set_qword_value(HKEY k, wzstring_view name,
                                         uint64_t val);

    RAD_EXPORT_DECL void set_bin_value(HKEY k, wzstring_view name,
                                       const_buffer val);

    RAD_EXPORT_DECL void set_none_value(HKEY k, wzstring_view name,
                                        const_buffer val);

    RAD_EXPORT_DECL void set_string_value(HKEY k, wzstring_view name,
                                          wzstring_view val,
                                          bool expand = false);

    RAD_EXPORT_DECL void set_multi_string(HKEY k, wzstring_view name,
                                          std::span<const std::wstring> val);

    RAD_EXPORT_DECL value_type get_value_type(HKEY k, wzstring_view name);

    RAD_EXPORT_DECL key_info get_key_info(HKEY k);

    RAD_EXPORT_DECL uint32_t get_dword_value(HKEY k, wzstring_view name,
                                             value_type& type,
                                             bool allow_conversion);

    RAD_EXPORT_DECL uint64_t get_qword_value(HKEY k, wzstring_view name,
                                             value_type& type,
                                             bool allow_conversion);

    RAD_EXPORT_DECL void get_binary_none_value(HKEY k, wzstring_view name,
                                               dynamic_buffer buff, bool none,
                                               value_type& type,
                                               bool allow_conversion);

    inline std::vector<uint8_t>
    get_binary_none_value(HKEY k, wzstring_view name, bool none,
                          value_type& type, bool allow_conversion) {
        std::vector<uint8_t> buff;
        get_binary_none_value(k, name, dynamic_buffer(buff), none, type,
                              allow_conversion);
        return buff;
    }

    RAD_EXPORT_DECL void get_string_value(HKEY k, wzstring_view name,
                                          std::wstring& str, value_type& type,
                                          bool allow_conversion);

    inline std::wstring get_string_value(HKEY k, wzstring_view name,
                                         value_type& type,
                                         bool allow_conversion) {
        std::wstring str;
        get_string_value(k, name, str, type, allow_conversion);
        return str;
    }

    RAD_EXPORT_DECL void get_expand_string_value(HKEY k, wzstring_view name,
                                                 std::wstring& str,
                                                 value_type& type, bool expand,
                                                 bool allow_conversion);

    inline std::wstring get_expand_string_value(HKEY k, wzstring_view name,
                                                value_type& type, bool expand,
                                                bool allow_conversion) {
        std::wstring str;
        get_expand_string_value(k, name, str, type, expand, allow_conversion);
        return str;
    }

    RAD_EXPORT_DECL void get_multi_string_value(HKEY k, wzstring_view name,
                                                std::vector<std::wstring>& strs,
                                                value_type& type,
                                                bool allow_conversion);

    inline std::vector<std::wstring>
    get_multi_string_value(HKEY k, wzstring_view name, value_type& type,
                           bool allow_conversion) {
        std::vector<std::wstring> strs;
        get_multi_string_value(k, name, strs, type, allow_conversion);
        return strs;
    }

    RAD_EXPORT_DECL bool fetch_sub_key(HKEY k, uint32_t index, wchar_t* name,
                                       uint32_t& len);

    RAD_EXPORT_DECL bool fetch_value_type(HKEY k, uint32_t index,
                                          std::wstring& name, value_type& type);

    RAD_EXPORT_DECL void delete_subkey(HKEY k, wzstring_view sub_key);

    RAD_EXPORT_DECL void delete_subkey(HKEY k, wzstring_view sub_key,
                                       key_arch arch);

    RAD_EXPORT_DECL void delete_value(HKEY k, wzstring_view name);

    RAD_EXPORT_DECL void delete_value(HKEY k, wzstring_view sub_key,
                                      wzstring_view name);

    RAD_EXPORT_DECL void delete_subkeys(HKEY k, wzstring_view sub_key);

    RAD_EXPORT_DECL void save_key_to_file(HKEY k, wzstring_view file_path,
                                          save_format format);

    RAD_EXPORT_DECL HKEY open_current_user(access a);

    struct iterator_end_mark {};

    template <class HandleType, bool Utf8>
    class subkeys_ietrator {
        static constexpr uint32_t max_key_size = 255;

    public:
        using deref_type =
            std::conditional_t<Utf8, std::string, std::wstring_view>;

        subkeys_ietrator(const key_base<HandleType>& key) : key{key} {
            fetch_next();
        }

        deref_type operator*() const {
            if constexpr (!Utf8) {
                return {name, name_len};
            }
            else {
                return to_string(std::wstring_view{name, name_len});
            }
        }

        subkeys_ietrator& operator++() {
            fetch_next();
            return *this;
        }

        bool operator!=(iterator_end_mark) {
            return !finished;
        }

        bool operator==(iterator_end_mark) {
            return finished;
        }

    private:
        const key_base<HandleType>& key;
        uint32_t index = 0;
        uint32_t name_len = 0;
        wchar_t name[max_key_size + 1];
        bool finished = false;

        void fetch_next() {
            name_len = max_key_size;
            finished = key.fetch_subkey(index++, name, name_len);
        }
    };

    template <class HandleType, bool Utf8>
    class subkeys_range {
    public:
        subkeys_range(const key_base<HandleType>& key) noexcept : key{key} {
        }

        auto begin() const {
            return subkeys_ietrator<HandleType, Utf8>{key};
        }

        auto end() const noexcept {
            return iterator_end_mark{};
        }

    private:
        const key_base<HandleType>& key;
    };

    template <class HandleType, bool Utf8>
    class value_type_iterator {
    public:
        value_type_iterator(const key_base<HandleType>& key) : key{key} {
            auto info = key.key_info();
            name.reserve(info.max_value_name);
            fetch_next();
        }

        using str_type =
            std::conditional_t<Utf8, std::string, std::wstring_view>;
        using deref_type = std::pair<str_type, value_type>;

        deref_type operator*() const {
            std::wstring_view v = name;
            if constexpr (!Utf8) {
                return std::make_pair(v, type);
            }
            else {
                return std::make_pair(to_string(v), type);
            }
        }

        value_type_iterator& operator++() {
            fetch_next();
            return *this;
        }

        bool operator!=(iterator_end_mark) {
            return !finished;
        }

        bool operator==(iterator_end_mark) {
            return finished;
        }

    private:
        const key_base<HandleType>& key;
        uint32_t index = 0;
        bool finished = false;
        std::wstring name;
        value_type type = value_type::none;

        void fetch_next() {
            finished = key.fetch_value_type(index++, name, type);
        }
    };

    template <class HandleType, bool Utf8>
    class value_type_range {
    public:
        value_type_range(const key_base<HandleType>& key) : key{key} {
        }

        auto begin() const {
            return value_type_iterator<HandleType, Utf8>{key};
        }

        auto end() const {
            return iterator_end_mark{};
        }

    private:
        const key_base<HandleType>& key;
    };
} // namespace RAD_LIB_NAMESPACE::winreg::detail

namespace RAD_LIB_NAMESPACE::winreg {
    /*!
     * @brief RAII Wrapper for the native registry owned key HKEY.
     */
    using registry_key_handle = std::unique_ptr<
        void,
        os::handle_deleter<HKEY, // underlying type that stores the handle
                           detail::close_handle>>; // the cleaner function

    /*!
     * @brief Registry key.
     * Users should use `key` for opend and created keys,
     * or `key_view` for predefined keys instead.
     * @tparam HandleType Type of key handle.
     * Must be either 1registry_key_handle`, or `detail::key_view_holder`.
     */
    template <class HandleType>
    class key_base {
        template <typename, bool>
        friend class detail::subkeys_ietrator;
        template <typename, bool>
        friend class detail::value_type_iterator;

    public:
        /*!
         * @brief The type of wrapper handle used by the key.
         */
        using native_handle_type = HandleType;

        /*!
         * @brief Construct a closed key.
         */
        key_base() = default;

        /*!
         * @brief Construct a key from a RAII owned native key wrapper.
         * The key will be open if @p handle is open.
         * After construction, @p handle will have no key.
         * @param handle The owned native key wrapper to take its key.
         */
        key_base(native_handle_type handle) noexcept
            : handle_{std::move(handle)} {
        }

        /*!
         * @brief Close the key if it is open.
         * This has no effect if the key is already closed,
         * or if it is a predefined key.
         */
        void close() noexcept {
            handle_.reset();
        }

        /*!
         * @brief Check if the key is open.
         * @return True if the key is open, otherwise false.
         */
        bool is_valid() const noexcept {
            return static_cast<bool>(handle_);
        }

        /*!
         * @brief Check if the key is open.
         * @return True if the key is open, otherwise false.
         */
        explicit operator bool() const {
            return is_valid();
        }

        /*!
         * @brief Get a reference to the wrapper handle used by the key.
         * @return A reference to the wrapper handle used by the key.
         */
        native_handle_type& native_handle() noexcept {
            return handle_;
        }

        /*!
         * @brief Get a const reference to the wrapper handle used by the key.
         * @return A const reference to the wrapper handle used by the key.
         */
        const native_handle_type& native_handle() const noexcept {
            return handle_;
        }

        /*!
         * @brief Create or open a sub key and on success, replace the
         * existing key with it.
         *
         * The current base key must be open or an exception will be thrown.
         *
         * This method is not supported for the predefined keys since they
         * can't be replaced. Use `create_subkey` method instead.
         * @param sub_key The sub key name.
         * @param opts The options used to open or create the key.
         * @param access A mask that specifies the desired access rights to the
         * key.
         * @param disposition On success it will indicate whether a new
         * key was created, or an existing key was opened.
         * @param security A pointer to a `SECURITY_ATTRIBUTES` structure that
         * determines whether the returned handle can be inherited by child
         * processes. If @p security is null, the handle cannot be inherited.
         */
        template <class StringType>
        void create(const StringType& sub_key, options opts, access access,
                    disposition& disposition, void* security = nullptr)
            requires std::is_same_v<HandleType, registry_key_handle>
        {
            auto hkey = detail::create_key(native_handle().get(),
                                           os::get_wstring(sub_key), opts,
                                           access, disposition, security);
            native_handle().reset(hkey);
        }

        /*!
         * @brief Create or open a sub key and on success, replace the
         * existing key with it.
         *
         * The current base key must be open or an exception will be thrown.
         *
         * This method is not supported for the predefined keys since they
         * can't be replaced. Use `create_subkey` method instead.
         * @param sub_key The sub key name.
         * @param opts The options used to open or create the key.
         * @param access A mask that specifies the desired access rights to the
         * key.
         */
        template <class StringType>
        void create(const StringType& sub_key, options opts, access access) {
            disposition disposition{};
            return create(sub_key, opts, opts, disposition);
        }

        /*!
         * @brief Create or open a sub key and on success, replace the
         * existing key with it.
         *
         * The current base key must be open or an exception will be thrown.
         *
         * This method is not supported for the predefined keys since they
         * can't be replaced. Use `create_subkey` method instead.
         * @param sub_key The sub key name.
         * @param access A mask that specifies the desired access rights to the
         * key.
         */
        template <class StringType>
        void create(const StringType& sub_key, access access) {
            disposition disposition{};
            return create(sub_key, options::default_options, access,
                          disposition);
        }

        /*!
         * @brief Create or open a sub key and return it.
         *
         * The current base key must be open or an exception will be thrown.
         * @param sub_key The sub key name.
         * If the sub key name is empty and this key is a predefined key,
         * then an empty key is returned.
         * @param opts The options used to open or create the key.
         * @param access A mask that specifies the desired access rights to the
         * key.
         * @param disposition On success it will indicate whether a new
         * key was created, or an existing key was opened.
         * @param security A pointer to a `SECURITY_ATTRIBUTES` structure that
         * determines whether the returned handle can be inherited by child
         * processes. If @p security is null, the handle cannot be inherited.
         * @return The created or opened sub key.
         */
        template <class StringType>
        key_base<registry_key_handle>
        create_subkey(const StringType& sub_key, options opts, access access,
                      disposition& disposition,
                      void* security = nullptr) const {
            auto hkey = detail::create_key(native_handle().get(),
                                           os::get_wstring(sub_key), opts,
                                           access, disposition, security);
            return {registry_key_handle{hkey}};
        }

        /*!
         * @brief Create or open a sub key and return it.
         *
         * The current base key must be open or an exception will be thrown.
         * @param sub_key The sub key name.
         * If the sub key name is empty and this key is a predefined key,
         * then an empty key is returned.
         * @param opts The options used to open or create the key.
         * @param access A mask that specifies the desired access rights to the
         * key.
         * @return The created or opened sub key.
         */
        template <class StringType>
        key_base<registry_key_handle> create_subkey(const StringType& sub_key,
                                                    options opts,
                                                    access access) const {
            disposition disposition{};
            return create_subkey(sub_key, opts, access, disposition);
        }

        /*!
         * @brief Create or open a sub key and return it.
         *
         * The current base key must be open or an exception will be thrown.
         * @param sub_key The sub key name.
         * If the sub key name is empty and this key is a predefined key,
         * then an empty key is returned.
         * @param access A mask that specifies the desired access rights to the
         * key.
         * @return The created or opened sub key.
         */
        template <class StringType>
        key_base<registry_key_handle> create_subkey(const StringType& sub_key,
                                                    access access) const {
            disposition disposition{};
            return create_subkey(sub_key, options::default_options, access,
                                 disposition);
        }

        /*!
         * @brief Open an existing sub key and return it.
         *
         * The current base key must be open or an exception will be thrown.
         * @param sub_key The sub key name.
         * If the sub key name is empty and this key is a predefined key,
         * then the system refreshes the predefined key and an empty key
         * is returned.
         * @param access A mask that specifies the desired access rights to the
         * key.
         * @param symbolic_link True if the key is a symbolic link.
         * @return The opened key.
         */
        template <class StringType>
        key_base<registry_key_handle> open(const StringType& sub_key,
                                           access access = access::all,
                                           bool symbolic_link = false) const {
            auto hkey = detail::open_key(native_handle().get(),
                                         os::get_wstring(sub_key),
                                         symbolic_link, access);
            return {registry_key_handle{hkey}};
        }
        /*!
         * @brief Set 32 bit little endian data and type of a
         * specified value under a registry key.
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The 32 bit little endian data.
         */
        template <class StringType>
        void dword_value(const StringType& value_name, uint32_t data) {
            detail::set_dword_value(native_handle().get(),
                                    os::get_wstring(value_name), data);
        }

        /*!
         * @brief Set 64 bit little endian data and type of a
         * specified value under a registry key.
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The 64 bit little endian data.
         */
        template <class StringType>
        void qword_value(const StringType& value_name, uint64_t data) {
            detail::set_qword_value(native_handle().get(),
                                    os::get_wstring(value_name), data);
        }

        /*!
         * @brief Set null terminated string data and type of a
         * specified value under this registry key.
         *
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The null terminated string data.
         */
        template <class StringType, class StringType2>
        void string_value(const StringType& value_name,
                          const StringType2& data) {
            detail::set_string_value(native_handle().get(),
                                     os::get_wstring(value_name),
                                     os::get_wstring(data));
        }

        /*!
         * @brief Set null terminated string data and type of a
         * specified value under this registry key.
         *
         * If the string @p data contains environment variables, they
         * will be expanded on read.
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The null terminated unexpanded string data.
         */
        template <class StringType, class StringType2>
        void expanded_string_value(const StringType& value_name,
                                   const StringType2& data) {
            detail::set_string_value(native_handle().get(),
                                     os::get_wstring(value_name),
                                     os::get_wstring(data), true);
        }

        /*!
         * @brief Set multi null terminated strings data and type of a
         * specified value under this registry key.
         *
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The multi null terminated strings data.
         */
        template <class StringType>
        void multi_string_value(const StringType& value_name,
                                const std::vector<std::wstring>& data) {
            detail::set_multi_string(native_handle().get(),
                                     os::get_wstring(value_name), data);
        }

        /*!
         * @brief Set binary data and type of a specified value under this
         * registry key.
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The binary data.
         */
        template <class StringType>
        void binary_value(const StringType& value_name, const_buffer data) {
            detail::set_bin_value(native_handle().get(),
                                  os::get_wstring(value_name), data);
        }

        /*!
         * @brief Set none data and type of a specified value under this
         * registry key.
         * @param value_name The name of the value to be set.
         * If a value with this name is not already present in the key,
         * the function adds it to the key.
         *
         * If @p value_name is empty, the function sets the type and data
         * for the key's unnamed or default value.
         * @param data The none data.
         */
        template <class StringType>
        void none_value(const StringType& value_name, const_buffer data) {
            detail::set_none_value(native_handle().get(),
                                   os::get_wstring(value_name), data);
        }

        /*!
         * @brief Get the type of a specified value under this registry key.
         * @param value_name The name of the value to get its type.
         *
         * If @p value_name is empty, the function retrieves the type
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @return The type of the value.
         */
        template <class StringType>
        winreg::value_type value_type(const StringType& value_name) const {
            return detail::get_value_type(native_handle().get(),
                                          os::get_wstring(value_name));
        }

        /*!
         * @brief Get the 32 bit little endian data of a specified value under
         * this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The 32 bit little endian data.
         */
        template <class StringType>
        uint32_t dword_value(const StringType& value_name,
                             bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_dword_value(native_handle().get(),
                                           os::get_wstring(value_name), type,
                                           allow_conversion);
        }

        /*!
         * @brief Get the 64 bit little endian data of a specified value under
         * this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The 64 bit little endian data.
         */
        template <class StringType>
        uint64_t qword_value(const StringType& value_name,
                             bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_qword_value(native_handle().get(),
                                           os::get_wstring(value_name), type,
                                           allow_conversion);
        }

        /*!
         * @brief Get the wide string UTF-16 data of a specified value under
         * this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The wide string UTF-16 data.
         */
        template <class StringType>
        std::wstring wstring_value(const StringType& value_name,
                                   bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_string_value(native_handle().get(),
                                            os::get_wstring(value_name), type,
                                            allow_conversion);
        }

        /*!
         * @brief Get the string UTF-8 data of a specified value under
         * this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The string UTF-8 data.
         */
        template <class StringType>
        std::string string_value(const StringType& value_name,
                                 bool allow_conversion = false) const {
            return to_string(wstring_value(value_name, allow_conversion));
        }

        /*!
         * @brief Get the wide string UTF-16 data of a specified value under
         * this registry key, and optionally expand environment variables
         * in the retrieved data.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param expand Whether to expan environment variables in the
         * retrieved data or not.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The wide string UTF-16 data, with environment variables
         * expanded if @p expand is true.
         */
        template <class StringType>
        std::wstring
        expanded_wstring_value(const StringType& value_name, bool expand = true,
                               bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_expand_string_value(
                native_handle().get(), os::get_wstring(value_name), type,
                expand, allow_conversion);
        }

        /*!
         * @brief Get the string UTF-8 data of a specified value under
         * this registry key, and optionally expand environment variables
         * in the retrieved data.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param expand Whether to expan environment variables in the
         * retrieved data or not.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The string UTF-8 data, with environment variables
         * expanded if @p expand is true.
         */
        template <class StringType>
        std::string expanded_string_value(const StringType& value_name,
                                          bool expand = true,
                                          bool allow_conversion = false) const {
            return to_string(
                expanded_wstring_value(value_name, expand, allow_conversion));
        }

        /*!
         * @brief Get the UTF-16 wide multi strings data of a specified value
         * under this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The UTF-16 wide multi strings data.
         */
        template <class StringType>
        std::vector<std::wstring>
        multi_wstring_value(const StringType& value_name,
                            bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_multi_string_value(native_handle().get(),
                                                  os::get_wstring(value_name),
                                                  type, allow_conversion);
        }

        /*!
         * @brief Get the raw binary data of a specified value under
         * this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The raw binary data.
         */
        template <class StringType>
        std::vector<uint8_t> binary_value(const StringType& value_name,
                                          bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_binary_none_value(native_handle().get(),
                                                 os::get_wstring(value_name),
                                                 false, type, allow_conversion);
        }

        /*!
         * @brief Get the raw none data of a specified value under
         * this registry key.
         * @param value_name The name of the value to get its data.
         *
         * If @p value_name is empty, the function retrieves the data
         * for the key's unnamed or default value, if any.
         *
         * If @p value_name specifies a value that is not in the registry,
         * an exception is thrown.
         * @param allow_conversion True to attempt to convert the data if
         * the type of the value is not the same as requested type.
         *
         * If @p allow_conversion is false and the type of the value differs, or
         * the retrieved data is not convertible to the required type, an
         * exception is thrown.
         * @return The raw none data.
         */
        template <class StringType>
        std::vector<uint8_t> none_value(const StringType& value_name,
                                        bool allow_conversion = false) const {
            winreg::value_type type{};
            return detail::get_binary_none_value(native_handle().get(),
                                                 os::get_wstring(value_name),
                                                 true, type, allow_conversion);
        }

        /*!
         * @brief Get the key informations.
         * @return The key informations.
         */
        winreg::key_info key_info() const {
            return detail::get_key_info(native_handle().get());
        }

        /*!
         * @brief Delete a subkey and its values from the registry.
         * The subkey to be deleted must not have subkeys.
         * To delete keys recursively, use the `delete_subkeys` method.
         * @param sub_key The sub key name.
         */
        template <class StringType>
        void delete_subkey(const StringType& sub_key) {
            detail::delete_subkey(native_handle().get(),
                                  os::get_wstring(sub_key));
        }

        /*!
         * @brief Delete a subkey and its values from the specified
         * platform-specific view of the registry.
         * The subkey to be deleted must not have subkeys.
         * To delete keys recursively, use the `delete_subkeys` method.
         * @param sub_key The sub key name.
         * @param arch The platform-specific view of the registry.
         */
        template <class StringType>
        void delete_subkey(const StringType& sub_key, key_arch arch) {
            detail::delete_subkey(native_handle().get(),
                                  os::get_wstring(sub_key), arch);
        }

        /*!
         * @brief Delete the subkeys and values of the specified key
         * recursively, then delete the key at @p sub_key.
         * @param sub_key The sub key name.
         */
        template <class StringType>
        void delete_subkeys(const StringType& sub_key) {
            detail::delete_subkeys(native_handle().get(), sub_key);
        }

        /*!
         * @brief Delete the subkeys and values of this key recursively.
         * This key itself is not deleted.
         */
        void delete_subkeys() {
            detail::delete_subkeys(native_handle().get(), {});
        }

        /*!
         * @brief Delete a specified value under this registry key.
         * @param value_name The name of the value to delete.
         * If @p value_name is empty, the function deletes the key's unnamed or
         * default value, if any.
         */
        template <class StringType>
        void delete_value(const StringType& value_name) {
            detail::delete_value(native_handle().get(),
                                 os::get_wstring(value_name));
        }

        /*!
         * @brief Delete a specified value under a specified registry sub key.
         * @param sub_key The name of the sub key that contains the value.
         * @param value_name The name of the value to delete.
         */
        template <class StringType, class StringType2>
        void delete_value_from_subkey(const StringType& sub_key,
                                      const StringType2& value_name) {
            detail::delete_value(native_handle().get(),
                                 os::get_wstring(sub_key),
                                 os::get_wstring(value_name));
        }

        /*!
         * @brief Enumerate the sub keys names of this registry key.
         * The returned sub keys names are UTF-16 wide strings.
         * The names are enumerated lazily.
         * @return A range type to enumerate the sub keys names.
         */
        detail::subkeys_range<HandleType, false> enum_subkeys() const {
            return {*this};
        }

        /*!
         * @brief Enumerate the sub keys names of this registry key.
         * The returned sub keys names are UTF-8 strings.
         * The names are enumerated lazily.
         * @return A range type to enumerate the sub keys names.
         */
        detail::subkeys_range<HandleType, true> enum_subkeys_utf8() const {
            return {*this};
        }

        /*!
         * @brief Enumerate the values names of this registry key.
         * The returned values names are UTF-16 wide strings.
         * The names are enumerated lazily.
         * @return A range type to enumerate the values names.
         */
        detail::value_type_range<HandleType, false> enum_values() const {
            return {*this};
        }

        /*!
         * @brief Enumerate the values names of this registry key.
         * The returned values names are UTF-8 strings.
         * The names are enumerated lazily.
         * @return A range type to enumerate the values names.
         */
        detail::value_type_range<HandleType, true> enum_values_utf8() const {
            return {*this};
        }

        /*!
         * @brief Save this key and all of its subkeys and values
         * to a registry file, in the specified format.
         * @param file_path The path of the file in which the specified
         * key and subkeys are to be saved. If the file already exists,
         * the function fails.
         *
         * The new file has the archive attribute.
         *
         * If the string does not include a path, the file is created
         * in the current directory of the calling process for a local key,
         * or in the %systemroot%\system32 directory for a remote key.
         * @param format The format of the saved key or hive.
         */
        template <class StringType>
        void save_to_file(const StringType& file_path,
                          save_format format = save_format::standard) {
            detail::save_key_to_file(native_handle().get(),
                                     os::get_wstring(file_path), format);
        }

    private:
        bool fetch_subkey(uint32_t index, wchar_t* Name, uint32_t& Len) const {
            return detail::fetch_sub_key(native_handle().get(), index, Name,
                                         Len);
        }

        bool fetch_value_type(uint32_t index, std::wstring& Name,
                              winreg::value_type& Type) const {
            return detail::fetch_value_type(native_handle().get(), index, Name,
                                            Type);
        }

        native_handle_type handle_;
    };

    /*!
     * @brief Registry predefined key.
     * This key can't be closed, opened or created.
     *
     * The predefined keys provided by the library:
     * `classes_root`, `current_config`, `current_user`,
     * `current_user_local_settings`, `local_machine`, `performance_data`,
     * `performance_text`, `performance_nlstext` and `users`.
     */
    using key_view = key_base<detail::key_view_holder>;

    /*!
     * @brief Registry owned key.
     * This key can be closed, opened and created.
     */
    using key = key_base<registry_key_handle>;

    /*!
     * @brief Retrieves a handle to the HKEY_CURRENT_USER key for
     * the user the current thread is impersonating.
     * @param access A mask that specifies the desired access rights to the key.
     * @return A key to the HKEY_CURRENT_USER key for
     * the user the current thread is impersonating.
     */
    inline key open_current_user(access access) {
        HKEY hkey = detail::open_current_user(access);
        return key{registry_key_handle{hkey}};
    }
}; // namespace RAD_LIB_NAMESPACE::winreg

namespace RAD_LIB_NAMESPACE::winreg::detail {
    inline auto get_predefined_key(defined_keys k) {
        return key_view{k};
    }
} // namespace RAD_LIB_NAMESPACE::winreg::detail

namespace RAD_LIB_NAMESPACE::winreg {
    /*!
     * @brief HKEY_CLASSES_ROOT predefined key.
     */
    inline auto classes_root =
        detail::get_predefined_key(defined_keys::classes_root);
    /*!
     * @brief HKEY_CURRENT_CONFIG predefined key.
     */
    inline auto current_config =
        detail::get_predefined_key(defined_keys::current_config);
    /*!
     * @brief HKEY_CURRENT_USER predefined key.
     */
    inline auto current_user =
        detail::get_predefined_key(defined_keys::current_user);
    /*!
     * @brief HKEY_CURRENT_USER_LOCAL_SETTINGS predefined key.
     */
    inline auto current_user_local_settings =
        detail::get_predefined_key(defined_keys::current_user_local_settings);
    /*!
     * @brief HKEY_LOCAL_MACHINE predefined key.
     */
    inline auto local_machine =
        detail::get_predefined_key(defined_keys::local_machine);
    /*!
     * @brief HKEY_PERFORMANCE_DATA predefined key.
     */
    inline auto performance_data =
        detail::get_predefined_key(defined_keys::performance_data);
    /*!
     * @brief HKEY_PERFORMANCE_TEXT predefined key.
     */
    inline auto performance_text =
        detail::get_predefined_key(defined_keys::performance_text);
    /*!
     * @brief HKEY_PERFORMANCE_NLSTEXT predefined key.
     */
    inline auto performance_nlstext =
        detail::get_predefined_key(defined_keys::performance_nlstext);
    /*!
     * @brief HKEY_USERS predefined key.
     */
    inline auto users = detail::get_predefined_key(defined_keys::users);
} // namespace RAD_LIB_NAMESPACE::winreg
