import { COQ_ENFORCED_VECTORS, SHARED_GOLDEN_VECTORS } from "../protocolFacts";

/** Three-layer model — consistent across narrative pages. */
export function ThreeLayerModel({ compact = false }: { compact?: boolean }) {
  const layers = [
    {
      id: "protocol",
      title: "Layer 1 — Production consensus",
      points: [
        "C++ node (core/src/) is the source of truth",
        "Lock headers: params.h / rigor_consensus_lock.h",
        "RFC / SPEC / documentation do not supersede code",
      ],
    },
    {
      id: "science",
      title: "Layer 2 — Scientific Model",
      points: [
        "State machine formalization",
        "PowDigest.v models the live digest; ASERT is C++ only",
        "Model properties, not C++ equivalence",
      ],
    },
    {
      id: "runtime",
      title: "Layer 3 — Runtime Verification",
      points: [
        `Finite golden-vector / differential gate (${SHARED_GOLDEN_VECTORS} / ${COQ_ENFORCED_VECTORS})`,
        "ASERT and CheckProofOfWork are not Coq theorems",
        "NOT a Coq⇄C++ refinement proof",
      ],
    },
  ];

  if (compact) {
    return (
      <div className="layer-compact mono">
        Code SSOT → Coq model properties → Runtime differential verification (not refinement)
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
