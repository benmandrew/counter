#include "runner/spot_simplify.hpp"

#include <mutex>
#include <optional>
#include <string>

#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/tl/simplify.hh>

#include "profile.hpp"

namespace {

std::mutex& spot_mutex() {
    static std::mutex mutex;
    return mutex;
}

}  // namespace

std::optional<std::string> spot_simplify(const std::string& formula) {
    // Outside the lock deliberately, so the recorded wall time includes waiting
    // for it. Wall far above CPU here would be the signal that serialising has
    // started to cost something.
    COUNTER_PROFILE_SCOPE("ltlfilt/libspot-simplify");
    const std::scoped_lock lock(spot_mutex());
    // Inside the lock, not at namespace scope: constructing a tl_simplifier
    // touches the same SPOT globals its use does, and doing that unserialised
    // crashes. The function-local static's own guard orders construction, but
    // does not order it against another thread already simplifying.
    // Level 3 matches `ltlfilt --simplify`; the default options do not.
    static spot::tl_simplifier simplifier{spot::tl_simplifier_options(3)};
    const spot::parsed_formula parsed = spot::parse_infix_psl(formula);
    if (!parsed.errors.empty() || !parsed.f) {
        return std::nullopt;
    }
    return spot::str_psl(simplifier.simplify(parsed.f));
}
