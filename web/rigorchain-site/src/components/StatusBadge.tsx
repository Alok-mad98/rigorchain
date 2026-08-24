type Tone = "ok" | "warn" | "err" | "neutral";

const TONE: Record<Tone, string> = {
  ok: "status-badge-ok",
  warn: "status-badge-warn",
  err: "status-badge-err",
  neutral: "",
};

export function StatusBadge({ label, tone = "neutral" }: { label: string; tone?: Tone }) {
  return <span className={`status-badge ${TONE[tone]}`.trim()}>{label}</span>;
}
