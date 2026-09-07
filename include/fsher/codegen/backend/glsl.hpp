#pragma once
#include <ranges>

#include "generator.hpp"

namespace fsher::backend {
    struct GLSLTarget {
        uint16_t version = 330;
        bool isES = false;
        bool isVulkan = false;

        [[nodiscard]] constexpr bool usesInOutQualifiers() const noexcept {
            return isVulkan || version >= 130;
        }

        [[nodiscard]] constexpr bool usesLayoutLocation() const noexcept {
            if (isVulkan) return true;
            if (isES) return version >= 300;
            return version >= 330;
        }

        [[nodiscard]] constexpr bool usesUnifiedTexture() const noexcept {
            return isVulkan || version >= 130;
        }

        [[nodiscard]] constexpr bool usesFragColorOutput() const noexcept {
            if (isVulkan) return false;
            if (isES) return version < 300;
            return version < 130;
        }

        [[nodiscard]] constexpr bool needsPrecisionQualifiers() const noexcept {
            return isES && !isVulkan;
        }

        static constexpr GLSLTarget Desktop120() noexcept { return { 120, false, false }; }
        static constexpr GLSLTarget Desktop330() noexcept { return { 330, false, false }; }
        static constexpr GLSLTarget Desktop450() noexcept { return { 450, false, false }; }
        static constexpr GLSLTarget ES100() noexcept { return { 100, true, false }; }
        static constexpr GLSLTarget ES300() noexcept { return { 300, true, false }; }
        static constexpr GLSLTarget ES320() noexcept { return { 320, true, false }; }
        static constexpr GLSLTarget WebGL1() noexcept { return ES100(); }
        static constexpr GLSLTarget WebGL2() noexcept { return ES300(); }
        static constexpr GLSLTarget Vulkan() noexcept { return { 450, false, true }; }
    };

    class GLSLGenerator : public IGenerator {
    public:
        static constexpr size_t DEFAULT_INDENT = 4;

        using Semantic = ir::Semantic;

        explicit GLSLGenerator(GLSLTarget target = GLSLTarget::Desktop330()) noexcept
            : m_target(target) {}

        Output generate(ir::ShaderProgram const& program, CodeStyle style = CodeStyle::Default) override {
            m_style = style;
            m_nextBinding = 2;
            m_bindingAssignments.clear();

            std::string vertex, fragment;

            for (auto const& stage : program.stages) {
                m_semanticNames.clear();

                for (auto const& o : stage.outputs) {
                    if (o.semantic == Semantic::None) continue;
                    m_semanticNames[o.name] = builtinNameFor(o.semantic);
                }

                auto code = this->generateModule(stage, program);
                if (stage.stage == Stage::Vertex) vertex = std::move(code);
                else if (stage.stage == Stage::Fragment) fragment = std::move(code);
            }

            return { std::move(vertex), std::move(fragment) };
        }

        util::StringMap<uint32_t> const& bindingAssignments() const noexcept {
            return m_bindingAssignments;
        }

    private:
        std::string_view builtinNameFor(Semantic s) const {
            switch (s) {
                case Semantic::Position: return "gl_Position";
                case Semantic::FragColor: return m_target.usesFragColorOutput() ? "gl_FragColor" : "_fragColor";
                case Semantic::FragCoord: return "gl_FragCoord";
                case Semantic::FrontFacing: return "gl_FrontFacing";
                case Semantic::VertexId: return "gl_VertexID";
                case Semantic::InstanceId: return "gl_InstanceID";
                default: return "";
            }
        }

