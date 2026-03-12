#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "model_counter.hpp"
#include "requirement.hpp"
#include "transfer_matrix.hpp"

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void expect(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

std::string state_labels(const TransferSystem& system) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < system.states.size(); ++index) {
        if (index > 0) {
            stream << ' ';
        }
        stream << system.states[index].label();
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

    expect(system.states.size() == 3,
           "immediately: expected three valid states after filtering");
    expect(state_labels(system) == "~P~Q ~PQ PQ",
           "immediately: unexpected state ordering");

    const std::vector<std::vector<Count>> expected_matrix = {
        {1, 1, 1},
        {1, 1, 1},
        {1, 1, 1},
    };

    expect_matrix_equals(system.transition_matrix, expected_matrix,
                         "immediately transfer matrix");
    expect_matrix_equals(weighted_transition_matrix(system), expected_matrix,
                         "immediately weighted transfer matrix");
    expect_trace_counts(system, {3, 9, 27, 81, 243},
                        "immediately trace counts");
}

void test_next_timepoint_requirement() {
    const Requirement requirement{"P", "Q", Timing::NextTimepoint};
    const TransferSystem system = build_transfer_system(requirement);

    expect(system.states.size() == 4,
           "next-timepoint: expected four canonical states");
    expect(state_labels(system) == "~P~Q ~PQ P~Q PQ",
           "next-timepoint: unexpected state ordering");

    const std::vector<std::vector<Count>> expected_matrix = {
        {1, 1, 1, 1},
        {1, 1, 1, 1},
        {0, 1, 0, 1},
        {0, 1, 0, 1},
    };

    expect_matrix_equals(system.transition_matrix, expected_matrix,
                         "next-timepoint transfer matrix");
    expect_matrix_equals(weighted_transition_matrix(system), expected_matrix,
                         "next-timepoint weighted transfer matrix");
    expect_trace_counts(system, {4, 12, 36, 108, 324},
                        "next-timepoint trace counts");
}

int main() {
    try {
        test_immediately_requirement();
        test_next_timepoint_requirement();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
