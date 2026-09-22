import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function DocumentationVerificationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Verification</h1>
        <p className="muted">Documentation index. Not the verification boundary page.</p>
      </header>

      <SectionTitle>Index</SectionTitle>
      <section className="card-grid">
        <DarkCard
          title="Formal Verification"
          to="/protocol/verification-boundary"
          linkLabel="Open"
        />
        <DarkCard title="Security" to="/documentation/security" linkLabel="Open" />
      </section>

      <section className="card prose-section">
        <p className="muted">
          Independent verification instructions are not a website route. Implementation details are
          maintained separately from the public protocol documentation.
        </p>
      </section>
    </div>
  );
}
