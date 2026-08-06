#include "tlsf/parser.hpp"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "lexer.hpp"
#include "prop_formula.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

namespace {

using internal::Tok;
using internal::Token;

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

// The SEMANTICS value is an unordered, case-insensitive list; unrecognised
// entries are ignored, and a later machine or strictness word overrides an
// earlier one.
Semantics semantics_from_idents(const std::vector<std::string>& idents) {
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
        return strict ? Semantics::MooreStrict : Semantics::MooreStandard;
    }
    return strict ? Semantics::MealyStrict : Semantics::MealyStandard;
}

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

    // Clamps on the trailing End token rather than running off the end, which
    // is what makes peek() total and terminates every `while (!at_boundary())
    // advance();` loop.
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

    // Stops a `{ ... }` body loop at the closing brace, or at end of input so
    // a truncated document falls through to the expect() that reports it
    // rather than spinning on the clamped End token.
    [[nodiscard]] bool at_block_end() const {
        return peek().m_type == Tok::RBrace || peek().m_type == Tok::End;
    }

    // --- INFO ---

    // True at the end of the INFO block or the start of the next `KEY:` entry —
    // the boundaries that terminate a semicolon-less INFO value.
    [[nodiscard]] bool at_info_boundary() const {
        if (at_block_end() || peek().m_type == Tok::Semicolon) {
            return true;
        }
        return peek().m_type == Tok::Ident && peek(1).m_type == Tok::Colon;
    }

    void parse_info(Specification& spec) {
        expect_ident("INFO");
        expect(Tok::LBrace, "'{'");
        while (!at_block_end()) {
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
            // TARGET, TAGS, VERSION, and any other key: the value is discarded,
            // not recorded.
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
        spec.m_semantics = semantics_from_idents(idents);
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
        while (!at_block_end()) {
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
        while (!at_block_end()) {
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
        while (!at_block_end()) {
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
    // output) round-trip.
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

    // `@` and `'` mark bus access and primed signals. Both may follow a signal
    // name as well as open one, so this is checked in more than one place.
    void reject_signal_access() const {
        if (peek().m_type == Tok::At || peek().m_type == Tok::Prime) {
            reject_construct("primed/bus-access signal syntax");
        }
    }

    // Full-format syntax that can only appear where an operand may start.
    // The `&&[...]`/`||[...]` loop aggregates are rejected here rather than in
    // parse_or/parse_and, where the same two tokens are ordinary connectives.
    void reject_unsupported_operand() const {
        if ((peek().m_type == Tok::AndAnd || peek().m_type == Tok::OrOr) &&
            peek(1).m_type == Tok::LBracket) {
            reject_construct("loop aggregate");
        }
        reject_signal_access();
    }

    Formula parse_unary() {
        reject_unsupported_operand();
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
        reject_unsupported_operand();
        if (peek().m_type == Tok::Ident) {
            return parse_ident_operand();
        }
        throw std::invalid_argument("TLSF parse error: unexpected token '" +
                                    peek().m_text +
                                    "' where an expression was expected");
    }

    Formula parse_ident_operand() {
        const Token token = advance();
        if (token.m_text == "true" || token.m_text == "false") {
            return Formula(token.m_text);
        }
        if (is_ltl_operator_ident(token.m_text)) {
            throw std::invalid_argument("TLSF parse error: operator '" +
                                        token.m_text +
                                        "' used where an operand was expected");
        }
        reject_signal_access();
        if (peek().m_type == Tok::LBracket) {
            reject_construct("bus access");
        }
        return Formula::make_atom(token.m_text);
    }
};

}  // namespace

Specification parse(const std::string& text) {
    Parser parser(internal::tokenize(text));
    return parser.parse();
}

}  // namespace tlsf
