#include <rad/cli.h>

#include <algorithm>
#include <cassert>

using namespace RAD_LIB_NAMESPACE;
using namespace cli;

namespace {
    enum class cli_error_code : int {
        no_error = 0,
        invalid_option_name,
        valueless_option,
        reject_multiple_values,
        value_parse_error,
        unkown_parameter,
        need_param,
        invalid_param,
        required_not_set,
    };

    constexpr std::string_view po_error_messages[][2] = {
        {"No error", ""},
        {"Invalid option name '", "'"},
        {"Option '", "' does not take a parameter"},
        {"Option '", "' does not take multiple parameters"},
        {"Failed to parse parameter of option '", "'"},
        {"Unkown parameter '", "'"},
        {"Option '", "' needs a parameter"},
        {"Invalid parameter '", "'"},
        {"Option '", "' is required"}};

    std::string make_error_message(int code, std::string_view opt_name) {
        if (code == 0) {
            return std::string{po_error_messages[0][0]};
        }
        std::string message;
        auto error_messages = po_error_messages[code];
        message.reserve(error_messages[0].size() + opt_name.size() +
                        error_messages[1].size());
        message += error_messages[0];
        message += opt_name;
        message += error_messages[1];
        return message;
    }

    template <cli_error_code code>
    [[noreturn]] void throw_exception(std::string_view opt_name) {
        throw std::runtime_error(
            make_error_message(static_cast<int>(code), opt_name));
    }

    enum arg_type {
        pos_arg,         // a, ab, abc, -
        short_opt,       // -a, -b, -c
        multi_short_opt, // -abc
        long_opt,        // --a, --b
        long_opt_eq,     // --a=1, --b=2
        terminate_opts,  // --
        invalid,         // a-b
    };

    arg_type parse_arg_opt(std::string_view arg, size_t& eq_pos) {
        assert(!arg.empty());

        if (arg[0] != '-') {
            return arg_type::pos_arg;
        }

        if (arg.size() == 1) { // -
            return arg_type::pos_arg;
        }

        if (arg[1] == '-') {
            if (arg.size() == 2) { // --
                return arg_type::terminate_opts;
            }

            eq_pos = arg.find('=');
            if (eq_pos != std::string_view::npos) { // --opt=1
                return arg_type::long_opt_eq;
            }

            return arg_type::long_opt; // --opt
        }

        if (arg.size() == 2) { // -a
            return arg_type::short_opt;
        }

        if (arg[2] == '-') { // -a-b
            return arg_type::invalid;
        }

        return arg_type::multi_short_opt; // -abc
    }

    class argc_argv_parser : public detail::cli::argv_reader {
    public:
        argc_argv_parser(int count, char** argv) noexcept
            : count{count}, argv{argv} {
        }

        virtual bool finished() const noexcept override {
            return count == 0;
        }

        virtual std::string_view read() override {
            --count;
            return argv[i++];
        }

    private:
        int count;
        int i = 0;
        char** argv;
    };

    class argc_wargv_parser : public detail::cli::argv_reader {
    public:
        argc_wargv_parser(int argc, wchar_t** argv) noexcept
            : count{argc}, argv{argv} {
            size_t max_len = 0;

            for (int i = 0; i < argc; ++i) {
                auto str = std::wstring_view(argv[i]);
                if (str.size() > max_len) {
                    max_len = str.size();
                }
            }

            token.reserve(max_len * sizeof(wchar_t));
        }

        virtual bool finished() const noexcept override {
            return count == 0;
        }

        virtual std::string_view read() override {
            --count;
            token.clear();
            to_string(argv[i++], token);
            return token;
        }

    private:
        int count;
        int i = 0;
        wchar_t** argv;
        std::string token;
    };

    template <class CharT>
    struct token_holder {
    protected:
        std::string token;

        std::string_view get_view() {
            return token;
        }
    };

    template <>
    struct token_holder<wchar_t> {
    protected:
        std::wstring token;

        std::string u8token;

        std::string_view get_view() {
            if (u8token.empty()) {
                to_string(token, u8token);
            }
            return u8token;
        }

    public:
        std::wstring_view wread() {
            return token;
        }
    };

    template <class CharT>
    struct win_line_parser final : public detail::cli::argv_reader,
                                   public token_holder<CharT> {
        using string_type = std::basic_string_view<CharT>;

        using iterator = typename string_type::iterator;

        using token_holder<CharT>::token;

        using token_holder<CharT>::get_view;

    public:
        win_line_parser(string_type line) : line{line} {
            token.reserve(line.size());
        }

        virtual bool finished() const noexcept override {
            return line.empty();
        }

        virtual std::string_view read() override {
            read_token();
            return get_view();
        }

        void read_only() {
            read_token();
        }

    private:
        void read_token();

        iterator read_backslashes(iterator it);

        iterator read_quotes(iterator it);

        string_type line;
    };

