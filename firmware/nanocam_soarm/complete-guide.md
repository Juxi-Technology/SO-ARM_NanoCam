English | [简体中文](complete-guide_CN.md)

# SO-ARM101 Wireless Teleoperation Project — Complete Run-Through Guide

> For the ESP32-NanoCam version (ESP32-S3 N16R8) follower-arm controller. Covers environment requirements, installation, flashing, calibration, the full run flow and troubleshooting. Intended for two-machine collaboration across Linux (Ubuntu 22.04 recommended) and Windows:
> **You can build and flash on any machine, but the actual teleoperation runs on Ubuntu**.

---

## 1. System Architecture and How It Works

```
SO-ARM101 leader arm → USB servo driver board → Ubuntu 22.04 (LeRobot + ROS2 Humble + micro-ROS Agent)
                                            │  2.4 GHz Wi-Fi (same LAN)
                                            ▼
                              ESP32-NanoCam follower controller (ESP32-S3)
                                            │  1 Mbps UART (routed through the servo driver board UART pins)
                                            ▼
                              SO-ARM101 follower arm 6×STS3215
```

- The operator moves the leader arm → LeRobot reads the leader → ROS2 topic `/joint_command` → micro-ROS Agent
  sends it over UDP 8888 → ESP32-NanoCam receives it and drives the 6 servos;
- The follower feeds back `/joint_states` (20Hz) in the reverse direction, used for closed loop and watchdog;
- The onboard camera publishes an MJPEG stream at `http://<IP>/stream` (FPV), which the PC can convert into a ROS topic.

**Division of roles**: the leader arm connects to the Ubuntu computer; the follower arm is controlled by the ESP32-NanoCam, and the two are linked wirelessly.

---

## 2. Hardware List

