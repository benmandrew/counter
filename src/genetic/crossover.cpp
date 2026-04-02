#include "genetic/crossover.hpp"

#include <stdexcept>

namespace {

[[noreturn]] void throw_not_implemented() {
    throw std::logic_error("genetic crossover is not implemented yet.");
}

}  // namespace

Requirement crossover_requirements(const Requirement& first_parent,
                                   const Requirement& second_parent) {
    (void)first_parent;
    (void)second_parent;
    throw_not_implemented();
}