    template <class StrType>
    class span_parser : public detail::cli::argv_reader {
    public:
        span_parser(std::span<StrType> argv) noexcept : argv{argv} {
        }

        virtual bool finished() const noexcept override {
            return i == argv.size();
        }

        virtual std::string_view read() override {
            return argv[i++];
        }

    private:
        std::span<StrType> argv;
        size_t i = 0;
    };

    class vec_parser : public detail::cli::argv_reader {
    public:
        vec_parser(const std::vector<std::string>& argv) noexcept : argv{argv} {
        }

        virtual bool finished() const noexcept override {
            return i == argv.size();
        }

        virtual std::string_view read() override {
            return argv[i++];
        }

    private:
        const std::vector<std::string>& argv;
        size_t i = 0;
    };

    class wvec_parser : public detail::cli::argv_reader {
    public:
        wvec_parser(const std::vector<std::wstring>& argv) noexcept
            : argv{argv} {
            size_t max_len = 0;
            for (const auto& arg : argv) {
                if (arg.size() > max_len) {
                    max_len = arg.size();
                }
            }
            token.reserve(max_len * sizeof(wchar_t));
        }

        virtual bool finished() const noexcept override {
            return i == argv.size();
        }

        virtual std::string_view read() override {
            token.clear();
            to_string(argv[i++], token);
            return token;
        }

    private:
        const std::vector<std::wstring>& argv;
        std::string token;
        size_t i = 0;
    };
} // namespace

// "a b c" d e
// [a b c] [d] [e]

// "ab\"c" "\\" d
// [ab"c] [\] [d]

// a\\\b d"e f"g h
// [a\\\b] [de fg] [h]

// a\\\"b c d
// [a\"b] [c] [d]

// a\\\\"b c" d e
// [a\\b c] [d] [e]

// a"b"" c d
// [ab" c d]

template <class CharT>
void win_line_parser<CharT>::read_token() {
    token.clear();

    if (line.empty()) {
        return;
    }

    while (line[0] == static_cast<CharT>(' ')) {
        line.remove_prefix(1);
    }

    auto it = line.begin();
    for (; it != line.end(); ++it) {
        if (*it == static_cast<CharT>(' ')) {
            break;
        }

        else if (*it == static_cast<CharT>('\\')) {
            it = read_backslashes(it);
            if (it == line.end()) {
                break;
            }
            continue;
        }

        else if (*it == static_cast<CharT>('"')) {
            it = read_quotes(it);
            if (it == line.end()) {
                break;
            }
            continue;
        }

        else {
            token.push_back(*it);
        }
    }

    assert(line.begin() != it);
    line.remove_prefix(static_cast<size_t>(std::distance(line.begin(), it)));
}

template <class CharT>
auto win_line_parser<CharT>::read_backslashes(iterator it) -> iterator {
    assert(it != line.end());

    size_t slashes_count = 0;

    for (; it != line.end(); ++it) {
        if (*it == static_cast<CharT>('\\')) {
            ++slashes_count;
        }
        else if (*it == static_cast<CharT>('"')) {
            token.append(slashes_count / 2, static_cast<CharT>('\\'));
            if (slashes_count % 2 == 0) {
                it = read_quotes(it);
            }
            else {
                token.push_back(static_cast<CharT>('"'));
            }
            return it;
        }
        else if (*it == static_cast<CharT>(' ')) {
            token.append(slashes_count, static_cast<CharT>('\\'));
            --it;
            return it;
        }
        else {
            token.append(slashes_count, static_cast<CharT>('\\'));
            token.push_back(*it);
            return it;
        }
    }

    token.append(slashes_count, static_cast<CharT>('\\'));
    return it;
}

template <class CharT>
auto win_line_parser<CharT>::read_quotes(iterator it) -> iterator {
    if (it == line.end() || ++it == line.end()) {
        return it;
    }

    for (; it != line.end(); ++it) {
        if (*it == static_cast<CharT>('"')) {
            ++it;
            if (it == line.end() || *it == static_cast<CharT>(' ') ||
                *it != static_cast<CharT>('"')) {
                --it;
                break;
            }
            token.push_back(static_cast<CharT>('"'));
        }
        else if (*it == static_cast<CharT>('\\')) {
            ++it;
            if (it == line.end()) {
                break;
            }
            if (*it == static_cast<CharT>('\\')) {
                token.push_back(static_cast<CharT>('\\'));
            }
            else if (*it == static_cast<CharT>('"')) {
                token.push_back(static_cast<CharT>('"'));
            }
        }
        else {
            token.push_back(*it);
        }
    }

    return it;
}

