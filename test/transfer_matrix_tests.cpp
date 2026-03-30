#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "model_counter.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "transfer_matrix.hpp"

namespace {

struct TraceStep {
    bool m_trigger_holds;
    bool m_response_holds;
};

std::size_t canonical_index(const TraceStep& step) {
    if (step.m_trigger_holds) {
        return step.m_response_holds ? 3U : 2U;
    }
    return step.m_response_holds ? 1U : 0U;
}

CountVector one_hot_counts(std::size_t index) {
    CountVector counts(4);
    counts.setZero();
    counts(static_cast<Eigen::Index>(index)) = 1;
    return counts;
}

std::vector<TransferSystem> one_hot_transfer_systems(
    const Requirement& requirement) {
    std::vector<TransferSystem> systems;
    systems.reserve(4);
    for (std::size_t index = 0; index < 4; ++index) {
        systems.push_back(
            build_transfer_system(requirement, one_hot_counts(index)));
    }
    return systems;
}

bool trace_accepted_by_transfer_system(const Requirement& requirement,
                                       const std::vector<TraceStep>& trace) {
    if (trace.empty()) {
        throw std::invalid_argument("Trace must have at least one step.");
    }

    const std::vector<TransferSystem> systems =
        one_hot_transfer_systems(requirement);
    CountVector active =
        systems[canonical_index(trace.front())].m_valuation_counts;
    for (std::size_t index = 1; index < trace.size(); ++index) {
        const CountMatrix step_matrix =
            weighted_transition_matrix(systems[canonical_index(trace[index])]);
        active = step_matrix.transpose() * active;
    }
    return active.sum() > 0;
}

bool trace_satisfies_requirement(const Requirement& requirement,
                                 const std::vector<TraceStep>& trace) {
    if (trace.empty()) {
        throw std::invalid_argument("Trace must have at least one step.");
    }

    switch (requirement.m_timing) {
        case Timing::Immediately: {
            for (const TraceStep& step : trace) {
                if (step.m_trigger_holds && !step.m_response_holds) {
                    return false;
                }
            }
            return true;
        }
        case Timing::NextTimepoint: {
            for (std::size_t index = 0; index + 1 < trace.size(); ++index) {
                if (trace[index].m_trigger_holds &&
                    !trace[index + 1].m_response_holds) {
                    return false;
                }
            }
            return true;
        }
        case Timing::WithinTicks: {
            std::size_t countdown = 0;
            for (const TraceStep& step : trace) {
                if (step.m_response_holds) {
                    countdown = 0;
                    continue;
                }
                if (countdown == 0) {
                    countdown =
                        step.m_trigger_holds ? requirement.m_tick_count : 0;
                    continue;
                }
                if (countdown == 1) {
                    return false;
                }
                countdown = step.m_trigger_holds ? requirement.m_tick_count
                                                 : countdown - 1;
            }
            return true;
        }
        case Timing::ForTicks: {
            std::size_t countdown = 0;
            for (const TraceStep& step : trace) {
                if (!step.m_response_holds) {
                    if (countdown > 0 || step.m_trigger_holds) {
                        return false;
                    }
                    continue;
                }
                if (step.m_trigger_holds) {
                    countdown = requirement.m_tick_count;
                    continue;
                }
                if (countdown > 0) {
                    --countdown;
                }
            }
            return true;
        }
    }

    return false;
}

void expect_trace_acceptance(const Requirement& requirement,
                             const std::vector<TraceStep>& trace,
                             bool expected_acceptance,
                             const std::string& label) {
    const bool semantic_acceptance =
        trace_satisfies_requirement(requirement, trace);
    const bool transfer_acceptance =
        trace_accepted_by_transfer_system(requirement, trace);
    expect(semantic_acceptance == expected_acceptance,
           label + ": semantic expectation mismatch");
    expect(transfer_acceptance == expected_acceptance,
           label + ": transfer-system expectation mismatch");
    expect(transfer_acceptance == semantic_acceptance,
           label + ": transfer system disagrees with semantics");
}

struct TraceAcceptanceCase {
    std::string m_label;
    Requirement m_requirement;
    std::vector<TraceStep> m_trace;
    bool m_expected_acceptance;
};

void test_trace_acceptance_cases() {
    const std::vector<TraceAcceptanceCase> cases = {
        {"immediately positive trace",
         {"P", "Q", Timing::Immediately},
         {{false, false}, {false, true}, {true, true}, {false, false}},
         true},
        {"immediately negative trace",
         {"P", "Q", Timing::Immediately},
         {{false, true}, {true, false}, {false, true}},
         false},
        {"next-timepoint positive trace",
         {"P", "Q", Timing::NextTimepoint},
         {{true, false}, {false, true}, {true, true}, {false, true}},
         true},
        {"next-timepoint negative trace",
         {"P", "Q", Timing::NextTimepoint},
         {{true, false}, {false, false}},
         false},
        {"within-ticks positive trace",
         {"P", "Q", Timing::WithinTicks, 2},
         {{true, false}, {false, false}, {false, true}},
         true},
        {"within-ticks negative trace",
         {"P", "Q", Timing::WithinTicks, 2},
         {{true, false}, {false, false}, {false, false}},
         false},
        {"for-ticks positive trace",
         {"P", "Q", Timing::ForTicks, 2},
         {{true, true}, {false, true}, {false, true}, {false, false}},
         true},
        {"for-ticks negative trace",
         {"P", "Q", Timing::ForTicks, 2},
         {{true, true}, {false, true}, {false, false}},
         false},
    };

    for (const TraceAcceptanceCase& test_case : cases) {
        expect_trace_acceptance(test_case.m_requirement, test_case.m_trace,
                                test_case.m_expected_acceptance,
                                test_case.m_label);
    }
}

}  // namespace

