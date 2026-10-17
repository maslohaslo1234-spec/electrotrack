# Electrotrack

![Electrotrack PCB — 3D view](https://github.com/user-attachments/assets/24169744-645e-483c-90ab-85a70d674799)

Electrotrack is an experimental electromagnetic ball accelerator built around a custom ESP32-based PCB and a modular, 3D-printed circular track.

The electronics design and PCB layout are complete, and the mechanical CAD is in progress. The board has not been manufactured or tested, and the firmware has not been implemented yet.

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


## Mechanical design

The mechanical parts are being designed in Onshape. The current CAD includes:

- Curved guide segments forming a circular track.
- Separate coil bobbins.
- IR sensor mounts.
- An initial assembly showing how the parts fit together.

The design is still in progress. No physical prints or fit tests have been completed yet, so the CAD assembly does not confirm real-world fit.

The mechanical part of the project is tracked separately:

[Electrotrack CAD repository](https://github.com/maslohaslo1234-spec/electrotrack-cad)

## Project files

| Path | Description |
| --- | --- |
| [`hardware/electrotrack.kicad_pro`](hardware/electrotrack.kicad_pro) | KiCad project file |
| [`hardware/electrotrack.kicad_sch`](hardware/electrotrack.kicad_sch) | Electronic schematic |
| [`hardware/electrotrack.kicad_pcb`](hardware/electrotrack.kicad_pcb) | PCB layout |
| [`hardware/electrotrack.pretty/`](hardware/electrotrack.pretty/) | Custom footprint library, including the ESP32 30-pin footprint |
| [`hardware/electrotrack.csv`](hardware/electrotrack.csv) | Electronic component list exported from KiCad |
| [`gerber/`](gerber/) | PCB fabrication Gerber exports |
| [`drill files/`](drill%20files/) | PCB drill exports |
| [`pdf/`](pdf/) | PDF plots of the PCB layers |
| [`BOM.md`](BOM.md) | Half Life funding and purchasing tracker |
| [`JOURNAL.md`](JOURNAL.md) | Development journal mirrored from Half Life |

To inspect the electronics design, open `hardware/electrotrack.kicad_pro` in KiCad 10. Keep the custom footprint library and library configuration files alongside the project.

Firmware has not been added yet.

## Status

- [x] Concept and hand-drawn schematic
- [x] KiCad schematic and component-list export
- [x] Two-layer PCB layout
- [x] PCB design rule check completed with 0 errors
- [x] Gerber and drill fabrication exports
- [x] Initial track, coil bobbin and IR sensor mount models
- [x] Initial mechanical assembly in Onshape
- [ ] Finish mechanical design and check fit
- [ ] Order PCB and parts
- [ ] Assemble and test the board
- [ ] Manufacture and assemble the mechanical parts
- [ ] Develop firmware
- [ ] Implement and verify speed measurement
- [ ] Test the complete system


## Design notes and known limitations

This is an unverified first revision.

- **MOSFET control:** Compatibility between the ESP32 outputs and the selected MOSFETs still requires verification.
- **Power and thermal behaviour:** Current consumption, protection and regulator temperatures have not been physically tested.
- **Firmware:** The control software and fault handling remain to be developed and tested.
- **Polarity protection:** The PCB does not include dedicated reverse-polarity protection.
- **Mechanical fit:** Clearances, printed-part fit and assembly access have not been verified with physical parts.
- **Measurements:** No ball-speed measurements or complete-system test results are available yet.

## Documentation

I am making an effort to document this project in English, but my English is still limited. I used a translator to help write and translate the descriptions and explain my work more clearly.

The funding BOM and development journal are synchronized from [Half Life](https://halflife.hackclub.com/).

## Safety

This is an experimental project involving high currents, stored electrical energy and a moving steel ball. The design has not been manufactured or validated, and this repository should not be treated as a tested construction guide.

## License

See [`LICENSE`](LICENSE).

## Inspiration

Inspired by Hyperspace Pirate’s electromagnetic accelerator project.
