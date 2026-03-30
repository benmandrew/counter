#include <iostream>
#include <vector>

#include "model_counter.hpp"
#include "requirement.hpp"
#include "transfer_matrix.hpp"

namespace {

void print_state_labels(const TransferSystem& system) {
    std::cout << "States:";
    for (const State& state : system.m_states) {
        std::cout << ' ' << state.label();
    }
    std::cout << "\n";
}

void print_trace_counts(const TransferSystem& system,
                        std::size_t max_trace_length) {
    for (std::size_t trace_length = 1; trace_length <= max_trace_length;
         ++trace_length) {
        const Count count = count_traces(system, trace_length);
        std::cout << "Length " << trace_length << ": " << count << "\n";
    }
}

void print_requirement_report(const Requirement& requirement) {
    const TransferSystem system = build_transfer_system(requirement);
    const CountMatrix weighted = weighted_transition_matrix(system);
    std::cout << "Requirement timing: " << to_string(requirement.m_timing)
              << "\n";
    print_state_labels(system);
    std::cout << "Transfer matrix:\n" << system.m_transition_matrix << "\n\n";
    std::cout << "Weighted transfer matrix:\n" << weighted << "\n\n";
    print_trace_counts(system, 5);
    std::cout << "\n";
}

}  // namespace

int main() {
    const std::vector<Requirement> requirements = {
        {"P", "Q", Timing::Immediately},
        {"P", "Q", Timing::NextTimepoint},
    };
    for (const Requirement& requirement : requirements) {
        print_requirement_report(requirement);
    }
    return 0;
}
