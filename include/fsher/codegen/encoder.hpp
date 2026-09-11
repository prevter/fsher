#pragma once
#include "ir.hpp"

#include "../ast/nodes/node.hpp"
#include "../semantic/analyzer.hpp"

#include <fsher/ast/nodes/assign_stmt.hpp>
#include <fsher/ast/nodes/binary.hpp>
#include <fsher/ast/nodes/call.hpp>
#include <fsher/ast/nodes/const_decl.hpp>
#include <fsher/ast/nodes/const_stmt.hpp>
#include <fsher/ast/nodes/expr_stmt.hpp>
#include <fsher/ast/nodes/field.hpp>
#include <fsher/ast/nodes/fn_decl.hpp>
#include <fsher/ast/nodes/for_stmt.hpp>
#include <fsher/ast/nodes/identifier.hpp>
#include <fsher/ast/nodes/if_expr.hpp>
#include <fsher/ast/nodes/index.hpp>
#include <fsher/ast/nodes/let_stmt.hpp>
#include <fsher/ast/nodes/literal.hpp>
#include <fsher/ast/nodes/match_expr.hpp>
#include <fsher/ast/nodes/return_stmt.hpp>
#include <fsher/ast/nodes/struct_decl.hpp>
#include <fsher/ast/nodes/struct_literal.hpp>
#include <fsher/ast/nodes/ternary.hpp>
#include <fsher/ast/nodes/unary.hpp>

namespace fsher::ir {
    class Encoder {
    public:
        Encoder(SymbolTable const& symbols, StructRegistry const& structs) noexcept
            : m_symbols(symbols), m_structs(structs) {}

        ShaderProgram processVFProgram(Node const* root, StageRegistry const& stages) {
            ShaderProgram out;
            out.structs = this->collectStructs();
            out.constants = this->collectConstants();

            if (auto vtx = stages.lookup(Stage::Vertex)) {
                out.stages.push_back(this->buildStage(Stage::Vertex, vtx, stages, false));
            } else {
                out.stages.push_back(this->synthesiseVertexStage(stages));
            }

            if (auto frag = stages.lookup(Stage::Fragment)) {
                out.stages.push_back(this->buildStage(Stage::Fragment, frag, stages, false));
            }

            return out;
        }

    private:
        enum class FuncKind { Regular, Vertex, Fragment };

        std::vector<StructDecl> collectStructs() const {
            std::vector<StructDecl> result;
            for (auto const& [name, info] : m_structs.structs()) {
                StructDecl decl;
                decl.name = std::string(name);
                decl.fields.reserve(info.members.size());
                for (auto const& m : info.members) {
                    decl.fields.push_back({ std::string(m.name), m.type });
                }
                result.push_back(std::move(decl));
            }
            return result;
        }

        std::vector<ConstGlobal> collectConstants() {
            std::vector<ConstGlobal> result;
            for (auto const& [name, sym] : m_symbols.symbols()) {
                if (sym.kind != Symbol::Kind::Const) continue;
                if (!sym.node || sym.node->type() != Node::Type::ConstDecl) continue;
                auto const& decl = static_cast<ConstDeclNode const&>(*sym.node);
                if (!decl.value()) continue;

                ConstGlobal c;
                c.name = std::string(name);
                c.type = sym.type;
                c.value = this->emitExpr(decl.value());
                result.push_back(std::move(c));
            }
            return result;
        }

        StageModule buildStage(Stage stage, FnDeclNode const* entry, StageRegistry const& stages, bool autoVaryings) {
            StageModule mod;
            mod.stage = stage;
            m_currentStage = stage;

            if (stage == Stage::Vertex) {
                for (auto const& [name, symbol] : m_symbols.symbols()) {
                    if (symbol.kind == Symbol::Kind::In) {
                        GlobalVar v;
                        v.name = fmt::format("a_{}", name);
                        v.type = symbol.type;
                        v.semantic = (name == stages.positionInput()) ? Semantic::Position : Semantic::None;
                        mod.inputs.push_back(std::move(v));
                    }
                }

                GlobalVar posOut;
                posOut.name = "_vtxPos";
                posOut.type = Type::Vector(ScalarKind::Float32, 4);
                posOut.semantic = Semantic::Position;
                mod.outputs.push_back(std::move(posOut));

                auto varyings = this->getUsedVaryings(stages.lookup(Stage::Fragment));
                for (auto* var : varyings) {
                    GlobalVar v;
                    v.name = fmt::format("v_{}", var->name);
                    v.type = var->type;
                    mod.outputs.push_back(std::move(v));
                }

                this->discoverUniforms(mod.uniforms, entry);

                mod.usedBuiltins = this->getUsedBuiltins(entry);
                mod.functions = this->collectFunctions(entry, FuncKind::Vertex);
            } else {
                auto varyings = this->getUsedVaryings(entry);
                for (auto* var : varyings) {
                    GlobalVar v;
                    v.name = fmt::format("v_{}", var->name);
                    v.type = var->type;
                    mod.inputs.push_back(std::move(v));
                }

                GlobalVar out;
                out.name = "_fragColor";
                out.type = Type::Vector(ScalarKind::Float32, 4);
                out.semantic = Semantic::FragColor;
                mod.outputs.push_back(std::move(out));

                this->discoverUniforms(mod.uniforms, entry);
                mod.usedBuiltins = this->getUsedBuiltins(entry);
                mod.functions = this->collectFunctions(entry, FuncKind::Fragment);
            }

            return mod;
        }

