import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AppShell } from "./components/AppShell";
import { SystemStateProvider } from "./context/SystemStateContext";
import { ExperimentPage } from "./pages/ExperimentPage";
import { LabPage } from "./pages/LabPage";
import { LandingPage } from "./pages/LandingPage";
import { ProtocolPage } from "./pages/ProtocolPage";
import { SciencePage } from "./pages/SciencePage";
import { StatusPage } from "./pages/StatusPage";

export default function App() {
  return (
    <SystemStateProvider>
      <BrowserRouter>
        <Routes>
          <Route element={<AppShell />}>
            <Route index element={<LandingPage />} />
            <Route path="protocol" element={<ProtocolPage />} />
            <Route path="science" element={<SciencePage />} />
            <Route path="lab" element={<LabPage />} />
            <Route path="status" element={<StatusPage />} />
            <Route path="experiment" element={<ExperimentPage />} />
            <Route path="verification" element={<Navigate to="/status" replace />} />
            <Route path="explorer" element={<Navigate to="/status" replace />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </SystemStateProvider>
  );
}
