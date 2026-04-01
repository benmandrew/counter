#include "fitness/syntactic_similarity.hpp"

#include <stdexcept>

namespace {

[[noreturn]] void throw_not_implemented() {
    throw std::logic_error(
        "syntactic_similarity metric is not implemented yet.");
}

}  // namespace

double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement) {
    throw_not_implemented();
}
