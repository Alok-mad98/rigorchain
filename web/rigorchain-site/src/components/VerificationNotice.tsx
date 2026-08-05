/** Mandatory pre-verification messaging — shown on every page (TASK-RELEASE-004 §2) */
export function VerificationNotice() {
  return (
    <aside className="verification-notice" role="note">
      <p>
        <strong>Live formal verification (PROOF-034) is in progress.</strong> Final equivalence is{" "}
        <em>not</em> established. No consensus or production finality is claimed.
      </p>
    </aside>
  );
}
