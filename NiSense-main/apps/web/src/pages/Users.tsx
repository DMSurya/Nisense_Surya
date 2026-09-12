import { useState } from "react";
import {
  Box,
  Button,
  Checkbox,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  FormControlLabel,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { DataGrid, type GridColDef } from "@mui/x-data-grid";
import { useSaveUser, useUsers } from "../api/hooks";

const ROLES = [
  "super_admin",
  "clinical_trial_admin",
  "doctor",
  "investigator",
  "patient",
  "device_engineer",
  "ai_analyst",
];

const columns: GridColDef[] = [
  { field: "username", headerName: "Username", flex: 1 },
  { field: "email", headerName: "Email", flex: 1 },
  {
    field: "roles",
    headerName: "Roles",
    flex: 1.5,
    valueGetter: (_v, row) => (row.roles ?? []).join(", "),
  },
  { field: "is_active", headerName: "Active", width: 100, type: "boolean" },
];

export default function Users() {
  const { data = [], isLoading } = useUsers();
  const save = useSaveUser();
  const [open, setOpen] = useState(false);
  const [username, setUsername] = useState("");
  const [email, setEmail] = useState("");
  const [roles, setRoles] = useState<string[]>([]);

  const toggle = (r: string) =>
    setRoles((c) => (c.includes(r) ? c.filter((x) => x !== r) : [...c, r]));

  const submit = async () => {
    await save.mutateAsync({ username, email, roles });
    setOpen(false);
    setUsername("");
    setEmail("");
    setRoles([]);
  };

  return (
    <Box>
      <Stack direction="row" justifyContent="space-between" mb={2}>
        <Typography variant="h5" fontWeight={700}>
          User &amp; Role Management
        </Typography>
        <Button variant="contained" onClick={() => setOpen(true)}>
          Add user
        </Button>
      </Stack>
      <div style={{ height: 560 }}>
        <DataGrid rows={data} columns={columns} loading={isLoading} getRowId={(r) => r.id} />
      </div>

      <Dialog open={open} onClose={() => setOpen(false)} fullWidth>
        <DialogTitle>New user</DialogTitle>
        <DialogContent>
          <TextField fullWidth margin="normal" label="Username"
            value={username} onChange={(e) => setUsername(e.target.value)} />
          <TextField fullWidth margin="normal" label="Email"
            value={email} onChange={(e) => setEmail(e.target.value)} />
          <Stack direction="row" flexWrap="wrap">
            {ROLES.map((r) => (
              <FormControlLabel key={r} sx={{ width: "48%" }}
                control={<Checkbox checked={roles.includes(r)} onChange={() => toggle(r)} />}
                label={r} />
            ))}
          </Stack>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setOpen(false)}>Cancel</Button>
          <Button variant="contained" onClick={submit} disabled={!username}>Save</Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
}
