#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "fitness/model_counter.hpp"
#include "fitness/transfer_matrix.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

using CountGrid = std::vector<std::vector<Count>>;

CountVector count_vector_from_values(const std::vector<Count>& values) {
    CountVector counts(static_cast<Eigen::Index>(values.size()));
    for (Eigen::Index index = 0; index < counts.size(); ++index) {
        counts(index) = values[static_cast<std::size_t>(index)];
    }
    return counts;
}

CountMatrix count_matrix_from_rows(const CountGrid& rows) {
    const auto row_count = static_cast<Eigen::Index>(rows.size());
    const Eigen::Index column_count =
        rows.empty() ? 0 : static_cast<Eigen::Index>(rows.front().size());
    CountMatrix matrix(row_count, column_count);
    for (Eigen::Index row = 0; row < row_count; ++row) {
        for (Eigen::Index column = 0; column < column_count; ++column) {
            matrix(row, column) = rows[static_cast<std::size_t>(row)]
                                      [static_cast<std::size_t>(column)];
        }
    }
    return matrix;
}

template <typename CaseType, typename Fn>
void run_cases(const std::vector<CaseType>& cases, Fn run_case) {
    for (const CaseType& test_case : cases) {
        run_case(test_case);
    }
}

using Trace = std::vector<std::pair<bool, bool>>;

bool satisfies_immediately(const Trace& trace) {
    return std::all_of(trace.begin(), trace.end(), [](const auto& step) {
        const auto& [trigger, response] = step;
        return !trigger || response;
    });
}

bool satisfies_next_timepoint(const Trace& trace) {
    for (std::size_t idx = 0; idx + 1 < trace.size(); ++idx) {
        if (trace[idx].first && !trace[idx + 1].second) {
            return false;
        }
    }
    return true;
}

bool satisfies_within_ticks(const Trace& trace, std::size_t ticks) {
    std::size_t countdown = 0;
    for (const auto& [trigger, response] : trace) {
        if (response) {
            countdown = 0;
            continue;
        }
        if (countdown == 0) {
            countdown = trigger ? ticks : 0;
            continue;
        }
        if (countdown == 1) {
            return false;
        }
        countdown = trigger ? ticks : countdown - 1;
    }
    return true;
}

bool satisfies_for_ticks(const Trace& trace, std::size_t ticks) {
    std::size_t countdown = 0;
    for (const auto& [trigger, response] : trace) {
        if (!response) {
            if (countdown > 0 || trigger) {
                return false;
            }
            continue;
        }
        if (trigger) {
            countdown = ticks;
            continue;
        }
        if (countdown > 0) {
            --countdown;
        }
    }
    return true;
}

bool satisfies_after_ticks(const Trace& trace, std::size_t ticks) {
    std::size_t countdown = 0;
    for (const auto& [trigger, response] : trace) {
        if (countdown == 0) {
            if (!trigger) {
                continue;
            }
            if (response) {
                return false;
            }
            countdown = ticks;
        } else if (countdown == 1) {
            if (!response || trigger) {
                return false;
            }
            countdown = 0;
        } else {
            if (response) {
                return false;
            }
            countdown = trigger ? ticks : countdown - 1;
        }
    }
    return true;
}

bool satisfies_eventually(const Trace& trace) {
    bool pending = false;
    for (const auto& [trigger, response] : trace) {
        if (trigger && !response) {
            pending = true;
        } else if (response) {
            pending = false;
        }
    }
    return !pending;
}

bool trace_satisfies_requirement(const Requirement& requirement,
                                 const Trace& trace) {
    if (trace.empty()) {
        return true;
    }
    return std::visit(
        [&trace](const auto& timing_value) {
            using T = std::decay_t<decltype(timing_value)>;
            if constexpr (std::is_same_v<T, timing::Immediately>) {
                return satisfies_immediately(trace);
            } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                return satisfies_next_timepoint(trace);
            } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                return satisfies_within_ticks(trace, timing_value.m_ticks);
            } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                return satisfies_for_ticks(trace, timing_value.m_ticks);
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                return satisfies_after_ticks(trace, timing_value.m_ticks);
            } else {
                return satisfies_eventually(trace);
            }
        },
        requirement.m_timing);
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

