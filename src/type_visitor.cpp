#include <fsher/semantic/type_visitor.hpp>

#include <cmath>
#include <limits>
#include <ranges>
#include <fsher/ast/nodes/assign_stmt.hpp>
#include <fsher/ast/nodes/binary.hpp>
#include <fsher/ast/nodes/block.hpp>
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
#include <fsher/ast/nodes/node.hpp>
#include <fsher/ast/nodes/return_stmt.hpp>
#include <fsher/ast/nodes/struct_literal.hpp>
#include <fsher/ast/nodes/ternary.hpp>
#include <fsher/ast/nodes/unary.hpp>

namespace fsher {
    TypeVisitor::TypeVisitor(
        StructRegistry& structs, SymbolTable& symbols, std::vector<AnalysisError>& errors
    ) noexcept
        : m_structs(structs),
          m_globals(symbols),
          m_currentScope(&symbols),
          m_errors(errors),
          m_resolver(structs) {}

    std::optional<Type> TypeVisitor::resolveType(std::string_view name, SourceRange const& range) {
        auto resolved = m_resolver.resolve(name);
        if (!resolved) {
            m_errors.emplace_back(AnalysisError::Kind::UnknownType, fmt::format("unknown type '{}'", name), range);
        }
        return resolved;
    }

    std::optional<Type> TypeVisitor::resolveType(TypeName const& type, SourceRange const& range) {
        auto resolved = this->resolveType(type.name, range);
        if (!resolved) return std::nullopt;

        if (type.arrayDims.empty()) {
            return resolved;
        }

        // TODO: handle multi-dimensional arrays
        if (type.arrayDims.size() > 1) {
            m_errors.emplace_back(
                AnalysisError::Kind::UnknownType,
                "multi-dimensional arrays are not supported",
                range
            );
            return std::nullopt;
        }

        return Type::Array(std::move(*resolved), type.arrayDims[0]);
    }

    Type TypeVisitor::visitIdentifier(IdentifierExprNode const& identifier) {
        auto symbol = m_currentScope->find(identifier.name());
        if (!symbol) {
            m_errors.emplace_back(
                AnalysisError::Kind::UndeclaredSymbol,
                fmt::format("undeclared identifier '{}'", identifier.name()),
                identifier.range()
            );
            return Type::Error();
        }

        if (symbol->kind == Symbol::Kind::Function) {
            m_errors.emplace_back(
                AnalysisError::Kind::UnknownType,
                fmt::format("'{}' is a function but used as value", identifier.name()),
                identifier.range()
            );
            return Type::Error();
        }

        if (symbol->kind == Symbol::Kind::Uniform || symbol->kind == Symbol::Kind::In ||
            symbol->kind == Symbol::Kind::Out || symbol->kind == Symbol::Kind::Param ||
            symbol->kind == Symbol::Kind::Local) {
            return symbol->type;
        }

        switch (symbol->state) {
            case Symbol::ResolutionState::Resolved:
                return symbol->type;

            case Symbol::ResolutionState::Error:
                return Type::Error();

            case Symbol::ResolutionState::Resolving:
                symbol->state = Symbol::ResolutionState::Error;
                m_errors.emplace_back(
                    AnalysisError::Kind::RecursiveDeclaration,
                    fmt::format("recursive usage inside '{}'", identifier.name()),
                    identifier.range()
                );
                return Type::Error();

            case Symbol::ResolutionState::Unresolved: {
                symbol->state = Symbol::ResolutionState::Resolving;
                Type resolved = this->deduceTypes(symbol->node);

                if (symbol->state == Symbol::ResolutionState::Error) {
                    return Type::Error();
                }

                symbol->type = resolved;
                symbol->state = Symbol::ResolutionState::Resolved;
                return symbol->type;
            }
        }

        std::unreachable();
    }