        StageModule synthesiseVertexStage(StageRegistry const& stages) {
            StageModule mod;
            mod.stage = Stage::Vertex;
            m_currentStage = Stage::Vertex;

            std::string_view posName = stages.positionInput();
            if (posName.empty()) {
                return mod;
            }

            for (auto const& [name, symbol] : m_symbols.symbols()) {
                if (symbol.kind == Symbol::Kind::In) {
                    GlobalVar v;
                    v.name = fmt::format("a_{}", name);
                    v.type = symbol.type;
                    v.semantic = (name == posName) ? Semantic::Position : Semantic::None;
                    mod.inputs.push_back(std::move(v));
                }
            }

            auto varyings = this->getUsedVaryings(stages.lookup(Stage::Fragment));
            for (auto* var : varyings) {
                GlobalVar v;
                v.name = fmt::format("v_{}", var->name);
                v.type = var->type;
                mod.outputs.push_back(std::move(v));
            }

            std::string_view mvpName = stages.matrixInput();
            if (!mvpName.empty()) {
                auto mvpSym = m_symbols.lookup(mvpName);
                Type mvpType = mvpSym ? mvpSym->type : Type::Matrix(ScalarKind::Float32, 4, 4);
                GlobalVar u;
                u.name = std::string(mvpName);
                u.type = mvpType;
                u.semantic = Semantic::MVP;
                mod.uniforms.push_back(std::move(u));
            }

            auto posSym = m_symbols.lookup(posName);
            Type posType = posSym ? posSym->type : Type::Vector(ScalarKind::Float32, 2);

            Expr posExpr;
            if (posType.kind == Type::Kind::Vector && posType.rows == 4) {
                posExpr = Expr{ posType, VarRef{ fmt::format("a_{}", posName) } };
            } else if (posType.kind == Type::Kind::Vector && posType.rows == 3) {
                std::vector<Expr> args;
                args.push_back({ posType, VarRef{ fmt::format("a_{}", posName) } });
                args.push_back({ Type::Float(), Literal{ 1.0 } });
                posExpr = Expr{ Type::Vector(ScalarKind::Float32, 4), Call{ "vec4", std::move(args) } };
            } else {
                std::vector<Expr> args;
                args.push_back({ posType, VarRef{ fmt::format("a_{}", posName) } });
                args.push_back({ Type::Float(), Literal{ 0.0 } });
                args.push_back({ Type::Float(), Literal{ 1.0 } });
                posExpr = Expr{ Type::Vector(ScalarKind::Float32, 4), Call{ "vec4", std::move(args) } };
            }

            if (!mvpName.empty()) {
                auto mvpSym = m_symbols.lookup(mvpName);
                Type mvpType = mvpSym ? mvpSym->type : Type::Matrix(ScalarKind::Float32, 4, 4);
                posExpr = Expr{
                    Type::Vector(ScalarKind::Float32, 4),
                    Binary{
                        TokenType::ASTERISK,
                        std::make_unique<Expr>(mvpType, VarRef{ std::string(mvpName) }),
                        std::make_unique<Expr>(std::move(posExpr))
                    }
                };
            }

            std::vector<Stmt> stmts;
            stmts.push_back(Stmt{ AssignStmt{
                .target = Expr{ Type::Vector(ScalarKind::Float32, 4), VarRef{ "_vtxPos" } },
                .op = TokenType::EQUAL,
                .value = std::move(posExpr)
            }});

            for (auto* var : varyings) {
                stmts.push_back(Stmt{ AssignStmt{
                    .target = Expr{ var->type, VarRef{ fmt::format("v_{}", var->name) }},
                    .op = TokenType::EQUAL,
                    .value = Expr{ var->type, VarRef{ fmt::format("a_{}", var->name) }},
                }});
            }

            // stmts.push_back(Stmt{ ReturnStmt{ std::nullopt } });

            Function mainFn;
            mainFn.name = "main";
            mainFn.returnType = Type::Void();
            mainFn.body = std::move(stmts);
            mainFn.isEntry = true;
            mod.functions.push_back(std::move(mainFn));

            GlobalVar posOut;
            posOut.name = "_vtxPos";
            posOut.type = Type::Vector(ScalarKind::Float32, 4);
            posOut.semantic = Semantic::Position;
            mod.outputs.insert(mod.outputs.begin(), std::move(posOut));

            return mod;
        }

        std::vector<Function> collectFunctions(FnDeclNode const* entry, FuncKind kind) {
            std::vector<Function> out;
            auto ordered = this->getOrderedFunctions(entry);
            TypeResolver resolver{ m_structs };

            for (auto const* func : ordered) {
                Function f;
                f.name = std::string(func->name());
                f.returnType = m_symbols.lookup(func->name())
                    .transform([](Symbol const& s) { return s.type; })
                    .value_or(Type::Void());

                for (auto const& arg : func->parameters()) {
                    auto t = resolver.resolve(arg.type);
                    if (!t) continue;
                    f.params.push_back(Param{ std::string(arg.name), *t });
                }

                bool isStageEntry = (func == entry);
                f.isEntry = isStageEntry;
                f.body = this->visitFunctionBody(func, isStageEntry ? kind : FuncKind::Regular);
                out.push_back(std::move(f));
            }

            return out;
        }

        std::vector<Stmt> visitFunctionBody(FnDeclNode const* decl, FuncKind kind) {
            std::vector<Stmt> stmts;
            if (!decl || !decl->body()) return stmts;

            m_localScopes.push_back({});
            for (auto const& arg : decl->parameters()) {
                m_localScopes.back().insert(std::string(arg.name));
            }

            if (decl->body()->type() == Node::Type::Block) {
                auto const& block = static_cast<BlockNode const&>(*decl->body());
                for (auto const& stmt : block.statements()) {
                    this->emitStmt(stmt.get(), stmts, kind);
                }
                if (auto trail = block.trail()) {
                    if (trail->type() == Node::Type::IfExpr) {
                        if (kind == FuncKind::Regular) {
                            stmts.push_back(this->emitIfStmt(static_cast<IfExprNode const*>(trail), std::nullopt, kind, true));
                        } else {
                            auto target = (kind == FuncKind::Vertex) ? "_vtxPos" : "_fragColor";
                            stmts.push_back(this->emitIfStmt(static_cast<IfExprNode const*>(trail), target, kind));
                        }
                    } else if (trail->type() == Node::Type::MatchExpr) {
                        if (kind == FuncKind::Regular) {
                            auto ms = this->emitMatchStmt(static_cast<MatchExprNode const*>(trail), std::nullopt, kind, true);
                            stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                        } else {
                            auto target = (kind == FuncKind::Vertex) ? "_vtxPos" : "_fragColor";
                            auto ms = this->emitMatchStmt(static_cast<MatchExprNode const*>(trail), target, kind);
                            stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                        }
                    } else {
                        this->makeReturn(this->emitExpr(trail), kind, stmts);
                    }
                }
            } else {
                if (decl->body()->type() == Node::Type::IfExpr) {
                    if (kind == FuncKind::Regular) {
                        stmts.push_back(this->emitIfStmt(static_cast<IfExprNode const*>(decl->body()), std::nullopt, kind, true));
                    } else {
                        auto target = (kind == FuncKind::Vertex) ? "_vtxPos" : "_fragColor";
                        stmts.push_back(this->emitIfStmt(static_cast<IfExprNode const*>(decl->body()), target, kind));
                    }
                } else if (decl->body()->type() == Node::Type::MatchExpr) {
                    if (kind == FuncKind::Regular) {
                        auto ms = this->emitMatchStmt(static_cast<MatchExprNode const*>(decl->body()), std::nullopt, kind, true);
                        stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                    } else {
                        auto target = (kind == FuncKind::Vertex) ? "_vtxPos" : "_fragColor";
                        auto ms = this->emitMatchStmt(static_cast<MatchExprNode const*>(decl->body()), target, kind);
                        stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                    }
                } else {
                    this->makeReturn(this->emitExpr(decl->body()), kind, stmts);
                }
            }

            if (!stmts.empty() && std::holds_alternative<ReturnStmt>(stmts.back().node)) {
                auto const& [value] = std::get<ReturnStmt>(stmts.back().node);
                if (!value.has_value()) {
                    stmts.pop_back();
                }
            }

            m_localScopes.pop_back();
            return stmts;
        }

