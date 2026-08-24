import type { InputHTMLAttributes, TextareaHTMLAttributes } from "react";

type Field = {
  label: string;
  id: string;
};

type InputProps = Field & { multiline?: false } & InputHTMLAttributes<HTMLInputElement>;
type TextProps = Field & { multiline: true } & TextareaHTMLAttributes<HTMLTextAreaElement>;

export function InputBox(props: InputProps | TextProps) {
  const { label, id } = props;

  return (
    <div className="ui-field">
      <label className="ui-input-label" htmlFor={id}>
        {label}
      </label>
      {props.multiline ? (
        <textarea
          id={id}
          className="ui-textarea mono"
          value={props.value}
          onChange={props.onChange}
          placeholder={props.placeholder}
          spellCheck={props.spellCheck}
        />
      ) : (
        <input
          id={id}
          className="ui-input"
          value={props.value}
          onChange={props.onChange}
          placeholder={props.placeholder}
          spellCheck={props.spellCheck}
          autoComplete={props.autoComplete}
        />
      )}
    </div>
  );
}
