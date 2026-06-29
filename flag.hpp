#ifndef FLAG_HPP_
#define FLAG_HPP_

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iterator>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <cassert>

namespace FlagHpp {

struct Error {
    enum class Kind {
        none,

        flag_provided_not_defined,
        no_argument_for_flag,
        incorrect_syntax,

        incorrect_format,
        out_of_range,
    };

    Kind kind = Kind::none;
    std::string_view flag_name;
};

struct Flag;

// API that uses global flag context

inline std::string_view &program_name();


inline void print_usage(std::ostreambuf_iterator<char> out);

inline void print_defaults(std::ostreambuf_iterator<char> out,
                           int flag_ident = 2, int usage_indent = 6,
                           bool always_display_default_value = false);

inline void print_error(std::ostreambuf_iterator<char> out);

inline void print_usage(std::FILE *out);

inline void print_defaults(std::FILE *out, int flag_ident = 2,
                           int usage_indent = 6,
                           bool always_display_default_value = false);

inline void print_error(std::FILE *out);


inline const Error &error();
inline int args_left();
inline int args_parsed();
inline int flags_parsed();


inline int flag_count();
inline const Flag &get_flag(int i);
inline const Flag *get_flag(std::string_view flag_name);
inline const Flag *get_flag(const void *flag_variable);
inline auto get_flags();


inline bool encountered_flag_stop();


bool *flag_bool(std::string_view flag_name, bool default_value,
                std::string_view help);

inline void flag_bool_var(bool *out, std::string_view flag_name,
                          bool default_value, std::string_view help);


template <std::integral T>
T *flag_int(std::string_view flag_name, T default_value, std::string_view help);

template <std::integral T, std::integral V>
void flag_int_var(T *out, std::string_view flag_name, V default_value,std::string_view help);


template <std::floating_point T>
T *flag_float(std::string_view flag_name, T default_value, std::string_view help);

template <std::floating_point T>
void flag_float_var(T *out, std::string_view flag_name, T default_value, std::string_view help);


std::string_view *flag_string(std::string_view flag_name, std::string_view default_value,
                std::string_view help);

inline void flag_string_var(std::string_view *out, std::string_view flag_name,
                       std::string_view default_value, std::string_view help);


template <typename T, typename V = std::vector<T>>
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
std::vector<T> *flag_list(std::string_view flag_name, V &&default_value,
                          std::string_view help);

template <typename T, typename V = std::vector<T>>
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
void flag_list_var(std::vector<T> *out, std::string_view flag_name,
                   V &&default_value, std::string_view help);


template <typename T, typename V = T>
    requires std::same_as<T, std::remove_cvref_t<V>>
T *flag(std::string_view flag_name, V &&default_value, std::string_view help);
   
template <typename T, typename V = T>
    requires std::same_as<T, std::remove_cvref_t<V>>
void flag_var(T *out, std::string_view flag_name, V &&default_value,
              std::string_view help);


// All global parse functions treat first argument as a program name

inline bool parse(int argc, char const *const *argv);

template <std::input_iterator It, std::sentinel_for<It> Se>
    requires std::convertible_to<std::iter_value_t<It>, std::string_view>
bool parse(It first, Se last);

template <std::ranges::input_range Rng>
    requires std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>
bool parse(Rng &&rng);


// This error handler is called when the flag name is incorrect.
// The std::string_view that is passed to the error handler is guranteed to be
// null terminated.
using ErrorHandlerFunc = void(std::string_view error);
inline ErrorHandlerFunc *&error_handler();

inline void print_error(const Error &error, std::ostreambuf_iterator<char> out);

constexpr inline std::array flag_prefixes = {
    std::string_view{"--"},
    std::string_view{"-"},
};
constexpr inline std::string_view flag_disable_prefix = "/";
constexpr inline std::string_view flag_arg_separator = "=";
constexpr inline std::string_view flag_stop = "--";

struct FlagValue {
    virtual Error::Kind set_value(std::string_view &flag_value) = 0;

    virtual std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const = 0;

    virtual bool behaves_like_bool() const {
        return false;
    }

    virtual void flag_present() {
        return;
    }

