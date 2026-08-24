import type { ReactNode } from "react";

type Props = {
  title?: string;
  state?: string;
  children: ReactNode;
  className?: string;
};

export function DataPanel({ title, state, children, className = "" }: Props) {
  return (
    <section className={`panel data-panel ${className}`.trim()}>
      {(title || state) && (
        <header className="panel-header">
          {title ? <h2>{title}</h2> : <span />}
          {state ? <span className={`panel-state ${state.toLowerCase()}`}>{state}</span> : null}
        </header>
      )}
      {children}
    </section>
  );
}
