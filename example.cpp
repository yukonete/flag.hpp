#include <cstdio>
#include <print>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "flag.hpp"

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    friend bool operator==(const Vec2 &left, const Vec2 &right) {
        return left.x == right.x && left.y == right.y;
    }
};

// In order for default value of a type to be displayed formatter has be to defined for it
template <>
struct std::formatter<Vec2> : public std::formatter<std::string_view> {
    template <class FmtContext>
    FmtContext::iterator format(const Vec2 &vec, FmtContext &ctx) const {
        std::array<char, 256> buffer = {};
        auto result = std::format_to_n(buffer.data(), buffer.size(), "({}, {})", vec.x, vec.y);
        auto written = static_cast<size_t>(result.out - buffer.begin());
        auto str = std::string_view{buffer.data(), written};
        return std::formatter<std::string_view>::format(str, ctx);
    }
};

int main(int argc, char **argv) {
    auto &help = *fhpp::flag_bool("help", false, "Show this help message.");

    auto boolean    = fhpp::flag_bool("bool", true, "A boolean.");
    auto integer    = fhpp::flag_int("int", 7, "An integer.");
    auto size       = fhpp::flag_int<size_t>("size", 0, "A size.");
    auto float_     = fhpp::flag_float("float", 0.0f, "A float.");
    auto double_    = fhpp::flag_float("double", -0.1, "A double.");
    auto str        = fhpp::flag_string("string", "str", "A string.");
    auto vec        = fhpp::flag<Vec2>("vec", Vec2{1.0f, -1.0f}, "A Vector2. Format: (x, y).");
    // Uses std::vector to store values
    auto ints_list  = fhpp::flag_list<int>("ints", {1, 2}, "List of ints. Format: [i0, i1, ...]");
    auto lists_list = fhpp::flag_list<std::vector<int>>("lists", {},
        "List of lists of ints. Format: [[i0, i1, ...], [i0, i1, ...], ...]");

    size_t size2 = {};
    fhpp::flag_int_var(&size2, "size2", 0, "A size.");

    if (auto ok = fhpp::parse(argc, argv); !ok || help) {
        fhpp::print_usage(std::cout);
        return ok ? 0 : 1;
    }

    std::size_t longest_flag_name = 0;
    for (const auto &flag : fhpp::get_flags()) {
        if (flag.name.size() > longest_flag_name) {
            longest_flag_name = flag.name.size();
        }
    }

    std::println("{:{}} = {}", fhpp::get_flag(integer)->name, longest_flag_name, *integer);
    std::println("{:{}} = {}", fhpp::get_flag(boolean)->name, longest_flag_name, *boolean);
    std::println("{:{}} = {}", fhpp::get_flag(size)->name, longest_flag_name, *size);
    std::println("{:{}} = {}", fhpp::get_flag(float_)->name, longest_flag_name, *float_);
    std::println("{:{}} = {}", fhpp::get_flag(double_)->name, longest_flag_name, *double_);
    std::println("{:{}} = {}", fhpp::get_flag(str)->name, longest_flag_name, *str);
    std::println("{:{}} = {}", fhpp::get_flag(vec)->name, longest_flag_name, *vec);
    std::println("{:{}} = {}", fhpp::get_flag(ints_list)->name, longest_flag_name, *ints_list);
    std::println("{:{}} = {}", fhpp::get_flag(lists_list)->name, longest_flag_name, *lists_list);
    std::println("{:{}} = {}", fhpp::get_flag(&size2)->name, longest_flag_name, size2);

    if (fhpp::args_left() > 0) {
        std::cout << "Rest of the arguments:\n";
        for (int i = fhpp::args_parsed(); i < argc; ++i) {
            std::println("{}", argv[i]);
        }
    }

    return 0;
}

template <>
struct fhpp::FlagValueImpl<Vec2> : public FlagValue {
    Vec2 *value = nullptr;

    FlagValueImpl(Vec2 *value) : value{value} {
    }

    // set_value should try to parse value from the beginning of flag_value string
    // and do not report error if there are any more characters left after parsing
    Error::Kind set_value(std::string_view &flag_value) override {
        if (!flag_value.starts_with('(')) {
            return Error::Kind::incorrect_format;
        }
        flag_value.remove_prefix(1);
        skip_whitespaces(flag_value);

        auto vec = Vec2{};
        auto error_x = fhpp::parse_number<float>(flag_value, vec.x);
        if (error_x != Error::Kind::none) {
            return error_x;
        }
        skip_whitespaces(flag_value);

        if (!flag_value.starts_with(',')) {
            return Error::Kind::incorrect_format;
        }
        flag_value.remove_prefix(1);
        skip_whitespaces(flag_value);

        auto error_y = fhpp::parse_number<float>(flag_value, vec.y);
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