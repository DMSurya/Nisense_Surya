import { useState } from "react";
import {
  Box,
  Button,
  Card,
  CardContent,
  Checkbox,
  Divider,
  FormControlLabel,
  Stack,
  TextField,
  Typography,
  Alert,
} from "@mui/material";
import { useNavigate } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";
import { tokens } from "../theme/nisense-tokens";

const ALL_ROLES = [
  "super_admin",
  "clinical_trial_admin",
  "doctor",
  "investigator",
  "patient",
  "device_engineer",
  "ai_analyst",
];

export default function Login() {
  const { devLogin, ssoLogin } = useAuth();
  const navigate = useNavigate();
  const [username, setUsername] = useState("admin");
  const [roles, setRoles] = useState<string[]>(["super_admin"]);
  const [error, setError] = useState("");

  const toggle = (r: string) =>
    setRoles((cur) => (cur.includes(r) ? cur.filter((x) => x !== r) : [...cur, r]));

  const doDev = async () => {
    try {
      await devLogin(username, roles);
      navigate("/");
    } catch (e) {
      setError(`Dev sign-in failed: ${e}`);
    }
  };

  return (
    <Box
      sx={{
        minHeight: "100vh",
        display: "grid",
        placeItems: "center",
        bgcolor: tokens.bg.primary,
      }}
    >
      <Card sx={{ width: 420 }}>
        <CardContent>
          <Typography variant="h5" fontWeight={700} gutterBottom>
            NiSense Platform
          </Typography>
          <Typography variant="body2" sx={{ color: tokens.text.muted }} gutterBottom>
            Sign in to the admin console.
          </Typography>

          {error && <Alert severity="error" sx={{ my: 1 }}>{error}</Alert>}

          <Button fullWidth variant="contained" sx={{ my: 2 }} onClick={ssoLogin}>
            Sign in with SSO (Keycloak / Google / Microsoft)
          </Button>

          <Divider>or dev sign-in (Pi test box)</Divider>

          <TextField
            fullWidth
            label="Username"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            margin="normal"
          />
          <Typography variant="caption" sx={{ color: tokens.text.muted }}>
            Roles
          </Typography>
          <Stack direction="row" flexWrap="wrap">
            {ALL_ROLES.map((r) => (
              <FormControlLabel
                key={r}
                control={<Checkbox checked={roles.includes(r)} onChange={() => toggle(r)} />}
                label={r}
                sx={{ width: "48%" }}
              />
            ))}
          </Stack>
          <Button fullWidth variant="outlined" sx={{ mt: 1 }} onClick={doDev}>
            Dev sign-in
          </Button>
        </CardContent>
      </Card>
    </Box>
  );
}
