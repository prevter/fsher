#include <ranges>
#include <fsher/semantic/analyzer.hpp>

#include <fsher/ast/nodes/const_decl.hpp>
#include <fsher/ast/nodes/fn_decl.hpp>
#include <fsher/ast/nodes/global_decl.hpp>
#include <fsher/ast/nodes/struct_decl.hpp>
#include <fsher/semantic/type_visitor.hpp>

namespace fsher {
    std::optional<Type> TypeResolver::resolve(std::string_view name) const noexcept {
        static util::StringMap<Type> const s_builtinTypes = {
            {"f32", Type::Float(false)},
            {"f64", Type::Float(true)},
            {"i32", Type::Int(false)},
            {"u32", Type::Int(true)},
            {"int", Type::Int(false)},
            {"float", Type::Float(false)},
            {"uint", Type::Int(true)},

            {"bool", Type::Bool()},
            {"void", Type::Void()},

            {"sampler2D", Type::Sampler2D()},

            // vectors
            {"vec2", Type::Vector(ScalarKind::Float32, 2)},
            {"vec3", Type::Vector(ScalarKind::Float32, 3)},
            {"vec4", Type::Vector(ScalarKind::Float32, 4)},

            {"dvec2", Type::Vector(ScalarKind::Float64, 2)},
            {"dvec3", Type::Vector(ScalarKind::Float64, 3)},
            {"dvec4", Type::Vector(ScalarKind::Float64, 4)},

            {"ivec2", Type::Vector(ScalarKind::Int32, 2)},
            {"ivec3", Type::Vector(ScalarKind::Int32, 3)},
            {"ivec4", Type::Vector(ScalarKind::Int32, 4)},

            {"uvec2", Type::Vector(ScalarKind::UInt32, 2)},
            {"uvec3", Type::Vector(ScalarKind::UInt32, 3)},
            {"uvec4", Type::Vector(ScalarKind::UInt32, 4)},

            {"bvec2", Type::Vector(ScalarKind::Bool, 2)},
            {"bvec3", Type::Vector(ScalarKind::Bool, 3)},
            {"bvec4", Type::Vector(ScalarKind::Bool, 4)},

            // matrices
            {"mat2", Type::Matrix(ScalarKind::Float32, 2, 2)},
            {"mat3", Type::Matrix(ScalarKind::Float32, 3, 3)},
            {"mat4", Type::Matrix(ScalarKind::Float32, 4, 4)},

            {"dmat2", Type::Matrix(ScalarKind::Float64, 2, 2)},
            {"dmat3", Type::Matrix(ScalarKind::Float64, 3, 3)},
            {"dmat4", Type::Matrix(ScalarKind::Float64, 4, 4)},

            {"mat2x2", Type::Matrix(ScalarKind::Float32, 2, 2)},
            {"mat2x3", Type::Matrix(ScalarKind::Float32, 2, 3)},
            {"mat2x4", Type::Matrix(ScalarKind::Float32, 2, 4)},

            {"mat3x2", Type::Matrix(ScalarKind::Float32, 3, 2)},
            {"mat3x3", Type::Matrix(ScalarKind::Float32, 3, 3)},
            {"mat3x4", Type::Matrix(ScalarKind::Float32, 3, 4)},

            {"mat4x2", Type::Matrix(ScalarKind::Float32, 4, 2)},
            {"mat4x3", Type::Matrix(ScalarKind::Float32, 4, 3)},
            {"mat4x4", Type::Matrix(ScalarKind::Float32, 4, 4)},

            {"dmat2x2", Type::Matrix(ScalarKind::Float64, 2, 2)},
            {"dmat2x3", Type::Matrix(ScalarKind::Float64, 2, 3)},
            {"dmat2x4", Type::Matrix(ScalarKind::Float64, 2, 4)},

            {"dmat3x2", Type::Matrix(ScalarKind::Float64, 3, 2)},
            {"dmat3x3", Type::Matrix(ScalarKind::Float64, 3, 3)},
            {"dmat3x4", Type::Matrix(ScalarKind::Float64, 3, 4)},

            {"dmat4x2", Type::Matrix(ScalarKind::Float64, 4, 2)},
            {"dmat4x3", Type::Matrix(ScalarKind::Float64, 4, 3)},
            {"dmat4x4", Type::Matrix(ScalarKind::Float64, 4, 4)},
        };

        auto it = s_builtinTypes.find(name);
        if (it != s_builtinTypes.end()) {
            return it->second;
        }

        if (auto structInfo = m_structs.lookup(name)) {
            return Type::Struct(structInfo->name);
        }

        return std::nullopt;
    }