    Type TypeVisitor::visitConstDecl(ConstDeclNode const* decl) {
        auto symbol = m_currentScope->find(decl->name());
        if (symbol && symbol->state == Symbol::ResolutionState::Resolved) {
            return symbol->type;
        }

        Type valueType = deduceTypes(decl->value());
        if (valueType.kind == Type::Kind::Error) return Type::Error();

        if (symbol && !symbol->declaredType.empty()) {
            Type targetType = symbol->type;

            if (auto coerced = tryCoerceLiteral(decl->value(), valueType, targetType)) {
                return *coerced;
            }

            if (valueType != targetType) {
                m_errors.emplace_back(
                    AnalysisError::Kind::TypeMismatch,
                    fmt::format("type mismatch: expected '{}', got '{}'", targetType, valueType),
                    decl->value()->range()
                );
                return Type::Error();
            }

            return targetType;
        }

        if (valueType.kind == Type::Kind::Scalar) {
            if (valueType.scalarKind == ScalarKind::UntypedFloat) return Type::Float();
            if (valueType.scalarKind == ScalarKind::UntypedInt) return Type::Int();
        }

        return valueType;
    }

    Type TypeVisitor::visitFuncDecl(FnDeclNode const& decl) {
        auto symbol = m_globals.find(decl.name());
        if (symbol && symbol->state == Symbol::ResolutionState::Resolved) {
            return symbol->type;
        }

        SymbolTable fnScope(m_currentScope);
        SymbolTable* prevScope = m_currentScope;
        m_currentScope = &fnScope;

        for (auto const& param : decl.parameters()) {
            Type paramType = resolveType(param.type, param.range).value_or(Type::Error());
            Symbol paramSym{
                .name = param.name,
                .declaredType = param.type,
                .node = &decl,
                .type = paramType,
                .kind = Symbol::Kind::Param,
                .state = Symbol::ResolutionState::Resolved
            };
            if (!fnScope.insert(paramSym)) {
                m_errors.emplace_back(
                    AnalysisError::Kind::DuplicateSymbol,
                    fmt::format("duplicate parameter '{}' in fn '{}'", param.name, decl.name()),
                    param.range
                );
            }
        }

        auto prevFnRet = m_currentFnReturnType;
        if (symbol && !symbol->declaredType.empty()) {
            m_currentFnReturnType = symbol->type;
        } else {
            m_currentFnReturnType = std::nullopt;
        }

        Type valueType = deduceTypes(decl.body());

        m_currentFnReturnType = prevFnRet;
        m_currentScope = prevScope;

        if (valueType.kind == Type::Kind::Error) return Type::Error();

        if (symbol && !symbol->declaredType.empty()) {
            Type targetType = symbol->type;

            if (auto coerced = tryCoerceLiteral(decl.body(), valueType, targetType)) {
                return *coerced;
            }

            if (valueType != targetType) {
                m_errors.emplace_back(
                    AnalysisError::Kind::TypeMismatch,
                    fmt::format("type mismatch: expected '{}', got '{}'", targetType, valueType),
                    decl.body()->range()
                );
                return Type::Error();
            }

            return targetType;
        }

        if (valueType.kind == Type::Kind::Scalar) {
            if (valueType.scalarKind == ScalarKind::UntypedFloat) return Type::Float();
            if (valueType.scalarKind == ScalarKind::UntypedInt) return Type::Int();
        }

        return valueType;
    }

    Type TypeVisitor::visitBlock(BlockNode const& block) {
        SymbolTable blockScope(m_currentScope);
        SymbolTable* prevScope = m_currentScope;
        m_currentScope = &blockScope;

        for (auto const& stmt : block.statements()) {
            if (stmt) {
                this->visitStmt(*stmt);
            }
        }

        Type result = Type::Void();
        if (auto trail = block.trail()) {
            result = this->deduceTypes(trail);
        }

        m_currentScope = prevScope;
        return result;
    }

    void TypeVisitor::visitStmt(StmtNode const& stmt) {
        switch (stmt.type()) {
            case Node::Type::LetStmt: visitLetStmt(static_cast<LetStmtNode const&>(stmt)); break;
            case Node::Type::ConstStmt: visitConstStmt(static_cast<ConstStmtNode const&>(stmt)); break;
            case Node::Type::AssignStmt: visitAssignStmt(static_cast<AssignStmtNode const&>(stmt)); break;
            case Node::Type::ForStmt: visitForStmt(static_cast<ForStmtNode const&>(stmt)); break;
            case Node::Type::ReturnStmt: visitReturnStmt(static_cast<ReturnStmtNode const&>(stmt)); break;
            case Node::Type::ExprStmt: visitExprStmt(static_cast<ExprStmtNode const&>(stmt)); break;
            default: break;
        }
    }

