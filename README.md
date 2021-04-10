# Electric field simulator. Easy to install - easy to demonstrate.
Simple graphic electric field simulator written on C++ using GTK+.
<br />
<img src="https://github.com/asuvalov/electrodynamics-simulator/blob/master/.screenshots/highlight_arrow.png" alt="drawing" width="400"/>

# Build
```
git clone git@github.com:asuvalov/electrodynamics-simulator.git
cd electrodynamics-simulator
mkdir build && cd build
cmake ..
make
```
# Features
* Drawing arrows to demonstrate electric field direction:
 * It's possible to show or hide the arrows.
 * Hightlighting arrows under the cursor with the corresponding line.
* Drawing electric field lines to demonstrate interaction between positive and
negative charges:
 * It's possible to show or hide the lines.
 * Moving a charge under the cursor.


# Mouse events
| Event        | Action           |
| ------------- |:-------------:|
| Press right button | Add positive charge|
| Press left button | Add negative charge|

# Keybindings
| Key        | Action           |
| ------------- |:-------------:|
| `q`      | Quit simulator |
| `c`      | Remove all charges |
| `a`      | Draw / hide arrows |
| `l`      | Draw / hide lines |
