#include "genetic/mutation.hpp"

#include <stdexcept>

namespace {

[[noreturn]] void throw_not_implemented() {
    throw std::logic_error("genetic mutation is not implemented yet.");
}

}  // namespace

Requirement mutate_requirement(const Requirement& requirement) {
    (void)requirement;
    throw_not_implemented();
}
