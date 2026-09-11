# design/ — UI reference from Claude Design

Source: Claude Design project "8-bit CRT Control Panel Design" (`1ccdd233-f903-4b5f-a694-ae203db9d6ab`), file `Servo Joint Controller.dc.html`. Pulled on 2026-09-11.

| File | What it is |
|---|---|
| `Servo Joint Controller.dc.html` | Template plus a mock logic class that simulates the motor with fake physics. It isn't wired to anything. |
| `support.js` | Claude Design's generated runtime. It loads React 18 from unpkg.com at page load. |

**This is a reference, not deployable code.** The runtime needs internet access (unpkg), and the logic is a simulator.
The real page will be a dependency-free port of this layout (see decision D-006). The review notes are in docs/gui.md.

The project also has 8 reference images in `uploads/`. They weren't pulled because they aren't needed for the port.
