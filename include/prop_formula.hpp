#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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
    enum class Kind {
        Atom,
        Not,
        And,
        Or,
        Implies,
        Iff,
    };

    using RewriteCallback =
        std::function<std::optional<Formula>(const Formula&)>;

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

    /// Creates an atomic formula from an identifier.
    /// @param atom The atom name
    /// @return     A formula containing a single atom
    static Formula make_atom(const std::string& atom);

    /// Creates a unary formula.
    /// @param kind  Unary operator kind (currently only Kind::Not)
    /// @param child Operand formula
    /// @return      A formula of the form op(child)
    static Formula make_unary(Kind kind, const Formula& child);

    /// Creates a binary formula.
    /// @param kind  Binary operator kind
    /// @param left  Left operand
    /// @param right Right operand
    /// @return      A formula of the form left op right
    static Formula make_binary(Kind kind, const Formula& left,
                               const Formula& right);

    /// Returns the kind of this formula's root node.
    Kind kind() const;

    /// Returns this formula's atom name if it is atomic.
    /// @return std::nullopt for non-atomic formulae
    std::optional<std::string> atom_name() const;

    /// Returns this formula's unary child if its root is unary.
    /// @return std::nullopt for non-unary formulae
    std::optional<Formula> unary_child() const;

    /// Returns this formula's binary children if its root is binary.
    /// @return std::nullopt for non-binary formulae
    std::optional<std::pair<Formula, Formula>> binary_children() const;

    /// Rewrites this formula using a post-order callback.
    /// Children are rewritten before their parent; if the callback returns a
    /// replacement, that replacement is used for the current subtree.
    /// @param rewrite_callback Callback that can replace a subtree
    /// @return                 The rewritten formula
    Formula rewrite_post_order(const RewriteCallback& rewrite_callback) const;

    /// Converts the formula to DIMACS CNF format for use with SAT/model
    /// counters. Uses Tseitin encoding to transform the formula into CNF.
    /// @return A string in DIMACS format (p cnf <vars> <clauses> followed by
    /// clauses)
    std::string to_dimacs() const;

    /// Computes a symmetric, normalized syntactic similarity score between
    /// this formula and another.
    /// Uses shared_subformulae(other) with multiplicity and returns
    /// 0.5 * (shared / n_subformulae() + shared / other.n_subformulae()).
    /// If either formula has zero subformulae, returns 1.0.
    /// @param other The formula to compare with
    /// @return A similarity score in [0, 1]
    double syntactic_similarity(const Formula& other) const;

    /// Returns the total number of subformulae (nodes) in this formula,
    /// including the root and all proper subformulae.
    /// @return The count of all nodes in the AST
    size_t n_subformulae() const;

    /// Returns the number of subformulae shared with another formula.
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