        static std::string_view tokenTypeToOperator(TokenType type) {
            switch (type) {
                case TokenType::PLUS: return "+";
                case TokenType::MINUS: return "-";
                case TokenType::ASTERISK: return "*";
                case TokenType::SLASH: return "/";
                case TokenType::PERCENT: return "%";
                case TokenType::AMPERSAND: return "&";
                case TokenType::PIPE: return "|";
                case TokenType::CARET: return "^";
                case TokenType::TILDE: return "~";
                case TokenType::LEFT_SHIFT: return "<<";
                case TokenType::RIGHT_SHIFT: return ">>";
                case TokenType::AMPERSAND_AMPERSAND: return "&&";
                case TokenType::PIPE_PIPE: return "||";
                case TokenType::BANG: return "!";
                case TokenType::EQUAL_EQUAL: return "==";
                case TokenType::BANG_EQUAL: return "!=";
                case TokenType::LESS: return "<";
                case TokenType::LESS_EQUAL: return "<=";
                case TokenType::GREATER: return ">";
                case TokenType::GREATER_EQUAL: return ">=";
                case TokenType::EQUAL: return "=";
                case TokenType::PLUS_EQUAL: return "+=";
                case TokenType::MINUS_EQUAL: return "-=";
                case TokenType::ASTERISK_EQUAL: return "*=";
                case TokenType::SLASH_EQUAL: return "/=";
                case TokenType::PERCENT_EQUAL: return "%=";
                case TokenType::CARET_EQUAL: return "^=";
                case TokenType::AMPERSAND_EQUAL: return "&=";
                case TokenType::PIPE_EQUAL: return "|=";
                case TokenType::LEFT_SHIFT_EQUAL: return "<<=";
                case TokenType::RIGHT_SHIFT_EQUAL: return ">>=";
                default: return "<ERROR>";
            }
        }

        std::string_view textureFnFor(ir::Call const& call) const {
            if (m_target.usesUnifiedTexture()) return "texture";
            if (!call.args.empty()) {
                switch (call.args[0].type.kind) {
                    case Type::Kind::Sampler2D: return "texture2D";
                    default: break;
                }
            }
            return "texture";
        }

        std::string_view resolveCallName(ir::Call const& call) const {
            if (call.callee == "texture") return textureFnFor(call);
            return call.callee;
        }

        void pushNewline(auto inserter) {
            if (m_style != CodeStyle::Compact) {
                fmt::format_to(inserter, "\n");
            }
        }

        [[nodiscard]] std::vector<std::string_view> requiredExtensions(std::vector<BuiltinID> const& usedBuiltins) const noexcept {
            std::vector<std::string_view> exts;

            if (m_target.isES && m_target.version < 300) {
                for (auto id : usedBuiltins) {
                    if (id == BuiltinID::DFdx || id == BuiltinID::DFdy || id == BuiltinID::Fwidth) {
                        exts.push_back("GL_OES_standard_derivatives");
                        break;
                    }
                }
            }

            return exts;
        }

