// The Quadra companion protocol -- a mirror of NanoDepsidf/src/host_proto.h. Change both.
// 64-byte reports, no report ID; [0] = command (to the device) or reply tag (from it).
// Little-endian; floats are IEEE-754 single.

export const REPORT_SIZE = 64;
export const PROTO_VERSION = 1;
export const ICON_BYTES = 48 * 48 * 2;

export const Cmd = {
  HELLO: 0x10,
  GET_SETTINGS: 0x11,
  SET: 0x12,
  SAVE: 0x13,
  REVERT: 0x14,
  STREAM: 0x15,
  PROFILE: 0x16,
  PROFILE_ICON: 0x17,
  RESET_PEAKS: 0x18,
} as const;

export const Tag = {
  HELLO: 0xb0,
  SETTINGS: 0xb1,
  PROFILE: 0xb2,
  PROFILE_ICON: 0xb3,
  STATE: 0xb5,
  SYS_A: 0xb6,
  SYS_B: 0xb7,
  ERROR: 0xbf,
  ICON_UPLOAD: 0xa0, // icon_store.h
} as const;

// HOST_SET_* -- also the bit order of Settings.dirty.
export const Set = {
  DETENTS: 0,
  KP: 1,
  KD: 2,
  FEEL: 3,
  AMP: 4,
  PITCH: 5,
  SOUND: 6,
  HID_TYPE: 7,
  MIDI_CH: 8,
  PROFILE: 9,
  BOOT: 10,
  ROTATION: 11,
  HOST: 12,
} as const;
export type SetId = (typeof Set)[keyof typeof Set];
const FLOAT_SETTINGS: ReadonlySet<number> = new globalThis.Set([Set.KP, Set.KD, Set.PITCH]);

// Enums as the firmware stores them (menu.h, haptic_params.h, boot_mode.h).
export const Feel = { SAW: 0, SINE: 1, VISCOSE: 2 } as const;
export const HidType = { KEYBOARD: 0, MOUSE: 1, MIDI: 2, APP: 3 } as const;
export const Host = { MAC: 0, PC: 1 } as const;
export const Boot = { HID: 0, SERIAL: 1 } as const;

// Limits, as the firmware clamps them (haptic_params.h, audio_trigger.h).
export const Limits = {
  detents: { min: 3, max: 36, step: 1 },
  kp: { min: 0, max: 20, step: 0.05 },
  kd: { min: 0, max: 0.15, step: 0.005 },
  amp: { min: 0, max: 100, step: 5 },
  pitch: { min: 0.5, max: 2, step: 0.05 },
};

export interface Hello {
  proto: number;
  profileCount: number;
  serialBoot: boolean;
  version: string;
  date: string;
}

export interface Settings {
  dirty: number;
  detents: number;
  kp: number;
  kd: number;
  feel: number;
  amp: number;
  pitch: number;
  sound: number;
  hidType: number;
  midiChannel: number;
  profile: number;
  boot: number;
  rotation: number;
  host: number;
}

export interface Profile {
  index: number;
  count: number;
  hasIcon: boolean;
  id: string;
  name: string;
  legend: string[];
}

export interface State {
  seq: number;
  angle: number; // rad, continuous
  detent: number;
  buttons: number; // bit0 F1 .. bit3 F4
  menuScreen: number; // 0 = menu closed
  screensaver: boolean;
  liveSlot: number;
  clicks: number;
  walls: number;
}

export interface SysA {
  motorMa: number;
  ledMa: number;
  boardMa: number;
  totalMa: number;
  totalPeakMa: number;
  chipOk: boolean;
  chipC: number;
  chipPeakC: number;
  coilMa: number;
  coilPeakMa: number;
  copperW: number;
  usbSource: number; // pd_source_t
  usbMa: number;
  usbMv: number;
}

export interface SysB {
  load: [number, number];
  loadPeak: [number, number];
  loopKhz: number;
  workAvgUs: number;
  workMaxUs: number;
  jitterUs: number;
  missed: number;
  spikesPerS: number;
  heapFree: number;
  heapMin: number;
  hidDrops: number;
  audioGaps: number;
  uptimeS: number;
  sensorCrcErrors: number;
}

export interface IconChunk {
  index: number;
  offset: number;
  bytes: Uint8Array;
}

export type Message =
  | { tag: typeof Tag.HELLO; hello: Hello }
  | { tag: typeof Tag.SETTINGS; settings: Settings }
  | { tag: typeof Tag.PROFILE; profile: Profile }
  | { tag: typeof Tag.PROFILE_ICON; chunk: IconChunk }
  | { tag: typeof Tag.STATE; state: State }
  | { tag: typeof Tag.SYS_A; sys: SysA }
  | { tag: typeof Tag.SYS_B; sys: SysB }
  | { tag: typeof Tag.ERROR; cmd: number; code: number }
  | { tag: number };

