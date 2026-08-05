#include "version.hpp"

#include <ostream>

// The only translation unit that includes the generated header, and it must
// stay that way. cmake/version.cmake rewrites git_version.hpp whenever the
// commit changes, so a second includer would turn every new commit into a wide
// rebuild instead of recompiling this file alone.
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
