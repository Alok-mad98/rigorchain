import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AppShell } from "./components/AppShell";
import { SystemStateProvider } from "./context/SystemStateContext";
import { AboutPage } from "./pages/AboutPage";
import { TestnetPage } from "./pages/TestnetPage";
import { FormalVerificationPage } from "./pages/FormalVerificationPage";
import { DocumentationPage } from "./pages/DocumentationPage";
import { DocumentationVerificationPage } from "./pages/DocumentationVerificationPage";
import { DownloadPage } from "./pages/DownloadPage";
import { ExplorerPortalPage } from "./pages/ExplorerPortalPage";
import { LandingPage } from "./pages/LandingPage";
import { MinimalScriptArchitecturePage } from "./pages/MinimalScriptArchitecturePage";
import { MiningPage } from "./pages/MiningPage";
import { MonetaryPolicyPage } from "./pages/MonetaryPolicyPage";
import { PlaceholderPage } from "./pages/PlaceholderPage";
import { ProtocolArchitecturePage } from "./pages/ProtocolArchitecturePage";
import { ProtocolConsensusPage } from "./pages/ProtocolConsensusPage";
import { ProtocolConvergencePage } from "./pages/ProtocolConvergencePage";
import { ProtocolHubPage } from "./pages/ProtocolHubPage";
import { ProtocolProofOfWorkPage } from "./pages/ProtocolProofOfWorkPage";
import { SecurityArchitecturePage } from "./pages/SecurityArchitecturePage";
import { UseFaucetPage } from "./pages/UseFaucetPage";
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

            <Route path="use" element={<TestnetPage />} />
            <Route path="use/download" element={<DownloadPage />} />
            <Route path="use/wallet" element={<WalletPage />} />
            <Route path="use/wallet/oru" element={<WalletTopicPage />} />
            <Route path="use/wallet/maturity" element={<WalletTopicPage />} />
            <Route path="use/wallet/convergence" element={<WalletTopicPage />} />
            <Route path="use/wallet/res" element={<WalletTopicPage />} />
            <Route path="use/mining" element={<MiningPage />} />
            <Route path="use/mining/windows" element={<PlaceholderPage />} />
            <Route path="use/mining/pool" element={<PlaceholderPage />} />
            <Route path="use/faucet" element={<UseFaucetPage />} />

            <Route path="protocol" element={<ProtocolHubPage />} />
            <Route path="protocol/overview" element={<AboutPage />} />
            <Route path="protocol/architecture" element={<ProtocolArchitecturePage />} />
            <Route path="protocol/architecture/whitepaper" element={<WhitepaperPage />} />
            <Route path="protocol/consensus" element={<ProtocolConsensusPage />} />
            <Route path="protocol/assets" element={<MonetaryPolicyPage />} />
            <Route path="protocol/script-as-asset" element={<MinimalScriptArchitecturePage />} />
            <Route path="protocol/convergence" element={<ProtocolConvergencePage />} />
            <Route path="protocol/proof-of-work" element={<ProtocolProofOfWorkPage />} />
            <Route path="protocol/verification-boundary" element={<FormalVerificationPage />} />

            <Route path="testnet" element={<Navigate to="/use" replace />} />
            <Route path="download" element={<Navigate to="/use/download" replace />} />
            <Route path="download/windows-wallet" element={<Navigate to="/use/download" replace />} />
            <Route path="download/linux-wallet" element={<Navigate to="/use/download" replace />} />
            <Route path="download/cli" element={<Navigate to="/use/download" replace />} />
            <Route path="download/miner" element={<Navigate to="/use/download" replace />} />
            <Route path="wallet" element={<Navigate to="/use/wallet" replace />} />
            <Route path="wallet/overview" element={<Navigate to="/use/wallet" replace />} />
            <Route path="mining" element={<Navigate to="/use/mining" replace />} />
            <Route path="mining/overview" element={<Navigate to="/use/mining" replace />} />
            <Route path="about" element={<Navigate to="/protocol/overview" replace />} />
            <Route
              path="documentation/whitepaper"
              element={<Navigate to="/protocol/architecture/whitepaper" replace />}
            />
            <Route
              path="documentation/formal-verification"
              element={<Navigate to="/protocol/verification-boundary" replace />}
            />

            <Route path="wallet/oru" element={<WalletTopicPage />} />
            <Route path="wallet/maturity" element={<WalletTopicPage />} />
            <Route path="wallet/convergence" element={<WalletTopicPage />} />
            <Route path="wallet/res" element={<WalletTopicPage />} />
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
            <Route path="documentation/verification" element={<DocumentationVerificationPage />} />
            <Route path="documentation/protocol" element={<PlaceholderPage />} />
            <Route path="documentation/developer" element={<PlaceholderPage />} />
            <Route path="documentation/user" element={<PlaceholderPage />} />
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
            <Route path="science" element={<Navigate to="/protocol/verification-boundary" replace />} />
            <Route path="lab" element={<Navigate to="/protocol/verification-boundary" replace />} />
            <Route path="status" element={<Navigate to="/protocol/verification-boundary" replace />} />
            <Route path="experiment" element={<Navigate to="/protocol/verification-boundary" replace />} />
            <Route path="verification" element={<Navigate to="/protocol/verification-boundary" replace />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </SystemStateProvider>
  );
}
