#include "Lexer.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace {

struct Keyword {
    std::string_view text;
    TokenKind kind;
};

constexpr std::array<Keyword, 8> kKeywords{{
    {"let", TokenKind::KwLet},
    {"rec", TokenKind::KwRec},
    {"in", TokenKind::KwIn},
    {"true", TokenKind::KwTrue},
    {"false", TokenKind::KwFalse},
    {"if", TokenKind::KwIf},
    {"then", TokenKind::KwThen},
    {"else", TokenKind::KwElse},
}};

constexpr TokenKind lookupKeyword(std::string_view text) noexcept {
    auto it = std::ranges::find_if(
        kKeywords, [&](const Keyword& k) { return k.text == text; });
    return it != kKeywords.end() ? it->kind : TokenKind::Identifier;
}

}

char Lexer::advance() noexcept {
    char c = src_[pos_++];
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

void Lexer::error(std::string msg, uint32_t startLine, uint32_t startCol,
                   size_t startPos) const {
    Span span{.startOffset = static_cast<uint32_t>(startPos),
              .endOffset = static_cast<uint32_t>(pos_),
              .line = startLine,
              .col = startCol};
    throw LexError(std::move(msg), span);
}

void Lexer::skipWhitespaceAndComments() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (src_.substr(pos_).starts_with("--")) {

            while (!atEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::lexIdentifierOrKeyword() {
    size_t start = pos_;
    uint32_t startLine = line_, startCol = col_;
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) ||
                        peek() == '_')) {
        advance();
    }

    std::string_view text = src_.substr(start, pos_ - start);
    Span span{.startOffset = static_cast<uint32_t>(start),
              .endOffset = static_cast<uint32_t>(pos_),
              .line = startLine,
              .col = startCol};

    return Token{.kind = lookupKeyword(text), .text = text, .span = span};
}

Token Lexer::lexNumber() {
    size_t start = pos_;
    uint32_t startLine = line_, startCol = col_;
    while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }
    std::string_view text = src_.substr(start, pos_ - start);
    Span span{.startOffset = static_cast<uint32_t>(start),
              .endOffset = static_cast<uint32_t>(pos_),
              .line = startLine,
              .col = startCol};
    return Token{.kind = TokenKind::IntLiteral, .text = text, .span = span};
}

Token Lexer::lexSymbol() {
    size_t start = pos_;
    uint32_t startLine = line_, startCol = col_;
    if (src_.substr(pos_).starts_with("\xCE\xBB")) {
        pos_ += 2;

        col_ += 1;
        Span span{.startOffset = static_cast<uint32_t>(start),
                  .endOffset = static_cast<uint32_t>(pos_),
                  .line = startLine,
                  .col = startCol};
        return Token{.kind = TokenKind::Lambda,
                     .text = src_.substr(start, pos_ - start),
                     .span = span};
    }

    char c = advance();

    auto tok = [&](TokenKind kind) {
        Span span{.startOffset = static_cast<uint32_t>(start),
                  .endOffset = static_cast<uint32_t>(pos_),
                  .line = startLine,
                  .col = startCol};
        return Token{.kind = kind, .text = src_.substr(start, pos_ - start),
                     .span = span};
    };

    switch (c) {
        case '\\': return tok(TokenKind::Lambda);
        case '.':  return tok(TokenKind::Dot);
        case '(':  return tok(TokenKind::LParen);
        case ')':  return tok(TokenKind::RParen);
        case '+':  return tok(TokenKind::Plus);
        case '-':  return tok(TokenKind::Minus);
        case '*':  return tok(TokenKind::Star);
        case '/':  return tok(TokenKind::Slash);
        case '<':  return tok(TokenKind::Lt);
        case '>':  return tok(TokenKind::Gt);
        case '=':
            if (peek() == '=') {
                advance();
                return tok(TokenKind::EqEq);
            }
            return tok(TokenKind::Equals);
        default:
            error(std::string("Unexpected character '") + c + "'", startLine,
                  startCol, start);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(src_.size() / 4 + 1);

    for (;;) {
        skipWhitespaceAndComments();
        if (atEnd()) break;

        char c = peek();
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(lexNumber());
        } else {
            tokens.push_back(lexSymbol());
        }
    }
    tokens.push_back(Token{
        .kind = TokenKind::EndOfFile,
        .text = {},
        .span = Span{.startOffset = static_cast<uint32_t>(pos_),
                     .endOffset = static_cast<uint32_t>(pos_),
                     .line = line_,
                     .col = col_},
    });
    return tokens;
}
