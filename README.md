electrotrack is a multi-stage electromagnetic ball accelerator (coilgun) with a custom PCB. An ESP32 reads IR photo-interrupters along a 3D-printed track and fires a sequence of coils to pull a steel ball forward. Each coil is switched off as the ball reaches its center, so it isn't pulled back.

Hardware

ESP32 controller, 4 coil channels (logic-level N-MOSFETs + flyback diodes)
Coils wound from 0.3 mm copper wire
3S LiPo (≈12 V) → L7805 → ESP32 VIN
IR position sensors salvaged from a photocopier
7.5 mm or 12 mm steel ball [final choice TBD]

Status

Concept, hand-drawn schematic and idea reel done (Entry 1)
Next: KiCad schematic, BOM, PCB layout
Later: coil winding, 3D-printed track, firmware, speed measurement

Repo: schematic, KiCad files, BOM and firmware will be in the GitHub repo.

Inspired by Hyperspace Pirate's electromagnetic accelerator project.