// --- encoding ---

function report(cmd: number): Uint8Array {
  const r = new Uint8Array(REPORT_SIZE);
  r[0] = cmd;
  return r;
}

export const encode = {
  hello: () => report(Cmd.HELLO),
  getSettings: () => report(Cmd.GET_SETTINGS),
  save: () => report(Cmd.SAVE),
  revert: () => report(Cmd.REVERT),
  resetPeaks: () => report(Cmd.RESET_PEAKS),
  stream: (hz: number) => {
    const r = report(Cmd.STREAM);
    r[1] = Math.max(0, Math.min(50, Math.round(hz)));
    return r;
  },
  set: (id: SetId, value: number) => {
    const r = report(Cmd.SET);
    r[1] = id;
    const v = new DataView(r.buffer);
    if (FLOAT_SETTINGS.has(id)) v.setFloat32(4, value, true);
    else v.setInt32(4, Math.round(value), true);
    return r;
  },
  profile: (index: number) => {
    const r = report(Cmd.PROFILE);
    r[1] = index;
    return r;
  },
  profileIcon: (index: number, offset: number) => {
    const r = report(Cmd.PROFILE_ICON);
    r[1] = index;
    new DataView(r.buffer).setUint16(2, offset, true);
    return r;
  },
};

// --- decoding ---

function str(b: Uint8Array, at: number, n: number): string {
  let end = at;
  while (end < at + n && b[end] !== 0) end++;
  return new TextDecoder().decode(b.subarray(at, end));
}

export function decode(b: Uint8Array): Message {
  const v = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const u16 = (o: number) => v.getUint16(o, true);
  const i32 = (o: number) => v.getInt32(o, true);
  const u32 = (o: number) => v.getUint32(o, true);
  const f32 = (o: number) => v.getFloat32(o, true);
  switch (b[0]) {
    case Tag.HELLO:
      return {
        tag: Tag.HELLO,
        hello: { proto: b[1], profileCount: b[2], serialBoot: b[3] === 1, version: str(b, 4, 32), date: str(b, 36, 16) },
      };
    case Tag.SETTINGS:
      return {
        tag: Tag.SETTINGS,
        settings: {
          dirty: u16(1),
          detents: i32(4),
          kp: f32(8),
          kd: f32(12),
          feel: b[16],
          amp: b[17],
          pitch: f32(18),
          sound: b[22],
          hidType: b[23],
          midiChannel: b[24],
          profile: b[25],
          boot: b[26],
          rotation: b[27],
          host: b[28],
        },
      };
    case Tag.PROFILE:
      return {
        tag: Tag.PROFILE,
        profile: {
          index: b[1],
          count: b[2],
          hasIcon: b[3] === 1,
          id: str(b, 4, 12),
          name: str(b, 16, 16),
          legend: [0, 1, 2, 3].map((i) => str(b, 32 + i * 8, 8)),
        },
      };
    case Tag.PROFILE_ICON:
      return { tag: Tag.PROFILE_ICON, chunk: { index: b[1], offset: u16(2), bytes: b.slice(8, 8 + b[4]) } };
    case Tag.STATE:
      return {
        tag: Tag.STATE,
        state: {
          seq: u16(1),
          angle: i32(4) / 1e4,
          detent: i32(8),
          buttons: b[12],
          menuScreen: b[13],
          screensaver: b[14] === 1,
          liveSlot: b[15],
          clicks: i32(16),
          walls: i32(20),
        },
      };
    case Tag.SYS_A:
      return {
        tag: Tag.SYS_A,
        sys: {
          motorMa: u16(4),
          ledMa: u16(6),
          boardMa: u16(8),
          totalMa: u16(10),
          totalPeakMa: u16(12),
          chipOk: b[14] === 1,
          chipC: f32(16),
          chipPeakC: f32(20),
          coilMa: u16(24),
          coilPeakMa: u16(26),
          copperW: f32(28),
          usbSource: b[32],
          usbMa: u16(34),
          usbMv: u16(36),
        },
      };
    case Tag.SYS_B:
      return {
        tag: Tag.SYS_B,
        sys: {
          load: [b[4], b[5]],
          loadPeak: [b[6], b[7]],
          loopKhz: f32(8),
          workAvgUs: f32(12),
          workMaxUs: f32(16),
          jitterUs: f32(20),
          missed: u32(24),
          spikesPerS: f32(28),
          heapFree: u32(32),
          heapMin: u32(36),
          hidDrops: u32(40),
          audioGaps: u32(44),
          uptimeS: u32(48),
          sensorCrcErrors: u32(52),
        },
      };
    case Tag.ERROR:
      return { tag: Tag.ERROR, cmd: b[1], code: b[2] };
    default:
      return { tag: b[0] };
  }
}
