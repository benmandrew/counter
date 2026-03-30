#include <string>
#include <vector>

#include "test_suite.hpp"
#include "test_support.hpp"
#include "transfer_matrix.hpp"
#include "transfer_system.hpp"

namespace {

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
            expect(actual_value == expected_value, label + ": matrix mismatch");
        }
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

}  // namespace

void run_transfer_matrix_tests() {
    test_weighted_transition_matrix_applies_column_weights();
    test_weighted_transition_matrix_passthrough();
}
