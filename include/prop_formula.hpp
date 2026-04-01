#pragma once

#include <memory>
#include <string>
#include <vector>

/// A propositional formula represented as a parse tree. Supports standard
/// boolean operators (¬, ∧, ∨, →, ↔) and variable atoms. Formulae can be
/// converted to DIMACS CNF format for SAT/model counting, and their syntactic
/// structure can be analyzed for similarity metrics in repair algorithms.
///
/// Uses the PImpl pattern to hide implementation details (internal AST node
/// representation) from the public interface.
class Formula {
   public:
    /// Default constructor creates a formula representing the logical constant
    /// "true" (implemented as a single variable named "⊤").
    Formula();

    /// Constructs a Formula by parsing a string representation.
    /// Supports operators: ! or ~ (negation), & (and), | (or), -> (implies),
    /// <-> (iff). Variables are alphanumeric identifiers (including _).
    /// @param formula A string representation of the propositional formula
    /// @throws std::invalid_argument if the formula is malformed or empty
    explicit Formula(const std::string& formula);

    Formula(const Formula& other);
    Formula(Formula&& other) noexcept;
    Formula& operator=(const Formula& other);
    Formula& operator=(Formula&& other) noexcept;
    ~Formula();

    /// Converts the formula to DIMACS CNF format for use with SAT/model
    /// counters. Uses Tseitin encoding to transform the formula into CNF.
    /// @return A string in DIMACS format (p cnf <vars> <clauses> followed by
    /// clauses)
    std::string to_dimacs() const;

    /// Computes the syntactic similarity between this formula and another.
    /// Counts the number of shared subformulae (including the root) based on
    /// structural signature matching, with multiplicity for repeated
    /// subformulae.
    /// @param other The formula to compare with
    /// @return The number of structurally identical subformulae
    double syntactic_similarity(const Formula& other) const;

    /// Returns the total number of subformulae (nodes) in this formula,
    /// including the root and all proper subformulae.
    /// @return The count of all nodes in the AST
    size_t n_subformulae() const;

    /// Returns the number of subformulae shared with another formula.
    /// This is equivalent to syntactic_similarity(other).
    /// @param other The formula to compare with
    /// @return The number of shared subformulae
    size_t shared_subformulae(const Formula& other) const;

    /// Converts the formula back to its string representation.
    /// Returns a string that, when parsed as a new Formula, produces an
    /// equivalent formula (minus whitespace and parentheses variations).
    /// @return A string representation of the formula
    std::string to_string() const;

   private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
