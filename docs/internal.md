# counter internals

counter is a C++17 *genetic algorithm* that repairs unrealisable FRETISH and
TLSF specifications. Candidate repairs are ranked by *bounded model counting*:
SPOT builds the automaton, Ganak counts the models it accepts, and the ratio
gives a semantic distance from the original specification. This page is the
landing page of the internal reference.

That build covers `include/` and `src/` together. Private, static and
anonymous-namespace members are all extracted, source listings are inlined into
the documentation, and every function carries a call graph and a caller graph.
The [public API reference](../index.html) is a different build over `include/`
alone — a curated surface for code that consumes counter. Anything below the
headers is visible only here.

[Files](files.html) is the best entry point. The code is organised by file and
by subsystem rather than by type, so the file list maps onto the design more
directly than the type list does. Under `src/`, the top-level subsystems are
`fitness/`, `filter/`, `genetic/`, `runner/` and `tlsf/`. `runner/` is the one
that leaves the process: it wraps the external tool subprocesses `ltl2tgba`,
`ltlsynt`, `black` and `ganak`, plus a Node FRET formaliser.

[Classes](annotated.html) lists the types, and [Namespaces](namespaces.html)
gives the namespace tree.

The call graphs are what this extra build buys. Headers show the intended shape
of a subsystem; the caller edges show how far it actually reaches.