        void writeExpression(auto inserter, ir::Expr const& expr) {
            std::visit([&]<typename T>(T const& s) {
                if constexpr (std::is_same_v<T, ir::VarRef>) {
                    auto it = m_semanticNames.find(s.name);
                    if (it != m_semanticNames.end()) {
                        fmt::format_to(inserter, "{}", it->second);
                    } else {
                        fmt::format_to(inserter, "{}", s.name);
                    }
                } else if constexpr (std::is_same_v<T, ir::Literal>) {
                    std::visit([&]<typename L>(L const& l) {
                        if constexpr (std::is_same_v<L, double>) {
                            fmt::memory_buffer buf;
                            fmt::format_to(std::back_inserter(buf), "{}", l);
                            if (!std::string_view{buf.data(), buf.end()}.contains('.')) {
                                fmt::format_to(std::back_inserter(buf), ".0", l);
                            }
                            fmt::format_to(inserter, "{}", std::string_view{buf.data(), buf.end()});
                        } else if constexpr (std::is_same_v<L, bool>) {
                            fmt::format_to(inserter, "{}", l ? "true" : "false");
                        } else {
                            fmt::format_to(inserter, "{}", l);
                        }
                    }, s.value);
                } else if constexpr (std::is_same_v<T, ir::Binary>) {
                    auto writeChild = [&](ir::Expr const& childExpr, bool isRhs) {
                        bool parenthesize = false;
                        if (auto childBin = std::get_if<ir::Binary>(&childExpr.node)) {
                            auto parentPrec = getPrecedence(s.op);
                            auto childPrec = getPrecedence(childBin->op);
                            if (childPrec < parentPrec) {
                                parenthesize = true;
                            } else if (childPrec == parentPrec && isRhs) {
                                parenthesize = true;
                            }
                        }
                        if (parenthesize) fmt::format_to(inserter, "(");
                        writeExpression(inserter, childExpr);
                        if (parenthesize) fmt::format_to(inserter, ")");
                    };

                    writeChild(*s.lhs, false);
                    fmt::format_to(inserter, " {} ", tokenTypeToOperator(s.op));
                    writeChild(*s.rhs, true);
                } else if constexpr (std::is_same_v<T, ir::FieldAccess>) {
                    writeExpression(inserter, *s.target);
                    fmt::format_to(inserter, ".{}", s.field);
                } else if constexpr (std::is_same_v<T, ir::IndexAccess>) {
                    writeExpression(inserter, *s.target);
                    fmt::format_to(inserter, "[");
                    writeExpression(inserter, *s.index);
                    fmt::format_to(inserter, "]");
                } else if constexpr (std::is_same_v<T, ir::StructLit>) {
                    fmt::format_to(inserter, "{}(", s.typeName);
                    for (size_t i = 0; i < s.fields.size(); ++i) {
                        writeExpression(inserter, s.fields[i].value);
                        if (i != s.fields.size() - 1) fmt::format_to(inserter, ", ");
                    }
                    fmt::format_to(inserter, ")");
                } else if constexpr (std::is_same_v<T, ir::Ternary>) {
                    fmt::format_to(inserter, "(");
                    writeExpression(inserter, *s.condition);
                    fmt::format_to(inserter, " ? ");
                    writeExpression(inserter, *s.thenExpr);
                    fmt::format_to(inserter, " : ");
                    writeExpression(inserter, *s.elseExpr);
                    fmt::format_to(inserter, ")");
                } else if constexpr (std::is_same_v<T, ir::Unary>) {
                    fmt::format_to(inserter, "{}", tokenTypeToOperator(s.op));
                    writeExpression(inserter, *s.operand);
                } else if constexpr (std::is_same_v<T, ir::Call>) {
                    fmt::format_to(inserter, "{}(", resolveCallName(s));
                    for (size_t i = 0; i < s.args.size(); ++i) {
                        writeExpression(inserter, s.args[i]);
                        if (i != s.args.size() - 1) fmt::format_to(inserter, ", ");
                    }
                    fmt::format_to(inserter, ")");
                } else {
                    fmt::println(stderr, "[WARN] Not implemented writeExpression for node of type '{}'", typeid(T).name());
                }
            }, expr.node);
        }