    std::optional<Type> TypeResolver::resolve(TypeName const& type) const noexcept {
        auto resolved = this->resolve(type.name);
        if (!resolved) return std::nullopt;

        if (type.arrayDims.empty()) {
            return resolved;
        }

        if (type.arrayDims.size() > 1) {
            return std::nullopt; // multi-dimensional arrays are not supported yet
        }

        return Type::Array(std::move(*resolved), type.arrayDims[0]);
    }

    bool StageRegistry::declare(Stage stage, FnDeclNode const& decl) noexcept {
        if (m_entries[static_cast<size_t>(stage)] != nullptr) {
            return false;
        }
        m_entries[static_cast<size_t>(stage)] = &decl;
        return true;
    }

    FnDeclNode const* StageRegistry::lookup(Stage stage) const noexcept {
        return m_entries[static_cast<size_t>(stage)];
    }

    std::vector<AnalysisError> SymbolCollector::collect(ProgramNode const& program) {
        // pass 1: collect all structs
        for (auto const& item : program.items()) {
            if (item->type() != Node::Type::StructDecl) {
                continue;
            }

            auto const& decl = static_cast<StructDeclNode const&>(*item);
            if (!m_structs.declare(StructInfo{
                .name = decl.name(),
                .members = {}
            })) {
                this->error(AnalysisError::Kind::DuplicateSymbol, fmt::format("struct '{}' redeclaration", decl.name()), decl.range());
            }
        }

        // pass 2: collect struct fields
        for (auto const& item : program.items()) {
            if (item->type() == Node::Type::StructDecl) {
                this->collectStructFields(static_cast<StructDeclNode const&>(*item));
            }
        }
        this->checkStructCycles();

        // pass 3: collect global declarations
        std::vector<ConstDeclNode*> constDecls;
        std::vector<FnDeclNode*> funcDecls;

        for (auto const& item : program.items()) {
            switch (item->type()) {
                case Node::Type::GlobalDecl: {
                    this->collectGlobalDecl(static_cast<GlobalDeclNode const&>(*item));
                    break;
                }
                case Node::Type::ConstDecl: {
                    constDecls.push_back(static_cast<ConstDeclNode*>(item.get()));
                    this->collectConstDecl(static_cast<ConstDeclNode const&>(*item));
                    break;
                }
                case Node::Type::FnDecl: {
                    funcDecls.push_back(static_cast<FnDeclNode*>(item.get()));
                    this->collectFnDecl(static_cast<FnDeclNode const&>(*item));
                    break;
                }
                default: break;
            }
        }

        // pass 4: deduce types
        this->visitConstNodes(constDecls);
        this->visitFnNodes(funcDecls);

        return std::move(m_errors);
    }

    void SymbolCollector::checkStructCycles() {
        std::unordered_set<std::string_view> visited, recStack;

        auto hasCycle = [&](auto& self, std::string_view current) -> bool {
            if (recStack.contains(current)) return true;
            if (visited.contains(current)) return false;

            visited.insert(current);
            recStack.insert(current);

            if (auto info = m_structs.lookup(current)) {
                for (auto const& member : info->members) {
                    if (member.type.kind == Type::Kind::Struct) {
                        if (self(self, member.type.structName)) {
                            this->error(
                                AnalysisError::Kind::RecursiveStruct,
                                fmt::format(
                                    "recursive dependency found: struct '{}' contains a cycle through field '{}'",
                                    current, member.name
                                ),
                                member.range
                            );
                            return true;
                        }
                    }
                }
            }

            return false;
        };

        for (auto const& name : m_structs.structs() | std::views::keys) {
            hasCycle(hasCycle, name);
        }
    }

    void SymbolCollector::collectStructFields(StructDeclNode const& decl) {
        std::vector<StructMember> fields;
        fields.reserve(decl.fields().size());

        for (auto const& field : decl.fields()) {
            if (!this->checkAttributesFor(field, AttributeScope::StructField)) {
                continue;
            }

            auto type = this->resolveOrError(field.type, field.range);

            if (std::ranges::any_of(fields, [&](StructMember const& f) { return f.name == field.name; })) {
                this->error(
                    AnalysisError::Kind::DuplicateStructField,
                    fmt::format("duplicate field '{}' in struct '{}'", field.name, decl.name()),
                    field.range
                );
            } else {
                fields.push_back(StructMember{
                    .name = field.name,
                    .type = type.value_or(Type{.kind = Type::Kind::Error}),
                    .range = field.range
                });
            }
        }

        m_structs.updateMembers(decl.name(), std::move(fields));
    }