        void makeReturn(Expr value, FuncKind kind, std::vector<Stmt>& out) {
            if (kind == FuncKind::Regular) {
                out.push_back(Stmt{ ReturnStmt{ std::move(value) } });
            } else {
                auto const& varName = (kind == FuncKind::Vertex) ? "_vtxPos" : "_fragColor";
                out.push_back(Stmt{ AssignStmt{
                    Expr{ value.type, VarRef{ varName } },
                    TokenType::EQUAL,
                    std::move(value)
                }});
                out.push_back(Stmt{ ReturnStmt{ std::nullopt } });
            }
        }

        void emitStmt(Node const* n, std::vector<Stmt>& stmts, FuncKind kind, bool root = false) {
            switch (n->type()) {
                case Node::Type::LetStmt: {
                    auto let = static_cast<LetStmtNode const*>(n);
                    this->declareLocalVar(let->name());
                    auto init = let->initializer();
                    if (init && (init->type() == Node::Type::IfExpr || init->type() == Node::Type::MatchExpr) && !this->isSimpleExpr(init)) {
                        Type varType = init->resolvedType();
                        stmts.push_back(Stmt{ LetStmt{
                            .name = std::string(let->name()),
                            .type = varType,
                            .value = std::nullopt
                        }});
                        auto varName = std::string(let->name());
                        auto stmtsToAdd = this->emitExprBodyStmts(init, varName, kind);
                        stmts.insert(stmts.end(), std::make_move_iterator(stmtsToAdd.begin()), std::make_move_iterator(stmtsToAdd.end()));
                    } else {
                        Type type = Type::Void();
                        if (auto* sym = m_symbols.find(let->name())) {
                            type = sym->type;
                        } else if (init) {
                            type = init->resolvedType();
                        }
                        if (type.kind == Type::Kind::Scalar) {
                            if (type.scalarKind == ScalarKind::UntypedFloat) type = Type::Float();
                            if (type.scalarKind == ScalarKind::UntypedInt)   type = Type::Int();
                        }
                        stmts.push_back(Stmt{ LetStmt{
                            .name = std::string(let->name()),
                            .type = type,
                            .value = init ? std::make_optional(this->emitExpr(init)) : std::nullopt
                        }});
                    }
                    break;
                }
                case Node::Type::ConstStmt: {
                    auto c = static_cast<ConstStmtNode const*>(n);
                    this->declareLocalVar(c->name());
                    auto init = c->initializer();
                    if (init && (init->type() == Node::Type::IfExpr || init->type() == Node::Type::MatchExpr) && !this->isSimpleExpr(init)) {
                        Type varType = init->resolvedType();
                        stmts.push_back(Stmt{ LetStmt{
                            .name = std::string(c->name()),
                            .type = varType,
                            .value = std::nullopt
                        }});
                        auto varName = std::string(c->name());
                        auto stmtsToAdd = this->emitExprBodyStmts(init, varName, kind);
                        stmts.insert(stmts.end(), std::make_move_iterator(stmtsToAdd.begin()), std::make_move_iterator(stmtsToAdd.end()));
                    } else {
                        Type type = Type::Void();
                        if (auto* sym = m_symbols.find(c->name())) {
                            type = sym->type;
                        } else if (init) {
                            type = init->resolvedType();
                        }
                        if (type.kind == Type::Kind::Scalar) {
                            if (type.scalarKind == ScalarKind::UntypedFloat) type = Type::Float();
                            if (type.scalarKind == ScalarKind::UntypedInt)   type = Type::Int();
                        }
                        stmts.push_back(Stmt{ LetStmt{
                            .name = std::string(c->name()),
                            .type = type,
                            .value = init ? std::make_optional(this->emitExpr(init)) : std::nullopt
                        }});
                    }
                    break;
                }
                case Node::Type::AssignStmt: {
                    auto assign = static_cast<AssignStmtNode const*>(n);
                    stmts.push_back(Stmt{ AssignStmt{
                        .target = this->emitExpr(assign->target()),
                        .op = assign->op(),
                        .value = this->emitExpr(assign->value())
                    }});
                    break;
                }
                case Node::Type::ReturnStmt: {
                    auto ret = static_cast<ReturnStmtNode const*>(n);
                    if (ret->value()) {
                        stmts.push_back(Stmt{ ReturnStmt{ this->emitExpr(ret->value()) } });
                    } else {
                        stmts.push_back(Stmt{ ReturnStmt{ std::nullopt } });
                    }
                    break;
                }
                case Node::Type::IfExpr: {
                    auto ifNode = static_cast<IfExprNode const*>(n);
                    stmts.push_back(this->emitIfStmt(ifNode, std::nullopt, kind));
                    break;
                }
                case Node::Type::MatchExpr: {
                    auto matchNode = static_cast<MatchExprNode const*>(n);
                    auto ms = this->emitMatchStmt(matchNode, std::nullopt, kind);
                    stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                    break;
                }
                case Node::Type::ExprStmt: {
                    auto exprStmt = static_cast<ExprStmtNode const*>(n);
                    if (exprStmt->expr()->type() == Node::Type::IfExpr) {
                        auto ifNode = static_cast<IfExprNode const*>(exprStmt->expr());
                        stmts.push_back(this->emitIfStmt(ifNode, std::nullopt, kind));
                    } else if (exprStmt->expr()->type() == Node::Type::MatchExpr) {
                        auto matchNode = static_cast<MatchExprNode const*>(exprStmt->expr());
                        auto ms = this->emitMatchStmt(matchNode, std::nullopt, kind);
                        stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                    } else {
                        stmts.push_back(Stmt{ ExprStmt{ this->emitExpr(exprStmt->expr()) } });
                    }
                    break;
                }
                case Node::Type::ForStmt: {
                    auto forStmt = static_cast<ForStmtNode const*>(n);
                    this->declareLocalVar(forStmt->loopVar());
                    std::vector<Stmt> bodyStmts;
                    for (auto const& s : forStmt->body()->statements()) {
                        this->emitStmt(s.get(), bodyStmts, kind);
                    }
                    if (auto trail = forStmt->body()->trail()) {
                        this->emitStmt(trail, bodyStmts, kind);
                    }
                    stmts.push_back(Stmt{ ForStmt{
                        .loopVar = std::string(forStmt->loopVar()),
                        .rangeStart = this->emitExpr(forStmt->rangeStart()),
                        .rangeEnd = this->emitExpr(forStmt->rangeEnd()),
                        .inclusive = forStmt->inclusive(),
                        .body = std::move(bodyStmts)
                    }});
                    break;
                }
                default: {
                    fmt::println(stderr, "[WARN] Not implemented emitStmt for node of type '{}'", Node::format_as(n->type()));
                    break;
                }
            }
        }