    virtual ~FlagValue() = default;
};

struct Flag {
    std::string_view name;
    std::string_view help;
    std::string default_value;
    std::unique_ptr<FlagValue> value;
};

inline void default_error_handler(std::string_view error) {
    std::fprintf(stderr, "%s", error.data());
    std::exit(1);
}

template <typename T>
struct FlagValueImpl;

template <typename T>
concept Formattable = std::is_default_constructible_v<std::formatter<T>>;

class FlagParser {
public:
    struct ParseResult {
        Error error;
        int args_parsed = 0;
        int flags_parsed = 0;
        bool encountered_flag_stop = false;
    };

    ErrorHandlerFunc *error_handler = default_error_handler;

    void print_defaults(std::ostreambuf_iterator<char> out, int flag_ident = 2,
                        int usage_indent = 6,
                        bool always_display_default_value = false) {
        for (const auto &flag : flags) {
            out = std::format_to(out, "{:{}}-{} ", "", flag_ident, flag.name);
            out = flag.value->output_type_name(out);
            out = std::format_to(out, "\n{:{}}{}", "", usage_indent, flag.help);
            bool display_default_value =
                flag.has_default_value &&
                (always_display_default_value || !flag.is_default_value_zero);
            if (display_default_value) {
                out = std::format_to(out, " (default: {})", flag.default_value);
            }
            out = std::format_to(out, "\n");
        }
    }

    int flag_count() const {
        return static_cast<int>(flags.size());
    }

    const Flag &get_flag(int i) const {
        assert(i > 0);
        return flags[static_cast<std::size_t>(i)];
    }

    const Flag *get_flag(std::string_view flag_name) const {
        auto search = std::ranges::find(flags, flag_name, &Flag::name);
        if (search != flags.end()) {
            return &(*search);
        }
        return nullptr;
    }

    const Flag *get_flag(const void *flag_variable) const {
        auto search = std::ranges::find(
            flags, flag_variable, [](const FlagInternal &flag) -> void * {
                void *result = nullptr;
                if (auto storage = std::get_if<void *>(&flag.storage);
                    storage != nullptr) {
                    result = *storage;
                }
                if (auto storage = std::get_if<UniquePtrAny>(&flag.storage);
                    storage != nullptr) {
                    result = storage->pointer.get();
                }
                return result;
            });
        if (search != flags.end()) {
            return &(*search);
        }
        return nullptr;
    }

    auto get_flags() const {
        return flags | std::views::transform(
                           [](const FlagInternal &flag) -> const Flag & {
                               return flag;
                           });
    }

    void flag_bool_var(bool *out, std::string_view flag_name,
                       bool default_value, std::string_view help) {
        flag_var(out, flag_name, default_value, help);
    }

    template <std::integral T, std::integral V>
    void flag_int_var(T *out, std::string_view flag_name, V default_value,
                      std::string_view help) {
        flag_var(out, flag_name, static_cast<T>(default_value), help);
    }

    template <std::floating_point T>
    void flag_float_var(T *out, std::string_view flag_name, T default_value,
                        std::string_view help) {
        flag_var(out, flag_name, default_value, help);
    }

    void flag_string_var(std::string_view *out, std::string_view flag_name,
                         std::string_view default_value,
                         std::string_view help) {
        flag_var(out, flag_name, default_value, help);
    }

    template <typename T, typename V = std::vector<T>>
        requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
    void flag_list_var(std::vector<T> *out, std::string_view flag_name,
                       V &&default_value, std::string_view help) {
        flag_var(out, flag_name, std::forward<V>(default_value), help);
    }

    bool *flag_bool(std::string_view flag_name, bool default_value,
                    std::string_view help) {
        return flag<bool>(flag_name, default_value, help);
    }

    template <std::integral T>
    T *flag_int(std::string_view flag_name, T default_value,
                std::string_view help) {
        return flag<T>(flag_name, default_value, help);
    }

    template <std::floating_point T>
    T *flag_float(std::string_view flag_name, T default_value,
                  std::string_view help) {
        return flag<T>(flag_name, default_value, help);
    }

    std::string_view *flag_string(std::string_view flag_name,
                                  std::string_view default_value,
                                  std::string_view help) {
        return flag<std::string_view>(flag_name, default_value, help);
    }

    template <typename T, typename V = std::vector<T>>
        requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
    std::vector<T> *flag_list(std::string_view flag_name, V &&default_value,
                              std::string_view help) {
        return flag<std::vector<T>>(flag_name, std::forward<V>(default_value),
                                    help);
    }

