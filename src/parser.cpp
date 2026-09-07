#include <fsher/ast/parser.hpp>

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
#include <fsher/ast/nodes/global_decl.hpp>
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

#include <charconv>

namespace fsher {
    using geode::Err, geode::Ok;

    Parser::Parser(Lexer& lexer) noexcept
        : m_lexer(lexer) { (void) advance(); }

    geode::Result<void, ParsingError> Parser::advance() noexcept {
        m_previous = m_current;

        auto res = m_lexer.next();
        if (res.isErr()) {
            return Err(ParsingError{
                .kind = ParsingError::Kind::LexingError,
                .lexingError = res.unwrapErr(),
                .token = m_current
            });
        }

        m_current = res.unwrap();
        return Ok();
    }

    bool Parser::check(TokenType type) const noexcept {
        return m_current.type == type;
    }

    bool Parser::match(TokenType type) noexcept {
        if (check(type)) {
            (void) advance();
            return true;
        }
        return false;
    }

    geode::Result<Token, ParsingError> Parser::expect(TokenType type, std::string_view message) noexcept {
        if (check(type)) {
            Token matched = m_current;
            GEODE_UNWRAP(this->advance());
            return Ok(matched);
        }

        return this->error(message);
    }

    geode::impl::ErrContainer<ParsingError> Parser::error(std::string_view message) const noexcept {
        return Err(ParsingError{
            .message = message,
            .kind = ParsingError::Kind::UnexpectedToken,
            .token = m_current
        });
    }

    Parser::Result<ProgramNode> Parser::parseProgram() {
        auto start = m_current.location;
        std::vector<std::unique_ptr<ItemNode>> items;

        while (!this->check(TokenType::END_OF_FILE)) {
            GEODE_UNWRAP_INTO(auto item, this->parseItem());
            items.push_back(std::move(item));
        }

        auto end = m_current.location;
        return Ok(std::make_unique<ProgramNode>(
            SourceRange{start, end},
            std::move(items)
        ));
    }

    Parser::Result<ItemNode> Parser::parseItem() {
        GEODE_UNWRAP_INTO(auto attrs, this->parseAttributes());

        if (check(TokenType::IN) || check(TokenType::OUT) || check(TokenType::UNIFORM)) {
            GEODE_UNWRAP_INTO(auto decl, this->parseGlobalDecl(std::move(attrs)));
            return Ok(std::move(decl));
        }

        if (check(TokenType::CONST)) {
            GEODE_UNWRAP_INTO(auto decl, this->parseConstDecl(std::move(attrs)));
            return Ok(std::move(decl));
        }

        if (check(TokenType::STRUCT)) {
            GEODE_UNWRAP_INTO(auto decl, this->parseStructDecl(std::move(attrs)));
            return Ok(std::move(decl));
        }

        if (check(TokenType::FN)) {
            GEODE_UNWRAP_INTO(auto decl, this->parseFnDecl(std::move(attrs)));
            return Ok(std::move(decl));
        }

        return this->error("Expected global declaration");
    }

    geode::Result<std::vector<Attribute>, ParsingError> Parser::parseAttributes() {
        std::vector<Attribute> attrs;

        if (!this->match(TokenType::HASH_BRACKET)) {
            return Ok(std::move(attrs));
        }

        do {
            GEODE_UNWRAP_INTO(auto token, this->expect(TokenType::IDENTIFIER, "Expected identifier in attribute"));
            attrs.push_back(Attribute{token.lexeme});
        } while (this->match(TokenType::COMMA));

        if (!this->match(TokenType::RBRACKET)) {
            return this->error("Expected closing bracket");
        }

        return Ok(std::move(attrs));
    }

