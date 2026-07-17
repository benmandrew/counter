#include "tlsf/mucs.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "runner/spot.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

void print_usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " <spec.tlsf>\n"
        << "\n"
        << "Extracts a minimal unrealizable core (MUC) from an unrealizable\n"
        << "basic-TLSF specification: the smallest subset of the guarantee-\n"
        << "side sections (PRESET, ASSERT, GUARANTEE) that stays unrealizable\n"
        << "against the full, unchanged environment side. Prints REALIZABLE\n"
        << "(no core) when the input is already realizable.\n"
        << "Exits 0 on success, 1 on error.\n";
}

bool has_flag(int argc, const char* const* argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, const char* const argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    if (has_flag(argc, argv, "-h") || has_flag(argc, argv, "--help")) {
        print_usage(argv[0]);
        return 0;
    }
    if (argc != 2 || argv[1] == nullptr) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string path = argv[1];
    if (std::filesystem::path(path).extension() != ".tlsf") {
        std::cerr << path
                  << ": expected a .tlsf file (FRETISH JSON is not "
                     "supported by mucs)\n";
        return 1;
    }

    std::ifstream file(path);
    if (!file) {
        std::cerr << path << ": cannot read file\n";
        return 1;
    }
    std::ostringstream contents;
    contents << file.rdbuf();

    tlsf::Specification spec;
    try {
        spec = tlsf::parse(contents.str());
    } catch (const std::exception& exc) {
        std::cerr << path << ": " << exc.what() << "\n";
        return 1;
    }

    RealizabilityChecker& checker = global_real_checker();
    if (checker.check_realizability_ltl(spec.to_ltl(), spec.m_inputs,
                                        spec.m_outputs)) {
        std::cout << "REALIZABLE (no core)\n";
        return 0;
    }

    const tlsf::MinimalUnrealizableCore muc = tlsf::extract_muc(spec);
    const std::size_t n_guarantee_side =
        spec.m_preset.size() + spec.m_assert.size() + spec.m_guarantee.size();
    std::cout << "core: " << muc.formulae.size() << " of " << n_guarantee_side
              << " guarantee-side formulae\n";
    for (const tlsf::CoreFormula& entry : muc.formulae) {
        std::cout << "[" << tlsf::section_name(entry.section_id) << "] "
                  << entry.formula.to_string() << "\n";
    }
    return 0;
}
