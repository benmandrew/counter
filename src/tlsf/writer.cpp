#include "tlsf/writer.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "prop_formula.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

namespace {

std::string escape_string(const std::string& value) {
    std::string result;
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    return result;
}

std::string replace_all(std::string text, const std::string& pattern,
                        const std::string& replacement) {
    std::size_t pos = 0;
    while ((pos = text.find(pattern, pos)) != std::string::npos) {
        text.replace(pos, pattern.size(), replacement);
        pos += replacement.size();
    }
    return text;
}

// Formula::to_string() renders SPOT syntax with single `&`/`|`; TLSF's boolean
// connectives are the doubled `&&`/`||`. Operands are fully parenthesized, so
// each connective appears only as the delimited substring " & " / " | ".
std::string to_tlsf_expr(const Formula& formula) {
    return replace_all(replace_all(formula.to_string(), " & ", " && "), " | ",
                       " || ");
}

const char* machine_name(Semantics semantics) {
    switch (semantics) {
        case Semantics::MooreStandard:
        case Semantics::MooreStrict:
            return "Moore";
        case Semantics::MealyStandard:
        case Semantics::MealyStrict:
        default:
            return "Mealy";
    }
}

const char* semantics_name(Semantics semantics) {
    switch (semantics) {
        case Semantics::MealyStrict:
            return "Mealy,Strict";
        case Semantics::MooreStandard:
            return "Moore";
        case Semantics::MooreStrict:
            return "Moore,Strict";
        case Semantics::MealyStandard:
        default:
            return "Mealy";
    }
}

void write_signal_list(std::string& out, const std::string& name,
                       const std::vector<std::string>& signals) {
    out += "  " + name + " {\n";
    for (const std::string& signal : signals) {
        out += "    " + signal + ";\n";
    }
    out += "  }\n";
}

void write_formula_section(std::string& out, const std::string& name,
                           const std::vector<Formula>& formulae) {
    if (formulae.empty()) {
        return;
    }
    out += "  " + name + " {\n";
    for (const Formula& formula : formulae) {
        out += "    " + to_tlsf_expr(formula) + ";\n";
    }
    out += "  }\n";
}

}  // namespace

std::string write(const Specification& specification) {
    std::string out;
    // TLSF INFO entries carry no `;` terminator (unlike MAIN statements).
    out += "INFO {\n";
    out += "  TITLE:       \"" + escape_string(specification.m_title) + "\"\n";
    out += "  DESCRIPTION: \"" + escape_string(specification.m_description) +
           "\"\n";
    out += std::string("  SEMANTICS:   ") +
           semantics_name(specification.m_semantics) + "\n";
    out += std::string("  TARGET:      ") +
           machine_name(specification.m_semantics) + "\n";
    out += "}\n\n";

    out += "MAIN {\n";
    write_signal_list(out, "INPUTS", specification.m_inputs);
    write_signal_list(out, "OUTPUTS", specification.m_outputs);
    write_formula_section(out, "INITIALLY", specification.m_initially);
    write_formula_section(out, "PRESET", specification.m_preset);
    write_formula_section(out, "REQUIRE", specification.m_require);
    write_formula_section(out, "ASSUME", specification.m_assume);
    write_formula_section(out, "ASSERT", specification.m_assert);
    write_formula_section(out, "GUARANTEE", specification.m_guarantee);
    out += "}\n";
    return out;
}

}  // namespace tlsf