    template <typename T, typename V = T>
        requires std::same_as<T, std::remove_cvref_t<V>>
    T *flag(std::string_view flag_name, V &&default_value,
            std::string_view help) {
        return store_flag<T>(flag_name, std::forward<V>(default_value), help);
    }

    template <typename T, typename V = T>
        requires std::same_as<T, std::remove_cvref_t<V>>
    void flag_var(T *out, std::string_view flag_name, V &&default_value,
                  std::string_view help) {
        store_flag<T>(flag_name, std::forward<V>(default_value), help, out);
    }

    template <std::input_iterator It, std::sentinel_for<It> Se>
        requires std::convertible_to<std::iter_value_t<It>, std::string_view>
    ParseResult parse(It first, Se last) {
        // Determine whether flag_name is a flag. If it is, strip flag prefix
        // and return true. Otherwise return false.
        auto strip_flag_prefix = [](std::string_view *flag_name) -> bool {
            for (const auto &flag_prefix : flag_prefixes) {
                if (flag_name->starts_with(flag_prefix)) {
                    if (flag_name->size() == flag_prefix.size()) {
                        // flag_name starts with flag_prefix and has the same
                        // size => flag_name == flag_prefix => flag_name is not
                        // a flag
                        return false;
                    }

                    *flag_name = flag_name->substr(flag_prefix.size());
                    return true;
                }
            }
            return false;
        };

        auto strip_flag_disable_prefix =
            [](std::string_view *flag_name) -> bool {
            if (flag_name->starts_with(flag_disable_prefix)) {
                *flag_name = flag_name->substr(flag_disable_prefix.size());
                return true;
            }
            return false;
        };

        auto argument_split = [](std::string_view arg,
                                 std::string_view *flag_name,
                                 std::string_view *flag_value) -> bool {
            auto equals_index = arg.find('=');
            if (equals_index == std::string_view::npos) {
                *flag_name = arg;
                return true;
            }

            *flag_name = arg.substr(0, equals_index);
            *flag_value = arg.substr(equals_index + 1);
            return false;
        };

        int args_parsed = 0;
        int flags_parsed = 0;
        bool encountered_flag_stop = false;
        auto error = Error{};
        for (auto it = first; it != last; ++it) {
            std::string_view original_arg = *it;
            auto arg = original_arg;

            if (arg == flag_stop) {
                encountered_flag_stop = true;
                break;
            }

            bool is_flag = strip_flag_prefix(&arg);
            if (!is_flag) {
                break;
            }

            args_parsed += 1;
            flags_parsed += 1;

            bool disable = strip_flag_disable_prefix(&arg);

            std::string_view flag_name;
            std::string_view flag_arg;
            bool look_for_next_arg = argument_split(arg, &flag_name, &flag_arg);

            if (flag_name.empty()) {
                error = Error{
                    .kind = Error::Kind::incorrect_syntax,
                    .flag_name = original_arg,
                };
                break;
            }

            auto flag = get_flag_internal(flag_name);
            if (flag == nullptr) {
                error = Error{.kind = Error::Kind::flag_provided_not_defined,
                              .flag_name = flag_name};
                break;
            }

            if (flag->value->behaves_like_bool() && look_for_next_arg) {
                if (!disable) {
                    flag->value->flag_present();
                }
                continue;
            }

            if (look_for_next_arg) {
                ++it;
                if (it == last) {
                    error = Error{.kind = Error::Kind::no_argument_for_flag,
                                  .flag_name = flag_name};
                    break;
                }

                args_parsed += 1;
                flag_arg = *it;
            }

            if (!disable) {
                auto error_kind = flag->value->set_value(flag_arg);
                if (error_kind != Error::Kind::none) {
                    error = Error{.kind = error_kind, .flag_name = flag_name};
                    break;
                }
                if (!flag_arg.empty()) {
                    error = Error{.kind = Error::Kind::incorrect_format,
                                  .flag_name = flag_name};
                    break;
                }
            }
        }

        return ParseResult{.error = error,
                           .args_parsed = args_parsed,
                           .flags_parsed = flags_parsed,
                           .encountered_flag_stop = encountered_flag_stop};
    }

