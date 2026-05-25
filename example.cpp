#include "flag2.hpp"
#include <cstdio>
#include <format>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

static bool operator==(const Vec2 &left, const Vec2 &right) {
    return left.x == right.x && left.y == right.y;
}

template <>
struct std::formatter<Vec2> {
    template <class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <class FmtContext>
    FmtContext::iterator format(const Vec2 &vec, FmtContext &ctx) const {
        return std::format_to(ctx.out(), "({}; {})", vec.x, vec.y);
    }
};

int main(int argc, char **argv) {
    auto &help = *FlagHpp::flag_bool("help", false, "Show this help message.");

    auto boolean = FlagHpp::flag_bool("bool", true, "A boolean.");
    auto integer = FlagHpp::flag_int("int", 7, "An integer.");
    auto size = FlagHpp::flag_int<size_t>("size", 0, "A size.");
    auto float_ = FlagHpp::flag_float("float", 0.0f, "A float.");
    auto double_ = FlagHpp::flag_float("double", -0.1, "A double.");
    auto str = FlagHpp::flag_string("string", "str", "A string.");
    auto vec = FlagHpp::flag<Vec2>("vec", Vec2{1.0f, -1.0f}, "A Vector2. Format: (x; y).");
    auto ints_list = FlagHpp::flag_list<int>("ints", std::vector{1, 2, 3},
                                        "List of integers. Format: i0, i1, ....");
    auto vecs_list = FlagHpp::flag_list<Vec2>("vecs", std::vector{Vec2{1.0f, 0.0f},
                                        Vec2{0.0f, 1.0f}}, "List of Vector2.");
    
    size_t size2 = {};
    FlagHpp::flag_int_var(&size2, "size2", 0, "A size.");

    if (auto ok = FlagHpp::parse(argc, argv); !ok || help) {
        FlagHpp::print_usage(stdout);
        return ok ? 0 : 1;
    }

    auto longest_flag_name = 0;
    for (const auto &flag : FlagHpp::get_flags()) {
        if (std::ssize(flag.name) > longest_flag_name) {
            longest_flag_name = static_cast<int>(std::ssize(flag.name));
        }
    }

    auto out = std::ostreambuf_iterator(std::cout);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(integer)->name, longest_flag_name, *integer);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(boolean)->name, longest_flag_name, *boolean);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(size)->name, longest_flag_name, *size);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(&size2)->name, longest_flag_name, size2);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(float_)->name, longest_flag_name, *float_);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(double_)->name, longest_flag_name, *double_);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(str)->name, longest_flag_name, *str);
    out = std::format_to(out, "{:{}} = {}\n", FlagHpp::get_flag(vec)->name, longest_flag_name, *vec);
    out = std::format_to(out, "{:{}} = ", FlagHpp::get_flag(ints_list)->name, longest_flag_name);
    for (auto i = 0; i < std::ssize(*ints_list); ++i) {
        out = std::format_to(out, "{}", (*ints_list)[i]);
        if (i != std::ssize(*ints_list) - 1) {
            out = std::format_to(out, ", ");
        }
    }
    out = std::format_to(out, "\n{:{}} = ", FlagHpp::get_flag(vecs_list)->name, longest_flag_name);
    for (auto i = 0; i < std::ssize(*vecs_list); ++i) {
        out = std::format_to(out, "{}", (*vecs_list)[i]);
        if (i != std::ssize(*vecs_list) - 1) {
            out = std::format_to(out, ", ");
        }
    }
    *out = '\n';

    if (FlagHpp::args_left() > 0) {
        std::cout << "Rest of the arguments:\n";
        for (int i = FlagHpp::args_parsed(); i < argc; ++i) {
            std::cout << argv[i];
        }
    }

    return 0;
}

template <>
struct FlagHpp::FlagValueImpl<Vec2> : public FlagValue {
    Vec2 *value = nullptr;

    FlagValueImpl(Vec2 *value) : value{value} {
    }

    Error::Kind set_value(std::string_view flag_value) override {
        if (!(flag_value.starts_with('(') && flag_value.ends_with(')'))) {
            return Error::Kind::incorrect_format;
        }

        flag_value.remove_prefix(1);
        flag_value.remove_suffix(1);

        auto comma_index = flag_value.find(';');
        if (comma_index == std::string_view::npos) {
            return Error::Kind::incorrect_format;
        }

        auto x_string = trim(flag_value.substr(0, comma_index));
        auto y_string = trim(flag_value.substr(comma_index + 1));

        auto result = Vec2{};
        auto error = FlagHpp::parse_number(x_string, &result.x);
        if (error != Error::Kind::none) {
            return error;
        }

        error = FlagHpp::parse_number(y_string, &result.y);
        if (error != Error::Kind::none) {
            return error;
        }

        *value = result;
        return Error::Kind::none;
    }

    std::ostreambuf_iterator<char>
    output_type_name(std::ostreambuf_iterator<char> out) const override {
        return std::format_to(out, "Vec2");
    }
};
