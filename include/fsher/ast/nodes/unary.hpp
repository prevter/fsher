#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"
#include "../../tokens.hpp"

namespace fsher {
    class UnaryExprNode : public ExprNode {
    public:
        UnaryExprNode(
            SourceRange const& range,
            TokenType op,
            std::unique_ptr<ExprNode> operand
        ) noexcept : ExprNode(range, Type::UnaryExpr),
                     m_operand(std::move(operand)),
                     m_op(op) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}UnaryExprNode {{ op: {},\n", "", indent, m_op);
            m_operand->debug(out, indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        ExprNode* operand() const noexcept { return m_operand.get(); }
        [[nodiscard]] TokenType op() const noexcept { return m_op; }

    private:
        std::unique_ptr<ExprNode> m_operand;
        TokenType m_op;
    };
}