std::string state_labels(const TransferSystem& system) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < system.m_states.size(); ++index) {
        if (index > 0) {
            stream << ' ';
        }
        stream << system.m_states[index].label();
    }
    return stream.str();
}

void expect_matrix_equals(const CountMatrix& actual,
                          const std::vector<std::vector<Count>>& expected,
                          const std::string& label) {
    expect(actual.rows() == static_cast<Eigen::Index>(expected.size()),
           label + ": unexpected row count");

    const Eigen::Index expected_columns =
        expected.empty() ? 0
                         : static_cast<Eigen::Index>(expected.front().size());
    expect(actual.cols() == expected_columns,
           label + ": unexpected column count");

    for (Eigen::Index row = 0; row < actual.rows(); ++row) {
        expect(expected[static_cast<std::size_t>(row)].size() ==
                   static_cast<std::size_t>(actual.cols()),
               label + ": jagged expected matrix");

        for (Eigen::Index column = 0; column < actual.cols(); ++column) {
            const Count actual_value = actual(row, column);
            const Count expected_value =
                expected[static_cast<std::size_t>(row)]
                        [static_cast<std::size_t>(column)];
            if (actual_value != expected_value) {
                std::ostringstream message;
                message << label << ": mismatch at (" << row << ", " << column
                        << "), expected " << count_to_string(expected_value)
                        << " but found " << count_to_string(actual_value);
                fail(message.str());
            }
        }
    }
}

void expect_trace_counts(const TransferSystem& system,
                         const std::vector<Count>& expected_counts,
                         const std::string& label) {
    for (std::size_t index = 0; index < expected_counts.size(); ++index) {
        const std::size_t trace_length = index + 1;
        const Count actual_count = count_traces(system, trace_length);
        const Count expected_count = expected_counts[index];
        if (actual_count != expected_count) {
            std::ostringstream message;
            message << label << ": count mismatch for trace length "
                    << trace_length << ", expected "
                    << count_to_string(expected_count) << " but found "
                    << count_to_string(actual_count);
            fail(message.str());
        }
    }
}

struct TransferSystemCase {
    std::string m_label;
    Requirement m_requirement;
    std::size_t m_expected_state_count;
    std::string m_expected_state_labels;
    bool m_check_valuation_counts;
    std::vector<std::vector<Count>> m_expected_valuation_counts;
    std::vector<Count> m_expected_trace_counts;
};

void test_transfer_system_cases() {
    const std::vector<TransferSystemCase> cases = {
        {"immediately",
         {"P", "Q", Timing::Immediately},
         3,
         "~P~Q ~PQ PQ",
         false,
         {},
         {3, 9, 27, 81, 243}},
        {"next-timepoint",
         {"P", "Q", Timing::NextTimepoint},
         4,
         "~P~Q ~PQ P~Q PQ",
         false,
         {},
         {4, 12, 36, 108, 324}},
        {"within-ticks (P,Q)",
         {"P", "Q", Timing::WithinTicks, 2},
         3,
         "c=0 c=1 c=2",
         true,
         {{3}, {0}, {1}},
         {4, 16, 62, 240}},
        {"within-ticks (P,P|Q)",
         {"P", "P|Q", Timing::WithinTicks, 2},
         3,
         "c=0 c=1 c=2",
         true,
         {{4}, {0}, {0}},
         {4, 16, 64, 256}},
        {"for-ticks (P,Q)",
         {"P", "Q", Timing::ForTicks, 2},
         3,
         "c=0 c=1 c=2",
         true,
         {{2}, {0}, {1}},
         {3, 8, 20, 49}},
        {"for-ticks (P,P|Q)",
         {"P", "P|Q", Timing::ForTicks, 2},
         3,
         "c=0 c=1 c=2",
         true,
         {{2}, {0}, {2}},
         {4, 14, 46, 148}},
    };

    for (const TransferSystemCase& test_case : cases) {
        const TransferSystem system =
            build_transfer_system(test_case.m_requirement);

        expect(system.m_states.size() == test_case.m_expected_state_count,
               test_case.m_label + ": unexpected state count");
        expect(state_labels(system) == test_case.m_expected_state_labels,
               test_case.m_label + ": unexpected state ordering");

        if (test_case.m_check_valuation_counts) {
            expect_matrix_equals(system.m_valuation_counts,
                                 test_case.m_expected_valuation_counts,
                                 test_case.m_label + " valuation counts");
        }

        expect_trace_counts(system, test_case.m_expected_trace_counts,
                            test_case.m_label + " trace counts");
    }
}