    void TypeVisitor::visitLetStmt(LetStmtNode const& stmt) {
        Type initType = stmt.initializer() ? deduceTypes(stmt.initializer()) : Type::Error();

        Type varType = initType;
        if (!stmt.typeName().empty()) {
            auto targetType = resolveType(stmt.typeName(), stmt.range()).value_or(Type::Error());
            if (stmt.initializer()) {
                if (auto coerced = tryCoerceLiteral(stmt.initializer(), initType, targetType)) {
                    varType = *coerced;
                } else if (initType != targetType && initType.kind != Type::Kind::Error) {
                    m_errors.emplace_back(
                        AnalysisError::Kind::TypeMismatch,
                        fmt::format("type mismatch: expected '{}', got '{}'", targetType, initType),
                        stmt.initializer()->range()
                    );
                    varType = targetType;
                } else {
                    varType = targetType;
                }
            } else {
                varType = targetType;
            }
        } else if (varType.kind == Type::Kind::Scalar) {
            if (varType.scalarKind == ScalarKind::UntypedFloat) varType = Type::Float();
            if (varType.scalarKind == ScalarKind::UntypedInt) varType = Type::Int();
        }

        Symbol sym{
            .name = stmt.name(),
            .declaredType = stmt.typeName(),
            .node = &stmt,
            .type = varType,
            .kind = Symbol::Kind::Local,
            .state = Symbol::ResolutionState::Resolved
        };

        if (!m_currentScope->insert(sym)) {
            m_errors.emplace_back(
                AnalysisError::Kind::DuplicateSymbol,
                fmt::format("symbol '{}' already declared in this scope", stmt.name()),
                stmt.range()
            );
        }
    }

    void TypeVisitor::visitConstStmt(ConstStmtNode const& stmt) {
        Type initType = stmt.initializer() ? deduceTypes(stmt.initializer()) : Type::Error();

        Type varType = initType;
        if (!stmt.typeName().empty()) {
            auto targetType = resolveType(stmt.typeName(), stmt.range()).value_or(Type::Error());
            if (stmt.initializer()) {
                if (auto coerced = tryCoerceLiteral(stmt.initializer(), initType, targetType)) {
                    varType = *coerced;
                } else if (initType != targetType && initType.kind != Type::Kind::Error) {
                    m_errors.emplace_back(
                        AnalysisError::Kind::TypeMismatch,
                        fmt::format("type mismatch: expected '{}', got '{}'", targetType, initType),
                        stmt.initializer()->range()
                    );
                    varType = targetType;
                } else {
                    varType = targetType;
                }
            } else {
                varType = targetType;
            }
        } else if (varType.kind == Type::Kind::Scalar) {
            if (varType.scalarKind == ScalarKind::UntypedFloat) varType = Type::Float();
            if (varType.scalarKind == ScalarKind::UntypedInt) varType = Type::Int();
        }

        Symbol sym{
            .name = stmt.name(),
            .declaredType = stmt.typeName(),
            .node = &stmt,
            .type = varType,
            .kind = Symbol::Kind::Const,
            .state = Symbol::ResolutionState::Resolved
        };

        if (!m_currentScope->insert(sym)) {
            m_errors.emplace_back(
                AnalysisError::Kind::DuplicateSymbol,
                fmt::format("symbol '{}' already declared in this scope", stmt.name()),
                stmt.range()
            );
        }
    }

    void TypeVisitor::visitAssignStmt(AssignStmtNode const& stmt) {
        Type targetType = stmt.target() ? deduceTypes(stmt.target()) : Type::Error();
        Type valueType = stmt.value() ? deduceTypes(stmt.value()) : Type::Error();

        if (targetType.kind == Type::Kind::Error || valueType.kind == Type::Kind::Error) return;

        if (auto coerced = tryCoerceLiteral(stmt.value(), valueType, targetType)) {
            return;
        }

        if (targetType != valueType) {
            m_errors.emplace_back(
                AnalysisError::Kind::TypeMismatch,
                fmt::format("assignment type mismatch: cannot assign '{}' to '{}'", valueType, targetType),
                stmt.range()
            );
        }
    }

