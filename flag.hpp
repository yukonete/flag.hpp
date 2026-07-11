#ifndef FLAG_HPP_
#define FLAG_HPP_

#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <span>

namespace fhpp {

// TODO: 
// Consider using single buffer to store flags values and FlagValueImpl<T> objects.
// That way the only 2 global configuration variables that are going to be left are
// max_flags and storage_size, where storage_size is the size of the that buffer.
// default_value_string_storage_size won't be needed because this buffer will be used to
// store string data as well.

// CONFIGURATION
// Library does no dynamic allocations to store flags.
// Unless type of the flag does an allocation, 
// which is the case for flag lists because they use std::vector to store values.
// So all the limits on storage size have to be specified here.

constexpr inline size_t max_flags = 64;

// Size of the storage for an actuall value of a flag
// All flag values are stored in storages of the same size
// So can think of this storage as a union, and this variable defines size of that union
constexpr inline size_t flag_storage_size_in_ptrs = 3;

// Size of the storage for FlagValueImpl<T>
// Dont forget to account for pointer to vtable
constexpr inline size_t flag_value_storage_size_in_ptrs = 3;

constexpr inline size_t default_value_string_storage_size = max_flags * 8;


constexpr inline std::array flag_prefixes = {
    std::string_view{"--"},
    std::string_view{"-"},
};
constexpr inline std::string_view flag_disable_prefix = "/";
constexpr inline std::string_view flag_arg_separator = "=";
constexpr inline std::string_view flag_stop = "--";

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

struct FlagValue {
    // set_value should try to parse value from the beginning of flag_value string
    // and do not report error if there are any more characters left after parsing
    virtual Error::Kind set_value(std::string_view &flag_value) = 0;

    virtual std::ostreambuf_iterator<char> output_type_name(std::ostreambuf_iterator<char> out) const = 0;

    virtual bool behaves_like_bool() const {
        return false;
    }

    virtual void set_flag_present() {
        return;
    }

