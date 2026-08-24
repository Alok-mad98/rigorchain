import type { ReactNode } from "react";
import { Button } from "./Button";

type Props = {
  title: string;
  children?: ReactNode;
  to?: string;
  linkLabel?: string;
  className?: string;
};

export function DarkCard({ title, children, to, linkLabel = "Open", className = "" }: Props) {
  return (
    <article className={`card dark-card ${className}`.trim()}>
      <h3>{title}</h3>
      {children}
      {to ? (
        <div className="dark-card-actions">
          <Button to={to}>{linkLabel}</Button>
        </div>
      ) : null}
    </article>
  );
}