    void TypeVisitor::visitForStmt(ForStmtNode const& stmt) {
        if (stmt.rangeStart()) deduceTypes(stmt.rangeStart());
        if (stmt.rangeEnd()) deduceTypes(stmt.rangeEnd());

        SymbolTable loopScope(m_currentScope);
        SymbolTable* prevScope = m_currentScope;
        m_currentScope = &loopScope;

        Symbol loopSym{
            .name = stmt.loopVar(),
            .declaredType = TypeName{.name = "i32"},
            .node = &stmt,
            .type = Type::Int(),
            .kind = Symbol::Kind::Local,
            .state = Symbol::ResolutionState::Resolved
        };
        loopScope.insert(loopSym);

        if (stmt.body()) {
            deduceTypes(stmt.body());
        }

        m_currentScope = prevScope;
    }

    void TypeVisitor::visitReturnStmt(ReturnStmtNode const& stmt) {
        Type valType = stmt.value() ? deduceTypes(stmt.value()) : Type::Void();
        if (m_currentFnReturnType && valType != *m_currentFnReturnType && valType.kind != Type::Kind::Error) {
            m_errors.emplace_back(
                AnalysisError::Kind::TypeMismatch,
                fmt::format("return type mismatch: expected '{}', got '{}'", *m_currentFnReturnType, valType),
                stmt.range()
            );
        }
    }

    void TypeVisitor::visitExprStmt(ExprStmtNode const& stmt) {
        if (stmt.expr()) {
            deduceTypes(stmt.expr());
        }
    }

    Type TypeVisitor::visitIfExpr(IfExprNode const& expr) {
        auto condType = this->deduceTypes(expr.condition());

        if (condType.kind != Type::Kind::Scalar || condType.scalarKind != ScalarKind::Bool) {
            m_errors.emplace_back(
                AnalysisError::Kind::TypeMismatch,
                fmt::format("condition expression must be of type 'bool', got '{}'", condType),
                expr.condition()->range()
            );
        }

        return this->deduceTypes(expr.thenBlock());
    }

    Type TypeVisitor::visitCallExpr(CallExprNode const& call) {
        if (call.callee()->type() != Node::Type::IdentifierExpr) {
            m_errors.emplace_back(AnalysisError::Kind::UnknownType, "callee must be a function name", call.callee()->range());
            return Type::Error();
        }

        auto const& calleeIdent = static_cast<IdentifierExprNode const&>(*call.callee());

        std::vector<Type> argTypes;
        for (auto const& arg : call.args()) {
            Type t = deduceTypes(arg.get());
            argTypes.push_back(t);
        }

        auto symbol = m_currentScope->find(calleeIdent.name());
        if (symbol) {
            if (symbol->kind != Symbol::Kind::Function) {
                m_errors.emplace_back(
                    AnalysisError::Kind::TypeMismatch,
                    fmt::format("'{}' is not callable", calleeIdent.name()),
                    calleeIdent.range()
                );
                return Type::Error();
            }

            auto funcDecl = static_cast<FnDeclNode const*>(symbol->node);
            if (funcDecl) {
                return this->visitFuncDecl(*funcDecl);
            }

            if (symbol->builtinId.has_value()) {
                auto const& registry = BuiltinRegistry::get();
                auto match = registry.resolve(calleeIdent.name(), argTypes);
                if (match) {
                    return match->returnType;
                }
            }

            m_errors.emplace_back(
                AnalysisError::Kind::TypeMismatch,
                fmt::format(
                    "no matching overload for builtin '{}' with {} argument(s)",
                    calleeIdent.name(), argTypes.size()
                ),
                calleeIdent.range()
            );
            return Type::Error();
        }

        if (auto resolvedType = m_resolver.resolve(calleeIdent.name())) {
            return *resolvedType;
        }

        m_errors.emplace_back(
            AnalysisError::Kind::UndeclaredSymbol,
            fmt::format("undeclared function or type '{}'", calleeIdent.name()),
            calleeIdent.range()
        );
        return Type::Error();
    }

