#pragma once
#include <string_view>
#include <fmt/format.h>

#include "expr.hpp"

namespace fsher {
    class IdentifierExprNode : public ExprNode {
    public:
        IdentifierExprNode(SourceRange const& range, std::string_view name) noexcept
            : ExprNode(range, Type::IdentifierExpr), m_name(name) {}

        [[nodiscard]] std::string_view name() const noexcept { return m_name; }

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}IdentifierExprNode {{ name: '{}' }}\n", "", indent, m_name);
        }

    private:
        std::string_view m_name;
    };
}