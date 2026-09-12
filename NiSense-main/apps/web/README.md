# NiSense Web Dashboard (`apps/web`)

React + TypeScript + Vite admin/operations console for the NiSense platform.
Phase 1 covers Module 1 (Platform Administration) and the core of Module 3
(Device Data Management): user & role management, Device/Sensor masters,
device↔patient / device↔algorithm mappings, firmware/model artifact management,
and a measurement **Data Browser** with date filters and Excel export. Later
phases (patient/clinical/AI dashboards) slot into the same nav shell.

## Stack

- MUI (+ X DataGrid) for the admin UI, ECharts for charts.
- TanStack Query for data fetching, React Router for routing.
- Auth: OIDC Authorization Code + PKCE (Keycloak/Cognito/Azure AD B2C) via
  `oidc-client-ts`, plus a dev sign-in that mints a token from the Pi `dev`
  provider (`/auth/dev-token`).
- Theme: `src/theme/nisense-tokens.ts` mirrors the shared brand palette (kept in
  sync with `apps/mobile/.../nisense_colors.dart`, `apps/pc/qml/Theme.qml`,
  `src/ui/ui_theme.h`) and feeds both the MUI theme and the ECharts theme.

## Develop

```bash
npm install
npm run dev          # http://localhost:5173 (proxies /api -> :8000, /auth -> :8080)
npm run typecheck
npm run build
```

Start the backend first (`../server/scripts/dev.sh up`), then use **Dev sign-in**
on the login page (e.g. username `admin`, role `super_admin`) — or **SSO** once
Keycloak IdPs are configured.

## Data Browser

Page: `src/pages/DataBrowser.tsx`.

| Control | Behavior |
|---------|----------|
| Type filter | Glucose, vitals, temp, PPG raw, glucose raw, … |
| Since / until | ISO date-time range sent to `GET /api/v1/readings` |
| Export to Excel | Downloads `GET /api/v1/export/readings.xlsx` with the same filters |

Sheets match the mobile XLSX layout (Summary, Glucose, Vitals, Temp, PPG_Raw,
Glucose_Raw). API detail: [`apps/server/README.md`](../server/README.md).  
Operator checklist: [`docs/guides/PLATFORM_GUIDE.md`](../../docs/guides/PLATFORM_GUIDE.md).

## RBAC

Nav items and routes are gated by role (authoritative checks are server-side):
`super_admin` (all), `device_engineer` (devices/sensors/artifacts/mappings),
`doctor` (mappings/data), etc. `super_admin` implies all roles.
