#include <cmath>
#include <string>

#include "config.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

tlsf::Specification parse(const std::string& main_body,
                          const std::string& semantics = "Mealy") {
    return tlsf::parse("INFO { SEMANTICS: " + semantics + "; }\nMAIN {\n" +
                       main_body + "\n}\n");
}

void test_syntactic_self_similarity_is_one() {
    const Config cfg;
    const tlsf::Specification spec =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(r -> F g); }");
    expect(tlsf_syntactic_similarity(spec, spec, cfg) == 1.0,
           "syntactic: a spec is syntactically identical to itself");
}

void test_semantic_self_similarity_is_one() {
    Config cfg;
    cfg.default_model_counting_bound = 4;
    const tlsf::Specification spec =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(r -> F g); }");
    expect(tlsf_semantic_similarity(spec, spec, cfg) == 1.0,
           "semantic: identical section formulae contribute nothing, so self "
           "similarity is 1.0");
}

// The metric config must reach the TLSF path, not only the FRETISH one. A
// non-equivalent guarantee pair (G(r -> g) strictly implies G(r -> F g)) scores
// differently under the two metrics, so their cross-scores must diverge -- were
// the config ignored, the TLSF path would stay on direct and the two would be
// identical.
void test_semantic_similarity_honours_configured_metric() {
    Config cfg;
    cfg.default_model_counting_bound = 4;
    const tlsf::Specification original =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(r -> F g); }");
    const tlsf::Specification candidate =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(r -> g); }");

    cfg.similarity_metric = SimilarityMetric::Direct;
    const double direct = tlsf_semantic_similarity(candidate, original, cfg);
    cfg.similarity_metric = SimilarityMetric::Logarithmic;
    const double logarithmic =
        tlsf_semantic_similarity(candidate, original, cfg);

    expect(direct >= 0.0 && direct <= 1.0 && logarithmic >= 0.0 &&
               logarithmic <= 1.0,
           "semantic: both metrics stay within [0, 1] on the TLSF path");
    expect(std::fabs(direct - logarithmic) > 1e-9,
           "semantic: the configured metric must reach the TLSF path, so the "
           "direct and logarithmic scores diverge for a non-equivalent pair");
}

void test_halstead_self_is_one_and_bounded() {
    const Config cfg;
    const tlsf::Specification base =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(r -> F g); }");
    const double self = tlsf_halstead_fitness(base, base, cfg);
    expect(self == 1.0, "halstead: a spec is no larger than itself, score 1.0");

    // A larger candidate against a smaller original scores in [0, 1].
    const tlsf::Specification larger =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(r -> F (g & g)); }");
    const double score = tlsf_halstead_fitness(larger, base, cfg);
    expect(score >= 0.0 && score <= 1.0,
           "halstead: a size penalty stays within [0, 1]");
}

void test_status_realizable_is_one() {
    const Config cfg;
    const tlsf::Specification spec =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(g <-> r); }");
    expect(tlsf_status(spec, cfg) == 1.0,
           "status: a realizable spec scores 1.0");
}

void test_status_unsatisfiable_guarantee_is_low() {
    const Config cfg;
    const tlsf::Specification spec =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { g; !g; }");
    const double status = tlsf_status(spec, cfg);
    expect(status < 0.5,
           "status: a spec with a contradictory guarantee conjunction scores a "
           "low tier");
}

void test_aggregate_scores_in_unit_interval() {
    Config cfg;
    cfg.default_model_counting_bound = 4;
    const tlsf::Specification spec =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(g <-> r); }");
    const auto fitness = tlsf_get_fitness_function(spec, cfg);
    expect(!fitness.empty(),
           "aggregate: default weights yield a non-empty fitness function");
    const double score = fitness(spec);
    expect(score >= 0.0 && score <= 1.0,
           "aggregate: the weighted score stays within [0, 1]");
}

}  // namespace

void run_tlsf_fitness_tests() {
    test_syntactic_self_similarity_is_one();
    test_semantic_self_similarity_is_one();
    test_semantic_similarity_honours_configured_metric();
    test_halstead_self_is_one_and_bounded();
    test_status_realizable_is_one();
    test_status_unsatisfiable_guarantee_is_low();
    test_aggregate_scores_in_unit_interval();
}
