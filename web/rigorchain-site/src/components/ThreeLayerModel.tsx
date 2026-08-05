/** Three-layer model — consistent across all narrative pages (TASK-RELEASE-004 §3) */
export function ThreeLayerModel({ compact = false }: { compact?: boolean }) {
  const layers = [
    {
      id: "protocol",
      title: "Layer 1 — Protocol (Frozen)",
      points: [
        "Deterministic rules — frozen specification",
        "No runtime interpretation on this layer",
        "No equivalence claims",
      ],
    },
    {
      id: "science",
      title: "Layer 2 — Scientific Model",
      points: [
        "State machine formalization",
        "Mathematical correctness layer",
        "No execution dependency",
      ],
    },
    {
      id: "runtime",
      title: "Layer 3 — Runtime Verification",
      points: [
        "PROOF-034 running toward height 2,100,001",
        "Archive generation in progress",
        "NOT completed · NOT final",
      ],
    },
  ];

  if (compact) {
    return (
      <div className="layer-compact mono">
        Protocol (frozen) → Scientific model → Runtime verification (PROOF-034, provisional)
      </div>
    );
  }

  return (
    <section className="three-layer-grid">
      {layers.map((l) => (
        <article key={l.id} className="card layer-card">
          <h3>{l.title}</h3>
          <ul>
            {l.points.map((p) => (
              <li key={p}>{p}</li>
            ))}
          </ul>
        </article>
      ))}
    </section>
  );
}
