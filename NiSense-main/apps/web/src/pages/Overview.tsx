import { Box, Card, CardContent, Grid, Stack, Typography } from "@mui/material";
import { useMemo, type ReactNode } from "react";
import { useArtifacts, useDevices, useReadings } from "../api/hooks";
import type { Reading } from "../api/hooks";
import { useAuth } from "../auth/AuthContext";
import ParamIcon from "../components/ParamIcon";
import { paramColor, tokens } from "../theme/nisense-tokens";

function num(extra: Record<string, unknown> | undefined, key: string): number | undefined {
  const v = extra?.[key];
  if (typeof v === "number" && Number.isFinite(v)) return v;
  if (typeof v === "string" && v.trim() !== "" && !Number.isNaN(Number(v))) {
    return Number(v);
  }
  return undefined;
}

function latestOf(readings: Reading[], type: string): Reading | undefined {
  return readings.find((r) => r.type === type);
}

function MetricTile({
  label,
  value,
  unit,
  paramKey,
}: {
  label: string;
  value: string;
  unit?: string;
  paramKey: string;
}) {
  return (
    <Card sx={{ height: "100%" }}>
      <CardContent>
        <Stack direction="row" spacing={1} alignItems="center" mb={1}>
          <ParamIcon paramKey={paramKey} size={20} />
          <Typography variant="body2" sx={{ color: tokens.text.muted }}>
            {label}
          </Typography>
        </Stack>
        <Typography variant="h5" fontWeight={700} sx={{ color: paramColor(paramKey) }}>
          {value}
          {unit ? (
            <Typography component="span" variant="body2" sx={{ ml: 0.75, color: tokens.text.dim }}>
              {unit}
            </Typography>
          ) : null}
        </Typography>
      </CardContent>
    </Card>
  );
}

function Section({
  title,
  children,
}: {
  title: string;
  children: ReactNode;
}) {
  return (
    <Box mb={3}>
      <Typography variant="h6" fontWeight={700} mb={1.5}>
        {title}
      </Typography>
      <Grid container spacing={2}>
        {children}
      </Grid>
    </Box>
  );
}

function fmt(v: number | undefined, digits = 0): string {
  if (v == null || !Number.isFinite(v) || v <= 0) return "--";
  return digits > 0 ? v.toFixed(digits) : String(Math.round(v));
}

export default function Overview() {
  const { username } = useAuth();
  const { data: devices = [] } = useDevices();
  const { data: artifacts = [] } = useArtifacts();
  const { data: glucoseReadings = [] } = useReadings({ type: "glucose", limit: 50 });
  const { data: vitalsReadings = [] } = useReadings({ type: "vitals", limit: 50 });

  const provisioned = devices.filter((d) => d.provisioned).length;
  const firmware = artifacts.filter((a) => a.kind === "firmware").length;
  const models = artifacts.filter((a) => a.kind === "model").length;

  const clinical = useMemo(() => {
    const g = latestOf(glucoseReadings, "glucose");
    const v = latestOf(vitalsReadings, "vitals");
    const ge = g?.extra ?? {};
    const ve = v?.extra ?? {};

    return {
      glucose: g?.value ?? num(ge, "glucose_mg_dl"),
      insulin: num(ge, "insulin") ?? num(ge, "insulin_uiu_ml"),
      homa: num(ge, "homa_ir") ?? num(ge, "homa_ir_index"),
      hr: num(ve, "hr_bpm") ?? (v?.value && v.value > 30 && v.value < 220 ? v.value : undefined),
      spo2: num(ve, "spo2_percent") ?? num(ve, "spo2"),
      hb: num(ve, "hb_g_dl") ??
        (num(ve, "hb_g_dl_x10") != null ? (num(ve, "hb_g_dl_x10") as number) / 10 : undefined),
      resp: num(ve, "resp_rate_bpm") ?? num(ve, "resp"),
      sdnn: num(ve, "sdnn_ms") ?? num(ve, "sdnn"),
      rmssd: num(ve, "rmssd_ms") ?? num(ve, "rmssd"),
      sys: num(ve, "systolic_mmhg") ?? num(ve, "systolic"),
      dia: num(ve, "diastolic_mmhg") ?? num(ve, "diastolic"),
      gTs: g?.ts,
      vTs: v?.ts,
    };
  }, [glucoseReadings, vitalsReadings]);

  return (
    <Box>
      <Typography variant="h5" fontWeight={700} mb={1}>
        Welcome, {username ?? "user"}
      </Typography>
      <Typography variant="body2" sx={{ color: tokens.text.muted }} mb={3}>
        NiSense Link — latest Metabolic, Vital, and Vascular results from synced devices.
      </Typography>

      <Section title="Metabolic">
        <Grid item xs={12} sm={4}>
          <MetricTile label="Glucose" value={fmt(clinical.glucose)} unit="mg/dL" paramKey="glucose" />
        </Grid>
        <Grid item xs={12} sm={4}>
          <MetricTile label="Insulin" value={fmt(clinical.insulin, 1)} unit="µIU/mL" paramKey="insulin" />
        </Grid>
        <Grid item xs={12} sm={4}>
          <MetricTile label="HOMA-IR" value={fmt(clinical.homa, 2)} paramKey="homa" />
        </Grid>
      </Section>

      <Section title="Vital">
        <Grid item xs={6} sm={3}>
          <MetricTile label="Heart Rate" value={fmt(clinical.hr)} unit="bpm" paramKey="hr" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="SpO₂" value={fmt(clinical.spo2)} unit="%" paramKey="spo2" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="Hemoglobin" value={fmt(clinical.hb, 1)} unit="g/dL" paramKey="hb" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="Resp Rate" value={fmt(clinical.resp)} unit="bpm" paramKey="resp" />
        </Grid>
      </Section>

      <Section title="Vascular">
        <Grid item xs={6} sm={3}>
          <MetricTile label="SDNN" value={fmt(clinical.sdnn)} unit="ms" paramKey="hrv" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="RMSSD" value={fmt(clinical.rmssd)} unit="ms" paramKey="hrv" />
        </Grid>
        <Grid item xs={12} sm={6}>
          <MetricTile
            label="Blood Pressure"
            value={
              clinical.sys && clinical.dia
                ? `${fmt(clinical.sys)}/${fmt(clinical.dia)}`
                : "--"
            }
            unit="mmHg"
            paramKey="bp"
          />
        </Grid>
      </Section>

      <Section title="Fleet">
        <Grid item xs={6} sm={3}>
          <MetricTile label="Devices" value={String(devices.length)} paramKey="temp" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="Provisioned" value={String(provisioned)} paramKey="spo2" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="Firmware" value={String(firmware)} paramKey="hr" />
        </Grid>
        <Grid item xs={6} sm={3}>
          <MetricTile label="Models" value={String(models)} paramKey="glucose" />
        </Grid>
      </Section>

      <Typography variant="caption" sx={{ color: tokens.text.dim }}>
        Latest glucose {clinical.gTs ? new Date(clinical.gTs).toLocaleString() : "—"} · vitals{" "}
        {clinical.vTs ? new Date(clinical.vTs).toLocaleString() : "—"}
      </Typography>
    </Box>
  );
}
