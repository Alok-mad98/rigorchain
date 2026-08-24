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
  };

type ButtonAsLink = Shared & {
  to: string;
};

export function Button(props: ButtonAsButton | ButtonAsLink) {
  const variant = props.variant ?? "default";
  const cls = `ui-btn${variant === "primary" ? " ui-btn-primary" : ""} ${props.className ?? ""}`.trim();

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
