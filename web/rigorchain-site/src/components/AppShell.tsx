import { Outlet } from "react-router-dom";
import { Footer } from "./Footer";
import { Navbar } from "./Navbar";

export function AppShell() {
  return (
    <div className="app-shell">
      <Navbar />

      <div className="network-banner" role="note">
        <p>
          <strong>RIGOR Testnet is a public testing network.</strong>
        </p>
        <p>
          The network is provided for evaluation, mining participation, and software testing.
        </p>
        <p>
          Mainnet parameters and final public specifications will be released at mainnet launch.
        </p>
      </div>

      <main className="main-content">
        <Outlet />
      </main>

      <Footer />
    </div>
  );
}
