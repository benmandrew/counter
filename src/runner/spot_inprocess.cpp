#include "runner/spot_inprocess.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/tl/simplify.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/translate.hh>

#include "profile.hpp"

namespace {

std::timed_mutex& spot_mutex() {
    static std::timed_mutex mutex;
    return mutex;
}

// Runs the simplification. The caller must already hold spot_mutex().
std::optional<std::string> simplify_locked(const std::string& formula) {
    // Constructed here rather than at namespace scope: building a
    // tl_simplifier touches the same SPOT globals its use does, and doing that
    // unserialised crashes. The function-local static's own guard orders
    // construction, but does not order it against another thread already
    // simplifying, so it has to sit inside the lock.
    // Level 3 matches `ltlfilt --simplify`; the default options do not.
    static spot::tl_simplifier simplifier{spot::tl_simplifier_options(3)};
    const spot::parsed_formula parsed = spot::parse_infix_psl(formula);
    if (!parsed.errors.empty() || !parsed.f) {
        return std::nullopt;
    }
    return spot::str_psl(simplifier.simplify(parsed.f));
}

}  // namespace

std::optional<std::string> spot_simplify(const std::string& formula) {
    // Outside the lock deliberately, so the recorded wall time includes waiting
    // for it. Wall far above CPU here is the signal that serialising has begun
    // to cost something.
    COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify");
    const std::scoped_lock lock(spot_mutex());
    return simplify_locked(formula);
}

SpotSimplification spot_try_simplify(const std::string& formula,
                                     std::chrono::milliseconds budget) {
    COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify");
    std::unique_lock<std::timed_mutex> lock(spot_mutex(), std::defer_lock);
    if (!lock.try_lock_for(budget)) {
        SpotSimplification result;
        result.m_lock_busy = true;
        return result;
    }
    SpotSimplification result;
    result.m_formula = simplify_locked(formula);
    return result;
}

SpotTranslation spot_translate_for_counting(const std::string& formula,
                                            std::chrono::milliseconds budget) {
    COUNTER_PROFILE_SCOPE("spot/libspot-translate");
    std::unique_lock<std::timed_mutex> lock(spot_mutex(), std::defer_lock);
    if (!lock.try_lock_for(budget)) {
        SpotTranslation busy;
        busy.m_lock_busy = true;
        return busy;
    }
    const spot::parsed_formula parsed = spot::parse_infix_psl(formula);
    if (!parsed.errors.empty() || !parsed.f) {
        return {};
    }
    // -D and -S: prefer a deterministic automaton with state-based acceptance.
    // A fresh bdd_dict per call, so atomic propositions are numbered from this
    // formula alone and match what the tool prints.
    spot::translator translator{spot::make_bdd_dict()};
    translator.set_pref(spot::postprocessor::Deterministic |
                        spot::postprocessor::SBAcc);
    std::ostringstream out;
    try {
        spot::print_hoa(out, translator.run(parsed.f), "");
    } catch (const std::runtime_error& error) {
        // Not an error in the formula: the automaton is correct and universal,
        // and only printing refuses it. Reported rather than swallowed, so the
        // caller substitutes the universal automaton instead of counting a
        // genuinely-true formula as a scoring failure.
        if (std::string(error.what())
                .find("automaton is complete but prop_complete()") !=
            std::string::npos) {
            SpotTranslation translation;
            translation.m_tautology_print_bug = true;
            return translation;
        }
        // Any other libspot failure is reported as "no automaton" rather than
        // rethrown, which sends the caller to the exec. That is the reference
        // behaviour this path replaced, so falling back to it keeps in-process
        // translation from ever being the worse of the two: whatever ltl2tgba
        // makes of the formula, including failing, is what happens.
        return {};
    }
    // print_hoa leaves off the trailing newline the tool emits after --END--.
    SpotTranslation translation;
    translation.m_hoa = out.str() + "\n";
    return translation;
}