    Parser::Result<GlobalDeclNode> Parser::parseGlobalDecl(std::vector<Attribute> attrs) {
        auto start = m_current.location;

        DeclKind declKind;
        if (this->match(TokenType::IN)) {
            declKind = DeclKind::In;
        } else if (this->match(TokenType::OUT)) {
            declKind = DeclKind::Out;
        } else if (this->match(TokenType::UNIFORM)) {
            declKind = DeclKind::Uniform;
        } else {
            return this->error("Expected 'in', 'out', or 'uniform' for global declaration");
        }

        GlobalDeclNode::BindTarget target;
        SourceLocation end;

        if (match(TokenType::LBRACE)) {
            GEODE_UNWRAP_INTO(auto fields, this->parseStructFields());
            GEODE_UNWRAP_INTO(auto brace, this->expect(TokenType::RBRACE, "Expected '}' after struct fields"));

            end = brace.location;
            end.column += 1;

            target = GlobalDeclNode::Group{std::move(fields)};
        } else {
            GEODE_UNWRAP_INTO(auto name, this->expect(TokenType::IDENTIFIER, "Expected identifier"));
            GEODE_UNWRAP(this->expect(TokenType::COLON, "Expected ':' after identifier"));
            GEODE_UNWRAP_INTO(auto type, this->parseType());
            GEODE_UNWRAP_INTO(auto semicolon, this->expect(TokenType::SEMICOLON, "Expected ';' after"));

            end = semicolon.location;
            target = GlobalDeclNode::Single{name.lexeme, std::move(type)};
        }

        return Ok(std::make_unique<GlobalDeclNode>(
            SourceRange{start, end},
            std::move(attrs),
            declKind,
            std::move(target)
        ));
    }

