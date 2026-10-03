// A profile's 48x48 icon (from the knob, or a draft's), at any size: whole multiples stay
// crisp (pixelated); smaller is a thumbnail. Black is see-through. No icon: its initial on a
// dark tile.

import { useEffect, useRef } from "preact/hooks";
import { cls } from "./controls";

export function ProfileIcon(p: { icon: ImageData | null; name: string; size: number; ring?: boolean }) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const c = ref.current;
    if (c && p.icon) c.getContext("2d")!.putImageData(p.icon, 0, 0);
  }, [p.icon]);
  const style = { width: `${p.size}px`, height: `${p.size}px` };
  if (!p.icon)
    return (
      <span class={cls("picon", "blank", p.ring && "inuse")} style={{ ...style, fontSize: `${Math.round(p.size * 0.5)}px` }} aria-hidden="true">
        {p.name.slice(0, 1)}
      </span>
    );
  return <canvas ref={ref} width={48} height={48} class={cls("picon", p.size % 48 === 0 && "px", p.ring && "inuse")} style={style} aria-hidden="true" />;
}