    static std::optional<Type> resolveOps(TokenType op, Type const& lhs, Type const& rhs) {
        auto isUntyped = [](ScalarKind k) {
            return k == ScalarKind::UntypedInt || k == ScalarKind::UntypedFloat;
        };

        auto isInt = [](ScalarKind k) {
            return k == ScalarKind::Int32 || k == ScalarKind::UInt32 || k == ScalarKind::UntypedInt;
        };

        if (op == TokenType::PLUS || op == TokenType::MINUS || op == TokenType::ASTERISK || op == TokenType::SLASH) {
            if (lhs.kind == Type::Kind::Scalar && rhs.kind == Type::Kind::Scalar) {
                if (lhs == rhs) return lhs;
                if (isUntyped(lhs.scalarKind) && !isUntyped(rhs.scalarKind)) return rhs;
                if (!isUntyped(lhs.scalarKind) && isUntyped(rhs.scalarKind)) return lhs;
                if (isUntyped(lhs.scalarKind) && isUntyped(rhs.scalarKind)) {
                    if (lhs.scalarKind == ScalarKind::UntypedFloat || rhs.scalarKind == ScalarKind::UntypedFloat) {
                        return Type::Numeric(true);
                    }
                    return Type::Numeric(false);
                }
            }

            if (lhs.kind == Type::Kind::Vector && rhs.kind == Type::Kind::Vector) {
                if (lhs.rows == rhs.rows && lhs.scalarKind == rhs.scalarKind) {
                    return lhs;
                }
            }

            if (lhs.kind == Type::Kind::Vector && rhs.kind == Type::Kind::Scalar) {
                if (lhs.scalarKind == rhs.scalarKind || isUntyped(rhs.scalarKind)) {
                    return lhs;
                }
            }

            if (lhs.kind == Type::Kind::Scalar && rhs.kind == Type::Kind::Vector) {
                if (lhs.scalarKind == rhs.scalarKind || isUntyped(lhs.scalarKind)) {
                    return rhs;
                }
            }

            if (lhs.kind == Type::Kind::Matrix && rhs.kind == Type::Kind::Vector && lhs.cols == rhs.rows) {
                return Type::Vector(lhs.scalarKind, lhs.rows);
            }
            if (lhs.kind == Type::Kind::Vector && rhs.kind == Type::Kind::Matrix && lhs.rows == rhs.rows) {
                return Type::Vector(rhs.scalarKind, rhs.cols);
            }
            if (lhs.kind == Type::Kind::Matrix && rhs.kind == Type::Kind::Matrix && lhs.cols == rhs.rows) {
                return Type::Matrix(lhs.scalarKind, rhs.cols, lhs.rows);
            }
        }

        if (op == TokenType::PERCENT || op == TokenType::AMPERSAND || op == TokenType::PIPE || op == TokenType::CARET) {
            if (lhs.kind == Type::Kind::Scalar && rhs.kind == Type::Kind::Scalar) {
                if (isInt(lhs.scalarKind) && isInt(rhs.scalarKind)) {
                    if (lhs == rhs) return lhs;
                    if (isUntyped(lhs.scalarKind) && !isUntyped(rhs.scalarKind)) return rhs;
                    if (!isUntyped(lhs.scalarKind) && isUntyped(rhs.scalarKind)) return lhs;
                    if (isUntyped(lhs.scalarKind) && isUntyped(rhs.scalarKind)) {
                        return Type::Numeric(false);
                    }
                }
            }

            if (lhs.kind == Type::Kind::Vector && rhs.kind == Type::Kind::Vector) {
                if (lhs.rows == rhs.rows && lhs.scalarKind == rhs.scalarKind && isInt(lhs.scalarKind)) {
                    return lhs;
                }
            }
            if (lhs.kind == Type::Kind::Vector && rhs.kind == Type::Kind::Scalar) {
                if ((lhs.scalarKind == rhs.scalarKind || isUntyped(rhs.scalarKind)) && isInt(lhs.scalarKind)) {
                    return lhs;
                }
            }
            if (lhs.kind == Type::Kind::Scalar && rhs.kind == Type::Kind::Vector) {
                if ((lhs.scalarKind == rhs.scalarKind || isUntyped(lhs.scalarKind)) && isInt(rhs.scalarKind)) {
                    return rhs;
                }
            }

            return std::nullopt;
        }

        if (op == TokenType::EQUAL_EQUAL || op == TokenType::BANG_EQUAL) {
            if (lhs == rhs) return Type::Bool();
            if (lhs.kind == Type::Kind::Scalar && rhs.kind == Type::Kind::Scalar) return Type::Bool();
        }

        if (op == TokenType::LESS || op == TokenType::LESS_EQUAL ||
            op == TokenType::GREATER || op == TokenType::GREATER_EQUAL) {
            return Type::Bool();
        }

        if (op == TokenType::AMPERSAND_AMPERSAND || op == TokenType::PIPE_PIPE) {
            return Type::Bool();
        }

        return std::nullopt;
    }

