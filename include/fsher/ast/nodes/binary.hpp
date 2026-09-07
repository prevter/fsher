#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"
#include "../../tokens.hpp"

namespace fsher {
    class BinaryExprNode : public ExprNode {
    public:
        BinaryExprNode(
            SourceRange const& range,
            TokenType op,
            std::unique_ptr<ExprNode> lhs,
            std::unique_ptr<ExprNode> rhs
        ) noexcept : ExprNode(range, Type::BinaryExpr),
                     m_lhs(std::move(lhs)),
                     m_rhs(std::move(rhs)),
                     m_op(op) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}BinaryExprNode {{ op: {},\n", "", indent, m_op);
            m_lhs->debug(out, indent + 1);
            m_rhs->debug(out, indent + 1);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        ExprNode* lhs() const noexcept { return m_lhs.get(); }
        ExprNode* rhs() const noexcept { return m_rhs.get(); }
        TokenType op() const noexcept { return m_op; }

    private:
        std::unique_ptr<ExprNode> m_lhs;
        std::unique_ptr<ExprNode> m_rhs;
        TokenType m_op;
    };
}