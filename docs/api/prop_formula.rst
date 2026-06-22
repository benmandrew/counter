prop_formula.hpp
================

``Formula`` is the propositional AST used for requirement conditions and
responses. It is stored behind a PImpl pointer so callers never see the internal
node representation. Key operations:

- **Parsing** — ``Formula(string)`` parses operators ``!``/``~``, ``&``, ``|``,
  ``->``, ``<->`` and alphanumeric atom names.
- **Simplification** — ``simplify()`` applies idempotence, tautology, excluded
  middle, and double-negation removal.
- **DIMACS conversion** — ``to_dimacs()`` uses Tseitin encoding to produce CNF
  for SAT/model-counter input.
- **Similarity** — ``syntactic_similarity(other)`` computes a normalised score
  in [0, 1] based on shared sub-formula count; used as a fitness component.
- **Rewriting** — ``rewrite_post_order`` applies a bottom-up callback, used by
  the mutation operator to replace sub-trees.

.. doxygenfile:: prop_formula.hpp
