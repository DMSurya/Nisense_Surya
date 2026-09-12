import { useState } from "react";
import {
  Box,
  Button,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { DataGrid, type GridColDef } from "@mui/x-data-grid";
import { useSaveSensor, useSensors } from "../api/hooks";

const columns: GridColDef[] = [
  { field: "sensor_id", headerName: "Sensor ID", flex: 1 },
  { field: "part_number", headerName: "Part No.", flex: 1 },
  { field: "supplier", headerName: "Supplier", flex: 1 },
  { field: "cost", headerName: "Cost", width: 100 },
  { field: "moq", headerName: "MOQ", width: 90 },
  { field: "currency", headerName: "Curr", width: 80 },
];

export default function Sensors() {
  const { data = [], isLoading } = useSensors();
  const save = useSaveSensor();
  const [open, setOpen] = useState(false);
  const [form, setForm] = useState({
    sensor_id: "",
    part_number: "",
    supplier: "",
    cost: 0,
    moq: 0,
    currency: "USD",
  });

  const submit = async () => {
    await save.mutateAsync(form);
    setOpen(false);
  };

  return (
    <Box>
      <Stack direction="row" justifyContent="space-between" mb={2}>
        <Typography variant="h5" fontWeight={700}>Sensor Master</Typography>
        <Button variant="contained" onClick={() => setOpen(true)}>Add sensor</Button>
      </Stack>
      <div style={{ height: 560 }}>
        <DataGrid rows={data} columns={columns} loading={isLoading} getRowId={(r) => r.id} />
      </div>

      <Dialog open={open} onClose={() => setOpen(false)} fullWidth>
        <DialogTitle>New sensor</DialogTitle>
        <DialogContent>
          <TextField fullWidth margin="normal" label="Sensor ID"
            value={form.sensor_id}
            onChange={(e) => setForm({ ...form, sensor_id: e.target.value })} />
          <TextField fullWidth margin="normal" label="Part number"
            value={form.part_number}
            onChange={(e) => setForm({ ...form, part_number: e.target.value })} />
          <TextField fullWidth margin="normal" label="Supplier"
            value={form.supplier}
            onChange={(e) => setForm({ ...form, supplier: e.target.value })} />
          <Stack direction="row" spacing={2}>
            <TextField margin="normal" label="Cost" type="number"
              value={form.cost}
              onChange={(e) => setForm({ ...form, cost: Number(e.target.value) })} />
            <TextField margin="normal" label="MOQ" type="number"
              value={form.moq}
              onChange={(e) => setForm({ ...form, moq: Number(e.target.value) })} />
            <TextField margin="normal" label="Currency"
              value={form.currency}
              onChange={(e) => setForm({ ...form, currency: e.target.value })} />
          </Stack>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setOpen(false)}>Cancel</Button>
          <Button variant="contained" onClick={submit} disabled={!form.sensor_id}>Save</Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
}
