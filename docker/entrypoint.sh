#!/bin/sh
# Entry point for the counter image. Dispatches the first argument over the
# binaries the image installs, so `docker run <image> realize spec.tlsf` reads
# the same as the command table in README.md rather than needing a path.
set -eu

k_commands="counter realize ltl compare mucs maximal lint-ideals signal_tracer"

usage() {
    cat <<USAGE
counter — genetic repair of unrealisable reactive specifications

Usage: docker run --rm -v "\$PWD:/work" <image> <command> [options]

Commands:
  counter --input <spec> --output-dir <dir>   repair an unrealisable specification
  realize <spec>...                           report whether a specification is realisable
  ltl <spec>...                               print the LTL a specification translates to
  compare --repairs <dir> --ideals <dir>      compare repairs against known-ideal ones
  mucs <spec.tlsf>                            extract a minimal unrealisable core
  maximal <dir-or-file>...                    report which specifications are maximal

Any command takes --help for its own options. An argument that is not a command
runs as it stands, so \`bash\` and \`sh\` reach a shell.

The bundled examples are at ${COUNTER_EXAMPLES:-unset}:

  docker run --rm <image> realize ${COUNTER_EXAMPLES:-<examples>}/lily02/spec.tlsf

\$COUNTER_EXAMPLES names that directory inside the container, so a command
using it has to be quoted and re-entered by a shell in there:

  docker run --rm <image> sh -c 'realize \$COUNTER_EXAMPLES/lily02/spec.tlsf'

Repairs are written relative to the working directory, so bind mount /work to
keep them. Size the run with --cpus and --memory: the scoring pool follows the
cgroup CPU quota, and ltlfilt has been measured peaking near 3.4GB.
USAGE
}

case "${1:-}" in
    '' | -h | --help | help)
        usage
        exit 0
        ;;
esac

for _command in ${k_commands}; do
    if [ "$1" = "${_command}" ]; then
        shift
        exec "${_command}" "$@"
    fi
done

# A leading option is counter's own, so `docker run <image> --input spec.tlsf`
# works without naming the command twice.
case "$1" in
    -*) exec counter "$@" ;;
esac

exec "$@"
