#pragma once
#include <memory>
#include <string_view>
#include <vector>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    struct FieldInit {
        std::string_view name;
        std::unique_ptr<ExprNode> value;
    };

    class StructLiteralExprNode : public ExprNode {
    public:
        StructLiteralExprNode(
            SourceRange const& range,
            std::string_view typeName,
            std::vector<FieldInit> fields
        ) noexcept : ExprNode(range, Type::StructLiteralExpr),
                     m_typeName(typeName),
                     m_fields(std::move(fields)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}StructLiteralExprNode {{ type: '{}',\n", "", indent, m_typeName);
            fmt::format_to(out, "{:>{}}Fields: [\n", "", indent + 1);
            for (auto const& field : m_fields) {
                fmt::format_to(out, "{:>{}}'{}':\n", "", indent + 2, field.name);
                field.value->debug(out, indent + 3);
            }
            fmt::format_to(out, "{:>{}}]\n", "", indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] std::string_view typeName() const noexcept { return m_typeName; }
        [[nodiscard]] std::vector<FieldInit> const& fields() const noexcept { return m_fields; }

    private:
        std::string_view m_typeName;
        std::vector<FieldInit> m_fields;
    };
}