    template <std::ranges::input_range Rng>
        requires std::convertible_to<std::ranges::range_value_t<Rng>,
                                     std::string_view>
    ParseResult parse(Rng &&rng) {
        return parse(std::ranges::cbegin(rng), std::ranges::cend(rng));
    }

private:
    struct UniquePtrAny {
        std::unique_ptr<void, void (*)(void *)> pointer = {nullptr, nullptr};

        UniquePtrAny() {
        }

        template <typename T>
        UniquePtrAny(T *pointer_to_store)
            : pointer{pointer_to_store,
                      [](void *pointer) { delete static_cast<T *>(pointer); }} {
        }
    };

    struct FlagInternal : public Flag {
        std::variant<UniquePtrAny, void *> storage = nullptr;
        bool is_default_value_zero = false;
        bool has_default_value = false;
    };

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args &&...args) {
        if (error_handler != nullptr) {
            auto buffer = std::array<char, 256>{};
            auto result = std::format_to_n(buffer.data(), buffer.size() - 1,
                                           fmt, std::forward<Args>(args)...);
            error_handler(std::string_view{buffer.data(), result.out});
        }
    }

    template <typename T, typename V>
        requires std::same_as<T, std::remove_cvref_t<V>>
    T *store_flag(std::string_view flag_name, V &&default_value,
                  std::string_view help, T *user_provided_storage = nullptr) {
        if (!validate_flag_name(flag_name)) {
            return nullptr;
        }

        flags.push_back(FlagInternal{});
        auto &flag = flags.back();

        flag.name = flag_name;
        flag.help = help;

        if constexpr (Formattable<T>) {
            flag.has_default_value = true;
            if constexpr (std::convertible_to<T, std::string_view>) {
                std::format_to(std::back_inserter(flag.default_value), "\"{}\"",
                               default_value);
            } else {
                std::format_to(std::back_inserter(flag.default_value), "{}",
                               default_value);
            }
        } else {
            flag.has_default_value = false;
        }

        if constexpr (std::is_default_constructible_v<T> &&
                      std::equality_comparable<T>) {
            flag.is_default_value_zero = (default_value == T{});
        }

        if (user_provided_storage != nullptr) {
            *user_provided_storage = std::forward<V>(default_value);
            flag.storage = user_provided_storage;
            flag.value =
                std::make_unique<FlagValueImpl<T>>(user_provided_storage);
            return user_provided_storage;
        } else {
            auto flag_storage = new T{std::forward<V>(default_value)};
            flag.storage = UniquePtrAny{flag_storage};
            flag.value = std::make_unique<FlagValueImpl<T>>(flag_storage);
            return flag_storage;
        }
    }

    FlagInternal *get_flag_internal(std::string_view flag_name) {
        auto search_static =
            std::ranges::find(flags, flag_name, &FlagInternal::name);
        if (search_static != flags.end()) {
            return &(*search_static);
        }
        return nullptr;
    }

    bool validate_flag_name(std::string_view flag_name) {
        if (is_flag(flag_name)) {
            error("ERROR: flag name \"{}\" is incorrect. Name contains "
                  "flag prefix.\n",
                  flag_name);
            return false;
        }
        if (flag_name.find(flag_arg_separator) != std::string_view::npos) {
            error("ERROR: flag name \"{}\" is incorrect. Name contains {}.\n",
                  flag_name, flag_arg_separator);
            return false;
        }
        if (flag_name.find(flag_disable_prefix) != std::string_view::npos) {
            error("ERROR: flag name \"{}\" is incorrect. Name contains {}.\n",
                  flag_name, flag_disable_prefix);
            return false;
        }
        if (flag_name.empty()) {
            error("ERROR: flag name is incorrect. Flag name is empty "
                  "string.\n");
            return false;
        }
        if (get_flag(flag_name) != nullptr) {
            error("ERROR: flag name \"{}\" is incorrect. Duplicate flag "
                  "name.\n",
                  flag_name);
            return false;
        }
        return true;
    }

    static bool is_flag(std::string_view str) {
        return std::ranges::any_of(flag_prefixes,
                                   [str](const std::string_view &prefix) {
                                       return str.starts_with(prefix);
                                   });
    }

    std::vector<FlagInternal> flags;
};

inline struct {
    FlagParser parser;
    FlagParser::ParseResult parse_result;
    int args_left = 0;
    std::string_view program_name;
} global_context = {};

inline std::string_view &program_name() {
    return global_context.program_name;
}