    Type TypeVisitor::visitBinaryExpr(BinaryExprNode const& call) {
        auto lhs = this->deduceTypes(call.lhs());
        auto rhs = this->deduceTypes(call.rhs());

        if (lhs.kind == Type::Kind::Error || rhs.kind == Type::Kind::Error) {
            return Type::Error();
        }

        if (auto result = resolveOps(call.op(), lhs, rhs)) {
            return *result;
        }

        m_errors.emplace_back(
            AnalysisError::Kind::InvalidOperation,
            fmt::format("invalid operation: '{}' between '{}' and '{}'", call.op(), lhs, rhs),
            call.range()
        );

        return Type::Error();
    }

    Type TypeVisitor::visitUnaryExpr(UnaryExprNode const& expr) {
        Type operandType = deduceTypes(expr.operand());
        if (operandType.kind == Type::Kind::Error) return Type::Error();

        if (expr.op() == TokenType::MINUS) {
            return operandType;
        }
        if (expr.op() == TokenType::BANG) {
            return Type::Bool();
        }

        return operandType;
    }

    Type TypeVisitor::visitFieldExpr(FieldExprNode const& expr) {
        Type targetType = deduceTypes(expr.target());
        if (targetType.kind == Type::Kind::Error) return Type::Error();

        if (targetType.kind == Type::Kind::Struct) {
            if (auto structInfo = m_structs.lookup(targetType.structName)) {
                if (auto member = structInfo->find(expr.field())) {
                    return member->type;
                }
                m_errors.emplace_back(
                    AnalysisError::Kind::UndeclaredSymbol,
                    fmt::format("struct '{}' has no field '{}'", targetType.structName, expr.field()),
                    expr.range()
                );
                return Type::Error();
            }
        }

        if (targetType.kind == Type::Kind::Vector) {
            std::string_view f = expr.field();
            if (f.empty()) return Type::Error();

            static constexpr std::string_view validComponents = "xyzwrgba";
            bool allValid = std::ranges::all_of(f, [](char c) {
                return validComponents.find(c) != std::string_view::npos;
            });

            if (!allValid) {
                m_errors.emplace_back(
                    AnalysisError::Kind::InvalidOperation,
                    fmt::format("invalid vector swizzle '.{}'", f),
                    expr.range()
                );
                return Type::Error();
            }

            if (f.size() == 1) {
                return Type{.kind = Type::Kind::Scalar, .scalarKind = targetType.scalarKind};
            }
            if (f.size() >= 2 && f.size() <= 4) {
                return Type::Vector(targetType.scalarKind, static_cast<uint8_t>(f.size()));
            }

            m_errors.emplace_back(
                AnalysisError::Kind::InvalidOperation,
                fmt::format("vector swizzle '.{}' length out of bounds", f),
                expr.range()
            );
            return Type::Error();
        }

        m_errors.emplace_back(
            AnalysisError::Kind::InvalidOperation,
            fmt::format("cannot access field '{}' on type '{}'", expr.field(), targetType),
            expr.range()
        );
        return Type::Error();
    }

    Type TypeVisitor::visitIndexExpr(IndexExprNode const& expr) {
        Type targetType = deduceTypes(expr.target());
        Type indexType = deduceTypes(expr.index());

        if (targetType.kind == Type::Kind::Error || indexType.kind == Type::Kind::Error) return Type::Error();

        if (targetType.kind == Type::Kind::Vector) {
            return Type{.kind = Type::Kind::Scalar, .scalarKind = targetType.scalarKind};
        }
        if (targetType.kind == Type::Kind::Matrix) {
            return Type::Vector(targetType.scalarKind, targetType.rows);
        }
        if (targetType.kind == Type::Kind::Array) {
            return targetType.elementType ? *targetType.elementType : Type::Error();
        }

        m_errors.emplace_back(
            AnalysisError::Kind::InvalidOperation,
            fmt::format("cannot index into type '{}'", targetType),
            expr.range()
        );
        return Type::Error();
    }

