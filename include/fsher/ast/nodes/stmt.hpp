#pragma once
#include "node.hpp"

namespace fsher {
    class StmtNode : public Node {
    public:
        StmtNode(SourceRange const& range, Type type) noexcept : Node(range, type) {}
    };
}