void cli::option::assign_name(std::string_view name) {
    if (name.empty() || name.front() == ',') {
        throw_exception<cli_error_code::invalid_option_name>(name);
    }

    if (name.size() == 1) {
        short_name_ = name.front();
        long_name_ = name;
    }
    else if (name[name.size() - 2] == ',') {
        if (contains(name.substr(0, name.size() - 2), ',')) {
            throw_exception<cli_error_code::invalid_option_name>(name);
        }
        short_name_ = name.back();
        long_name_ = name.substr(0, name.size() - 2);
    }
    else {
        long_name_ = name;
    }
}

void option::parse(std::string_view text, bool use_short) {
    auto val = get_value();
    if (!val) {
        throw_exception<cli_error_code::valueless_option>(get_name(use_short));
    }
    if (parsed_ && !val->many_values()) {
        throw_exception<cli_error_code::reject_multiple_values>(
            get_name(use_short));
    }
    if (!val->parse(text)) {
        throw_exception<cli_error_code::value_parse_error>(get_name(use_short));
    }
    parsed_ = true;
}

void positional::validate_name_repeat() {
    if (name_.empty()) {
        throw_exception<cli_error_code::invalid_option_name>(name_);
    }

    if (repeat_ > 1 && !get_value().many_values()) {
        throw_exception<cli_error_code::reject_multiple_values>(name_);
    }
}

void positional::parse(std::string_view text) {
    auto& val = get_value();
    if (!val.parse(text)) {
        throw_exception<cli_error_code::value_parse_error>(name());
    }
    ++parsed_;
}

void parser::parse(int argc, char** argv) {
    argc_argv_parser p{argc, argv};
    do_parse(p);
}

void parser::parse(int argc, wchar_t** argv) {
    argc_wargv_parser p{argc, argv};
    do_parse(p);
}

void parser::parse(std::span<const std::string> argv) {
    span_parser p{argv};
    do_parse(p);
}

void parser::parse(std::span<const std::string_view> argv) {
    span_parser p{argv};
    do_parse(p);
}

void parser::parse(std::string_view cmd_line) {
    win_line_parser p{cmd_line};
    do_parse(p);
}

void parser::parse(std::wstring_view cmd_line) {
    win_line_parser p{cmd_line};
    do_parse(p);
}

std::size_t parser::count_options(std::string_view long_name) const noexcept {
    size_t n = 0;
    for (const auto& opt : opts) {
        if (opt.parsed_ && opt.long_name() == long_name) {
            ++n;
        }
    }
    return n;
}

std::size_t parser::count_options(char short_name) const noexcept {
    if (short_name == -1) {
        return 0;
    }

    size_t n = 0;
    for (const auto& opt : opts) {
        if (opt.parsed_ && opt.short_name() == short_name) {
            ++n;
        }
    }
    return n;
}

std::size_t parser::count_positionals(std::string_view name) const noexcept {
    size_t n = 0;
    for (const auto& pos : poses) {
        if (pos.parsed_ && pos.name() == name) {
            ++n;
        }
    }
    return n;
}

std::size_t parser::count(std::string_view name) const noexcept {
    return count_options(name) + count_positionals(name);
}

bool parser::has_option(std::string_view long_name) const noexcept {
    return std::find_if(opts.begin(), opts.end(),
                        [long_name](const option& opt) {
                            return opt.parsed_ && opt.long_name() == long_name;
                        }) != opts.end();
}

bool parser::has_option(char short_name) const noexcept {
    if (short_name == -1) {
        return false;
    }

    return std::find_if(
               opts.begin(), opts.end(), [short_name](const option& opt) {
                   return opt.parsed_ && opt.short_name() == short_name;
               }) != opts.end();
}

bool parser::has_positional(std::string_view name) const noexcept {
    return std::find_if(poses.begin(), poses.end(),
                        [name](const positional& pos) {
                            return pos.parsed_ != 0 && pos.name() == name;
                        }) != poses.end();
}

bool parser::has(std::string_view name) const noexcept {
    return has_option(name) || has_positional(name);
}