        std::vector<Stmt> emitBlockStmts(BlockNode const* block, std::optional<std::string_view> tempResultVar, FuncKind kind) {
            std::vector<Stmt> stmts;
            if (!block) return stmts;
            for (auto const& stmt : block->statements()) {
                this->emitStmt(stmt.get(), stmts, kind);
            }
            if (auto trail = block->trail()) {
                if (trail->type() == Node::Type::IfExpr) {
                    stmts.push_back(this->emitIfStmt(static_cast<IfExprNode const*>(trail), tempResultVar, kind));
                } else if (trail->type() == Node::Type::MatchExpr) {
                    auto ms = this->emitMatchStmt(static_cast<MatchExprNode const*>(trail), tempResultVar, kind);
                    stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
                } else if (tempResultVar.has_value()) {
                    stmts.push_back(Stmt{ AssignStmt{
                        .target = Expr{ trail->resolvedType(), VarRef{ std::string(*tempResultVar) } },
                        .op = TokenType::EQUAL,
                        .value = this->emitExpr(trail)
                    }});
                } else {
                    stmts.push_back(Stmt{ ExprStmt{ this->emitExpr(trail) } });
                }
            }
            return stmts;
        }

        std::vector<Stmt> emitExprBodyStmts(ExprNode const* expr, std::optional<std::string_view> const& tempResultVar, FuncKind kind) {
            if (expr && expr->type() == Node::Type::Block) {
                return this->emitBlockStmts(static_cast<BlockNode const*>(expr), tempResultVar, kind);
            }
            std::vector<Stmt> stmts;
            if (!expr) return stmts;
            if (expr->type() == Node::Type::IfExpr) {
                stmts.push_back(this->emitIfStmt(static_cast<IfExprNode const*>(expr), tempResultVar, kind));
            } else if (expr->type() == Node::Type::MatchExpr) {
                auto ms = this->emitMatchStmt(static_cast<MatchExprNode const*>(expr), tempResultVar, kind);
                stmts.insert(stmts.end(), std::make_move_iterator(ms.begin()), std::make_move_iterator(ms.end()));
            } else if (tempResultVar.has_value()) {
                stmts.push_back(Stmt{ AssignStmt{
                    .target = Expr{ expr->resolvedType(), VarRef{ std::string(*tempResultVar) } },
                    .op = TokenType::EQUAL,
                    .value = this->emitExpr(expr)
                }});
            } else {
                stmts.push_back(Stmt{ ExprStmt{ this->emitExpr(expr) } });
            }
            return stmts;
        }