inline void print_usage(std::ostreambuf_iterator<char> out) {
    print_error(out);
    out = std::format_to(out, "Usage of {}:\n", program_name());
    print_defaults(out);
}

inline void print_defaults(std::ostreambuf_iterator<char> out, int flag_ident,
                           int usage_indent,
                           bool always_display_default_value) {
    global_context.parser.print_defaults(out, flag_ident, usage_indent,
                                         always_display_default_value);
}

inline void print_error(std::ostreambuf_iterator<char> out) {
    print_error(global_context.parse_result.error, out);
}

inline void print_usage(std::FILE *out) {
    std::stringbuf ss;
    print_usage(&ss);
    auto view = ss.view();
    std::fprintf(out, "%.*s", static_cast<unsigned int>(view.size()),
                 view.data());
}

inline void print_defaults(std::FILE *out, int flag_ident, int usage_indent,
                           bool always_display_default_value) {
    std::stringbuf ss;
    global_context.parser.print_defaults(&ss, flag_ident, usage_indent,
                                         always_display_default_value);
    auto view = ss.view();
    std::fprintf(out, "%.*s", static_cast<unsigned int>(view.size()),
                 view.data());
}

inline void print_error(std::FILE *out) {
    std::stringbuf ss;
    print_error(global_context.parse_result.error, &ss);
    auto view = ss.view();
    std::fprintf(out, "%.*s", static_cast<unsigned int>(view.size()),
                 view.data());
}

inline ErrorHandlerFunc *&error_handler() {
    return global_context.parser.error_handler;
}

inline const Error &error() {
    return global_context.parse_result.error;
}

inline int args_left() {
    return global_context.args_left;
}

inline int args_parsed() {
    return global_context.parse_result.args_parsed;
}

inline int flags_parsed() {
    return global_context.parse_result.flags_parsed;
}

inline bool encountered_flag_stop() {
    return global_context.parse_result.encountered_flag_stop;
}

inline const Flag &get_flag(int i) {
    return global_context.parser.get_flag(i);
}

inline const Flag *get_flag(std::string_view flag_name) {
    return global_context.parser.get_flag(flag_name);
}

inline const Flag *get_flag(const void *flag_variable) {
    return global_context.parser.get_flag(flag_variable);
}

inline int flag_count() {
    return global_context.parser.flag_count();
}

inline auto get_flags() {
    return global_context.parser.get_flags();
}

inline void flag_bool_var(bool *out, std::string_view flag_name,
                          bool default_value, std::string_view help) {
    global_context.parser.flag_bool_var(out, flag_name, default_value, help);
}

template <std::integral T, std::integral V>
void flag_int_var(T *out, std::string_view flag_name, V default_value,
             std::string_view help) {
    global_context.parser.flag_int_var(out, flag_name, default_value, help);
}

template <typename T, typename V>
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
void flag_list_var(std::vector<T> *out, std::string_view flag_name,
                   V &&default_value, std::string_view help) {
    global_context.parser.flag_list_var(out, flag_name,
                                        std::forward<V>(default_value), help);
}

template <std::floating_point T>
void flag_float_var(T *out, std::string_view flag_name, T default_value,
               std::string_view help) {
    global_context.parser.flag_float_var(out, flag_name, default_value, help);
}

inline void flag_string_var(std::string_view *out, std::string_view flag_name,
                       std::string_view default_value, std::string_view help) {
    global_context.parser.flag_string_var(out, flag_name, default_value, help);
}

inline bool *flag_bool(std::string_view flag_name, bool default_value,
                       std::string_view help) {
    return global_context.parser.flag_bool(flag_name, default_value, help);
}

template <std::integral T>
T *flag_int(std::string_view flag_name, T default_value,
            std::string_view help) {
    return global_context.parser.flag_int(flag_name, default_value, help);
}

template <std::floating_point T>
T *flag_float(std::string_view flag_name, T default_value,
              std::string_view help) {
    return global_context.parser.flag_float(flag_name, default_value, help);
}

inline std::string_view *flag_string(std::string_view flag_name,
                                     std::string_view default_value,
                                     std::string_view help) {
    return global_context.parser.flag_string(flag_name, default_value, help);
}

