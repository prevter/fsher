#pragma once
#include <memory>
#include <string_view>
#include <fmt/format.h>

#include "expr.hpp"
#include "stmt.hpp"
#include "../type_name.hpp"

namespace fsher {
    class ConstStmtNode : public StmtNode {
    public:
        ConstStmtNode(
            SourceRange const& range,
            Token const& name,
            TypeName typeName,
            std::unique_ptr<ExprNode> initializer
        ) noexcept : StmtNode(range, Type::ConstStmt),
                     m_identifier(name),
                     m_name(name.lexeme),
                     m_typeName(std::move(typeName)),
                     m_initializer(std::move(initializer)) {}

        void debug(fmt::appender out, int indent = 0) const override {
            fmt::format_to(out, "{:>{}}ConstStmtNode {{\n", "", indent);
            fmt::format_to(out, "{:>{}}Name: '{}',\n", "", indent + 1, m_name);
            if (!m_typeName.empty()) {
                fmt::format_to(out, "{:>{}}Type: '{}',\n", "", indent + 1, m_typeName);
            }
            if (m_initializer) {
                fmt::format_to(out, "{:>{}}Initializer:\n", "", indent + 1);
                m_initializer->debug(out, indent + 2);
            }
            fmt::format_to(out, "{:>{}}}}\n", "", indent);
        }

        [[nodiscard]] Token const& identifier() const noexcept { return m_identifier; }
        [[nodiscard]] std::string_view name() const noexcept { return m_name; }
        [[nodiscard]] TypeName const& typeName() const noexcept { return m_typeName; }
        [[nodiscard]] ExprNode* initializer() const noexcept { return m_initializer.get(); }

    private:
        Token m_identifier;
        std::string_view m_name;
        TypeName m_typeName;
        std::unique_ptr<ExprNode> m_initializer;
    };
}