void expect_matrix_equals(const CountMatrix& actual, const CountGrid& expected,
                          const std::string& label) {
    expect(actual.rows() == static_cast<Eigen::Index>(expected.size()),
           label + ": unexpected row count");

    const Eigen::Index expected_columns =
        expected.empty() ? 0
                         : static_cast<Eigen::Index>(expected.front().size());
    expect(actual.cols() == expected_columns,
           label + ": unexpected column count");

    for (Eigen::Index row = 0; row < actual.rows(); ++row) {
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

struct TransferSystemCase {
    std::string m_label;
    Requirement m_requirement;
    std::vector<Count> m_expected_trace_counts;
};

void test_transfer_system_cases() {
    const std::vector<TransferSystemCase> cases = {
        {"immediately",
         Requirement(Formula("P"), Formula("Q"), timing::immediately()),
         {3, 9, 27, 81, 243}},
        {"next-timepoint",
         Requirement(Formula("P"), Formula("Q"), timing::next_timepoint()),
         {4, 12, 36, 108, 324}},
        // k=3,4 counts differ from the old bespoke automaton: SPOT's
        // G(P -> F[0..2] Q) uses standard LTL semantics with no
        // restart-on-retrigger.
        {"within-ticks (P,Q)",
         Requirement(Formula("P"), Formula("Q"), timing::within_ticks(2)),
         {4, 16, 60, 228}},
        {"within-ticks (P,P|Q)",
         Requirement(Formula("P"), Formula("P|Q"), timing::within_ticks(2)),
         {4, 16, 64, 256}},
        {"for-ticks (P,Q)",
         Requirement(Formula("P"), Formula("Q"), timing::for_ticks(2)),
         {3, 8, 20, 49}},
        {"for-ticks (P,P|Q)",
         Requirement(Formula("P"), Formula("P|Q"), timing::for_ticks(2)),
         {4, 14, 46, 148}},
    };

    run_cases(cases, [](const TransferSystemCase& test_case) {
        const TransferSystem system =
            build_transfer_system(test_case.m_requirement);
        expect_trace_counts(system, test_case.m_expected_trace_counts,
                            test_case.m_label + " trace counts");
    });
}

struct TraceAcceptanceCase {
    std::string m_label;
    Requirement m_requirement;
    std::vector<std::pair<bool, bool>> m_trace;
    bool m_expected_acceptance;
};

void test_trace_acceptance_cases() {
    const std::vector<TraceAcceptanceCase> cases = {
        {"immediately positive trace",
         Requirement(Formula("P"), Formula("Q"), timing::immediately()),
         {{false, false}, {false, true}, {true, true}, {false, false}},
         true},
        {"immediately negative trace",
         Requirement(Formula("P"), Formula("Q"), timing::immediately()),
         {{false, true}, {true, false}, {false, true}},
         false},
        {"next-timepoint positive trace",
         Requirement(Formula("P"), Formula("Q"), timing::next_timepoint()),
         {{true, false}, {false, true}, {true, true}, {false, true}},
         true},
        {"next-timepoint negative trace",
         Requirement(Formula("P"), Formula("Q"), timing::next_timepoint()),
         {{true, false}, {false, false}},
         false},
        {"within-ticks positive trace",
         Requirement(Formula("P"), Formula("Q"), timing::within_ticks(2)),
         {{true, false}, {false, false}, {false, true}},
         true},
        {"within-ticks negative trace",
         Requirement(Formula("P"), Formula("Q"), timing::within_ticks(2)),
         {{true, false}, {false, false}, {false, false}},
         false},
        {"for-ticks positive trace",
         Requirement(Formula("P"), Formula("Q"), timing::for_ticks(2)),
         {{true, true}, {false, true}, {false, true}, {false, false}},
         true},
        {"for-ticks negative trace",
         Requirement(Formula("P"), Formula("Q"), timing::for_ticks(2)),
         {{true, true}, {false, true}, {false, false}},
         false},
    };

    run_cases(cases, [](const TraceAcceptanceCase& test_case) {
        const bool actual = trace_satisfies_requirement(test_case.m_requirement,
                                                        test_case.m_trace);
        expect(actual == test_case.m_expected_acceptance,
               test_case.m_label + ": oracle acceptance mismatch");
    });
}

}  // namespace

void expect_square_matrix_size(const CountMatrix& matrix,
                               Eigen::Index expected_size,
                               const std::string& label) {
    expect(matrix.rows() == expected_size, label + ": unexpected row count");
    expect(matrix.cols() == expected_size, label + ": unexpected column count");
}

void test_weighted_transition_matrix_cases() {
    struct WeightedTransitionCase {
        std::string m_label;
        std::vector<Count> m_valuation_counts;
        CountGrid m_transition_matrix;
        bool m_transition_matrix_is_weighted;
        CountGrid m_expected_weighted_matrix;
    };

    const std::vector<WeightedTransitionCase> cases = {
        {"weighted transition column scaling",
         {5, 7},
         {{1, 2}, {3, 4}},
         false,
         {{5, 14}, {15, 28}}},
        {"weighted transition passthrough",
         {3, 9},
         {{2, 4}, {6, 8}},
         true,
         {{2, 4}, {6, 8}}},
    };

    run_cases(cases, [](const WeightedTransitionCase& test_case) {
        TransferSystem system;
        const std::size_t state_count = test_case.m_valuation_counts.size();
        system.m_states.assign(state_count, State{});
        system.m_valuation_counts =
            count_vector_from_values(test_case.m_valuation_counts);
        system.m_transition_matrix =
            count_matrix_from_rows(test_case.m_transition_matrix);
        system.m_transition_matrix_is_weighted =
            test_case.m_transition_matrix_is_weighted;

        const CountMatrix weighted = weighted_transition_matrix(system);
        expect_matrix_equals(weighted, test_case.m_expected_weighted_matrix,
                             test_case.m_label);
    });
}

void test_count_traces_formula() {
    CountMatrix weighted(2, 2);
    weighted << 1, 2, 3, 4;

    TransferSystem system;
    system.m_states.assign(2, State{});
    system.m_transition_matrix = weighted;
    system.m_transition_matrix_is_weighted = true;

    expect(count_traces(system, 0) == 1, "start-state trace count for k=0");
    expect(count_traces(system, 1) == 3, "start-state trace count for k=1");
    expect(count_traces(system, 2) == 17, "start-state trace count for k=2");
}

void run_transfer_matrix_tests() {
    test_transfer_system_cases();
    test_trace_acceptance_cases();
    test_weighted_transition_matrix_cases();
    test_count_traces_formula();
}
