#pragma once
#include <memory>
#include <variant>
#include <fmt/format.h>

#include "block.hpp"
#include "expr.hpp"

namespace fsher {
    class IfExprNode : public ExprNode {
    public:
        using ElseBranch = std::variant<std::monostate, std::unique_ptr<IfExprNode>, std::unique_ptr<BlockNode>>;

        IfExprNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> condition,
            std::unique_ptr<BlockNode> thenBlock,
            ElseBranch elseBranch
        ) noexcept : ExprNode(range, Type::IfExpr),
                     m_condition(std::move(condition)),
                     m_then(std::move(thenBlock)),
                     m_else(std::move(elseBranch)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}IfExprNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Condition:\n", "", indent + 1);
            m_condition->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Then:\n", "", indent + 1);
            m_then->debug(out, indent + 2);
            std::visit([&]<typename T0>(T0 const& branch) {
                using T = std::decay_t<T0>;
                if constexpr (!std::is_same_v<T, std::monostate>) {
                    fmt::format_to(out, "{:>{}}Else:\n", "", indent + 1);
                    branch->debug(out, indent + 2);
                }
            }, m_else);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        ExprNode* condition() const noexcept { return m_condition.get(); }
        BlockNode* thenBlock() const noexcept { return m_then.get(); }

        enum class ElseKind {
            None, ElseIf, Else
        };

        ElseKind elseKind() const noexcept { return static_cast<ElseKind>(m_else.index()); }

        IfExprNode* elseIfBranch() const noexcept { return std::get<1>(m_else).get(); }
        BlockNode* elseBlock() const noexcept { return std::get<2>(m_else).get(); }

    private:
        std::unique_ptr<ExprNode> m_condition;
        std::unique_ptr<BlockNode> m_then;
        ElseBranch m_else;
    };
}