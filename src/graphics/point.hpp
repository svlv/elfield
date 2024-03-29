#pragma once

#include "types.hpp"

namespace elfield
{

struct point {
    point(double x_, double y_);
    point(const point& other);
    point& operator+=(const point& coord);
    point& operator-=(const point& coord);

    void move(const point& coord);
    void rotate(angle_t angle);

    friend point operator+(const point& lhs, const point& rhs);
    friend point operator-(const point& lhs, const point& rhs);
    friend bool operator<(const point& lhs, const point& rhs);

    double module() const;
    angle_t get_phi(const point& coord) const;

    double x;
    double y;
};

} // namespace elfield
