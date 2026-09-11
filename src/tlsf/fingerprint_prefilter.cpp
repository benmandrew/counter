#include "tlsf/fingerprint_prefilter.hpp"

#include <exception>
#include <string>
#include <vector>

namespace tlsf::prefilter {

std::vector<fingerprint::PackedFingerprint> fingerprints_of(
    const std::vector<Specification>& specs) {
    if (specs.empty()) {
        return {};
    }
    std::vector<std::string> signals = specs.front().m_inputs;
    signals.insert(signals.end(), specs.front().m_outputs.begin(),
                   specs.front().m_outputs.end());
    if (signals.empty()) {
        return {};
    }
    const std::vector<fingerprint::LassoWord> words = fingerprint::sample_words(
        signals, k_words, k_seed, k_max_prefix, k_max_cycle);
    std::vector<fingerprint::PackedFingerprint> prints;
    prints.reserve(specs.size());
    for (const Specification& spec : specs) {
        try {
            prints.push_back(fingerprint::pack(
                fingerprint::fingerprint_of(spec.to_ltl_formula(), words)));
        } catch (const std::exception&) {
            return {};
        }
    }
    return prints;
}

}  // namespace tlsf::prefilter
