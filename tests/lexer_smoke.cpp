// Standalone smoke test — compiles directly with g++ (no cmake/test
// framework needed yet). Prints the token stream for a few sample programs
// so we can eyeball correctness while the project is scaffolded.
#include "../include/Lexer.h"

#include <iostream>

const char* kindName(TokenKind k) {
    switch (k) {
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::IntLiteral: return "IntLiteral";
        case TokenKind::KwLet:      return "KwLet";
        case TokenKind::KwRec:      return "KwRec";
        case TokenKind::KwIn:       return "KwIn";
        case TokenKind::KwTrue:     return "KwTrue";
        case TokenKind::KwFalse:    return "KwFalse";
        case TokenKind::KwIf:       return "KwIf";
        case TokenKind::KwThen:     return "KwThen";
        case TokenKind::KwElse:     return "KwElse";
        case TokenKind::Lambda:     return "Lambda";
        case TokenKind::Dot:        return "Dot";
        case TokenKind::Equals:     return "Equals";
        case TokenKind::LParen:     return "LParen";
        case TokenKind::RParen:     return "RParen";
        case TokenKind::Plus:       return "Plus";
        case TokenKind::Minus:      return "Minus";
        case TokenKind::Star:       return "Star";
        case TokenKind::Slash:      return "Slash";
        case TokenKind::EqEq:       return "EqEq";
        case TokenKind::Lt:         return "Lt";
        case TokenKind::Gt:         return "Gt";
        case TokenKind::EndOfFile:  return "EndOfFile";
    }
    return "?";
}

void run(std::string_view label, std::string_view src) {
    std::cout << "--- " << label << " ---\n";
    std::cout << "source: " << src << "\n";
    Lexer lexer(src);
    try {
        auto tokens = lexer.tokenize();
        for (auto& t : tokens) {
            std::cout << "  " << kindName(t.kind);
            if (!t.text.empty()) std::cout << " '" << t.text << "'";
            std::cout << "  (line " << t.span.line << ", col " << t.span.col
                       << ")\n";
        }
    } catch (const LexError& e) {
        std::cout << "  LexError: " << e.what() << " at line "
                   << e.span().line << ", col " << e.span().col << "\n";
    }
    std::cout << "\n";
}

int main() {
    run("identity", "let id = \\x. x in id 42");
    run("lambda symbol + comment", "λx. x + 1 -- identity-ish\n");
    run("comparisons", "let f = \\x. x == true in f 42");
    run("bad char", "let x = @ in x");
    return 0;
}
