#pragma once
#include <memory>
#include <string_view>
#include <fmt/format.h>

#include "block.hpp"
#include "expr.hpp"
#include "stmt.hpp"

namespace fsher {
    class ForStmtNode : public StmtNode {
    public:
        ForStmtNode(
            SourceRange const& range,
            std::string_view loopVar,
            std::unique_ptr<ExprNode> rangeStart,
            std::unique_ptr<ExprNode> rangeEnd,
            bool inclusive,
            std::unique_ptr<BlockNode> body
        ) noexcept : StmtNode(range, Type::ForStmt),
                     m_loopVar(loopVar),
                     m_rangeStart(std::move(rangeStart)),
                     m_rangeEnd(std::move(rangeEnd)),
                     m_inclusive(inclusive),
                     m_body(std::move(body)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}ForStmtNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}LoopVar: '{}',\n", "", indent + 1, m_loopVar);
            fmt::format_to(out, "{:>{}}Inclusive: {},\n", "", indent + 1, m_inclusive);
            fmt::format_to(out, "{:>{}}RangeStart:\n", "", indent + 1);
            m_rangeStart->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}RangeEnd:\n", "", indent + 1);
            m_rangeEnd->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}Body:\n", "", indent + 1);
            m_body->debug(out, indent + 2);
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] std::string_view loopVar() const noexcept { return m_loopVar; }
        [[nodiscard]] ExprNode* rangeStart() const noexcept { return m_rangeStart.get(); }
        [[nodiscard]] ExprNode* rangeEnd() const noexcept { return m_rangeEnd.get(); }
        [[nodiscard]] bool inclusive() const noexcept { return m_inclusive; }
        [[nodiscard]] BlockNode* body() const noexcept { return m_body.get(); }

    private:
        std::string_view m_loopVar;
        std::unique_ptr<ExprNode> m_rangeStart;
        std::unique_ptr<ExprNode> m_rangeEnd;
        bool m_inclusive;
        std::unique_ptr<BlockNode> m_body;
    };
}
