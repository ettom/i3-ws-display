# i3-ws-display
Display active i3 workspaces on a 7-segment display

A set of programs and configuration files for displaying i3 workspaces on a display
controlled by an Arduino Nano.

## Background
I started using a secondary monitor and I didn't want to waste screen real estate for
a status bar on it. However, peeking at the other monitor to see which workspaces
are active and focused felt awkward. This led me to create this project. The end
result is a 4-digit 7-segment display that shows the active workspaces, mounted on
top of my external monitor. It also shows if a workspace is focused by lighting up
the dot after the corresponding workspace number.

There is no real reason to be limited to a 4-digit display, or in fact - a 7-segment
display. I have workspaces 4-7 mapped to my external display. This setup works quite
well for me.

## Hardware setup
#### You'll need:
* Arduino Nano. Technically any Arduino should work, but then you'll have to rewrite
 the pin mappings/use a library for the 7-segment display.
* 7-segment display module. Mine is labelled HS420561K-32. It's a 4-digit
 common-cathode unit. It's important that there is a dot after every digit so you
  can indicate the currently focused workspace.
* Four 220-ohm resistors and wires to connect it all up

#### Setup
Connect the display to your Arduino and flash the `nano/7seg.ino` file.<br>
Wiring for a common-cathode display with the following pin arrangement (according
to the [typical segment identification](https://commons.wikimedia.org/wiki/File:7_Segment_Display_with_Labeled_Segments.svg#/media/File:7_Segment_Display_with_Labeled_Segments.svg)):<br>
**Top row** - 1, a, f, 2, 3, b <br>
**Bottom row** - e, d, DP, c, g, 4

![Fritzing schematic](https://github.com/ettom/i3-ws-display/blob/master/nano/fritzing.png)


## Software setup
### Dependencies
```
cmake 3.11+
pkgconf
g++
libserial
jsoncpp
sigc++
```

This project also uses the [i3ipcpp](https://github.com/drmgc/i3ipcpp) library
which is pulled and built automatically by cmake.

##### Installing the dependencies for Ubuntu 18.04
Get the latest version of [cmake](https://apt.kitware.com/).<br>
Then run the following command:

`sudo apt install pkgconf g++ libjsoncpp-dev libsigc++-2.0-dev libserial-dev`

Installing `libserial-dev` might give you a linker error, the
solution is to uninstall it and build [libserial](https://github.com/crayzeewulf/libserial) from
source.

### Building
Run the following commands:
```
git clone https://github.com/ettom/i3-ws-display.git
cd i3-ws-display
mkdir build && cd build
cmake ..
cmake --build .
sudo cp -f app/ws_display /usr/local/bin/ws_display
```


### Configuration
Configure `~/.config/ws_display.json` to match your setup.
* `output` -  If you want the 7-segment display to show workspaces only for a
  specific output, set it here. The name of the output should be the same as is shown by
  `xrandr`.
* `display_length` - Number of digits on your 7-segment display.
* `serial_port` - Serial port your Arduino is connected to.

You'll probably want to write an udev rule for the Arduino. Otherwise it might not
always get bound to the same `/dev` node.<br>
Unfortunately the Arduino Nano does not have a serial id, so it's difficult to write
an udev rule for it. I found that the easiest way was to tie it to a USB
hub port. This works as long as the Arduino is always plugged into the same USB
port.<br>
I won't go into writing udev rules here, see the `97-ws_display.rules` file
for an example. I found [this](https://unix.stackexchange.com/a/326708) post helpful.

If the Arduino will always be connected to your machine you could (and should) just
start `ws_display` from your i3 config.

I have a laptop and a docking station to which the Arduino is connected to so I'm
using a systemd user service for starting and stopping `ws_display`. The systemd user
service starts `ws_display` as soon as the Arduino becomes available. The service
also depends on a custom `i3.target` which gets started by i3:

`exec_always --no-startup-id systemctl --user start i3.target`

I also put `systemctl --user stop i3.target` in a script that runs when i3 exits. See the
`systemd/ws_display.service` and `systemd/i3.target` files.
