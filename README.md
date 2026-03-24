# advaita
Simple breathing companion app for M5Stack Cardputer with visual & aural cues, developed 99% by Antigravity.

## features
2 breathing patterns "square", "triangle" to exercise:

- Square Breathing Pattern: 4 sec. inhale > 4 sec. hold > 4 sec. exhale > 4 sec. hold
- Triangle Breathing Pattern: 4 sec. inhale > 7 sec. hold > 8 sec. exhale

additional "combo" preset for seamless transition from "square" to "triangle"

2 themes with different visuals & sounds:
- monochrome (default)
- LED (with random colors)

sound and display brightness level adjustments

auto display off after 30 seconds of inactivity when not exercising (press any key to wake) and, auto shutdown after 5 minutes of inactivity (in juice we trust).

compatible with M5Launcher

## usage
Press 1, 2 or 3 to excercise with the desired preset/pattern

## bindings
- **h:** shows help (just the key bindings)
- **esc:** exit to menu
- **space:** pause/resume
- **a:** switch theme
- **s:** sound on/off
- **d:** display on/off
- **-+:** sound level
- **.;:** display brightness level

## installation
Compile the .ino to .bin with Arduino IDE, or download it directly from M5Burner.
