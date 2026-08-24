import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AppShell } from "./components/AppShell";
import { SystemStateProvider } from "./context/SystemStateContext";
import { AboutPage } from "./pages/AboutPage";
import { FormalVerificationPage } from "./pages/FormalVerificationPage";
import { DocumentationPage } from "./pages/DocumentationPage";
import { DownloadPage } from "./pages/DownloadPage";
import { ExperimentPage } from "./pages/ExperimentPage";
import { ExplorerPortalPage } from "./pages/ExplorerPortalPage";
import { LabPage } from "./pages/LabPage";
import { LandingPage } from "./pages/LandingPage";
import { MinimalScriptArchitecturePage } from "./pages/MinimalScriptArchitecturePage";
import { MiningPage } from "./pages/MiningPage";
import { MonetaryPolicyPage } from "./pages/MonetaryPolicyPage";
import { PlaceholderPage } from "./pages/PlaceholderPage";
import { ProtocolPage } from "./pages/ProtocolPage";
import { SciencePage } from "./pages/SciencePage";
import { SecurityArchitecturePage } from "./pages/SecurityArchitecturePage";
import { StatusPage } from "./pages/StatusPage";
import { WalletPage } from "./pages/WalletPage";
import { WalletTopicPage } from "./pages/WalletTopicPage";
import { WhatIsZeroTrustPage } from "./pages/WhatIsZeroTrustPage";
import { WhitepaperPage } from "./pages/WhitepaperPage";
import { ZeroTrustVerificationPage } from "./pages/ZeroTrustVerificationPage";

export default function App() {
  return (
    <SystemStateProvider>
      <BrowserRouter>
        <Routes>
          <Route element={<AppShell />}>
            <Route index element={<LandingPage />} />
            <Route path="download" element={<DownloadPage />} />
            <Route path="download/windows-wallet" element={<PlaceholderPage />} />
            <Route path="download/linux-wallet" element={<PlaceholderPage />} />
            <Route path="download/cli" element={<PlaceholderPage />} />
            <Route path="download/miner" element={<PlaceholderPage />} />
            <Route path="wallet" element={<WalletPage />} />
            <Route path="wallet/overview" element={<Navigate to="/wallet" replace />} />
            <Route path="wallet/oru" element={<WalletTopicPage />} />
            <Route path="wallet/maturity" element={<WalletTopicPage />} />
            <Route path="wallet/convergence" element={<WalletTopicPage />} />
            <Route path="wallet/res" element={<WalletTopicPage />} />
            <Route path="mining" element={<MiningPage />} />
            <Route path="mining/overview" element={<Navigate to="/mining" replace />} />
            <Route path="mining/windows" element={<PlaceholderPage />} />
            <Route path="mining/pool" element={<PlaceholderPage />} />
            <Route path="mining/economics" element={<PlaceholderPage />} />
            <Route path="explorer" element={<ExplorerPortalPage />} />
            <Route path="explorer/blocks" element={<PlaceholderPage />} />
            <Route path="explorer/oru-supply" element={<PlaceholderPage />} />
            <Route path="explorer/res" element={<PlaceholderPage />} />
            <Route path="explorer/fingerprint" element={<PlaceholderPage />} />
            <Route path="explorer/status" element={<PlaceholderPage />} />
            <Route path="documentation" element={<DocumentationPage />} />
            <Route path="documentation/whitepaper" element={<WhitepaperPage />} />
            <Route path="documentation/formal-verification" element={<FormalVerificationPage />} />
            <Route path="documentation/monetary-policy" element={<MonetaryPolicyPage />} />
            <Route path="documentation/user-guide" element={<PlaceholderPage />} />
            <Route path="documentation/developer-guide" element={<PlaceholderPage />} />
            <Route path="documentation/architecture" element={<PlaceholderPage />} />
            <Route path="documentation/rfc" element={<PlaceholderPage />} />
            <Route path="documentation/security" element={<SecurityArchitecturePage />} />
            <Route
              path="documentation/security/minimal-script-architecture"
              element={<MinimalScriptArchitecturePage />}
            />
            <Route path="zero-trust-verification" element={<ZeroTrustVerificationPage />} />
            <Route
              path="zero-trust-verification/what-is-zero-trust"
              element={<WhatIsZeroTrustPage />}
            />
            <Route
              path="zero-trust-verification/transaction"
              element={<Navigate to="/zero-trust-verification?mode=tx" replace />}
            />
            <Route
              path="zero-trust-verification/block"
              element={<Navigate to="/zero-trust-verification?mode=block" replace />}
            />
            <Route path="zero-trust-verification/fingerprint" element={<PlaceholderPage />} />
            <Route path="verify" element={<Navigate to="/zero-trust-verification" replace />} />
            <Route path="about" element={<AboutPage />} />
            <Route path="protocol" element={<ProtocolPage />} />
            <Route path="science" element={<SciencePage />} />
            <Route path="lab" element={<LabPage />} />
            <Route path="status" element={<StatusPage />} />
            <Route path="experiment" element={<ExperimentPage />} />
            <Route path="verification" element={<Navigate to="/status" replace />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </SystemStateProvider>
  );
}
