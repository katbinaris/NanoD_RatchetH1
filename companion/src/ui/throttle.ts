// While a slider is dragged: at most one SET per setting every INTERVAL ms, and always the
// last value -- the knob follows smoothly without flooding the 64-byte link.

const INTERVAL = 40;

export function throttled<K>(fn: (key: K, v: number) => void) {
  const last = new Map<K, number>();
  const pending = new Map<K, number>();
  const timers = new Map<K, number>();
  return (key: K, v: number) => {
    const now = performance.now();
    const since = now - (last.get(key) ?? 0);
    if (since >= INTERVAL) {
      last.set(key, now);
      fn(key, v);
      return;
    }
    pending.set(key, v);
    if (!timers.has(key)) {
      timers.set(
        key,
        window.setTimeout(() => {
          timers.delete(key);
          last.set(key, performance.now());
          fn(key, pending.get(key)!);
        }, INTERVAL - since),
      );
    }
  };
}
