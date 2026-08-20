hash_combine.hpp
================

``hash_combine`` is the single sequence-hash combiner behind every ``std::hash`` specialisation in the codebase — ``Formula``, ``Requirement``, ``Specification``, ``tlsf::Specification`` and the subformula-signature hasher in the syntactic-similarity fold. It exists as one function so the mixing constant cannot drift between hashers, which is what it had done: three of the four hand-written copies mixed a 64-bit seed with a 32-bit constant.

.. doxygenfile:: hash_combine.hpp
