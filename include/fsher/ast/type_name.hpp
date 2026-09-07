#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
#include <fmt/format.h>

#include "../tokens.hpp"

namespace fsher {
    struct TypeName {
        std::string_view name;
        std::vector<uint64_t> arrayDims;
        SourceRange range;

        bool empty() const noexcept { return name.empty(); }
        bool isArray() const noexcept { return !arrayDims.empty(); }
    };
}

template <>
struct fmt::formatter<fsher::TypeName> {
    static constexpr auto parse(format_parse_context& ctx) noexcept {
        return ctx.begin();
    }

    auto format(fsher::TypeName const& type, format_context& ctx) const noexcept {
        fmt::format_to(ctx.out(), "{}", type.name);
        for (auto const& dim : type.arrayDims) {
            fmt::format_to(ctx.out(), "[{}]", dim);
        }
        return ctx.out();
    }
};