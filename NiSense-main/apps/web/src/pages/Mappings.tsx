import { useState } from "react";
import {
  Box,
  Button,
  Card,
  CardContent,
  MenuItem,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { DataGrid, type GridColDef } from "@mui/x-data-grid";
import {
  useDeviceAlgorithmMaps,
  useDevicePatientMaps,
  useDevices,
  useMapDeviceAlgorithm,
  useMapDevicePatient,
  usePatients,
} from "../api/hooks";

const dpCols: GridColDef[] = [
  { field: "device_id", headerName: "Device", flex: 1 },
  { field: "patient_id", headerName: "Patient", flex: 1 },
  { field: "calibration_status", headerName: "Calibration", flex: 1 },
];
const daCols: GridColDef[] = [
  { field: "device_id", headerName: "Device", flex: 1 },
  { field: "algorithm_version", headerName: "Algo", flex: 1 },
  { field: "ai_model_version", headerName: "AI Model", flex: 1 },
  { field: "personalized_model_version", headerName: "Personalized", flex: 1 },
];

export default function Mappings() {
  const { data: devices = [] } = useDevices();
  const { data: patients = [] } = usePatients();
  const { data: dp = [] } = useDevicePatientMaps();
  const { data: da = [] } = useDeviceAlgorithmMaps();
  const mapDP = useMapDevicePatient();
  const mapDA = useMapDeviceAlgorithm();

  const [dpForm, setDpForm] = useState({ device_id: "", patient_id: "", calibration_status: "pending" });
  const [daForm, setDaForm] = useState({ device_id: "", algorithm_version: "", ai_model_version: "", personalized_model_version: "" });

  return (
    <Box>
      <Typography variant="h5" fontWeight={700} mb={2}>Device Mappings</Typography>

      <Card sx={{ mb: 3 }}>
        <CardContent>
          <Typography fontWeight={600} mb={1}>Device → Patient</Typography>
          <Stack direction="row" spacing={2} mb={2} flexWrap="wrap">
            <TextField select label="Device" sx={{ minWidth: 200 }}
              value={dpForm.device_id}
              onChange={(e) => setDpForm({ ...dpForm, device_id: e.target.value })}>
              {devices.map((d) => (
                <MenuItem key={d.id} value={d.id}>{d.device_code}</MenuItem>
              ))}
            </TextField>
            <TextField select label="Patient" sx={{ minWidth: 200 }}
              value={dpForm.patient_id}
              onChange={(e) => setDpForm({ ...dpForm, patient_id: e.target.value })}>
              {patients.map((p) => (
                <MenuItem key={p.id} value={p.id}>{p.nisense_id}</MenuItem>
              ))}
            </TextField>
            <TextField label="Calibration status"
              value={dpForm.calibration_status}
              onChange={(e) => setDpForm({ ...dpForm, calibration_status: e.target.value })} />
            <Button variant="contained"
              disabled={!dpForm.device_id || !dpForm.patient_id}
              onClick={() => mapDP.mutate(dpForm)}>Map</Button>
          </Stack>
          <div style={{ height: 240 }}>
            <DataGrid rows={dp} columns={dpCols} getRowId={(r) => r.id as string} />
          </div>
        </CardContent>
      </Card>

      <Card>
        <CardContent>
          <Typography fontWeight={600} mb={1}>Device → Algorithm / Model</Typography>
          <Stack direction="row" spacing={2} mb={2} flexWrap="wrap">
            <TextField select label="Device" sx={{ minWidth: 200 }}
              value={daForm.device_id}
              onChange={(e) => setDaForm({ ...daForm, device_id: e.target.value })}>
              {devices.map((d) => (
                <MenuItem key={d.id} value={d.id}>{d.device_code}</MenuItem>
              ))}
            </TextField>
            <TextField label="Algorithm ver"
              value={daForm.algorithm_version}
              onChange={(e) => setDaForm({ ...daForm, algorithm_version: e.target.value })} />
            <TextField label="AI model ver"
              value={daForm.ai_model_version}
              onChange={(e) => setDaForm({ ...daForm, ai_model_version: e.target.value })} />
            <TextField label="Personalized ver"
              value={daForm.personalized_model_version}
              onChange={(e) => setDaForm({ ...daForm, personalized_model_version: e.target.value })} />
            <Button variant="contained" disabled={!daForm.device_id}
              onClick={() => mapDA.mutate(daForm)}>Map</Button>
          </Stack>
          <div style={{ height: 240 }}>
            <DataGrid rows={da} columns={daCols} getRowId={(r) => r.id as string} />
          </div>
        </CardContent>
      </Card>
    </Box>
  );
}
