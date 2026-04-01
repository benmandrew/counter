#pragma once

#include <memory>
#include <string>
#include <vector>

class Formula {
   public:
    explicit Formula(const std::string& formula);
    Formula(const Formula& other);
    Formula(Formula&& other) noexcept;
    Formula& operator=(const Formula& other);
    Formula& operator=(Formula&& other) noexcept;
    ~Formula();

    std::string to_dimacs() const;
    double syntactic_similarity(const Formula& other) const;
    size_t n_subformulae() const;
    size_t shared_subformulae(const Formula& other) const;

   private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
