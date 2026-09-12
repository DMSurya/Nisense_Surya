import { useState } from "react";
import {
  Box,
  Button,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Snackbar,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { DataGrid, type GridColDef } from "@mui/x-data-grid";
import { useDevices, useProvisionToken, useSaveDevice } from "../api/hooks";

export default function Devices() {
  const { data = [], isLoading } = useDevices();
  const save = useSaveDevice();
  const provision = useProvisionToken();
  const [open, setOpen] = useState(false);
  const [form, setForm] = useState({ device_code: "", device_type: "", version: "" });
  const [token, setToken] = useState("");

  const columns: GridColDef[] = [
    { field: "device_code", headerName: "Device Code", flex: 1 },
    { field: "device_type", headerName: "Type", flex: 1 },
    { field: "hw_id", headerName: "HW ID", flex: 1 },
    { field: "firmware_version", headerName: "FW", width: 100 },
    { field: "provisioned", headerName: "Provisioned", width: 120, type: "boolean" },
    {
      field: "actions",
      headerName: "Actions",
      width: 160,
      renderCell: (p) => (
        <Button
          size="small"
          onClick={async () => {
            const res = await provision.mutateAsync(p.row.id as string);
            setToken(res.token as string);
          }}
        >
          Provision token
        </Button>
      ),
    },
  ];

  const submit = async () => {
    await save.mutateAsync(form);
    setOpen(false);
    setForm({ device_code: "", device_type: "", version: "" });
  };

  return (
    <Box>
      <Stack direction="row" justifyContent="space-between" mb={2}>
        <Typography variant="h5" fontWeight={700}>Device Master</Typography>
        <Button variant="contained" onClick={() => setOpen(true)}>Add device</Button>
      </Stack>
      <div style={{ height: 560 }}>
        <DataGrid rows={data} columns={columns} loading={isLoading} getRowId={(r) => r.id} />
      </div>

      <Dialog open={open} onClose={() => setOpen(false)} fullWidth>
        <DialogTitle>New device</DialogTitle>
        <DialogContent>
          <TextField fullWidth margin="normal" label="Device code"
            value={form.device_code}
            onChange={(e) => setForm({ ...form, device_code: e.target.value })} />
          <TextField fullWidth margin="normal" label="Type"
            value={form.device_type}
            onChange={(e) => setForm({ ...form, device_type: e.target.value })} />
          <TextField fullWidth margin="normal" label="Version"
            value={form.version}
            onChange={(e) => setForm({ ...form, version: e.target.value })} />
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setOpen(false)}>Cancel</Button>
          <Button variant="contained" onClick={submit} disabled={!form.device_code}>Save</Button>
        </DialogActions>
      </Dialog>

      <Snackbar
        open={!!token}
        autoHideDuration={12000}
        onClose={() => setToken("")}
        message={`Provisioning token: ${token}`}
      />
    </Box>
  );
}
