/** Mandatory pre-verification messaging — shown on every page (TASK-RELEASE-004 §2) */
export function VerificationNotice() {
  return (
    <aside className="verification-notice" role="note">
      <p>
        <strong>Coq model strongly verified.</strong> Correspondence uses golden-vector and
        differential verification (324 shared vectors, hash and σ/root CLIs).{" "}
        <em>equivalence_claimed = false</em>. Not a refinement proof.
      </p>
    </aside>
  );
}
