import { useEffect, useState } from "react";

const LINES = [
  "RIGOR BIOS",
  "",
  "Network: RIGOR MAINNET",
  "",
  "Initializing system...... OK",
  "Checking protocol....... OK",
  "Loading network......... OK",
  "Starting desktop........ OK",
  "",
  "C:\\>",
];

const STEP_MS = 120;

export function BootScreen({ onDone }: { onDone: () => void }) {
  const [count, setCount] = useState(1);

  useEffect(() => {
    if (count >= LINES.length) {
      const t = window.setTimeout(onDone, 140);
      return () => window.clearTimeout(t);
    }
    const t = window.setTimeout(() => setCount((c) => c + 1), STEP_MS);
    return () => window.clearTimeout(t);
  }, [count, onDone]);

  return (
    <div className="boot-screen" role="status" aria-live="polite">
      {LINES.slice(0, count).map((line, i) => (
        <div key={`${i}-${line}`}>
          {line.endsWith("OK") ? (
            <>
              {line.replace(" OK", "")} <span className="ok">OK</span>
            </>
          ) : (
            line
          )}
        </div>
      ))}
    </div>
  );
}
