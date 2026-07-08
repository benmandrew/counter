#include <nlohmann/json.hpp>

#include "requirement.hpp"
#include "serialisation.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_timing_immediately() {
    const Timing tim = timing::immediately();
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "Immediately",
           "to_json(Timing): Immediately should serialise with type key");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): Immediately round-trip should match");
}

void test_timing_next_timepoint() {
    const Timing tim = timing::next_timepoint();
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "NextTimepoint",
           "to_json(Timing): NextTimepoint should serialise with type key");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): NextTimepoint round-trip should match");
}

void test_timing_within_ticks() {
    const Timing tim = timing::within_ticks(7);
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "WithinTicks",
           "to_json(Timing): WithinTicks should serialise with type key");
    expect(jobj.at("ticks") == 7,
           "to_json(Timing): WithinTicks should serialise ticks value");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): WithinTicks round-trip should match");
}

void test_timing_for_ticks() {
    const Timing tim = timing::for_ticks(3);
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "ForTicks",
           "to_json(Timing): ForTicks should serialise with type key");
    expect(jobj.at("ticks") == 3,
           "to_json(Timing): ForTicks should serialise ticks value");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): ForTicks round-trip should match");
}

void test_timing_after_ticks() {
    const Timing tim = timing::after_ticks(2);
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "AfterTicks",
           "to_json(Timing): AfterTicks should serialise with type key");
    expect(jobj.at("ticks") == 2,
           "to_json(Timing): AfterTicks should serialise ticks value");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): AfterTicks round-trip should match");
}

void test_timing_eventually() {
    const Timing tim = timing::eventually();
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "Eventually",
           "to_json(Timing): Eventually should serialise with type key");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): Eventually round-trip should match");
}

void test_timing_always() {
    const Timing tim = timing::always();
    nlohmann::json jobj;
    timing::to_json(jobj, tim);
    expect(jobj.at("type") == "Always",
           "to_json(Timing): Always should serialise with type key");
    expect(jobj.get<Timing>() == tim,
           "from_json(Timing): Always round-trip should match");
}

void test_requirement_round_trip() {
    const Requirement req(Formula("(P) & (Q)"), Formula("R"),
                          timing::within_ticks(3), ConditionType::Trigger);
    nlohmann::json jobj;
    to_json(jobj, req);
    const Requirement req2 = serialisation::requirement_from_json(jobj);
    expect(req == req2,
           "requirement_from_json: round-trip should preserve all fields");
}

void test_requirement_json_keys() {
    const Requirement req(Formula("t"), Formula("r"), timing::immediately(),
                          ConditionType::Continual);
    nlohmann::json jobj;
    to_json(jobj, req);
    expect(jobj.contains("condition"),
           "to_json(Requirement): should have condition key");
    expect(jobj.contains("condition-type"),
           "to_json(Requirement): should have condition-type key");
    expect(jobj.at("condition-type") == "continual",
           "to_json(Requirement): condition-type should be continual");
    expect(jobj.contains("response"),
           "to_json(Requirement): should have response key");
    expect(jobj.contains("timing"),
           "to_json(Requirement): should have timing key");
    expect(jobj.at("timing").at("type") == "Immediately",
           "to_json(Requirement): timing type should be embedded");
}

void test_requirement_json_keys_trigger() {
    const Requirement req(Formula("c"), Formula("r"), timing::immediately(),
                          ConditionType::Trigger);
    nlohmann::json jobj;
    to_json(jobj, req);
    expect(jobj.at("condition-type") == "trigger",
           "to_json(Requirement): condition-type should be trigger");
}

void test_specification_round_trip() {
    const Specification spec(
        {Requirement(Formula("a"), Formula("b"), timing::immediately(),
                     ConditionType::Continual)},
        {Requirement(Formula("t"), Formula("r"), timing::for_ticks(2),
                     ConditionType::Trigger),
         Requirement(Formula("p"), Formula("q"), timing::after_ticks(1),
                     ConditionType::Continual)},
        {"a", "t", "p"}, {"b", "r", "q"});
    nlohmann::json jobj;
    to_json(jobj, spec);
    const Specification spec2 = jobj.get<Specification>();
    expect(spec == spec2,
           "from_json(Specification): round-trip should preserve full spec");
}