struct TransferMatrixCase {
    std::string m_label;
    Requirement m_requirement;
    bool m_check_transition_matrix;
    std::vector<std::vector<Count>> m_expected_transition_matrix;
    std::vector<std::vector<Count>> m_expected_weighted_matrix;
};

void test_transfer_matrix_cases() {
    const std::vector<TransferMatrixCase> cases = {
        {"immediately",
         {"P", "Q", Timing::Immediately},
         true,
         {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}},
         {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}}},
        {"next-timepoint",
         {"P", "Q", Timing::NextTimepoint},
         true,
         {{1, 1, 1, 1}, {1, 1, 1, 1}, {0, 1, 0, 1}, {0, 1, 0, 1}},
         {{1, 1, 1, 1}, {1, 1, 1, 1}, {0, 1, 0, 1}, {0, 1, 0, 1}}},
        {"within-ticks (P,Q)",
         {"P", "Q", Timing::WithinTicks, 2},
         false,
         {},
         {{3, 0, 1}, {2, 0, 0}, {2, 1, 1}}},
        {"within-ticks (P,P|Q)",
         {"P", "P|Q", Timing::WithinTicks, 2},
         false,
         {},
         {{4, 0, 0}, {3, 0, 0}, {3, 1, 0}}},
        {"for-ticks (P,Q)",
         {"P", "Q", Timing::ForTicks, 2},
         false,
         {},
         {{2, 0, 1}, {1, 0, 1}, {0, 1, 1}}},
        {"for-ticks (P,P|Q)",
         {"P", "P|Q", Timing::ForTicks, 2},
         false,
         {},
         {{2, 0, 2}, {1, 0, 2}, {0, 1, 2}}},
        {"formula weighted matrix",
         {"A | B", "A", Timing::NextTimepoint},
         false,
         {},
         {{1, 0, 1, 2}, {1, 0, 1, 2}, {0, 0, 0, 2}, {0, 0, 0, 2}}},
    };

    for (const TransferMatrixCase& test_case : cases) {
        const TransferSystem system =
            build_transfer_system(test_case.m_requirement);

        if (test_case.m_check_transition_matrix) {
            expect_matrix_equals(system.m_transition_matrix,
                                 test_case.m_expected_transition_matrix,
                                 test_case.m_label + " transfer matrix");
        }

        expect_matrix_equals(weighted_transition_matrix(system),
                             test_case.m_expected_weighted_matrix,
                             test_case.m_label + " weighted transfer matrix");
    }
}

struct FormulaValuationCase {
    std::string m_label;
    Requirement m_requirement;
    std::vector<std::vector<Count>> m_expected_valuation_counts;
};

void test_formula_valuation_cases() {
    const std::vector<FormulaValuationCase> cases = {
        {"formula valuation counts",
         {"A | B", "A", Timing::NextTimepoint},
         {{1}, {0}, {1}, {2}}},
    };

    for (const FormulaValuationCase& test_case : cases) {
        const TransferSystem system =
            build_transfer_system(test_case.m_requirement);
        expect_matrix_equals(system.m_valuation_counts,
                             test_case.m_expected_valuation_counts,
                             test_case.m_label);
    }
}

void test_weighted_transition_matrix_applies_column_weights() {
    TransferSystem system;
    system.m_states = {{false, false}, {true, true}};
    system.m_valuation_counts = CountVector(2);
    system.m_valuation_counts << 5, 7;
    system.m_transition_matrix = CountMatrix(2, 2);
    system.m_transition_matrix << 1, 2, 3, 4;
    system.m_transition_matrix_is_weighted = false;

    const CountMatrix weighted = weighted_transition_matrix(system);
    expect_matrix_equals(weighted, {{5, 14}, {15, 28}},
                         "weighted transition column scaling");
}

void test_weighted_transition_matrix_passthrough() {
    TransferSystem system;
    system.m_states = {{false, false}, {true, true}};
    system.m_valuation_counts = CountVector(2);
    system.m_valuation_counts << 3, 9;
    system.m_transition_matrix = CountMatrix(2, 2);
    system.m_transition_matrix << 2, 4, 6, 8;
    system.m_transition_matrix_is_weighted = true;

    const CountMatrix weighted = weighted_transition_matrix(system);
    expect_matrix_equals(weighted, {{2, 4}, {6, 8}},
                         "weighted transition passthrough");
}

void run_transfer_matrix_tests() {
    test_transfer_system_cases();
    test_transfer_matrix_cases();
    test_formula_valuation_cases();
    test_trace_acceptance_cases();
    test_weighted_transition_matrix_applies_column_weights();
    test_weighted_transition_matrix_passthrough();
}
