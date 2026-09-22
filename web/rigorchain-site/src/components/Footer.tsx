export function Footer() {
  return (
    <footer className="site-footer">
      <div className="site-footer-inner">
        <p>
          Read-only observer. No signing, no transaction submission, no consensus control.
          Trust is established by independent verification of public bytes, not by this website.
        </p>
        <p className="footer-muted">
          Formal verification covers selected consensus properties through a machine-checked Coq
          model. The production implementation remains the operational consensus implementation.
        </p>
        <p className="footer-muted">
          Not an investment offering · Native assets: ORU and RES
        </p>
        <p className="footer-muted">RIGOR Public Test Network · www.rigorchain.com</p>
      </div>
    </footer>
  );
}