        void writeStatement(auto inserter, ir::Stmt const& stmt, size_t indent = DEFAULT_INDENT) {
            fmt::format_to(inserter, "{:>{}}", " ", indent);
            std::visit([&]<typename T>(T const& s) {
                if constexpr (std::is_same_v<T, ir::AssignStmt>) {
                    writeExpression(inserter, s.target);
                    fmt::format_to(inserter, " {} ", tokenTypeToOperator(s.op));
                    writeExpression(inserter, s.value);
                    fmt::format_to(inserter, ";");
                    pushNewline(inserter);
                } else if constexpr (std::is_same_v<T, ir::ReturnStmt>) {
                    if (s.value.has_value()) {
                        fmt::format_to(inserter, "return ");
                        writeExpression(inserter, *s.value);
                        fmt::format_to(inserter, ";");
                        pushNewline(inserter);
                    } else {
                        fmt::format_to(inserter, "return;");
                        pushNewline(inserter);
                    }
                } else if constexpr (std::is_same_v<T, ir::IfStmt>) {
                    writeIfChain(inserter, s, indent);
                } else if constexpr (std::is_same_v<T, ir::LetStmt>) {
                    if (s.value.has_value()) {
                        fmt::format_to(inserter, "{:c} {}{} = ", s.type.elementBase(), s.name, s.type.arraySuffix());
                        writeExpression(inserter, *s.value);
                        fmt::format_to(inserter, ";");
                        pushNewline(inserter);
                    } else {
                        fmt::format_to(inserter, "{:c} {}{};", s.type.elementBase(), s.name, s.type.arraySuffix());
                        pushNewline(inserter);
                    }
                } else if constexpr (std::is_same_v<T, ir::ExprStmt>) {
                    writeExpression(inserter, s.expr);
                    fmt::format_to(inserter, ";");
                    pushNewline(inserter);
                } else if constexpr (std::is_same_v<T, ir::ForStmt>) {
                    writeForLoop(inserter, s, indent);
                } else if constexpr (std::is_same_v<T, ir::ContinueStmt>) {
                    fmt::format_to(inserter, "continue;");
                    pushNewline(inserter);
                } else if constexpr (std::is_same_v<T, ir::BreakStmt>) {
                    fmt::format_to(inserter, "break;");
                    pushNewline(inserter);
                } else if constexpr (std::is_same_v<T, ir::DiscardStmt>) {
                    fmt::format_to(inserter, "discard;");
                    pushNewline(inserter);
                } else {
                    fmt::println(stderr, "[WARN] Not implemented writeStatement for node of type '{}'", typeid(T).name());
                }
            }, stmt.node);
        }

        void writeIfChain(auto inserter, ir::IfStmt const& s, size_t indent) {
            auto const& writeBody = [&](std::vector<ir::Stmt> const& body, size_t innerIndent) {
                for (auto const& innerStmt : body) {
                    writeStatement(inserter, innerStmt, innerIndent);
                }
            };

            fmt::format_to(inserter, "if (");
            writeExpression(inserter, s.condition);
            fmt::format_to(inserter, ") {{");
            pushNewline(inserter);
            writeBody(s.thenBody, indent + DEFAULT_INDENT);

            if (!s.elseBody.empty()) {
                if (s.elseBody.size() == 1 && std::holds_alternative<ir::IfStmt>(s.elseBody[0].node)) {
                    fmt::format_to(inserter, "{:>{}}", " ", indent);
                    fmt::format_to(inserter, "}} else ");
                    writeIfChain(inserter, std::get<ir::IfStmt>(s.elseBody[0].node), indent);
                    return;
                }
                fmt::format_to(inserter, "{:>{}}", " ", indent);
                fmt::format_to(inserter, "}} else {{");
                pushNewline(inserter);
                writeBody(s.elseBody, indent + DEFAULT_INDENT);
            }

            fmt::format_to(inserter, "{:>{}}", " ", indent);
            fmt::format_to(inserter, "}}");
            pushNewline(inserter);
        }

        void writeForLoop(auto inserter, ir::ForStmt const& s, size_t indent) {
            fmt::format_to(inserter, "for (int {} = ", s.loopVar);
            writeExpression(inserter, s.rangeStart);
            fmt::format_to(inserter, "; {} {} ", s.loopVar, s.inclusive ? "<=" : "<");
            writeExpression(inserter, s.rangeEnd);
            fmt::format_to(inserter, "; {}++) {{", s.loopVar);
            pushNewline(inserter);
            for (auto const& bodyStmt : s.body) {
                writeStatement(inserter, bodyStmt, indent + DEFAULT_INDENT);
            }
            fmt::format_to(inserter, "{:>{}}", " ", indent);
            fmt::format_to(inserter, "}}");
            pushNewline(inserter);
        }