template <typename T, typename V>
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
std::vector<T> *flag_list(std::string_view flag_name, V &&default_value,
                          std::string_view help) {
    return global_context.parser.flag_list<T>(
        flag_name, std::forward<V>(default_value), help);
}

template <typename T, typename V>
    requires std::same_as<T, std::remove_cvref_t<V>>
T *flag(std::string_view flag_name, V &&default_value, std::string_view help) {
    return global_context.parser.flag<T>(flag_name,
                                         std::forward<V>(default_value), help);
}

template <typename T, typename V>
    requires std::same_as<T, std::remove_cvref_t<V>>
void flag_var(T *out, std::string_view flag_name, V &&default_value,
              std::string_view help) {
    global_context.parser.flag_var(out, flag_name,
                                   std::forward<V>(default_value), help);
}

inline bool parse(int argc, char const *const *argv) {
    return parse(argv, argv + argc);
}

template <std::input_iterator It, std::sentinel_for<It> Se>
    requires std::convertible_to<std::iter_value_t<It>, std::string_view>
bool parse(It first, Se last) {
    if (first == last) {
        return true;
    }

    auto total = std::ranges::distance(first, last);
    global_context.program_name = *first;
    global_context.parse_result = global_context.parser.parse(++first, last);
    global_context.parse_result.args_parsed += 1; // Include program name
    global_context.args_left =
        static_cast<int>(total) - global_context.parse_result.args_parsed;
    return global_context.parse_result.error.kind == Error::Kind::none;
}

template <std::ranges::input_range Rng>
    requires std::convertible_to<std::ranges::range_value_t<Rng>,
                                 std::string_view>
bool parse(Rng &&rng) {
    return parse(std::ranges::cbegin(rng), std::ranges::cend(rng));
}

inline void print_error(const Error &error,
                        std::ostreambuf_iterator<char> out) {
    switch (error.kind) {
        using enum Error::Kind;
        case none: {
            return;
        }

        case flag_provided_not_defined: {
            std::format_to(out, "flag provided but not defined: -{}\n",
                           error.flag_name);
            return;
        }
        case no_argument_for_flag: {
            std::format_to(out, "no value provided for -{}\n", error.flag_name);
            return;
        }
        case incorrect_syntax: {
            std::format_to(out, "incorrect syntax: {}\n", error.flag_name);
            return;
        }

        case incorrect_format: {
            std::format_to(out, "invalid value format for -{}\n",
                           error.flag_name);
            return;
        }
        case out_of_range: {
            std::format_to(out, "value ouf of range for -{}\n",
                           error.flag_name);
            return;
        }
    }
}

template <typename T>
concept IntegerOrFloatingPoint = std::integral<T> || std::floating_point<T>;

template <IntegerOrFloatingPoint T>
Error::Kind parse_number(std::string_view &value, T &out) {
    auto [ptr, ec] = std::from_chars(value.begin(), value.end(), out);
    value = std::string_view{ptr, value.end()};
    
    auto error = Error::Kind::none;
    if (ec == std::errc::invalid_argument) {
        error = Error::Kind::incorrect_format;
    } else if (ec == std::errc::result_out_of_range) {
        error = Error::Kind::out_of_range;
    }
    return error;
}

template <IntegerOrFloatingPoint T>
struct FlagValueImpl<T> : public FlagValue {
    T *value;

    FlagValueImpl(T *value) : value{value} {
    }

    Error::Kind set_value(std::string_view &flag_value) override {
        auto error = parse_number<T>(flag_value, *value);
        if (error != Error::Kind::none) {
            return error;
        }
        return Error::Kind::none;
    }

    std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const override {
        if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>) {
                return std::format_to(out, "int{}", sizeof(T) * 8);
            } else {
                return std::format_to(out, "uint{}", sizeof(T) * 8);
            }
        } else {
            return std::format_to(out, "float{}", sizeof(T) * 8);
        }
    }
};

template <>
struct FlagValueImpl<bool> : public FlagValue {
    bool *value;

    FlagValueImpl(bool *value) : value{value} {
    }

