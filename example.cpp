#include <cstdio>
#include <format>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "flag.hpp"

#if !(defined(__cpp_lib_format_ranges) && (__cpp_lib_format_ranges == 202207L))
template <typename T>
struct std::formatter<std::vector<T>> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(const std::vector<T> &vec, FmtContext &ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "["); 
        for (std::size_t i = 0; i < vec.size(); ++i) {
            if (i != 0) {
                out = std::format_to(out, ", "); 
            }
            out = std::format_to(out, "{}", vec[i]); 
        }
        out = std::format_to(out, "]"); 
        return out;
    }
};
#endif

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    friend bool operator==(const Vec2 &left, const Vec2 &right) {
        return left.x == right.x && left.y == right.y;
    }
};

template <>
struct std::formatter<Vec2> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(const Vec2 &vec, FmtContext &ctx) const {
        return std::format_to(ctx.out(), "({}, {})", vec.x, vec.y);
    }
};

template <typename... Args>
void print(std::ostream &out, std::format_string<Args...> fmt, Args &&...args) {
    std::format_to(std::ostreambuf_iterator(out), fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void print(std::format_string<Args...> fmt, Args &&...args) {
    print(std::cout, fmt, std::forward<Args>(args)...);
}

int main(int argc, char **argv) {
    auto &help = *FlagHpp::flag_bool("help", false, "Show this help message.");

    auto boolean = FlagHpp::flag_bool("bool", true, "A boolean.");
    auto integer = FlagHpp::flag_int("int", 7, "An integer.");
    auto size = FlagHpp::flag_int<size_t>("size", 0, "A size.");
    auto float_ = FlagHpp::flag_float("float", 0.0f, "A float.");
    auto double_ = FlagHpp::flag_float("double", -0.1, "A double.");
    auto str = FlagHpp::flag_string("string", "str", "A string.");
    auto vec = FlagHpp::flag<Vec2>("vec", Vec2{1.0f, -1.0f},
                                   "A Vector2. Format: (x, y).");
    auto vecs_list = FlagHpp::flag_list<Vec2>(
        "vecs", std::vector{Vec2{1.0f, 0.0f}, Vec2{0.0f, 1.0f}},
        "List of Vector2. Format: [v0, v1, ...]");
    auto strings_list =
        FlagHpp::flag_list<std::string_view>("strings", {}, "List of strings.");
    auto lists_list = FlagHpp::flag_list<std::vector<int>>(
        "lists", {},
        "List of lists of ints. Format: [[i0, i1, ...], [i0, i1, ...], ...]");

    size_t size2 = {};
    FlagHpp::flag_int_var(&size2, "size2", 0, "A size.");

    if (auto ok = FlagHpp::parse(argc, argv); !ok || help) {
        FlagHpp::print_usage(stdout);
        return ok ? 0 : 1;
    }

    std::size_t longest_flag_name = 0;
    for (const auto &flag : FlagHpp::get_flags()) {
        if (flag.name.size() > longest_flag_name) {
            longest_flag_name = flag.name.size();
        }
    }

    print("{:{}} = {}\n", FlagHpp::get_flag(integer)->name, longest_flag_name, *integer);
    print("{:{}} = {}\n", FlagHpp::get_flag(boolean)->name, longest_flag_name, *boolean);
    print("{:{}} = {}\n", FlagHpp::get_flag(size)->name, longest_flag_name, *size);
    print("{:{}} = {}\n", FlagHpp::get_flag(&size2)->name, longest_flag_name, size2);
    print("{:{}} = {}\n", FlagHpp::get_flag(float_)->name, longest_flag_name, *float_);
    print("{:{}} = {}\n", FlagHpp::get_flag(double_)->name, longest_flag_name, *double_);
    print("{:{}} = {}\n", FlagHpp::get_flag(str)->name, longest_flag_name, *str);
    print("{:{}} = {}\n", FlagHpp::get_flag(vec)->name, longest_flag_name, *vec);
    print("{:{}} = {}\n", FlagHpp::get_flag(vecs_list)->name, longest_flag_name, *vecs_list);
    print("{:{}} = {}\n", FlagHpp::get_flag(strings_list)->name, longest_flag_name, *strings_list);
    print("{:{}} = {}\n", FlagHpp::get_flag(lists_list)->name, longest_flag_name, *lists_list);

    if (FlagHpp::args_left() > 0) {
        std::cout << "Rest of the arguments:\n";
        for (int i = FlagHpp::args_parsed(); i < argc; ++i) {
            print("{}\n", argv[i]);
        }
    }

    return 0;
}

template <>
struct FlagHpp::FlagValueImpl<Vec2> : public FlagValue {
    Vec2 *value = nullptr;

    FlagValueImpl(Vec2 *value) : value{value} {
    }

    Error::Kind set_value(std::string_view &flag_value) override {
        if (!flag_value.starts_with('(')) {
            return Error::Kind::incorrect_format;
        }
        flag_value.remove_prefix(1);
        skip_whitespaces(flag_value);

        auto vec = Vec2{};
        auto error_x = FlagHpp::parse_number<float>(flag_value, vec.x);
        if (error_x != Error::Kind::none) {
            return error_x;
        }
        skip_whitespaces(flag_value);

        if (!flag_value.starts_with(',')) {
            return Error::Kind::incorrect_format;
        }
        flag_value.remove_prefix(1);
        skip_whitespaces(flag_value);

        auto error_y = FlagHpp::parse_number<float>(flag_value, vec.y);
        if (error_y != Error::Kind::none) {
            return error_y;
        }
        skip_whitespaces(flag_value);

        if (!flag_value.starts_with(')')) {
            return Error::Kind::incorrect_format;
        }
        flag_value.remove_prefix(1);

        *value = vec;
        return Error::Kind::none;
    }

    std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const override {
        return std::format_to(out, "Vec2");
    }
};
