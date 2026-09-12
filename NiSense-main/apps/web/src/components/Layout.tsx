import {
  AppBar,
  Box,
  Chip,
  Drawer,
  List,
  ListItemButton,
  ListItemIcon,
  ListItemText,
  Stack,
  Toolbar,
  Typography,
  Button,
} from "@mui/material";
import PeopleIcon from "@mui/icons-material/People";
import DevicesIcon from "@mui/icons-material/Devices";
import SensorsIcon from "@mui/icons-material/Sensors";
import HubIcon from "@mui/icons-material/Hub";
import CloudUploadIcon from "@mui/icons-material/CloudUpload";
import TimelineIcon from "@mui/icons-material/Timeline";
import DashboardIcon from "@mui/icons-material/Dashboard";
import { NavLink, Outlet, useNavigate } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";
import { tokens } from "../theme/nisense-tokens";

const DRAWER_WIDTH = 248;

interface NavItem {
  to: string;
  label: string;
  icon: JSX.Element;
  roles: string[];
}

const NAV: NavItem[] = [
  { to: "/", label: "Overview", icon: <DashboardIcon />, roles: [] },
  { to: "/users", label: "Administration", icon: <PeopleIcon />, roles: ["super_admin"] },
  {
    to: "/devices",
    label: "Device Management",
    icon: <DevicesIcon />,
    roles: ["device_engineer", "super_admin"],
  },
  {
    to: "/sensors",
    label: "Sensor Management",
    icon: <SensorsIcon />,
    roles: ["device_engineer", "super_admin"],
  },
  {
    to: "/mappings",
    label: "Mappings",
    icon: <HubIcon />,
    roles: ["device_engineer", "doctor", "super_admin"],
  },
  {
    to: "/artifacts",
    label: "Firmware & Models",
    icon: <CloudUploadIcon />,
    roles: ["device_engineer", "super_admin"],
  },
  { to: "/data", label: "Data Repository", icon: <TimelineIcon />, roles: [] },
];

export default function Layout() {
  const { username, roles, hasRole, logout } = useAuth();
  const navigate = useNavigate();

  const visible = NAV.filter((n) => n.roles.length === 0 || hasRole(n.roles));

  return (
    <Box sx={{ display: "flex", bgcolor: tokens.bg.primary, minHeight: "100vh" }}>
      <AppBar position="fixed" sx={{ zIndex: (t) => t.zIndex.drawer + 1 }}>
        <Toolbar>
          <Typography variant="h6" sx={{ flexGrow: 1, fontWeight: 700 }}>
            NiSense Platform
          </Typography>
          <Stack direction="row" spacing={1} alignItems="center">
            {roles.slice(0, 3).map((r) => (
              <Chip key={r} label={r} size="small" color="primary" variant="outlined" />
            ))}
            <Typography variant="body2" sx={{ color: tokens.text.muted }}>
              {username}
            </Typography>
            <Button color="inherit" onClick={() => { logout(); navigate("/login"); }}>
              Sign out
            </Button>
          </Stack>
        </Toolbar>
      </AppBar>

      <Drawer
        variant="permanent"
        sx={{
          width: DRAWER_WIDTH,
          flexShrink: 0,
          [`& .MuiDrawer-paper`]: { width: DRAWER_WIDTH, boxSizing: "border-box" },
        }}
      >
        <Toolbar />
        <List>
          {visible.map((n) => (
            <ListItemButton
              key={n.to}
              component={NavLink}
              to={n.to}
              sx={{
                color: tokens.text.muted,
                "& .MuiListItemIcon-root": { color: tokens.text.muted, minWidth: 40 },
                "&.active": {
                  bgcolor: tokens.bg.elevated,
                  color: tokens.text.navActive,
                  "& .MuiListItemIcon-root": { color: tokens.text.navActive },
                },
                "&:hover": {
                  color: tokens.text.primary,
                  "& .MuiListItemIcon-root": { color: tokens.brand.secondary },
                },
              }}
            >
              <ListItemIcon>{n.icon}</ListItemIcon>
              <ListItemText primary={n.label} />
            </ListItemButton>
          ))}
        </List>
      </Drawer>

      <Box component="main" sx={{ flexGrow: 1, p: 3 }}>
        <Toolbar />
        <Outlet />
      </Box>
    </Box>
  );
}
