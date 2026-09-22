/** Public three-layer description. Consistent across narrative pages. */
export function ThreeLayerModel({ compact = false }: { compact?: boolean }) {
  const layers = [
    {
      id: "protocol",
      title: "Production software",
      points: [
        "The running node is the operational consensus implementation",
        "Wallets and this website do not override it",
        "Public documentation does not replace the software that validates blocks",
      ],
    },
    {
      id: "science",
      title: "Coq model",
      points: [
        "Selected consensus properties are machine-checked",
        "The model can be replayed independently of this website",
        "Checked results are properties of the model",
      ],
    },
    {
      id: "runtime",
      title: "What is not claimed",
      points: [
        "Verification does not claim full binary equivalence",
        "The effort is ongoing",
        "Cryptography, networking, and wallets sit outside the checked model",
      ],
    },
  ];

  if (compact) {
    return (
      <div className="layer-compact mono">
        Production software → Coq model of selected properties → no binary-equivalence claim
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