    void SymbolCollector::collectGlobalDecl(GlobalDeclNode const& decl) {
        Symbol::Kind kind = [&] {
            switch (decl.declKind()) {
                case DeclKind::In: return Symbol::Kind::In;
                case DeclKind::Out: return Symbol::Kind::Out;
                case DeclKind::Uniform: return Symbol::Kind::Uniform;
            }
            std::unreachable();
        }();

        std::visit([&]<typename T0>(T0 const& target) {
            using T = std::decay_t<T0>;

            if constexpr (std::is_same_v<T, GlobalDeclNode::Single>) {
                if (!this->checkAttributesFor(decl, AttributeScope::Global)) {
                    return;
                }

                auto type = this->resolveOrError(target.type, decl.range()).value_or(Type{.kind = Type::Kind::Error});

                if (!m_globals.insert(Symbol{
                    .name = target.name,
                    .declaredType = target.type,
                    .node = &decl,
                    .type = type,
                    .kind = kind
                })) {
                    this->error(
                        AnalysisError::Kind::DuplicateSymbol,
                        fmt::format("duplicate symbol '{}'", target.name),
                        decl.range()
                    );
                }

                this->trackGlobalAttributes(decl.attributes(), target.name);
            } else if constexpr (std::is_same_v<T, GlobalDeclNode::Group>) {
                for (auto const& field : target.fields) {
                    if (!this->checkAttributesFor(field, AttributeScope::GlobalField)) {
                        return;
                    }

                    auto type = this->resolveOrError(field.type, field.range).value_or(Type{.kind = Type::Kind::Error});

                    if (!m_globals.insert(Symbol{
                        .name = field.name,
                        .declaredType = field.type,
                        .node = &decl,
                        .type = type,
                        .kind = kind
                    })) {
                        this->error(
                            AnalysisError::Kind::DuplicateSymbol,
                            fmt::format("duplicate symbol '{}'", field.name),
                            decl.range()
                        );
                    }

                    this->trackGlobalAttributes(field.attributes, field.name);
                }
            }
        }, decl.bindTarget());
    }

    void SymbolCollector::collectConstDecl(ConstDeclNode const& decl) {
        if (!this->checkAttributesFor(decl, AttributeScope::Const)) {
            return;
        }

        Type type{ .kind = Type::Kind::Unresolved };
        if (!decl.declType().empty()) {
            type = this->resolveOrError(decl.declType(), decl.range()).value_or(Type{.kind = Type::Kind::Error});
        }

        if (!m_globals.insert(Symbol{
            .name = decl.name(),
            .declaredType = decl.declType(),
            .node = &decl,
            .type = type,
            .kind = Symbol::Kind::Const,
        })) {
            this->error(
                AnalysisError::Kind::DuplicateSymbol,
                fmt::format("'{}' is already declared at global scope", decl.name()),
                decl.range()
            );
        }
    }

    void SymbolCollector::collectFnDecl(FnDeclNode const& decl) {
        if (!this->checkAttributesFor(decl, AttributeScope::Fn)) {
            return;
        }

        auto isVertex = decl.hasAttribute("vertex");
        auto isFragment = decl.hasAttribute("fragment");

        if (isVertex && isFragment) {
            this->error(
                AnalysisError::Kind::InvalidAttribute,
                "function cannot be both a vertex and fragment entry point",
                decl.range()
            );
            return;
        }

        auto checkVF = [&](bool check, Stage stage, std::string_view msg) -> bool {
            if (!check) return false;
            if (!m_stages.declare(stage, decl)) {
                this->error(
                    AnalysisError::Kind::DuplicateSymbol,
                    std::string(msg),
                    decl.range()
                );
            }
            return true;
        };

        if (checkVF(isVertex, Stage::Vertex, "vertex entry point redeclared") ||
            checkVF(isFragment, Stage::Fragment, "fragment entry point redeclared")) {
            return;
        }

        Type returnType{ .kind = Type::Kind::Unresolved };
        if (!decl.returnType().empty()) {
            returnType = this->resolveOrError(decl.returnType(), decl.range()).value_or(Type{.kind = Type::Kind::Error});
        }

        if (!m_globals.insert(Symbol{
            .name = decl.name(),
            .declaredType = decl.returnType(),
            .node = &decl,
            .type = returnType,
            .kind = Symbol::Kind::Function,
        })) {
            this->error(
                AnalysisError::Kind::DuplicateSymbol,
                fmt::format("'{}' is already declared at global scope", decl.name()),
                decl.range()
            );
        }
    }

