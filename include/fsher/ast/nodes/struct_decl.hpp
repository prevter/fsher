#pragma once
#include <cstdint>
#include <fmt/ranges.h>

#include "item.hpp"
#include "../type_name.hpp"

namespace fsher {
    struct StructField {
        std::vector<Attribute> attributes;
        std::string_view name;
        TypeName type;
        SourceRange range;
    };

    class StructDeclNode : public ItemNode {
    public:
        StructDeclNode(SourceRange const& range, std::vector<Attribute> attributes, std::string_view name, std::vector<StructField> fields) noexcept
            : ItemNode(range, Type::StructDecl, std::move(attributes)), m_fields(std::move(fields)), m_name(name) {}

        void debug(fmt::appender out, int indent = 0) const override;

        std::vector<StructField> const& fields() const noexcept { return m_fields; }
        std::string_view name() const noexcept { return m_name; }

    private:
        std::vector<StructField> m_fields;
        std::string_view m_name;
    };
}

template <>
struct fmt::formatter<fsher::StructField> {
    static constexpr auto parse(format_parse_context& ctx) noexcept {
        return ctx.begin();
    }

    auto format(fsher::StructField const& field, format_context& ctx) const noexcept {
        return fmt::format_to(
            ctx.out(), "StructField {{ name: '{}', type: '{}', attributes: [{}] }}",
            field.name, field.type, fmt::join(field.attributes, ", ")
        );
    }
};

inline void fsher::StructDeclNode::debug(fmt::appender out, int indent) const {
    fmt::format_to(out, "{:>{}}StructDeclNode {{\n", "", indent);
    fmt::format_to(out, "{:>{}}Name: '{}',\n", "", indent + 1, m_name);
    fmt::format_to(out, "{:>{}}Fields: [\n", "", indent + 1);
    for (auto const& field : m_fields) {
        fmt::format_to(out, "{:>{}}{},\n", "", indent + 2, field);
    }
    fmt::format_to(out, "{:>{}}]\n", "", indent + 1);
    if (!this->attributes().empty()) {
        fmt::format_to(out, "{:>{}}Attributes: [ {} ]\n", "", indent + 1, fmt::join(this->attributes(), ", "));
    }
    fmt::format_to(out, "{:>{}}}}\n", "", indent);
}