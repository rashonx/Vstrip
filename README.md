# Wheel Bridge

A small Windows app by Dodge City Plugins. It turns a Moza wheel (built for the R5) into a virtual
Xbox 360 controller for games with weak wheel support, like Need for Speed Most Wanted, and holds a
constant centring force on the wheel the whole time you play.

- Reads the wheel, pedals and buttons with DirectInput
- Creates a virtual Xbox 360 controller with [ViGEmBus](https://github.com/nefarius/ViGEmBus)
- Hides the real wheel from games with [HidHide](https://github.com/nefarius/HidHide)
- Drives the wheel's motor directly: centring spring, damper, game rumble, plus v1.1 force feedback effects: crash kicks, road texture, speed weight and kickback

Installs its own helper drivers (ViGEmBus + HidHide, built in) with one click and hides the real wheel from games automatically while running.

See `SETUP.txt` for step-by-step setup. Builds come from GitHub Actions (Actions tab ->
latest "Build Wheel Bridge" run -> WheelBridge-Windows artifact).