    void SymbolCollector::visitConstNodes(std::vector<ConstDeclNode*> const& consts) {
        TypeVisitor visitor{ m_structs, m_globals, m_errors };
        for (auto decl : consts) {
            auto res = visitor.deduceTypes(decl);

            if (auto symbol = m_globals.find(decl->name())) {
                if (symbol->type.kind == Type::Kind::Unresolved) {
                    symbol->type = res;
                } else if (symbol->type != res && res.kind != Type::Kind::Error) {
                    this->error(
                        AnalysisError::Kind::DeduceMismatch,
                        fmt::format("type mismatch for const '{}': declared as '{}' but deduced as '{}'", decl->name(), symbol->declaredType, res),
                        decl->range()
                    );
                }
            }
        }
    }

    void SymbolCollector::visitFnNodes(std::vector<FnDeclNode*> const& funcs) {
        TypeVisitor visitor{ m_structs, m_globals, m_errors };
        for (auto decl : funcs) {
            auto res = visitor.deduceTypes(decl);

            if (auto symbol = m_globals.find(decl->name())) {
                if (symbol->type.kind == Type::Kind::Unresolved) {
                    symbol->type = res;
                } else if (symbol->type != res && res.kind != Type::Kind::Error) {
                    this->error(
                        AnalysisError::Kind::DeduceMismatch,
                        fmt::format("type mismatch for fn '{}': return type declared as '{}' but deduced as '{}'", decl->name(), symbol->declaredType, res),
                        decl->range()
                    );
                }
            }
        }
    }

    bool SymbolCollector::checkAttributes(
        std::vector<Attribute> const& attributes,
        AttributeScope scope,
        SourceRange const& range
    ) {
        static util::StringMap<std::vector<AttributeScope>> const legalScopes = {
            {"vertex", {AttributeScope::Fn}},
            {"fragment", {AttributeScope::Fn}},
            {"position", {AttributeScope::Global, AttributeScope::GlobalField}},
            {"mvp", {AttributeScope::Global, AttributeScope::GlobalField}},
            {"model", {AttributeScope::Global, AttributeScope::GlobalField}},
            {"view", {AttributeScope::Global, AttributeScope::GlobalField}},
            {"projection", {AttributeScope::Global, AttributeScope::GlobalField}},
        };

        bool ok = true;

        for (auto const& attr : attributes) {
            auto it = legalScopes.find(attr.name);
            if (it == legalScopes.end() || !std::ranges::contains(it->second, scope)) {
                this->error(
                    AnalysisError::Kind::InvalidAttribute,
                    fmt::format("'#[{}]' is not valid here", attr.name),
                    range
                );
                ok = false;
            }
        }

        return ok;
    }

    std::optional<Type> SymbolCollector::resolveOrError(TypeName const& type, SourceRange const& range) {
        auto resolved = m_resolver.resolve(type.name);
        if (!resolved) {
            this->error(AnalysisError::Kind::UnknownType, fmt::format("unknown type '{}'", type.name), range);
            return std::nullopt;
        }

        if (type.arrayDims.empty()) {
            return resolved;
        }

        if (type.arrayDims.size() > 1) {
            this->error(
                AnalysisError::Kind::UnknownType,
                fmt::format("multi-dimensional arrays are not yet supported (in type '{}')", type),
                range
            );
            return std::nullopt;
        }

        return Type::Array(std::move(*resolved), type.arrayDims[0]);
    }

    void SymbolCollector::trackGlobalAttributes(std::vector<Attribute> const& attributes, std::string_view name) {
        for (auto const& attr : attributes) {
            if (attr.name == "position") {
                m_stages.setPositionInput(name);
            } else if (attr.name == "mvp") {
                m_stages.setMatrixInput(name);
            }
        }
    }

    void SymbolCollector::error(AnalysisError::Kind kind, std::string message, SourceRange const& range) {
        m_errors.push_back(AnalysisError{
            .kind = kind,
            .message = std::move(message),
            .range = range
        });
    }
}
