import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AppShell } from "./components/AppShell";
import { SystemStateProvider } from "./context/SystemStateContext";
import { AboutPage } from "./pages/AboutPage";
import { FormalVerificationPage } from "./pages/FormalVerificationPage";
import { DocumentationPage } from "./pages/DocumentationPage";
import { DownloadPage } from "./pages/DownloadPage";
import { ExplorerPortalPage } from "./pages/ExplorerPortalPage";
import { LandingPage } from "./pages/LandingPage";
import { MinimalScriptArchitecturePage } from "./pages/MinimalScriptArchitecturePage";
import { MiningPage } from "./pages/MiningPage";
import { MonetaryPolicyPage } from "./pages/MonetaryPolicyPage";
import { PlaceholderPage } from "./pages/PlaceholderPage";
import { SecurityArchitecturePage } from "./pages/SecurityArchitecturePage";
import { WalletPage } from "./pages/WalletPage";
import { WalletTopicPage } from "./pages/WalletTopicPage";
import { WhitepaperPage } from "./pages/WhitepaperPage";

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
            <Route path="about" element={<AboutPage />} />
            <Route path="protocol" element={<Navigate to="/documentation/formal-verification" replace />} />
            <Route path="science" element={<Navigate to="/documentation/formal-verification" replace />} />
            <Route path="lab" element={<Navigate to="/documentation/formal-verification" replace />} />
            <Route path="status" element={<Navigate to="/documentation/formal-verification" replace />} />
            <Route path="experiment" element={<Navigate to="/documentation/formal-verification" replace />} />
            <Route path="verification" element={<Navigate to="/documentation/formal-verification" replace />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </SystemStateProvider>
  );
}
