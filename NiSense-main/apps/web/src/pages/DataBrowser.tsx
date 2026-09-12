import { useMemo, useState } from "react";
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
import DownloadIcon from "@mui/icons-material/Download";
import { DataGrid, type GridColDef } from "@mui/x-data-grid";
import ReactECharts from "echarts-for-react";
import { api } from "../api/client";
import { useReadings } from "../api/hooks";
import { echartsTheme } from "../theme/echarts";
import { paramColor, tokens } from "../theme/nisense-tokens";

function extraNum(extra: Record<string, unknown> | undefined, key: string): number | null {
  const v = extra?.[key];
  if (typeof v === "number" && Number.isFinite(v)) return v;
  if (typeof v === "string" && v.trim() !== "" && !Number.isNaN(Number(v))) {
    return Number(v);
  }
  return null;
}

const SERIES: Record<string, { label: string; keys: string[]; colorKey: string }> = {
  glucose: { label: "Glucose", keys: ["value", "glucose_mg_dl"], colorKey: "glucose" },
  insulin: { label: "Insulin", keys: ["insulin", "actual_insulin", "insulin_uiu_ml"], colorKey: "insulin" },
  homa: { label: "HOMA-IR", keys: ["homa_ir", "homa_ir_index"], colorKey: "homa" },
  hr: { label: "Heart Rate", keys: ["hr_bpm", "value"], colorKey: "hr" },
  spo2: { label: "SpO₂", keys: ["spo2_percent", "spo2"], colorKey: "spo2" },
  hb: { label: "Hemoglobin", keys: ["hb_g_dl", "hb_g_dl_x10"], colorKey: "hb" },
  resp: { label: "Resp Rate", keys: ["resp_rate_bpm", "resp"], colorKey: "resp" },
  sdnn: { label: "SDNN", keys: ["sdnn_ms", "sdnn"], colorKey: "hrv" },
  rmssd: { label: "RMSSD", keys: ["rmssd_ms", "rmssd"], colorKey: "hrv" },
  bp_sys: { label: "BP Systolic", keys: ["systolic_mmhg", "systolic"], colorKey: "hr" },
  bp_dia: { label: "BP Diastolic", keys: ["diastolic_mmhg", "diastolic"], colorKey: "hr" },
  temp: { label: "Temperature", keys: ["skin_temp_c", "value"], colorKey: "temp" },
};

function pickSeriesValue(
  row: { value?: number; type: string; extra?: Record<string, unknown> },
  seriesKey: string,
): number | null {
  const spec = SERIES[seriesKey];
  if (!spec) return row.value ?? null;
  for (const k of spec.keys) {
    if (k === "value" && row.value != null) return row.value;
    const fromExtra = extraNum(row.extra, k);
    if (fromExtra != null) {
      if (k === "hb_g_dl_x10") return fromExtra / 10;
      return fromExtra;
    }
  }
  return null;
}

const columns: GridColDef[] = [
  { field: "ts", headerName: "Timestamp", flex: 1.2 },
  { field: "device_id", headerName: "Device", flex: 1 },
  { field: "type", headerName: "Type", width: 100 },
  { field: "value", headerName: "Value", width: 90 },
  { field: "quality", headerName: "Quality", width: 90 },
  {
    field: "extra",
    headerName: "Details",
    flex: 2,
    valueGetter: (_v, row) =>
      row.extra ? JSON.stringify(row.extra) : "",
  },
];

