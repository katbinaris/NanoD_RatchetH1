# Nano_D++ ESP-IDF Rewrite — Development Plan

Living document. Check off tasks as they're completed, add notes/dates inline where useful.
Update this file whenever a phase or sub-task finishes — don't let it drift from reality.

**Legend**: `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked

**This is not a 1:1 port.** Legacy behavior is a reference, not a spec. Default posture per
phase is: improve what's worth improving, and prune what no longer serves the new purpose
rather than carrying it forward out of inertia. Where legacy complexity existed to serve
MIDI/OSC/multi-profile breadth that's no longer the point of the device, cut it instead of
translating it.

---

## Session handoff (continuing elsewhere -- read this first)

**Where things stand right now**: Phase 2a (FOC driver) and Phase 2b (haptic detent demo)
are both functionally complete and hardware-validated end to end. **Phase 7 (I2S audio)**
has a hardware-confirmed first slice: startup chime plays, and a haptic click ("wood"
timbre, retuned live on hardware to 2400Hz/5ms) is audible on each detent edge, modeled on
`Lucu-Kind/src/haptics.h`/`.cpp` (a different sibling project, not a `legacy_fw` port --
see §2 Phase 7). **Phase 3 (USB HID) is in progress now** -- composite CDC+HID device
confirmed enumerating and HID reports confirmed delivering (keyboard self-test typed 'a'
on the host), but getting CDC-ACM added broke `pio run -t upload` (this board's
1200bps-touch reset trick has no free equivalent in bare TinyUSB); resolved via a new
boot-time USB-mode-select in `main.c` rather than the register-level fix that was
considered and rejected as too risky to implement blind -- see §2 Phase 3 for the full
story. **Not yet re-tested on hardware after that fix.**

**How to test the current firmware** (`NanoDepsidf/`, build/flash with
`~/.platformio/penv/bin/pio run -t upload --upload-port <port>`): **hold BTN_C+BTN_D at
boot (own ~2s window, checked before any other task starts) to flash/get a serial
console** -- this skips installing the Phase 3 TinyUSB HID device for that boot, leaving
the native USB-Serial-JTAG path (which the 1200bps-touch upload trick and console both
rely on) on its default routing. A normal boot (neither held) instead brings up the
composite HID+CDC device. **A plain reset/reconnect (no buttons held) now runs the haptic
detent demo directly** (see Phase 2b below) -- promoted to the default/main behavior now
that it's mature, no combo needed. Hold **BTN_A** during boot instead (within ~3-6s of
power-up/reset, its own separate window _after_ the USB-mode-select one above) to arm the
legacy diagnostic suite: **BTN_A** alone for the closed-loop bench test; **BTN_A + BTN_B**
to also force a fresh calibration (otherwise it loads the cached one from NVS); **BTN_A +
BTN_C** for the open-loop diagnostic; **BTN_A + BTN_D** for the pole-pair diagnostic.
Serial console is at 115200 baud; a plain `pio
device monitor` needs an interactive TTY, so this session used a small retrying-open
pyserial script instead -- worth knowing if reading the live log turns out to matter again,
since the ESP32-S3's USB-CDC/JTAG console takes a couple seconds to re-enumerate after a
physical unplug/replug, easy to miss the first few log lines otherwise (this is also why
`control_task.c` has a deliberate 3s delay before the arm-check even starts).

**Key config values right now** (`src/motor_config.h`, `src/control_task.c`,
`src/tasks_common.h`, `src/motor_driver.c`): **7 pole pairs** (corrected from an earlier,
now-superseded 4 -- confirmed via the dedicated pole-pair diagnostic: -13.462 rad over 5.0s
at 3.00Hz electrical → exactly 7.00 pole pairs), ~2.645ohm per-phase resistance. Current cap
is `MOTOR_MAX_CURRENT_STATIC_A` = `MOTOR_MAX_CURRENT_ROTATING_A` = 0.756A (~2.0V effective
vq clamp, raised in steps from an original 0.2A/0.5A split as haptic click tuning demanded
more peak torque -- beyond the originally-validated 0.5A range, watch for brownout/coil
heating). **Control loop at 10kHz** (`CONTROL_LOOP_PERIOD_US=100`, raised from a 1kHz
placeholder; the IDLE0 task watchdog this originally tripped was fixed at the root via
`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n` in `sdkconfig.defaults`, not by lowering the
rate). **MCPWM carrier at 32kHz** (`MCPWM_RESOLUTION_HZ=32000000`, re-raised from a 10kHz
whine-avoidance compromise after re-confirming clean/brownout-free on hardware). An 8V/s
voltage slew-rate limit bounds inrush current on step changes in `foc_calibration.c`'s
ramped align/step and `control_task.c`'s bench-tour `CL_MAX_VQ_STEP_V`; the haptic demo uses
its own looser `HAPTIC_VOLTAGE_SLEW_LIMIT_V_PER_S=200` for its background command, plus a
deliberately un-slew-limited "click" transient pulse layered on top (see Phase 2b). Bench-
tour PD gains Kp=1.5 V/rad / Kd=0.05 V/(rad/s), divergence-abort at 1.5 rad with a 1s
post-target-change grace period.

**Don't re-litigate without new evidence** (each cost real debugging time across
sessions): phase resistance is line-to-line/2, not the raw legacy constant; pole pairs are
**7**, confirmed via the pole-pair diagnostic test's electrical-vs-mechanical rotation
ratio (an earlier direct-magnet-count estimate of 4 was wrong and has been fully
superseded); MCPWM `MCPWM_TIMER_COUNT_MODE_UP_DOWN` halves `period_ticks` into the real
compare-value ceiling (`peak_ticks`) -- confirmed directly from `esp_driver_mcpwm` source,
not assumed; this board's STSPIN233 EN pins are plain push-pull enables (not open-drain
EN/FAULT -- that's a different, separate pin on this board's schematic, wired only to an
LED, not any MCU GPIO); a fixed-value, per-tick constant (iteration counts, filter alphas)
silently changes real-world meaning if the control loop rate changes -- always express
these via `MS_TO_ITERS()`/a real time-constant divided by the loop period, not a bare
iteration count or alpha (see `HAPTIC_VELOCITY_FILTER_TAU_S` for the pattern).

**Full incident history** (several real bugs, each with root cause and fix) is in the
Phase 2a and 2b sections of §2 below -- worth reading before assuming something is broken
again; there's a good chance it already happened once and got fixed/explained.

---

## 0. Project scope

Porting/rewriting Nano_D++ (Binaris Circuitry) firmware from Arduino framework
(`legacy_fw/`, reference only) to native ESP-IDF (`NanoDepsidf/`).

**Purpose change**: legacy device was MIDI/haptic-controller focused. New device drops
MIDI as a primary function; primary purpose becomes a **USB keyboard/mouse/gamepad
HID controller** with a haptic FOC knob. Audio, display and LED are secondary/cosmetic
features carried forward, re-scoped as needed.

**Hardware** (unchanged, board: `NanoFOC_D`, ESP32-S3, see `legacy_fw/include/nanofoc_d.h`):

- MCU: ESP32-S3, 240MHz, 4MB flash, PSRAM
- Magnetic sensor: MT6701, SSI mode over SPI (CLK=18, DO=21, CS=17)
- Motor driver: STSPIN233 (ST, low-voltage triple half-bridge, single-PWM-per-phase +
  per-phase enable -- 3x IN + 3x EN, not a 6-pin high/low-side scheme), 4 pole pairs
  (8 magnetic poles -- differs from legacy's assumed 7; a cross-check at 7 caused stalling
  at high commanded RPM, but see `motor_config.h` for why that specific test is confounded
  and doesn't actually validate pole-pair count either way; 4 is the value with real
  empirical support: smooth, stall-free operation across the full tested speed range),
  ~2.645Ω per-phase resistance
  (measured 5.29Ω phase-to-phase / 2 -- legacy's "5.3Ω" was this same line-to-line value
  mistakenly used as per-phase), no current-sense ADC
  (IN: 34/35/37, EN: 33/48/36) → voltage-mode FOC only
- Display: GC9A01, 240×240, SPI (MOSI=4, SCLK=3, CS=6, DC=7, RST=2), backlight on GPIO5 (PWM)
- LEDs: WS2811, ring A (60px, GPIO38) + ring B (8px, GPIO42)
- Buttons: 4x (GPIO41/40/45/46)
- I2S transducer: DOUT=9, BCLK=10, LRC=11
- No WiFi/BLE used in this project — disable stack entirely to free resources

---

## 1. Architecture decisions (log)

Record of calls made so we don't re-litigate them each session.

- **Core split**: Core 0 = FOC control loop + MT6701 SPI read + key read + knob/key→HID
  mapping (kept exclusive, timer-paced, highest priority). Core 1 = USB (TinyUSB) + I2S
  audio + LVGL/display + LED (RMT), priority order USB > I2S > LVGL > LED.
- **FOC/sensor**: write native, don't port SimpleFOC's Arduino driver/sensor classes.
  MT6701 SSI is a fixed-width clocked frame (no register protocol) → trivial `spi_master`
  driver. Motor control via `driver/mcpwm` (hardware-synchronized 3-phase), not LEDC.
  No current sensing on this board → voltage-mode control only, no current-loop tuning needed.
- **Display**: `esp_lcd` (+ community `esp_lcd_gc9a01` component) + `esp_lvgl_port`,
  replacing TFT_eSPI + `lv_tft_espi_create()`. Existing SquareLine-generated UI
  (`ui.c`, `ui_helpers.c`, `src/screens/*.c`) is pure LVGL API, no Arduino dependency —
  portable largely as-is; only the driver/glue layer (`lcd_thread.cpp` equivalent) is rewritten.
  Decision on how much of the existing screens to keep vs. simplify: **pending**
  (depends on how much on-screen UI the keyboard/mouse-focused device still needs).
- **LEDs**: native RMT-based driver (`espressif/led_strip` component) instead of FastLED.
- **ULP-RISC-V**: not usable for USB/SPI/I2S monitoring — it only reaches the RTC domain
  (RTC GPIO/I2C/ADC), no access to USB OTG or main SPI/I2S peripherals. Not part of this
  design; revisit only if a low-power sleep/wake mode is wanted later.
- **WiFi/BLE**: no init calls anywhere in the app. Correction after hands-on verification:
  `ESP_WIFI_ENABLED` has no prompt in this IDF version — it's hard-derived as
  `default y if SOC_WIFI_SUPPORTED` (always true on S3) and cannot be toggled via sdkconfig
  at all; confirmed empirically (every attempt to disable it was silently discarded on
  rebuild). The real mechanism is simply never calling `esp_wifi_init()`/`esp_netif_init()`
  — the linker's dead-code elimination drops the WiFi driver automatically since nothing
  references it (confirmed: build stays ~13% flash with zero WiFi calls). `BT_ENABLED` _is_
  a real, promptable option and is disabled via `sdkconfig.defaults`.
- **Config gotcha (keep in mind for later phases)**: hand-editing the generated per-env
  `sdkconfig.<env>` file directly is unreliable under PlatformIO — some values get silently
  reconciled back to computed defaults on rebuild, and the actual partition-table generation
  step is driven by `board_build.partitions` in `platformio.ini`, not by the
  `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` value recorded in sdkconfig (confirmed: sdkconfig
  claimed the custom table while the generated `partitions.bin` silently used PlatformIO's
  built-in default until `board_build.partitions` was set). Going forward: config deltas go
  in `sdkconfig.defaults`, board-specific build settings go in `platformio.ini`/`boards/*.json`
  — don't hand-patch the generated sdkconfig snapshot.
- **Timing**: FOC loop paced by `esp_timer` (hardware, µs resolution) firing into a
  high-priority task via semaphore/notification, not a free-running `while(true)` loop
  or `vTaskDelay` tick polling (both used in legacy FW) — needed for a fixed, known Ts.
- **I2S audio**: reference source now provided — not `legacy_fw`'s original WAV-playback
  code, but a separate sibling project's implementation (`Lucu-Kind/src/haptics.h`/`.cpp`
  at `/Users/kama10/Documents/PlatformIO/Projects/Lucu-Kind/`), a different device using
  the same MAX98357A I2S amp wiring (DOUT/BCLK/LRCK = 9/10/11, identical to this board).
  Its model: a free-running Core 1 task doing pure procedural sine-LUT synthesis (no WAV
  assets), driven by lock-free atomics from the encoder/control side rather than a
  blocking queue, with several selectable click timbres and an independent out-of-bounds
  drone tone. **Adapt this architecture to ESP-IDF's `driver/i2s_std` and this project's
  own task/IPC conventions — do not port the Arduino source line-for-line.** See Phase 7.
- **Haptic control program vs. FOC driver**: split into two distinct layers, deliberately
  not conflated. The **FOC driver** (Phase 2a) is just "make the motor accurately produce a
  commanded torque/position at a known sensor angle" — mechanical/electrical correctness,
  can proceed independently. The **haptic control program** (Phase 2b, now complete — see
  §2 below) was _not_ ported from `legacy_fw/src/HapticCommander.*` / `haptic.cpp`; it was
  redesigned from scratch as a nearest-grid-point linear PD detent law plus a separate,
  deliberately un-slew-limited "click" transient pulse. **Phase 2c (haptic programs)** is
  the follow-on: turning the current single hard-coded profile into a set of selectable
  presets built from the same Kp/Kd/ramp/pulse primitives.
- **Configuration menu button roles** (Phase 8): F1/F2/F3/F4 are this device's physical
  key labels (silkscreen/user-facing naming) -- firmware pin constants stay
  `PIN_BTN_A`-`PIN_BTN_D` internally, unchanged; this is a labeling decision only, not a
  source rename. Fixed roles across all menu screens: F4 = open menu / back to Main
  Screen, F3 = back one level (cancels an in-progress edit and reverts it), F1 = select /
  enter edit / commit, F2 = unused/reserved. Knob rotation is context-sensitive (haptic
  feel + HID wheel in normal operation, list navigation inside the menu, value adjustment
  while editing a field) -- chosen because this board's 4 buttons have no dedicated
  encoder push-button to hang select/back on separately.
- **Pruning is an active goal, not a fallback.** See §4 for candidate cuts.

---

## 2. Phases

### Phase 0 — Project & hardware bring-up

- [x] PlatformIO + ESP-IDF skeleton created (`NanoDepsidf/`)
- [x] Custom board definition for `NanoFOC_D` (`NanoDepsidf/boards/nanofoc_d.json`, ported
      from `legacy_fw/boards/nanofoc_d.json` — trimmed to drop Arduino-only fields; 4MB
      flash, correct hwids/vendor/openocd target)
- [x] Custom partition table ported (`NanoDepsidf/boards/nano_partitions.csv`, wired via
      `board_build.partitions` in `platformio.ini` — verified the generated `partitions.bin`
      actually matches, not just the sdkconfig record of it)
- [x] Pin mapping header ported from `legacy_fw/include/nanofoc_d.h` →
      `NanoDepsidf/src/board_pins.h` (pin facts only, no TFT_eSPI-specific macros)
- [x] sdkconfig baseline via `sdkconfig.defaults`: flash size 4MB, custom partition table,
      Bluetooth disabled (`BT_ENABLED` is a real toggle). WiFi has no real toggle in this IDF
      version (`ESP_WIFI_ENABLED` is hard-derived from SoC capability, not settable) — "no
      WiFi" achieved simply by never calling its init APIs; confirmed via build size that
      dead-code elimination drops it.
- [x] PSRAM enabled: chip confirmed as ESP32-S3-**N4R2** (4MB flash + 2MB PSRAM, both
      in-package). R2 PSRAM is always Quad SPI (Octal is only R8/R16 variants — checked
      against `esp_psram`'s Kconfig, not assumed). Set `SPIRAM=y`, `SPIRAM_MODE_QUAD=y`,
      `SPIRAM_TYPE_AUTO=y`, `SPIRAM_SPEED_40M` (conservative for first bring-up; 80MHz is
      a documented follow-up once boot is confirmed stable on real hardware — see comments
      in `sdkconfig.defaults`).
- [ ] `CONFIG_FREERTOS_HZ` tick rate tuning — deferred to Phase 1 (task scaffolding), not
      needed just to get a first image booting
- [x] Build verified clean (`pio run`, RAM 4.8% / 15,596 B, Flash 13.4% / 175,453 B of the
      1,310,720 B `app0` partition), PSRAM config resolved correctly in generated config
- [x] **Flashed to real hardware and confirmed via serial console**: boot log shows
      `esp_psram: Found 2MB PSRAM device, Speed: 40MHz` + `SPI SRAM memory test OK` (N4R2
      PSRAM config verified correct on actual silicon, not just in theory), and the
      partition dump matches `nano_partitions.csv` exactly. Chip identifies as ESP32-S3
      (QFN56, rev v0.1), confirming Embedded Flash 4MB + Embedded PSRAM 2MB.
- Incidental finding: default UART0 console uses GPIO43/44, which coincide with the legacy
  MIDI jack pins in `board_pins.h`. Not an issue now; relevant only if MIDI/physical serial
  survives the Phase 8/pruning decision.

### Phase 1 — Core task scaffolding

- [x] Core 0 control task skeleton (`src/control_task.c`) — `esp_timer`-paced via
      `xTaskNotifyGive`/`ulTaskNotifyTake` (not a busy loop or `vTaskDelay` ticks), placeholder
      1kHz rate, pinned to Core 0 at priority 20. Kept in plain C, not C++ — nothing in this
      phase needs classes, and introducing C++ before there's an actual need would just be
      unnecessary complexity to carry forward.
- [x] Core 1 IO task skeletons, one file per subsystem so each is the natural home for its
      owning phase's real implementation: `src/usb_task.c` (prio 12), `src/i2s_task.c`
      (prio 10, explicitly marked blocked/placeholder pending Phase 6 source — no audio logic
      written), `src/display_task.c` (prio 5), `src/led_task.c` (prio 3). All pinned to Core 1.
- [x] Inter-core queues defined in `src/ipc.c`/`ipc.h`: `g_hid_report_queue` (control→USB,
      placeholder 8-byte report struct) and `g_motor_cmd_queue` (IO→control, unused until
      Phase 8 has a real producer). Message shapes are explicitly placeholders, not final.
- [x] Cross-core queue proven end-to-end, not just defined: control task pushes a dummy HID
      message every 100 loop iterations, `usb_task` receives and logs it — confirms the
      Core 0 → Core 1 handoff actually works, not just that it compiles.
- [x] Build verified clean (RAM 4.8% / 15,676 B, Flash 13.7% / 179,981 B)
- [x] Core affinity confirmed on real hardware via serial console: `control task started on
  core 0, prio 20`; `usb`/`i2s`/`display`/`led` all report `core 1` at priorities
      12/10/5/3 respectively. Cross-core queue delivery observed live (control task's dummy
      HID messages arriving at `usb` task every ~100ms, matching the 100-iteration/1kHz
      pacing exactly) — the whole Phase 1 mechanism is hardware-verified, not just compiled.

### Phase 2a — FOC driver (motor + sensor, mechanical/electrical correctness only)

- [x] Safety limits encoded in `src/motor_config.h` before any PWM output exists: max
      voltage 5V, max current 0.5A. No current sensing on this board, so the current limit
      is enforced as a computed Ohm's-law voltage clamp rather than a closed-loop limit.
      `MOTOR_EFFECTIVE_VOLTAGE_LIMIT_V` is the one constant control code must respect, not
      the raw voltage cap -- it takes the min of three constraints (current-derived cap,
      absolute voltage cap, and a topology-imposed Vbus/2 ceiling, see below). **Motor
      parameters corrected from legacy assumptions after hardware measurement**:
      phase-to-phase resistance measured directly at 5.29Ω (essentially identical to the
      legacy "5.3Ω" constant, confirming that value was actually line-to-line but had been
      used as per-phase resistance all along -- true per-phase is half, ~2.645Ω); pole pairs
      corrected from legacy's assumed 7 to the physically-confirmed 4 (motor is 8 magnetic
      poles = 4 pole pairs). With the corrected resistance, the current-derived limit
      (0.5A × 2.645Ω ≈ 1.32V) is now clearly the binding constraint.
- [x] MT6701 SSI driver over `spi_master` (`src/mt6701.c/h`, own SPI2 bus, no SimpleFOC
      dependency) — fully verified on real hardware: stable at rest (~12330/16383, ±4
      counts noise), and smooth bidirectional tracking while the knob was rotated by hand,
      including a correct wraparound at the 16383/0 boundary (16330 → 2401) rather than a
      glitch. SPI mode/clock/bit-alignment all confirmed correct.
- [x] `driver/mcpwm` 3-phase bring-up (`src/motor_driver.c`, single generator per phase --
      adapted from Espressif's `mcpwm_foc_svpwm_open_loop` example, which targets a 6-pin
      high/low-side topology; this board's STSPIN233 is 3x IN + 3x EN, so no dead-time
      submodule needed). Plain-float inverse Park/Clarke in `src/foc_math.c` (same logic as
      the reference example's `esp_foc.c`, float instead of fixed-point IQ math).
      **First bring-up attempt failed and surfaced a real bug**, logged in full because it's
      worth remembering: `MOTOR_EFFECTIVE_VOLTAGE_LIMIT_V` was set to the current-derived
      2.65V without accounting for this topology's actual linear ceiling of **Vbus/2 =
      2.5V** (single-supply, half-bridge-per-phase, duty centered at 50% = 0V -- a phase
      can't swing further than half the bus without clipping). The 0.15V overshoot made
      `mcpwm_comparator_set_compare_value` reject the value as out-of-range for ~1/3 of
      every rotation; the resulting **per-tick ESP_LOGE over UART (~7ms/line) starved the
      scheduler** badly enough to trip the idle-task watchdog and desync the iteration
      counter from wall-clock time by ~18s. Symptom on hardware matched exactly: motor
      stayed energized (resisted hand rotation) with a high-pitched noise, no actual
      rotation -- the field was stuck near-static instead of sweeping smoothly. Fixed with
      three changes: (1) `motor_config.h` now takes the min of current-derived/absolute
      cap/**Vbus-2 topology limit** — 2.5V now binds; (2) `motor_driver.c`'s duty clamp has
      a 1% margin on both edges so a command can never touch the exact boundary again
      regardless of upstream rounding; (3) `control_task.c` now has a **wall-clock hard
      timeout** (`esp_timer_get_time()`-based, originally 15s) that force-disables the
      driver independent of the iteration-counting state machine, so a similar desync
      can't again leave the motor energized past the intended bounded window.

      **Second attempt also failed** (board reset repeatedly under motor load -- USB
      port dropping/reappearing, symptom identical to the first attempt: energized,
      high-pitched noise, no rotation). Root cause: `MOTOR_PHASE_RESISTANCE_OHM` (5.3Ω,
      inherited from legacy) turned out to be **line-to-line, not per-phase** — confirmed
      by direct multimeter measurement (5.29Ω phase-to-phase). True per-phase resistance is
      ~2.645Ω, meaning the current-derived voltage limit was ~2x too permissive: at the
      2.5V commanded in attempt 2, actual current was ~0.94A against an intended 0.5A cap,
      sustained under near-locked-rotor load (knob wasn't rotating) for the whole 8s test —
      consistent with a brownout reset. Fixed by measuring the real resistance and deriving
      `MOTOR_PHASE_RESISTANCE_OHM` from it transparently in code rather than trusting the
      unverified legacy constant. Additional defensive changes made before a third attempt:
      - **Reset-reason logging** (`main.c`, `esp_reset_reason()`) — every boot now logs
        BROWNOUT/PANIC/TASK_WDT/etc. explicitly, so a recurrence gives hard evidence instead
        of inferring from symptoms.
      - **Button-armed gate** (`control_task.c`) — the motor driver no longer initializes
        unless BTN_A is held at boot. Previously *every* reflash or reset silently
        re-energized the motor, which is what made both incidents hard to investigate
        safely. A plain reset/reconnect now touches nothing motor-related.
      - **Shortened test window** while confidence was low: rotate phase cut from 8s→2s,
        hard timeout from 15s→5s (proportionally). Can be lengthened back once a clean run
        is confirmed.

      **Third attempt failed more seriously**: board fully powered down (LED dark) and
      reset, port disappeared entirely rather than the quick reconnect seen in attempt 2 —
      consistent with the host USB-C port's own over-current protection tripping (powered
      directly from a MacBook, no hub), not just an internal chip-level brownout. User
      observed slight (~1°) movement then immediate power-down. This pointed at something
      beyond "current a bit too high" -- a genuine spike. **Root cause found by checking the
      STSPIN233 datasheet** (not assumed from memory this time): the EN pins are actually
      **EN/FAULT combined pins** — the chip pulls one low via an internal open-drain MOSFET
      to signal overcurrent/short-circuit/thermal fault, and relies on an external RC network
      (on the board) for auto-retry timing. `motor_driver_init()` had configured these as
      plain push-pull `GPIO_MODE_OUTPUT` and actively drove them high to "enable" — which
      **fights the chip's own open-drain fault-latch** during a real fault, defeating the
      protection instead of letting it shut down. Suspected as the real cause of the current
      spike. Fixed: pins reconfigured as `GPIO_MODE_INPUT_OUTPUT_OD` (open-drain, readable);
      `motor_driver_enable()` now only ever actively drives LOW (force-disable) or releases
      the pin (lets the board's external pull-up/RC bring it high); new
      `motor_driver_check_fault()` reads the pins so a real chip-detected fault is now
      visible to firmware and immediately stops the test, instead of being silently fought.
      Also, per explicit request: `MOTOR_MAX_CURRENT_A` lowered further (0.5A→0.15A, new
      effective limit ≈0.397V), and the test itself simplified to remove rotation entirely
      for now — staged as enable-at-0V (pure enable-transient test) → brief fixed-voltage
      hold (no sweep) → ramp-down, so each question (is enabling itself safe? is static
      energization at the new cap safe?) is answered separately rather than combined into
      one test that makes multiple failure modes indistinguishable.
      **Fourth attempt (with the EN/FAULT open-drain fix + button-arm timing fixed)**:
      board booted cleanly (reset reason POWERON), armed correctly (`BTN_A` detected,
      `armed=1`), `motor_driver_init()` succeeded -- but `motor_driver_check_fault()` read
      **fault=1 immediately on enable, at exactly 0V commanded**, and held there constantly
      for the entire 500ms settle-check window with zero change (25 consecutive identical
      samples). Test auto-aborted safely (this is exactly what the fault-check-before-acting
      change was for) -- no PWM voltage was ever applied. A flat, unchanging fault reading
      across 500ms rules out a brief RC-charge-up transient (which would show a gradual
      clear); it looks like either a persistent/latched fault or a deeper hardware/wiring
      issue, not a timing artifact. Extended the settle window to 10s (still purely
      diagnostic, EN stays disabled, zero voltage/current risk regardless of outcome) to
      rule out a slower retry timer before concluding it's genuinely persistent. Also asked
      user: (1) has this exact board ever spun the motor successfully on the legacy Arduino
      firmware (would indicate the hardware itself is known-good), (2) any visible damage on
      the STSPIN233 or nearby components. Before the 10s-extended test could even run,
      user answered both: **this exact board has spun successfully on the legacy Arduino
      firmware**, and **no visible damage/smell** on the chip or nearby components.

      **This resolved it, and reversed the EN/FAULT open-drain change.** Legacy drove these
      same EN pins as plain push-pull outputs and never read them as inputs -- it worked.
      Combined with the fault reading being stuck at an unchanging value for the full 500ms
      (not gradually clearing, not moving at all), the real explanation is: this board most
      likely doesn't implement the external pull-up/RC network that STSPIN233's fault-output
      feature needs (that's a real feature of the chip, confirmed against the datasheet, but
      apparently not one Binaris wired up on this board) -- so switching the pin to
      open-drain just left it floating, and a floating CMOS input settling to a constant
      value is indistinguishable from a "stuck fault" without external pull infrastructure
      to make the reading meaningful. **Reverted**: `motor_driver.c`'s EN pins are back to
      plain `GPIO_MODE_OUTPUT` (matching what's proven to work), `motor_driver_check_fault()`
      removed entirely (a fault reading is meaningless on a pin *we're* driving push-pull --
      it would just echo back our own last-written value). The datasheet research itself
      wasn't wasted -- it's accurate about the chip -- it just doesn't describe how *this*
      board uses the pin. The actual fix for the real incidents remains the current/voltage
      correction above (measured resistance, correct pole pairs, lower cap); safety now
      relies on that plus the wall-clock hard timeout, same philosophy legacy used
      successfully.

      **User supplied the actual driver schematic**, which fully confirmed the above and
      added detail: ENU/ENV/ENW connect directly to MCU GPIOs with no external components
      (plain digital enables, as now coded) -- the *real* combined EN/FAULT pin is a
      separate, dedicated pin (13) on a net called `DRV_ERR`, which does have the external
      pull-up + RC network the datasheet describes (R6 18K to 3V3, C8/C9), shared with
      STBY/RESET (pin 14). But `DRV_ERR` connects only to an LED, not to any MCU GPIO -- so
      there genuinely is no way to read fault status from firmware on this board, confirming
      `motor_driver_check_fault()` was correctly removed rather than fixable.

      **Re-tested the revert -- "out of range" errors came back**, immediately on
      transitioning from OL_ENABLE_NEUTRAL (0V, no errors for a full 1s) to OL_HOLD_LOW
      (~0.397V). This led to the actual definitive root cause, found by reading
      `esp_driver_mcpwm`'s source directly instead of assuming: `mcpwm_timer.c` halves
      `period_ticks` into `peak_ticks` for `MCPWM_TIMER_COUNT_MODE_UP_DOWN` ("in symmetric
      mode, peak_ticks = period_ticks / 2"), and `mcpwm_cmpr.c`'s
      `mcpwm_comparator_set_compare_value()` validates against `peak_ticks`, not
      `period_ticks`. Every "out of range" incident this entire bring-up (including the very
      first one, originally attributed to a Vbus/2 topology ceiling) was actually this same
      factor-of-2 mistake: `voltage_to_compare()` scaled duty against `MCPWM_PERIOD_TICKS`
      (1000) when the real ceiling in this count mode is always half that (500). The
      Vbus/2 topology consideration is still electrically real (this is still a
      single-supply half-bridge-per-phase board), but it was never the software bug actually
      producing these errors -- this API misunderstanding was. Fixed: added
      `MCPWM_PEAK_TICKS = MCPWM_PERIOD_TICKS / 2`, used in `voltage_to_compare()`'s final
      scaling instead of the raw period. Neutral duty (0.5) now correctly maps to
      compare=250 (half of peak_ticks), not 500.
      **Confirmed clean**: full test sequence (enable-at-0V → hold @ 0.397V → ramp-down →
      disabled) completed with zero errors, zero resets, zero watchdog trips, finishing in
      the expected ~2.3s. First fully clean run of the entire Phase 2a bring-up. User
      observed a tiny nudge, no hold torque -- expected at this deliberately tiny 0.15A cap.

      **Restored to intended settings now that root causes are fixed**: `MOTOR_MAX_CURRENT_A`
      back to 0.5A (0.15A was only ever a debugging safety margin, not the real target;
      effective limit is now ~1.32V), and rotation reintroduced in `control_task.c` (was
      removed for diagnostic isolation, not because rotation itself was ever the problem) --
      0.3s enable-at-0V, 3s rotate @ 0.5Hz electrical (0.125Hz mechanical given 4 pole
      pairs), 0.3s ramp-down.

      **SUCCESS.** Full sequence ran exactly on schedule (armed 3738ms → rotating 4048ms →
      stopping 7048ms, exactly 3.0s of rotation → complete/disabled 7348ms), zero errors,
      zero resets. User confirmed: **the motor actually spun** (slowly, as designed --
      0.125Hz mechanical), with only minor high-pitched noise (expected PWM carrier tone,
      benign). This is the first successful motor rotation of the ESP-IDF rewrite -- open-loop
      FOC commutation direction/phasing confirmed correct, all four root-cause bugs from this
      bring-up (wrong phase resistance, MCPWM peak_ticks scaling, and two dead-end detours
      investigating EN/FAULT and a topology voltage ceiling that weren't the actual problems)
      resolved.

      **Extended to continuous low-RPM rotation** per request: `OL_TARGET_RPM` (20, easily
      raised to 40) drives `OL_ELECTRICAL_HZ` directly, rotate duration extended to 120s
      (still bounded, not truly infinite -- consistent with every other safety choice here),
      hard timeout extended to 130s accordingly. **Confirmed clean for the full 2 minutes**:
      zero errors, iteration counter tracked wall-clock time exactly for the entire duration
      (no scheduler starvation), motor spun continuously and steadily at 20 RPM, stopped and
      disabled exactly on schedule. This is the most sustained clean operation of the
      ESP-IDF rewrite to date.

      **Pushed further to 200 RPM per request, with a proper speed ramp.** A 10x instant
      step (20→200 RPM) risks the classic open-loop failure mode -- with no position
      feedback, the rotor can only track the commanded field if it accelerates fast enough
      to keep up, and stepping the speed can make it fall out of sync (the BLDC/PMSM
      equivalent of a stepper motor "missing steps"). Implemented a linear ramp
      (`OL_START_RPM`→`OL_TARGET_RPM` over `OL_SPEED_RAMP_ITERS`) instead of jumping there
      directly. **Confirmed**: software executed the ramp exactly on schedule (target
      reached at exactly 5.0s, held 10s, stopped on schedule, zero errors), and user
      confirmed the rotor physically tracked it -- smooth acceleration from near-zero up to
      200 RPM, steady hold, no stall or desync. Audible high-pitched noise persists (likely
      the 10kHz PWM carrier tone).

      **Pushed to a 5-step speed staircase (200/400/600/800/1000 RPM)**, ramping 3s + holding
      4s at each level, using the already-logged MT6701 angle as an objective sync check
      alongside visual/audible observation. **Confirmed clean**: all 5 steps completed
      exactly on schedule, zero errors, sensor kept reading continuously varying values
      throughout (never stuck, i.e. never stalled). User confirmed: smooth ramp, smooth
      rotation, no vibration, no stutter, all the way to 1000 RPM.

      **Tackled the PWM noise**: raised MCPWM timer resolution from 10MHz to 32MHz (a clean
      /5 prescale from this chip's 160MHz `MCPWM_TIMER_CLK_SRC_DEFAULT` PLL, confirmed via
      `soc/clk_tree_defs.h`) while keeping `period_ticks` unchanged -- moves the carrier from
      10kHz (audible) to 32kHz (above human hearing) with zero loss of PWM duty resolution
      (still 500 compare steps). **Re-tested clean** (same 5-step staircase, zero errors,
      identical timing to the pre-change run) -- noise reduced but user reports some remains,
      possibly harmonic content; not fully resolved, deprioritized versus moving to closed
      loop (see below).

      **Pole-pair cross-check (7 vs. 4) -- resolved back to 4.** User asked to try reverting
      to legacy's original 7 pole pairs to see if it affected the noise. It doesn't change
      waveform shape (pole pairs only convert electrical<->mechanical domains for RPM
      labeling), so it was tested mainly as a cross-check. Result: the same speed staircase
      that ran clean up to 1000 RPM under 4 pole pairs caused the rotor to visibly stall/lose
      sync at high RPM under 7 (software completed all steps with zero logged errors -- open
      loop can't detect a physical stall itself -- confirmed by user observation only). This
      is expected and **does not validate pole-pair count either way**: electrical Hz =
      RPM/60 x pole_pairs, so a higher assumed pole-pair count directly commands a higher
      electrical frequency for the same RPM label, demanding more back-EMF-compensating
      voltage -- confounding "stalls at this label" with "how many pole pairs is it".
      Reverted to 4 (the value with actual empirical support: smooth, stall-free operation
      across the full tested range). Documented the properly unconfounded verification
      method for later, if wanted: command a known electrical frequency, measure actual
      mechanical RPM from the MT6701 sensor directly, and back out
      `pole_pairs = electrical_Hz / measured_mechanical_Hz`.

      **Brownout incident under a newly tightened bench supply (5V/500mA, 2.5W total
      system budget -- materially tighter than earlier bring-up, which had more headroom).**
      During the closed-loop bench-validation test (BTN_A armed -> calibration -> PD hold),
      the rotor visibly moved (as expected -- calibration's align/direction-detect jerks)
      then the board browned out shortly after. Root cause: two places commanded a full
      step in commanded voltage/angle in a single tick -- `foc_calibration.c`'s align and
      direction-detect steps (`command_dq()` jumped straight to
      `MOTOR_EFFECTIVE_VOLTAGE_LIMIT_V`, the direction step *also* jumping the electrical
      angle by 45 electrical degrees in the same instant), and `control_task.c`'s
      closed-loop `vq` (a target change legitimately creates a large instantaneous error,
      commanding close to full effective voltage in one 1ms tick). The existing current cap
      is explicitly a **steady-state** Ohm's-law model (see `motor_config.h` comment) that
      cannot see the resulting inrush current while the field/rotor catch up to a step --
      exactly the gap the comment already flagged ("ignores... inrush at enable-time").
      Fixed with two complementary changes: (1) `MOTOR_MAX_CURRENT_A` lowered 0.5A->0.2A
      (~1.32V->~0.53V effective limit) as a safety margin appropriate to the new tighter
      supply, which is now shared across the whole board, not just the motor; (2) new
      `MOTOR_VOLTAGE_SLEW_LIMIT_V_PER_S` (8V/s) bounding dV/dt directly -- calibration's
      align/step now ramp over 200ms (`command_dq_ramped()`, 20 steps x 10ms) instead of
      jumping in one tick, and the closed-loop hold's `vq` is slew-limited per iteration
      (`CL_MAX_VQ_STEP_V`) relative to the last actually-applied value. Build verified clean;
      **not yet re-tested on hardware** -- next step is re-running the same bench sequence
      under the 5V/500mA supply and confirming no recurrence before considering this closed.

      **Re-tested -- still browns out, but with a key new data point: only during the
      sustained PD hold, not the (already-fixed) calibration jerks, and confirmed the power
      source is a fixed-voltage plain USB port/power bank (not an adjustable bench supply,
      so "raise V, lower I" isn't actually available -- P=VI only trades off on an adjustable
      source; on a fixed 5V rail, lowering commanded voltage is what lowers current).** This
      also weighs against "too much average motor current" as the mechanism: at the 0.2A/
      ~0.53V cap, modeled average bus current for a 3-phase locked/slow-rotor drive
      (P_avg = 1.5 x Vq^2/R, I_bus = P_avg/Vbus) is only ~32mA -- nowhere near 500mA.
      Combined with it being specific to the *sustained* (up to 15s) PD hold -- the only
      phase running the full 1kHz sensor-read+control loop continuously -- the leading
      theories are (a) baseline board draw (ESP32-S3 + continuous MT6701 SPI polling + USB
      CDC) already consuming most of the 500mA budget with little headroom left, and/or
      (b) instantaneous PWM switching-current spikes (32kHz carrier) tripping the port's
      protection despite low average current -- the kind of thing local bulk decoupling
      capacitance near the STSPIN233 fixes in hardware, not firmware. **Diagnostic step
      taken**: `motor_driver.c`'s MCPWM carrier temporarily reverted 32kHz->10kHz (fewer
      switching edges/sec, audible whine returns -- expected, purely diagnostic) as a cheap,
      reversible test of the switching-spike theory before concluding a hardware fix (bulk
      cap on the 5V rail) is required. **Not yet re-tested on hardware.** Also still
      pending: an actual `esp_reset_reason()` log capture to confirm this is genuinely a
      BROWNOUT reset and not, e.g., a watchdog trip that looks similar -- logging for this
      already exists in `main.c` (prints unconditionally at every boot), just hasn't been
      captured yet.

      **Re-tested at 10kHz -- no brownout, confirming the 32kHz MCPWM carrier (not current
      level) was the actual root cause.** However, holding torque/authority was now very
      weak -- expected, since the current cap was still at the 0.2A/~0.53V level lowered
      while the wrong theory ("average current too high") was being chased. Since the real
      cause (switching-current spikes at 32kHz) is now identified and fixed by staying at
      10kHz, there's no remaining evidence the 0.5A level itself was ever unsafe -- it was
      only ever implicated by a flawed correlation (lower it and lower carrier frequency
      happened together in the same test). **`MOTOR_MAX_CURRENT_A` restored to 0.5A**
      (~1.32V effective limit), 10kHz carrier kept permanently (audible whine accepted as
      the tradeoff until bulk decoupling capacitance is added near the STSPIN233 in
      hardware, which is the electrically-correct fix for switching-current spikes -- 32kHz
      can be revisited once that exists). Voltage slew-rate limiting kept in place (harmless
      defensive measure, not the actual fix, but no reason to remove it). Build verified
      clean; **not yet re-tested on hardware at the restored 0.5A cap** -- next step is
      confirming both no brownout AND acceptable holding torque together.

      **Re-tested at 0.5A/10kHz -- brownout came BACK, now even during calibration (not just
      PD hold), and holding torque was still weak. This disproves the prior conclusion.**
      The earlier claim that "current level was never actually the problem, only the 32kHz
      carrier was" was premature -- it rested on a single confounded comparison where carrier
      frequency AND current cap changed together. Restoring to 0.5A at the *already-fixed*
      10kHz carrier brought the brownout straight back, proving current level independently
      matters too. Corrected understanding: this power source (a plain USB port/power bank,
      hard-limited to 5V/500mA) has a shared budget that BOTH carrier frequency (switching-
      current spikes) and commanded current/voltage level (resistive draw) draw against --
      it isn't one factor alone, and treating a single-variable test as conclusive was the
      mistake. Empirically bounded so far: 0.2A continuous at 10kHz = confirmed safe (if
      weak); 0.5A at 10kHz = confirmed unsafe (even briefly, during calibration). **Fixed**:
      split the single current cap into two (`motor_config.h`) -- `MOTOR_MAX_CURRENT_CONTINUOUS_A`
      (0.2A, kept at the last confirmed-safe sustained level, used by `control_task.c`'s PD
      hold) and `MOTOR_MAX_CURRENT_PEAK_A` (0.3A, an untested bisection point between
      confirmed-safe and confirmed-unsafe, used only by `foc_calibration.c`'s brief ~1.5s
      align/step) -- since a brief bounded pulse and a 15s sustained hold have different risk
      against what looks like an inverse-time-tripping port limit. Build verified clean;
      **not yet re-tested on hardware.** Given the pattern so far (every single-variable
      conclusion this incident has been wrong), next verification should vary ONE thing at a
      time and get a clean run before declaring victory. Also worth a decisive test whenever
      convenient: run this exact 0.5A/10kHz build on a different, known-beefier power source
      (e.g. a wall PD adapter or a USB3 port) -- if it runs clean there, that conclusively
      separates "this firmware has a bug" from "this specific power source's budget is the
      hard ceiling," which still hasn't been directly established either way.

      **RESOLVED. Tested on a beefier PD-capable supply (this board has an onboard USB-PD
      chip with its own NVS-baked negotiation profile, not yet driven by firmware, but it
      negotiates independently of the MCU) -- no brownout at all, even at the same 0.2A/
      0.3A caps.** This confirms the plain 5V/500mA USB port really was the hard ceiling all
      along -- not a firmware bug, not bad hardware. But holding torque was still weak on
      the PD supply too, because 0.2A/0.3A were bisected specifically against the *weak*
      port's budget, not this one -- an easy, expected consequence, not a new mystery.
      This also finally explains, with real physics rather than another guess, why
      open-loop rotation felt strong at a given current cap while closed-loop holding
      (and calibration's static align/step) didn't at the same cap: current = (V -
      back_EMF)/R. While rotating, back-EMF opposes the applied voltage, so real current
      stays below the Ohm's-law worst-case model this cap is sized against. While holding
      static (zero speed = zero back-EMF), real current sits at the *full* modeled worst
      case -- holding is inherently the more current-hungry case for the same nominal cap,
      not a firmware inconsistency. **Fixed**: merged the split cap back into one, raised to
      0.5A (`MOTOR_MAX_CURRENT_CONTINUOUS_A` = `MOTOR_MAX_CURRENT_PEAK_A` = 0.5A, ~1.32V
      effective) -- matches the level that gave strong static holding torque earlier in
      open-loop bring-up. Build verified clean; **not yet re-tested on hardware**.
      Also asked whether an existing reference FOC implementation exists for this exact
      problem (voltage-mode holding torque, no current sensing): yes -- legacy_fw already
      uses SimpleFOC, which is the closest prior art (same voltage-mode-no-current-sensor
      approach, `voltage_limit` parameter serving the same role as `MOTOR_EFFECTIVE_VOLTAGE_LIMIT_V`
      here). One thing SimpleFOC also implements that this port doesn't yet -- SVPWM
      (space-vector modulation, already a TODO item below) -- was considered as a possible
      torque fix here but is NOT actually relevant to the current weak-torque symptom: SVPWM
      only extends the usable range before the Vbus/2 topology ceiling (2.5V) is reached,
      and the current cap (0.53-1.32V range) is nowhere near that ceiling -- the real
      bottleneck has been the current safety cap the whole time, not modulation headroom.
      SVPWM remains worth doing eventually (Phase 2a TODO), just not as a fix for this.

      **Regression re-reported: brownout on laptop again, weak even on PD charger, and user
      confirms re-flashing genuinely-old firmware gives strong torque with no brownout on
      the same hardware -- pointing at a real regression introduced somewhere in this
      session's changes, not (only) a power-budget question.** To isolate "closed-loop-path
      regression" from "something wrong at a more fundamental level (hardware/cap/PWM)",
      reconstructed the original open-loop rotation test (removed earlier this session when
      the closed-loop path was added -- `NanoDepsidf/` was never under git, so there's no
      history to restore verbatim from) as a **BTN_C-gated diagnostic mode** in
      `control_task.c`, run on the exact same build/settings as the currently-weak
      closed-loop test (same 0.5A cap, same 10kHz carrier). Hold BTN_A+BTN_C at boot to run
      it: ramps 20->200 RPM over 5s (direct-axis/Vd trick, no feedback needed), holds 5s,
      ramps down. **Not yet tested.** If this reconstructed test is also weak/browns out,
      the regression is at the driver/cap/PWM level (or hardware); if it's strong like the
      original, the regression is specific to the closed-loop code path added/changed this
      session (calibration, PD hold, slew-rate limiting, or the current-cap plumbing).

      **CONCLUSIVE. Tested the reconstructed open-loop diagnostic on the same laptop supply
      that browns out the closed-loop test, at the exact same 0.5A cap and 10kHz carrier:
      strong (held firmly by hand) and zero brownouts.** Same supply, same cap, same
      carrier, opposite outcome from closed-loop -- there is no hardware regression and no
      driver/cap/PWM-level bug. This is exactly the STATIC-vs-ROTATING physical distinction
      already documented above (current = (V-back_EMF)/R; rotating has back-EMF assist,
      static holding doesn't), now confirmed with a clean, unconfounded, same-supply A/B
      test rather than inferred. It also retroactively reveals a naming/framing mistake:
      the earlier CONTINUOUS-vs-PEAK split (calibration = brief/PEAK, PD hold = sustained/
      CONTINUOUS) modeled the wrong axis -- calibration's static align/step and the PD
      hold's static holding are electrically the *same* worst case (near-zero back-EMF),
      regardless of duration; what actually differs is static vs. rotating, which the old
      naming didn't capture at all (both closed-loop paths are static). **Fixed**: renamed
      and re-split in `motor_config.h` -- `MOTOR_MAX_CURRENT_STATIC_A` (0.2A, used by
      `foc_calibration.c`'s align/step AND `control_task.c`'s PD hold -- both static
      scenarios) and `MOTOR_MAX_CURRENT_ROTATING_A` (0.5A, used only by the open-loop
      diagnostic -- now directly confirmed strong and brownout-free on the weakest supply
      tested). `MOTOR_EFFECTIVE_VOLTAGE_LIMIT_V` split into
      `MOTOR_EFFECTIVE_STATIC_VOLTAGE_LIMIT_V` / `MOTOR_EFFECTIVE_ROTATING_VOLTAGE_LIMIT_V`
      to match. Build verified clean; **not yet re-tested on hardware**. Real takeaway for
      Phase 2b later: static holding torque on this board, on a weak supply, is genuinely
      capped lower than rotating torque -- not a bug to keep chasing, a physical constraint
      to design the haptic feel around (or fix in hardware via bulk decoupling capacitance
      near the STSPIN233, which would raise the static ceiling too).

- [ ] Clarke/Park + SVPWM implementation (voltage mode, no current loop) -- note: basic
      inverse Park/Clarke already exists in `src/foc_math.c` for the open-loop test above;
      this item is about the full control-oriented version (forward transforms too, SVPWM
      sector logic for extra bus utilization) once closed-loop control lands
- [x] Basic position/velocity control primitives (command an angle/torque, hold it — no
      "feel" design here, just proving the driver obeys commands accurately). Implemented:
      `src/foc_calibration.c` (blocking two-step routine -- direct-axis align at electrical
      0, then a small 45°-electrical step to detect sensor direction; aborts safely,
      `.valid=false`, if the rotor doesn't visibly move during the step rather than trusting
      a noisy reading) and a closed-loop position-hold test in `control_task.c` (P controller
      on position error -> quadrature-axis voltage, using the real calibrated electrical
      angle -- correctly Vq this time, not the Vd trick the open-loop test used, since Vq is
      the actual torque-producing axis once you have real angle feedback). New failure mode
      closed-loop introduces that open-loop didn't have: a calibration sign error would be
      positive feedback (push harder away from target as error grows) rather than negative --
      guarded by `CL_DIVERGE_ABORT_RAD` (aborts if position error ever exceeds ~86°, which a
      correctly-signed small-signal hold test should never approach). Voltage/current safety
      limits are unchanged from open-loop testing, so this isn't a new electrical-safety
      concern even if the sign is wrong -- just a correctness one.

      **SUCCESS on the first real attempt.** Calibration consistent across repeated boots
      (direction=-1 both times; offset differs each run only because the rotor rests at a
      different raw angle at power-up, not a bug). At rest, the controller holds error
      ~0.005 rad (~0.3°) with a correspondingly tiny correction voltage -- essentially
      locked. User physically confirmed: felt the 2 expected calibration jerks (align, then
      the direction-detection step), then gentle resistance and spring-back when nudged in
      **both directions** (CW and CCW) -- true bidirectional closed-loop holding, not a
      fluke of one direction happening to work. Log during a firm nudge showed textbook
      P-controller behavior: error swung 0.777 -> 0.440 -> -0.547 rad with `vq` correctly
      flipping sign each time to push back toward target (real negative feedback, not
      divergence) -- some overshoot/oscillation, expected for proportional-only control,
      not a bug; a damping (D) term would tighten settling if wanted later. This is the
      first working closed-loop control of the ESP-IDF rewrite.

- [x] Calibration routine storage via NVS (replacing `DeviceSettings::toSPIFFS`/
      Preferences-style storage). `nvs_flash_init()` added to `main.c` (standard erase-and-
      retry pattern for a partition needing reformat). `foc_calibration_load/save()` store
      the whole `foc_calibration_t` as one NVS blob (namespace `foc_cal`), with a sanity
      check on load (direction must be exactly +-1, offset must be finite) so corrupted/
      partial data is never trusted over a fresh calibration. Boot flow: load cached
      calibration if present and valid; otherwise run `foc_calibration_run()` and save the
      result. BTN_B held alongside BTN_A during the arm window forces a fresh calibration
      even when a valid one is cached, for whenever the sensor/motor mounting is deliberately
      changed. **Confirmed working**: forced a fresh calibration (direction=-1,
      offset=-0.4096 rad, saved), then re-armed without BTN_B on the next boot and it loaded
      the exact same values from NVS and skipped the jerks entirely, exactly as designed.
- [x] Bench validation: motor tracks commanded angle/torque accurately, low jitter.
      Implemented: the closed-loop test now steps through 5 target positions (a tour around
      a full revolution and back: 0, +90°, +180°, -90°, 0, relative to wherever the rotor
      starts) instead of only holding the starting point, 3s per target (move + settle +
      measure), logging steady-state max/avg |error| over the last 1s of each target's hold.
      Also added a **D (damping) term**: `CL_KD=0.05 V/(rad/s)` on measured velocity (not
      raw error-derivative, which would spike on every target change) alongside the existing
      `CL_KP=1.5`, aimed at the overshoot/oscillation seen on a firm nudge during the P-only
      test.

      **First run found a real bug in the test itself, not the control loop.** Target 1 held
      excellently in both the fresh-calibration and NVS-load runs (steady-state max|err|
      0.003-0.011 rad, avg 0.0007-0.0078 rad -- tight, and the PD gains look like a genuine
      improvement over the P-only test's ~0.005 rad). But the very next tick after target 1's
      summary printed, `CL_DIVERGE_ABORT_RAD` (1.5 rad) fired and aborted the test. Cause: a
      legitimate 90° target step creates a ~1.57 rad error for an instant, before the motor
      has had any time to move -- almost exactly the abort threshold, so every target change
      false-tripped it. Not a calibration or controller problem. Fixed with
      `CL_DIVERGE_CHECK_SETTLE_ITERS` (1s grace period after each target change before the
      divergence check applies, using the already-existing per-target `s_cl_iter` counter).

      **Re-tested with the fix.** Target 1 held essentially perfectly again (error
      0.0000-0.0012 rad for the first 2s) -- confirms the PD damping improvement is real and
      repeatable. Mid-hold, a large disturbance appeared (error jumped to -0.80 rad with no
      target change), then after the step to target 2 the error stayed large
      (1.77 -> 1.57 rad) despite ~max commanded voltage (1.32V) for a full second, eventually
      re-tripping `CL_DIVERGE_ABORT_RAD` -- motor disabled ("goes limp"). **User confirmed
      this was them manually testing resistance by hand** ("when I rotate the knob a little
      too much it clicks and goes limp... motor tried to overcome resistance slightly").
      This is the safety net working exactly as designed, not a bug: the motor is
      deliberately current-limited (0.5A cap, see motor_config.h) and is easily overpowered
      by a firm manual push over a large angle -- very different from the earlier small
      nudge-and-release test, which stayed within the motor's available torque margin.
      `CL_DIVERGE_ABORT_RAD` doing its job (disabling rather than fighting indefinitely once
      error is clearly not converging) is the correct behavior for a bring-up test; it was
      not loosened.

      **Net result: bench validation goal is met.** Closed-loop position accuracy is proven
      (target 1's near-zero error, both before and after the PD change), the damping
      improvement is confirmed, and the divergence safety net is confirmed to behave
      correctly under a real stress case (human resistance), not just in theory. The full
      unattended 5-target tour was never completed hands-off (both attempts were interrupted
      by manual knob testing) -- optional follow-up if a completely clean run end-to-end is
      wanted, but not considered blocking given what's already been directly confirmed.

### Phase 2b — Haptic control program [x] COMPLETE

Redesigned from scratch rather than ported from `legacy_fw`; ended up on a
nearest-grid-point linear ("sawtooth") PD detent law plus a separate transient "click"
pulse, arrived at through several iterations logged below.

- [x] Hand-operable haptic detent demo, originally armed via **BTN_A+BTN_B+BTN_D at boot**
      (`control_task.c`, `s_haptic_mode`). Reuses the same NVS-cached calibration as the
      bench tour. Runs indefinitely (no hard timeout, unlike the other diagnostics) since
      it's meant for open-ended hands-on testing.
      **Promoted to the default/main boot behavior** (no combo needed) once mature enough
      -- explicit decision, not a bring-up artifact: `s_haptic_mode = !armed` now, so a
      plain reset/reconnect runs haptic mode directly, while BTN_A held at boot instead
      arms the legacy diagnostic suite (open-loop/pole-pair/bench-tour). This is a
      deliberate, scoped departure from the "motor never energizes on a plain reset"
      safety posture from Phase 2a bring-up -- that posture protected an _unvalidated_
      control loop; haptic mode is validated and has its own independent abort (error
      exceeding one full detent spacing signals a real bug, not normal operation), and is
      exactly the mode meant to run continuously. The old explicit BTN_A+BTN_B+BTN_D combo
      is retired. **Not yet re-tested on hardware after this change.**
- [x] **Control law**: retarget every tick to whichever of N evenly-spaced detents
      (`s_haptic_num_detents`, live-adjustable, default 12) is nearest, then
      `Vq = Kp*error - Kd*filtered_velocity`. Went through a full architecture detour first
      (a sine-shaped torque profile was tried and abandoned — its slope is inverted
      relative to what a mechanical click needs: steepest at rest, flattest at the snap
      point, causing a mushy transition plus persistent center oscillation) before landing
      back on the structurally-correct linear/sawtooth version (steep at the boundary,
      gentle at center).
- [x] **Live tuning while running**: BTN_A/BTN_B step Kp up/down, BTN_A+BTN_C /
      BTN_B+BTN_D combo steps Kd down/up, BTN_C/BTN_D step detent count down/up (see
      constants and debounce/cooldown logic in `control_task.c`).
      Discovered on hardware: Kp=0 (`HAPTIC_KP_MIN` lowered to reach it), Kd~0.055 gives a
      distinct "viscous fluid" feel — pure velocity damping, no positional spring.
- [x] **Velocity coasting** (legacy-inspired): above `HAPTIC_COAST_VELOCITY_RAD_S`
      (fast hand-flick speed), apply zero torque and let the knob spin freely on momentum
      rather than fighting it; only slow/fine adjustment gets the restoring spring.
- [x] **Rate-independent velocity filter**: `HAPTIC_VELOCITY_FILTER_TAU_S` is a real time
      constant (not a bare EMA alpha), so it stays correct regardless of
      `CONTROL_LOOP_PERIOD_US` — a fixed alpha would have silently gotten weaker when the
      loop rate was later raised 1kHz→10kHz. This class of bug (fixed per-tick constants
      silently changing real-world meaning across a loop-rate change) also motivated the
      `MS_TO_ITERS()`/`ITERS_TO_SEC()` helpers used throughout `control_task.c`.
- [x] **Audible "click" transient**: the background PD alone, even correctly shaped and
      slew-limited, wasn't audible — the slew limiter smears any transition over ~13ms, far
      too slow to produce a real sound. Solved by injecting a short, deliberately
      un-slew-limited two-stage pulse (brief over-driven "impact" that clips into a sharp
      percussive edge, then a longer lower-amplitude decaying "tail" ring-down) on top of
      the still-slew-limited background, triggered on each detent-index edge. Retuned twice
      by ear (single-tone → two-stage → lower-frequency "thock").
- [x] **Direction-symmetry bug found and fixed**: the pulse's fixed waveform phase didn't
      account for which way the knob was turned, so it reinforced the snap on one rotation
      direction and partially fought the background push on the other. Fixed by capturing
      the background Vq's sign at the exact trigger tick and flipping the pulse to match.
- [x] **Midpoint chatter bug found and fixed**: sitting still exactly at a detent midpoint
      let sensor noise alone flip the nearest-detent pick every tick, chattering between
      two targets. Fixed with hysteresis (`HAPTIC_DETENT_HYSTERESIS_FRAC`) — the committed
      detent only switches once rel moves past the midpoint by an extra margin, not on
      whatever's instantaneously nearest.
- [x] Bench/hands-on validation: feel and sound confirmed good on real hardware by direct
      testing across all the iterations above (gain tuning, viscous-fluid mode, click
      timbre, direction symmetry, midpoint stability all separately confirmed on hardware).
- [x] **Fast-rotation pulse-retrigger bug found and fixed (discovered via Phase 8's menu
      testing, but a pre-existing bug in this phase's own pulse logic, unrelated to the
      menu itself)**: `s_haptic_pulse_ticks_remaining` reset to `HAPTIC_PULSE_DURATION_ITERS`
      unconditionally on every detent-index change, with no guard against retriggering
      mid-pulse. The pulse takes ~24ms to decay; a fast flick crosses detents faster than
      that, so each new crossing restarted the deliberately over-driven (6V, clipped to the
      safety cap) IMPACT phase before the previous one could decay -- producing a
      **sustained**, repeatedly-refreshed near-max clamped voltage in the direction of
      travel instead of a brief click. Felt on hardware as the knob "actively being driven
      further / spinning by itself" during fast rotation. The audio click already guards
      against exactly this scenario (`MIN_CLICK_RETRIGGER_S`, Phase 7/`i2s_task.c`); the
      electrical pulse never had the equivalent guard. Only reachable in practice by
      genuinely fast, sustained flicking -- normal Kp/Kd/timbre feel-testing during this
      phase's original development never crossed detents that quickly, which is why this
      went unnoticed until Phase 8's menu-navigation testing (which does involve fast
      exploratory flicks) surfaced it.

      **First fix (guarded the whole ~24ms pulse) was too coarse.** Re-tested: overshoot
      improved but clicks now felt weak specifically while navigating the menu. Root cause
      of *that*: blocking retrigger for the full pulse (impact + tail) also blocked it
      during the much gentler ~1V tail, which was never the dangerous part -- only the
      clipped ~6V impact phase (first ~4ms) could chain into sustained near-max voltage.
      Any moderately brisk menu-browsing cadence has plenty of crossings landing inside that
      full 24ms window, so most were silently getting no click at all. **Refined fix**: the
      guard now only spans the impact phase (`HAPTIC_PULSE_IMPACT_DURATION_ITERS`, ~4ms) --
      retriggering once past it just starts a fresh pulse a bit early (the desired snappy
      feel during fast navigation), not a safety concern.

      **Re-tested: overshoot still present, described more precisely this time --
      "spins fast on its own, still producing clicks" (motor clicks, not audio), alongside
      "Kd feels weaker" specifically during fast rotation.** Both trace to the SAME real
      root cause, found by re-reading the coast-velocity code the pulse sits next to:
      `HAPTIC_COAST_VELOCITY_RAD_S` already zeroes the *background* torque above a fast
      hand-flick speed (deliberate -- "let it spin freely on momentum"), but the click-pulse
      arming logic had **no idea this coast state existed** -- it fired a full, cap-level
      voltage kick on every single crossing regardless. During a fast flick that enters
      coast, every crossing was still injecting an active push in the direction of travel --
      actively adding energy to what should have been a passive, torque-free spin: more
      speed -> more crossings -> more kicks -> more speed, a genuine positive-feedback loop.
      This explains both complaints as one mechanism: reduced background resistance is
      coasting working exactly as designed, while the continuing active clicks accelerating
      the spin is the bug. Notably: **this bug is entirely within `control_task.c`'s own
      single-core logic** -- not a menu/Core-1 interaction at all (the user directly and
      reasonably questioned whether Core 1 could somehow be reaching back into Core 0's
      control loop; it can't, and doesn't here -- `menu_input_*()` are ordinary function
      calls executing on Core 0 within the same tick, and the only actual cross-core traffic
      is Core 1 *reading* what Core 0 already wrote). Menu testing simply happened to be the
      first thing in this session fast enough to reach the coast threshold. Fixed: pulse
      arming now also requires `!is_coasting`, matching the background torque's own gate
      exactly -- once genuinely coasting, no new clicks fire until velocity drops back into
      the normal range. Build verified clean (RAM 27.0%/88416B, Flash 52.6%/689373B). **Not
      yet re-tested on hardware.**

### Phase 3 — USB HID (keyboard / mouse / gamepad) [~] IN PROGRESS — first slice built, not yet hardware-tested

- [x] `esp_tinyusb` integration, composite HID descriptor (keyboard + mouse + gamepad;
      MIDI USB interface dropped per scope change). Adapted from this project's installed
      framework's own reference example
      (`examples/peripherals/usb/device/tusb_hid`) rather than guessed -- confirmed the
      exact `esp_tinyusb` API shape (this IDF version pulls in `espressif/esp_tinyusb
    ^2.3.0` via a new `src/idf_component.yml`) against that example rather than an older
      /different-version tutorial. One HID interface, three report IDs (keyboard=1,
      mouse=2, gamepad=3) rather than three separate interfaces -- simpler, and how most
      real composite keyboard+mouse+gamepad devices do it. Implemented in `usb_task.c`.
      `CONFIG_TINYUSB_HID_COUNT=1` added to `sdkconfig.defaults` -- the only Kconfig entry
      TinyUSB's HID class needs; endpoint size/polling interval/report-descriptor length
      are literal args in `TUD_HID_DESCRIPTOR`, not Kconfig. Build verified clean (Flash
      24.8%, up from 22.6% -- consistent with just the TinyUSB+HID stack, not a WiFi pull-
      in). **Confirmed on hardware: enumerates correctly** (HID recognized by host;
      keyboard self-test 'a' tap visibly typed, confirming report delivery end-to-end).

      **Confirmed the flagged debug-workflow conflict, then hit a second, more serious one
      -- both now resolved.** ESP32-S3 has one physical USB-OTG PHY shared between the
      native USB-OTG controller (what TinyUSB takes over) and the built-in USB-Serial-JTAG
      controller (this project's secondary console). Adding a CDC-ACM interface alongside
      HID (endpoint budget confirmed fine: ESP32-S3's OTG FS has 7 total endpoint numbers/
      5 usable IN, confirmed directly from TinyUSB's `dwc2_esp32.h` port source -- CDC's
      2 IN+1 OUT plus HID's 1 shared IN is comfortably inside budget, so all 3 HID usages
      stayed) fixed the console side and was confirmed via `pio device list`: port
      enumerates correctly as `303A:4005`, `SER=NANOD-DEV`, `Nano D++` -- exactly our
      descriptor's strings.

      **But `pio run -t upload` then hung at "Connecting..." even though esptool found the
      port.** Root cause: this board's upload flow
      (`use_1200bps_touch` in `boards/nanofoc_d.json`) depends on the *running* firmware
      responding to a 1200-baud line-coding request by resetting into the ROM bootloader
      -- legacy firmware got this for free from `Adafruit_TinyUSB`
      (`legacy_fw/src/main.cpp`'s `TinyUSBDevice.setID(0x239A, 0x8010)` confirms it used
      that library), but bare `esp_tinyusb`/TinyUSB does not implement this on its own --
      we never defined `tud_cdc_line_coding_cb()`, so the request was silently ignored.
      Investigated porting Arduino-ESP32's exact mechanism
      (`usb_persist_restart()` in `esp32-hal-tinyusb.c`): shut down the OTG controller,
      flip the PHY mux back to USB-Serial-JTAG (`RTC_CNTL_USB_CONF_REG`/
      `USB_SERIAL_JTAG_CONF0_REG` bit-banging), force a bus reset via GPIO, set the ROM's
      `RTC_CNTL_FORCE_DOWNLOAD_BOOT` persist flag, then `esp_restart()`. Deliberately did
      **not** implement this: genuinely chip-specific, hard-to-verify-without-hardware
      register sequencing (even Arduino-ESP32's own maintainers special-case IDF ≥6.0,
      the version this project is on, differently) -- not worth the risk for a convenience
      feature when a much simpler alternative exists.

      **Resolved with a boot-time USB-mode-select instead** (`main.c`,
      `usb_serial_mode_requested()`): hold BTN_C+BTN_D at boot (own ~2s poll window,
      independent of `control_task.c`'s BTN_A-prefixed diagnostic combos) to skip
      installing TinyUSB entirely for that boot -- the PHY then stays on its default
      USB-Serial-JTAG routing, which is the *exact* path that handled console + the
      1200bps-touch reset flawlessly through every phase before Phase 3 ever touched USB,
      with zero custom code. Release both (normal boot) to get the composite HID+CDC
      device. Sidesteps the register-level problem entirely rather than solving it.
      **Not yet re-tested on hardware after this change** -- next step is confirming
      `pio run -t upload` now works normally when BTN_C+BTN_D are held at boot, and that
      normal (unheld) boot still gives the working composite HID+CDC device confirmed
      above.

- [x] HID report generation + end-to-end test, first slice: a periodic self-test (once
      `tud_mounted()`) sent a keyboard 'a' tap and a small mouse-square, proving report
      delivery actually works. Confirmed on hardware, then **removed** once it had served
      that purpose (kept sending forever otherwise -- it was a proof, not a feature).
      Gamepad is present in the descriptor (proves the composite descriptor itself
      enumerates correctly) but was never exercised by the self-test -- harder to verify
      visually without a gamepad-test utility; descriptor presence was enough for that
      slice. Same "prove the dumbest real thing first" pattern as every other phase in
      this plan (Phase 1's dummy cross-core HID message, Phase 7's BLIP click).
- [x] **First real mapping-engine decision made**: knob rotation -> mouse scroll wheel,
      one detent crossing = one wheel step (not the whole "Open decisions" mapping-engine
      question below -- just this one axis). `ipc.h`'s `hid_report_msg_t` placeholder
      replaced with a real typed message (`HID_EVENT_MOUSE_WHEEL` + signed `wheel_delta`)
      rather than raw report bytes, so `control_task.c` doesn't need to know TinyUSB's
      API. Produced in `control_task.c` at the exact same detent-index edge that already
      drives the haptic click and audio click (Phase 2b/7) -- one shared trigger point,
      three effects. Direction taken from the filtered rotation velocity's sign at that
      instant, deliberately **not** from `detent_index`'s own raw increasing/decreasing
      value -- `detent_index` is derived from a `wrap_pi`'d angle, so it jumps once per
      full revolution at the +-pi wrap boundary; velocity has no such discontinuity, so it
      won't glitch on continuous rotation. Which physical direction (CW/CCW) maps to
      scroll-up vs. scroll-down is a single flippable constant (`HID_WHEEL_SIGN`),
      untested -- same pattern as other direction ambiguities in this file
      (`MOTOR_POLE_PAIRS`/`s_cal.direction`). `usb_task.c`'s queue-drain loop now actually
      acts on messages (`tud_hid_mouse_report`) instead of discarding them, blocking on
      the queue with a timeout rather than polling on a fixed interval, for minimal added
      latency. Queue depth raised 8->32 for burst headroom (fast spins can generate many
      detent crossings per drain cycle -- see the ~200/s figure from Phase 2a's open-loop
      speed testing). The old Phase 1 "dummy message every 100 iterations" producer was
      removed from `control_task.c` -- it would have injected bogus messages into the
      same queue real scroll events now use. **Not yet tested on hardware.**
- [ ] Key input driver (GPIO read/debounce, replacing AceButton) -- not started. Belongs
      on Core 0 per the architecture log (control loop already owns key/knob reads), not
      folded into `usb_task.c`.
- [ ] Mapping engine, the rest of it: decide how much of `legacy_fw`'s flexible
      profile/mapping system (`mapping.md`) survives now that MIDI/OSC/gamepad-axis
      breadth is less relevant — **scope decision still pending** for keys and anything
      beyond the one knob axis above.
- [ ] **HID Type menu decision** (Phase 8): "HID Type" (Keyboard/Mouse/MIDI) is an
      **input-mapping mode switch**, not a USB descriptor change -- the composite
      keyboard+mouse+gamepad descriptor above stays fixed/always-enumerated, no
      re-installation or re-enumeration. Mouse mode = today's existing knob->wheel
      mapping. Keyboard mode needs an actual key-mapping decision (which keys does the
      knob/buttons send? -- not yet decided) plus a new `tud_hid_keyboard_report()` call
      path (the keyboard HID interface is already enumerated but no keyboard report has
      ever been sent). MIDI mode is settings-only (Channel/Note stored, nothing
      transmitted) -- does **not** reinstate the USB MIDI interface dropped above; see
      Phase 8 task 8 for real USB-MIDI transport, explicitly deferred.
- [ ] End-to-end hardware test: OS recognizes device as keyboard+mouse+gamepad, and the
      knob's scroll-wheel mapping actually scrolls something on a real host -- not yet
      run.

### Phase 4 — Display [~] IN PROGRESS — first bring-up slice built, not yet hardware-tested

- [x] `esp_lcd_gc9a01` panel bring-up (init sequence). Added `espressif/esp_lcd_gc9a01
    ^2.0.4` + `espressif/esp_lvgl_port ^2.9.0` to `src/idf_component.yml` (lvgl itself
      -- latest 9.x -- comes in transitively via `esp_lvgl_port`'s own dependency spec,
      not pinned separately). Own SPI bus (`SPI3_HOST`), deliberately kept separate from
      MT6701's `SPI2_HOST` (`mt6701.c`) -- confirmed by reading that file rather than
      assumed, no sharing/contention between sensor reads and display updates.
      Implemented in `display_task.c`. Rotation/mirror not addressed yet (default
      orientation only) -- follow-on once real screen content needs it.
- [x] `esp_lvgl_port` integration (task, tick, buffer). Went with the partial
      (40-row, internal-SRAM, single-buffered) option rather than a full-frame PSRAM
      buffer for this first slice -- matches legacy's own "1/10-screen partial buffer"
      approach, lower-risk than getting PSRAM/DMA cache-coherency right blind on a first
      bring-up. Revisit if partial-buffer refresh rate becomes a real limit once real
      screen content exists. LVGL9 (the version this component defaults to) renamed
      several APIs from LVGL8 -- e.g. `lv_disp_get_scr_act()` is
      `lv_display_get_screen_active()` now, `lv_disp_t` is `lv_display_t` -- confirmed by
      reading the actual installed `lvgl` package headers rather than assumed from
      possibly-stale tutorials/memory.
- [x] Backlight via `driver/ledc` (port of GPIO5 PWM control) -- ramped up only after
      panel init completes, avoiding a visible flash of uninitialized framebuffer content.
- [x] Smoke test: one static "Nano D++" label, proving the panel+LVGL pipeline actually
      renders -- same "prove the primitive first" pattern as every other phase in this
      plan. **Confirmed on hardware, but not clean on the first try**: background
      rendered dark purple (expected a light theme background) with visible artifacts,
      and content appeared mirrored. Both hardware-orientation/format facts about this
      specific panel, not code bugs -- same category as `MOTOR_POLE_PAIRS`/
      `HID_WHEEL_SIGN` elsewhere in this project, empirically determined per-board, not
      derivable from the datasheet alone. Fixed with two flippable constants in
      `display_task.c`: `LCD_SWAP_BYTES=true` (SPI TFT controllers typically want RGB565
      big-endian on the wire; ESP32 stores it little-endian in memory -- a symmetric
      color like pure white/black renders right by coincidence when this is wrong, which
      is why the smoke test's _label_ looked fine while the _background_ didn't) and
      `LCD_MIRROR_X=true` (this panel's FPC mounting vs. the vendor driver's default
      MADCTL assumption). **Both correct on the first guess** -- re-tested: black
      background, white text, correct orientation, confirmed by direct user report.
      **Requirements finalized after that confirmation**: 90-degree CCW rotation on top
      of the above (requested), plus locking in black background/white text as the
      default screen's spec going forward (already what the code did, now explicit).
      Rotation implemented as LVGL _software_ rotation (`flags.sw_rotate` +
      `lv_display_set_rotation()`, `LCD_ROTATION`) layered on top of the
      already-confirmed hardware orientation, rather than re-deriving a 4th
      swap_xy/mirror_x/mirror_y combination -- decouples "which quarter-turn" from the
      per-board hardware-mirroring guesswork above. Direction picked by tracing LVGL's
      own `lv_display_rotate_point()` transform (not assumed from the enum name):
      `LV_DISPLAY_ROTATION_90` rotates content clockwise -- reasoned "so CCW must be 270"
      and got it backwards regardless; **hardware feedback: the actual requirement is 90
      CW, so `LCD_ROTATION` is `LV_DISPLAY_ROTATION_90` directly.**

      **Also hit a second, unrelated issue on the same hardware pass**: background
      rendered white where black was expected -- a clean full-color inversion
      (0x0000<->0xFFFF), not the garbled/wrong-hue kind of symptom the earlier
      `LCD_SWAP_BYTES` fix addressed. Correctly diagnosed as a different mechanism before
      touching code: byte-swapping is a no-op on both 0x0000 and 0xFFFF (both bytes are
      already equal within each), so `LCD_SWAP_BYTES` genuinely cannot cause this --
      this also retroactively means the earlier "both correct on the first guess"
      black/white smoke test never actually exercised `LCD_SWAP_BYTES` at all (black and
      white are the two RGB565 values invariant under byte-swap), so that constant
      remains formally unconfirmed, just not implicated in either symptom seen so far.
      Fixed with `esp_lcd_panel_invert_color(panel_handle, true)` (`LCD_INVERT_COLOR`) --
      sends the panel controller's INVON/INVOFF command, a real, common GC9A01 hardware
      polarity fact independent of anything on the software/LVGL side.

      **Re-tested: colors fixed (black bg/white text confirmed), but content came back
      mirrored on X** even though `LCD_MIRROR_X` was still `true`. Toggling that constant
      to `false` made **no visible difference at all** -- the real clue. Traced into
      `esp_lvgl_port`'s source (`esp_lvgl_port_disp.c`,
      `lvgl_port_disp_rotation_update()`): with `flags.sw_rotate=true` (required for the
      `LCD_ROTATION` feature), that function returns immediately at every call site, init
      included -- `esp_lcd_panel_mirror()`/`swap_xy()` are the *only* things that ever send
      MADCTL to the panel, and in sw_rotate mode they are simply never called. `LCD_MIRROR_X`
      was dead configuration, not a wrong value, which is exactly why flipping it changed
      nothing. **Fixed**: `esp_lcd_panel_mirror()`/`swap_xy()` now called directly in
      `panel_and_lvgl_init()` right after panel init, bypassing `esp_lvgl_port`'s (dead, in
      this mode) rotation plumbing entirely -- `LCD_MIRROR_X` restored to `true` since it's
      now actually reaching hardware.

      That real mirror being newly-active then made the previously-picked rotation angle
      wrong by exactly 180 degrees (content came out upside-down) -- expected, since
      composing a genuine reflection with a rotation flips handedness. `LCD_ROTATION`
      reverted from `LV_DISPLAY_ROTATION_270` back to the originally-reasoned
      `LV_DISPLAY_ROTATION_90`, now correct given the mirror fix actually landed.
      **Confirmed on hardware: correct position, correct (non-mirrored) orientation,
      right-side up** -- all three fixes (invert, direct mirror call, rotation angle) now
      validated together.

- [x] **Mock status/menu UI built on top of the confirmed-working panel** (not in the
      original Phase 4 task list, added once the panel itself was proven): default screen
      shows the current detent index (wrapped to `0..num_detents-1`); BTN_D pops a mock
      3-item menu overlay ("Menu Item 1/2/3") that the knob navigates (wrapping at both
      ends) instead of its normal job. Deliberately just labels + visibility toggling,
      not a real menu framework -- proving the toggle/navigate/render pipeline end to
      end, matching this whole plan's "prove the primitive first" pattern once more.
      New shared module `ui_state.h`/`.c` (Core 0 producer in `control_task.c`, Core 1
      consumer in `display_task.c`, plain atomics -- unlike `audio_trigger.h`'s click
      queue, the display only ever wants the _latest_ value on its own redraw timer, so
      there's no event history to preserve). Real design decisions made along the way,
      confirmed with the user before implementing (see session log if this needs
      revisiting): - **BTN_D retired entirely from haptic live-tuning** (was solo detent-count-up and
      `BTN_B+BTN_D` damping-up) and repurposed as a dedicated, non-combo menu toggle
      (press to open, press again to close). Detent count can still be _decreased_
      (BTN_C) and Kd _decreased_ (`BTN_A+BTN_C`), but not increased via button anymore
      -- accepted trade, since menu-driven config is the intended eventual home for
      this kind of tuning anyway, not a gap to patch with a new combo. - **Underlying haptic feel is unchanged while the menu is open** -- same Kp/Kd/
      detent-count/click feel; only what a detent crossing _means_ changes (menu-index
      step instead of scroll-wheel step). A menu-specific feel (e.g. exactly 3 coarse
      detents matching the 3 items) would be a nicer follow-up, not done here. - **No HID wheel events are sent while the menu is open** -- `control_task.c`
      simply doesn't enqueue them in that branch, rather than having `usb_task.c`
      filter them out downstream. - Also caught and fixed one deprecated-API warning along the way:
      `lv_obj_add_flag`/`remove_flag(obj, LV_OBJ_FLAG_HIDDEN)` are deprecated in this
      LVGL version in favor of the simpler `lv_obj_set_hidden(obj, bool)`.
      **Not yet tested on hardware.**
- [x] **UI scope decision made**: a real hierarchical, NVS-backed configuration menu
      (Main Screen / Haptic Configurator / HID Type / Boot USB Mode) -- not a port of
      `legacy_fw`'s SquareLine screens (`ui_profSelectScreen.c`, `ui_valueScreen.c`, etc.,
      profile/MIDI-value-display oriented, would need real rework either way), and not the
      mock menu's flat 3-item list either. See Phase 8 for the full architecture, screen
      hierarchy, button roles, and build order. The mock menu/detent UI above was
      explicitly a pipeline proof, now being replaced by the real thing.
- [ ] Screens implemented per Phase 8
- [ ] Rotation/mirror, if the final mounting orientation needs it beyond the swap/mirror
      already applied for correct rendering

**Build-size note**: this phase's dependencies (LVGL + esp_lcd_gc9a01 + esp_lvgl_port) are
a real jump, not a rounding error -- Flash went 25.5%->51.4% (~334KB->~673KB of the
1.3MB `app0` partition), RAM 6.4%->26.9% (~21KB->~88KB of 320KB). Still comfortably
within budget, but worth knowing this is where the budget actually went, and worth
re-checking if later phases start feeling tight.

**PlatformIO/component-manager gotcha hit during this bring-up, worth remembering**:
adding new entries to `src/idf_component.yml` did **not** get picked up by a normal
`pio run` -- `dependencies.lock` stayed stale (only listed the Phase 3 `esp_tinyusb` dep)
and the build failed with missing headers for the new components. A plain `pio run` does
not always force CMake to reconfigure/re-resolve when the manifest changes. Fixed by a
full `pio run -t clean` (removing `.pio/build/<env>` entirely) before the next `pio run`
-- that forced a fresh CMake configure, which re-ran the component manager and picked up
the new dependencies correctly. If a future `idf_component.yml` edit silently doesn't
take effect, this is the first thing to try, before assuming the manifest itself is wrong.

### Phase 5 — LEDs

- [ ] `led_strip` (RMT) driver bring-up for ring A (60px) + ring B (8px)
- [ ] Lighting modes ported/rewritten (per-key colors, ring indicator, brightness control)

### Phase 6 — Haptic programs (presets) [~] IN PROGRESS -- reframed as force-shape selection, not named preset bundles

**Storage/switching UX decided, superseding the two "TBD" bullets this phase originally
had**: not a curated list of named preset bundles, and not a code-array-vs-NVS question in
isolation. Instead: independent, continuously-tunable Kp/Kd/detent-count (already
live-tunable since Phase 2b) plus one orthogonal **Haptic Type** selector -- Saw / Sine /
Viscose -- exposed as a field in Phase 8's Haptic Configurator menu screen, NVS-persisted
via that phase's settings work. "Saw" is today's existing nearest-grid-point profile,
unchanged.

- [x] ~~Decide the program authoring/storage shape~~ -- superseded, see above (Phase 8's
      NVS-backed menu, not a code array)
- [x] ~~Decide program switching UX~~ -- superseded, see above (menu-driven via Phase 8,
      not a button combo)
- [ ] **Sine profile**: same discrete detent grid as Saw, but the restoring force follows
      a smooth sinusoid instead of Saw's instant sign-flip at the midpoint boundary --
      zero force at both the detent center (stable) and the midpoint (unstable), a
      continuous transition between them instead of the current hard snap.
- [ ] **Viscose profile**: no discrete detents at all -- pure velocity damping (Kd term
      only, no Kp/target-angle term), a continuous "thick fluid" drag feel. Not new
      territory: this is exactly the "viscous fluid" mode already discovered and felt
      during live-tuning (Kp=0, Kd~0.055, no pulse, noted below) -- now promoted to a
      selectable profile instead of only reachable by manually zeroing Kp. Viscose mode
      should fire **no** HID wheel-scroll or audio-click events (Phase 3/7) -- there's no
      detent-crossing edge to hang them on.
- [ ] Bench/hands-on validation: Saw/Sine/Viscose feel distinctly different and match
      their intended character
- [ ] Consider whether "ramp" deserves to be a first-class parameter distinct from Kp/Kd
      (e.g. a profile-specific approach/exit velocity near a detent boundary, rather than
      always the same fixed linear slope) — needs a concrete use case before adding the
      complexity

### Phase 7 — I2S Audio [~] IN PROGRESS — first slice confirmed working on hardware

**Not** a port of `legacy_fw`'s Arduino WAV-playback code. Reference/design model is a
different sibling project's implementation: `Lucu-Kind/src/haptics.h`/`.cpp`
(`/Users/kama10/Documents/PlatformIO/Projects/Lucu-Kind/`) — a different device on the
same MAX98357A-amp I2S wiring (DOUT=9, BCLK=10, LRCK=11, matching this board's pinout
exactly). Its architecture, to be adapted (not copied 1:1 as Arduino) to ESP-IDF's
`driver/i2s_std` and this project's existing conventions (`esp_timer`-paced tasks,
`ipc.c`-style queues/atomics):

- Pure procedural synthesis via a precomputed 256-entry sine LUT — no embedded WAV
  assets (`WavData*.cpp`/`sounds/*.wav` from legacy are not needed under this model)
- Dedicated free-running Core 1 task (`xTaskCreatePinnedToCore`) that continuously
  writes fixed-size DMA chunks (`i2s_write`, small chunk size e.g. 16 samples, many
  buffers deep) rather than being triggered per-event
- Click triggers delivered from the encoder/control side via a lock-free monotonic
  counter + small ring buffer of click types (acquire/release atomics), not a blocking
  queue — a burst of fast detents queues as distinct audible pulses instead of
  collapsing into one or blocking the producer
- Several interchangeable click timbres (single decaying sine, wood-tock chirp,
  tick+thud layer, inharmonic partials, filtered-noise+ping hybrid, rubber-thump,
  minimal fixed-decay blip) plus a separate always-fixed "switch" click for on/off-style
  transitions, and an independent out-of-bounds drone tone that fades in/out based on
  how fast the knob was moving at the moment of impact
- Envelope/decay computed incrementally (precomputed per-sample multiplier) rather than
  calling `expf()` per sample — a real perf lesson from that codebase's own chime-glitch
  incident, worth carrying over here too

Tasks:

- [x] Read `Lucu-Kind/src/config.h` + `shared_state.h` alongside `haptics.h`/`.cpp` for
      the full constant/atomic-state picture before designing this project's version
- [x] `driver/i2s_std` init on this board's I2S pins (`board_pins.h`: DOUT=9, BCLK=10,
      LRC=11), modeled on the MAX98357A config above (channel format, DMA buffer sizing).
      Implemented in `i2s_task.c` (`audio_i2s_init()`).
- [x] Core 1 synthesis task, integrated with this project's existing `i2s_task.c`
      skeleton (Phase 1) rather than a new ad-hoc task.
- [x] Click-trigger IPC from the haptic control loop (Core 0, `control_task.c`'s detent
      edge already computed in Phase 2b) to the Core 1 audio task — lock-free
      counter/queue per the reference model, not a blocking cross-core call. Implemented
      as its own module (`audio_trigger.h`/`.c`), not folded into `ipc.c` -- deliberately
      different pattern (lock-free atomics, single-producer/single-consumer) from that
      file's FreeRTOS queues.
- [x] Port/redesign the startup chime and at least one click timbre as a first working
      slice before expanding timbre choices. **Confirmed working on real hardware**: the
      3-note arpeggio startup chime plays correctly. Click timbre went through several
      hands-on iterations (`i2s_task.c`, `CLICK_TIMBRE` switch) -- started with the
      reference's simplest "BLIP" (500Hz/6ms), found too quiet/subtle against the motor's
      own noise; tried TICK_THUD; landed on WOOD_TOCK retuned progressively higher-pitched
      and shorter through direct hands-on comparison -- 260Hz/30ms (reference default,
      too low/long) → 900Hz/15ms → 1200Hz/9ms → **2400Hz/5ms, confirmed excellent** as the
      audible complement to the motor's own tactile click. User's own framing: the
      combination is "perfectly tunable from both audiophysical and kinesthetic
      perspective" -- i.e. the I2S click timbre and the motor's Kp/Kd/pulse haptic
      parameters (Phase 2b) are two independent, complementary tuning axes for the same
      overall "detent feel," not competing or redundant. A debug trigger log
      (`click trigger consumed, type=%d`) was added in `i2s_task.c` during this bring-up
      to separate "not triggering" from "too quiet" -- edge-triggered, not rate-limited;
      worth removing/throttling before any fast-spin testing (same class of concern as the
      Phase 2a per-tick-logging watchdog incident, just on Core 1 this time).
      Amplitude/gain note for later: firmware-side amplitude and the MAX98357A's hardware
      GAIN-pin strap are two separate volume knobs -- if amplitude tuning alone hits a
      ceiling, that points at the hardware strap, not firmware.
- [x] **DMA underrun under concurrent display load -- found on hardware via Phase 8 menu
      testing, fixed, not yet re-verified.** This item's own placeholder sizing note said
      "revisit if underruns show up" -- they did. Clicks went weak/glitchy specifically
      while the menu was open, not in normal (single-label) display mode. Root cause: a
      click and a menu redraw are triggered by the same detent-crossing event, and a menu
      redraw touches up to 4 styled/positioned labels (vs. one text update in normal mode)
      -- meaningfully more Core 1 SPI/LVGL work landing at the exact instant the I2S task
      needs to keep feeding its DMA buffer on time. USB/LED load specifically wasn't the
      trigger here -- display load was. Fixed: `AUDIO_DMA_DESC_NUM` 8->16 (~11.6ms->~23.2ms
      buffered), giving the audio task enough slack to ride through a redraw burst. Still
      short enough that the added latency on a click is imperceptible. **Not yet re-tested
      on hardware.**
- [ ] Expand beyond the one confirmed timbre (TICK_THUD exists but is currently the
      inactive `#if` branch, untested against WOOD_TOCK's confirmed-good tuning) — richer
      timbres (inharmonic, hybrid, rubber-thump), the reference's fixed "switch" click
      variant, and the out-of-bounds drone tone remain unported, per the original plan
- [x] **Runtime-selectable click type, first instance**: added `AUDIO_CLICK_BUTTON_THUMP`
      (`audio_trigger.h`) as a second click type alongside the detent click
      (`AUDIO_CLICK_NORMAL`) -- a distinct, lower ~110Hz/40ms decay timbre with no chirp,
      dispatched at runtime in `i2s_task.c` by tracking the active click's type/duration
      rather than picking a timbre at compile time (`CLICK_TIMBRE` still selects the
      *detent* click's timbre only). Briefly wired to fire on any button press edge in
      `control_task.c` as an experiment, then **removed from being triggered** (button-tap
      audio not wanted for now) -- the enum value and synthesis stay in place, unused, a
      one-line change to bring back. `CLICK_AMPLITUDE`/`CLICK_CLIP_LIMIT` also went through
      a quiet->max->reverted->max round-trip during this session's tuning (currently at max,
      32000/32000) -- MAX98357A gain is a hardware SD_MODE strap, not firmware-controlled,
      so digital amplitude is the only volume lever available from code.
- [ ] **Runtime-selectable timbre for the menu** (Phase 8's Haptic Configurator "Haptic
      Sound" field): extend the dispatch-by-type pattern proven above to WOOD_TOCK/
      TICK_THUD selection for the *detent* click (currently still compile-time only via
      `#if CLICK_TIMBRE`), persisted through Phase 8's `haptic_cfg` NVS struct.
- [ ] Remove or rate-limit the debug trigger log once its job (confirming triggering
      works, now done) is no longer needed for active tuning

### Phase 8 — Configuration menu & persistence [~] IN PROGRESS -- architecture decided, build starting

**Scope decided** (resolves Phase 4's "UI scope decision" and Phase 6's program
authoring/storage question, replaces the old vague "profile management" framing this
phase started with): a real hierarchical, NVS-based configuration menu, replacing
SPIFFS-based `DeviceSettings`/`HapticProfileManager`. Not a port of `legacy_fw`'s screens,
not a fixed array of named haptic presets.

**Screens**, built on top of the Phase 4 mock menu's toggle/render pipeline (not a rewrite
from scratch):
- **Main Screen** (default view) — live detent readout (existing), plus a compact
  button-legend cheat sheet
- **Haptic Configurator** — Number of Detents (0-120), Kp, Kd, Haptic Type (Saw/Sine/
  Viscose, see Phase 6), Haptic Sound (timbre, see Phase 7), Save
- **HID Type** — Keyboard/Mouse/MIDI (input-mapping switch, see Phase 3), MIDI Mapping
  (Channel, Note — skipped entirely in navigation unless HID Type=MIDI), Save
- **Boot USB Mode** — Serial/HID, Save (persists the choice that `main.c`'s
  `usb_serial_mode_requested()` currently only offers via a boot-time button hold)

**Button roles** (physical keys are labeled F1-F4 on this device; see §1's Architecture
decisions log for the full reasoning): F4 = open menu / back to Main Screen entirely, F3 =
back one level (cancels an in-progress edit and reverts it), F1 = select / enter edit /
commit, F2 = unused/reserved. Knob rotation is context-sensitive: haptic feel + HID wheel
in normal operation, list navigation inside the menu, value adjustment while editing a
field.

**Navigation state machine**: four states cover every screen -- `Normal` (today's default
behavior) -> `MenuRoot` (F4) -> `Section` (F1, one of the three screens above) ->
`EditValue` (F1 on an adjustable field; F1 commits, F3 cancels-and-reverts). Disabled items
(MIDI Mapping when HID Type != MIDI) are skipped entirely during navigation, not shown
greyed-and-inert.

**Boot USB Mode failsafe, kept**: the existing BTN_C+BTN_D-hold-at-boot override (Phase 3)
must keep forcing Serial mode regardless of the saved NVS preference -- this is the exact
mechanism that recovered this session's own flashing workflow after a composite-HID boot
broke the 1200bps-touch reset trick. The NVS setting must never be able to silently
override it.

**Build order** (confirming with the user before advancing to each next step; checked off
below as each one lands and is confirmed):

- [ ] 1. Menu framework + data-driven rendering skeleton -- **implemented, build verified
      clean, not yet tested on hardware.** New `menu.c`/`menu.h`, not folded into
      `control_task.c` (already ~68KB): a generic item/screen model (submenu / value /
      action kinds, an `is_enabled` predicate per item), a navigation stack (depth 0 =
      closed, not hardcoded to the current 2-level depth), and edit-mode as a flag on top
      of the stack rather than a separate state enum -- `Normal`/`MenuRoot`/`Section` from
      the planning diagram collapse into "stack depth 0/1/2+", `EditValue` is just
      `editing=true` at whatever depth. Disabled items are skipped entirely when stepping
      the selection (bounded loop, can't spin forever even if a screen were all-disabled).
      A render snapshot (open/editing/title/formatted row text + selected flag) is
      recomputed after every input call and handed to `display_task.c` under a mutex --
      first mutex in this codebase (`ui_state.h`'s cross-core pattern is atomics-only,
      which doesn't fit variable-length formatted strings). All three screens
      (Haptic Configurator/HID Type/Boot USB Mode) exist with **placeholder values local
      to `menu.c`** -- not the real `s_haptic_kp`/etc. or NVS, that's steps 2-3/6-7; this
      step is purely the framework, deliberately testable (navigate, edit, disabled-item
      skip for MIDI Mapping) before any real setting is touched.

      `control_task.c`: the old BTN_D-toggle mock menu and all the retired live-tuning
      button combos (BTN_D/BTN_C detent count, BTN_A/BTN_B Kp, BTN_A+BTN_C/BTN_B+BTN_D Kd)
      are removed, replaced by F1/F3/F4 (BTN_A/BTN_C/BTN_D) edge-detection calling
      `menu_input_select()`/`back()`/`toggle_open()`, and the existing detent-crossing edge
      now calls `menu_input_rotate()` instead of the old 3-item wraparound math when the
      menu is open. `ui_state.h`/`.c`: the mock menu's `menu_active`/`menu_selection`
      atomics are gone (superseded by menu.c's own snapshot) -- only the detent readout
      remains.

      `display_task.c`: Main Screen gained the button-legend cheat sheet (F4/F3/F1, small
      font). Menu rendering shows up to `MENU_VISIBLE_WINDOW=4` rows at once, windowed to
      always include the current selection, rather than every row a screen has -- Haptic
      Configurator's 6 items don't all fit this round panel's ~4-row safe area at once. The
      top/bottom fade-edge hint from the UI preview mockup is **not** implemented yet
      (plain windowing is enough to make every item reachable) -- noted as a follow-up
      polish item, not a gap in this step's own scope.
      `CONFIG_LV_FONT_MONTSERRAT_12` added to `sdkconfig.defaults` -- the existing default
      (Montserrat 14, set during Phase 4 bring-up) was too large for menu rows/cheat-sheet
      captions to fit without overflowing the circular safe area; 14 is kept for the Main
      Screen's big detent readout.

      Build verified clean (RAM 27.0%/88400B, Flash 52.6%/689149B, up slightly from Phase
      4's numbers as expected for a new module + a second font).

      **Re-tested on hardware: navigation/edit/disabled-item-skip all confirmed working**,
      but LCD refresh reported as noticeably slow. Root cause: `UI_REDRAW_PERIOD_MS=150`
      (fine for a passive number readout, a very noticeable input-to-screen lag for
      something actively navigated) combined with `update_ui()` unconditionally re-touching
      every visible label's text/style/position on every single poll regardless of whether
      anything actually changed -- LVGL treats each of those calls as dirtying that object,
      so most polls were doing a full re-layout of the whole visible row set for nothing.
      **Fixed**: `update_ui()` now diffs the new menu snapshot (`memcmp`) and detent value
      against what it last rendered, and returns immediately (no LVGL calls, no lock taken)
      on a tick where nothing changed -- between an actual button press/detent crossing,
      menu.c's snapshot is byte-identical call to call, so this makes most polls nearly
      free. That in turn made a much shorter poll period cheap: `UI_REDRAW_PERIOD_MS`
      lowered 150->30ms (5x), since the cost is no longer proportional to poll frequency.
      Build verified clean (RAM 27.1%/88656B, Flash 52.6%/689241B). **Not yet re-tested on
      hardware for the speed fix specifically.**

      Visual polish deferral above was later overridden by explicit request -- see the
      dedicated visual-fidelity pass logged further down this step, done ahead of steps 2-3
      rather than after.

      **Two more hardware findings from this same re-test, both fixed, neither yet
      re-verified on hardware:**
      - **Amber selection highlight rendered as blue.** Same bug *class* as Phase 4's
        `LCD_SWAP_BYTES` mirror issue -- black/white (the only colors the original smoke
        test ever showed) are invariant under a Red/Blue channel swap, so this was never
        actually exercised until a genuinely asymmetric color (amber: high red, low blue)
        existed on screen. `esp_lcd_panel_dev_config_t.rgb_ele_order` was
        `LCD_RGB_ELEMENT_ORDER_RGB`; both `esp_lcd_gc9a01`'s own test app and
        `esp_lvgl_port`'s example use `LCD_RGB_ELEMENT_ORDER_BGR` for this exact panel
        family -- RGB was the wrong first guess. Fixed: switched to BGR.
      - **"Blocky" visible redraw while scrolling, distinct from the latency issue above.**
        SPI/DMA confirmed not the bottleneck: `spi_bus_initialize()` already uses
        `SPI_DMA_CH_AUTO`, `lvgl_port_display_cfg_t.flags.buff_dma=true`, and
        `LCD_SPI_FREQUENCY_HZ=80MHz` is fast (~2ms per 40-row partial-buffer chunk). The
        real suspect was `double_buffer=false` -- flagged in this exact line's own comment
        back in Phase 4 as "simplest option, revisit if tearing shows up." Single-buffering
        forces LVGL to wait for the previous chunk's DMA transfer to finish before
        rendering the next chunk into that same memory, serializing render+transfer instead
        of overlapping them -- the original static single-label smoke test never had enough
        redraw traffic to expose this; real scrolling menu content does. Fixed: flipped to
        `double_buffer=true` (one more 19200-byte buffer -- LCD_WIDTH x
        LCD_DRAW_BUFFER_ROWS x 2 bytes -- trivial against the ~232KB free). If this doesn't
        fully resolve it, the next suspect is Core 1 task-priority contention:
        `PRIO_I2S=10` > `PRIO_DISPLAY=5`, both on `CORE_IO=1`, and every menu-navigation
        knob click also fires an audio click (`audio_trigger_click()` fires unconditionally,
        menu open or not, per Phase 7) -- worth testing in isolation (one variable at a
        time, per this project's own established debugging pattern) rather than changing
        priorities blindly alongside the buffering fix.

      **Re-tested after the BGR + double-buffer fixes: colors correct, redraw improved but
      "not as smooth as it could be," AND a serious separate bug found: fast CW rotation
      while the menu is open made the knob overshoot and spin by itself.** Root cause:
      `menu_input_rotate()` runs on Core 0 inside `control_task.c`'s hard-real-time 10kHz
      loop (one call per detent crossing). The original implementation recomputed a fully-
      formatted render snapshot (`snprintf` across up to 8 rows) inline on every call,
      guarded by a FreeRTOS mutex shared with the display task -- variable-cost, potentially
      -blocking work injected directly into the one loop this project has treated as
      sacrosanct since Phase 1 (fixed 100us budget; see `MOTOR_VOLTAGE_SLEW_LIMIT_V_PER_S`
      and the wall-clock hard-timeout elsewhere in this doc for why). Fast rotation drives
      many detent crossings per second, clustering budget overruns; since the slew-rate
      limiter's math assumes a fixed dt per iteration, a run of delayed/skipped iterations
      followed by catch-up ticks can stack voltage steps in a burst -- exactly "overshoot
      and spin by itself." Same bug *class* as Phase 2a's per-tick-`ESP_LOGE`-induced
      watchdog/desync incident, just triggered by string formatting instead of UART logging.

      **Fixed**: `menu_input_*()` now only ever touches a few bounded ints/pointers (stack
      depth, selected index, editing flag) under a `portMUX` spinlock (non-blocking, unlike
      a mutex) -- no formatting, no locks-that-can-block. All `snprintf` work moved into
      `menu_get_render_snapshot()`, called only from `display_task.c`'s relaxed ~30ms poll
      on Core 1, which has no real-time deadline to violate. The placeholder settings
      values (Kp/Kd/detents/etc.) are now `_Atomic`, matching `ui_state.h`'s existing
      cross-core convention, so Core 0's `on_rotate()` writes and Core 1's `format_value()`
      reads need no lock at all. Build verified clean (RAM 27.0%/88416B, Flash
      52.6%/689325B). **Not yet re-tested on hardware.**

      The "not perfectly smooth yet" complaint was reported in the same breath as the
      overshoot bug, from the same pre-fix test -- plausible the real-time violation above
      was contributing to both (a stalling control loop firing catch-up bursts would affect
      more than just motor voltage). Deliberately not applying further LVGL-specific tuning
      (larger partial-buffer rows, Core 1 task-priority changes) yet -- re-test this fix
      first, one variable at a time per this project's own established debugging pattern,
      before assuming more display-side tuning is still needed.

      **Re-tested: overshoot persisted even after the spinlock fix above.** Confirmed via
      direct question that it felt like active torque (motor driving the knob further), not
      freewheeling -- ruling out `HAPTIC_COAST_VELOCITY_RAD_S`'s intentional zero-torque
      coast-above-threshold behavior as the explanation. Real root cause found in Phase 2b's
      click-pulse logic, not the menu framework itself -- see the pulse-retrigger bug/fix
      entry in Phase 2b above. **Not yet re-tested on hardware.**

      **Visual-fidelity pass, by explicit request** ("I want the device menu to look
      exactly like the [UI preview mockup] artifact you built") -- done ahead of steps 2-3
      rather than after, overriding the earlier deferral above. Closed the gaps between the
      working-but-plain Step 1 render and the mockup:
      - `menu.h`'s `menu_render_row_t` split from one concatenated `text` field into
        separate `label`/`value` fields -- needed so `display_task.c` can lay a row out
        label-left/value-right (the mockup's justified layout) instead of one string with a
        double-space separator standing in for real alignment.
      - Each visible menu row is now a real flex container (`LV_FLEX_FLOW_ROW`,
        space-between) holding two child labels, not a single `lv_label` -- the amber
        pill/radius/padding live on the container so both the key and value sit inside one
        shared background when selected, matching the mockup instead of approximating it.
      - Cheat-sheet tags (F4/F3/F1) gained the mockup's pill treatment (faint white
        fill + thin border) instead of being bare text -- they were functionally present
        since step 1 but visually didn't match at all.
      - `MENU_ROW_STEP` 26->32, radius 6->8, padding 8/3->10/6 (hor/ver) -- closer to the
        mockup's more spacious pill proportions; re-verified against the round panel's
        circular safe area at the new spacing (4 rows still comfortably clear of the edge).
      - Not done: the mockup's top/bottom fade-edge scroll hint -- still the one
        deliberately-deferred piece (needs gradient overlay work, a separate, smaller task
        from this pass).
      Build verified clean (RAM 27.0%/88480B, Flash 52.6%/689697B). **Not yet re-tested on
      hardware.**

      **Switched to `lv_roller` for the menu list, by request** ("its better but likely we
      need to use LV_Roller for menu, make font larger too and menu need to be little more
      narrow in horizontal") -- replaces the hand-rolled windowed flex-container list from
      the visual-fidelity pass above. Rationale beyond just following the request: a roller
      gives native smooth scroll-to-selection animation and a built-in `LV_PART_SELECTED`
      band for free (styled amber/black to match, same as before), and its default rendering
      already fades rows away from center -- which happens to be the top/bottom "more above/
      below" hint that was the one deliberately-deferred piece of the mockup. One real
      tradeoff: a roller option is a single line of plain text, so label/value went from two
      independently-styled, justified columns (the previous pass's flex containers) back to
      one joined string per row (`"Label  Value"`) -- simplicity and native scroll behavior
      won out over pixel-perfect justification. `LV_ROLLER_MODE_INFINITE` matches menu.c's
      own wraparound navigation (`step_index()`) -- wrapping from the last item to the first
      is a short animated wrap, not a long spin back through the whole list. Font bumped
      12->14 (larger, by request -- the largest already-compiled-in size, avoids adding a
      third font just for this) and width narrowed 188->150 (by request). The roller's
      options text is only rebuilt (`lv_roller_set_options()`) when the visible text itself
      changes (a value edit, or an item's enabled/disabled set changing) -- rebuilding on
      every ordinary selection change would reset the roller's scroll state instead of
      letting it play its native short scroll animation between adjacent items. Build
      verified clean (RAM 27.1%/88752B, Flash 52.6%/689753B), zero warnings.

      **Re-tested: 150 clipped text at the larger font -- widened to 200.** Also reported:
      the joined "Label  Value" string (accepted tradeoff when switching to roller) looks
      worse than the previous flex-container pass's justified columns; asked about
      `lv_canvas` for full manual control. Recommended against canvas (loses the roller's
      free scroll animation/selection tracking, needs a dedicated pixel buffer, hand-rolled
      redraw/dirty-tracking -- a lot of cost for a cosmetic gain); proposed overlaying a
      separate right-aligned value label positioned over just the roller's selected band
      (fixed, known position -- dead center) as a cheaper middle ground. **Not implemented
      yet** -- deliberately deferred until the scheduling fix below is confirmed in
      isolation, so it's not muddying which change fixed what.

      **Major problem reported: roller animation not smooth at all while the menu is
      open, despite lv_roller normally being known for smooth scrolling.** User's own
      hypothesis (independently arrived at): the I2S task. Confirmed the mechanism by
      reading `i2s_task.c`'s loop: it runs `while(1)` calling
      `i2s_channel_write(..., portMAX_DELAY)` back to back with **no explicit yield**, even
      when synthesizing silence between clicks. ESP-IDF FreeRTOS is strictly
      priority-preemptive across different priorities (time-slicing only applies between
      tasks of the *same* priority) -- at the original `PRIO_I2S(10) > PRIO_DISPLAY(5)`
      (`tasks_common.h`), the display/LVGL task could only run during I2S's brief DMA-queue-
      full blocking windows. Doubling the I2S DMA buffer depth earlier this session (to fix
      an audio-glitch-during-menu-redraw bug) plausibly made this *worse*, not better, by
      making those blocking windows rarer. The original priority ordering's own comment
      ("a late HID report or audio underrun is user-perceptible in a way a late screen
      redraw... isn't") was correct when written (Phase 4, passive number readout) and is
      now stale (Phase 8, animated interactive menu). **Fixed**: `PRIO_I2S` and
      `PRIO_DISPLAY` equalized (10/5 -> 9/9) so FreeRTOS time-slices between them instead of
      I2S being able to fully starve display -- the safer, more easily-reasoned-about middle
      ground versus a full priority swap (which risks just inverting which task starves the
      other) or hand-tuning explicit yields into the I2S loop (risks reintroducing underruns
      if the timing is gotten wrong without hardware to verify against). Build verified clean
      (RAM 27.1%/88752B, Flash 52.6%/689753B). **Not yet re-tested on hardware** -- next
      step is confirming both that the roller animation is now smooth *and* that audio
      hasn't regressed (glitching would mean equal priority isn't enough and a different
      balance, or an explicit yield in the I2S loop, is needed instead).

      **Re-tested: audio confirmed clean/improved, but roller animation still very laggy --
      the priority fix alone wasn't the whole story.** Found the second, independent cause
      by reading `lv_roller.c`/`lv_theme_default.c` directly rather than guessing further:
      the default theme applies a **200ms scroll animation** to every roller
      (`theme->styles.anim`, confirmed at the exact line applying it to `lv_roller_class`).
      200ms is longer than the gap between detent crossings at any normal turning speed, so
      each new `lv_roller_set_selected()` call (one per crossing) was interrupting and
      restarting the *previous* still-in-flight animation before it ever completed --
      "laggy" was a perpetually-aborted animation, not a rendering-throughput problem, and
      fully independent of the I2S scheduling fix above (which is why that fix alone didn't
      touch it). Fixed: `lv_obj_set_style_anim_duration(roller, 60, LV_PART_MAIN)` overrides
      the theme default with something well below a realistic crossing interval, so each
      scroll-step animation actually finishes before the next one starts. Build verified
      clean (RAM 27.1%/88752B, Flash 52.6%/689789B).

      **Re-tested: still very laggy, barely visible improvement.** Neither the priority
      equalization nor the animation-duration fix resolved it. By request, next test
      isolates buffer configuration itself as its own variable: `LCD_DRAW_BUFFER_ROWS`
      40->24 and `.double_buffer` true->false, exactly replicating `legacy_fw`'s own
      known-smooth LVGL8/TFT_eSPI configuration (`lcd_thread.cpp`: `240*240/10` = 24 rows,
      single buffer -- confirmed by reading that file directly, not from memory). Legacy
      using a *smaller*, *less*-buffered configuration than what we'd already tried is a
      real data point against buffer size/double-buffering being the bottleneck, but worth
      testing directly rather than assuming. Known risk: double-buffering was originally
      added to fix a separate, already-confirmed bug (visible blocky redraw during scroll
      with real menu content) -- reverting to single-buffer risks reintroducing that
      specific artifact independent of whatever this test finds about the animation-lag
      symptom. Build verified clean (RAM 27.1%/88752B, Flash 52.6%/689789B).

      **Re-tested: still bad.** By request, I2S task disabled entirely (commented out in
      `main.c`, not deleted) as a decisive isolation test. **Re-tested with I2S fully
      disabled: still laggy, no improvement at all** -- I2S is conclusively not the cause.
      Symptom also described more precisely this time: "switch between settings are delayed
      but only occasionally trigger properly" -- sounds like something more specific than
      general animation choppiness (events being missed/delayed, not just unsmooth motion),
      distinct enough from the earlier framing that guessing a sixth hypothesis blind isn't
      the right move. I2S re-enabled (its disable was purely diagnostic).

      **Added a temporary latency probe instead of guessing further**, matching this
      project's own established debugging pattern of getting hard evidence over inferring
      from symptoms (reset-reason logging, click-trigger-consumed logging, etc.):
      `menu.c`'s `menu_input_*()` functions now stamp an atomic timestamp
      (`esp_timer_get_time()`, not a log call -- safe from Core 0's real-time loop, unlike
      logging there would be, exactly the class of mistake Phase 2a's per-tick-logging
      watchdog incident already taught this project to avoid) on every call. `menu.h` exposes
      `menu_get_last_input_us()`; `display_task.c`'s `update_ui()` logs the delta
      (`ESP_LOGI "menu render latency: %lld ms"`) whenever it detects an actual menu change
      -- only on Core 1, only on real changes, so no real-time risk. Build verified clean
      (RAM 27.1%/88768B, Flash 52.6%/690081B). **Purpose**: distinguish "Core 0 is
      processing input but Core 1 is slow/inconsistent to notice+render it" (large or wildly
      inconsistent logged latency) from "Core 1 notices quickly but LVGL's own animation/
      flush pipeline is what's slow" (small, consistent latency despite visible lag) --
      next step is reading these logs live while navigating the menu and reporting the
      pattern, not just the visual symptom.

      **Result: latency 4-25ms -- small.** This rules out "Core 1 not noticing/processing
      the change in time" -- the bug is downstream, in LVGL's own render/flush pipeline
      itself (this log fires before any LVGL API call). Traced `esp_lvgl_port`'s flush
      callback (`esp_lvgl_port_disp.c`) directly to look for a cause there: confirmed dirty-
      rectangle tracking IS working correctly (LVGL's per-flush `area` stays a small
      sub-rectangle all the way through, including after `sw_rotate`'s coordinate transform,
      `lvgl_port_rotate_area()`) -- the earlier "are we redrawing the whole screen every
      frame" concern is not what's happening. But found a real, previously-unaccounted-for
      cost: with `sw_rotate=true` (required for this project's 90-degree rotation, Phase 4),
      every single flush also runs `lv_draw_sw_rotate()` -- a full software pixel-transpose
      of the dirty rectangle into a third scratch buffer -- before the SPI send. Separately,
      and likely more significant: **this project had been building at ESP-IDF's default
      debug optimization level (`CONFIG_COMPILER_OPTIMIZATION_DEBUG`, -Og-equivalent) and
      160MHz (`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160`) this entire time** -- neither had ever
      been touched, in this project or carried over from anywhere. Both affect all CPU-bound
      work broadly, sw_rotate's per-flush transpose included. **Fixed**: added
      `CONFIG_COMPILER_OPTIMIZATION_PERF=y` and `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y` to
      `sdkconfig.defaults` (confirmed applied post-clean-rebuild, replacing the debug/160MHz
      values). Also, by request: re-enabled `.double_buffer=true` on top of the already-
      reverted 24-row buffer size (rather than only ever testing buffer count and buffer
      size bundled together as one variable, matching legacy exactly) -- isolates the two.
      Confirmed `esp_lvgl_port` already allocates its draw buffers with `MALLOC_CAP_DMA`
      internally (`esp_lvgl_port_disp.c:353`) whenever `buff_dma=true` is set, which it
      already is -- the manually-`heap_caps_malloc`'d buffer suggested as a possible fix is
      already effectively what happens under the hood; there's no user-supplied-buffer path
      in this version of `lvgl_port_display_cfg_t` to inject one directly. Build verified
      clean after a full clean rebuild (RAM 27.0%/88444B, Flash 54.8%/717879B -- flash grew
      as expected, `-O2`-class optimization can increase code size via inlining/unrolling
      relative to `-Og`). **Not yet re-tested on hardware.** If this doesn't resolve it, the
      sw_rotate per-flush transpose is the next concrete lever -- switching to hardware
      rotation (`sw_rotate=false`) would eliminate it entirely, at the cost of re-deriving
      the MADCTL mirror/rotation combo again (bounded, now well-understood work given this
      session's earlier mirror/rotation debugging, but real effort -- held off unless needed).

      **Switched to hardware rotation now, by request, rather than waiting to see if the
      optimization/frequency fix alone was enough.** `flags.sw_rotate` false (was true) --
      eliminates the `lv_draw_sw_rotate()` per-flush software pixel-transpose entirely.
      This changes the actual mechanism, not just a flag: `lvgl_port_disp_rotation_update()`
      (`esp_lvgl_port_disp.c`) is no longer a no-op -- that early-return was specifically
      gated on `sw_rotate` -- it now composes `LCD_MIRROR_X`/`_Y`/`LCD_SWAP_XY` (the base
      orientation) with whatever `LCD_ROTATION` angle is requested, via its own dihedral-
      group math, and sends the result to MADCTL directly. This is a fundamentally different
      composition than the software path's "rotate the pixel buffer, hardware orientation
      stays fixed" model. Removed the manual `esp_lcd_panel_mirror()`/`swap_xy()` calls this
      file was making directly (added earlier specifically because `lvgl_port_disp_rotation_
      update()` was dead code under `sw_rotate=true`) -- `esp_lvgl_port` owns this again now,
      and calling both would just have the second one silently overwrite the first.
      **Consequence, flagged clearly rather than silently assumed away**: `LCD_ROTATION`
      being `LV_DISPLAY_ROTATION_90` was correct *for the software-rotation formula* -- it is
      NOT necessarily correct for this new hardware-rotation formula, which operates on the
      same base config through different math. Left at `_90` as the starting guess (smallest
      diff) but this is genuinely untested -- treat exactly like every other per-board
      orientation fact in this project (flip to `_270` first if wrong, same methodology as
      `LCD_MIRROR_X` originally). Build verified clean (RAM 27.0%/88444B, Flash
      54.8%/717683B). **Not yet re-tested on hardware -- expect orientation may need
      re-confirming on top of whatever this does for performance.**

      **Re-tested: performance unchanged, no improvement at all.** User directly and
      reasonably questioned whether `sw_rotate` was actually disabled rather than accepting
      the code change at face value -- right call. Added a temporary diagnostic
      (`ESP_LOGW`, rate-limited to every 50th flush) directly inside `esp_lvgl_port`'s own
      `lvgl_port_flush_callback()` (`esp_lvgl_port_disp.c`, a vendored `managed_components`
      file -- marked clearly as temporary, not an upstream change) logging
      `disp_ctx->flags.sw_rotate` and `current_rotation` on every flush, to get direct proof
      rather than re-asserting the config should work.

      **While setting that up, found a second, separate, previously-missed always-active
      software pixel pass, unrelated to sw_rotate**: `LCD_SWAP_BYTES=true` (needed --
      real hardware fact, this panel wants RGB565 big-endian on the wire, ESP32 stores it
      little-endian) was driving `.flags.swap_bytes=true`, which makes `esp_lvgl_port` run
      `lv_draw_sw_rgb565_swap()` -- a full software byte-swap over the entire dirty region --
      on *every single flush*, completely independent of the sw_rotate switch. This runs
      regardless of double-buffering, buffer size, or task priority -- none of the other
      fixes this session touched it. **Fixed properly, not worked around**: LVGL has a
      native `LV_COLOR_FORMAT_RGB565_SWAPPED` format (confirmed available in this build --
      `CONFIG_LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=y`) where the software renderer produces
      already-correctly-ordered pixels as part of normal drawing, instead of rendering
      normally and then running a separate full-buffer post-process pass. Switched
      `.color_format` to `LV_COLOR_FORMAT_RGB565_SWAPPED` and `.flags.swap_bytes` to `false`
      (setting both would double-swap back to the wrong order) -- same underlying hardware
      fact addressed, moved from a separate O(dirty-area) pass to being absorbed into
      rendering itself. Build verified clean (RAM 27.0%/88444B, Flash 54.8%/717823B). **Not
      yet re-tested on hardware for either the sw_rotate proof or this fix.**

      **Re-tested, still zero improvement.** "Zero improvement" across CPU frequency,
      compiler optimization, sw_rotate, byte-swap format, buffer size/count, task priority,
      and animation duration -- every one of them a real, verified change -- was itself the
      key signal: none of them was ever the actual bottleneck. Rewrote the flush diagnostic
      to log the actual inter-flush interval unconditionally (the first version's 1-in-50
      rate limit had never fired at all, which in hindsight was itself already a clue) and
      asked for a fresh capture specifically while scrolling the open menu.

      **Root cause found from that capture, not guessed.** The log showed `sw_rotate=0`
      (hardware rotation confirmed genuinely active) and three flush calls 9-11ms apart
      covering the roller's redraw area in buffer-sized chunks -- fast, once started. But
      the first of those three chunks came **549ms** after the previous flush, while our own
      "menu render latency" log confirmed Core 0's input had already been detected and
      processed roughly 300ms *before* that flush even began. Traced `esp_lvgl_port.c`'s own
      internal task (`lvgl_port_task()`, a *separate* FreeRTOS task from this project's own
      "display" task that calls `update_ui()`): it calls `lv_timer_handler()` then sleeps in
      `xEventGroupWaitBits()` for up to `task_max_sleep_ms` whenever nothing was pending last
      check. This project's own `task_max_sleep_ms` was **500** -- matching the measured
      549ms gap almost exactly. `update_ui()` calls LVGL APIs (`lv_roller_set_selected()`
      etc.) directly from its own task, but never told the port's internal task that new
      work existed -- so that task had no way to wake up early; it would only notice on its
      own next up-to-500ms timeout, then process everything queued at once. This is exactly
      "freezes, then jumps" (the user's own description, now with hard numbers behind it) --
      and explains why nothing else worked: every other fix was a real cost worth removing,
      but none of them could matter while the task doing the actual rendering was simply
      asleep the entire time in between.

      **Fixed**: `lvgl_port_task_wake(LVGL_PORT_EVENT_USER, NULL)` -- a public API that
      exists specifically for an external task to notify the port's task that something
      changed, distinct from pretending to be a touch/encoder event -- called at the end of
      `update_ui()` right after `lvgl_port_unlock()`, so the port task wakes immediately
      instead of waiting out its own timeout. `task_max_sleep_ms` also lowered 500->50 as a
      defense-in-depth bound (not the primary fix) in case a future code path changes LVGL
      objects without remembering to call the wake function. Build verified clean (RAM
      27.0%/88444B, Flash 54.8%/717979B).

      **Confirmed on hardware: this was the actual root cause -- roller animation is smooth.**
      With the real bug fixed, `MENU_ROLLER_ANIM_MS` raised 60->120 (by request) for a more
      visible transition -- 60 was chosen partly to limit how often a fast navigation step
      would interrupt/restart the animation, less of a concern now that updates land
      promptly instead of queuing behind a sleeping task. Still well under the original
      200ms theme default that caused that interruption problem in the first place. Build
      verified clean. **Not yet re-tested on hardware for this specific tuning value.**
      Temporary diagnostics (the `esp_lvgl_port_disp.c` flush-interval log and menu.c's
      input-latency probe) still in place, marked clearly as temporary -- revert once this
      is fully confirmed stable.

      **Three more polish items, by request, now that the real bug is fixed:**
      - `LV_ROLLER_MODE_INFINITE` -> `LV_ROLLER_MODE_NORMAL`. `menu.c`'s own wraparound
        navigation (`step_index()`) still wraps at the data level regardless -- this only
        changes the roller's own visual behavior at the wrap boundary, from a short animated
        wrap to a real scroll back through the list. Item counts here are small (2-6), so
        that's a modest scroll, not a long spin, but worth knowing this is a real behavior
        change, not just cosmetic.
      - `MENU_ROLLER_VISIBLE_ROWS` 3->5.
      - Top/bottom fade added -- a generated 8-bit luminance ("L8") bitmap mask (black at
        each outer edge fading to white/fully-visible toward the middle), attached via
        `lv_obj_set_style_bitmap_mask_src()`, adapted directly from LVGL's own
        `examples/widgets/roller/lv_example_roller_fade_mask.c`. Confirmed the required
        features are enabled in this build first rather than assuming
        (`CONFIG_LV_DRAW_SW_COMPLEX=y`, `CONFIG_LV_USE_CANVAS=y`). This is the real "more
        above/below" hint the original windowed-list version had deferred, now actually
        implemented rather than approximated by the roller's own built-in edge dimming.
        **Real RAM cost, not free**: the mask buffer is 200x150x1 byte (L8) = ~30KB, static
        for the object's lifetime -- RAM usage jumped 27.0%->36.2% (88444B->118460B) from
        this one feature. Still comfortably within budget, but a real number, not
        negligible. Mask dimensions (`MENU_FADE_MASK_*`) are empirical starting values
        (LVGL's own example's comment literally says "empirical values for simplicity" too)
        -- may need tuning once seen on hardware against the actual 5-row/14pt layout.
        Build verified clean (RAM 36.2%/118460B, Flash 54.9%/720011B). **Not yet tested on
        hardware for any of these three.**

      **Two more, by request: row spacing and a real Sora font (selected bold, rest
      regular).**
      - Row spacing: `lv_roller` has no per-item padding of its own -- its options are one
        internal multi-line label -- so `lv_obj_set_style_text_line_space()` on `LV_PART_MAIN`
        is the actual knob. New `MENU_ROLLER_LINE_SPACE` (14px). Growing this grows each row's
        effective height, which changes how much of the fade-mask band each partial row shows
        -- `MENU_FADE_MASK_*` may need retuning against it once seen on hardware.
      - Font: LVGL ships no bold variant of anything built-in -- every `lv_font_montserrat_*`
        size is a single weight -- so "selected bold, rest regular" needed an actual second
        font asset, not a style tweak. Sora isn't a Google/LVGL built-in either. Pipeline used
        (by user's explicit choice of the three offered -- fetch it myself from Google Fonts
        rather than wait for a provided TTF or fake bold via outline on Montserrat):
        1. Google Fonts' `google/fonts` GitHub repo ships Sora as a single variable font
           (`ofl/sora/Sora[wght].ttf`, OFL-licensed, wght axis 100-800), not separate static
           weight files.
        2. `fontTools.varLib.instancer` (`pip install fonttools`) pinned two static instances,
           wght=400 and wght=700 -- confirmed these actually differ (not just a metadata
           no-op) by comparing the 'H' glyph's bounding box between the two before converting
           anything.
        3. `lv_font_conv` (via `npx`) converted each to an LVGL C bitmap font, ASCII
           0x20-0x7E only, 14px/4bpp (matching the existing `lv_font_montserrat_14` size/bpp)
           -- `src/fonts/ui_font_sora_14_regular.c` / `_bold.c` + `ui_font_sora.h`
           (`LV_FONT_DECLARE`). Picked up automatically by `src/CMakeLists.txt`'s
           `GLOB_RECURSE`, no build-file edit needed.
        4. Wired into `display_task.c`: `ui_font_sora_14_regular` on the roller's
           `LV_PART_MAIN`, `ui_font_sora_14_bold` on `LV_PART_SELECTED` -- same pattern
           already used for the selected row's black-on-amber text color. Confirmed both
           instances report identical `line_height`/`base_line` (16/4) before wiring, so
           switching weight on selection doesn't jitter row position vertically.
        Real cost: Flash 54.9%->55.8% (720043B->731303B, +11260B for both weights combined,
        ASCII-only keeps this small). RAM unaffected (font glyph data is flash-resident, not
        copied to RAM). Build verified clean.

        **Confirmed broken on hardware: Sora rendered as blank text** (roller rows showed no
        glyphs at all, not garbled/wrong ones). Root cause, confirmed not guessed: the
        generated font `.c` files have `bitmap_format=1` (RLE-compressed per glyph -- grepped
        directly, this is `lv_font_conv`'s default output), but this build's sdkconfig had
        `CONFIG_LV_USE_FONT_COMPRESSED` unset -- confirmed via
        `sdkconfig.esp32-s3-devkitm-1`. With that config off, the compressed-format decode
        branch in LVGL's `lv_font_get_bitmap_fmt_txt()` is compiled out entirely: glyph
        lookup still succeeds but returns nothing to draw -- a silent no-op, not a crash or
        build warning, which is exactly why it looked like "font not loaded" rather than an
        obvious error. LVGL's built-in Montserrat fonts never hit this because they ship
        uncompressed -- this project had simply never used a compressed custom font before
        now. Fix: `CONFIG_LV_USE_FONT_COMPRESSED=y` added to `sdkconfig.defaults`, then a full
        `pio run -t clean` + rebuild (sdkconfig deltas need this to reliably regenerate, per
        this file's WiFi-config lesson above). Confirmed applied in the regenerated
        `sdkconfig.esp32-s3-devkitm-1`. Build verified clean (RAM 36.2%/118476B, Flash
        55.9%/732487B -- the decoder itself costs ~1.2KB flash, negligible).

        **Confirmed on hardware: font loaded, but Sora looked soft/blurry.** Root cause: Sora
        is a smooth outline font, and its Regular (400) weight's thin stems partially collapse
        under 4bpp anti-aliasing at 14px -- exactly the reported look. User's own diagnosis
        (unprompted, matches the above): stick to Medium/SemiBold weight for small pixel
        sizes, Regular often collapses to uneven sub-pixel stems; also suggested pre-converting
        to bitmaps (already doing that) and offered two alternative fonts, Inter or Silkscreen.
        Asked which direction since they're visually very different (Inter: same
        modern-sans family as Sora/the mockup, just better small-size hinting; Silkscreen: a
        literal pixel-art bitmap font, blocky retro look) -- user picked **Silkscreen**.
        Switched: `ofl/silkscreen/Silkscreen-{Regular,Bold}.ttf` (google/fonts, static, no
        variable-font instancing needed this time) -> `lv_font_conv --size 16 --bpp 1
        --no-compress --no-prefilter --autohint-off` -> `src/fonts/ui_font_silkscreen_16_*.c`
        + `ui_font_silkscreen.h`, replacing the Sora files. 1bpp (true monochrome, no
        anti-aliasing at all) is the correct choice for a pixel-art font -- it's already drawn
        on a grid rather than being an outline rasterized down, so it can't suffer Sora's
        failure mode. `CONFIG_LV_USE_FONT_COMPRESSED` no longer needed (Silkscreen is
        uncompressed) -- explicitly unset in `sdkconfig.defaults` the same way this file
        already unsets `CONFIG_BT_ENABLED`, though note below. Build verified clean (RAM
        36.2%/118476B, Flash 55.3%/724711B -- smaller than Sora despite going 14px->16px,
        since 1bpp beats 4bpp on size). Confirmed both weights report identical
        `line_height`/`base_line` (18/2) before wiring, same jitter check as before.

        **Real overflow risk, computed not guessed**: pulled this font's own glyph
        `adv_w` table out of the generated `.c` and summed it for the longest menu label --
        "Haptic Configurator" comes out to ~208px in this font, vs. `MENU_ROLLER_WIDTH`'s
        200px (and LVGL's internal roller padding eats into that further). Growing the
        roller width to compensate isn't free either -- it was already pushed from an
        original 188px to 200px specifically to stay inside this round panel's circular safe
        area (see the roller-switch log entry above), so there's limited headroom left before
        text starts getting clipped by the bezel itself, not just the roller's own bounds.
        Cheapest real fix if this is visibly clipped on hardware: shorten the label text
        itself (e.g. "Haptic Config") rather than widening the roller -- a one-line change in
        `menu.c`, zero layout-constant risk. **Not yet confirmed one way or the other on
        hardware.**

        **Also found while regenerating fonts: `pio run -t clean` doesn't delete the
        per-env `sdkconfig.esp32-s3-devkitm-1` file, only `.pio/build/` -- so an explicit
        `# CONFIG_LV_USE_FONT_COMPRESSED is not set` added to `sdkconfig.defaults` didn't
        actually clear the symbol; it stayed `y` in the generated file across the rebuild.
        Kconfig defaults only fill in symbols with no saved value yet, they don't override
        one already explicitly saved -- same mechanism as this file's existing WiFi-config
        lesson, just in the opposite direction (there, an unwanted default kept reasserting
        itself; here, a wanted override couldn't dislodge an old explicit value). Left as-is
        -- harmless, ~1.2KB of now-dead compressed-glyph decoder code. Would need deleting
        the actual `sdkconfig.esp32-s3-devkitm-1` file (not just a clean) to truly force a
        from-scratch regen; not done since the cost of being wrong there (losing some other
        legitimate manual per-env tweak) outweighs reclaiming 1.2KB.**

      **Separately, by request: try to close the FPS gap, "8bit workflow" investigated.**
      8bpp color is not an option on this hardware -- confirmed from
      `esp_lcd_gc9a01.c`'s own `panel_gc9a01_init()`: its `switch (bits_per_pixel)` only
      handles `16` (colmod 0x55, RGB565) and `18` (colmod 0x66, RGB666/24-bit-packed); any
      other value hits `default:` and returns `ESP_ERR_NOT_SUPPORTED` -- a hard init failure,
      not a slow path. The GC9A01 IC itself has no 8-bit color mode in its command set. Dead
      end, not pursued further.

      Real lever found instead: `CONFIG_LV_DEF_REFR_PERIOD` (default 33ms, ~30fps) gates how
      often LVGL advances *any* animation step or redraw, independent of how much rendering/
      transfer headroom actually exists. Checked the headroom first rather than assuming it:
      a full 240x240 RGB565 frame over this project's 80MHz SPI bus is
      240*240*2 bytes * 8 bits / 80e6 Hz =~ 11.5ms -- and the menu only ever redraws a small
      dirty region, well under that. So the 33ms default was giving the 120ms roller-scroll
      animation only ~4 discrete steps total, a real, previously-invisible ceiling, not a
      throughput problem -- same *shape* of bug as the `task_max_sleep_ms` discovery earlier
      in this phase (a hidden gate, not insufficient horsepower), though a different
      mechanism. Halved to `CONFIG_LV_DEF_REFR_PERIOD=16` (~60fps ceiling, ~8 steps per
      animation). Added `CONFIG_LV_USE_PERF_MONITOR` + `_LOG_MODE` (prints real FPS/CPU% to
      the log) as a temporary diagnostic, so the next hardware test reports a measured number
      instead of a feeling -- same evidence-first approach that found the real
      `task_max_sleep_ms` bug rather than the ~10 smaller-but-real fixes that preceded it.
      `LV_USE_PERF_MONITOR` turned out to be gated behind `LV_USE_SYSMON` (a "Debugging >
      System Monitoring" parent option, off by default) -- first attempt at just the two
      `PERF_MONITOR` lines silently produced no `CONFIG_LV_USE_PERF_MONITOR` line at all in
      the generated sdkconfig (confirmed by grep, not assumed fixed); added
      `CONFIG_LV_USE_SYSMON=y` and reconfirmed all three lines present after rebuild. Build
      verified clean (RAM 36.2%/118476B, Flash 55.4%/726195B).

      **Confirmed on hardware: sysmon printed nothing at all, on screen or in the
      console.** Read `lv_sysmon.c` directly rather than guess again: `LV_USE_PERF_MONITOR_LOG_MODE`
      calls the bare `LV_LOG(...)` macro -> `lv_log()`, and that function's body
      (`src/misc/lv_log.c`) is a no-op unless *either* a custom print callback is registered
      via `lv_log_register_print_cb()` (this project never calls that) *or*
      `CONFIG_LV_LOG_PRINTF` routes it through `vprintf()`. `LV_USE_SYSMON`/`LV_USE_PERF_MONITOR`
      only compute the stats -- nothing was ever going to print without one of those two.
      Added `CONFIG_LV_USE_LOG=y` + `CONFIG_LV_LOG_PRINTF=y` (`vprintf()` lands on ESP-IDF's
      default console, same stream as every `ESP_LOGx` call, so no callback needed). Left
      `LV_LOG_LEVEL` at its default WARN -- `LV_LOG()` itself isn't level-filtered, but WARN
      keeps LVGL's own internal TRACE/INFO logging compiled out, avoiding noise. **Real,
      larger-than-expected cost**: enabling `LV_USE_LOG` at all means every `LV_LOG_WARN`/
      `_ERROR` call site across the *entire* LVGL library compiles to real code instead of a
      no-op, not just this one line -- Flash jumped 55.4%->57.1% (726195B->749055B, +22.8KB),
      confirmed via a size diff before/after, not estimated. Comfortably within budget, but
      worth knowing "turn on logging" wasn't free the way most single-flag toggles in this
      log have been. Build verified clean, both symbols confirmed present via grep on the
      regenerated sdkconfig.

      **Separately, by request mid-fix: drop bold everywhere, all fonts regular, and also
      switch the Main Screen's detent readout to the pixel font** (it had never had an
      explicit font at all until now -- silently riding the theme default,
      `lv_font_montserrat_14`). Roller's `LV_PART_SELECTED` now uses
      `ui_font_silkscreen_16_regular` instead of a separate bold weight;
      `ui_font_silkscreen_16_bold.c` and its `LV_FONT_DECLARE` deleted outright as genuinely
      unused rather than left dead. `s_detent_label` (Main Screen) now explicitly styled with
      `ui_font_silkscreen_16_regular` too. Cheat-sheet legend tags (F4/F3/F1, Main Screen)
      deliberately left on `lv_font_montserrat_12` for now, not swapped to match -- they're
      small fixed-position pill badges spaced only 72px apart, and Silkscreen's only
      generated size (16px) is noticeably larger than Montserrat 12; converting them risked a
      real overlap regression that can't be checked without hardware. Flagged to the user as
      an interpretation call, not silently decided. Build verified clean (RAM
      36.2%/118604B, Flash 57.0%/746963B -- essentially flat vs. the logging change above,
      -2KB from the removed bold font roughly cancelling the two new font-style call sites).
      **Confirmed on hardware, real numbers this time (sysmon working):** FPS/CPU varied a
      lot with activity (17-50 FPS, 0-77% CPU), but the actually diagnostic figures are
      `flush` and `render`. `flush` (the real SPI transfer) stayed at 0-3ms across every
      sample -- confirms the earlier bandwidth math, panel transfer was never the
      bottleneck. `render` (LVGL's own software compositing) sat at a near-constant
      **45-48ms on every window that had any redraws at all** (redraw_cnt 2, 3, or 5 all
      averaged about the same ~45-48ms) and exactly 0ms when idle -- a fixed per-redraw cost,
      not one that scales with how much content changed. That fixed-cost signature, plus
      LVGL gating bitmap masks behind its "complex" software-draw tier specifically because
      per-pixel mask compositing is expensive, points at the top/bottom fade mask
      (`lv_obj_set_style_bitmap_mask_src()`, applied over the whole 200x150 roller area on
      every redraw) as the likely culprit -- not `LV_DEF_REFR_PERIOD` (that ceiling is real
      and still worth having lowered, but at 45ms/render it was never the binding constraint;
      render time alone caps this well under the 60fps the period change aimed for).
      Isolating with a controlled A/B rather than guessing: the `lv_obj_set_style_bitmap_mask_src()`
      call is commented out (mask generation itself still runs once at init, kept only so the
      function/buffer stay referenced -- irrelevant to per-frame cost either way) with a
      `TEMPORARY DIAGNOSTIC` marker in `display_task.c`. Build verified clean (RAM
      36.2%/118604B, Flash 57.0%/746931B -- flat, as expected for removing one style call).

      **Confirmed on hardware: mask ruled out.** `render` stayed at 47-65ms with the mask
      disabled -- no better than the 42-48ms measured with it on (if anything slightly
      higher, but within what looks like ordinary sample-to-sample variance across different
      redraw workloads, not a real regression from removing work). Whatever is costing
      45-65ms per redraw, it isn't the fade mask.

      Next suspect, same reasoning applied one variable at a time: `LV_PART_SELECTED`'s
      rounded background (`MENU_ROW_RADIUS=8`). Rounded corners force LVGL's software
      renderer into its anti-aliased-arc "complex" draw path -- independent of whether a mask
      is attached, and a commonly-cited real LVGL performance cost, redrawn from scratch on
      every redraw rather than cached. Set `MENU_ROW_RADIUS` to 0 (square corners, forces the
      cheap plain-fill path) as the next isolated A/B, `TEMPORARY DIAGNOSTIC` marker in place,
      mask still left off from the previous test so only one variable changes at a time.
      Build verified clean (RAM 36.2%/118604B, Flash 57.0%/746931B -- flat, a `#define` value
      change costs nothing).

      **Confirmed on hardware: real, partial win.** `render` dropped from 45-65ms to
      33-40ms with square corners -- a genuine ~25-30% reduction, so rounded-corner AA was a
      real cost, but a large chunk (~33-40ms) remained unaccounted for.

      **Root-caused the remainder by reading `lv_roller.c` directly instead of continuing to
      bisect blindly**: it is architectural, not a misconfiguration. `draw_label()` (fires on
      every redraw, both `LV_ROLLER_MODE_NORMAL` and `_INFINITE`) unconditionally splits the
      draw into an "above selection" and "below selection" clip, each calling
      `lv_draw_label()` with the *label object's full coords* -- i.e. LVGL shapes/lays out the
      entire option-list text twice per redraw regardless of what's actually visible, clipping
      only happens at the pixel-blit stage after shaping. `draw_main()`'s `LV_EVENT_DRAW_POST`
      then does a *third* full `lv_draw_label()` call for the selected row's own styling, plus
      a `lv_text_get_size_attributes()` call right before it that re-shapes the same text a
      *fourth* time just to measure it. Four full text-shape passes per redraw, every redraw,
      unconditionally, is simply how `lv_roller` implements per-part-styled selection --
      there's no style property or Kconfig flag that turns this off; it would take patching
      LVGL itself or replacing the widget.

      This means the ~33-40ms floor is the practical ceiling for a `lv_roller`-based menu on
      this hardware, not a bug still waiting to be found -- roughly a 25-30fps ceiling during
      active scrolling (vs. the ~45-65ms/~15-22fps before this round's two fixes, and *far*
      better than the original task_max_sleep_ms-era near-freeze). Reaching further would mean
      either accepting this, or reverting to a custom hand-drawn windowed list (this project's
      own abandoned "first pass" architecture, dropped specifically because it looked worse
      and lacked native smooth-scroll animation -- a real trade-off, not a free win, if
      revisited). Also still open: whether `MENU_ROW_RADIUS=0` (square corners) and the fade
      mask staying off are acceptable visually, or whether the user wants either/both back at
      this now-known performance cost. **Decision point** -- put to the user directly
      (accept the ~30fps ceiling / restore rounded corners+mask at that cost / replace
      lv_roller with a custom list); **chose to replace it.**

      **`lv_roller` replaced with fixed row slots + one sliding highlight rect** (planned via
      Claude Code's plan-mode workflow first, given the scope -- see plan file
      `valiant-puzzling-adleman.md` for the full design writeup; summarized here). All
      changes confined to `src/display_task.c` -- `menu.c`/`menu.h`'s snapshot API already
      fit this without changes.

      Key realization that simplified the design: `MENU_MAX_VISIBLE_ITEMS` is 8, but no
      actual screen in `menu.c` has more than 6 items (Haptic Configurator), against 5
      visible rows -- so root/HID Type/Boot USB Mode never need to scroll at all, and the
      *only* overflow case anywhere in the app today is Haptic Configurator's 6th item by
      exactly one row. Rather than build a general virtualized/recycled-scroll list to match
      what `lv_roller` provided, this uses `MENU_LIST_VISIBLE_ROWS` (5) **fixed-position
      `lv_label` objects** (text only rewritten when the snapshot actually changes -- already
      gated by `update_ui()`'s existing diff) plus **one small plain `lv_obj` "highlight"
      rect** with no text at all, animated between fixed row-slot y-positions with a
      hand-written `lv_anim` (the first in this codebase -- every earlier animation used a
      widget's own built-in behavior) over `MENU_LIST_ANIM_MS` (120ms, carried over
      unchanged). A frame during the slide now costs one small rect reposition, not
      `lv_roller`'s four full-text-shape passes. The rare one-row-overflow case gets an
      instant window recompute (`window_start` clamped so the visible window never runs past
      either end of the list) rather than continuous virtualized scrolling -- a real
      simplification given it affects one screen by one row, and a side benefit: window
      recompute being always-instant also removes the earlier-logged wraparound quirk
      (`NORMAL` mode's "real scroll back through the list" on wrap).

      Top/bottom "more items" hint reimplemented as a plain per-row text-opacity dim
      (`MENU_LIST_DIM_TEXT_OPA`, ~40%) on the edge-most visible row when there's hidden
      content above/below, replacing the canvas/bitmap-mask fade entirely -- that mask was
      already a measured real cost, now serving an even rarer case. `MENU_ROW_RADIUS` (now
      `MENU_LIST_ROW_RADIUS`) restored to 8 (rounded corners) on the theory that it's cheap
      again now that it's one standalone rect repositioned only on an actual selection
      change rather than something coupled to `lv_roller`'s per-redraw text-shaping cost --
      explicitly flagged in-code as unverified until checked with `sysmon` on hardware.

      Deleted entirely rather than left disabled: `generate_menu_fade_mask()`, the static
      `s_menu_fade_mask` `L8` draw buffer, all `MENU_FADE_MASK_*`/old `MENU_ROLLER_*`
      defines, `s_last_roller_options` (no longer needed -- `lv_label_set_text_fmt()` on a
      small label is cheap and doesn't interrupt any per-row animation, unlike
      `lv_roller_set_options()` did, so the options-string-diff cache this existed for is no
      longer necessary).

      Build verified clean on the first attempt (no compile errors/warnings). **Real,
      unplanned RAM win**: 36.2%->26.9% (118604B->88284B, -30KB) -- almost exactly the size
      of the deleted fade-mask buffer (200x150x1 byte L8 = 30000B). Flash also dropped
      slightly (57.0%->56.8%). **Not yet tested on hardware** -- need fresh `sysmon`
      `render`/`flush` numbers compared against the ~33-40ms `lv_roller` floor this replaces,
      a felt-smoothness judgment on the new sliding-highlight-over-static-rows feel (an
      intentional UX change, not just perf -- no longer a continuous-scroll wheel), whether
      the Haptic Configurator 6-item instant-window-shift looks acceptable, whether the
      opacity-dim hint is visible/useful, whether `MENU_LIST_ROW_RADIUS=8`'s rounded corners
      are in fact still cheap in this new architecture, and the still-open "Haptic
      Configurator" ~208px-vs-200px text-width clipping question (unchanged by this rewrite --
      independent of widget choice).
- [ ] 2. NVS scaffolding: three new structs/namespaces (`haptic_cfg`, `hid_cfg`,
      `boot_cfg`), load/save modeled directly on `foc_calibration.c`'s existing pattern.
- [ ] 3. Haptic Configurator wired to the existing live-tunable Kp/Kd/detent-count
      (`control_task.c`'s haptic demo) -- move from button combos to the menu, add Save.
- [ ] 4. Sine + Viscose haptic profiles (Phase 6) -- new control-loop math, expect a
      hardware feel-tuning pass like Phase 2b's Kp/Kd work.
- [ ] 5. Runtime-selectable click timbre (Phase 7) wired into Haptic Configurator's
      Haptic Sound field.
- [ ] 6. Boot USB Mode: NVS + `main.c` wiring, keep the button-hold failsafe.
- [ ] 7. HID Type + MIDI Mapping screens (Phase 3) -- Keyboard mode blocked on a
      key-mapping decision (which keys?); MIDI Mapping is settings-only, no MIDI actually
      transmitted -- does not reinstate the USB MIDI interface dropped in Phase 3/§4.
- [ ] 8. *(Deferred, its own later phase)* Real USB-MIDI transport -- new TinyUSB MIDI
      class interface, descriptor/endpoint-budget work, likely a re-enumeration.

### Phase 8.5 — Haptic feel parity, menu vs. normal mode [ ] deferred until Display/Menu work (Phase 4/8) is done

Inserted here (not renumbered into the main sequence) to avoid disrupting Phase 9-11 --
explicitly queued to come after the LCD/menu work, per direct request, not before.

- [ ] **Residual "still feels slightly different" haptic complaint, after the coast-gate
      pulse fix (Phase 2b) resolved the worse overshoot-while-coasting bug.** The coast-gate
      fix was confirmed to help ("temporarily resolved"), but a subtler difference between
      menu-mode and normal-mode feel was still reported. Given `menu_is_open()` doesn't
      touch the force computation at all (confirmed by direct code inspection -- see Phase
      2b's log), the remaining difference, if real and not just a comparison/technique
      effect, needs a fresh, focused investigation rather than another guess layered on top
      of the coast-gate fix. Come back to this once Phase 4/8 display+menu work has settled
      enough that hardware time isn't split across both fronts.

### Phase 9 — Host communication / config protocol

- [ ] Decide fate of existing JSON serial protocol (`api.md`, `communications.md`) used
      by the "Zero/One" config app — keep for compatibility, or redesign given new scope
- [x] **Direction decided (design discussion, not yet implemented): nanopb over a
      dedicated second CDC-ACM channel**, superseding the plain-JSON idea above pending
      actual implementation. Reasoning: - Transport: a **second CDC-ACM interface** (`esp_tinyusb` supports up to 2,
      `CONFIG_TINYUSB_CDC_COUNT`), separate from the CDC channel Phase 3 already added
      for console/`tinyusb_console_init()` -- keeps human log text and the machine
      protocol from interleaving on the same stream. HID GET_REPORT/SET_REPORT
      (`tud_hid_get_report_cb`/`tud_hid_set_report_cb` in `usb_task.c`, currently
      no-ops) was considered as a fully driverless alternative and stays an option
      later if driverless discovery (e.g. a WebHID-based browser tool) ever matters
      more than it does now. - Encoding: **nanopb** (the small-footprint pure-C protobuf for microcontrollers --
      not mainline/Google protobuf, which is too heavy for this target: dynamic-
      allocation-heavy, expects a C++ runtime). Chosen over plain JSON and over a
      hand-rolled packed binary struct for one concrete reason: a single `.proto`
      schema generates both the firmware C code and the PC app's client code, which
      eliminates the real risk of the two sides' field layouts silently drifting apart
      as this protocol grows across phases (Phase 6 haptic presets, this phase, future
      telemetry) -- a hand-synced struct has no mechanism to catch that drift; JSON
      avoids drift (self-describing field names) but was reconsidered once framing and
      throughput were examined -- see below. - Framing: nanopb's built-in `pb_encode_delimited`/`pb_decode_delimited`
      (length-prefix) rather than hand-rolling message boundaries (which plain
      JSON-over-serial would have needed too, e.g. newline-delimiting). - Throughput/"minifying JSON" was examined and ruled out as the actual constraint:
      USB Full-Speed CDC has ample headroom for config-sized payloads (a full mapping
      config is well under 1-2KB; even unminified JSON sends in single-digit ms) --
      real compression (gzip/deflate) was explicitly rejected as the wrong tool at
      these message sizes, the compute cost would exceed any bytes saved. nanopb's
      binary encoding is adopted for schema/drift-safety and framing, not because JSON
      would have throttled the link. - One config+telemetry schema, not two systems: nanopb is meant to cover both the
      config get/set protocol _and_ any future high-rate telemetry channel (e.g. a
      live knob angle/velocity/torque plot in a PC tuning app), rather than JSON for
      one and an ad-hoc binary struct for the other. - Known tradeoff, accepted: raw protobuf bytes aren't human-eyeball-debuggable on
      the wire the way newline-delimited JSON is. Mitigation: a small Python decoder
      generated from the same `.proto` file (`protoc --python_out`) for debugging --
      one extra step, not a lost capability.
      **Not yet implemented** -- no `.proto` file, no nanopb build integration, no second
      CDC channel exist yet. This entry is the recorded direction for when Phase 9 work
      actually starts.
- [ ] Implement chosen protocol (second CDC-ACM channel + nanopb, per above)

### Phase 10 — Integration & optimization pass

- [ ] Cross-core load test: display flush + audio playback + USB report send happening
      concurrently — confirm no dropped HID reports / audio glitches / control loop jitter
- [ ] Measure actual FOC loop jitter (target Ts stability) on hardware
- [ ] Check GDMA channel allocation isn't exhausted (encoder-SPI + display-SPI + I2S
      all wanting DMA concurrently)
- [ ] Power/thermal sanity check

### Phase 11 — Cleanup

- [ ] Update README/docs for new scope (drop MIDI-first framing, document HID behavior)
- [ ] Decide fate of `legacy_fw/` (archive vs. remove once parity reached)

---

## 3. Open decisions to resolve along the way

- ~~How much on-screen UI survives the purpose change (Phase 4)~~ — decided: a real
  configuration menu (Phase 8), not a legacy port
- How much of the flexible mapping/profile system survives (Phase 3/7) — Keyboard mode's
  actual key mapping is the one piece of this still genuinely open (Phase 3/8)
- Whether the existing JSON config protocol / Zero/One app compatibility is kept (Phase 9)
- I2S source now available (see Phase 7) — architecture to adapt from, not port verbatim
- ~~Haptic program authoring/storage shape and switching UX~~ — decided: continuous
  Kp/Kd/detent-count + a Saw/Sine/Viscose selector via Phase 8's menu, not named preset
  bundles (Phase 6)

## 4. Pruning candidates

Legacy features to actively reconsider cutting rather than porting, given the new
keyboard/mouse-first scope. Not final — confirm per item as its phase comes up, but the
default assumption is "cut unless there's a reason to keep it," not the reverse.

- [x] **MIDI** (USB MIDI interface, MIDI mini-jacks, routing matrix in `communications.md`)
      — USB MIDI interface confirmed dropped (Phase 3), mini-jacks/routing-matrix removed.
      **Partially reinstated in reduced form**: Phase 8's HID Type menu offers a
      settings-only MIDI mode (Channel/Note stored in NVS), with no real MIDI transport --
      not a contradiction of this cut, just a smaller, deferred surface (see Phase 8 task 8
      for the real USB-MIDI transport work, not yet started).
- [ ] **OSC output** (`mapping.md` value/action type) — niche, likely removed
- [ ] **Audio profile/file management complexity** (`HapticProfileManager`'s WAV asset
      handling beyond simple click playback) — likely moot given Phase 7's procedural-
      synthesis model (no WAV assets at all), confirm once Phase 7 lands
- [ ] **Multi-profile management breadth** (arbitrary profile lists, reordering, per-profile
      full config surface) — likely simplified given fewer output modes to configure per profile
- [ ] **Gamepad axis configurability breadth** (deadzones, arbitrary axis mapping) — keep only
      if gamepad emulation stays a real target use case, otherwise trim to keyboard/mouse
- [ ] **JSON config protocol surface** (`api.md`) — audit against Phase 8 decision; drop
      message types tied to removed features
