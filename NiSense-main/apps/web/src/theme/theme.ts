import { createTheme } from "@mui/material/styles";
import { tokens } from "./nisense-tokens";

// MUI dark theme fed by the shared NiSense tokens.
export const theme = createTheme({
  palette: {
    mode: "dark",
    primary: { main: tokens.brand.primary },
    secondary: { main: tokens.brand.secondary },
    success: { main: tokens.status.normal },
    warning: { main: tokens.status.warning },
    error: { main: tokens.status.critical },
    info: { main: tokens.status.info },
    background: {
      default: tokens.bg.primary,
      paper: tokens.bg.card,
    },
    text: {
      primary: tokens.text.primary,
      secondary: tokens.text.muted,
    },
    divider: tokens.border.subtle,
  },
  shape: { borderRadius: 12 },
  typography: {
    fontFamily:
      "Inter, Roboto, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif",
  },
  components: {
    MuiAppBar: {
      styleOverrides: { root: { backgroundColor: tokens.bg.header } },
    },
    MuiDrawer: {
      styleOverrides: { paper: { backgroundColor: tokens.bg.nav } },
    },
    MuiCard: {
      styleOverrides: {
        root: {
          backgroundColor: tokens.bg.card,
          border: `1px solid ${tokens.border.subtle}`,
        },
      },
    },
  },
});
