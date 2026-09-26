import { PixelIcon } from "./icons";
import type { IconName } from "./types";

export function DesktopIcon({
  id,
  name,
  icon,
  selected,
  onSelect,
  onOpen,
}: {
  id: string;
  name: string;
  icon: IconName;
  selected: boolean;
  onSelect: (id: string) => void;
  onOpen: () => void;
}) {
  return (
    <button
      type="button"
      className={`desktop-icon${selected ? " is-selected" : ""}`}
      onMouseDown={(e) => e.stopPropagation()}
      onClick={(e) => {
        e.stopPropagation();
        if (selected) onOpen();
        else onSelect(id);
      }}
      onDoubleClick={(e) => {
        e.stopPropagation();
        onOpen();
      }}
    >
      <PixelIcon name={icon} className="desktop-icon-glyph" />
      <span className="desktop-icon-label">{name}</span>
    </button>
  );
}
