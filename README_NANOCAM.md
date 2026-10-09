English | [简体中文](README_NANOCAM_CN.md)

# Engineering Notes

This project is adapted for the **in-house ESP32-NanoCam module** (ESP32-S3 N16R8, with onboard DVP camera / ES8311 audio / WS2812 RGB).

## 1. System Architecture

```
SO-ARM101 leader arm → USB servo driver board → Ubuntu 22.04 (LeRobot + ROS2 Humble + micro-ROS Agent)
                                      │  2.4 GHz Wi-Fi (same LAN)
                                      ▼
                        ESP32-NanoCam follower arm controller (ESP32-S3)
                                      │  1 Mbps UART servo bus
                                      ▼
                        SO-ARM101 follower arm (6 × STS3215)
```

The PC and the NanoCam must be on the same LAN (2.4 GHz Wi-Fi; a phone hotspot is fine).

## 2. Hardware Adaptation Notes (NanoCam)

### 2.1 Pin Mapping

> The pin definitions come from [docs/reference/nano_config.h](./docs/reference/nano_config.h) and the hardware schematic documentation.

| Peripheral | Pin |
|---|---|
| Servo bus (UART1) | **TX=GPIO20 (P2-8 ESP_P), RX=GPIO19 (P2-7 ESP_N)** ← module P2 header |
| Debug serial (UART0) | GPIO43/44 → onboard CH340K → USB-C (no native USB CDC) |
| Camera DVP (GC2145) | D0~D7=GPIO4/2/1/3/5/7/8/10, PCLK=6, VSYNC=13, HREF=11, XCLK=9 (24MHz), PWDN=12, RESET=14, SCCB SDA/SCL=41/42 |
| Audio ES8311 (I2S) | MCLK=39, BCLK=38, WS=47, DIN(ADC)=40, DOUT(DAC)=48; I2C SDA/SCL=41/42, address 0x30 |
| Microphone | AP2718AT analog MEMS (via ES8311 ADC) |
| Speaker | NS4150B class-D amplifier (via ES8311 DAC), no PA enable pin on board |
| RGB | WS2812 @ GPIO18 (1 unit, GRB, RMT-driven) |
| BOOT | GPIO0 |

### 2.2 Differences from Earlier Versions

| Item | This project (ESP32-NanoCam) |
|---|---|
| Board definition | Custom `boards/nano_cam.json` (16MB Flash / 8MB Octal PSRAM, qio_opi) |
| Servo bus | Serial1/UART1, TX=20/RX=19 (UART0 is taken by the CH340K debug port) |
| Debug serial | UART0 (43/44) → CH340K → USB-C |
| Camera | NanoCam DVP GC2145 (GPIO1~14 + 41/42), XCLK 24MHz |
| Audio | ES8311 + AP2718AT microphone + NS4150B speaker (new) |
| RGB | WS2812 status LED (new) |
| micro-ROS library | xtensa-lx7 — the NanoCam is also an ESP32-S3, so it shares the library with the S3 version |
| PC-side scripts | Unchanged (tools/, start_soarm_demo.sh are hardware-independent) |

### 2.3 Servo Bus Wiring (Important)

The NanoCam's UART0 is taken by the onboard CH340K (USB debug serial), so the servo bus runs on
**UART1, mapped to the module P2 header's expansion serial port**. The follower arm connects
**through the UART pins on the servo driver board**:

```text
Servo driver board UART:   RX ←── NanoCam TX (P2-8 / GPIO20)
                           TX ──→ NanoCam RX (P2-7 / GPIO19)
                          GND ──→ NanoCam GND
```

