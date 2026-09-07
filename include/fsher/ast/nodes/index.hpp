#pragma once
#include <memory>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    class IndexExprNode : public ExprNode {
    public:
        IndexExprNode(
            SourceRange const& range,
            std::unique_ptr<ExprNode> target,
            std::unique_ptr<ExprNode> index
        ) noexcept : ExprNode(range, Type::IndexExpr),
                     m_target(std::move(target)),
                     m_index(std::move(index)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}IndexExprNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Target:\n", "", indent + 1);
            m_target->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Index:\n", "", indent + 1);
            m_index->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] ExprNode* target() const noexcept { return m_target.get(); }
        [[nodiscard]] ExprNode* index() const noexcept { return m_index.get(); }

    private:
        std::unique_ptr<ExprNode> m_target;
        std::unique_ptr<ExprNode> m_index;
    };
}