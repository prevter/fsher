#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "../semantic/analyzer.hpp"
#include "../semantic/symbol_table.hpp"

namespace fsher::ir {
    enum class Semantic : uint8_t {
        None = 0,
        Position,
        FragColor,
        VertexId,
        InstanceId,
        PrimitiveId,
        FragCoord,
        FrontFacing,
        PointCoord,
        MVP,
        Model,
        View,
        Projection,
    };

    enum class Interpolation : uint8_t {
        Smooth = 0,
        Flat,
        NoPerspective,
    };

    struct Expr;

    struct Literal {
        std::variant<int64_t, double, bool> value;
    };

    struct VarRef {
        std::string name;
    };

    struct Unary {
        TokenType op;
        std::unique_ptr<Expr> operand;
    };

    struct Binary {
        TokenType op;
        std::unique_ptr<Expr> lhs, rhs;
    };

    struct Call {
        std::string callee;
        std::vector<Expr> args;
    };

    struct FieldAccess {
        std::unique_ptr<Expr> target;
        std::string field;
    };

    struct IndexAccess {
        std::unique_ptr<Expr> target;
        std::unique_ptr<Expr> index;
    };

    struct StructFieldEntry;

    struct StructLit {
        std::string typeName;
        std::vector<StructFieldEntry> fields;
    };

    struct Ternary {
        std::unique_ptr<Expr> condition;
        std::unique_ptr<Expr> thenExpr;
        std::unique_ptr<Expr> elseExpr;
    };

    struct Expr {
        Type type;
        std::variant<Literal, VarRef, Unary, Binary, Call, FieldAccess, IndexAccess, StructLit, Ternary> node;
    };

    struct StructFieldEntry {
        std::string name;
        Expr value;
    };

    struct Stmt;

    struct LetStmt {
        std::string name;
        Type type;
        std::optional<Expr> value;
    };

    struct AssignStmt {
        Expr target;
        TokenType op;
        Expr value;
    };

    struct IfStmt {
        Expr condition;
        std::vector<Stmt> thenBody;
        std::vector<Stmt> elseBody;
    };

    struct ForStmt {
        std::string loopVar;
        Expr rangeStart, rangeEnd;
        bool inclusive;
        std::vector<Stmt> body;
    };

    struct ReturnStmt {
        std::optional<Expr> value;
    };

    struct ExprStmt {
        Expr expr;
    };

    struct ContinueStmt {};

    struct BreakStmt {};

    struct DiscardStmt {};

    struct Stmt {
        using T = std::variant<
            LetStmt, AssignStmt, IfStmt, ForStmt, ReturnStmt,
            ExprStmt, ContinueStmt, BreakStmt, DiscardStmt
        >;
        T node;
    };

    struct Param {
        std::string name;
        Type type;
    };

    struct Function {
        std::string name;
        std::vector<Param> params;
        Type returnType;
        std::vector<Stmt> body;
        bool isEntry = false;
    };

    struct GlobalVar {
        std::string name;
        Type type;
        std::optional<uint32_t> location;
        std::optional<uint32_t> binding;
        std::optional<uint32_t> set;
        Semantic semantic = Semantic::None;
        Interpolation interpolation = Interpolation::Smooth;
    };

    struct StructField {
        std::string name;
        Type type;
    };

    struct StructDecl {
        std::string name;
        std::vector<StructField> fields;
    };

    struct ConstGlobal {
        std::string name;
        Type type;
        Expr value;
    };

    struct StageModule {
        Stage stage;
        std::vector<GlobalVar> inputs;
        std::vector<GlobalVar> outputs;
        std::vector<GlobalVar> uniforms;
        std::vector<Function> functions;
        std::vector<BuiltinID> usedBuiltins;
    };

    struct ShaderProgram {
        std::vector<StageModule> stages;
        std::vector<StructDecl> structs;
        std::vector<ConstGlobal> constants;
    };
}