- **TX connects to RX and RX connects to TX (crossover)**, GND shared, 1 Mbps baud rate;
- The servo bus shares GND with the servo power supply (the follower arm's 12V 5A supply).

### 2.4 Power Supply Notes

| Device | Power |
|---|---|
| ESP32-NanoCam | **Powered via USB data cable** (the CH340K debug serial works at the same time) |
| Follower arm (6×STS3215) | **12V 5A** external supply |
| Leader arm (connected to the Ubuntu PC) | **5V 6A** external supply |

> ⚠️ USB cannot power 6 servos; the follower arm must use a 12V 5A external supply. The ESP32 is fine running off a USB data cable.

### 2.5 Header Paths and build_flags

The micro-ROS header tree is flat (`include/<pkg>/<header>.h`), so only the `-Ilib/microros/include` root path is kept. **Do not** add per-package `-Ilib/microros/include/<pkg>/` paths — that resolves `<string.h>` to `rosidl_runtime_c/string.h` and the WiFi library's `<Client.h>` to `rcl/Client.h`, causing build failures.

## 3. Onboard Peripherals at a Glance

Full functionality once the firmware is powered on:

| Feature | Implementation | Notes |
|---|---|---|
| micro-ROS teleoperation | `main.cpp` + `servo_bus.cpp` | `/joint_states` at 20Hz, receives `/joint_command`, safety mechanisms unchanged |
| Camera FPV | `camera_stream.cpp` | `http://<IP>/stream` MJPEG stream (QVGA) |
| Microphone | `audio_es8311.cpp` | Ambient volume level → `/follower_audio/level` (Float32, 5Hz) |
| Speaker | `audio_es8311.cpp` | Boot/ready/unlock/error tones |
| RGB status LED | `rgb_status.cpp` | Boot red → WiFi orange → micro-ROS green → unlock blue; WiFi lost red |

### RGB Status Meanings

| Color | Status |
|---|---|
| Red | Booting / micro-ROS init failed / WiFi lost |
| Orange | WiFi connected, waiting for micro-ROS Agent |
| Green | micro-ROS ready (teleoperation link up) |
| Blue | Servo control unlocked (ARMED) |
| Purple | Control command rejected (handshake/limit/step mismatch) |

### Speaker Tones

- Power-on: two short "beep beep" (boot tone)
- micro-ROS ready: rising two-tone
- Servo unlock: rising two-tone
- Init failed: one low tone

## 4. Quick Start

### 1. Calibrate Both Robot Arms (on Ubuntu 22.04)

First install ROS 2 Humble, LeRobot (with Feetech support), micro-ROS Agent, and PlatformIO.
Then use the USB bus servo driver board to calibrate the follower arm and leader arm separately:

```bash
python -m lerobot.scripts.lerobot_calibrate \
  --robot.type=so101_follower \
  --robot.port=/dev/ttyACM0 \
  --robot.id=follower_recal \
  --robot.calibration_dir="$PWD/cali"

python -m lerobot.scripts.lerobot_calibrate \
  --teleop.type=so101_leader \
  --teleop.port=/dev/ttyACM0 \
  --teleop.id=leader_recal \
  --teleop.calibration_dir="$PWD/cali"
```

### 2. Write the Follower Arm Calibration into the Firmware

Open `firmware/nanocam_soarm/src/servo_bus.cpp` and replace the three arrays `kHomingOffsets` / `kRangeMin` / `kRangeMax` with the values from your own `cali/follower_recal.json` (order: shoulder_pan, shoulder_lift, elbow_flex, wrist_flex, wrist_roll, gripper).

> If you later swap out the follower arm, you need to recalibrate, update the firmware arrays, recompile, and reflash.

### 3. Configure WiFi and Flash

**Option 1: Compile-time configuration (default)**

```bash
cd firmware/nanocam_soarm
cp src/wifi_config.example.h src/wifi_config.h
# Edit wifi_config.h: WIFI_SSID / WIFI_PASS / AGENT_IP (the LAN IP of the Ubuntu PC)

python3 -m platformio run
python3 -m platformio run --target upload
python3 -m platformio device monitor --baud 115200
```

**Option 2: Serial command configuration (recommended, no reflash needed)**

The firmware has built-in runtime configuration (stored in NVS); enter these on the debug serial (115200, `pio device monitor`) at any time:

```text
wifi_ssid:MyWiFi       Set the WiFi name and save
wifi_pass:MyPassword   Set the WiFi password and save
agent_ip:192.168.1.100 Set the micro-ROS Agent IP and save
wifi_show              Show the currently active configuration
wifi_clear             Clear saved configuration, restore compile-time defaults
```

Any settings command reboots automatically 3 seconds after saving to take effect. Priority: **serial-saved configuration > compile-time defaults**. To switch hotspots or computers, just plug in USB and type three commands — no code changes or reflashing needed.

> Note: the compile-time defaults in `wifi_config.h` are still kept (fallback when nothing has been configured over serial); `wifi_show` distinguishes "from NVS" and "compile-time default". The password is stored in plaintext in NVS, which is acceptable for a LAN demo scenario.

> The NanoCam's USB interface is a CH340K-to-UART0, so under Linux the device name is usually `/dev/ttyUSB0` (or `/dev/serial/by-id/...CH340*`); PlatformIO detects it automatically (the board json already configures HWID 0x1A86:0x7523). Serial monitor baud rate is 115200.

A successful boot log should contain:

```
Servo Ping mask: 0x3f (expected 0x3f)
Servo calibration match: YES
audio: ES8311 ready @24000Hz
IP: 192.168.x.x  RSSI: -xx
Waiting for micro-ROS Agent...
```

### 4. Start the micro-ROS Agent and Verify

```bash
source /opt/ros/humble/setup.bash
snap run micro-ros-agent udp4 --port 8888
```

Once `micro-ROS ready` appears on the NanoCam serial (RGB turns green, speaker plays a tone):

```bash
ros2 topic echo /joint_states --once
ros2 topic hz /joint_states          # should be about 20 Hz
ros2 topic echo /follower_audio/level --once   # microphone level
```

### 5. Wireless Leader-Follower Teleoperation

```bash
export SOARM_WIFI_SSID="YOUR_2G4_WIFI_SSID"
export SOARM_AGENT_IP="YOUR_UBUNTU_PC_IP"
export SOARM_LEADER_PORT="/dev/ttyUSB*"
export SOARM_PYTHON="$(command -v python)"

./start_soarm_demo.sh --check   # pre-flight check
./start_soarm_demo.sh           # start teleoperation, Ctrl+C to stop
```

Safety mechanisms: servo identity check, EEPROM calibration check, current pose handshake (0.05 rad), soft limits, per-command step of 0.25 rad, feedback watchdog 0.5 s, Wi-Fi 10 s recovery timeout restart.

## 5. Rebuilding libmicroros.a (ESP32-S3 / xtensa-lx7)

> This project's `firmware/nanocam_soarm/lib/microros/` already ships the ESP32-S3 static library (the NanoCam is an ESP32-S3, so the library is shared). **Skip this section for normal use.** You only need to rebuild it if you require a custom micro-ROS configuration (message types, QoS, memory pool, etc.).

### Option A: Official Docker builder (recommended, runs on any machine)

The generation script for micro-ROS's official `micro_ros_arduino` library ships with an **esp32s3 target**:

```bash
git clone -b humble https://github.com/micro-ROS/micro_ros_arduino.git
cd micro_ros_arduino
docker pull microros/micro_ros_static_library_builder:humble
docker run -it --rm -v $(pwd):/project \
  --env MICROROS_LIBRARY_FOLDER=extras \
  microros/micro_ros_static_library_builder:humble -p esp32s3
```

The artifacts are at `src/esp32s3/libmicroros.a`, and the headers are under each package directory in `src/`:

```bash
cp src/esp32s3/libmicroros.a <project>/firmware/nanocam_soarm/lib/microros/
# Replace the headers as a whole (keep the three custom files in that
# directory: default_transport.cpp / wifi_transport.cpp /
# micro_ros_arduino.h)
rsync -a src/* <project>/firmware/nanocam_soarm/lib/microros/include/ \
  --exclude esp32s3 --exclude '*.cpp' --exclude micro_ros_arduino.h
```

> **About the toolchain**: The official script's esp32s3 section compiles with the `xtensa-esp32-elf` (LX6) toolchain by default; LX6/LX7 are instruction-set compatible for ordinary C code and it runs fine. The `libmicroros.a` shipped with this project was compiled with the **genuine LX7 toolchain** (`xtensa-esp32s3-elf` gcc 8.4.0, the same version built into PlatformIO). The procedure: download `xtensa-esp32s3-elf-gcc8_4_0-esp-2021r2-patch5-linux-amd64.tar.gz` (Espressif crosstool-NG releases), extract it, change the `TOOLCHAIN_PREFIX` of the esp32s3 section in `library_generation.sh` to `/uros_ws/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-`, then mount it into the container and rerun:

> ```bash
> docker run --platform linux/amd64 -it --rm \
>   -v $(pwd):/project \
>   -v <extract_dir>/xtensa-esp32s3-elf:/uros_ws/xtensa-esp32s3-elf \
>   --env MICROROS_LIBRARY_FOLDER=extras \
>   microros/micro_ros_static_library_builder:humble -p esp32s3
> ```
>
> Note: on Apple Silicon you must add `--platform linux/amd64` (the esp32 toolchain bundled in the image is an x86_64 binary and cannot execute inside an arm64 container).

### Option B: Ubuntu 22.04 + ROS 2 Humble + PlatformIO toolchain

1. Make sure PlatformIO has already downloaded the S3 toolchain (running `pio run` once in the firmware directory is enough):

   ```bash
   ls ~/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc
   ls ~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32s3
   ```

2. Use micro_ros_setup to fetch the micro-ROS source (matching the `/tmp/firmware/mcu_ws` layout in `build_microros.sh`):

   ```bash
   mkdir -p /tmp/firmware && cd /tmp/firmware
   git clone -b humble https://github.com/micro-ROS/micro_ros_setup.git src/micro_ros_setup
   # After installing micro_ros_setup dependencies:
   source /opt/ros/humble/setup.bash
   colcon build && source install/local_setup.bash
   ros2 run micro_ros_setup create_firmware_ws.sh generate_lib
   ```

3. Run this project's S3 build script:

   ```bash
   cd <project>/firmware/nanocam_soarm
   chmod +x build_microros_s3.sh
   ./build_microros_s3.sh
   ```

   The script has already changed riscv32 → xtensa-esp32s3, `-march=rv32imc` → `-mlongcalls`, and the `esp32c3` SDK → the `esp32s3` SDK. The artifacts can be copied into the project as indicated at the end of the script.

## 6. Camera Video Streaming (FPV)

Once the firmware powers on and connects to the network, it automatically starts an MJPEG streaming service (onboard GC2145, DVP interface):

```text
http://<NANOCAM_IP>/         Info page
http://<NANOCAM_IP>/jpg      Single JPEG frame (snapshot)
http://<NANOCAM_IP>/stream   Continuous MJPEG stream (FPV)
```

> Default HTTP port **80** (firmware `HTTPD_DEFAULT_CONFIG()`).

### Parameters

- Resolution **QVGA 320×240** (production configuration), **RGB565 capture + `frame2jpg` software encoding** (the GC2145 has no hardware JPEG; only the OV2640/OV5640 do), quality 12, double-buffered in **8MB Octal PSRAM**
- **Why QVGA**: in testing, VGA (640×480) RGB565 has too high a data rate on this board's DVP — roughly the bottom 2/3 of the image is garbled (reproduced across XCLK 24/20/16MHz × single/double buffer combinations); QVGA is complete and smooth
- The stream runs in a separate httpd task (stack raised to 16KB to accommodate software encoding), and does not interfere with micro-ROS teleoperation or audio capture
- To change resolution/quality: edit `config.frame_size` / `kJpegQuality` in `firmware/nanocam_soarm/src/camera_stream.cpp`; adjust image orientation with `set_vflip` / `set_hmirror` (same file)
- If camera initialization fails, the firmware prints one line and continues working normally (teleoperation is unaffected)
- **Note**: esp_http_server is single-task, so `/stream` and `/jpg` cannot be accessed simultaneously (with the stream open, `/jpg` will hang)

### PC-Side Receive (Publish as a ROS 2 Topic)

```bash
# Terminal 1: start teleoperation as usual
./start_soarm_demo.sh

# Terminal 2: receive video and publish the topic
source /opt/ros/humble/setup.bash
python3 tools/follower_camera.py --stream http://<NANOCAM_IP>/stream
# Optional: --topic /custom_topic  --max-fps 10

# Verify
ros2 topic hz /follower_camera/image_raw/compressed   # should be about 10~15 Hz
rviz2    # Add → By topic → Camera, select /follower_camera/image_raw/compressed
```

You can verify the link without installing ROS first: open `http://<NANOCAM_IP>/stream` in a browser, or `curl -s http://<NANOCAM_IP>/jpg -o snap.jpg`.

## 7. Audio and RGB Status LED

### Microphone (AP2718AT → ES8311 ADC)

The firmware reads the microphone volume level every 200ms (RMS, normalized 0~1) and publishes it to `/follower_audio/level` (`std_msgs/Float32`, best-effort). You can implement your own use for voice activity detection, ambient monitoring, or as a simple trigger signal for "capture when someone speaks".

```bash
ros2 topic echo /follower_audio/level
```

### Speaker (ES8311 DAC → NS4150B)

The firmware includes four tone sets (boot/ready/unlock/error, see Section 3). To customize the tones, modify the `play_tone()` calls in `firmware/nanocam_soarm/src/audio_es8311.cpp`. Volume is at ES8311 register 0x32 (`R_DAC32 = 0xFF`; the current firmware already sets it to maximum).

### Audio Notes

- Sample rate 24 kHz, 16-bit, stereo slots (matching the NanoCam factory firmware), MCLK = 256×FS = 6.144 MHz
- **MCLK is generated by LEDC** (GPIO39, 80MHz÷13≈6.154MHz, error 0.16% within tolerance): the legacy I2S driver does not output MCLK on the ESP32-S3, which causes no speaker sound + a constant 0 microphone level; this is fixed with LEDC in `start_ledc_mclk()` in `audio_es8311.cpp`
- ES8311 control goes over I2C1 (the physical bus on GPIO41/42 is shared with the camera SCCB; the camera only uses SCCB at startup, so there is no runtime conflict); `Wire1.end()` at the end of `init()` releases I2C for the camera
- The default microphone gain is the same as NanoCam factory (register 0x16 = 0x24); to increase sensitivity, adjust the `R_ADC16` value in `audio_es8311.cpp`

## 8. Troubleshooting Points That Differ from the Original

- **Stuck at `Waiting for micro-ROS Agent...`**: first check AGENT_IP / UDP 8888 / client isolation. The NanoCam's U.FL antenna is on the module; if RSSI is poor, check the antenna and placement first.
- **No serial output after flashing**: the NanoCam's USB is CH340K → UART0, so under Linux the device name is `/dev/ttyUSB0`; if it is not recognized when plugged in, check the USB cable and the CH340 driver (in-kernel).
- **Servo bus unresponsive (servo_mask≠0x3f)**: confirm the servo bus is connected through the servo driver board's UART to **P2-7/P2-8** (GPIO19/20) rather than UART0's 43/44; the follower arm must have a 12V 5A external supply (USB cannot power it).
- **Confusing the servo bus with the debug serial**: the debug serial is USB-C (CH340K→UART0), completely independent of the servo bus, and both can be used at the same time.
- **Microphone level stays 0**: check the `audio: ES8311 ready` log; confirm MCLK is being output (GPIO39 should have ~1.65V, generated by LEDC); I2C bus pull-ups on 41/42 (there is already a 10K on board); blow at the microphone and see whether `/follower_audio/level` moves.
- **No speaker sound**: confirm the NS4150B speaker is connected to the speaker connector; confirm GPIO39 MCLK has output (LEDC, `start_ledc_mclk()`); the volume register `R_DAC32` (currently 0xFF); when ES8311 is not initialized, the log prints the failure reason.
- **RGB LED not lit**: the WS2812 data pin is GPIO18; check whether the firmware boot log shows an RMT initialization error before `camera_stream` (it generally does not).

## 9. Test Results (Expected)

- All six follower servos recognized (`servo_mask=0x3f`)
- `/joint_states` published at about 20 Hz
- Main control bridge publishes commands at 30 Hz
- Camera stream `http://<IP>/stream` QVGA smooth
- `/follower_audio/level` published at 5 Hz, with the level rising noticeably when speaking
- RGB status LED changes progressively through boot → network → ready → unlock
- Still runs after unplugging the USB data cable (with the ESP32 independently powered and the follower arm externally powered at 12V)

## 10. Maintenance Convention (Code Changes Must Update Docs)

**Rule: any code change that alters user-visible behavior must update the relevant documentation in the same commit.**

| What you changed                                         | Documentation that must be updated in sync                     |
| ------------------------------------------------------ | ------------------------------------------------------------ |
| `src/main.cpp` (topic names/frequency/state machine/RGB colors) | The "ROS Interface" and "Onboard Peripherals" sections of `firmware/nanocam_soarm/README.md` |
| `src/servo_bus.cpp` (servo pins/calibration/bus parameters) | The firmware README differences table + the pin table/wiring section of `README_NANOCAM.md` |
| `src/camera_stream.cpp` (resolution/orientation/port) | Section 6 (FPV) of `README_NANOCAM.md` |
| `src/audio_es8311.cpp` (tones/gain/volume) | Section 7 (Audio) of `README_NANOCAM.md` |
| `platformio.ini` / `boards/nano_cam.json` (board/dependencies) | The firmware README hardware adaptation table |
| `tools/`, `start_soarm_demo.sh` (PC side) | The corresponding commands in Sections 4/6 of `README_NANOCAM.md` |
| Adding/removing a ROS topic or message type | The "ROS Interface" table in the firmware README (changing a message type also requires rebuilding libmicroros.a) |
| On-board verification passed / pitfall notes / new feature landed | Create `docs/soarm_project_progress_<date>.md` and append a progress record |

**Other conventions:**

- Commit format: `git add -A && git commit -m "one-line description of the change and why" && git push`
- Important versions verified on hardware should be noted in the commit message (e.g. "verified on hardware") as a rollback point
- `wifi_config.h` and `.pio/` are never committed (already in .gitignore)
- Unverified hardware assumptions are marked "pending on-board verification" in code comments

## 11. References

- [NanoCam hardware reference documentation (schematic/datasheet/pin definitions/ES8311 driver)](./docs/reference/)
- [micro-ROS](https://micro.ros.org/) / [micro_ros_arduino](https://github.com/micro-ROS/micro_ros_arduino)
- [LeRobot](https://github.com/huggingface/lerobot)
