# Electrotrack

Electrotrack is an experimental multi-stage electromagnetic ball accelerator (coilgun). An ESP32 reads infrared (IR) photo-interrupter sensors placed along a 3D-printed track and fires a sequence of coils to pull a steel ball forward. Each coil is switched off as the ball reaches its centre, so the ball is not pulled back.

## Hardware overview

- **Controller:** ESP32 30-pin development board, plugged into the PCB.
- **Coil drivers:** 4 channels, each with a logic-level N-channel MOSFET (IRLZ44N), a gate series resistor and pull-down, and an SB560 flyback diode. Coils connect to screw terminals.
- **Position sensing:** 4 IR sensor inputs on 3-pin JST XH connectors (sensors salvaged from a photocopier), each with its own resistors on the board.
- **Power:** 3S LiPo (about 12 V) through an XT30 connector, a 5 A fuse, an on/off switch, a 1000 uF bulk capacitor, and an L7805 regulator that supplies 5 V to the ESP32.
- **Extras:** a 4-pin I2C header for a 0.96" OLED display (to show ball speed) and a 1x10 header that breaks out spare ESP32 GPIO pins together with 3.3 V and GND.
- **PCB:** 2-layer board designed in KiCad 10, with a GND copper pour on both layers.

Planned (not built yet): coils wound from 0.3 mm copper wire, a 3D-printed track, and a 7.5 mm or 12 mm steel ball (final choice to be decided).

## Project files

| Path | Description |
| --- | --- |
| [`hardware/electrotrack.kicad_pro`](hardware/electrotrack.kicad_pro) | KiCad project file (open this in KiCad 10) |
| [`hardware/electrotrack.kicad_sch`](hardware/electrotrack.kicad_sch) | KiCad schematic |
| [`hardware/electrotrack.kicad_pcb`](hardware/electrotrack.kicad_pcb) | KiCad PCB layout |
| [`hardware/electrotrack.pretty/`](hardware/electrotrack.pretty/) | Custom footprint library (ESP32 30-pin footprint), referenced by `fp-lib-table` |
| [`hardware/electrotrack.csv`](hardware/electrotrack.csv) | Component list exported from KiCad |
| [`gerber/`](gerber/) | PCB fabrication Gerber exports |
| [`drill files/`](drill%20files/) | PCB drill exports |
| [`pdf/`](pdf/) | PDF plots of the individual PCB layers |
| [`software/`](software/) | Reserved for firmware (not added yet) |
| [`BOM.md`](BOM.md) | Half Life funding and parts tracker |
| [`JOURNAL.md`](JOURNAL.md) | Half Life project journal |

## Status

- [x] Concept and hand-drawn schematic
- [x] KiCad schematic and component-list export
- [x] PCB layout (2 layers), design rule check with 0 errors
- [x] Gerber and drill fabrication exports
- [ ] Order PCB and parts, assemble the board
- [ ] Wind the coils
- [ ] 3D-printed track
- [ ] Firmware and ball speed measurement
- [ ] System tests

The board has not been manufactured or tested yet.

## Design notes and known limitations

- **MOSFET gate drive:** the IRLZ44N is a logic-level MOSFET, but the manufacturer specifies its on-resistance only for 4-5 V gate drive. The gates are driven directly from 3.3 V ESP32 pins, so the real on-resistance will be somewhat higher. This is to be checked by measuring the drain-source voltage during a coil pulse. If it turns out to be a problem, a gate driver can be added in a later revision.
- **Fuse:** the 5 A fuse has to be matched to the final coil resistance (current is roughly 12 V divided by the coil resistance).
- **Firmware:** it must limit the maximum on-time of each coil, so that a software fault cannot leave a coil switched on.
- **Polarity:** there is no dedicated reverse-polarity protection. The design relies on the keyed XT30 connector.

The BOM and journal are synchronized from [Half Life](https://halflife.hackclub.com); 

## Safety

This is an experimental project involving high currents, stored electrical energy, and a moving projectile. Treat the design as unverified: review electrical and mechanical hazards, use appropriate protective measures, and follow applicable local requirements before building or operating it.

## License

See [`LICENSE`](LICENSE).

## Inspiration

Inspired by Hyperspace Pirate's electromagnetic accelerator project.