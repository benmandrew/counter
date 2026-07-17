#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "prop_formula.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/mucs.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

// Whether any guarantee-side section of `spec` contains the named atom. The
// fake oracles below phrase their (un)realizability verdict over these.
bool has_atom(const tlsf::Specification& spec, const std::string& atom) {
    auto contains = [&atom](const std::vector<Formula>& section) {
        return std::any_of(section.begin(), section.end(),
                           [&atom](const Formula& formula) {
                               return formula.to_string() == atom;
                           });
    };
    return contains(spec.m_preset) || contains(spec.m_assert) ||
           contains(spec.m_guarantee);
}

std::set<std::string> core_atoms(const tlsf::MinimalUnrealizableCore& muc) {
    std::set<std::string> atoms;
    for (const tlsf::CoreFormula& entry : muc.formulae) {
        atoms.insert(entry.formula.to_string());
    }
    return atoms;
}

// The two-guarantee conflict {a, b} sits among four guarantees; QuickXplain
// must return exactly {a, b}.
void test_extracts_minimal_pair() {
    tlsf::Specification spec;
    spec.m_inputs = {"x"};
    spec.m_outputs = {"y"};
    spec.m_guarantee = {Formula::make_atom("a"), Formula::make_atom("b"),
                        Formula::make_atom("c"), Formula::make_atom("d")};
    const tlsf::RealizabilityOracle oracle =
        [](const tlsf::Specification& probe) {
            return !(has_atom(probe, "a") && has_atom(probe, "b"));
        };

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec, oracle);
    expect(core_atoms(muc) == std::set<std::string>({"a", "b"}),
           "core is exactly {a, b}");
    expect(has_atom(muc.spec, "a") && has_atom(muc.spec, "b"),
           "rebuilt core spec keeps the culprits");
    expect(!has_atom(muc.spec, "c") && !has_atom(muc.spec, "d"),
           "rebuilt core spec drops the innocent guarantees");
}

// The conflict is on the last two candidates; confirms the result is a genuine
// minimal set, not just a prefix of the input order.
void test_conflict_at_tail() {
    tlsf::Specification spec;
    spec.m_outputs = {"y"};
    spec.m_guarantee = {Formula::make_atom("a"), Formula::make_atom("b"),
                        Formula::make_atom("c"), Formula::make_atom("d")};
    const tlsf::RealizabilityOracle oracle =
        [](const tlsf::Specification& probe) {
            return !(has_atom(probe, "c") && has_atom(probe, "d"));
        };

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec, oracle);
    expect(core_atoms(muc) == std::set<std::string>({"c", "d"}),
           "core is exactly {c, d}");
}

// A single-formula core.
void test_singleton_core() {
    tlsf::Specification spec;
    spec.m_outputs = {"y"};
    spec.m_guarantee = {Formula::make_atom("a"), Formula::make_atom("b")};
    const tlsf::RealizabilityOracle oracle =
        [](const tlsf::Specification& probe) { return !has_atom(probe, "a"); };

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec, oracle);
    expect(muc.formulae.size() == 1, "core has one formula");
    expect(core_atoms(muc) == std::set<std::string>({"a"}), "core is {a}");
}

// The culprits span two different sections; the core must tag each with the
// right section id so the rebuilt spec places them correctly.
void test_core_spans_sections() {
    tlsf::Specification spec;
    spec.m_outputs = {"y"};
    spec.m_preset = {Formula::make_atom("p")};
    spec.m_guarantee = {Formula::make_atom("g"), Formula::make_atom("h")};
    const tlsf::RealizabilityOracle oracle =
        [](const tlsf::Specification& probe) {
            return !(has_atom(probe, "p") && has_atom(probe, "g"));
        };

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec, oracle);
    expect(core_atoms(muc) == std::set<std::string>({"p", "g"}),
           "core is {p, g} across preset and guarantee");
    bool preset_tagged = false;
    bool guarantee_tagged = false;
    for (const tlsf::CoreFormula& entry : muc.formulae) {
        if (entry.formula.to_string() == "p") {
            preset_tagged = entry.section_id == 1;
        }
        if (entry.formula.to_string() == "g") {
            guarantee_tagged = entry.section_id == 5;
        }
    }
    expect(preset_tagged, "p is tagged as the PRESET section");
    expect(guarantee_tagged, "g is tagged as the GUARANTEE section");
    expect(muc.spec.m_preset.size() == 1 && muc.spec.m_guarantee.size() == 1,
           "rebuilt spec places each culprit in its own section");
}

// With no guarantee-side formulae there is nothing to minimise: the core is
// empty even when the oracle reports unrealizable.
void test_empty_guarantee_side() {
    tlsf::Specification spec;
    spec.m_inputs = {"x"};
    spec.m_outputs = {"y"};
    const tlsf::RealizabilityOracle oracle = [](const tlsf::Specification&) {
        return false;
    };

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec, oracle);
    expect(muc.formulae.empty(), "empty guarantee side yields an empty core");
}

// End-to-end against ltlsynt on the unrealizable arbiter fixture: the core
// must be a strict, still-unrealizable subset, minimal in that dropping any
// one member restores realizability.
const char* const k_unrealizable_arbiter =
    "INFO { SEMANTICS: Mealy; }\n"
    "MAIN {\n"
    "  INPUTS { r0; r1; }\n"
    "  OUTPUTS { g0; g1; }\n"
    "  GUARANTEE {\n"
    "    G (g0 -> r0);\n"
    "    G (g1 -> r1);\n"
    "    G !(g0 & g1);\n"
    "    G F g0;\n"
    "    G F g1;\n"
    "  }\n"
    "}\n";

bool is_realizable(const tlsf::Specification& spec) {
    return global_real_checker().check_realizability_ltl(
        spec.to_ltl(), spec.m_inputs, spec.m_outputs);
}

tlsf::Specification without(const tlsf::Specification& base,
                            const tlsf::CoreFormula& entry) {
    tlsf::Specification reduced = base;
    auto erase_one = [&entry](std::vector<Formula>& section) {
        for (auto it = section.begin(); it != section.end(); ++it) {
            if (it->to_string() == entry.formula.to_string()) {
                section.erase(it);
                return;
            }
        }
    };
    switch (entry.section_id) {
        case 1:
            erase_one(reduced.m_preset);
            break;
        case 4:
            erase_one(reduced.m_assert);
            break;
        default:
            erase_one(reduced.m_guarantee);
            break;
    }
    return reduced;
}

void test_arbiter_end_to_end() {
    const tlsf::Specification spec = tlsf::parse(k_unrealizable_arbiter);
    expect(!is_realizable(spec), "fixture is unrealizable");

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec);
    expect(!muc.formulae.empty(), "arbiter core is non-empty");
    expect(muc.formulae.size() < spec.m_guarantee.size(),
           "core is a strict subset of the guarantees");
    expect(!is_realizable(muc.spec), "core spec is still unrealizable");
    for (const tlsf::CoreFormula& entry : muc.formulae) {
        expect(is_realizable(without(muc.spec, entry)),
               "dropping any one core formula restores realizability");
    }
}

}  // namespace

void run_tlsf_mucs_tests() {
    test_extracts_minimal_pair();
    test_conflict_at_tail();
    test_singleton_core();
    test_core_spans_sections();
    test_empty_guarantee_side();
    test_arbiter_end_to_end();
}