    Type TypeVisitor::visitTernaryExpr(TernaryExprNode const& expr) {
        if (expr.condition()) deduceTypes(expr.condition());
        Type thenType = expr.thenExpr() ? deduceTypes(expr.thenExpr()) : Type::Void();
        Type elseType = expr.elseExpr() ? deduceTypes(expr.elseExpr()) : Type::Void();
        if (thenType == elseType) return thenType;
        return thenType;
    }

    Type TypeVisitor::visitMatchExpr(MatchExprNode const& expr) {
        if (expr.scrutinee()) deduceTypes(expr.scrutinee());
        Type bodyType = Type::Void();
        for (auto const& arm : expr.arms()) {
            if (arm.pattern.kind == Pattern::Kind::Expression && arm.pattern.expr) {
                deduceTypes(arm.pattern.expr.get());
            }
            if (arm.body) {
                bodyType = deduceTypes(arm.body.get());
            }
        }
        return bodyType;
    }

    Type TypeVisitor::visitStructLiteralExpr(StructLiteralExprNode const& expr) {
        for (auto const& field : expr.fields()) {
            if (field.value) deduceTypes(field.value.get());
        }
        if (m_structs.lookup(expr.typeName())) {
            return Type::Struct(expr.typeName());
        }
        return resolveType(expr.typeName(), expr.range()).value_or(Type::Error());
    }

    Type TypeVisitor::deduceTypes(Node const* node) {
        if (!node) return Type::Void();

        Type result;
        switch (node->type()) {
            case Node::Type::ConstDecl: result = visitConstDecl(static_cast<ConstDeclNode const*>(node)); break;
            case Node::Type::IdentifierExpr: result = visitIdentifier(static_cast<IdentifierExprNode const&>(*node)); break;
            case Node::Type::FnDecl: result = visitFuncDecl(static_cast<FnDeclNode const&>(*node)); break;
            case Node::Type::Block: result = visitBlock(static_cast<BlockNode const&>(*node)); break;
            case Node::Type::IfExpr: result = visitIfExpr(static_cast<IfExprNode const&>(*node)); break;
            case Node::Type::CallExpr: result = visitCallExpr(static_cast<CallExprNode const&>(*node)); break;
            case Node::Type::BinaryExpr: result = visitBinaryExpr(static_cast<BinaryExprNode const&>(*node)); break;
            case Node::Type::UnaryExpr: result = visitUnaryExpr(static_cast<UnaryExprNode const&>(*node)); break;
            case Node::Type::FieldExpr: result = visitFieldExpr(static_cast<FieldExprNode const&>(*node)); break;
            case Node::Type::IndexExpr: result = visitIndexExpr(static_cast<IndexExprNode const&>(*node)); break;
            case Node::Type::TernaryExpr: result = visitTernaryExpr(static_cast<TernaryExprNode const&>(*node)); break;
            case Node::Type::MatchExpr: result = visitMatchExpr(static_cast<MatchExprNode const&>(*node)); break;
            case Node::Type::StructLiteralExpr: result = visitStructLiteralExpr(static_cast<StructLiteralExprNode const&>(*node)); break;
            case Node::Type::LiteralExpr: {
                auto& literal = static_cast<LiteralExprNode const&>(*node);
                switch (literal.valueType()) {
                    case LiteralExprNode::ValueType::Integer: result = Type::Numeric(); break;
                    case LiteralExprNode::ValueType::Float: result = Type::Numeric(true); break;
                    case LiteralExprNode::ValueType::Boolean: result = Type::Bool(); break;
                    default: std::unreachable();
                }
                break;
            }
            case Node::Type::LetStmt:
            case Node::Type::ConstStmt:
            case Node::Type::AssignStmt:
            case Node::Type::ForStmt:
            case Node::Type::ReturnStmt:
            case Node::Type::ExprStmt:
                visitStmt(static_cast<StmtNode const&>(*node));
                result = Type::Void();
                break;
            default: {
                m_errors.emplace_back(AnalysisError::Kind::UnknownType, "FIXME: unable to deduce type for node", node->range());
                return Type::Error();
            }
        }

        if (auto* expr = dynamic_cast<ExprNode const*>(node)) {
            expr->setResolvedType(result);
        }

        return result;
    }