void parser::do_parse(argv_reader& reader) {
    if (opts.empty() && poses.empty()) {
        return;
    }
    for (auto& opt : opts) {
        opt.reset();
    }
    for (auto& pos : poses) {
        pos.reset();
    }

    std::string_view arg;
    size_t eq_pos = 0;

    while (!reader.finished()) {
        arg = reader.read();

        if (arg.empty()) {
            continue;
        }

        switch (parse_arg_opt(arg, eq_pos)) {
        case arg_type::pos_arg:
            parse_positional(arg);
            break;

        case arg_type::short_opt:
            parse_short(arg[1], reader, true);
            break;

        case arg_type::multi_short_opt:
            arg.remove_prefix(1);
            parse_multi_short(arg, reader);
            break;

        case arg_type::long_opt:
            arg.remove_prefix(2);
            parse_long(arg, reader);
            break;

        case arg_type::long_opt_eq:
            eq_pos -= 2;
            arg.remove_prefix(2);
            parse_long_eq(arg, eq_pos);
            break;

        case arg_type::terminate_opts:
            while (!reader.finished()) {
                parse_positional(reader.read());
            }
            break;

        case arg_type::invalid:
            throw_exception<cli_error_code::invalid_param>(arg);
            break;

        default:
            break;
        }
    }

    if (!opts.empty()) {
        auto it = std::find_if(opts.begin(), opts.end(), [](const option& opt) {
            return !opt.is_optional() && !opt.parsed_;
        });
        if (it != opts.end()) {
            throw_exception<cli_error_code::required_not_set>(it->long_name());
        }
    }

    if (!poses.empty()) {
        auto it =
            std::find_if(poses.begin(), poses.end(), [](const positional& pos) {
                return !pos.optional_ && !pos.parsed_;
            });
        if (it != poses.end()) {
            throw_exception<cli_error_code::required_not_set>(it->name());
        }
    }
}

void parser::parse_positional(std::string_view arg) {
    auto it =
        std::find_if(poses.begin(), poses.end(),
                     [](const positional& pos) { return pos.need_value(); });
    if (it != poses.end()) {
        it->parse(arg);
    }
}

void parser::parse_short(char arg, argv_reader& reader, bool may_have_param) {
    auto it = std::find_if(opts.begin(), opts.end(), [arg](const option& opt) {
        return opt.short_name() == arg;
    });
    if (it == opts.end()) {
        if (allow_undefined) {
            return;
        }
        throw_exception<cli_error_code::unkown_parameter>(
            std::string_view(&arg, 1));
    }

    if (it->get_value() == nullptr) {
        it->parsed_ = true;
        return;
    }

    if (!may_have_param || reader.finished()) {
        if (it->is_optional()) {
            return;
        }
        throw_exception<cli_error_code::need_param>(it->get_name(true));
    }

    it->parse(reader.read(), true);
}

void parser::parse_multi_short(std::string_view args, argv_reader& reader) {
    char last_arg = args.back();
    args.remove_suffix(1);

    for (auto arg : args) {
        parse_short(arg, reader, false);
    }

    parse_short(last_arg, reader, true);
}

void parser::parse_long(std::string_view arg, argv_reader& reader) {
    auto it = std::find_if(opts.begin(), opts.end(), [arg](const option& opt) {
        return opt.long_name() == arg;
    });

    if (it == opts.end()) {
        if (allow_undefined) {
            return;
        }
        throw_exception<cli_error_code::unkown_parameter>(arg);
    }

    if (it->get_value() == nullptr) {
        it->parsed_ = true;
        return;
    }

    if (reader.finished()) {
        if (it->is_optional()) {
            return;
        }
        throw_exception<cli_error_code::need_param>(it->long_name());
    }

    it->parse(reader.read(), false);
}

void parser::parse_long_eq(std::string_view arg, size_t eq_pos) {
    auto name = arg.substr(0, eq_pos);
    auto val = arg.substr(eq_pos + 1);

    auto it = std::find_if(opts.begin(), opts.end(), [name](const option& opt) {
        return opt.long_name() == name;
    });

    if (it == opts.end()) {
        if (allow_undefined) {
            return;
        }
        throw_exception<cli_error_code::unkown_parameter>(name);
    }

    it->parse(val, false);
}

std::vector<std::string> cli::split_winmain(std::string_view cmd_line) {
    win_line_parser p{cmd_line};
    std::vector<std::string> argv;
    while (!p.finished()) {
        argv.emplace_back(p.read());
    }
    return argv;
}

std::vector<std::wstring> cli::split_winmain(std::wstring_view cmd_line) {
    win_line_parser p{cmd_line};
    std::vector<std::wstring> argv;
    while (!p.finished()) {
        p.read_only();
        argv.emplace_back(p.wread());
    }
    return argv;
}