    Parser::Result<ConstDeclNode> Parser::parseConstDecl(std::vector<Attribute> attrs) {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::CONST, "Expected 'const'"));
        GEODE_UNWRAP_INTO(auto name, this->expect(TokenType::IDENTIFIER, "Expected identifier"));

        TypeName typeName;
        if (this->match(TokenType::COLON)) {
            GEODE_UNWRAP_INTO(typeName, this->parseType());
        }

        GEODE_UNWRAP(this->expect(TokenType::EQUAL, "Expected '='"));

        GEODE_UNWRAP_INTO(auto value, this->parseExpr());
        auto end = m_current.location;
        GEODE_UNWRAP(this->expect(TokenType::SEMICOLON, "Expected ';'"));

        return Ok(std::make_unique<ConstDeclNode>(
            SourceRange{start, end},
            std::move(attrs),
            name, std::move(typeName),
            std::move(value)
        ));
    }

    Parser::Result<StructDeclNode> Parser::parseStructDecl(std::vector<Attribute> attrs) {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::STRUCT, "Expected 'struct'"));
        GEODE_UNWRAP_INTO(auto name, this->expect(TokenType::IDENTIFIER, "Expected identifier"));
        GEODE_UNWRAP(this->expect(TokenType::LBRACE, "Expected '{' after 'struct'"));
        GEODE_UNWRAP_INTO(auto fields, this->parseStructFields());
        GEODE_UNWRAP(this->expect(TokenType::RBRACE, "Expected '}' after struct fields"));

        auto end = m_current.location;

        return Ok(std::make_unique<StructDeclNode>(
            SourceRange{start, end},
            std::move(attrs),
            name.lexeme,
            std::move(fields)
        ));
    }

    Parser::Result<FnDeclNode> Parser::parseFnDecl(std::vector<Attribute> attrs) {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::FN, "Expected 'fn'"));
        GEODE_UNWRAP_INTO(auto name, this->expect(TokenType::IDENTIFIER, "Expected identifier"));
        GEODE_UNWRAP(this->expect(TokenType::LPAREN, "Expected '(' after function name"));

        std::vector<StructField> parameters;
        if (!this->check(TokenType::RPAREN)) {
            GEODE_UNWRAP_INTO(parameters, this->parseStructFields());
        }

        GEODE_UNWRAP_INTO(auto rparen, this->expect(TokenType::RPAREN, "Expected ')' after function parameters"));

        auto argsEndPos = rparen.location;
        argsEndPos.column += 1;

        TypeName returnType;
        if (this->match(TokenType::ARROW)) {
            GEODE_UNWRAP_INTO(returnType, this->parseType());
        }

        std::unique_ptr<ExprNode> body;
        SourceLocation end;
        if (this->check(TokenType::LBRACE)) {
            GEODE_UNWRAP_INTO(body, this->parseBlock());
            end = m_current.location;
        } else if (this->match(TokenType::FAT_ARROW)) {
            GEODE_UNWRAP_INTO(body, this->parseExpr());
            GEODE_UNWRAP(this->expect(TokenType::SEMICOLON, "Expected ';' after function body"));
            end = m_current.location;
        } else {
            return this->error("Expected '{' or '=>' for function body");
        }

        return Ok(std::make_unique<FnDeclNode>(
            SourceRange{start, end},
            std::move(attrs),
            name.lexeme,
            std::move(returnType),
            std::move(body),
            std::move(parameters),
            argsEndPos
        ));
    }

    enum class Precedence : uint8_t {
        None = 0,
        Assignment,     // = += -= ...
        Ternary,        // ? :
        Pipeline,       // |>
        LogicOr,        // ||
        LogicAnd,       // &&
        BitOr,          // |
        BitXor,         // ^
        BitAnd,         // &
        Equality,       // == !=
        Comparison,     // < > <= >=
        Shift,          // << >>
        Additive,       // + -
        Multiplicative, // * / %
        Unary,          // ! - ~
        Postfix,        // .index, .field, ()
    };

    static Precedence getPrecedence(TokenType type) noexcept {
        switch (type) {
            case TokenType::EQUAL:
            case TokenType::PLUS_EQUAL:
            case TokenType::MINUS_EQUAL:
            case TokenType::ASTERISK_EQUAL:
            case TokenType::SLASH_EQUAL:
            case TokenType::PERCENT_EQUAL:
            case TokenType::CARET_EQUAL:
            case TokenType::AMPERSAND_EQUAL:
            case TokenType::PIPE_EQUAL:
            case TokenType::LEFT_SHIFT_EQUAL:
            case TokenType::RIGHT_SHIFT_EQUAL:
                return Precedence::Assignment;

            case TokenType::QUESTION:
                return Precedence::Ternary;

            case TokenType::PIPE_ARROW:
                return Precedence::Pipeline;

            case TokenType::PIPE_PIPE:
                return Precedence::LogicOr;

            case TokenType::AMPERSAND_AMPERSAND:
                return Precedence::LogicAnd;

            case TokenType::PIPE:
                return Precedence::BitOr;

            case TokenType::CARET:
                return Precedence::BitXor;

            case TokenType::AMPERSAND:
                return Precedence::BitAnd;

            case TokenType::EQUAL_EQUAL:
            case TokenType::BANG_EQUAL:
                return Precedence::Equality;

            case TokenType::LESS:
            case TokenType::GREATER:
            case TokenType::LESS_EQUAL:
            case TokenType::GREATER_EQUAL:
                return Precedence::Comparison;

            case TokenType::LEFT_SHIFT:
            case TokenType::RIGHT_SHIFT:
                return Precedence::Shift;

            case TokenType::PLUS:
            case TokenType::MINUS:
                return Precedence::Additive;

            case TokenType::ASTERISK:
            case TokenType::SLASH:
            case TokenType::PERCENT:
                return Precedence::Multiplicative;

            case TokenType::LPAREN:
            case TokenType::LBRACKET:
            case TokenType::DOT:
                return Precedence::Postfix;

            default:
                return Precedence::None;
        }
    }

    static bool isRightAssociative(TokenType type) noexcept {
        switch (type) {
            case TokenType::EQUAL:
            case TokenType::PLUS_EQUAL:
            case TokenType::MINUS_EQUAL:
            case TokenType::ASTERISK_EQUAL:
            case TokenType::SLASH_EQUAL:
            case TokenType::PERCENT_EQUAL:
            case TokenType::CARET_EQUAL:
            case TokenType::AMPERSAND_EQUAL:
            case TokenType::PIPE_EQUAL:
            case TokenType::LEFT_SHIFT_EQUAL:
            case TokenType::RIGHT_SHIFT_EQUAL:
                return true;
            default:
                return false;
        }
    }

    Parser::Result<BlockNode> Parser::parseBlock() {
        auto start = m_current.location;

        std::vector<std::unique_ptr<StmtNode>> statements;
        std::unique_ptr<ExprNode> trailingExpr = nullptr;

        GEODE_UNWRAP(this->expect(TokenType::LBRACE, "Expected '{' to start block"));

        while (!this->check(TokenType::RBRACE) && !this->check(TokenType::END_OF_FILE)) {
            if (this->check(TokenType::LET)) {
                GEODE_UNWRAP_INTO(auto stmt, this->parseLetStmt());
                statements.push_back(std::move(stmt));
            } else if (this->check(TokenType::CONST)) {
                GEODE_UNWRAP_INTO(auto stmt, this->parseConstStmt());
                statements.push_back(std::move(stmt));
            } else if (this->check(TokenType::FOR)) {
                GEODE_UNWRAP_INTO(auto stmt, this->parseForStmt());
                statements.push_back(std::move(stmt));
            } else if (this->check(TokenType::RETURN)) {
                GEODE_UNWRAP_INTO(auto stmt, this->parseReturnStmt());
                statements.push_back(std::move(stmt));
            } else if (this->check(TokenType::LBRACE) || this->check(TokenType::IF) || this->check(TokenType::MATCH)) {
                GEODE_UNWRAP_INTO(auto expr, this->parseExpr());
                if (this->match(TokenType::SEMICOLON)) {
                    statements.push_back(std::make_unique<ExprStmtNode>(std::move(expr)));
                } else if (this->check(TokenType::RBRACE)) {
                    trailingExpr = std::move(expr);
                    break;
                } else {
                    statements.push_back(std::make_unique<ExprStmtNode>(std::move(expr)));
                }
            } else {
                GEODE_UNWRAP_INTO(auto expr, this->parseExpr(static_cast<int>(Precedence::Ternary) + 1));
                if (this->check(TokenType::EQUAL) ||
                    this->check(TokenType::PLUS_EQUAL) ||
                    this->check(TokenType::MINUS_EQUAL) ||
                    this->check(TokenType::ASTERISK_EQUAL) ||
                    this->check(TokenType::SLASH_EQUAL) ||
                    this->check(TokenType::PERCENT_EQUAL) ||
                    this->check(TokenType::CARET_EQUAL) ||
                    this->check(TokenType::AMPERSAND_EQUAL) ||
                    this->check(TokenType::PIPE_EQUAL) ||
                    this->check(TokenType::LEFT_SHIFT_EQUAL) ||
                    this->check(TokenType::RIGHT_SHIFT_EQUAL)) {

                    Token opToken = m_current;
                    (void) advance();
                    GEODE_UNWRAP_INTO(auto value, this->parseExpr());
                    GEODE_UNWRAP_INTO(auto semi, expect(TokenType::SEMICOLON, "Expected ';' after assignment"));

                    auto end = semi.location;
                    end.column += 1;

                    statements.push_back(std::make_unique<AssignStmtNode>(
                        SourceRange{expr->range().start, end},
                        std::move(expr),
                        opToken.type,
                        std::move(value)
                    ));
                } else if (this->match(TokenType::SEMICOLON)) {
                    statements.push_back(std::make_unique<ExprStmtNode>(std::move(expr)));
                } else if (this->check(TokenType::RBRACE)) {
                    trailingExpr = std::move(expr);
                    break;
                } else {
                    return this->error("Expected ';' after expression");
                }
            }
        }

        GEODE_UNWRAP_INTO(auto brace, this->expect(TokenType::RBRACE, "Expected '}' to end block"));

        auto end = brace.location;
        end.column += 1;

        return Ok(std::make_unique<BlockNode>(
            SourceRange{start, end},
            std::move(statements),
            std::move(trailingExpr)
        ));
    }

    Parser::Result<LetStmtNode> Parser::parseLetStmt() {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::LET, "Expected 'let'"));
        GEODE_UNWRAP_INTO(auto nameToken, this->expect(TokenType::IDENTIFIER, "Expected identifier after 'let'"));

        TypeName typeName;
        if (this->match(TokenType::COLON)) {
            GEODE_UNWRAP_INTO(typeName, this->parseType());
        }

        GEODE_UNWRAP(this->expect(TokenType::EQUAL, "Expected '=' in 'let' statement"));
        GEODE_UNWRAP_INTO(auto value, this->parseExpr());
        GEODE_UNWRAP_INTO(auto semi, this->expect(TokenType::SEMICOLON, "Expected ';' after 'let' statement"));

        auto end = semi.location;
        end.column += 1;

        return Ok(std::make_unique<LetStmtNode>(
            SourceRange{start, end},
            nameToken,
            std::move(typeName),
            std::move(value)
        ));
    }

    Parser::Result<ConstStmtNode> Parser::parseConstStmt() {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::CONST, "Expected 'const'"));
        GEODE_UNWRAP_INTO(auto nameToken, this->expect(TokenType::IDENTIFIER, "Expected identifier after 'const'"));

        TypeName typeName;
        if (this->match(TokenType::COLON)) {
            GEODE_UNWRAP_INTO(typeName, this->parseType());
        }

        GEODE_UNWRAP(this->expect(TokenType::EQUAL, "Expected '=' in 'const' statement"));
        GEODE_UNWRAP_INTO(auto value, this->parseExpr());
        GEODE_UNWRAP_INTO(auto semi, this->expect(TokenType::SEMICOLON, "Expected ';' after 'const' statement"));

        auto end = semi.location;
        end.column += 1;

        return Ok(std::make_unique<ConstStmtNode>(
            SourceRange{start, end},
            nameToken,
            std::move(typeName),
            std::move(value)
        ));
    }

    Parser::Result<ForStmtNode> Parser::parseForStmt() {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::FOR, "Expected 'for'"));
        GEODE_UNWRAP_INTO(auto varToken, this->expect(TokenType::IDENTIFIER, "Expected loop variable identifier after 'for'"));
        GEODE_UNWRAP(this->expect(TokenType::IN, "Expected 'in' after loop variable"));

        GEODE_UNWRAP_INTO(auto startExpr, this->parseExpr(static_cast<int>(Precedence::Additive), false));

        bool inclusive = false;
        if (this->match(TokenType::DOT_DOT_EQUAL)) {
            inclusive = true;
        } else if (this->match(TokenType::DOT_DOT)) {
            inclusive = false;
        } else {
            return this->error("Expected '..' or '..=' in range expression");
        }

        GEODE_UNWRAP_INTO(auto endExpr, this->parseExpr(static_cast<int>(Precedence::Additive), false));

        GEODE_UNWRAP_INTO(auto body, this->parseBlock());

        auto end = body->range().end;

        return Ok(std::make_unique<ForStmtNode>(
            SourceRange{start, end},
            varToken.lexeme,
            std::move(startExpr),
            std::move(endExpr),
            inclusive,
            std::move(body)
        ));
    }

    Parser::Result<ReturnStmtNode> Parser::parseReturnStmt() {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::RETURN, "Expected 'return'"));

        std::unique_ptr<ExprNode> value = nullptr;
        if (!this->check(TokenType::SEMICOLON)) {
            GEODE_UNWRAP_INTO(value, this->parseExpr());
        }

        GEODE_UNWRAP_INTO(auto semi, this->expect(TokenType::SEMICOLON, "Expected ';' after 'return' statement"));

        auto end = semi.location;
        end.column += 1;

        return Ok(std::make_unique<ReturnStmtNode>(
            SourceRange{start, end},
            std::move(value)
        ));
    }

    Parser::Result<ExprNode> Parser::parseExpr(int precedence, bool allowStructLiterals) {
        GEODE_UNWRAP_INTO(auto left, this->parsePrefixOrPrimary(allowStructLiterals));

        while (true) {
            TokenType opType = m_current.type;
            Precedence opPrec = getPrecedence(opType);

            if (static_cast<int>(opPrec) < precedence || opPrec == Precedence::None) {
                break;
            }

            Token opToken = m_current;
            GEODE_UNWRAP(this->advance());

            if (opType == TokenType::LPAREN) {
                std::vector<std::unique_ptr<ExprNode>> args;
                if (!check(TokenType::RPAREN)) {
                    do {
                        GEODE_UNWRAP_INTO(auto arg, this->parseExpr(0, true));
                        args.push_back(std::move(arg));
                    } while (match(TokenType::COMMA));
                }
                GEODE_UNWRAP_INTO(auto paren, expect(TokenType::RPAREN, "Expected ')' after arguments"));
                auto end = paren.location;
                end.column += 1;

                left = std::make_unique<CallExprNode>(
                    SourceRange{left->range().start, end},
                    std::move(left),
                    std::move(args)
                );
            } else if (opType == TokenType::LBRACKET) {
                GEODE_UNWRAP_INTO(auto index, this->parseExpr(0, true));
                GEODE_UNWRAP(expect(TokenType::RBRACKET, "Expected ']' after index"));

                left = std::make_unique<IndexExprNode>(
                    SourceRange{left->range().start, m_previous.location},
                    std::move(left),
                    std::move(index)
                );
            } else if (opType == TokenType::DOT) {
                GEODE_UNWRAP_INTO(auto field, expect(TokenType::IDENTIFIER, "Expected field name after '.'"));

                SourceLocation fieldEnd = field.location;
                fieldEnd.column += field.lexeme.size();

                left = std::make_unique<FieldExprNode>(
                    SourceRange{left->range().start, fieldEnd},
                    std::move(left),
                    field.lexeme
                );
            } else if (opType == TokenType::QUESTION) {
                GEODE_UNWRAP_INTO(auto thenExpr, this->parseExpr(0, true));
                GEODE_UNWRAP(expect(TokenType::COLON, "Expected ':' in ternary expression"));
                GEODE_UNWRAP_INTO(auto elseExpr, this->parseExpr(static_cast<int>(opPrec), allowStructLiterals));

                left = std::make_unique<TernaryExprNode>(
                    SourceRange{left->range().start, elseExpr->range().end},
                    std::move(left),
                    std::move(thenExpr),
                    std::move(elseExpr)
                );
            } else if (opType == TokenType::PIPE_ARROW) {
                GEODE_UNWRAP_INTO(auto targetIdent, expect(TokenType::IDENTIFIER, "Expected identifier after '|>'"));

                std::vector<std::unique_ptr<ExprNode>> args;
                args.push_back(std::move(left));

                SourceLocation callEnd = targetIdent.location;
                if (match(TokenType::LPAREN)) {
                    if (!check(TokenType::RPAREN)) {
                        do {
                            GEODE_UNWRAP_INTO(auto arg, this->parseExpr(0, true));
                            args.push_back(std::move(arg));
                        } while (match(TokenType::COMMA));
                    }
                    GEODE_UNWRAP_INTO(auto paren, expect(TokenType::RPAREN, "Expected ')' after pipeline arguments"));
                    callEnd = paren.location;
                    callEnd.column += 1;
                }

                auto callee = std::make_unique<IdentifierExprNode>(
                    SourceRange{targetIdent.location, SourceLocation{targetIdent.location.line, targetIdent.location.column + targetIdent.lexeme.size()}},
                    targetIdent.lexeme
                );

                left = std::make_unique<CallExprNode>(
                    SourceRange{args[0]->range().start, callEnd},
                    std::move(callee),
                    std::move(args)
                );
            } else {
                int nextPrecedence = static_cast<int>(opPrec);
                if (!isRightAssociative(opType)) {
                    nextPrecedence += 1;
                }

                GEODE_UNWRAP_INTO(auto right, this->parseExpr(nextPrecedence, allowStructLiterals));

                left = std::make_unique<BinaryExprNode>(
                    SourceRange{left->range().start, right->range().end},
                    opToken.type,
                    std::move(left),
                    std::move(right)
                );
            }
        }

        return Ok(std::move(left));
    }

    Parser::Result<ExprNode> Parser::parsePrefixOrPrimary(bool allowStructLiterals) {
        auto start = m_current.location;

        if (match(TokenType::MINUS) || match(TokenType::BANG) || match(TokenType::TILDE)) {
            Token op = m_previous;
            GEODE_UNWRAP_INTO(auto operand, this->parseExpr(static_cast<int>(Precedence::Unary), allowStructLiterals));

            return Ok(std::make_unique<UnaryExprNode>(
                SourceRange{start, operand->range().end},
                op.type,
                std::move(operand)
            ));
        }

        if (match(TokenType::LPAREN)) {
            GEODE_UNWRAP_INTO(auto expr, this->parseExpr(0, true));
            GEODE_UNWRAP(expect(TokenType::RPAREN, "Expected ')' after expression"));
            return Ok(std::move(expr));
        }

        if (check(TokenType::IF)) {
            GEODE_UNWRAP_INTO(auto ifExpr, this->parseIfExpr());
            return Ok(std::move(ifExpr));
        }
        if (check(TokenType::MATCH)) {
            GEODE_UNWRAP_INTO(auto matchExpr, this->parseMatchExpr());
            return Ok(std::move(matchExpr));
        }

        if (match(TokenType::IDENTIFIER)) {
            Token idToken = m_previous;

            if (allowStructLiterals && check(TokenType::LBRACE)) {
                (void) advance();
                std::vector<FieldInit> fields;
                if (!check(TokenType::RBRACE)) {
                    do {
                        GEODE_UNWRAP_INTO(auto name, expect(TokenType::IDENTIFIER, "Expected field name"));
                        GEODE_UNWRAP(expect(TokenType::COLON, "Expected ':' after field name"));
                        GEODE_UNWRAP_INTO(auto val, this->parseExpr(0, true));
                        fields.push_back({name.lexeme, std::move(val)});
                    } while (match(TokenType::COMMA));
                }
                GEODE_UNWRAP(expect(TokenType::RBRACE, "Expected '}' after struct initializer"));

                return Ok(std::make_unique<StructLiteralExprNode>(
                    SourceRange{start, m_current.location},
                    idToken.lexeme,
                    std::move(fields)
                ));
            }

            SourceLocation endLoc = idToken.location;
            endLoc.column += idToken.lexeme.size();
            return Ok(std::make_unique<IdentifierExprNode>(SourceRange{start, endLoc}, idToken.lexeme));
        }

        if (match(TokenType::INTEGER) || match(TokenType::FLOAT) || match(TokenType::TRUE) || match(TokenType::FALSE)) {
            GEODE_UNWRAP_INTO(auto value, LiteralExprNode::parseLiteralValue(m_previous));
            return Ok(std::make_unique<LiteralExprNode>(SourceRange{start, m_current.location}, value));
        }

        return this->error("Expected expression");
    }

    Parser::Result<IfExprNode> Parser::parseIfExpr() {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::IF, "Expected 'if' for if expression"));
        GEODE_UNWRAP_INTO(auto condition, this->parseExpr(0, false));
        GEODE_UNWRAP_INTO(auto thenBlock, this->parseBlock());

        IfExprNode::ElseBranch elseBranch = std::monostate{};
        if (this->match(TokenType::ELSE)) {
            if (this->check(TokenType::IF)) {
                GEODE_UNWRAP_INTO(elseBranch, this->parseIfExpr());
            } else {
                GEODE_UNWRAP_INTO(elseBranch, this->parseBlock());
            }
        }

        auto end = m_current.location;

        return Ok(std::make_unique<IfExprNode>(
            SourceRange{start, end},
            std::move(condition),
            std::move(thenBlock),
            std::move(elseBranch)
        ));
    }

    Parser::Result<MatchExprNode> Parser::parseMatchExpr() {
        auto start = m_current.location;

        GEODE_UNWRAP(this->expect(TokenType::MATCH, "Expected 'match' for match expression"));
        GEODE_UNWRAP_INTO(auto scrutinee, this->parseExpr(0, false));
        GEODE_UNWRAP(this->expect(TokenType::LBRACE, "Expected '{' after 'match'"));

        std::vector<MatchArm> arms;
        while (!this->check(TokenType::RBRACE) && !this->check(TokenType::END_OF_FILE)) {
            if (this->check(TokenType::IDENTIFIER) && m_current.lexeme == "_") {
                GEODE_UNWRAP(this->advance());
                GEODE_UNWRAP(this->expect(TokenType::FAT_ARROW, "Expected '=>' after match pattern"));

                std::unique_ptr<ExprNode> body;
                if (this->check(TokenType::LBRACE)) {
                    GEODE_UNWRAP_INTO(body, this->parseBlock());
                } else {
                    GEODE_UNWRAP_INTO(body, this->parseExpr());
                }

                arms.push_back(MatchArm{
                    Pattern{
                        Pattern::Kind::Wildcard,
                        nullptr
                    },
                    std::move(body)
                });
            } else {
                GEODE_UNWRAP_INTO(auto patternExpr, this->parseExpr());
                GEODE_UNWRAP(this->expect(TokenType::FAT_ARROW, "Expected '=>' after match pattern"));

                std::unique_ptr<ExprNode> body;
                if (this->check(TokenType::LBRACE)) {
                    GEODE_UNWRAP_INTO(body, this->parseBlock());
                } else {
                    GEODE_UNWRAP_INTO(body, this->parseExpr());
                }

                arms.push_back(MatchArm{
                    Pattern{
                        Pattern::Kind::Expression,
                        std::move(patternExpr)
                    },
                    std::move(body)
                });
            }

            if (!this->match(TokenType::COMMA)) {
                break;
            }
        }

        GEODE_UNWRAP(this->expect(TokenType::RBRACE, "Expected '}' after match arms"));
        auto end = m_current.location;

        return Ok(std::make_unique<MatchExprNode>(
            SourceRange{start, end},
            std::move(scrutinee),
            std::move(arms)
        ));
    }

    geode::Result<std::vector<StructField>, ParsingError> Parser::parseStructFields() {
        std::vector<StructField> fields;

        while (!this->check(TokenType::RBRACE) && !this->check(TokenType::END_OF_FILE)) {
            GEODE_UNWRAP_INTO(auto attrs, this->parseAttributes());
            GEODE_UNWRAP_INTO(auto name, this->expect(TokenType::IDENTIFIER, "Expected identifier"));
            GEODE_UNWRAP(this->expect(TokenType::COLON, "Expected ':' after struct field name"));
            GEODE_UNWRAP_INTO(auto type, this->parseType());

            auto end = type.range.end;
            fields.push_back(StructField{
                .attributes = std::move(attrs),
                .name = name.lexeme,
                .type = std::move(type),
                .range = SourceRange{name.location, end}
            });

            if (!this->match(TokenType::COMMA)) {
                break;
            }
        }

        return Ok(std::move(fields));
    }

    geode::Result<TypeName, ParsingError> Parser::parseType() {
        GEODE_UNWRAP_INTO(auto nameToken, this->expect(TokenType::IDENTIFIER, "Expected type identifier"));

        TypeName type;
        type.name = nameToken.lexeme;

        auto end = nameToken.location;
        end.column += nameToken.lexeme.size();

        while (this->check(TokenType::LBRACKET)) {
            GEODE_UNWRAP(this->expect(TokenType::LBRACKET, "Expected '['"));
            GEODE_UNWRAP_INTO(auto sizeToken, this->expect(TokenType::INTEGER, "Expected array size"));

            uint64_t size = 0;
            auto [ptr, ec] = std::from_chars(sizeToken.lexeme.data(), sizeToken.lexeme.data() + sizeToken.lexeme.size(), size);
            if (ec != std::errc() || ptr != sizeToken.lexeme.data() + sizeToken.lexeme.size()) {
                return this->error("Invalid array size");
            }

            if (size == 0) {
                return this->error("Array size must be greater than 0");
            }

            GEODE_UNWRAP_INTO(auto rbracket, this->expect(TokenType::RBRACKET, "Expected ']' after array size"));
            type.arrayDims.push_back(size);

            end = rbracket.location;
            end.column += 1;
        }

        type.range = SourceRange{nameToken.location, end};
        return Ok(std::move(type));
    }
}
