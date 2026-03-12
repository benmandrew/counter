#include <Eigen/Dense>
#include <iostream>

int main() {
    Eigen::Matrix2d matrix;
    matrix << 1.0, 2.0, 3.0, 4.0;

    Eigen::Vector2d vector(5.0, 6.0);
    const Eigen::Vector2d result = matrix * vector;

    std::cout << "Matrix:\n" << matrix << "\n\n";
    std::cout << "Vector:\n" << vector << "\n\n";
    std::cout << "Result:\n" << result << '\n';

    return 0;
}
