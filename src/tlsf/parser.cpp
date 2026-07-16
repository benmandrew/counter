#include "tlsf/parser.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "prop_formula.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

namespace {

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

[[noreturn]] void reject_construct(const std::string& construct) {
    throw std::invalid_argument(
        "TLSF full-format construct not supported: " + construct +
        ". Lower to basic format first with: syfco -f basic <file>");
}

std::string to_lower(const std::string& value) {
    std::string result = value;
    for (char& character : result) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return result;
}

bool is_ident_start(char character) {
    return (std::isalpha(static_cast<unsigned char>(character)) != 0) ||
           character == '_';
}

bool is_ident_char(char character) {
    return (std::isalnum(static_cast<unsigned char>(character)) != 0) ||
           character == '_';
}

class Lexer {
   public:
    explicit Lexer(const std::string& text) : m_text(text) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            Token token = next_token();
            const bool is_end = token.m_type == Tok::End;
            tokens.push_back(std::move(token));
            if (is_end) {
                break;
            }
        }
        return tokens;
    }

   private:
    const std::string& m_text;
    std::size_t m_pos = 0;

    [[nodiscard]] char peek(std::size_t offset = 0) const {
        const std::size_t idx = m_pos + offset;
        return idx < m_text.size() ? m_text[idx] : '\0';
    }

    void skip_trivia() {
        while (m_pos < m_text.size()) {
            const char character = m_text[m_pos];
            if (std::isspace(static_cast<unsigned char>(character)) != 0) {
                ++m_pos;
                continue;
            }
            if (character == '/' && peek(1) == '/') {
                while (m_pos < m_text.size() && m_text[m_pos] != '\n') {
                    ++m_pos;
                }
                continue;
            }
            if (character == '/' && peek(1) == '*') {
                m_pos += 2;
                while (m_pos < m_text.size() &&
                       (m_text[m_pos] != '*' || peek(1) != '/')) {
                    ++m_pos;
                }
                if (m_pos < m_text.size()) {
                    m_pos += 2;
                }
                continue;
            }
            break;
        }
    }

    Token next_token() {
        skip_trivia();
        if (m_pos >= m_text.size()) {
            return {Tok::End, ""};
        }
        const char character = m_text[m_pos];
        if (is_ident_start(character)) {
            return lex_ident();
        }
        if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
            return lex_number();
        }
        if (character == '"') {
            return lex_string();
        }
        return lex_symbol();
    }

    Token lex_ident() {
        const std::size_t start = m_pos;
        while (m_pos < m_text.size() && is_ident_char(m_text[m_pos])) {
            ++m_pos;
        }
        return {Tok::Ident, m_text.substr(start, m_pos - start)};
    }

    Token lex_number() {
        const std::size_t start = m_pos;
        while (m_pos < m_text.size() &&
               std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0) {
            ++m_pos;
        }
        return {Tok::Number, m_text.substr(start, m_pos - start)};
    }

    Token lex_string() {
        ++m_pos;  // opening quote
        std::string value;
        while (m_pos < m_text.size() && m_text[m_pos] != '"') {
            if (m_text[m_pos] == '\\' && m_pos + 1 < m_text.size()) {
                value.push_back(m_text[m_pos + 1]);
                m_pos += 2;
                continue;
            }
            value.push_back(m_text[m_pos]);
            ++m_pos;
        }
        if (m_pos >= m_text.size()) {
            throw std::invalid_argument(
                "TLSF parse error: unterminated string");
        }
        ++m_pos;  // closing quote
        return {Tok::String, value};
    }

    Token lex_symbol() {
        const char character = m_text[m_pos];
        switch (character) {
            case '{':
                ++m_pos;
                return {Tok::LBrace, "{"};
            case '}':
                ++m_pos;
                return {Tok::RBrace, "}"};
            case '(':
                ++m_pos;
                return {Tok::LParen, "("};
            case ')':
                ++m_pos;
                return {Tok::RParen, ")"};
            case '[':
                ++m_pos;
                return {Tok::LBracket, "["};
            case ']':
                ++m_pos;
                return {Tok::RBracket, "]"};
            case ';':
                ++m_pos;
                return {Tok::Semicolon, ";"};
            case ':':
                ++m_pos;
                return {Tok::Colon, ":"};
            case ',':
                ++m_pos;
                return {Tok::Comma, ","};
            case '@':
                ++m_pos;
                return {Tok::At, "@"};
            case '\'':
                ++m_pos;
                return {Tok::Prime, "'"};
            case '!':
                ++m_pos;
                return {Tok::Not, "!"};
            case '.':
                if (peek(1) == '.') {
                    m_pos += 2;
                    return {Tok::DotDot, ".."};
                }
                ++m_pos;
                return {Tok::Unknown, "."};
            case '&':
                if (peek(1) == '&') {
                    m_pos += 2;
                    return {Tok::AndAnd, "&&"};
                }
                ++m_pos;
                return {Tok::And, "&"};
            case '|':
                if (peek(1) == '|') {
                    m_pos += 2;
                    return {Tok::OrOr, "||"};
                }
                ++m_pos;
                return {Tok::Or, "|"};
            case '-':
                if (peek(1) == '>') {
                    m_pos += 2;
                    return {Tok::Implies, "->"};
                }
                ++m_pos;
                return {Tok::Unknown, "-"};
            case '<':
                if (peek(1) == '-' && peek(2) == '>') {
                    m_pos += 3;
                    return {Tok::Iff, "<->"};
                }
                ++m_pos;
                return {Tok::Unknown, "<"};
            default:
                ++m_pos;
                return {Tok::Unknown, std::string(1, character)};
        }
    }
};

