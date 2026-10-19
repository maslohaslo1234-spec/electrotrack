# Electrotrack

![Electrotrack PCB — 3D view](https://github.com/user-attachments/assets/24169744-645e-483c-90ab-85a70d674799)

Electrotrack is an experimental electromagnetic ball accelerator built around a custom ESP32-based PCB and a modular, 3D-printed circular track.

The electronics design and PCB layout are complete, and the mechanical CAD is in progress. The board has not been manufactured or tested. A first draft of the firmware (written with AI assistance) is included, but it has not been tested on hardware.

## Why I’m building it

I started this project after watching Hyperspace Pirate’s electromagnetic accelerator videos. I wanted to develop my own version and learn how to take a hardware project from a hand-drawn schematic to a custom PCB, mechanical design, firmware and a working prototype.

The project brings together several skills I want to improve: electronics design in KiCad, 3D modelling in Onshape, embedded programming and documenting the development process.

I am also reusing components I already have, including infrared sensors salvaged from a photocopier.

## What it is intended to do

The planned system will use infrared sensors to detect a steel ball moving around the track. An ESP32 will control four electromagnetic stages, and an OLED display is intended to show the measured ball speed.

The goal is to build and document a working experimental prototype, then compare the planned behaviour with the results of physical tests.

## Hardware overview

- **Controller:** ESP32 30-pin development board, plugged into the PCB.
- **Coil drivers:** Four channels using IRLZ44N N-channel MOSFETs, gate resistors, pull-down resistors and SB560 flyback diodes. Coils connect through screw terminals.
- **Position sensing:** Four IR sensor inputs on 3-pin JST XH connectors. The sensors were salvaged from a photocopier.
- **Power:** 3S LiPo input through an XT30 connector, with a fuse, on/off switch, bulk capacitor and L7805 regulator for the 5 V supply.
- **Display:** A 4-pin I2C header for a 0.96-inch OLED, intended to display ball speed.
- **Expansion:** A 1×10 header exposing spare ESP32 GPIO pins, 3.3 V and GND.
- **PCB:** A two-layer board designed in KiCad 10, with GND copper pours on both layers.

## ESP32 connections

The PCB uses an ESP32 30-pin DevKit. The numbers below are GPIO numbers, not physical connector-pin numbers.

### IR sensor inputs

| Connector | Receiver signal | Pull-up | ESP32 pin |
| --- | --- | --- | --- |
| J1 | OUT, connector pin 3 | R1 — 10 kΩ to 3.3 V | GPIO36 |
| J2 | OUT, connector pin 3 | R2 — 10 kΩ to 3.3 V | GPIO35 |
| J3 | OUT, connector pin 3 | R3 — 10 kΩ to 3.3 V | GPIO34 |
| J4 | OUT, connector pin 3 | R4 — 10 kΩ to 3.3 V | GPIO39 |

Each IR connector has the following pin assignment:

| Connector pin | Connection |
| --- | --- |
| 1 | IR emitter supply through a 150 Ω resistor (R5–R8) from 3.3 V |
| 2 | GND |
| 3 | Receiver output, connected to the corresponding ESP32 input |

GPIO34, GPIO35, GPIO36 and GPIO39 are input-only pins without internal pull-up resistors. The firmware therefore uses `INPUT`; the pull-ups are provided by the PCB.

### MOSFET gate connections

| MOSFET | ESP32 GPIO | Series gate resistor | Gate pull-down |
| --- | --- | --- | --- |
| Q1 | GPIO17 | R12 — 100 Ω | R15 — 10 kΩ to GND |
| Q2 | GPIO16 | R11 — 100 Ω | R13 — 10 kΩ to GND |
| Q3 | GPIO18 | R10 — 100 Ω | R14 — 10 kΩ to GND |
| Q4 | GPIO19 | R9 — 100 Ω | R16 — 10 kΩ to GND |

### OLED display

Connector J10 is intended for an I2C SSD1306 OLED.

| J10 pin | Signal | ESP32 connection |
| --- | --- | --- |
| 1 | SDA | GPIO21 |
| 2 | SCL | GPIO22 |
| 3 | Display supply | 3.3 V |
| 4 | Ground | GND |


### Expansion connector J11

| J11 pin | Connection |
| --- | --- |
| 1 | GND |
| 2 | 3.3 V |
| 3 | +5 V |
| 4 | GPIO5 |
| 5 | GPIO3 (RX0) |
| 6 | GPIO32 |
| 7 | GPIO33 |
| 8 | GPIO14 |
| 9 | GPIO13 |
| 10 | GPIO27 |

GPIO3 is the UART0 RX pin used for USB programming, and GPIO5 is a strapping pin, so avoid driving these externally during boot and flashing.

### Power connections

| ESP32 connection | PCB connection |
| --- | --- |
| VIN | Board's +5 V rail |
| 3.3 V | Logic supply for the sensor circuits and OLED |
| GND | Common PCB ground |

## Mechanical design

The mechanical parts are being designed in Onshape. The current CAD includes:

- Curved guide segments forming a circular track.
- Separate coil formers.
- IR sensor mounts.
- An initial assembly showing how the parts fit together.

The design is still in progress. No physical prints or fit tests have been completed yet, so the CAD assembly does not confirm real-world fit.

The mechanical part of the project is tracked separately:

[Electrotrack CAD repository](https://github.com/maslohaslo1234-spec/electrotrack-cad)

## Firmware

> **AI disclosure:** The firmware was written with AI assistance (vibe coding) and then reviewed and tidied up with AI tools. It is a first draft and has **not been tested on hardware**. Treat it as an untested starting point, not as verified code.

The sketch is in [`firmware/electrotrack/electrotrack.ino`](firmware/electrotrack/electrotrack.ino).

### How it works

One IR sensor is placed just before each of the four coils. When the ball is detected by sensor *i*:

1. Coil *i* is switched on to pull the ball in.
2. The firmware estimates when the ball will reach the centre of the coil, based on the speed measured between the two previous sensors.
3. Coil *i* is switched off at that moment, so the coil does not pull the ball back.

Ball speed is calculated as the sensor spacing divided by the time between two consecutive sensor events. The current speed, peak speed and hit count are shown on the OLED and printed to Serial.

The time-critical part is handled by a sensor interrupt (no floating-point maths, I2C or Serial inside it) and a high-priority sequencer task that busy-waits for the switch-off time with microsecond precision. The OLED runs in a separate low-priority task so it cannot delay switching.

### Controls

Press the on-board **BOOT** button to start or stop a run. At the start of a run, coil 0 is switched on to give the ball an initial pull, so the ball should rest just before sensor 0.

### Software safety features

- Each coil has a hard maximum on-time of 80 ms.
- When sensor *i* fires, coil *i − 1* is forced off, as a backstop for the timed switch-off.
- Only the expected sensor is accepted, which rejects noise and out-of-order events.
- A run is limited to 400 sensor events (about 100 laps).
- Any fault switches all coils off and latches a `FAULT` state until the next start.

These checks run in software only. A crash or freeze of the ESP32 while a coil is on is not covered, so the hardware limitations listed below still apply.

### Parameters to tune

| Constant | Meaning |
| --- | --- |
| `SENSOR_SPACING_MM` | Distance between two consecutive sensors along the track |
| `SENSOR_TO_CENTER_MM` | Distance from a sensor to the centre of its coil |
| `OFF_LEAD_US` | How early the coil is switched off, to compensate for acceleration and flyback decay |
| `FIRST_OFF_DELAY_US` | Switch-off delay for the first hit, when no speed is known yet |
| `COIL_MAX_ON_US` | Hard maximum on-time per coil activation |
| `IR_TRIGGER_EDGE` | `RISING` for the phototransistor sensors, `FALLING` for push-button simulation |

The geometry values are initial estimates and must be updated to match the final mechanical design. `OFF_LEAD_US` and `FIRST_OFF_DELAY_US` can only be tuned on the real hardware.

### Building

- Board: ESP32 Dev Module (ESP32 Arduino core).
- Libraries: Adafruit SSD1306 and Adafruit GFX.
- The pin mapping in the sketch matches the tables in [ESP32 connections](#esp32-connections).
- GPIO16 and GPIO17 drive MOSFET gates Q2 and Q1, so use a WROOM-based ESP32 board. WROVER modules use these pins for PSRAM.

### Known limitations

- Not tested on hardware, and the control logic is unverified.
- Timing parameters and geometry values are untuned.
- There is no hardware watchdog or independent coil cut-off.
- Failure modes such as a missed sensor event are handled only by the software timeout and fault state.

## Project files

| Path | Description |
| --- | --- |
| [`hardware/electrotrack.kicad_pro`](hardware/electrotrack.kicad_pro) | KiCad project file |
| [`hardware/electrotrack.kicad_sch`](hardware/electrotrack.kicad_sch) | Electronic schematic |
| [`hardware/electrotrack.kicad_pcb`](hardware/electrotrack.kicad_pcb) | PCB layout |
| [`hardware/electrotrack.pretty/`](hardware/electrotrack.pretty/) | Custom footprint library, including the ESP32 30-pin footprint |
| [`hardware/electrotrack.csv`](hardware/electrotrack.csv) | Electronic component list exported from KiCad |
| [`firmware/electrotrack/electrotrack.ino`](firmware/electrotrack/electrotrack.ino) | ESP32 firmware (AI-assisted first draft, untested) |
| [`gerber/`](gerber/) | PCB fabrication Gerber exports |
| [`drill files/`](drill%20files/) | PCB drill exports |
| [`pdf/`](pdf/) | PDF plots of the PCB layers |
| [`BOM.md`](BOM.md) | Half Life funding and purchasing tracker |
| [`JOURNAL.md`](JOURNAL.md) | Development journal mirrored from Half Life |

To inspect the electronics design, open `hardware/electrotrack.kicad_pro` in KiCad 10. Keep the custom footprint library and library configuration files alongside the project.

## Status

- [x] Concept and hand-drawn schematic
- [x] KiCad schematic and component-list export
- [x] Two-layer PCB layout
- [x] PCB design rule check completed with 0 errors
- [x] Gerber and drill fabrication exports
- [x] Initial track, coil bobbin and IR sensor mount models
- [x] Initial mechanical assembly in Onshape
- [x] First firmware draft (AI-assisted, untested)
- [ ] Finish mechanical design and check fit
- [ ] Order PCB and parts
- [ ] Assemble and test the board
- [ ] Manufacture and assemble the mechanical parts
- [ ] Test and tune the firmware on hardware
- [ ] Implement and verify speed measurement
- [ ] Test the complete system

## Design notes and known limitations

This is an unverified first revision.

- **MOSFET control:** Compatibility between the ESP32 outputs and the selected MOSFETs still requires verification.
- **Power and thermal behaviour:** Current consumption, protection and regulator temperatures have not been physically tested.
- **Firmware:** An AI-assisted first draft exists but has not been tested on hardware; the control logic, timing parameters and fault handling remain to be verified.
- **Polarity protection:** The PCB does not include dedicated reverse-polarity protection.
- **Mechanical fit:** Clearances, printed-part fit and assembly access have not been verified with physical parts.
- **Measurements:** No ball-speed measurements or complete-system test results are available yet.

## Documentation

I am making an effort to document this project in English, but my English is still limited. I use AI tools to help translate and improve the English in my project documentation.

The firmware was also written with AI assistance (vibe coding); see the [Firmware](#firmware) section.

The funding BOM and development journal are synchronized from [Half Life](https://halflife.hackclub.com/).

## Safety

This is an experimental project involving high currents, stored electrical energy and a moving steel ball. The design has not been manufactured or validated, and this repository should not be treated as a tested construction guide.

## License

See [`LICENSE`](LICENSE).

## Inspiration

Inspired by Hyperspace Pirate’s electromagnetic accelerator project.