        Stmt emitIfStmt(IfExprNode const* ifNode, std::optional<std::string_view> const& tempResultVar, FuncKind kind, bool addReturn = false) {
            Expr cond = this->emitExpr(ifNode->condition());
            std::vector<Stmt> thenBody = this->emitBlockStmts(ifNode->thenBlock(), tempResultVar, kind);
            if (addReturn && !tempResultVar.has_value() && ifNode->thenBlock()->trail()) {
                if (!thenBody.empty() && std::holds_alternative<ExprStmt>(thenBody.back().node)) {
                    auto exprStmt = std::move(std::get<ExprStmt>(thenBody.back().node));
                    thenBody.pop_back();
                    this->makeReturn(std::move(exprStmt.expr), kind, thenBody);
                }
            }
            std::vector<Stmt> elseBody;
            switch (ifNode->elseKind()) {
                case IfExprNode::ElseKind::ElseIf:
                    if (ifNode->elseIfBranch()) {
                        elseBody.push_back(this->emitIfStmt(ifNode->elseIfBranch(), tempResultVar, kind, addReturn));
                    }
                    break;
                case IfExprNode::ElseKind::Else:
                    if (ifNode->elseBlock()) {
                        elseBody = this->emitBlockStmts(ifNode->elseBlock(), tempResultVar, kind);
                        if (addReturn && !tempResultVar.has_value() && ifNode->elseBlock()->trail()) {
                            if (!elseBody.empty() && std::holds_alternative<ExprStmt>(elseBody.back().node)) {
                                auto exprStmt = std::move(std::get<ExprStmt>(elseBody.back().node));
                                elseBody.pop_back();
                                this->makeReturn(std::move(exprStmt.expr), kind, elseBody);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
            return Stmt{ IfStmt{
                .condition = std::move(cond),
                .thenBody = std::move(thenBody),
                .elseBody = std::move(elseBody)
            }};
        }

        std::vector<Stmt> emitMatchStmt(MatchExprNode const* matchNode, std::optional<std::string_view> const& tempResultVar, FuncKind kind, bool addReturn = false) {
            auto const& matchArms = matchNode->arms();
            auto wildcardIt = std::ranges::find_if(matchArms, [](auto const& arm) {
                return arm.pattern.kind == Pattern::Kind::Wildcard;
            });

            MatchArm const* wildcardArm = nullptr;
            if (wildcardIt != matchArms.end()) {
                wildcardArm = &(*wildcardIt);
            }

            std::vector<Stmt> resultStmts;

            std::optional<std::string> tempScrutineeVar;
            auto scrutineeType = matchNode->scrutinee()->type();
            if (scrutineeType != Node::Type::LiteralExpr && scrutineeType != Node::Type::IdentifierExpr) {
                tempScrutineeVar = fmt::format("_fs_tmp{}", m_tempVarCounter++);
                resultStmts.push_back(Stmt{ LetStmt{
                    .name = *tempScrutineeVar,
                    .type = matchNode->scrutinee()->resolvedType(),
                    .value = this->emitExpr(matchNode->scrutinee())
                }});
            }

            auto getScrutineeExpr = [&]() -> Expr {
                if (tempScrutineeVar.has_value()) {
                    return Expr{ matchNode->scrutinee()->resolvedType(), VarRef{ *tempScrutineeVar } };
                }
                return this->emitExpr(matchNode->scrutinee());
            };

            Stmt rootIf{};
            IfStmt* currentInnerIf = nullptr;
            bool isFirst = true;

            for (auto const& arm : matchArms) {
                if (arm.pattern.kind == Pattern::Kind::Wildcard) {
                    continue;
                }

                std::vector<Stmt> bodyStmts = this->emitExprBodyStmts(arm.body.get(), tempResultVar, kind);
                if (addReturn && !tempResultVar.has_value() && arm.body) {
                    if (!bodyStmts.empty() && std::holds_alternative<ExprStmt>(bodyStmts.back().node)) {
                        auto exprStmt = std::move(std::get<ExprStmt>(bodyStmts.back().node));
                        bodyStmts.pop_back();
                        this->makeReturn(std::move(exprStmt.expr), kind, bodyStmts);
                    } else if (arm.body->type() == Node::Type::Block) {
                        auto block = static_cast<BlockNode const*>(arm.body.get());
                        if (block->trail() && block->trail()->type() != Node::Type::IfExpr && block->trail()->type() != Node::Type::MatchExpr) {
                             if (!bodyStmts.empty() && std::holds_alternative<ExprStmt>(bodyStmts.back().node)) {
                                auto exprStmt = std::move(std::get<ExprStmt>(bodyStmts.back().node));
                                bodyStmts.pop_back();
                                this->makeReturn(std::move(exprStmt.expr), kind, bodyStmts);
                            }
                        }
                    }
                }

                auto lhsExpr = getScrutineeExpr();
                auto rhsExpr = this->emitExpr(arm.pattern.expr.get());
                rhsExpr = this->coerceLiteralToType(std::move(rhsExpr), lhsExpr.type);

                auto cond = Expr {
                    .type = Type::Bool(),
                    .node = Binary {
                        .op = TokenType::EQUAL_EQUAL,
                        .lhs = std::make_unique<Expr>(std::move(lhsExpr)),
                        .rhs = std::make_unique<Expr>(std::move(rhsExpr))
                    }
                };

                IfStmt newIf{
                    .condition = std::move(cond),
                    .thenBody = std::move(bodyStmts),
                    .elseBody = {}
                };

                if (isFirst) {
                    rootIf = Stmt{ std::move(newIf) };
                    currentInnerIf = &std::get<IfStmt>(rootIf.node);
                    isFirst = false;
                } else {
                    currentInnerIf->elseBody.push_back(Stmt{ std::move(newIf) });
                    currentInnerIf = &std::get<IfStmt>(currentInnerIf->elseBody.back().node);
                }
            }

            if (wildcardArm != nullptr) {
                std::vector<Stmt> bodyStmts = this->emitExprBodyStmts(wildcardArm->body.get(), tempResultVar, kind);
                if (addReturn && !tempResultVar.has_value() && wildcardArm->body) {
                    if (!bodyStmts.empty() && std::holds_alternative<ExprStmt>(bodyStmts.back().node)) {
                        auto exprStmt = std::move(std::get<ExprStmt>(bodyStmts.back().node));
                        bodyStmts.pop_back();
                        this->makeReturn(std::move(exprStmt.expr), kind, bodyStmts);
                    } else if (wildcardArm->body->type() == Node::Type::Block) {
                        auto block = static_cast<BlockNode const*>(wildcardArm->body.get());
                        if (block->trail() && block->trail()->type() != Node::Type::IfExpr && block->trail()->type() != Node::Type::MatchExpr) {
                             if (!bodyStmts.empty() && std::holds_alternative<ExprStmt>(bodyStmts.back().node)) {
                                auto exprStmt = std::move(std::get<ExprStmt>(bodyStmts.back().node));
                                bodyStmts.pop_back();
                                this->makeReturn(std::move(exprStmt.expr), kind, bodyStmts);
                            }
                        }
                    }
                }

                if (isFirst) {
                    resultStmts.push_back(Stmt{ IfStmt{
                        .condition = Expr{ Type::Bool(), Literal{ true } },
                        .thenBody = std::move(bodyStmts),
                        .elseBody = {}
                    }});
                    return resultStmts;
                }

                currentInnerIf->elseBody = std::move(bodyStmts);
            }

            if (!isFirst) {
                resultStmts.push_back(std::move(rootIf));
            }
            return resultStmts;
        }

        void declareLocalVar(std::string_view name) {
            if (!m_localScopes.empty()) {
                m_localScopes.back().insert(std::string(name));
            }
        }

        bool isLocalVar(std::string_view name) const {
            for (auto it = m_localScopes.rbegin(); it != m_localScopes.rend(); ++it) {
                if (it->contains(std::string(name))) return true;
            }
            return false;
        }

        std::string resolveVarName(std::string_view name) const {
            if (this->isLocalVar(name)) {
                return std::string(name);
            }
            auto* symbol = m_symbols.find(name);
            if (!symbol) return std::string(name);
            switch (symbol->kind) {
                default: return std::string(name);
                case Symbol::Kind::Out: return fmt::format("v_{}", name);
                case Symbol::Kind::In:
                    return m_currentStage == Stage::Fragment
                        ? fmt::format("v_{}", name)
                        : fmt::format("a_{}", name);
            }
        }

        Expr coerceLiteralToType(Expr expr, Type const& targetType) {
            if (std::holds_alternative<Literal>(expr.node) && targetType.kind == Type::Kind::Scalar) {
                auto lit = std::get<Literal>(expr.node);
                if (targetType.scalarKind == ScalarKind::Float32 || targetType.scalarKind == ScalarKind::Float64) {
                    if (std::holds_alternative<int64_t>(lit.value)) {
                        expr.node = Literal{ static_cast<double>(std::get<int64_t>(lit.value)) };
                        expr.type = targetType;
                    }
                } else if (targetType.scalarKind == ScalarKind::Int32 || targetType.scalarKind == ScalarKind::UInt32) {
                    if (std::holds_alternative<double>(lit.value)) {
                        expr.node = Literal{ static_cast<int64_t>(std::get<double>(lit.value)) };
                        expr.type = targetType;
                    }
                }
            }
            return expr;
        }

        Expr emitExpr(Node const* n) {
            switch (n->type()) {
                case Node::Type::BinaryExpr: {
                    auto bin = static_cast<BinaryExprNode const*>(n);
                    Binary ret{ .op = bin->op() };
                    auto lhs = this->emitExpr(bin->lhs());
                    auto rhs = this->emitExpr(bin->rhs());
                    if (std::holds_alternative<Literal>(rhs.node) && !std::holds_alternative<Literal>(lhs.node)) {
                        rhs = this->coerceLiteralToType(std::move(rhs), lhs.type);
                    } else if (std::holds_alternative<Literal>(lhs.node) && !std::holds_alternative<Literal>(rhs.node)) {
                        lhs = this->coerceLiteralToType(std::move(lhs), rhs.type);
                    }
                    ret.lhs = std::make_unique<Expr>(std::move(lhs));
                    ret.rhs = std::make_unique<Expr>(std::move(rhs));
                    return Expr{ .type = bin->resolvedType(), .node = std::move(ret) };
                }
                case Node::Type::IdentifierExpr: {
                    auto ident = static_cast<IdentifierExprNode const*>(n);
                    return Expr { .type = ident->resolvedType(), .node = VarRef{ this->resolveVarName(ident->name()) } };
                }
                case Node::Type::LiteralExpr: {
                    auto literal = static_cast<LiteralExprNode const*>(n);
                    auto type = literal->resolvedType();
                    auto valueType = literal->valueType();
                    if (type.scalarKind == ScalarKind::Float32 || type.scalarKind == ScalarKind::Float64 || type.scalarKind == ScalarKind::UntypedFloat) {
                        valueType = LiteralExprNode::ValueType::Float;
                    } else if (type.scalarKind == ScalarKind::Int32 || type.scalarKind == ScalarKind::UInt32 || type.scalarKind == ScalarKind::UntypedInt) {
                        valueType = LiteralExprNode::ValueType::Integer;
                    } else if (type.scalarKind == ScalarKind::Bool) {
                        valueType = LiteralExprNode::ValueType::Boolean;
                    }
                    return Expr { .type = literal->resolvedType(), .node = Literal { literal->getAs(valueType) } };
                }
                case Node::Type::UnaryExpr: {
                    auto unary = static_cast<UnaryExprNode const*>(n);
                    return Expr {
                        .type = unary->resolvedType(),
                        .node = Unary {
                            unary->op(),
                            std::make_unique<Expr>(this->emitExpr(unary->operand()))
                        }
                    };
                }
                case Node::Type::CallExpr: {
                    auto call = static_cast<CallExprNode const*>(n);
                    auto const& calleeIdent = static_cast<IdentifierExprNode const&>(*call->callee());

                    std::vector<Expr> args;
                    for (auto const& arg : call->args()) args.push_back(this->emitExpr(arg.get()));

                    return Expr { .type = call->resolvedType(), .node = Call{
                        .callee = std::string(calleeIdent.name()),
                        .args = std::move(args)
                    }};
                }
                case Node::Type::FieldExpr: {
                    auto field = static_cast<FieldExprNode const*>(n);
                    return Expr {
                        .type = field->resolvedType(),
                        .node = FieldAccess {
                            .target = std::make_unique<Expr>(this->emitExpr(field->target())),
                            .field = std::string(field->field())
                        }
                    };
                }
                case Node::Type::IndexExpr: {
                    auto indexExpr = static_cast<IndexExprNode const*>(n);
                    return Expr {
                        .type = indexExpr->resolvedType(),
                        .node = IndexAccess {
                            .target = std::make_unique<Expr>(this->emitExpr(indexExpr->target())),
                            .index = std::make_unique<Expr>(this->emitExpr(indexExpr->index()))
                        }
                    };
                }
                case Node::Type::StructLiteralExpr: {
                    auto lit = static_cast<StructLiteralExprNode const*>(n);
                    StructLit sl;
                    sl.typeName = std::string(lit->typeName());
                    for (auto const& f : lit->fields()) {
                        sl.fields.emplace_back(std::string(f.name), this->emitExpr(f.value.get()));
                    }
                    return Expr { .type = lit->resolvedType(), .node = std::move(sl) };
                }
                case Node::Type::Block: {
                    auto block = static_cast<BlockNode const*>(n);
                    if (auto trail = block->trail()) {
                        return this->emitExpr(trail);
                    }
                    return {};
                }
                case Node::Type::TernaryExpr: {
                    auto tern = static_cast<TernaryExprNode const*>(n);
                    return Expr {
                        .type = tern->resolvedType(),
                        .node = Ternary {
                            .condition = std::make_unique<Expr>(this->emitExpr(tern->condition())),
                            .thenExpr = std::make_unique<Expr>(this->emitExpr(tern->thenExpr())),
                            .elseExpr = std::make_unique<Expr>(this->emitExpr(tern->elseExpr()))
                        }
                    };
                }
                case Node::Type::IfExpr: {
                    auto ifNode = static_cast<IfExprNode const*>(n);
                    Expr elseExpr{};
                    switch (ifNode->elseKind()) {
                        case IfExprNode::ElseKind::ElseIf:
                            if (ifNode->elseIfBranch()) {
                                elseExpr = this->emitExpr(ifNode->elseIfBranch());
                            }
                            break;
                        case IfExprNode::ElseKind::Else:
                            if (ifNode->elseBlock()) {
                                elseExpr = this->emitExpr(ifNode->elseBlock());
                            }
                            break;
                        default:
                            break;
                    }
                    return Expr {
                        .type = ifNode->resolvedType(),
                        .node = Ternary {
                            .condition = std::make_unique<Expr>(this->emitExpr(ifNode->condition())),
                            .thenExpr = std::make_unique<Expr>(this->emitExpr(ifNode->thenBlock())),
                            .elseExpr = std::make_unique<Expr>(std::move(elseExpr))
                        }
                    };
                }
                case Node::Type::MatchExpr: {
                    auto match = static_cast<MatchExprNode const*>(n);
                    std::optional<Expr> resultExpr;
                    for (auto it = match->arms().rbegin(); it != match->arms().rend(); ++it) {
                        auto const& arm = *it;
                        Expr bodyExpr = this->emitExpr(arm.body.get());

                        if (arm.pattern.kind == Pattern::Kind::Wildcard) {
                            resultExpr = std::move(bodyExpr);
                        } else {
                            Expr scrutineeExpr = this->emitExpr(match->scrutinee());
                            Expr patExpr = this->emitExpr(arm.pattern.expr.get());
                            Binary condNode{
                                .op = TokenType::EQUAL_EQUAL,
                                .lhs = std::make_unique<Expr>(std::move(scrutineeExpr)),
                                .rhs = std::make_unique<Expr>(std::move(patExpr))
                            };
                            Expr condExpr{
                                .type = Type::Bool(),
                                .node = std::move(condNode)
                            };

                            Expr elseBranch = resultExpr.has_value() ? std::move(*resultExpr) : Expr{};

                            resultExpr = Expr {
                                .type = match->resolvedType(),
                                .node = Ternary {
                                    .condition = std::make_unique<Expr>(std::move(condExpr)),
                                    .thenExpr = std::make_unique<Expr>(std::move(bodyExpr)),
                                    .elseExpr = std::make_unique<Expr>(std::move(elseBranch))
                                }
                            };
                        }
                    }
                    return resultExpr ? std::move(*resultExpr) : Expr{};
                }
                default: {
                    fmt::println(stderr, "[WARN] Not implemented emitExpr for node of type '{}'", Node::format_as(n->type()));
                    break;
                }
            }
            return {};
        }

        bool isSimpleExpr(Node const* n) const {
            if (!n) return true;
            if (n->type() == Node::Type::IfExpr) {
                auto ifNode = static_cast<IfExprNode const*>(n);
                if (!isSimpleExpr(ifNode->thenBlock())) return false;
                if (ifNode->elseKind() == IfExprNode::ElseKind::ElseIf) {
                    if (!isSimpleExpr(ifNode->elseIfBranch())) return false;
                } else if (ifNode->elseKind() == IfExprNode::ElseKind::Else) {
                    if (!isSimpleExpr(ifNode->elseBlock())) return false;
                }
                return true;
            }

            if (n->type() == Node::Type::MatchExpr) {
                auto matchNode = static_cast<MatchExprNode const*>(n);
                for (auto const& arm : matchNode->arms()) {
                    if (!isSimpleExpr(arm.body.get())) return false;
                }
                return true;
            }

            if (n->type() == Node::Type::Block) {
                auto block = static_cast<BlockNode const*>(n);
                if (!block->statements().empty()) return false;
                return isSimpleExpr(block->trail());
            }

            return true;
        }

        void discoverUniforms(std::vector<GlobalVar>& out, FnDeclNode const* entry) {
            if (!entry) return;
            std::unordered_set<Symbol const*> seen;
            walkWithFuncs(entry->body(), [&](Node const* n) {
                if (n->type() == Node::Type::IdentifierExpr) {
                    auto* symbol = m_symbols.find(static_cast<IdentifierExprNode const*>(n)->name());
                    if (symbol && symbol->kind == Symbol::Kind::Uniform) seen.insert(symbol);
                }
            });

            for (auto* symbol : seen) {
                GlobalVar v;
                v.name = std::string(symbol->name);
                v.type = symbol->type;
                if (symbol->kind == Symbol::Kind::Uniform && symbol->type.kind == Type::Kind::Matrix) {}
                out.push_back(std::move(v));
            }
        }

        std::vector<Symbol const*> getUsedVaryings(FnDeclNode const* frag) {
            if (!frag) return {};

            std::unordered_set<Symbol const*> seen;
            walkWithFuncs(frag->body(), [&](Node const* n) {
                if (n->type() == Node::Type::IdentifierExpr) {
                    auto* symbol = m_symbols.find(static_cast<IdentifierExprNode const*>(n)->name());
                    if (symbol && (symbol->kind == Symbol::Kind::In || symbol->kind == Symbol::Kind::Out)) seen.insert(symbol);
                }
            });

            std::vector<Symbol const*> varyings;
            varyings.assign(seen.begin(), seen.end());
            std::ranges::sort(varyings, {}, &Symbol::declOrder);
            return varyings;
        }

        std::vector<BuiltinID> getUsedBuiltins(FnDeclNode const* entry) {
            if (!entry) return {};

            std::vector<BuiltinID> ids;
            std::unordered_set<uint16_t> seen;

            walkWithFuncs(entry->body(), [&](Node const* n) {
                if (n->type() == Node::Type::CallExpr) {
                    auto const& call = static_cast<CallExprNode const&>(*n);

                    if (call.callee()->type() != Node::Type::IdentifierExpr) return;
                    auto const& callee = static_cast<IdentifierExprNode const&>(*call.callee());

                    if (auto* sym = m_symbols.find(callee.name())) {
                        if (sym->builtinId.has_value()) {
                            auto raw = static_cast<uint16_t>(*sym->builtinId);
                            if (seen.insert(raw).second) {
                                ids.push_back(*sym->builtinId);
                            }
                        }
                    }
                }
            });

            return ids;
        }

        template <typename Fn>
        static void forEachChild(Node const* node, Fn&& visit) {
            switch (node->type()) {
                case Node::Type::BinaryExpr: {
                    auto const& n = static_cast<BinaryExprNode const&>(*node);
                    visit(n.lhs()); visit(n.rhs());
                    break;
                }
                case Node::Type::UnaryExpr:
                    visit(static_cast<UnaryExprNode const&>(*node).operand());
                    break;
                case Node::Type::TernaryExpr: {
                    auto const& n = static_cast<TernaryExprNode const&>(*node);
                    visit(n.condition()); visit(n.thenExpr()); visit(n.elseExpr());
                    break;
                }
                case Node::Type::CallExpr: {
                    auto const& n = static_cast<CallExprNode const&>(*node);
                    visit(n.callee());
                    for (auto const& arg : n.args()) visit(arg.get());
                    break;
                }
                case Node::Type::FieldExpr:
                    visit(static_cast<FieldExprNode const&>(*node).target());
                    break;
                case Node::Type::IndexExpr: {
                    auto const& n = static_cast<IndexExprNode const&>(*node);
                    visit(n.target()); visit(n.index());
                    break;
                }
                case Node::Type::StructLiteralExpr: {
                    auto const& n = static_cast<StructLiteralExprNode const&>(*node);
                    for (auto const& f : n.fields()) visit(f.value.get());
                    break;
                }
                case Node::Type::IfExpr: {
                    auto const& n = static_cast<IfExprNode const&>(*node);
                    visit(n.condition()); visit(n.thenBlock());
                    switch (n.elseKind()) {
                        case IfExprNode::ElseKind::ElseIf:
                            if (auto e = n.elseIfBranch()) visit(e);
                            break;
                        case IfExprNode::ElseKind::Else:
                            if (auto e = n.elseBlock()) visit(e);
                            break;
                        default: break;
                    }
                    break;
                }
                case Node::Type::Block: {
                    auto const& n = static_cast<BlockNode const&>(*node);
                    for (auto const& stmt : n.statements()) visit(stmt.get());
                    if (auto t = n.trail()) visit(t);
                    break;
                }
                case Node::Type::MatchExpr: {
                    auto const& n = static_cast<MatchExprNode const&>(*node);
                    visit(n.scrutinee());
                    for (auto const& arm : n.arms()) visit(arm.body.get());
                    break;
                }
                case Node::Type::FnDecl: {
                    auto const& n = static_cast<FnDeclNode const&>(*node);
                    visit(n.body());
                    break;
                }
                case Node::Type::LetStmt: {
                    auto const& n = static_cast<LetStmtNode const&>(*node);
                    if (n.initializer()) visit(n.initializer());
                    break;
                }
                case Node::Type::ConstStmt: {
                    auto const& n = static_cast<ConstStmtNode const&>(*node);
                    if (n.initializer()) visit(n.initializer());
                    break;
                }
                case Node::Type::AssignStmt: {
                    auto const& n = static_cast<AssignStmtNode const&>(*node);
                    visit(n.target()); visit(n.value());
                    break;
                }
                case Node::Type::ForStmt: {
                    auto const& n = static_cast<ForStmtNode const&>(*node);
                    visit(n.rangeStart()); visit(n.rangeEnd()); visit(n.body());
                    break;
                }
                case Node::Type::ReturnStmt: {
                    auto const& n = static_cast<ReturnStmtNode const&>(*node);
                    if (n.value()) visit(n.value());
                    break;
                }
                case Node::Type::ExprStmt: {
                    auto const& n = static_cast<ExprStmtNode const&>(*node);
                    visit(n.expr());
                    break;
                }
                case Node::Type::IdentifierExpr:
                case Node::Type::LiteralExpr:
                    break;
                default:
                    fmt::println(stderr, "[WARN] forEachChild not implemented for node of type '{}'", Node::format_as(node->type()));
                    break;
            }
        }

        template <typename Fn>
        static void walk(Node const* node, Fn&& onNode) {
            onNode(node);
            forEachChild(node, [&](Node const* child) { walk(child, onNode); });
        }

        template <typename Fn>
        void walkWithFuncs(Node const* node, Fn&& onNode) {
            std::unordered_set<Node const*> seen;

            auto walker = [&](auto& self, Node const* n) -> void {
                if (!n) return;
                onNode(n);

                if (n->type() == Node::Type::CallExpr) {
                    auto const& call = static_cast<CallExprNode const&>(*n);
                    if (call.callee()->type() != Node::Type::IdentifierExpr) return;
                    auto const& callee = static_cast<IdentifierExprNode const&>(*call.callee());
                    if (auto fn = m_symbols.find(callee.name())) {
                        auto [_, inserted] = seen.insert(fn->node);
                        if (inserted) self(self, fn->node);
                    }
                }

                forEachChild(n, [&](Node const* child) { self(self, child); });
            };

            walker(walker, node);
        }

        std::vector<FnDeclNode const*> getOrderedFunctions(FnDeclNode const* entry) {
            if (!entry) return {};

            std::vector<FnDeclNode const*> ordered;
            std::unordered_set<FnDeclNode const*> visited;

            auto dfs = [&](auto& self, Node const* node) -> void {
                if (!node) return;

                if (node->type() == Node::Type::CallExpr) {
                    auto const& call = static_cast<CallExprNode const&>(*node);
                    if (call.callee()->type() == Node::Type::IdentifierExpr) {
                        auto const& callee = static_cast<IdentifierExprNode const&>(*call.callee());

                        if (auto fnSymbol = m_symbols.find(callee.name())) {
                            if (fnSymbol->node && fnSymbol->node->type() == Node::Type::FnDecl) {
                                auto const* fnDecl = static_cast<FnDeclNode const*>(fnSymbol->node);

                                if (!visited.contains(fnDecl)) {
                                    visited.insert(fnDecl);
                                    self(self, fnDecl->body());
                                    ordered.push_back(fnDecl);
                                }
                            }
                        }
                    }
                }

                forEachChild(node, [&](Node const* child) { self(self, child); });
            };

            visited.insert(entry);
            dfs(dfs, entry->body());
            ordered.push_back(entry);

            return ordered;
        }

    private:
        SymbolTable const& m_symbols;
        StructRegistry const& m_structs;
        size_t m_tempVarCounter = 0;
        Stage m_currentStage = Stage::Vertex;
        std::vector<std::unordered_set<std::string>> m_localScopes;
    };
}
