#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    class TernaryExprNode : public ExprNode {
    public:
        TernaryExprNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> condition,
            std::unique_ptr<ExprNode> thenExpr,
            std::unique_ptr<ExprNode> elseExpr
        ) noexcept : ExprNode(range, Type::TernaryExpr),
                     m_condition(std::move(condition)),
                     m_then(std::move(thenExpr)),
                     m_else(std::move(elseExpr)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}TernaryExprNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Condition:\n", "", indent + 1);
            m_condition->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Then:\n", "", indent + 1);
            m_then->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Else:\n", "", indent + 1);
            m_else->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* condition() const noexcept { return m_condition.get(); }
        [[nodiscard]] ExprNode* thenExpr() const noexcept { return m_then.get(); }
        [[nodiscard]] ExprNode* elseExpr() const noexcept { return m_else.get(); }

    private:
        std::unique_ptr<ExprNode> m_condition;
        std::unique_ptr<ExprNode> m_then;
        std::unique_ptr<ExprNode> m_else;
    };
}