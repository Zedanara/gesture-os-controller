
# gesture-os-controller


> **Touchless Windows control via a ToF laser distance sensor and a procedural C++ host application.**
> Wave your hand. Change the volume. Skip the track. Lock the screen. No touch required.

---

## Abstract

`gesture-os-controller` is a Windows host application written in **pure procedural C++** that transforms a plug-and-play ESP32 ToF sensor into a touchless human-interface device. The ESP32 acts as a dumb telemetry streamer — it continuously broadcasts raw ASCII distance measurements over a USB COM port and nothing more. All intelligence lives in the host application: a robust byte-stream parser feeds a fixed-size ring buffer, a median filter removes sensor noise, and a deterministic Finite State Machine classifies hand movements into discrete gestures. Each recognized gesture is dispatched to a Win32 API action — media control, volume adjustment, or workstation lock — with no third-party frameworks involved at any layer.

The project was developed as the coursework submission for **Programowanie Proceduralne (Procedural Programming in C++)** and deliberately avoids object-oriented design patterns. Every module is expressed as a `struct` paired with free functions that operate on it by pointer — making data flow, ownership, and state transitions fully explicit and auditable.

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [Hardware Requirements](#hardware-requirements)
3. [Repository Structure](#repository-structure)
4. [Build Instructions](#build-instructions)
5. [Running the Application](#running-the-application)
6. [Gesture Reference Guide](#gesture-reference-guide)
7. [Procedural Design Notes](#procedural-design-notes)
8. [Troubleshooting](#troubleshooting)
9. [License](#license)

---

## System Architecture

### Overview

The application is a single-threaded, cooperative pipeline. There is no RTOS, no thread pool, and no event loop framework. The main loop calls each stage in sequence on every iteration; stages that have no new data return immediately without blocking.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          HARDWARE LAYER                                     │
│                                                                             │
│   ┌──────────────────────────────────┐                                      │
│   │  ESP32-DEVKIT + Waveshare        │                                      │
│   │  TOF Laser Range Sensor (B)      │  Pre-flashed firmware — read-only.   │
│   │                                  │  Streams ASCII telemetry at ~50 Hz.  │
│   │  Measurement rate : ~50 Hz       │                                      │
│   │  Range            : 0 – 2000 mm  │                                      │
│   │  Interface        : USB → COM    │                                      │
│   └────────────────┬─────────────────┘                                      │
└────────────────────│────────────────────────────────────────────────────────┘
                     │  USB / Virtual COM Port
                     │  115 200 baud, 8N1
                     │
                     │  Raw byte stream (ASCII):
                     │  ...YY284T28308EYY285T28328EYY291T28348E...
                     │       ───┬───  ───┬────               │
                     │      dist(mm)  timestamp(ms)      end marker 'E'
                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                     C++ HOST APPLICATION  (Windows)                         │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  STAGE 1 — COM PORT READER                  com_port.cpp / .h       │    │
│  │                                                                     │    │
│  │  Win32 API: CreateFile, ReadFile, SetCommState, SetCommTimeouts     │    │
│  │  Fills rx_buf[512] with raw bytes per ReadFile call.                │    │
│  │  Non-blocking: ReadIntervalTimeout = 10 ms.                         │    │
│  └──────────────────────────────┬──────────────────────────────────────┘    │
│                                 │  raw bytes  (0 – 512 per cycle)           │
│                                 ▼                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  STAGE 2 — PACKET PARSER                  packet_parser.cpp / .h    │    │
│  │                                                                     │    │
│  │  4-state byte FSM:                                                  │    │
│  │  WAIT_Y1 → WAIT_Y2 → READ_DIST → READ_TIME ──► emit TofReading      │    │
│  │      ▲________↑___________↑___________↑                             │    │
│  │      └── self-heal: any unexpected byte resets to WAIT_Y1 ──────┘   │    │
│  │                                                                     │    │
│  │  Output: TofReading { dist_mm: u16, timestamp_ms: u32, valid: bool }│    │
│  └──────────────────────────────┬──────────────────────────────────────┘    │
│                                 │  TofReading (one per valid packet)        │
│                                 ▼                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  STAGE 3 — RING BUFFER + MEDIAN FILTER    ring_buffer.cpp / .h      │    │
│  │                                                                     │    │
│  │  Fixed array: TofReading data[64]  (power-of-2, no malloc)          │    │
│  │  rb_push()  : head = (head + 1) & 63   ← branchless wrap            │    │
│  │  rb_peek()  : index 0 = newest sample                               │    │
│  │  rb_median3(): median of 3 newest dist_mm  ← noise suppression      │    │
│  │  rb_velocity(): Δdist / Δtime  using ESP32 hardware timestamps      │    │
│  └──────────────────────────────┬──────────────────────────────────────┘    │
│                                 │  smoothed_dist_mm, velocity_mm_s          │
│                                 ▼                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  STAGE 4 — GESTURE FSM                   gesture_engine.cpp / .h    │    │
│  │                                                                     │    │
│  │  States (GestureState enum):                                        │    │
│  │                                                                     │    │
│  │   IDLE ──(dist < 150mm)──► APPROACH ──(held 800ms)──► HOLD          │    │
│  │    ▲                           │                         │          │    │
│  │    │                    (fast retract)            (held 3000ms)     │    │
│  │    │                           ▼                         │          │    │
│  │    │                      RETRACT_1                 LONG_HOLD       │    │
│  │    │                           │                         │          │    │
│  │    │                   (dist < 150mm)              [fire 0x21]      │    │
│  │    │                           ▼                         │          │    │
│  │    │                      APPROACH_2 ──(fast retract)──►            │    │
│  │    │                           │       DOUBLE_WAVE [fire 0x10]      │    │
│  │    │                           │                                    │    │
│  │    │              (vel > 300mm/s, dur < 300ms)                      │    │
│  │    │                      SWIPE [fire 0x30 / 0x31]                  │    │
│  │    │                                                                │    │
│  │    └──────────────── COOLDOWN (700ms) ◄─── all terminal states ────┘     │
│  │                                                                     │    │
│  │  Volume Slider Mode (entered on DOUBLE_WAVE):                       │    │
│  │    dist < 200mm  → volume += 2% per tick                            │    │
│  │    200–350mm     → volume -= 2% per tick                            │    │
│  │    no hand / idle for 4 s → exit mode automatically                 │    │
│  └──────────────────────────────┬──────────────────────────────────────┘    │
│                                 │  GestureEvent { gesture_id, duration_ms } │
│                                 ▼                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  STAGE 5 — WIN32 OS DISPATCHER              os_control.cpp / .h     │    │
│  │                                                                     │    │
│  │  0x20 Short Hold  → SendInput(VK_MEDIA_PLAY_PAUSE)                  │    │
│  │  0x21 Long Hold   → LockWorkStation()                               │    │
│  │  0x30 Swipe In    → SendInput(VK_MEDIA_NEXT_TRACK)                  │    │
│  │  0x31 Swipe Out   → SendInput(VK_MEDIA_PREV_TRACK)                  │    │
│  │  0x10 Double Wave → enter Volume Slider Mode                        │    │
│  │         ↳ IAudioEndpointVolume::SetMasterVolumeLevelScalar()        │    │
│  │                                                                     │    │
│  │  COM objects: raw interface pointers, explicit Release() sequence   │    │
│  │  CoInitialize(NULL) called once in main(); CoUninitialize() on exit │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Telemetry Packet Format

The ESP32 firmware is pre-flashed and locked. It emits a continuous ASCII stream at 115 200 baud with no handshake required. Each measurement is one self-delimited packet:

```
YY[distance_mm]T[timestamp_ms]E

Header ──► YY          (2-byte sync marker; parser re-syncs here on noise)
Distance ► [0–9]+      (decimal mm, typically 3–4 digits)
Separator► T
Timestamp► [0–9]+      (decimal ms, hardware counter from ESP32 boot)
Terminator► E

Valid example:   YY284T28308E     distance = 284 mm,  time = 28308 ms
Invalid (range): YY4095T99000E    dist > 2000 mm → rejected by parser validator
Invalid (parse): YY??T28308E      non-digit in distance field → parser resets
```

The parser accepts nothing on faith. Every packet goes through range validation (`0 < dist ≤ 2000`) before being admitted to the ring buffer.

---

## Hardware Requirements

| Component | Details | Notes |
|---|---|---|
| **ESP32-DEVKIT V1** | Pre-flashed with TOF telemetry firmware | Do not reflash — firmware is locked |
| **Waveshare TOF Laser Range Sensor (B)** | VL53L1X chip, I2C, already wired to ESP32 | Assembled in 3D-printed housing |
| **USB Cable** | USB-A to Micro-USB or USB-C (check your board) | Provides power and COM port |
| **Windows PC** | Windows 10 or 11, x64 | Windows 7/8 not tested |
| **Free COM port** | Assigned automatically by Windows | Identify in Device Manager |

### Physical Setup

```
   ┌───────────────────────────┐
   │   3D-Printed Enclosure    │
   │  ┌──────────┐             │
   │  │  TOF     │◄──────────────   Point toward hand / workspace
   │  │  Sensor  │  laser beam │    Optimal range: 5 cm – 150 cm
   │  └────┬─────┘             │    Mount at desk edge, facing user
   │       │ I2C               │
   │  ┌────▼─────┐             │
   │  │  ESP32   │             │
   │  │  DEVKIT  │             │
   │  └────┬─────┘             │
   └───────│───────────────────┘
           │ USB
           ▼
      Windows PC
   (gesture_host.exe)
```

Place the sensor at the edge of your desk, facing the space where your hand naturally rests. The sweet spot for detection is **5 cm – 120 cm** from the sensor face. Avoid positioning it where sunlight or reflective surfaces fall directly on the lens.

---

## Repository Structure

```
gesture-os-controller/
│
├── host/
│   ├── src/
│   │   ├── main.cpp              # Entry point, main loop, AppState init
│   │   ├── types.h               # All structs, enums, constants — single source of truth
│   │   ├── com_port.cpp          # Win32 COM: CreateFile / ReadFile / SetCommTimeouts
│   │   ├── com_port.h
│   │   ├── packet_parser.cpp     # Byte-FSM: YY...T...E → TofReading
│   │   ├── packet_parser.h
│   │   ├── ring_buffer.cpp       # Circular buffer, median filter, velocity
│   │   ├── ring_buffer.h
│   │   ├── gesture_engine.cpp    # 8-state gesture FSM + Volume Slider Mode
│   │   ├── gesture_engine.h
│   │   ├── os_control.cpp        # Win32 API: SendInput, IAudioEndpointVolume, LockWorkStation
│   │   └── os_control.h
│   └── CMakeLists.txt
│
├── docs/
│   ├── architecture.md           # Detailed module descriptions and FSM diagram
│   ├── protocol_spec.md          # Full ESP32 packet format specification
│   └── test_results.md           # Gesture detection accuracy measurements
│
├── .gitignore
├── LICENSE
└── README.md                     # This file
```

> **No `firmware/` directory.** The ESP32 firmware is pre-flashed by the university and is not part of this codebase. This project is entirely a Windows host application.

---

## Build Instructions

### Prerequisites

Install the following tools and ensure they are available on your system `PATH`:

| Tool | Version | Download |
|---|---|---|
| **MinGW-w64** (g++) | ≥ 13.0 | [winlibs.com](https://winlibs.com) — select UCRT, POSIX threads, Win32 |
| **CMake** | ≥ 3.20 | [cmake.org/download](https://cmake.org/download/) |

Verify your environment before building:

```
g++ --version
cmake --version
```

Both commands must print a version number. If either fails, add the tool's `bin/` folder to your `PATH` in System Environment Variables.

### Build Steps

```bash
# 1. Clone the repository
git clone https://github.com/<your-username>/gesture-os-controller.git
cd gesture-os-controller

# 2. Enter the host project directory
cd host

# 3. Create and enter the build directory
mkdir build
cd build

# 4. Generate MinGW Makefiles
cmake .. -G "MinGW Makefiles"

# 5. Compile
cmake --build .
```

A successful build produces:

```
build/
└── gesture_host.exe
```

### What Gets Linked

The `CMakeLists.txt` links the following Windows system libraries explicitly:

```cmake
target_link_libraries(gesture_host PRIVATE ole32 winmm)
```

| Library | Purpose |
|---|---|
| `ole32` | COM subsystem — required for `IAudioEndpointVolume` (volume control) |
| `winmm` | Windows Multimedia — required for media key timing |

No other dependencies. No vcpkg. No NuGet. No third-party headers.

### Compiler Flags

```cmake
target_compile_options(gesture_host PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -std=c++17
    -O2
)
```

The build is warning-clean at `-Wall -Wextra`. Any warning is treated as a sign of a real problem and should not be suppressed.

---

## Running the Application

### Step 1 — Find Your COM Port

1. Connect the ESP32 via USB.
2. Open **Device Manager** (`Win + X` → Device Manager).
3. Expand **Ports (COM & LPT)**.
4. Note the port name — e.g., `COM3`, `COM7`, `COM12`.

If no port appears, install the **CP210x** or **CH340** USB-UART driver depending on your ESP32 board revision.

### Step 2 — Run

```bash
# From the build/ directory
gesture_host.exe COM3
```

Replace `COM3` with your actual port number.

### Expected Console Output

```
[INFO]  Opened COM3 at 115200 baud
[INFO]  Listening for gestures... (Press ESC to exit)
[PARSE] dist=284mm  ts=28308ms
[PARSE] dist=285mm  ts=28328ms
[FSM]   IDLE → APPROACH  dist=142mm  ts=31450ms
[FSM]   APPROACH → HOLD  ts=32290ms  (held 840ms)
[ACTION] PLAY_PAUSE  (gesture=SHORT_HOLD, confidence=94%)
[FSM]   HOLD → COOLDOWN
[FSM]   COOLDOWN → IDLE
```

### Exiting

Press **ESC** at any time. The application closes the COM port cleanly before exiting, ensuring no "port in use" error on the next launch.

For Ctrl+C (e.g., in a script), the registered `SetConsoleCtrlHandler` closes the port and calls `CoUninitialize()` before the process terminates.

---

## Gesture Reference Guide

All gestures are performed by moving your **open hand** in front of the sensor. No special grip or orientation is required. The sensor measures the distance to the nearest reflective surface — your palm is the target.

### Detection Zones

```
        Sensor face
             │
     ◄───────┼────────────────────────────────────────►  distance
             │
          0 cm      10 cm     20 cm     30 cm     40 cm+
             │         │          │          │         │
             │  ██████ │ ░░░░░░░░ │ ░░░░░░░░ │         │
             │  DANGER │  NEAR    │  MID     │   FAR   │
             │  (<5cm) │ (<20cm)  │ (20-35cm)│ (>35cm) │
             │  avoid  │  holds   │  volume  │  idle   │
             │         │          │   zone   │         │
```

---

### Gesture 1 — Short Hold → Play / Pause Media

**What it does:** Sends `VK_MEDIA_PLAY_PAUSE` to the system — pauses or resumes whatever media is playing (Spotify, YouTube, VLC, etc.).

**How to perform:**

```
Time ──────────────────────────────────────────────────►

   hand          ████████████████
   in NEAR zone  ├──────────────┤
   (<20 cm)      ~0.8s to ~2.5s

                 └── hold still, then remove hand
```

1. Move your hand into the **NEAR zone** (closer than 20 cm to the sensor).
2. Hold it steady for approximately **1 second**.
3. Remove your hand.
4. The FSM fires on release when hold duration is between 800 ms and 2500 ms.

**Tip:** The gesture fires when you **remove** your hand, not when you enter. You will hear/see media respond within ~80 ms of pulling your hand away.

---

### Gesture 2 — Long Hold → Lock Workstation

**What it does:** Calls `LockWorkStation()` — immediately locks Windows and shows the login screen.

**How to perform:**

```
Time ──────────────────────────────────────────────────►

   hand          ████████████████████████████████
   in NEAR zone  ├───────────────────────────────┤
   (<20 cm)      ~3 seconds or more

                 └── hold until you hear lock sound
```

1. Move your hand into the **NEAR zone** (closer than 20 cm).
2. Hold it steady for at least **3 seconds**.
3. The FSM fires the event at the 3-second threshold — you do not need to remove your hand first.

**Tip:** Keep your hand reasonably still. The median filter tolerates small jitter, but drifting in and out of the NEAR zone resets the hold timer.

---

### Gesture 3 — Fast Swipe → Next Track / Previous Track

**What it does:** Sends `VK_MEDIA_NEXT_TRACK` (swipe toward sensor) or `VK_MEDIA_PREV_TRACK` (swipe away). Also advances slides in presentation software (PowerPoint, LibreOffice Impress).

**How to perform:**

```
Time ──────────────────────────────────────────────────►

   SWIPE IN (Next):                SWIPE OUT (Prev):
   ─────────────────               ─────────────────
       ██                                      ██
      ├──┤                                    ├──┤
      <0.3s  fast approach                <0.3s  fast retreat
      vel > 300 mm/s                      vel < -300 mm/s
```

**Swipe In (Next Track):**
1. Start with your hand far from the sensor (> 35 cm).
2. Move it quickly toward the sensor, passing through the NEAR zone.
3. Total motion should complete in under 300 ms.

**Swipe Out (Previous Track):**
1. Start with your hand close to the sensor (< 20 cm).
2. Move it quickly away, past the FAR threshold.
3. Total motion should complete in under 300 ms.

**Tip:** Speed is what distinguishes a swipe from a hold. Move decisively — a slow approach will be interpreted as the beginning of a hold gesture, not a swipe.

---

### Gesture 4 — Double Wave → Volume Slider Mode

**What it does:** A double wave (two quick in-out motions) enters **Volume Slider Mode** — a continuous volume control mode where hand position directly maps to volume direction.

#### Step 1 — Entering Volume Slider Mode

**How to perform:**

```
Time ──────────────────────────────────────────────────────────────►

          ████    ████
          ├──┤    ├──┤
          in  out  in  out         total sequence < ~1 second
          wave 1   wave 2

   dist:  ─────────────────────────────────────────────────────────
   FAR    ─ ─ ─ ┐   ┌─ ─ ─ ┐   ┌─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
   NEAR         └───┘      └───┘
```

1. With hand starting far (> 35 cm), move quickly into NEAR zone (< 20 cm) — **wave 1 in**.
2. Immediately retract back past FAR zone — **wave 1 out**.
3. Immediately move back into NEAR zone — **wave 2 in**.
4. Immediately retract again — **wave 2 out**.
5. Complete both cycles within approximately 1 second total.

The console will confirm:

```
[FSM]   DOUBLE_WAVE detected — entering Volume Slider Mode
[MODE]  Volume Slider: ACTIVE (4s timeout)
```

#### Step 2 — Controlling Volume in Slider Mode

Once inside Volume Slider Mode, the sensor reads your hand position continuously:

```
        Sensor
           │
        ────────────────────────────────────────────────►
           │
       0 cm│  10 cm    20 cm    25 cm    35 cm    40 cm+
           │   │           │        │          │        │
           │   │  ▲▲▲▲▲▲▲  │        │ ▼▼▼▼▼▼▼▼ │        │
           │   │  VOL UP   │  DEAD  │ VOL DOWN │  EXIT  │
           │   │  +2%/tick │  ZONE  │ -2%/tick │  zone  │
           │   │  ~50ms    │        │  ~50ms   │ (idle) │
```

| Hand position | Effect |
|---|---|
| Closer than **20 cm** | Volume increases **+2% every tick** (~50 ms) |
| Between **20 cm and 25 cm** | Dead zone — volume holds steady |
| Between **25 cm and 35 cm** | Volume decreases **−2% every tick** (~50 ms) |
| Beyond **35 cm** or no hand | Inactivity counter starts |

Volume is clamped to [0%, 100%] — it will not overflow or underflow.

Console feedback in slider mode:

```
[VOLUME] 63% → 65%  (hand at 142mm, NEAR zone)
[VOLUME] 65% → 63%  (hand at 298mm, MID zone)
```

#### Step 3 — Exiting Volume Slider Mode

Volume Slider Mode exits automatically. No explicit gesture is needed.

```
Exit condition: no hand detected in active zones for 4 consecutive seconds.
```

```
[MODE]  Volume Slider: TIMEOUT — returning to IDLE
```

You can also exit immediately by performing any other gesture (the COOLDOWN state that follows will bypass slider mode).

---

### Gesture Quick Reference

| Gesture | How to Perform | Duration | Action |
|---|---|---|---|
| **Short Hold** | Hand steady at < 20 cm | 0.8 s – 2.5 s | Play / Pause |
| **Long Hold** | Hand steady at < 20 cm | ≥ 3 s | Lock Workstation |
| **Swipe In** | Fast approach to sensor | < 0.3 s | Next Track |
| **Swipe Out** | Fast retreat from sensor | < 0.3 s | Previous Track |
| **Double Wave** | Two quick in-out motions | < 1 s total | Enter Volume Slider Mode |

---

## Procedural Design Notes

This section exists to make the architectural decisions explicit for academic review.

### Why Structs + Functions, Not Classes

Every module in this project follows the same pattern:

```cpp
// A module is: one struct that owns its data...
typedef struct {
    uint16_t data[RING_BUF_SIZE];
    uint8_t  head;
    uint8_t  count;
} RingBuffer;

// ...and free functions that operate on it by pointer.
void     rb_init  (RingBuffer* rb);
void     rb_push  (RingBuffer* rb, const TofReading* r);
uint16_t rb_median3(const RingBuffer* rb);
int32_t  rb_velocity(const RingBuffer* rb);
```

This is functionally equivalent to a class with methods, but the ownership and mutation contract is visible at every call site. `rb_push(&buf, &reading)` tells the reader exactly what is being modified. `buf.push(reading)` hides it.

### Why a Single Global `AppState`

```cpp
// main.cpp
static AppState g_app;
```

There is exactly one global variable. All modules receive pointers into `g_app` as parameters — they do not access it directly. This means every function is testable in isolation by constructing a local `AppState` in a test harness, without any global state interference.

### Why ESP32 Timestamps, Not `GetTickCount()`

The gesture FSM uses `TofReading.timestamp_ms` (embedded in the ESP32 packet) as its time source, not the Windows clock. This eliminates timing error introduced by `ReadFile` latency, `Sleep()` imprecision, and Windows scheduler jitter. The sensor samples at a fixed 50 Hz hardware rate; its timestamps are ground truth for gesture duration calculations.

### Ring Buffer Bitwise Modulo

```cpp
rb->head = (rb->head + 1) & (RING_BUF_SIZE - 1);
```

`RING_BUF_SIZE` is 64 (a power of two). Masking with `63` is mathematically equivalent to modulo 64 but compiles to a single `AND` instruction with no branch. This is documented explicitly because it only works for power-of-two sizes — a constraint enforced by `static_assert` at compile time.

```cpp
static_assert((RING_BUF_SIZE & (RING_BUF_SIZE - 1)) == 0,
              "RING_BUF_SIZE must be a power of 2");
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `[ERROR] Cannot open COM3 — code 2` | Wrong port number | Check Device Manager → Ports (COM & LPT) |
| `[ERROR] Cannot open COM12 — code 5` | Port held by another app | Close Arduino IDE Serial Monitor, other terminal tools |
| Gesture never fires, FSM stays in IDLE | Sensor out of range or facing wrong direction | Bring hand within 120 cm; check sensor is facing you |
| Swipe detected instead of Hold | Hand moving during hold | Keep hand still; rest your wrist on desk edge |
| Volume Slider Mode exits immediately | Hand leaving active zone too fast | Keep hand between 5–35 cm; move slowly after double wave |
| Double Wave not detected | Waves too slow | Complete both in-out motions in under 1 second total |
| `dist=8190mm` in logs constantly | Sensor sees no target or out of range | Move hand closer; check sensor cable connections |
| App compiles but crashes on launch | Missing `ole32` linkage | Verify `CMakeLists.txt` has `target_link_libraries(... ole32 winmm)` |
| Volume control has no effect | No default audio device | Set a default playback device in Windows Sound settings |
| Workstation does not lock | Group Policy restriction | Contact your system administrator |

---

## License

This project is licensed under the **MIT License**. See [`LICENSE`](LICENSE) for full terms.

---

*Developed as coursework for Programowanie Proceduralne — Procedural Programming in C++.*
*All application logic written in procedural C++. No OOP. No frameworks. No shortcuts.*
