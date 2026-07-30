#include "version.hpp"

#include <ostream>

#include "git_version.hpp"

namespace version {

const char* commit() { return generated::k_commit; }

const char* commit_short() { return generated::k_commit_short; }

bool dirty() { return generated::k_dirty; }

void print(std::ostream& out) {
    out << "commit=" << commit() << "\n"
        << "commit_short=" << commit_short() << "\n"
        << "dirty=" << (dirty() ? 1 : 0) << "\n";
}

}  // namespace version