bool is_ltl_operator_ident(const std::string& text) {
    return text == "X" || text == "F" || text == "G" || text == "U" ||
           text == "R" || text == "W";
}

class Parser {
   public:
    explicit Parser(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

    Specification parse() {
        Specification spec;
        parse_info(spec);
        if (peek().m_type == Tok::Ident && peek().m_text == "GLOBAL") {
            reject_construct("GLOBAL block");
        }
        parse_main(spec);
        return spec;
    }

   private:
    std::vector<Token> m_tokens;
    std::size_t m_pos = 0;

    [[nodiscard]] const Token& peek(std::size_t offset = 0) const {
        const std::size_t idx = m_pos + offset;
        return idx < m_tokens.size() ? m_tokens[idx] : m_tokens.back();
    }

    const Token& advance() {
        const Token& token = peek();
        if (m_pos + 1 < m_tokens.size()) {
            ++m_pos;
        }
        return token;
    }

    bool accept(Tok type) {
        if (peek().m_type == type) {
            advance();
            return true;
        }
        return false;
    }

    const Token& expect(Tok type, const std::string& what) {
        if (peek().m_type != type) {
            throw std::invalid_argument("TLSF parse error: expected " + what +
                                        " but found '" + peek().m_text + "'");
        }
        return advance();
    }

    const Token& expect_ident(const std::string& value) {
        if (peek().m_type != Tok::Ident || peek().m_text != value) {
            throw std::invalid_argument("TLSF parse error: expected '" + value +
                                        "' but found '" + peek().m_text + "'");
        }
        return advance();
    }

    // --- INFO ---

    // True at the end of the INFO block or the start of the next `KEY:` entry —
    // the boundaries that terminate a semicolon-less INFO value.
    [[nodiscard]] bool at_info_boundary() const {
        const Tok type = peek().m_type;
        if (type == Tok::RBrace || type == Tok::End || type == Tok::Semicolon) {
            return true;
        }
        return type == Tok::Ident && peek(1).m_type == Tok::Colon;
    }

    void parse_info(Specification& spec) {
        expect_ident("INFO");
        expect(Tok::LBrace, "'{'");
        while (peek().m_type != Tok::RBrace && peek().m_type != Tok::End) {
            parse_info_entry(spec);
        }
        expect(Tok::RBrace, "'}'");
    }

    // TLSF INFO entries are whitespace-separated `KEY: value`, with no `;`
    // terminator (unlike MAIN statements); a stray trailing `;` is tolerated.
    void parse_info_entry(Specification& spec) {
        const Token key = expect(Tok::Ident, "an INFO key");
        expect(Tok::Colon, "':'");
        if (key.m_text == "TITLE") {
            spec.m_title = expect(Tok::String, "a string").m_text;
        } else if (key.m_text == "DESCRIPTION") {
            spec.m_description = expect(Tok::String, "a string").m_text;
        } else if (key.m_text == "SEMANTICS") {
            parse_semantics(spec);  // consumes its own optional ';'
            return;
        } else {
            // TARGET, TAGS, VERSION, and any other key: value consumed
            // verbatim.
            skip_info_value();  // consumes its own optional ';'
            return;
        }
        accept(Tok::Semicolon);
    }

    void parse_semantics(Specification& spec) {
        std::vector<std::string> idents;
        while (!at_info_boundary()) {
            if (peek().m_type == Tok::Ident) {
                idents.push_back(peek().m_text);
            }
            advance();
        }
        accept(Tok::Semicolon);
        bool machine_moore = false;
        bool machine_found = false;
        bool strict = false;
        for (const std::string& ident : idents) {
            const std::string lower = to_lower(ident);
            if (lower == "finite") {
                throw std::invalid_argument(
                    "TLSF semantics not supported: finite (LTLf) semantics are "
                    "out of scope");
            }
            if (lower == "mealy") {
                machine_moore = false;
                machine_found = true;
            } else if (lower == "moore") {
                machine_moore = true;
                machine_found = true;
            } else if (lower == "strict") {
                strict = true;
            } else if (lower == "standard") {
                strict = false;
            }
        }
        if (!machine_found) {
            throw std::invalid_argument(
                "TLSF parse error: SEMANTICS must name Mealy or Moore");
        }
        if (machine_moore) {
            spec.m_semantics =
                strict ? Semantics::MooreStrict : Semantics::MooreStandard;
        } else {
            spec.m_semantics =
                strict ? Semantics::MealyStrict : Semantics::MealyStandard;
        }
    }

    void skip_info_value() {
        while (!at_info_boundary()) {
            advance();
        }
        accept(Tok::Semicolon);
    }

    // --- MAIN ---

    void parse_main(Specification& spec) {
        expect_ident("MAIN");
        expect(Tok::LBrace, "'{'");
        while (peek().m_type != Tok::RBrace && peek().m_type != Tok::End) {
            parse_main_section(spec);
        }
        expect(Tok::RBrace, "'}'");
    }

    void parse_main_section(Specification& spec) {
        const Token name = expect(Tok::Ident, "a section name");
        if (name.m_text == "INPUTS") {
            parse_signal_list(spec.m_inputs);
            return;
        }
        if (name.m_text == "OUTPUTS") {
            parse_signal_list(spec.m_outputs);
            return;
        }
        if (name.m_text == "PARAMETERS") {
            reject_construct("PARAMETERS section");
        }
        if (name.m_text == "DEFINITIONS") {
            reject_construct("DEFINITIONS section");
        }
        std::vector<Formula>* target = section_target(spec, name.m_text);
        if (target == nullptr) {
            throw std::invalid_argument(
                "TLSF parse error: unknown MAIN section '" + name.m_text + "'");
        }
        parse_formula_section(*target);
    }

    static std::vector<Formula>* section_target(Specification& spec,
                                                const std::string& name) {
        if (name == "INITIALLY") {
            return &spec.m_initially;
        }
        if (name == "PRESET") {
            return &spec.m_preset;
        }
        if (name == "REQUIRE" || name == "REQUIREMENTS") {
            return &spec.m_require;
        }
        if (name == "ASSUME" || name == "ASSUMPTIONS") {
            return &spec.m_assume;
        }
        if (name == "ASSERT" || name == "INVARIANTS") {
            return &spec.m_assert;
        }
        if (name == "GUARANTEE" || name == "GUARANTEES") {
            return &spec.m_guarantee;
        }
        return nullptr;
    }

    void parse_signal_list(std::vector<std::string>& out) {
        expect(Tok::LBrace, "'{'");
        while (peek().m_type != Tok::RBrace && peek().m_type != Tok::End) {
            const Token signal = expect(Tok::Ident, "a signal name");
            if (peek().m_type == Tok::LBracket) {
                reject_construct("bus declaration");
            }
            if (peek().m_type == Tok::LBrace) {
                reject_construct("enumeration");
            }
            out.push_back(signal.m_text);
            expect(Tok::Semicolon, "';'");
        }
        expect(Tok::RBrace, "'}'");
    }

    void parse_formula_section(std::vector<Formula>& out) {
        expect(Tok::LBrace, "'{'");
        while (peek().m_type != Tok::RBrace && peek().m_type != Tok::End) {
            out.push_back(parse_expr());
            expect(Tok::Semicolon, "';'");
        }
        expect(Tok::RBrace, "'}'");
    }

    // --- LTL expression grammar (loosest to tightest) ---

    Formula parse_expr() { return parse_iff(); }

    Formula parse_iff() {
        Formula lhs = parse_implies();
        while (accept(Tok::Iff)) {
            Formula rhs = parse_implies();
            lhs = Formula::make_binary(Formula::Kind::Iff, lhs, rhs);
        }
        return lhs;
    }

    Formula parse_implies() {
        Formula lhs = parse_or();
        if (accept(Tok::Implies)) {
            Formula rhs = parse_implies();  // right-associative
            return Formula::make_binary(Formula::Kind::Implies, lhs, rhs);
        }
        return lhs;
    }

    // TLSF's boolean connectives are the doubled `&&`/`||`; the
    // single-character
    // `&`/`|` are accepted too so SPOT-syntax formulae (e.g. Formula::to_string
    // output) round-trip. The bracketed `&&[...]`/`||[...]` loop aggregates are
    // rejected as operands in parse_unary/parse_primary, not here.
    Formula parse_or() {
        Formula lhs = parse_and();
        while (peek().m_type == Tok::Or || peek().m_type == Tok::OrOr) {
            advance();
            Formula rhs = parse_and();
            lhs = Formula::make_binary(Formula::Kind::Or, lhs, rhs);
        }
        return lhs;
    }

    Formula parse_and() {
        Formula lhs = parse_until();
        while (peek().m_type == Tok::And || peek().m_type == Tok::AndAnd) {
            advance();
            Formula rhs = parse_until();
            lhs = Formula::make_binary(Formula::Kind::And, lhs, rhs);
        }
        return lhs;
    }

    Formula parse_until() {
        Formula lhs = parse_unary();
        if (peek().m_type == Tok::Ident &&
            (peek().m_text == "U" || peek().m_text == "R" ||
             peek().m_text == "W")) {
            const std::string op_text = advance().m_text;
            Formula rhs = parse_until();  // right-associative
            Formula::Kind kind = Formula::Kind::Until;
            if (op_text == "R") {
                kind = Formula::Kind::Release;
            } else if (op_text == "W") {
                kind = Formula::Kind::WeakUntil;
            }
            return Formula::make_binary(kind, lhs, rhs);
        }
        return lhs;
    }

    Formula parse_unary() {
        if ((peek().m_type == Tok::AndAnd || peek().m_type == Tok::OrOr) &&
            peek(1).m_type == Tok::LBracket) {
            reject_construct("loop aggregate");
        }
        if (peek().m_type == Tok::At || peek().m_type == Tok::Prime) {
            reject_construct("primed/bus-access signal syntax");
        }
        if (accept(Tok::Not)) {
            return Formula::make_unary(Formula::Kind::Not, parse_unary());
        }
        if (peek().m_type == Tok::Ident) {
            const std::string& text = peek().m_text;
            if (text == "X") {
                advance();
                return parse_next();
            }
            if (text == "F") {
                advance();
                return parse_bounded(Formula::Kind::Eventually);
            }
            if (text == "G") {
                advance();
                return parse_bounded(Formula::Kind::Globally);
            }
        }
        return parse_primary();
    }

    // X phi, or X[n] phi expanded to n nested Next.
    Formula parse_next() {
        if (accept(Tok::LBracket)) {
            const std::size_t count = parse_bound_number();
            expect(Tok::RBracket, "']'");
            Formula operand = parse_unary();
            return nest_next(operand, count);
        }
        return Formula::make_unary(Formula::Kind::Next, parse_unary());
    }

    // F phi / G phi, or F[a..b] / G[a..b] expanded to an Or/And of X-chains.
    Formula parse_bounded(Formula::Kind unary_kind) {
        if (!accept(Tok::LBracket)) {
            return Formula::make_unary(unary_kind, parse_unary());
        }
        const std::size_t lower = parse_bound_number();
        std::size_t upper = lower;
        if (accept(Tok::DotDot)) {
            upper = parse_bound_number();
        }
        expect(Tok::RBracket, "']'");
        if (lower > upper) {
            throw std::invalid_argument(
                "TLSF parse error: bounded operator lower bound exceeds upper "
                "bound");
        }
        Formula operand = parse_unary();
        const Formula::Kind combine = (unary_kind == Formula::Kind::Globally)
                                          ? Formula::Kind::And
                                          : Formula::Kind::Or;
        Formula result = nest_next(operand, lower);
        for (std::size_t i = lower + 1; i <= upper; ++i) {
            result =
                Formula::make_binary(combine, result, nest_next(operand, i));
        }
        return result;
    }

    std::size_t parse_bound_number() {
        const Token number = expect(Tok::Number, "a bound (integer)");
        std::size_t value = 0;
        try {
            value = static_cast<std::size_t>(std::stoull(number.m_text));
        } catch (const std::exception&) {
            throw std::invalid_argument("TLSF parse error: invalid bound '" +
                                        number.m_text + "'");
        }
        if (value > k_max_bound_expansion) {
            throw std::invalid_argument(
                "TLSF parse error: bounded operator bound " + number.m_text +
                " exceeds maximum expansion of " +
                std::to_string(k_max_bound_expansion));
        }
        return value;
    }

    static Formula nest_next(const Formula& operand, std::size_t count) {
        Formula result = operand;
        for (std::size_t i = 0; i < count; ++i) {
            result = Formula::make_unary(Formula::Kind::Next, result);
        }
        return result;
    }

    Formula parse_primary() {
        if (accept(Tok::LParen)) {
            Formula inner = parse_expr();
            expect(Tok::RParen, "')'");
            return inner;
        }
        if ((peek().m_type == Tok::AndAnd || peek().m_type == Tok::OrOr) &&
            peek(1).m_type == Tok::LBracket) {
            reject_construct("loop aggregate");
        }
        if (peek().m_type == Tok::At || peek().m_type == Tok::Prime) {
            reject_construct("primed/bus-access signal syntax");
        }
        if (peek().m_type == Tok::Ident) {
            const Token token = advance();
            if (token.m_text == "true" || token.m_text == "false") {
                return Formula(token.m_text);
            }
            if (is_ltl_operator_ident(token.m_text)) {
                throw std::invalid_argument(
                    "TLSF parse error: operator '" + token.m_text +
                    "' used where an operand was expected");
            }
            if (peek().m_type == Tok::At || peek().m_type == Tok::Prime) {
                reject_construct("primed/bus-access signal syntax");
            }
            if (peek().m_type == Tok::LBracket) {
                reject_construct("bus access");
            }
            return Formula::make_atom(token.m_text);
        }
        throw std::invalid_argument("TLSF parse error: unexpected token '" +
                                    peek().m_text +
                                    "' where an expression was expected");
    }
};

}  // namespace

Specification parse(const std::string& text) {
    Lexer lexer(text);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

}  // namespace tlsf
