#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tlsf::internal {

enum class Tok : std::uint8_t {
    Ident,
    Number,
    String,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,
    Semicolon,
    Colon,
    Comma,
    DotDot,
    Not,
    And,
    Or,
    Implies,
    Iff,
    AndAnd,
    OrOr,
    At,
    Prime,
    Unknown,
    End,
};

struct Token {
    Tok m_type = Tok::End;
    std::string m_text;
};

/// Splits TLSF source text into tokens. The result always ends with a single
/// Tok::End token, which the parser relies on as a sentinel to clamp on rather
/// than running off the end of the vector.
///
/// @throws std::invalid_argument on an unterminated string literal.
std::vector<Token> tokenize(const std::string& text);

}  // namespace tlsf::internal
