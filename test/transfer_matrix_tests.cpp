#include <sstream>
#include <string>
#include <vector>

#include "model_counter.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "transfer_matrix.hpp"

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
                        << "), expected " << expected_value << " but found "
                        << actual_value;
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
                    << trace_length << ", expected " << expected_count
                    << " but found " << actual_count;
            fail(message.str());
        }
    }
}

void test_immediately_requirement() {
    const Requirement requirement{"P", "Q", Timing::Immediately};
    const TransferSystem system = build_transfer_system(requirement);

    expect(system.m_states.size() == 3,
           "immediately: expected three valid states after filtering");
    expect(state_labels(system) == "~P~Q ~PQ PQ",
           "immediately: unexpected state ordering");

    const std::vector<std::vector<Count>> expected_matrix = {
        {1, 1, 1},
        {1, 1, 1},
        {1, 1, 1},
    };

    expect_matrix_equals(system.m_transition_matrix, expected_matrix,
                         "immediately transfer matrix");
    expect_matrix_equals(weighted_transition_matrix(system), expected_matrix,
                         "immediately weighted transfer matrix");
    expect_trace_counts(system, {3, 9, 27, 81, 243},
                        "immediately trace counts");
}

void test_next_timepoint_requirement() {
    const Requirement requirement{"P", "Q", Timing::NextTimepoint};
    const TransferSystem system = build_transfer_system(requirement);

    expect(system.m_states.size() == 4,
           "next-timepoint: expected four canonical states");
    expect(state_labels(system) == "~P~Q ~PQ P~Q PQ",
           "next-timepoint: unexpected state ordering");

    const std::vector<std::vector<Count>> expected_matrix = {
        {1, 1, 1, 1},
        {1, 1, 1, 1},
        {0, 1, 0, 1},
        {0, 1, 0, 1},
    };

    expect_matrix_equals(system.m_transition_matrix, expected_matrix,
                         "next-timepoint transfer matrix");
    expect_matrix_equals(weighted_transition_matrix(system), expected_matrix,
                         "next-timepoint weighted transfer matrix");
    expect_trace_counts(system, {4, 12, 36, 108, 324},
                        "next-timepoint trace counts");
}

void test_within_ticks_requirement() {
    const Requirement requirement{"P", "Q", Timing::WithinTicks, 2};
    const TransferSystem system = build_transfer_system(requirement);

    expect(system.m_states.size() == 3,
           "within-ticks: expected three countdown states for N=2");
    expect(state_labels(system) == "c=0 c=1 c=2",
           "within-ticks: unexpected state ordering");

    const std::vector<std::vector<Count>> expected_weighted_matrix = {
        {3, 0, 1},
        {2, 0, 0},
        {2, 1, 1},
    };

    const std::vector<std::vector<Count>> expected_initial = {
        {3},
        {0},
        {1},
    };

    expect_matrix_equals(weighted_transition_matrix(system),
                         expected_weighted_matrix,
                         "within-ticks weighted transfer matrix");
    expect_matrix_equals(system.m_valuation_counts, expected_initial,
                         "within-ticks initial weights");
    expect_trace_counts(system, {4, 16, 62, 240}, "within-ticks trace counts");
}

void test_for_ticks_requirement() {
    const Requirement requirement{"P", "Q", Timing::ForTicks, 2};
    const TransferSystem system = build_transfer_system(requirement);

    expect(system.m_states.size() == 3,
           "for-ticks: expected three countdown states for N=2");
    expect(state_labels(system) == "c=0 c=1 c=2",
           "for-ticks: unexpected state ordering");

    const std::vector<std::vector<Count>> expected_weighted_matrix = {
        {2, 0, 1},
        {1, 0, 1},
        {0, 1, 1},
    };

    const std::vector<std::vector<Count>> expected_initial = {
        {2},
        {0},
        {1},
    };

    expect_matrix_equals(weighted_transition_matrix(system),
                         expected_weighted_matrix,
                         "for-ticks weighted transfer matrix");
    expect_matrix_equals(system.m_valuation_counts, expected_initial,
                         "for-ticks initial weights");
    expect_trace_counts(system, {3, 8, 20, 49}, "for-ticks trace counts");
}

void test_formula_valuation_counts() {
    const Requirement requirement{"A | B", "A", Timing::NextTimepoint};
    const TransferSystem system = build_transfer_system(requirement);

    const std::vector<std::vector<Count>> expected_counts = {
        {1},
        {0},
        {1},
        {2},
    };
    const std::vector<std::vector<Count>> expected_weighted_matrix = {
        {1, 0, 1, 2},
        {1, 0, 1, 2},
        {0, 0, 0, 2},
        {0, 0, 0, 2},
    };

    expect_matrix_equals(system.m_valuation_counts, expected_counts,
                         "formula valuation counts");
    expect_matrix_equals(weighted_transition_matrix(system),
                         expected_weighted_matrix,
                         "formula weighted transfer matrix");
}

void run_transfer_matrix_tests() {
    test_immediately_requirement();
    test_next_timepoint_requirement();
    test_within_ticks_requirement();
    test_for_ticks_requirement();
    test_formula_valuation_counts();
}
