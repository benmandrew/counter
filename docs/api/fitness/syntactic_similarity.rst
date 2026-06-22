fitness/syntactic_similarity.hpp
=================================

Computes syntactic similarity between requirements and specifications by
comparing the shared sub-formula structure of their condition and response
formulae. For a pair of requirements the score is a weighted average of the
``Formula::syntactic_similarity`` scores for the two triggers and the two
responses, plus a timing component (1.0 if the timing variants match, 0.0
otherwise). Specification-level similarity conjoins all triggers into a single
formula and all responses into a single formula per specification, then averages
the trigger and response pair similarities. All scores lie in [0, 1].

.. doxygenfile:: syntactic_similarity.hpp