    virtual ~FlagValue() = default;
};

struct Flag {
    std::string_view name;
    std::string_view help;
    std::string_view default_value;
    FlagValue *value = nullptr;
};

// API that uses global flag context

inline std::string_view &program_name();

inline void print_usage(std::ostreambuf_iterator<char> out);
inline void print_defaults(std::ostreambuf_iterator<char> out, int flag_ident = 2, int usage_indent = 6);
inline void print_error(std::ostreambuf_iterator<char> out);

inline void print_usage(std::FILE *out);
inline void print_defaults(std::FILE *out, int flag_ident = 2, int usage_indent = 6);
inline void print_error(std::FILE *out);

inline const Error &get_error();
inline void clear_error();

inline int args_left();
inline int args_parsed();
inline int flags_parsed();

inline int flag_count();
inline const Flag &get_flag(int i);
inline const Flag *get_flag(std::string_view flag_name);
inline const Flag *get_flag(const void *flag_variable);
inline std::span<const Flag> get_flags();

inline bool encountered_flag_stop();

inline bool *flag_bool(std::string_view flag_name, bool default_value, std::string_view help);

template <std::integral T>
T *flag_int(std::string_view flag_name, T default_value, std::string_view help);

template <std::floating_point T>
T *flag_float(std::string_view flag_name, T default_value, std::string_view help);

std::string_view *flag_string(std::string_view flag_name, std::string_view default_value, std::string_view help);

template <typename T, typename V = std::vector<T>>
std::vector<T> *flag_list(std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>;

template <typename T, typename V = T>
T *flag(std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<T, std::remove_cvref_t<V>>;

inline void flag_bool_var(bool *out, std::string_view flag_name, bool default_value, std::string_view help);

template <std::integral T, std::integral V>
void flag_int_var(T *out, std::string_view flag_name, V default_value, std::string_view help);

template <std::floating_point T>
void flag_float_var(T *out, std::string_view flag_name, T default_value, std::string_view help);

inline void flag_string_var(std::string_view *out, std::string_view flag_name, std::string_view default_value,
                            std::string_view help);

template <typename T, typename V = std::vector<T>>

void flag_list_var(std::vector<T> *out, std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>;

template <typename T, typename V = T>
void flag_var(T *out, std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<T, std::remove_cvref_t<V>>;

// Global parse functions treat first argument as a program name
inline bool parse(int argc, char const *const *argv);

// Global parse functions treat first argument as a program name
template <std::ranges::input_range Rng>
bool parse(Rng &&rng)
    requires std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>;

// This error handler is called for different errors that occur on flag registration stage.
// The std::string_view that is passed to the error handler is guranteed to be null terminated.
using ErrorHandlerFunc = void(std::string_view error);
inline ErrorHandlerFunc *&error_handler();

inline void print_error(const Error &error, std::ostreambuf_iterator<char> out);

inline void default_error_handler(std::string_view error) {
    std::fprintf(stderr, "%s\n", error.data());
    std::exit(1);
}

template <typename T>
struct FlagValueImpl;

class FlagParser {
public:
    struct ParseResult {
        Error error;
        int args_parsed = 0;
        int flags_parsed = 0;
        bool encountered_flag_stop = false;
    };

    ErrorHandlerFunc *error_handler = default_error_handler;

    void print_defaults(std::ostreambuf_iterator<char> out, int flag_ident = 2, int usage_indent = 6) const {
        auto flags = get_flags();
        for (size_t i = 0; i < flags.size(); ++i) {
            const auto &flag = flags[i];
            const auto &data = flags_data_[i];
            out = std::format_to(out, "{:{}}-{} ", "", flag_ident, flag.name);
            out = data.flag_value_storage.as<FlagValue>()->output_type_name(out);
            out = std::format_to(out, "\n{:{}}{}", "", usage_indent, flag.help);
            if (data.display_default_value) {
                out = std::format_to(out, " (default: {})", flag.default_value);
            }
            out = std::format_to(out, "\n");
        }
    }

    int flag_count() const {
        return static_cast<int>(flag_count_);
    }

    const Flag &get_flag(int i) const {
        assert(i > 0 && static_cast<std::size_t>(i) < flag_count_);
        return flags_[static_cast<std::size_t>(i)];
    }

    const Flag *get_flag(std::string_view flag_name) const {
        for (const auto &flag : get_flags()) {
            if (flag.name == flag_name) {
                return &flag;
            }
        }
        return nullptr;
    }

    const Flag *get_flag(const void *flag_variable) const {
        auto flags = get_flags();
        for (size_t i = 0; i < flags.size(); ++i) {
            const auto &data = flags_data_[i];
            const void *pointer = nullptr;
            if (data.user_provided_storage) {
                pointer = *data.storage.as<const void *>();
            } else {
                pointer = data.storage.as<const FlagValue>();
            }
            if (flag_variable == pointer) {
                return &flags[i];
            }
        }
        return nullptr;
    }

    std::span<const Flag> get_flags() const {
        return std::span{flags_.data(), flag_count_};
    }

    void flag_bool_var(bool *out, std::string_view flag_name, bool default_value, std::string_view help) {
        flag_var(out, flag_name, default_value, help);
    }

    template <std::integral T, std::integral V>
    void flag_int_var(T *out, std::string_view flag_name, V default_value, std::string_view help) {
        flag_var(out, flag_name, static_cast<T>(default_value), help);
    }

    template <std::floating_point T>
    void flag_float_var(T *out, std::string_view flag_name, T default_value, std::string_view help) {
        flag_var(out, flag_name, default_value, help);
    }

    void flag_string_var(std::string_view *out, std::string_view flag_name, std::string_view default_value,
                         std::string_view help) {
        flag_var(out, flag_name, default_value, help);
    }

    template <typename T, typename V = std::vector<T>>
    void flag_list_var(std::vector<T> *out, std::string_view flag_name, V &&default_value, std::string_view help)
        requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
    {
        flag_var(out, flag_name, std::forward<V>(default_value), help);
    }

    bool *flag_bool(std::string_view flag_name, bool default_value, std::string_view help) {
        return flag<bool>(flag_name, default_value, help);
    }

    template <std::integral T>
    T *flag_int(std::string_view flag_name, T default_value, std::string_view help) {
        return flag<T>(flag_name, default_value, help);
    }

    template <std::floating_point T>
    T *flag_float(std::string_view flag_name, T default_value, std::string_view help) {
        return flag<T>(flag_name, default_value, help);
    }

    std::string_view *flag_string(std::string_view flag_name, std::string_view default_value, std::string_view help) {
        return flag<std::string_view>(flag_name, default_value, help);
    }

    template <typename T, typename V = std::vector<T>>
    std::vector<T> *flag_list(std::string_view flag_name, V &&default_value, std::string_view help)
        requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
    {
        return flag<std::vector<T>>(flag_name, std::forward<V>(default_value), help);
    }

    template <typename T, typename V = T>
    T *flag(std::string_view flag_name, V &&default_value, std::string_view help)
        requires std::same_as<T, std::remove_cvref_t<V>>
    {
        return store_flag<T>(flag_name, std::forward<V>(default_value), help);
    }

    template <typename T, typename V = T>
    void flag_var(T *out, std::string_view flag_name, V &&default_value, std::string_view help)
        requires std::same_as<T, std::remove_cvref_t<V>>
    {
        store_flag<T>(flag_name, std::forward<V>(default_value), help, out);
    }

    template <std::ranges::input_range Rng>
    ParseResult parse(Rng &&rng)
        requires std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>
    {
        // Determine whether flag_name is a flag. If it is, strip flag prefix
        // and return true. Otherwise return false.
        auto strip_flag_prefix = [](std::string_view &flag_name) -> bool {
            for (const auto &flag_prefix : flag_prefixes) {
                if (flag_name.starts_with(flag_prefix)) {
                    if (flag_name.size() == flag_prefix.size()) {
                        // flag_name starts with flag_prefix and has the same
                        // size => flag_name == flag_prefix => flag_name is not
                        // a flag
                        return false;
                    }

                    flag_name = flag_name.substr(flag_prefix.size());
                    return true;
                }
            }
            return false;
        };

        auto strip_flag_disable_prefix = [](std::string_view &flag_name) -> bool {
            if (flag_name.starts_with(flag_disable_prefix)) {
                flag_name = flag_name.substr(flag_disable_prefix.size());
                return true;
            }
            return false;
        };

        auto argument_split = [](std::string_view arg, std::string_view &flag_name,
                                 std::string_view &flag_value) -> bool {
            auto equals_index = arg.find('=');
            if (equals_index == std::string_view::npos) {
                flag_name = arg;
                return true;
            }

            flag_name = arg.substr(0, equals_index);
            flag_value = arg.substr(equals_index + 1);
            return false;
        };

        int args_parsed = 0;
        int flags_parsed = 0;
        bool encountered_flag_stop = false;
        auto error = Error{};
        auto last = std::ranges::cend(rng);
        for (auto it = std::ranges::cbegin(rng); it != last; ++it) {
            std::string_view original_arg = *it;
            auto arg = original_arg;

            if (arg == flag_stop) {
                encountered_flag_stop = true;
                break;
            }

            bool is_flag = strip_flag_prefix(arg);
            if (!is_flag) {
                break;
            }

            args_parsed += 1;
            flags_parsed += 1;

            bool disable = strip_flag_disable_prefix(arg);

            std::string_view flag_name;
            std::string_view flag_arg;
            bool look_for_next_arg = argument_split(arg, flag_name, flag_arg);

            if (flag_name.empty()) {
                error = Error{
                    .kind = Error::Kind::incorrect_syntax,
                    .flag_name = original_arg,
                };
                break;
            }

            auto data = get_flag_data(flag_name);
            if (data == nullptr) {
                error = Error{.kind = Error::Kind::flag_provided_not_defined, .flag_name = flag_name};
                break;
            }
            auto flag_value = data->flag_value_storage.as<FlagValue>();

            if (flag_value->behaves_like_bool() && look_for_next_arg) {
                if (!disable) {
                    flag_value->set_flag_present();
                }
                continue;
            }

            if (look_for_next_arg) {
                ++it;
                if (it == last) {
                    error = Error{.kind = Error::Kind::no_argument_for_flag, .flag_name = flag_name};
                    break;
                }

                args_parsed += 1;
                flag_arg = *it;
            }

            if (!disable) {
                auto error_kind = flag_value->set_value(flag_arg);
                if (error_kind != Error::Kind::none) {
                    error = Error{.kind = error_kind, .flag_name = flag_name};
                    break;
                }
                if (!flag_arg.empty()) {
                    error = Error{.kind = Error::Kind::incorrect_format, .flag_name = flag_name};
                    break;
                }
            }
        }

        return ParseResult{.error = error,
                           .args_parsed = args_parsed,
                           .flags_parsed = flags_parsed,
                           .encountered_flag_stop = encountered_flag_stop};
    }

private:
    template <size_t SizeInPtrs>
    struct Storage {
        using Destructor = void (*)(void *pointer);

        uintptr_t storage[SizeInPtrs] = {};
        Destructor destructor = nullptr;

        constexpr Storage() {};
        constexpr Storage(const Storage &) = delete;
        constexpr Storage(Storage &&other) = delete;

        ~Storage() {
            drop();
        }

        Storage *drop() {
            if (destructor) {
                destructor(storage);
            }
            return this;
        }

        template <typename T, typename... Args>
        T *store(Args &&...args) {
            using ObjectType = T;
            static_assert(alignof(ObjectType) <= alignof(Storage));
            static_assert(sizeof(ObjectType) <= sizeof(storage));

            drop();
            auto pointer = new (storage) ObjectType{std::forward<Args>(args)...};
            if constexpr (std::is_trivially_destructible_v<T>) {
                destructor = nullptr;
            } else {
                destructor = [](void *pointer) {
                    auto object = static_cast<ObjectType *>(pointer);
                    object->~ObjectType();
                };
            }
            return pointer;
        }

        template <typename T>
        T *as() {
            return reinterpret_cast<T *>(storage);
        }

        template <typename T>
        const T *as() const {
            return reinterpret_cast<const T *>(storage);
        }
    };

    struct FlagData {
        Storage<flag_storage_size_in_ptrs> storage;
        Storage<flag_value_storage_size_in_ptrs> flag_value_storage;
        bool user_provided_storage = false;
        bool display_default_value = false;
    };

    template <typename T, typename V>
    T *store_flag(std::string_view flag_name, V &&default_value, std::string_view help,
                  T *user_provided_storage = nullptr)
        requires std::same_as<T, std::remove_cvref_t<V>>
    {
        static_assert(sizeof(FlagValueImpl<T>) <= flag_value_storage_size_in_ptrs * sizeof(void *),
                      "Size of FlagValueImpl<T> for one of the types used as a flag is bigger than storage size, this "
                      "storage size is controlled by flag_value_storage_size_in_ptrs global variable");
        static_assert(sizeof(T) <= flag_value_storage_size_in_ptrs * sizeof(void *),
                      "Size of one of the types used as a flag is bigger than storage size, this "
                      "storage size is controlled by flag_storage_size_in_ptrs global variable");
        static_assert(std::derived_from<FlagValueImpl<T>, FlagValue>, "FlagValueImpl<T> should inherit from FlagValue");
        static_assert(std::constructible_from<FlagValueImpl<T>, T*>, "FlagValueImpl<T> should be constructible from T");

        if (!validate_flag_name(flag_name)) {
            return nullptr;
        }

        auto equals_default_value = false;
        if constexpr (std::is_default_constructible_v<T> && std::equality_comparable<T>) {
            equals_default_value = (default_value == T{});
        }

        auto has_formatter = false;
        auto default_value_string = std::string_view{};
        if constexpr (std::is_default_constructible_v<std::formatter<T>>) {
            has_formatter = true;
            if (default_value_string_storage_cursor_ < default_value_string_storage_.size()) {
                auto write_position = &default_value_string_storage_[default_value_string_storage_cursor_];
                auto free_space = default_value_string_storage_.end() - write_position;
                char *after_write_position = nullptr;
                if constexpr (std::convertible_to<T, std::string_view>) {
                    after_write_position = std::format_to_n(write_position, free_space, "\"{}\"", default_value).out;
                } else {
                    after_write_position = std::format_to_n(write_position, free_space, "{}", default_value).out;
                }
                assert(after_write_position);
                auto written = static_cast<size_t>(after_write_position - write_position);
                default_value_string_storage_cursor_ += written;
                default_value_string = std::string_view{write_position, written};
            }
            if (default_value_string_storage_cursor_ >= default_value_string_storage_.size()) {
                error("ERROR: Buffer for default_value strings ran out.");
                return nullptr;
            }
        }

        if (flag_count_ >= flags_.size()) {
            error("ERROR: max_flags limit reached.");
            return nullptr;
        }
        assert(flags_.size() == flags_data_.size());

        auto &data = flags_data_[flag_count_];
        data.display_default_value = (has_formatter && !equals_default_value);

        if (user_provided_storage != nullptr) {
            data.user_provided_storage = true;
            *user_provided_storage = std::forward<V>(default_value);
            data.storage.store<T *>(user_provided_storage);
            data.flag_value_storage.store<FlagValueImpl<T>>(user_provided_storage);
        } else {
            data.user_provided_storage = false;
            auto flag_storage = data.storage.store<T>(std::forward<V>(default_value));
            data.flag_value_storage.store<FlagValueImpl<T>>(flag_storage);
        }

        auto &flag = flags_[flag_count_];
        flag.name = flag_name;
        flag.help = help;
        flag.default_value = default_value_string;
        flag.value = data.flag_value_storage.as<FlagValue>();

        flag_count_ += 1;

        if (data.user_provided_storage) {
            return *data.storage.as<T *>();
        }
        return data.storage.as<T>();
    }

    FlagData *get_flag_data(std::string_view flag_name) {
        auto flags = get_flags();
        for (size_t i = 0; i < flags.size(); ++i) {
            const auto &flag = flags[i];
            if (flag.name == flag_name) {
                return &flags_data_[i];
            }
        }
        return nullptr;
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args &&...args) {
        if (error_handler != nullptr) {
            auto buffer = std::array<char, 256>{};
            auto result = std::format_to_n(buffer.data(), buffer.size() - 1, fmt, std::forward<Args>(args)...);
            error_handler(std::string_view{buffer.data(), result.out});
        }
    }

    bool validate_flag_name(std::string_view flag_name) {
        if (is_flag(flag_name)) {
            error("ERROR: flag name \"{}\" is incorrect. Name contains "
                  "flag prefix.",
                  flag_name);
            return false;
        }
        if (flag_name.find(flag_arg_separator) != std::string_view::npos) {
            error("ERROR: flag name \"{}\" is incorrect. Name contains {}.", flag_name, flag_arg_separator);
            return false;
        }
        if (flag_name.find(flag_disable_prefix) != std::string_view::npos) {
            error("ERROR: flag name \"{}\" is incorrect. Name contains {}.", flag_name, flag_disable_prefix);
            return false;
        }
        if (flag_name.empty()) {
            error("ERROR: flag name is incorrect. Flag name is empty "
                  "string.");
            return false;
        }
        if (get_flag(flag_name) != nullptr) {
            error("ERROR: flag name \"{}\" is incorrect. Duplicate flag "
                  "name.",
                  flag_name);
            return false;
        }
        return true;
    }

    static bool is_flag(std::string_view str) {
        for (const auto &prefix : flag_prefixes) {
            if (str.starts_with(prefix)) {
                return true;
            }
        }
        return false;
    }

    std::array<Flag, max_flags> flags_ = {};
    std::array<FlagData, max_flags> flags_data_;
    size_t flag_count_ = 0;
    std::array<char, default_value_string_storage_size> default_value_string_storage_ = {};
    size_t default_value_string_storage_cursor_ = 0;
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

inline void print_defaults(std::ostreambuf_iterator<char> out, int flag_ident, int usage_indent) {
    global_context.parser.print_defaults(out, flag_ident, usage_indent);
}

inline void print_error(std::ostreambuf_iterator<char> out) {
    print_error(global_context.parse_result.error, out);
}

inline void print_usage(std::FILE *out) {
    std::stringbuf ss;
    print_usage(&ss);
    auto view = ss.view();
    std::fprintf(out, "%.*s", static_cast<unsigned int>(view.size()), view.data());
}

inline void print_defaults(std::FILE *out, int flag_ident, int usage_indent) {
    std::stringbuf ss;
    global_context.parser.print_defaults(&ss, flag_ident, usage_indent);
    auto view = ss.view();
    std::fprintf(out, "%.*s", static_cast<unsigned int>(view.size()), view.data());
}

inline void print_error(std::FILE *out) {
    std::stringbuf ss;
    print_error(global_context.parse_result.error, &ss);
    auto view = ss.view();
    std::fprintf(out, "%.*s", static_cast<unsigned int>(view.size()), view.data());
}

inline ErrorHandlerFunc *&error_handler() {
    return global_context.parser.error_handler;
}

inline const Error &get_error() {
    return global_context.parse_result.error;
}

inline void clear_error() {
    global_context.parse_result.error = {};
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

inline std::span<const Flag> get_flags() {
    return global_context.parser.get_flags();
}

inline void flag_bool_var(bool *out, std::string_view flag_name, bool default_value, std::string_view help) {
    global_context.parser.flag_bool_var(out, flag_name, default_value, help);
}

template <std::integral T, std::integral V>
void flag_int_var(T *out, std::string_view flag_name, V default_value, std::string_view help) {
    global_context.parser.flag_int_var(out, flag_name, default_value, help);
}

template <typename T, typename V>
void flag_list_var(std::vector<T> *out, std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
{
    global_context.parser.flag_list_var(out, flag_name, std::forward<V>(default_value), help);
}

template <std::floating_point T>
void flag_float_var(T *out, std::string_view flag_name, T default_value, std::string_view help) {
    global_context.parser.flag_float_var(out, flag_name, default_value, help);
}

inline void flag_string_var(std::string_view *out, std::string_view flag_name, std::string_view default_value,
                            std::string_view help) {
    global_context.parser.flag_string_var(out, flag_name, default_value, help);
}

inline bool *flag_bool(std::string_view flag_name, bool default_value, std::string_view help) {
    return global_context.parser.flag_bool(flag_name, default_value, help);
}

template <std::integral T>
T *flag_int(std::string_view flag_name, T default_value, std::string_view help) {
    return global_context.parser.flag_int(flag_name, default_value, help);
}

template <std::floating_point T>
T *flag_float(std::string_view flag_name, T default_value, std::string_view help) {
    return global_context.parser.flag_float(flag_name, default_value, help);
}

inline std::string_view *flag_string(std::string_view flag_name, std::string_view default_value,
                                     std::string_view help) {
    return global_context.parser.flag_string(flag_name, default_value, help);
}

template <typename T, typename V>
std::vector<T> *flag_list(std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<std::vector<T>, std::remove_cvref_t<V>>
{
    return global_context.parser.flag_list<T>(flag_name, std::forward<V>(default_value), help);
}

template <typename T, typename V>
T *flag(std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<T, std::remove_cvref_t<V>>
{
    return global_context.parser.flag<T>(flag_name, std::forward<V>(default_value), help);
}

template <typename T, typename V>
void flag_var(T *out, std::string_view flag_name, V &&default_value, std::string_view help)
    requires std::same_as<T, std::remove_cvref_t<V>>
{
    global_context.parser.flag_var(out, flag_name, std::forward<V>(default_value), help);
}

inline bool parse(int argc, char const *const *argv) {
    return parse(std::ranges::subrange(argv, argv + argc));
}

template <std::ranges::input_range Rng>
bool parse(Rng &&rng)
    requires std::convertible_to<std::ranges::range_value_t<Rng>, std::string_view>
{
    auto first = std::ranges::cbegin(rng);
    auto last  = std::ranges::cend(rng);

    if (first == last) {
        return true;
    }

    auto total = std::ranges::distance(first, last);
    global_context.program_name = *first;
    global_context.parse_result = global_context.parser.parse(std::ranges::subrange(++first, last));
    global_context.parse_result.args_parsed += 1; // Include program name
    global_context.args_left = static_cast<int>(total) - global_context.parse_result.args_parsed;
    return global_context.parse_result.error.kind == Error::Kind::none;
}

inline void print_error(const Error &error, std::ostreambuf_iterator<char> out) {
    switch (error.kind) {
        using enum Error::Kind;
        case none: {
            return;
        }

        case flag_provided_not_defined: {
            std::format_to(out, "flag provided but not defined: -{}\n", error.flag_name);
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
            std::format_to(out, "invalid value format for -{}\n", error.flag_name);
            return;
        }
        case out_of_range: {
            std::format_to(out, "value ouf of range for -{}\n", error.flag_name);
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

void skip_whitespaces(std::string_view &str) {
    while (!str.empty() && str[0] == ' ') {
        str.remove_prefix(1);
    }
};

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

    std::ostreambuf_iterator<char> output_type_name(std::ostreambuf_iterator<char> out) const override {
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

        static constexpr std::array true_values = {
            "true"sv, "True"sv, "TRUE"sv, "t"sv, "T"sv, "1"sv,
        };
        for (const auto &true_value : true_values) {
            if (true_value == word) {
                *value = true;
                return Error::Kind::none;
            }
        }

        static constexpr std::array false_values = {
            "false"sv, "False"sv, "FALSE"sv, "f"sv, "F"sv, "0"sv,
        };
        for (const auto &false_value : false_values) {
            if (false_value == word) {
                *value = false;
                return Error::Kind::none;
            }
        }

        return Error::Kind::incorrect_format;
    }

    std::ostreambuf_iterator<char> output_type_name(std::ostreambuf_iterator<char> out) const override {
        return std::format_to(out, "bool");
    }

    bool behaves_like_bool() const override {
        return true;
    }

    void set_flag_present() override {
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

    std::ostreambuf_iterator<char> output_type_name(std::ostreambuf_iterator<char> out) const override {
        return std::format_to(out, "string");
    }
};

template<typename T>
struct IsVector : std::false_type {};

template<typename T, typename Alloc>
struct IsVector<std::vector<T, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool IsVectorV = IsVector<T>::value;

template <typename T>
struct FlagValueImpl<std::vector<T>> : public FlagValue {
    std::vector<T> *value = nullptr;
    bool first = true;

    FlagValueImpl(std::vector<T> *value) : value{value} {
    }

    Error::Kind set_value(std::string_view &flag_value) override {
        auto parse_and_add_value = [this](std::string_view &value_string) -> Error::Kind {
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
            [parse_and_add_value](std::string_view &quoted_string) -> Error::Kind {
            std::size_t i = 0;
            while (i < quoted_string.size() && quoted_string[i] != ',' && quoted_string[i] != ']') {
                i += 1;
            }

            auto str = quoted_string.substr(0, i);
            quoted_string = quoted_string.substr(i);
            return parse_and_add_value(str);
        };

        if (!flag_value.starts_with('[')) {
            if constexpr (std::convertible_to<T, std::string_view>) {
                // Have to use parse_string_untill_comma_or_close_bracket here because we might be
                // parsing, for example, list of lists of strings
                // So if we are parsing list of lists of strings and the input is [[a], b, c]
                // And call parse_and_add_value instead that will parse "b, c]" as a string
                // And with parse_string_untill_comma_or_close_bracket we will parse b and c as lists with 1 string
                return parse_string_untill_comma_or_close_bracket(flag_value);
            }
            return parse_and_add_value(flag_value);
        }
        auto flag_value_before = flag_value;
        flag_value.remove_prefix(1);
        skip_whitespaces(flag_value);
        if constexpr (IsVectorV<T>) {
            // For lists, if the next char is not [ we will get list of lists with 1 element
            // So instead, put [ back to flag_value and then call parse_and_add_value()
            // That will produce list of 1 element (and that element will be a list of provided elements)
            // Example:
            //     Input: [1, 2, 3]
            //     If we eat [ and do not put it back to input we will get [[1], [2], [3]]
            //     But if we put [ back to input we get [[1, 2, 3]]
            //     And i think it is better behaviour
            if (!flag_value.starts_with('[')) {
                flag_value = flag_value_before;
                return parse_and_add_value(flag_value);
            }
        }

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

    std::ostreambuf_iterator<char> output_type_name(std::ostreambuf_iterator<char> out) const override {
        auto flag_value = FlagValueImpl<T>{nullptr};
        out = std::format_to(out, "List<");
        out = flag_value.output_type_name(out);
        out = std::format_to(out, ">");
        return out;
    }
};

}; // namespace fhpp

#endif // #ifndef FLAG_HPP
