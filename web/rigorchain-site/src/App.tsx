import { BrowserRouter, Route, Routes } from "react-router-dom";
import { SystemStateProvider } from "./context/SystemStateContext";
import { DesktopShell } from "./desktop/DesktopShell";

export default function App() {
  return (
    <SystemStateProvider>
      <BrowserRouter>
        <Routes>
          <Route path="*" element={<DesktopShell />} />
        </Routes>
      </BrowserRouter>
    </SystemStateProvider>
  );
}
