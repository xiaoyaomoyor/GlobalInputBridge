# Changelog

## [1.0.2] - 2026-09-02

- **Changed**: the key event filter (`Set Global Input Event Filter`) now also gates Global Input Action Event nodes — filtered keys can no longer start actions (Started) and active actions stop receiving Triggered. `Completed` is still emitted for actions that started before the filter was applied, so Started handlers are always closed out. Previously the filter only affected the `On Global Key Event` delegate.
- Modifier-key state, state queries, and frame edges remain unaffected by filtering.

## [1.0.1] - 2026-08-27

- Added a demo map (`Content/Demo/DemoMap`) with blueprint examples covering listening lifecycle, key state queries, mouse delta aim mirroring, the Global Input Action Event node, and event filtering.
- Added demo assets (character, materials, game mode) used by the demo map.
- Added screenshots to the documentation.
- Updated repository links after the repository rename to `GlobalInputBridge`.
- Documentation: bilingual README alignment, Vibe-Coding disclosure.

## [1.0.0] - 2026-08-26

First open-source release.

### Features

- Focus-independent global keyboard input via a background Windows Raw Input worker thread (`RIDEV_INPUTSINK`), with per-device state and hot-plug support.
- Global mouse tracking with four modes:
  - **Raw Input** (default): relative mouse deltas from `WM_INPUT` (`RAWMOUSE.lLastX/lLastY`) — works in cursor-locked FPS games such as Hunt: Showdown, unaffected by pointer acceleration or DPI scaling.
  - **Polling**: desktop pixel deltas via `GetCursorPos`.
  - **Buttons Only** / **Disabled**.
- Automatic Raw Input registration re-arm (500 ms heartbeat) that recovers from same-process registration takeovers (e.g. the UE viewport high-precision mouse mode), with no cross-process interference.
- Aggregated key state queries (`Is Global Key Down`, `Was Global Key Pressed/Released This Frame`, `Get Pressed Global Keys`).
- `On Global Key Event` / `On Global Mouse Move` broadcast delegates.
- **Global Input Action Event** blueprint node with an Enhanced-Input-like `Started` / `Triggered` / `Completed` lifecycle, optional exclusive modifier matching, and automatic per-instance registration.
- Key event filtering (allow-list or exclude-list).
- Modifier state queries (Ctrl / Alt / Shift / Win).
- Debug snapshot (`Get Global Input Debug Info`) and configurable log levels.
- Automation tests for state manager, chord binding manager, subsystem lifecycle, key mapping, and editor node compilation.
