# Working on Quadra

- Firmware: `NanoDepsidf/` (ESP-IDF via PlatformIO). How it works, and the rules for changing
  the control loop: `NanoDepsidf/docs/FIRMWARE.md`. Build with `pio run` in `NanoDepsidf/`.
- Desktop companion: `companion/` (Tauri + TypeScript). Building: `companion/README.md`.
  The wire protocol is `NanoDepsidf/src/host_proto.h` mirrored in `companion/src/proto.ts`;
  change both together.
- **Anything visual** (device screens, sprites, icons, command cards, animations, the
  companion's UI, doc screenshots) follows `NanoDepsidf/docs/PIXEL_ART.md`. Read it first.
- Check UI changes with `NanoDepsidf/tools/ui_preview/run.sh`, and profile JSON changes with
  `NanoDepsidf/tools/profile_json_test/run.sh`. Type-check the companion with
  `npx tsc --noEmit` in `companion/`.
- Code in the 10 kHz control loop must not block and must run from IRAM (`CONTROL_HOT`);
  see FIRMWARE.md section 17.
- When a change alters what the device or the companion shows or does, update `README.md`,
  `companion/docs/COMPANION.md` and the affected screenshots in the same change.