export default function DataBrowser() {
  const [deviceId, setDeviceId] = useState("");
  const [type, setType] = useState("vitals");
  const [seriesKey, setSeriesKey] = useState("hr");
  const [since, setSince] = useState("");
  const [until, setUntil] = useState("");
  const [exporting, setExporting] = useState(false);
  const [exportError, setExportError] = useState<string | null>(null);

  const filterParams = useMemo(
    () => ({
      type,
      ...(deviceId ? { device_id: deviceId } : {}),
      ...(since ? { since: new Date(since).toISOString() } : {}),
      ...(until ? { until: new Date(until).toISOString() } : {}),
    }),
    [type, deviceId, since, until],
  );
  const params = useMemo(() => ({ ...filterParams, limit: 500 }), [filterParams]);
  const { data = [], isLoading } = useReadings(params);

  const chartOption = useMemo(() => {
    const points = [...data]
      .reverse()
      .map((r) => [new Date(r.ts).getTime(), pickSeriesValue(r, seriesKey)]);
    const color = paramColor(SERIES[seriesKey]?.colorKey ?? type);
    return {
      ...echartsTheme,
      tooltip: { ...echartsTheme.tooltip, trigger: "axis" },
      xAxis: { type: "time" },
      yAxis: { type: "value", name: SERIES[seriesKey]?.label ?? seriesKey },
      series: [
        {
          name: SERIES[seriesKey]?.label ?? seriesKey,
          type: "line",
          showSymbol: false,
          data: points,
          lineStyle: { color },
          itemStyle: { color },
        },
      ],
    };
  }, [data, seriesKey, type]);

  async function handleExport() {
    setExporting(true);
    setExportError(null);
    try {
      const res = await api.get("/export/readings.xlsx", {
        params: filterParams,
        responseType: "blob",
      });
      const disposition = res.headers["content-disposition"] as string | undefined;
      const match = disposition?.match(/filename="?([^"]+)"?/);
      const filename = match?.[1] ?? `nisense_export_${Date.now()}.xlsx`;

      const url = window.URL.createObjectURL(res.data as Blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      a.remove();
      window.URL.revokeObjectURL(url);
    } catch (e) {
      setExportError(e instanceof Error ? e.message : "Export failed");
    } finally {
      setExporting(false);
    }
  }

  return (
    <Box>
      <Typography variant="h5" fontWeight={700} mb={2}>
        Measurement Data Repository
      </Typography>
      <Typography variant="body2" sx={{ color: tokens.text.muted }} mb={2}>
        Metabolic / Vital / Vascular fields live in reading <code>extra</code>; pick a
        series below to chart them.
      </Typography>

      <Stack direction="row" spacing={2} mb={2} flexWrap="wrap" useFlexGap alignItems="center">
        <TextField
          label="Device ID"
          value={deviceId}
          onChange={(e) => setDeviceId(e.target.value)}
        />
        <TextField
          select
          label="Record type"
          value={type}
          onChange={(e) => setType(e.target.value)}
          sx={{ minWidth: 160 }}
        >
          <MenuItem value="glucose">Glucose</MenuItem>
          <MenuItem value="vitals">Vitals</MenuItem>
          <MenuItem value="temp">Temperature</MenuItem>
          <MenuItem value="ppg_raw">PPG Raw</MenuItem>
          <MenuItem value="glucose_raw">Glucose Raw</MenuItem>
        </TextField>
        <TextField
          select
          label="Chart series"
          value={seriesKey}
          onChange={(e) => setSeriesKey(e.target.value)}
          sx={{ minWidth: 180 }}
        >
          {Object.entries(SERIES).map(([k, s]) => (
            <MenuItem key={k} value={k}>
              {s.label}
            </MenuItem>
          ))}
        </TextField>
        <TextField
          label="Since"
          type="datetime-local"
          value={since}
          onChange={(e) => setSince(e.target.value)}
          InputLabelProps={{ shrink: true }}
        />
        <TextField
          label="Until"
          type="datetime-local"
          value={until}
          onChange={(e) => setUntil(e.target.value)}
          InputLabelProps={{ shrink: true }}
        />
        <Button
          variant="outlined"
          startIcon={<DownloadIcon />}
          onClick={handleExport}
          disabled={exporting}
        >
          {exporting ? "Exporting…" : "Export to Excel"}
        </Button>
      </Stack>
      {exportError && (
        <Typography variant="body2" color="error" mb={2}>
          {exportError}
        </Typography>
      )}

      <Card sx={{ mb: 3 }}>
        <CardContent>
          <ReactECharts option={chartOption} style={{ height: 320 }} notMerge />
        </CardContent>
      </Card>

      <div style={{ height: 420 }}>
        <DataGrid
          rows={data}
          columns={columns}
          loading={isLoading}
          getRowId={(r) => r.id}
        />
      </div>
    </Box>
  );
}