    TypeVisitor::Coercion TypeVisitor::checkLiteralCoercion(LiteralExprNode const& literal, Type const& target) {
        if (target.kind != Type::Kind::Scalar) {
            return {CoercionResult::Ok, {}};
        }

        switch (literal.valueType()) {
            case LiteralExprNode::ValueType::Integer: {
                int64_t v = literal.intValue();

                if (target.scalarKind == ScalarKind::UInt32) {
                    if (v < 0) {
                        return {CoercionResult::Error, fmt::format("negative literal '{}' cannot be assigned to 'u32'", v)};
                    }
                    if (v > std::numeric_limits<uint32_t>::max()) {
                        return {CoercionResult::Error, fmt::format("literal '{}' does not fit in 'u32'", v)};
                    }
                    return {CoercionResult::Ok, {}};
                }

                if (target.scalarKind == ScalarKind::Int32) {
                    if (v < std::numeric_limits<int32_t>::min() || v > std::numeric_limits<int32_t>::max()) {
                        return {CoercionResult::Error, fmt::format("literal '{}' does not fit in 'i32'", v)};
                    }
                    return {CoercionResult::Ok, {}};
                }

                if (target.scalarKind == ScalarKind::Float32) {
                    return {CoercionResult::Ok, {}};
                }

                break;
            }

            case LiteralExprNode::ValueType::Float: {
                double v = literal.floatValue();

                if (target.scalarKind == ScalarKind::Int32 || target.scalarKind == ScalarKind::UInt32) {
                    if (target.scalarKind == ScalarKind::UInt32 && v < 0.0) {
                        return {CoercionResult::Error, fmt::format("negative literal '{}' cannot be assigned to 'u32'", v)};
                    }

                    if (std::trunc(v) != v) {
                        return {CoercionResult::Warn, fmt::format("implicit truncation of '{}' to '{}'", v, target)};
                    }

                    if (target.scalarKind == ScalarKind::Int32) {
                        if (v < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
                            v > static_cast<double>(std::numeric_limits<int32_t>::max())) {
                            return {CoercionResult::Error, fmt::format("float literal '{}' out of bounds for 'i32'", v)};
                        }
                    } else if (target.scalarKind == ScalarKind::UInt32) {
                        if (v > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
                            return {CoercionResult::Error, fmt::format("float literal '{}' out of bounds for 'u32'", v)};
                        }
                    }

                    return {CoercionResult::Ok, {}};
                }

                if (target.scalarKind == ScalarKind::Float32) {
                    return {CoercionResult::Ok, {}};
                }

                break;
            }
            case LiteralExprNode::ValueType::Boolean: {
                if (target.scalarKind != ScalarKind::Bool) {
                    return {CoercionResult::Error, "cannot implicitly convert 'bool' literal to a numeric type"};
                }
                return {CoercionResult::Ok, {}};
            }
            default: break;
        }

        return {CoercionResult::Ok, {}};
    }

    std::optional<Type> TypeVisitor::tryCoerceLiteral(
        Node const* valueExpr,
        Type const& valueType,
        Type const& targetType
    ) const {
        if (valueType.kind != Type::Kind::Scalar || (valueType.scalarKind != ScalarKind::UntypedInt && valueType.scalarKind != ScalarKind::UntypedFloat)) {
            return std::nullopt;
        }

        if (valueExpr->type() != Node::Type::LiteralExpr) {
            return std::nullopt;
        }

        auto const& literal = static_cast<LiteralExprNode const&>(*valueExpr);
        auto coercion = checkLiteralCoercion(literal, targetType);

        switch (coercion.result) {
            case CoercionResult::Error:
                m_errors.emplace_back(AnalysisError::Kind::TypeMismatch, std::move(coercion.message), valueExpr->range());
                return Type::Error();
            case CoercionResult::Warn:
                m_errors.emplace_back(AnalysisError::Kind::TypeMismatch, std::move(coercion.message), valueExpr->range(), AnalysisError::Severity::Warning);
                return targetType;
            case CoercionResult::Ok:
                return targetType;
        }

        std::unreachable();
    }
}
