#include "charge.hpp"
#include "constants.hpp"
#include <cmath>
#include <iostream>
namespace elfield
{

charge::charge(const point& coord, double value) : _coord(coord), _value(value)
{
}

charge::charge(const charge& chrg) : _coord(chrg._coord), _value(chrg._value) {}

const point& charge::get_coord() const { return _coord; }

point& charge::get_coord() { return _coord; }
double charge::get_value() const { return _value; }
double& charge::get_value() { return _value; }

void charge::set_coord(const point& coord) { _coord = coord; }
void charge::set_value(double value) { _value = value; }

} // namespace elfield