        void writeFunction(auto inserter, ir::Function const& function) {
            pushNewline(inserter);
            fmt::format_to(inserter, "{:c} {}(", function.returnType, function.name);

            for (auto const& [i, param] : std::ranges::views::enumerate(function.params)) {
                fmt::format_to(inserter, "{:c} {}{}", param.type.elementBase(), param.name, param.type.arraySuffix());
                if (i != function.params.size() - 1) {
                    fmt::format_to(inserter, ", ");
                }
            }

            fmt::format_to(inserter, ") {{");

            if (function.body.empty()) {
                fmt::format_to(inserter, "}}");
                pushNewline(inserter);
                return;
            }

            pushNewline(inserter);
            for (auto const& stmt : function.body) {
                writeStatement(inserter, stmt);
            }

            fmt::format_to(inserter, "}}");
            pushNewline(inserter);
        }

        std::string generateModule(ir::StageModule const& module, ir::ShaderProgram const& program) {
            fmt::memory_buffer out;
            auto write = [&](std::string_view str) {
                fmt::format_to(std::back_inserter(out), "{}", str);
            };

            if (m_target.isVulkan) {
                fmt::format_to(std::back_inserter(out), "#version {}\n", m_target.version);
            } else if (m_target.isES) {
                fmt::format_to(std::back_inserter(out), "#version {} es\n", m_target.version);
            } else {
                if (m_target.version >= 150) {
                    fmt::format_to(std::back_inserter(out), "#version {} core\n", m_target.version);
                } else {
                    fmt::format_to(std::back_inserter(out), "#version {}\n", m_target.version);
                }
            }

            auto exts = this->requiredExtensions(module.usedBuiltins);
            for (auto ext : exts) {
                fmt::format_to(std::back_inserter(out), "#extension {} : require\n", ext);
            }
            if (!exts.empty()) {
                this->pushNewline(std::back_inserter(out));
            }

            if (m_target.needsPrecisionQualifiers()) {
                write("precision highp float;");
                pushNewline(std::back_inserter(out));
                write("precision highp int;");
                pushNewline(std::back_inserter(out));
                write("precision highp sampler2D;");
                pushNewline(std::back_inserter(out));
                pushNewline(std::back_inserter(out));
            } else {
                pushNewline(std::back_inserter(out));
            }

            for (auto const& s : program.structs) {
                fmt::format_to(std::back_inserter(out), "struct {} {{", s.name);
                pushNewline(std::back_inserter(out));
                for (auto const& f : s.fields) {
                    fmt::format_to(std::back_inserter(out), "    {:c} {}{};", f.type.elementBase(), f.name, f.type.arraySuffix());
                    pushNewline(std::back_inserter(out));
                }
                write("};");
                pushNewline(std::back_inserter(out));
                pushNewline(std::back_inserter(out));
            }

            for (auto const& c : program.constants) {
                fmt::format_to(std::back_inserter(out), "const {:c} {}{} = ", c.type.elementBase(), c.name, c.type.arraySuffix());
                writeExpression(std::back_inserter(out), c.value);
                write(";");
                pushNewline(std::back_inserter(out));
            }

            if (!program.constants.empty()) {
                pushNewline(std::back_inserter(out));
            }

            for (auto const& [i, input] : std::ranges::views::enumerate(module.inputs)) {
                writeGlobalDecl(out, input, true,  static_cast<uint32_t>(i), module.stage);
            }

            if (!module.inputs.empty()) {
                pushNewline(std::back_inserter(out));
            }

            uint32_t outIdx = 0;
            for (auto const& output : module.outputs) {
                if (output.semantic == Semantic::Position) continue;
                if (output.semantic == Semantic::FragColor && m_target.usesFragColorOutput()) continue;
                writeGlobalDecl(out, output, false, outIdx, module.stage);
                ++outIdx;
            }

            if (!module.outputs.empty()) {
                pushNewline(std::back_inserter(out));
            }

            writeUniforms(out, module.uniforms, module.stage);

            for (auto const& func : module.functions) {
                writeFunction(std::back_inserter(out), func);
            }

            auto str = fmt::to_string(out);
            while (str.ends_with('\n')) str.pop_back();
            return str;
        }

