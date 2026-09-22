import type { ButtonHTMLAttributes, ReactNode } from "react";
import { Link } from "react-router-dom";

type Variant = "default" | "primary";

type Shared = {
  children: ReactNode;
  variant?: Variant;
  className?: string;
};

type ButtonAsButton = Shared &
  Omit<ButtonHTMLAttributes<HTMLButtonElement>, "className" | "children"> & {
    to?: undefined;
    href?: undefined;
  };

type ButtonAsLink = Shared & {
  to: string;
  href?: undefined;
};

type ButtonAsAnchor = Shared & {
  href: string;
  to?: undefined;
};

export function Button(props: ButtonAsButton | ButtonAsLink | ButtonAsAnchor) {
  const variant = props.variant ?? "default";
  const cls = `ui-btn${variant === "primary" ? " ui-btn-primary" : ""} ${props.className ?? ""}`.trim();

  if ("href" in props && props.href) {
    return (
      <a href={props.href} className={cls} target="_blank" rel="noreferrer">
        {props.children}
      </a>
    );
  }

  if ("to" in props && props.to) {
    return (
      <Link to={props.to} className={cls}>
        {props.children}
      </Link>
    );
  }

  const buttonProps = props as ButtonAsButton;
  const { children, variant: _variant, className: _className, type, ...rest } = buttonProps;
  return (
    <button type={type ?? "button"} className={cls} {...rest}>
      {children}
    </button>
  );
}