| Hardware | Qty | Notes |
|---|---|---|
| SO-ARM101 leader arm | 1 | With 6×STS3215 servos |
| SO-ARM101 follower arm | 1 | With 6×STS3215 servos |
| ESP32-NanoCam module | 1 | ESP32-S3 N16R8, onboard camera/audio/RGB |
| USB servo driver board | 2 | Calibration + leader/follower bus routing (UART pins) |
| Ubuntu 22.04 computer | 1 | Runs LeRobot + ROS2 + Agent |
| 2.4GHz router or phone hotspot | 1 | Leader computer and NanoCam on the same LAN |
| 12V 5A external power supply | 1 | **Powers the follower arm** (USB can't drive 6 servos) |
| 5V 6A external power supply | 1 | **Powers the leader arm** (connected to the Ubuntu computer) |
| USB-C data cable | 2 | NanoCam power/debug + leader driver board to computer |

### Wiring (important)

Between the ESP32-NanoCam and the follower arm, the connection is **routed through the servo driver board UART pins**:

```text
Servo driver board UART:   RX ←── NanoCam TX (P2-8 / GPIO20)
                           TX ──→ NanoCam RX (P2-7 / GPIO19)
                          GND ──→ NanoCam GND
```

- **TX to RX, RX to TX (crossed)**, GND common, 1 Mbps baud rate;
- The NanoCam servo bus uses UART1 (module **P2-7 / P2-8**);
- The debug serial port uses USB-C (CH340K → UART0, `/dev/ttyUSB0` on Linux).

### Power

| Device | Power |
|---|---|
| ESP32-NanoCam | **Powered via USB data cable** (CH340K debug serial also works simultaneously) |
| Follower arm (6×STS3215) | **12V 5A** external power supply |
| Leader arm (connected to Ubuntu) | **5V 6A** external power supply |

---

## 3. Environment Requirements

### 3.1 Build and Flash Environment (Windows or Linux)

| Item | Requirement |
|---|---|
| OS | Windows 10/11 or Linux (macOS also works) |
| Python | 3.8+ (verify with `python --version`) |
| PlatformIO | Core 6.x (with the esp32s3 toolchain + Arduino framework) |
| Disk space | At least 3 GB free |
| Network | Able to reach GitHub / Espressif CDN (first toolchain download is about 1-2 GB) |

### 3.2 Run Environment (Ubuntu 22.04 computer, where teleoperation finally runs)

| Item | Requirement |
|---|---|
| OS | Ubuntu 22.04 (64-bit) |
| ROS 2 | Humble (Hawksbill) |
| LeRobot | With Feetech SO-101 support (`so101_leader` / `so101_follower`) |
| micro-ROS Agent | `snap run micro-ros-agent` or built from source |
| Dependency commands | `nmcli`, `ip`, `flock` (bundled with NetworkManager, iproute2, util-linux) |
| Python environment | `lerobot_so101` virtual environment (conda/miniforge) |

> Debug serial recognition: on Linux the NanoCam is `/dev/ttyUSB0` (the CH340 driver is built into the kernel).

---

## 4. Environment Installation

### 4.1 Install PlatformIO (build and flash side)

**Option A: VSCode extension (recommended)**
1. Install [VSCode](https://code.visualstudio.com/);
2. Search the extension marketplace for **PlatformIO IDE** and install it; it will restart automatically and download PlatformIO Core;
3. Verify: `pio --version` (in the VSCode terminal).

**Option B: Command-line install**
```bash
pip install platformio
```

> On Windows, if the `pio` command isn't found in Git Bash, use a PowerShell/CMD terminal instead, or add
> `C:\Users\<username>\.platformio\penv\Scripts` to PATH.

### 4.2 First Build Automatically Downloads the Toolchain

Enter the firmware directory and run a build once (without flashing):
```bash
cd firmware/nanocam_soarm
pio run
```
The first run downloads, in order:
1. The espressif32 platform (`espressif32@7.0.1`);
2. The **toolchain** `toolchain-xtensa-esp32s3` (about 100 MB, downloaded from the Espressif CDN);
3. The Arduino framework `framework-arduinoespressif32` (about 200 MB).

**Handling slow/stalled downloads**:

- Be patient and watch: PlatformIO's remaining-time estimate is unreliable and often stalls for a while then suddenly jumps to done; give it 5 minutes to see whether the percentage advances;
- Enable a proxy/VPN (using the system proxy);
- Download the toolchain manually: use a browser to download
  `https://dl.espressif.com/dl/xtensa-esp32s3-elf-gcc8_4_0-esp-2021r2-patch5-win32.zip`
  (on Linux the corresponding file is `-linux-amd64.tar.gz`), extract it, rename the directory to `toolchain-xtensa-esp32s3`, place it in `C:\Users\<username>\.platformio\packages\`, and re-run `pio run`.
- Interrupting midway with `Ctrl+C` won't corrupt the environment; re-running will resume.

### 4.3 Install the Ubuntu Run Environment

```bash
# 1. ROS 2 Humble (follow the official docs)
#    https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debs.html
# After installation:
source /opt/ros/humble/setup.bash

# 2. LeRobot (with Feetech support)
conda create -n lerobot_so101 python=3.10 -y
conda activate lerobot_so101
pip install lerobot[feetech]

# 3. micro-ROS Agent
sudo snap install micro-ros-agent
snap run micro-ros-agent udp4 --port 8888   # test whether it starts

# 4. PlatformIO (if you also want to build and flash on the Ubuntu side)
pip install platformio
```

---

## 5. Configure the Firmware and Flash

The micro-ROS static library is located in `lib/microros/`; this project uses the **xtensa-lx7 (ESP32-S3)** version.

### 5.1 Configure WiFi (choose one)

**Option 1: Build-time configuration (default)**
```bash
cd firmware/nanocam_soarm
cp src/wifi_config.example.h src/wifi_config.h
# edit wifi_config.h: WIFI_SSID / WIFI_PASS / AGENT_IP (the Ubuntu computer's LAN IP)
```

**Option 2: Serial command configuration (recommended, no re-flashing needed)**
After flashing, enter via the debug serial port (115200 baud):
```text
wifi_ssid:your hotspot name
wifi_pass:your password
agent_ip:Ubuntu computer IP
wifi_show    # view the currently active configuration
wifi_clear   # clear it and restore the build-time defaults
```
Any setting command **auto-restarts after 3 seconds** to take effect. Priority: serial-saved > build-time default.

### 5.2 Flashing

```bash
cd firmware/nanocam_soarm
pio run --target upload
```

**Entering download mode (critical)**:

- The NanoCam downloads over CH340K → UART0 serial (not USB-CDC auto-download);
- First just run upload; if the board has an auto-download circuit it will succeed directly;
- If it reports that it can't connect: **hold the BOOT button (GPIO0) → plug in USB (or press reset) → release BOOT**, then re-run upload immediately;
- On Windows, if the serial port isn't auto-detected, add a line to `[env:nano_cam]` in `platformio.ini`:
  `upload_port = COM3` (replace with the actual COM number of the CH340 in Device Manager).

**Viewing serial logs**:

```bash
pio device monitor --baud 115200
```

**After flashing you should see (appearing in order)**:
```
audio: ES8311 ready @24000Hz      ← audio initialized successfully
Servo Ping mask: 0x3f             ← all 6 servos online
Servo calibration match: YES      ← calibration arrays match the servo EEPROM
IP: 192.168.x.x  RSSI: -xx        ← WiFi connected
Waiting for micro-ROS Agent...    ← waiting for the Agent (disappears once the next step starts)
```

> The servo bus can be left disconnected while flashing; flashing and servo operation don't interfere with each other (UART0 debug / UART1 servo are independent).

---

## 6. Calibrate Both Robot Arms (on Ubuntu, only when hardware is replaced)

The repo's `cali/` already contains the leader/follower calibration files, and the firmware calibration arrays match the follower calibration. **You only need to recalibrate when you replace the follower/leader hardware**.

```bash
# Follower
python -m lerobot.scripts.lerobot_calibrate \
  --robot.type=so101_follower --robot.port=/dev/ttyACM0 \
  --robot.id=follower_recal --robot.calibration_dir="$PWD/cali"

# Leader
python -m lerobot.scripts.lerobot_calibrate \
  --teleop.type=so101_leader --teleop.port=/dev/ttyACM0 \
  --teleop.id=leader_recal --teleop.calibration_dir="$PWD/cali"
```

After recalibrating the follower, you must also update the
`kHomingOffsets` / `kRangeMin` / `kRangeMax` arrays in
`firmware/nanocam_soarm/src/servo_bus.cpp` (order: shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, wrist_roll, gripper), then recompile and flash.

---

## 7. Run Flow (on Ubuntu)

### 7.1 Pre-Start Checks

```bash
# 1. The Ubuntu computer is connected to the same 2.4GHz WiFi as the NanoCam
# 2. The leader arm's USB servo driver board is connected and recognized
ls -l /dev/ttyACM*   # find the leader serial port
# 3. The follower arm's NanoCam is powered and online (confirm the MJPEG stream is reachable via serial or browser)
```

### 7.2 One-Click Teleoperation Start

```bash
# Set the environment (or directly edit the defaults at the top of start_soarm_demo.sh)
export SOARM_WIFI_SSID="your 2.4G hotspot"
export SOARM_AGENT_IP="Ubuntu computer IP"
export SOARM_LEADER_PORT="/dev/ttyACM*"
export SOARM_PYTHON="$(command -v python)"   # lerobot_so101 environment

./start_soarm_demo.sh --check    # pre-flight check: network/leader/Agent/follower online
./start_soarm_demo.sh            # start teleoperation for real, Ctrl+C to stop
```

The script does the following in order:
1. Check the network (the SSID must match `EXPECTED_WIFI_SSID`), the leader serial port, and calibration file existence;
2. Start the micro-ROS Agent (if not already running, logs go to `logs/micro_ros_agent.log`);
3. Wait for the follower `/joint_states` to come online (15s timeout);
4. Leader motion → follower follows, 30Hz command rate, **`--mapping-mode absolute` (absolute mapping)**.

**About absolute mapping**: the leader pose and follower pose correspond one-to-one in their respective calibration coordinate frames. The advantage is
**no accumulated drift after disconnect/reconnect** — on reconnect the follower smoothly aligns to the leader's current pose within 8 seconds (`startup_blend`), after which the leader returns to zero → the follower also returns to its own zero. We previously used `relative` mapping, but after a disconnect/reconnect the follower stayed at the disconnected position, producing a permanent offset versus the leader that returned to zero, so we switched to absolute.

**About automatic restart on Agent loss** (firmware after 2026-08-19): after Ctrl+C stops teleoperation, the follower auto-restarts within about 10 seconds back to `Waiting for micro-ROS Agent...`. So after Ctrl+C you can directly re-run this script without manually resetting the follower. During the reconnect the follower returns to zero (power-cycles).

### 7.3 Camera FPV (optional)

> The GC2145 has no hardware JPEG encoder, so the firmware uses **RGB565 capture + `frame2jpg` software encoding**. The formal configuration is **QVGA 320×240** (VGA exceeds the data rate on this board's DVP and produces artifacts, see Section 9 troubleshooting). The frame rate is lower than hardware JPEG, which is normal; `/stream` and `/jpg` cannot be accessed simultaneously (httpd is single-task).

```bash
# Terminal 2 (Ubuntu):
source /opt/ros/humble/setup.bash
python3 tools/follower_camera.py --stream http://<NANOCAM_IP>/stream
# Verify
ros2 topic hz /follower_camera/image_raw/compressed    # measured about 10~15 Hz
# You can also verify the link without ROS: open http://<NANOCAM_IP>/stream in a browser
```

### 7.4 Manually Verify Each Topic

```bash
ros2 topic echo /joint_states --once           # follower feedback
ros2 topic hz /joint_states                    # should be about 20 Hz
ros2 topic echo /follower_audio/level --once   # microphone level (rises when you speak)
```

---

## 8. Status LEDs and Prompt Tones

| RGB color | Status |
|---|---|
| Red | Booting / micro-ROS init failed / WiFi lost |
| Orange | WiFi connected, waiting for micro-ROS Agent |
| Green | micro-ROS ready (teleoperation link up) |
| Blue | Servo control unlocked (ARMED) |
| Purple | Control command rejected (handshake/limit/step mismatch) |

Speaker prompt tones: two "beep beep" on power-up; micro-ROS ready/servo unlock: a rising two-tone; init failure: a single low tone.

**Safety mechanisms (built into the firmware, no manual action needed)**: servo identity check, EEPROM calibration check, current-pose handshake (0.05 rad), soft limits, single-command step of 0.25 rad, feedback watchdog 0.5 s, and a 10 s WiFi-loss timeout restart.

---

## 9. Cautions

### Power and wiring
- **The follower arm must be powered externally at 12V 5A**; USB can't drive 6 STS3215s; the servo bus and servo power share a common ground;
- **The leader arm is powered at 5V 6A** (connected to the Ubuntu computer); **the ESP32-NanoCam is powered via the USB data cable**;
- The follower arm's servo bus connects to the servo driver board UART at **P2-7 (RX) / P2-8 (TX)**; do not connect it to UART0's 43/44 (that's the debug serial);
- For flight demos: after inverted mounting you must re-confirm joint directions, center of gravity, the power (BEC) scheme, and run EMI interference tests.

### Communication
- The NanoCam and the Ubuntu computer must be on the **same 2.4GHz LAN** (a phone hotspot is fine); client isolation must be off;
- The NanoCam antenna is on the module (U.FL); if RSSI is poor, first check the antenna and placement; it's recommended to run a 5/10/20/30-meter distance test;
- `wifi_config.h` contains the WiFi password and is excluded by .gitignore — **do not commit it**.

### Build/code
- Code changes must be accompanied by a README update (a project maintenance convention, see the "Maintenance Conventions" comparison table in the repo root README);
- Rebuilding the micro-ROS library is only needed when you change message types/QoS/memory pool (see Section 5 of README_NANOCAM.md); day-to-day development does **not** require recompiling `libmicroros.a`;
- For important versions that pass on-board verification, note "verified on board" in the commit message as a rollback point.

### Troubleshooting quick reference
| Symptom | Check |
|---|---|
| Flashing won't connect | Manually enter download mode (BOOT+reset); add `upload_port` to `platformio.ini` |
| No serial output after flashing | Check the USB cable and CH340 driver; on Windows check the COM port in Device Manager |
| Stuck at `Waiting for micro-ROS Agent...` | Check AGENT_IP / UDP 8888 / client isolation |
| Servo bus not responding (servo_mask≠0x3f) | Confirm the servo driver board UART is connected to P2-7/P2-8; follower arm needs 12V 5A external power |
| Microphone level stuck at 0 | Check the `audio: ES8311 ready` log; I2C 41/42 pull-ups; blow on the mic to verify |
| No speaker sound | Check the speaker connection; ES8311 volume register `R_DAC32` (currently 0x30) |
| WiFi frequently drops | Check the antenna and distance; red RGB means WiFi lost, auto-restart after 10s |

### Camera-specific troubleshooting (on-board measured experience)
| Symptom | Root cause | Fix |
|---|---|---|
| `i2c driver install error` + `camera probe failed` | **I2C conflict**: ES8311 uses `Wire1` occupying GPIO41/42, and the camera SCCB then gets refused when installing the I2C driver | Add `Wire1.end()` at the end of `init()` in `audio_es8311.cpp` to release the I2C for the camera (already fixed in firmware) |
| `JPEG format is not supported on this sensor` (0x106) | **The GC2145 has no hardware JPEG encoder** (only OV2640/OV5640 do) | Switch capture to `PIXFORMAT_RGB565`, and use `frame2jpg` to software-encode to JPEG in `/stream` and `/jpg` (already fixed in firmware) |
| `/jpg`, `/stream` no response, browser spins forever | **httpd stack overflow**: the default 8KB stack can't hold `frame2jpg` software encoding | Set `config.stack_size = 16384` in `start_server()` (already fixed in firmware) |
| `/stream` opens but is black | **Missing multipart boundary**: no `STREAM_BOUNDARY` sent between frames, so the browser can't parse it | Send `STREAM_BOUNDARY` before each frame (already fixed in firmware) |
| curl testing `/jpg` returns `HTTP:000`, but the browser shows an image | esp_http_server is **single-task**: while `/stream` occupies the httpd task, `/jpg` can't get in the queue; or curl's timeout is too short | Close `/stream` and test `/jpg` alone; use a browser instead of curl to verify |
| Camera initializes successfully but is all black / no frames | Usually a **hardware** issue: AVDD/DOVDD power, PWDN level, ribbon cable contact | First test a snapshot with `/jpg` in a browser (an image means the link works); check the camera's 2.8V power and ribbon cable |
| Bottom ~2/3 of the VGA image is garbled | **DVP data rate too high**: VGA RGB565 exceeds this board's DVP sampling timing margin (reproduced at 24/20/16MHz × single/double buffer); QVGA is fine | Use **QVGA 320×240** in the formal configuration (sufficient for FPV), or switch to a more stable XCLK / change the DVP hardware routing |

### Audio-specific troubleshooting (on-board measured experience)
| Symptom | Root cause | Fix |
|---|---|---|
| Speaker **completely silent** + microphone level ≈ 0 (e.g. `0.0009`) | **MCLK not output**: the legacy I2S driver doesn't generate MCLK on the ESP32-S3, so the ES8311's internal DAC/ADC has no clock | Use **LEDC to generate a 6.15MHz MCLK on GPIO39** (`start_ledc_mclk()` in `audio_es8311.cpp`, already fixed in firmware) |
| Prompt tone **too quiet** (only audible with your ear against it) | Low digital amplitude + low ES8311 master volume | `play_tone` amplitude 12000→30000, `R_DAC32` 0x30→0xFF (already fixed in firmware, about +29dB) |
| Only the startup "beep beep" on power-up, no other prompt tones | **Normal behavior**: the ready/unlock prompt tones are event-driven and only triggered once teleoperation runs | Startup tone = played immediately on power-up; ready tone = Agent communication established; unlock tone = control command received |

**Note**: esp_http_server is single-task, so `/stream` and `/jpg` **cannot be accessed simultaneously** — while `/stream` is open, `/jpg` will hang indefinitely. Close the stream page before capturing a single frame.

---

## 10. Current Development Status Reminder

- **Parts that have passed on-board verification** (2026-08-19):
  - ✅ Audio `ES8311 ready @24000Hz` (MCLK output normal + speaker/microphone both working; fixed the missing MCLK + too-low volume, see Section 9)
  - ✅ WiFi connection + micro-ROS communication (`/joint_states` stable at 20Hz, `/follower_audio/level` normal)
  - ✅ GC2145 camera FPV: QVGA `/stream` complete and smooth (fixed the I2C conflict / software encoding / httpd stack / multipart boundary, see Section 9)
  - ✅ Full teleoperation link (leader arm following the follower arm)
  - ✅ **absolute mapping + automatic restart on Agent loss**: after disconnect/reconnect the leader and follower align with no offset; after Ctrl+C the follower auto-restarts and waits to reconnect (see Section 7)
- **Still to be verified**:
  - ⚠️ Flight scenario: inverted-mount orientation, center of gravity, power (BEC), EMI interference
- Detailed documentation: repo root `README.md` (architecture/maintenance conventions), `README_NANOCAM.md` (hardware/peripherals/rebuilding the library),
  and progress records in `docs/soarm_project_progress_*.md`.
