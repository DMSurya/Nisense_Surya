import { Navigate, Route, Routes } from "react-router-dom";
import Layout from "./components/Layout";
import RoleGate from "./components/RoleGate";
import { useAuth } from "./auth/AuthContext";
import Login from "./pages/Login";
import Overview from "./pages/Overview";
import Users from "./pages/Users";
import Devices from "./pages/Devices";
import Sensors from "./pages/Sensors";
import Mappings from "./pages/Mappings";
import Artifacts from "./pages/Artifacts";
import DataBrowser from "./pages/DataBrowser";

export default function App() {
  const { isAuthenticated } = useAuth();

  if (!isAuthenticated) {
    return (
      <Routes>
        <Route path="/login" element={<Login />} />
        <Route path="*" element={<Navigate to="/login" replace />} />
      </Routes>
    );
  }

  return (
    <Routes>
      <Route element={<Layout />}>
        <Route path="/" element={<Overview />} />
        <Route
          path="/users"
          element={<RoleGate roles={["super_admin"]}><Users /></RoleGate>}
        />
        <Route
          path="/devices"
          element={
            <RoleGate roles={["device_engineer", "super_admin"]}><Devices /></RoleGate>
          }
        />
        <Route
          path="/sensors"
          element={
            <RoleGate roles={["device_engineer", "super_admin"]}><Sensors /></RoleGate>
          }
        />
        <Route path="/mappings" element={<Mappings />} />
        <Route
          path="/artifacts"
          element={
            <RoleGate roles={["device_engineer", "super_admin"]}><Artifacts /></RoleGate>
          }
        />
        <Route path="/data" element={<DataBrowser />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Route>
    </Routes>
  );
}