void test_specification_json_structure() {
    const Specification spec(
        {}, {Requirement(Formula("t"), Formula("r"), timing::immediately())},
        {"t"}, {"r"});
    nlohmann::json jobj;
    to_json(jobj, spec);
    expect(jobj.contains("assumptions"),
           "to_json(Specification): should have assumptions key");
    expect(jobj.contains("guarantees"),
           "to_json(Specification): should have guarantees key");
    expect(jobj.contains("in_atoms"),
           "to_json(Specification): should have in_atoms key");
    expect(jobj.contains("out_atoms"),
           "to_json(Specification): should have out_atoms key");
    expect(jobj.at("guarantees").size() == 1,
           "to_json(Specification): guarantee count should match");
    expect(!jobj.contains("fitness"),
           "to_json(Specification): should not have fitness key");
}

void test_scored_specification_without_fitness() {
    const serialisation::ScoredSpecification scored{
        Specification(
            {},
            {Requirement(Formula("t"), Formula("r"), timing::next_timepoint())},
            {"t"}, {"r"}),
        std::nullopt};
    nlohmann::json jobj;
    serialisation::to_json(jobj, scored);
    expect(!jobj.contains("fitness"),
           "ScoredSpecification: absent fitness should not appear in JSON");
    const auto scored2 = jobj.get<serialisation::ScoredSpecification>();
    expect(scored2.spec == scored.spec,
           "ScoredSpecification: spec round-trip should match");
    expect(
        !scored2.fitness.has_value(),
        "ScoredSpecification: fitness should remain absent after round-trip");
}

void test_scored_specification_with_fitness() {
    const serialisation::ScoredSpecification scored{
        Specification(
            {}, {Requirement(Formula("t"), Formula("r"), timing::eventually())},
            {"t"}, {"r"}),
        serialisation::FitnessRecord{
            0.72, {{"syntactic", 0.8, 0.5}, {"semantic", 0.65, 0.5}}}};
    nlohmann::json jobj;
    serialisation::to_json(jobj, scored);
    expect(jobj.contains("fitness"),
           "ScoredSpecification: present fitness should appear in JSON");
    expect(jobj.at("fitness").at("total") == 0.72,
           "ScoredSpecification: total fitness should serialise correctly");
    expect(jobj.at("fitness").at("components").size() == 2,
           "ScoredSpecification: component count should serialise correctly");
    const auto scored2 = jobj.get<serialisation::ScoredSpecification>();
    expect(scored2.spec == scored.spec,
           "ScoredSpecification: spec round-trip with fitness should match");
    if (!scored2.fitness.has_value()) {
        fail("ScoredSpecification: fitness should be present after round-trip");
    }
    const auto& frec = *scored2.fitness;
    expect(frec.total == 0.72,
           "ScoredSpecification: total fitness round-trip should match");
    expect(frec.components.size() == 2,
           "ScoredSpecification: component count round-trip should match");
    expect(frec.components[0].name == "syntactic",
           "ScoredSpecification: component name round-trip should match");
    expect(frec.components[0].score == 0.8,
           "ScoredSpecification: component score round-trip should match");
    expect(frec.components[0].weight == 0.5,
           "ScoredSpecification: component weight round-trip should match");
}

}  // namespace

void run_serialisation_tests() {
    test_timing_immediately();
    test_timing_next_timepoint();
    test_timing_within_ticks();
    test_timing_for_ticks();
    test_timing_after_ticks();
    test_timing_eventually();
    test_timing_always();
    test_requirement_round_trip();
    test_requirement_json_keys();
    test_requirement_json_keys_trigger();
    test_specification_round_trip();
    test_specification_json_structure();
    test_scored_specification_without_fitness();
    test_scored_specification_with_fitness();
}