        void writeGlobalDecl(fmt::memory_buffer& out, ir::GlobalVar const& v, bool isInput, uint32_t index, Stage stage) {
            std::string_view kw;
            if (m_target.usesInOutQualifiers()) {
                kw = isInput ? "in" : "out";
            } else {
                bool isVertexAttr = (stage == Stage::Vertex) && isInput;
                kw = isVertexAttr ? "attribute" : "varying";
            }

            bool emitLocation = m_target.usesLayoutLocation();
            if (m_target.isES) {
                bool isVertexAttrIn   = (stage == Stage::Vertex)   && isInput;
                bool isFragColorOut   = (stage == Stage::Fragment) && !isInput;
                emitLocation = isVertexAttrIn || isFragColorOut;
            }

            if (emitLocation) {
                fmt::format_to(std::back_inserter(out), "layout (location = {}) {} {:c} {}{};", index, kw, v.type.elementBase(), v.name, v.type.arraySuffix());
            } else {
                fmt::format_to(std::back_inserter(out), "{} {:c} {}{};", kw, v.type.elementBase(), v.name, v.type.arraySuffix());
            }

            pushNewline(std::back_inserter(out));
        }

        uint32_t getNextBinding(std::string_view name) {
            auto it = m_bindingAssignments.find(name);
            if (it != m_bindingAssignments.end()) {
                return it->second;
            }
            uint32_t binding = m_nextBinding++;
            m_bindingAssignments.insert_or_assign(std::string(name), binding);
            return binding;
        }

        void writeUniforms(fmt::memory_buffer& out, std::vector<ir::GlobalVar> const& uniforms, Stage stage) {
            if (uniforms.empty()) return;

            if (!m_target.isVulkan) {
                for (auto const& uniform : uniforms) {
                    fmt::format_to(std::back_inserter(out), "uniform {:c} {}{};", uniform.type.elementBase(), uniform.name, uniform.type.arraySuffix());
                    pushNewline(std::back_inserter(out));
                }
                return;
            }

            std::vector<ir::GlobalVar const*> opaque, nonOpaque;
            for (auto const& u : uniforms) {
                if (u.type.kind == Type::Kind::Sampler2D) {
                    opaque.push_back(&u);
                } else {
                    nonOpaque.push_back(&u);
                }
            }

            for (auto const* u : opaque) {
                uint32_t binding = this->getNextBinding(u->name);
                fmt::format_to(std::back_inserter(out), "layout (set = 0, binding = {}) uniform {:c} {}{};", binding, u->type.elementBase(), u->name, u->type.arraySuffix());
                pushNewline(std::back_inserter(out));
            }

            if (!nonOpaque.empty()) {
                uint32_t binding = stage == Stage::Vertex ? 0 : 1;
                fmt::format_to(std::back_inserter(out), "layout (set = 0, binding = {}) uniform Uniforms {{", binding);
                pushNewline(std::back_inserter(out));
                for (auto const* u : nonOpaque) {
                    fmt::format_to(std::back_inserter(out), "    {:c} {}{};", u->type.elementBase(), u->name, u->type.arraySuffix());
                    pushNewline(std::back_inserter(out));
                }
                fmt::format_to(std::back_inserter(out), "}};");
                pushNewline(std::back_inserter(out));
            }
        }

    private:
        GLSLTarget m_target;
        CodeStyle m_style;
        util::StringMap<std::string> m_semanticNames;
        uint32_t m_nextBinding = 0;
        util::StringMap<uint32_t> m_bindingAssignments;
    };
}