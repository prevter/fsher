#pragma once
#include "node.hpp"
#include "../../semantic/symbol_table.hpp"

namespace fsher {
    class ExprNode : public Node {
    public:
        ExprNode(SourceRange const& range, Type type) noexcept : Node(range, type) {}

        [[nodiscard]] fsher::Type resolvedType() const { return m_resolvedType; }
        void setResolvedType(fsher::Type const& type) const { m_resolvedType = type; }

    private:
        mutable fsher::Type m_resolvedType{ .kind = fsher::Type::Kind::Unresolved };
    };
}