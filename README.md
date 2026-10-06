# Welcome to the Razorbotz NASA Robotic Mining Competition Project!
This page intends to provide a starting point and overview of the project, as well as a roadmap for how to get involved with the project even if you aren't familiar with the code or technology stack. Please note that these links may not be up to date and any links should be followed at your own risk.  If you find any links that no longer work or changes that need to be made, please contact me at andrewburroughs17@gmail.com.  Click [here](https://razorbotz.github.io/CPP/) to view the documentation for the project.  If you are not familiar with Github and the git cli, please refer to the [Razorbotz Github Intro page](https://github.com/Razorbotz/Test).

## Overview
* [Documentation](#documentation)
* [Building the Control Program](#building-the-control-program)
* [Running Against the Real Robot](#running-against-the-real-robot)
* [Running in Simulation (No Robot)](#running-in-simulation-no-robot)
* [Flight Engineer Station](#flight-engineer-station)
* [Foxglove 3D Visualization](#foxglove-3d-visualization)
* [Command-Line Reference](#command-line-reference)
* [Controls Reference](#controls-reference)
* [Troubleshooting](#troubleshooting)

---

## Building the Control Program
The control program (`control.cpp`) is the operator station GUI. It shows the robot's video feed with telemetry overlaid, reads joystick/controller input, and sends drive and mechanism commands to the robot.

### Project layout
```
control/
├── CMakeLists.txt
├── control_completion.sh   # bash tab-completion for the command-line flags
├── include/                # headers (InfoFrame.hpp, BinaryMessage.hpp, BotConfig.hpp, ...)
├── src/                    # control.cpp, NetworkHandler.cpp, InputRebindTool.cpp, ...
├── resources/              # Glade layouts, images, config files
└── build/                  # created by you; run the program from here
```

### Dependencies
CMake 3.12+ and a C++17 compiler are required. On Ubuntu/Debian the following installs everything:

```bash
sudo apt install build-essential cmake pkg-config git \
    libgtkmm-3.0-dev libcairo2-dev libsdl2-dev libopencv-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
    libcurl4-openssl-dev zlib1g-dev libssl-dev \
    nlohmann-json3-dev libwebsocketpp-dev libasio-dev
```

| Library | Purpose |
|---|---|
| gtkmm 3 / cairo | GUI windows, widgets, and custom drawing (Glade layouts) |
| SDL2 | Joystick / game controller input |
| OpenCV | Video frame handling |
| FFmpeg (avcodec, avformat, avutil, swscale) | Video stream decoding |
| libcurl | Camera servo HTTP commands |
| zlib | Telemetry compression/decompression |
| OpenSSL | Required by the Foxglove WebSocket library |
| nlohmann/json, websocketpp, asio | Used by the Foxglove WebSocket server |

The Foxglove WebSocket library itself is **downloaded automatically** by CMake (`FetchContent` pulls the `main` branch of [foxglove/ws-protocol](https://github.com/foxglove/ws-protocol)), so the first `cmake` run needs an internet connection. Build once while online before heading to competition.

The CMakeLists also includes header paths for `aarch64` and `armhf`, so the same build works on the Jetsons and Raspberry Pi as well as x86_64 laptops.

### Build
```bash
cd control
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

This produces two executables in `build/`:
* `control` – the operator station GUI
* `input_rebind_tool` – a GUI for creating joystick mapping JSON files used with `--input_config`

> **Important:** Always run the program from the `build/` directory. It loads its layouts, icons, and images from `../resources/` (e.g. `mainLayout.glade`, `feLayout.glade`, `sensorsLayout.glade`, `razorbotz.png`). Running it from anywhere else will fail with `CRITICAL: Failed to load layout glade`.

### Tab-completion for flags (optional)
`control_completion.sh` adds bash tab-completion for all of the command-line flags. Load it in your current shell:

```bash
source ../control_completion.sh
```

To have it load in every terminal, add it to your `~/.bashrc` (use the full path to your checkout):

```bash
echo "source ~/path/to/control/control_completion.sh" >> ~/.bashrc
```

Then type `./control --` and press `Tab` twice to list the options. Completion is registered for `./control`, so it applies when you run the program from `build/` as shown above. After a flag that takes a file (`--config_file`, `--input_config`), `Tab` completes file names.

If you add a new flag to `processArguments()` in `control.cpp`, add it to the list in `control_completion.sh` too.

---

## Running Against the Real Robot

### Network setup
The robot has two onboard computers. Connect the operator laptop to the robot's network (192.168.0.x) before launching.

| Device | Default IP | Notes |
|---|---|---|
| Jetson Orin | `192.168.0.6` | Primary robot connection and default video source |
| Jetson Nano | `192.168.0.5` | Second robot connection |
| Flight Engineer laptop | `192.168.0.4` | Receives forwarded telemetry/video (see below) |
| Camera servo controllers | `192.168.1.8` / `192.168.1.9` | HTTP control, switch with keys `1` / `2` |

The IP fields in the GUI are pre-filled with these values and can be edited before connecting. Robots that are broadcasting on the network will also appear in the address lists automatically.

### Launching
Pick the flag for the robot you are driving:

```bash
./control                 # Primary bot (default)
./control --backup_bot    # Backup bot
./control --dump_bot      # Dump bot
```

Then, in the top bar:
1. Click **Connect** for the Orin (and the Nano's connect button if that computer is in use).
2. Click **Connect** in the video section, then **Stream** to start the camera feed.
3. Plug in the joystick(s) or Xbox controller **before** launching — controllers are only detected at startup.

If no packets arrive for 5 seconds, the connection is marked as timed out and the telemetry widgets are greyed out as stale.

### Windows that open
* **Main window** – video with overlaid motor status indicators, speedometers, gear dial, arm/bucket position, bucket tilt and height, attitude indicator, and battery bar.
* **Sensors window** – motor status dashboard (health cards, per-motor table, autonomy, network, and power budget panels) plus a Diagnostics tab for the ESP32 handheld tool. With three monitors it is placed on the third screen automatically; otherwise it is maximized.

Double-click a motor indicator to open a detailed dial view for that motor.

### Flight Engineer forwarding
By default the pilot station forwards telemetry and video to the Flight Engineer laptop at `192.168.0.4`:

| Bot | Telemetry port | Video port |
|---|---|---|
| Primary | 5001 | 5010 |
| Dump | 5002 | 5011 |
| Backup | *(no default forwarding)* | |

Override with `--forward <IP> <telemetry_port> <video_port>`, or turn it off with `--no_forward`.

---

## Running in Simulation (No Robot)
You can run and test the GUI on any machine without a robot connected.

### Network Simulator
```bash
./control --simulate
```
This pre-populates the GUI with all of the active bot's telemetry frames (same as `--init`) and opens a **Network Simulator** window. Choose a message type (e.g. `Talon 1`, `Kraken 2`, `Zed`, `Communication`), edit any field values, and click **Send Message** — the message is fed straight into the GUI as if it came from the robot. This is the quickest way to test new widgets, warning thresholds (e.g. set `Bus Voltage` below 12 V), and layout changes.

Combine it with a bot flag to simulate that bot's motor set:
```bash
./control --simulate --dump_bot
./control --simulate --backup_bot
```

Note: `Bus Voltage` and `Output Current` are entered in volts/amps; the simulator scales them ×100 the same way the robot does.

### Testing controls without a robot
```bash
./control --test_input
```
Normally the main loop idles until a robot or video connection is active. `--test_input` keeps joystick and keyboard handling running so you can verify controller mappings with nothing connected.

### Running on WSL / localhost
```bash
./control --wsl
```
Sets the robot IPs to `127.0.0.1` (Orin) and `127.0.0.2` (Nano) so you can connect to robot software running on the same machine, and disables the camera servo HTTP commands.

### Encode tool
```bash
./control --encode_tool
```
Opens the **BinaryMessage Encode Tool**, which shows the hex bytes for a chosen message encoded with string labels vs. FieldStrings labels, optionally with checksum and the zlib server envelope, along with the size savings and an optional decoded preview. Useful when changing the telemetry protocol.

### Other useful development flags
* `--init` – show all telemetry frames immediately with zeroed values, instead of waiting for data to arrive.
* `--no_video` – replace the video area with a plain grid of sensor values.
* `--debug_glade_bounds` – draw red outlines and Glade IDs on every widget for layout debugging.
* `--debug_motors` – print every motor packet's contents to the console.
* `--disable_foxglove` – don't start the Foxglove server (avoids port conflicts when running two copies).

---

## Flight Engineer Station
The Flight Engineer (FE) station is a second laptop that passively monitors both robots without controlling them.

```bash
./control --fe        # or --flight_engineer
```

It loads `feLayout.glade` and listens for forwarded data from the pilot stations on ports **5001/5002** (telemetry for Robot 1/Robot 2) and **5010/5011** (video). The FE laptop must be at the IP the pilots forward to (`192.168.0.4` by default). FE mode never forwards data itself.

The dashboard shows per-robot connection status and latency, ESP32 diagnostics, a combined motor telemetry grid, navigation (roll/pitch/yaw/X/Y), autonomy, lidar, and communication status, plus both video feeds.

**Mission timer:** `T` starts/pauses, `R` resets. Set the length with `--mission_time <seconds>` (default 600 = 10 min). It turns orange at 2 minutes left and red at 1 minute.

---

## Foxglove 3D Visualization
The control program runs a Foxglove WebSocket server on **`ws://<laptop-ip>:8765`** (the console message currently says 8766 — 8765 is the actual port). Open [Foxglove Studio](https://foxglove.dev/), connect to that address, and add a 3D panel. The `/tf` topic publishes `world → base_link → Arm → Bucket` and the four wheel frames, using offsets from the active bot's URDF (`sierra.urdf`, `dump_bot.urdf`, or `my_robot_tf.urdf`), so the robot model moves with live position and arm/bucket angles.

---

## Command-Line Reference
Run `./control --help` for the built-in list.

| Flag | Description |
|---|---|
| `--backup_bot` / `--dump_bot` | Select robot configuration (default is the primary bot) |
| `--nano` | Use the Jetson Nano connection instead of the Orin |
| `--init` | Initialize the GUI with zeroed telemetry frames |
| `--simulate` | Open the Network Simulator (implies `--init`) |
| `--encode_tool` | Open the BinaryMessage Encode Tool (implies `--init`) |
| `--test_input` | Process controller input without a robot connection |
| `--wsl` | Use localhost IPs and disable servo HTTP commands |
| `--no_video` | Show a sensor grid instead of the video area |
| `--fe`, `--flight_engineer` | Run as the Flight Engineer station |
| `--forward <IP> <tel_port> <vid_port>` | Forward telemetry/video to an FE station |
| `--no_forward` | Disable default FE forwarding |
| `--mission_time <s>` | FE mission timer length in seconds (default 600) |
| `--battery_capacity <Ah>` | Battery size for the power budget estimate (default 18) |
| `--max_bandwidth <MB/s>` | Full-scale value of the bandwidth bar (default 5) |
| `--input_config <file>` | Load a JSON joystick mapping (made with `./input_rebind_tool`, built alongside `control`) |
| `--alt_layout` | Alternate joystick axis mapping |
| `--set_colors "#light" "#dark"` | Override the light and dark background colors |
| `--config_file <file>` | Load display settings from `../resources/<file>` and open the config editor. **Must be the last flag** — arguments after it are ignored |
| `--disable_foxglove` | Don't start the Foxglove server |
| `--debug_glade_bounds` | Draw widget outlines and Glade IDs |
| `--debug_motors` | Print motor packet contents to the console |

---

## Controls Reference

### Keyboard
| Key | Action |
|---|---|
| `M` | Toggle light/dark mode |
| `U` / `I` / `O` / `P` | Camera servo left / right / up / down (stops on release) |
| `1` / `2` | Switch servo controller to `192.168.1.8` / `192.168.1.9` |
| `Shift` + `+` / `-` | Gear up / down |
| `T` / `R` | FE mission timer start-pause / reset (FE mode only) |

Other key presses and releases are forwarded to the robot.

### Joysticks and Xbox controller
Two flight joysticks are detected automatically. If an Xbox-style controller is connected it uses this layout:

| Input | Action |
|---|---|
| Left stick Y | Left wheels |
| Right stick Y | Right wheels |
| Left trigger / Left bumper | Bucket down / up |
| Right trigger / Right bumper | Arm down / up |
| A / B / X / Y | Reserved for macros (currently log only) |

Axis values inside a ±4000 dead zone are sent as 0 (configurable through `--input_config`). Axis commands are sent at 20 Hz.

---

## Troubleshooting
* **`Failed to load layout glade`** – you are not running from `build/`; the program needs `../resources/`.
* **Controller not responding** – it was plugged in after launch; restart the program.
* **"Orin connection timed out"** – no packets for 5 s; check the robot network and that the robot software is running.
* **FE station shows "Disconnected"** – confirm the FE laptop's IP matches the pilot's forward target and that the pilot did not launch with `--no_forward`.
* **Foxglove port already in use** – another copy of the program is running; launch one with `--disable_foxglove`.

---

## Documentation
This project uses [Doxygen](https://www.doxygen.nl/index.html) to generate documentation for the files automatically.  To learn more about the Doxygen formatting, please refer to the [Documenting the code](https://www.doxygen.nl/manual/docblocks.html) section of the Doxygen docs.  The documentation for this project can be found at the project website that is found [here](https://razorbotz.github.io/CPP/).

### Documentation Template
To standardize the documentation across multiple authors, the following documentation template will be used throughout the project.  To see an example of how files should be commented to generate the documentation correctly, see [Example.cpp](https://github.com/Razorbotz/ROS2/blob/master/docs/Example.cpp).

**Files**
* Description of file
* Topics subscribed to
* Topics published
* Related files

**Functions**
* Description of Function
* Parameters
* Return values
* Related files and/or functions