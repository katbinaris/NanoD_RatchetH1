# Quadra companion

The desktop app for the Quadra knob. It reads the knob live and changes its settings from the
computer, in the same pixel style as the device's own screens.

- **The device:** a render of the knob as it is, live. Its screen shows inside the knob (the
  detents, the active app or mode with its feel, F1–F4); the LED ring glows round it in the
  colours the LEDs show right now, and the keys light up and press down while held. Hover a key
  to see what it does. (Firmware from before the LED stream: the ring is made up from the
  knob's angle.)
- **HAPTICS:** the haptic profiles (STEPS: WIDE / COARSE / MEDIUM / FINE / SMOOTH) and, for the
  chosen one, its FEEL (SAW / SINE / VISCOSE) and the tuning sliders SNAP, DAMP, SHAPE, AMP
  and PITCH. **RESET TO FACTORY** puts that profile back. Drag, scroll or use the arrow keys; the knob changes as you go.
- **PROFILES:** the mode (APP, MOUSE, KEYS, MIDI); in MOUSE and KEYS, the haptic profile the
  knob uses; in APP, the app profiles with the icons the device draws. **EDIT** opens any profile, built-ins included: name, icon (import any
  image), key labels, what the knob and F1–F4 send, and the command wheel. Edits go to the knob
  a moment after you make them; **SAVE TO KNOB** stores them. A changed built-in keeps its
  original in the firmware, and **RESET TO DEFAULT** brings that back. **DUPLICATE** and
  **+ NEW PROFILE** make profiles of your own. **MACROS** are key presses, text and pauses the
  knob types by itself (no software needed once saved): build them step by step or **RECORD**
  them, then give one to a key (TAP, or its quick tap) or to a command-wheel entry.
- **DEVICE:** BINDINGS (MAC / PC), screen rotation, boot mode, and the firmware version.
- **SYS INFO:** power (an estimate), heat, CPU and system, with a minute of history and RESET
  PEAKS.

Changes are live on the knob but not saved. Values that differ from what's stored show in
amber, and **SAVE** writes them to the knob; F2 on the device does the same. **REVERT** goes
back to what's stored.

The [user guide](docs/COMPANION.md) covers every screen, with screenshots.

## How it talks to the knob

It uses the knob's vendor HID interface: usage page `0xFF00`, 64-byte reports, the same
interface the icon upload uses. No driver is needed on any OS, and on macOS there's no Input
Monitoring prompt. The wire format is
[`NanoDepsidf/src/host_proto.h`](../NanoDepsidf/src/host_proto.h), mirrored in
[`src/proto.ts`](src/proto.ts); change both together. The knob must run firmware with that
protocol (version 3: haptic profiles) and be in **HID** boot mode. With older firmware the
settings still work, but the HAPTICS tab can't show the profiles' own limits.

The protocol and the whole UI are TypeScript, and only the USB connection differs:

| | Desktop app (Tauri) | Web page (WebHID) |
|---|---|---|
| USB access | Rust, `hidapi` (`src-tauri/src/lib.rs`), a thin pipe | The browser's WebHID |
| Browsers | – | Chrome or Edge (not Safari or Firefox) |
| Connecting | Automatic, and reconnects on replug | CONNECT once (the browser asks), then automatic |

## Running it

Needs Node with pnpm, and Rust (`rustup`).

```sh
pnpm install
pnpm tauri dev          # the app, with live reload
pnpm dev                # just the page: open http://localhost:1420 in Chrome for WebHID
```

**Demo mode:** add `?demo` to the page's URL, for example `http://localhost:1420/?demo`. A
simulated knob answers the protocol, so the UI can be worked on without the hardware.
`&tab=SYS_INFO` (or `PROFILES`, `DEVICE`) opens on that tab, and `&tab=PROFILES&edit=1` opens
the editor on profile 1.

## Building

```sh
pnpm tauri build        # -> src-tauri/target/release/bundle/macos/Quadra.app (~4 MB)
```

Local builds are signed ad-hoc, which needs no Apple account. Ad-hoc signing is fine on your
own Mac; when someone else opens the app for the first time, macOS warns about an unidentified
developer.

`bundle.targets` is `["app"]`. Tauri's DMG step styles the disk image by scripting Finder,
which can hang waiting for a permission prompt. For a DMG, use `CI=true pnpm tauri build
--bundles dmg`, which skips the Finder styling, or build it in CI.

### Notarizing (for handing it to other people)

This needs the Apple Developer Program ($99/year). After a one-time setup, Tauri does the
signing, notarization and stapling itself on every build:

1. In the developer account, create a **Developer ID Application** certificate and install it
   in the keychain.
2. In App Store Connect, create an **API key** (Users and Access → Integrations) and download
   the `.p8` file.
3. Build with:

   ```sh
   export APPLE_SIGNING_IDENTITY="Developer ID Application: <name> (<team id>)"
   export APPLE_API_ISSUER=<issuer id>
   export APPLE_API_KEY=<key id>
   export APPLE_API_KEY_PATH=/path/to/AuthKey_<key id>.p8
   pnpm tauri build
   ```

The hardened runtime is already on, which notarization requires. For releases, the same
variables work as GitHub Actions secrets with `tauri-action`.

## Layout

```
src/proto.ts        the protocol (mirror of host_proto.h)
src/profile.ts      profiles as JSON (mirror of profile_json.h), key names, icon conversion
src/transport.ts    Tauri pipe | WebHID, one interface
src/device.ts       connection, state, profiles + icons, history
src/mock.ts         ?demo: a simulated knob
src/ui/             the device view (render + LEDs + screen mirror), the four panels, the editor,
                    the shared kit (blocks, cards, pixel drawing)
src/assets/         device.png, the top-down render (geometry in deviceView.ts)
src-tauri/          the Rust side: HID list / open / write / close, reports as events
```