    Error::Kind set_value(std::string_view &flag_value) override {
        using namespace std::literals;

        if (flag_value == "0"sv || flag_value == "1"sv) {
            *value = (flag_value == "1"sv);
            flag_value.remove_prefix(1);
            return Error::Kind::none;
        }

        auto take_word_or_number = [](std::string_view &str) -> std::string_view {
            std::size_t i = 0;
            while (i < str.size() && std::isalnum(str[i])) {
                i += 1;
            }
            auto result = str.substr(0, i);
            str = str.substr(i);
            return result;
        };

        auto word = take_word_or_number(flag_value);

        static constexpr std::array true_values{
            "true"sv, "True"sv, "TRUE"sv, "t"sv, "T"sv, "1"sv,
        };
        auto search_true = std::ranges::find(true_values, word);
        if (search_true != true_values.end()) {
            *value = true;
            return Error::Kind::none;
        }

        static constexpr std::array false_values{
            "false"sv, "False"sv, "FALSE"sv, "f"sv, "F"sv, "0"sv,
        };
        auto search_false = std::ranges::find(false_values, word);
        if (search_false != false_values.end()) {
            *value = false;
            return Error::Kind::none;
        }

        return Error::Kind::incorrect_format;
    }

    std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const override {
        return std::format_to(out, "bool");
    }

    bool behaves_like_bool() const override {
        return true;
    }

    void flag_present() override {
        *value = true;
    }
};

template <>
struct FlagValueImpl<std::string_view> : public FlagValue {
    std::string_view *value;

    FlagValueImpl(std::string_view *value) : value{value} {
    }

    Error::Kind set_value(std::string_view &flag_value) override {
        *value = flag_value;
        flag_value = {};
        return Error::Kind::none;
    }

    std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const override {
        return std::format_to(out, "string");
    }
};

void skip_whitespaces(std::string_view &str) {
    while(!str.empty() && str[0] == ' ') {
        str.remove_prefix(1);
    }  
};

template <typename T>
struct FlagValueImpl<std::vector<T>> : public FlagValue {
    std::vector<T> *value = nullptr;
    bool first = true;

    FlagValueImpl(std::vector<T> *value) : value{value} {
    }

    Error::Kind set_value(std::string_view &flag_value) override {
        using std::operator""sv;

        auto parse_and_add_value =
            [this](std::string_view &value_string) -> Error::Kind {
            auto elem = T{};
            auto elem_flag_value = FlagValueImpl<T>{&elem};
            auto err = elem_flag_value.set_value(value_string);
            if (err != Error::Kind::none) {
                return err;
            }

            if (first) {
                value->clear();
                first = false;
            }
            value->push_back(std::move(elem));
            return Error::Kind::none;
        };

        auto parse_string_untill_comma_or_close_bracket =
            [parse_and_add_value](
                std::string_view &quoted_string) -> Error::Kind {
            std::size_t i = 0;
            while (i < quoted_string.size() && quoted_string[i] != ',' &&
                   quoted_string[i] != ']') {
                i += 1;
            }

            auto str = quoted_string.substr(0, i);
            quoted_string = quoted_string.substr(i);
            return parse_and_add_value(str);
        };

        if (!flag_value.starts_with('[')) {
            if constexpr(std::convertible_to<T, std::string_view>) {
                return parse_string_untill_comma_or_close_bracket(flag_value);
            }
            return parse_and_add_value(flag_value);
        }
        flag_value.remove_prefix(1);
        skip_whitespaces(flag_value);

        bool first = true;
        while (!(flag_value.starts_with(']') || flag_value.empty())) {
            if (!first) {
                if (!flag_value.starts_with(',')) {
                    return Error::Kind::incorrect_format;
                }
                flag_value.remove_prefix(1);
                skip_whitespaces(flag_value);
            } else {
                first = false;
            }
            
            if constexpr (std::convertible_to<T, std::string_view>) {
                auto error = parse_string_untill_comma_or_close_bracket(flag_value);
                if (error != Error::Kind::none) {
                    return error;
                }
            } else {
                auto error = parse_and_add_value(flag_value);
                if (error != Error::Kind::none) {
                    return error;
                }
            }
            skip_whitespaces(flag_value);
        }

        if (!flag_value.starts_with(']')) {
            return Error::Kind::incorrect_format;
        }
        flag_value.remove_prefix(1);

        return Error::Kind::none;
    }

    std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const override {
        auto flag_value = FlagHpp::FlagValueImpl<T>{nullptr};
        out = std::format_to(out, "List<");
        out = flag_value.output_type_name(out);
        *out = '>';
        return out;
    }
};

}; // namespace FlagHpp
#endif // #ifndef FLAG_HPP
