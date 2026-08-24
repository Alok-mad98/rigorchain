import type { ReactNode } from "react";

export function SectionTitle({ children }: { children: ReactNode }) {
  return <h2 className="ui-section-title">{children}</h2>;
}
