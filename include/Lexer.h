#pragma once
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

/// A source location range, used to point at tokens/expressions in error
/// messages (e.g. "unbound variable at line 3, column 12").
struct Span {
    uint32_t startOffset;
    uint32_t endOffset;
    uint32_t line;
    uint32_t col;
};

enum class TokenKind {
    // variable-length lexemes
    Identifier,
    IntLiteral,

    // keywords (the only reserved words in the language)
    KwLet,
    KwRec,
    KwIn,
    KwTrue,
    KwFalse,
    KwIf,
    KwThen,
    KwElse,

    // symbols
    Lambda, // '\' or 'λ'
    Dot,    // '.'
    Equals, // '='
    LParen,
    RParen,
    Plus,
    Minus,
    Star,
    Slash,
    EqEq,
    Lt,
    Gt,

    EndOfFile,
};

struct Token {
    TokenKind kind;
    std::string_view text; // raw slice of the source; only meaningful for
                            // Identifier / IntLiteral
    Span span;
};

/// Thrown for lexical errors: unrecognized characters, malformed numbers, etc.
class LexError : public std::exception {
public:
    LexError(std::string msg, Span span) : msg_(std::move(msg)), span_(span) {}
    const char* what() const noexcept override { return msg_.c_str(); }
    Span span() const noexcept { return span_; }

private:
    std::string msg_;
    Span span_;
};

/// Converts source text into a flat token stream. One-shot: construct with
/// the full source, call tokenize() once.
class Lexer {
public:
    explicit Lexer(std::string_view source) : src_(source) {}

    /// Scans the entire source and returns all tokens, including a trailing
    /// EndOfFile sentinel. Throws LexError on invalid input.
    [[nodiscard]] std::vector<Token> tokenize();

private:
    std::string_view src_;
    size_t pos_ = 0;
    uint32_t line_ = 1;
    uint32_t col_ = 1;

    bool atEnd() const noexcept { return pos_ >= src_.size(); }
    char peek() const noexcept { return atEnd() ? '\0' : src_[pos_]; }
    char peekAt(size_t offset) const noexcept {
        return pos_ + offset < src_.size() ? src_[pos_ + offset] : '\0';
    }
    char advance() noexcept;

    void skipWhitespaceAndComments();
    Token lexIdentifierOrKeyword();
    Token lexNumber();
    Token lexSymbol();

    [[noreturn]] void error(std::string msg, uint32_t startLine,
                             uint32_t startCol, size_t startPos) const;
};
