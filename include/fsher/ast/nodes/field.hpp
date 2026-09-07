#pragma once
#include <memory>
#include <string_view>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    class FieldExprNode : public ExprNode {
    public:
        FieldExprNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> target,
            std::string_view field
        ) noexcept : ExprNode(range, Type::FieldExpr),
                     m_target(std::move(target)),
                     m_field(field) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}FieldExprNode {{ field: '{}',\n", "", indent, m_field);
            m_target->debug(out, indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* target() const noexcept { return m_target.get(); }
        [[nodiscard]] std::string_view field() const noexcept { return m_field; }

    private:
        std::unique_ptr<ExprNode> m_target;
        std::string_view m_field;
    };
}