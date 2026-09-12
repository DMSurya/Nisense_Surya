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
import { useArtifacts, useDeleteArtifact, useUploadArtifact } from "../api/hooks";

const columns: GridColDef[] = [
  { field: "kind", headerName: "Kind", width: 110 },
  { field: "variant", headerName: "Variant", width: 120 },
  { field: "version", headerName: "Version", flex: 1 },
  { field: "filename", headerName: "File", flex: 1 },
  {
    field: "size_bytes",
    headerName: "Size",
    width: 110,
    valueGetter: (v) => `${((v as number) / 1024).toFixed(1)} KB`,
  },
  { field: "sha256", headerName: "SHA-256", flex: 1.5 },
];

export default function Artifacts() {
  const { data = [], isLoading } = useArtifacts();
  const upload = useUploadArtifact();
  const del = useDeleteArtifact();
  const [kind, setKind] = useState("firmware");
  const [variant, setVariant] = useState("");
  const [version, setVersion] = useState("");
  const [signature, setSignature] = useState("");
  const [file, setFile] = useState<File | null>(null);
  const [msg, setMsg] = useState("");

  const submit = async () => {
    if (!file || !version) return;
    const form = new FormData();
    form.append("kind", kind);
    form.append("version", version);
    if (variant) form.append("variant", variant);
    if (signature) form.append("signature_b64", signature);
    form.append("file", file);
    try {
      await upload.mutateAsync(form);
      setMsg("Uploaded");
      setVersion("");
      setSignature("");
      setFile(null);
    } catch (e) {
      setMsg(`Upload failed: ${e}`);
    }
  };

  return (
    <Box>
      <Typography variant="h5" fontWeight={700} mb={2}>
        Firmware, Model &amp; Bundle Artifacts
      </Typography>

      <Card sx={{ mb: 3 }}>
        <CardContent>
          <Typography fontWeight={600} mb={1}>Upload signed artifact</Typography>
          <Stack direction="row" spacing={2} flexWrap="wrap" alignItems="center">
            <TextField select label="Kind" value={kind}
              onChange={(e) => {
                setKind(e.target.value);
                if (e.target.value !== "model") {
                  setVariant("");
                  setSignature("");
                }
              }} sx={{ minWidth: 140 }}>
              <MenuItem value="firmware">firmware</MenuItem>
              <MenuItem value="model">model</MenuItem>
              <MenuItem value="bundle">bundle (ZIP)</MenuItem>
            </TextField>
            {kind === "model" && (
              <TextField select label="Variant" value={variant}
                onChange={(e) => setVariant(e.target.value)} sx={{ minWidth: 140 }}>
                <MenuItem value="wearable">wearable</MenuItem>
                <MenuItem value="pulse">pulse</MenuItem>
              </TextField>
            )}
            <TextField label="Version" value={version}
              onChange={(e) => setVersion(e.target.value)} />
            {kind === "model" && (
              <TextField label="Signature (base64, Ed25519)" value={signature}
                onChange={(e) => setSignature(e.target.value)} sx={{ minWidth: 260 }} />
            )}
            <Button variant="outlined" component="label">
              {file ? file.name : "Choose file"}
              <input hidden type="file"
                accept={kind === "bundle" ? ".zip,application/zip" : undefined}
                onChange={(e) => setFile(e.target.files?.[0] ?? null)} />
            </Button>
            <Button variant="contained" onClick={submit} disabled={!file || !version}>
              Upload
            </Button>
          </Stack>
          {kind === "bundle" && (
            <Typography variant="caption" color="text.secondary" display="block" mt={1}>
              Upload a nisense-ota-v1 ZIP (manifest.json + components). Model signatures
              inside the manifest are verified when signed models are required.
            </Typography>
          )}
          {msg && <Typography variant="body2" mt={1}>{msg}</Typography>}
        </CardContent>
      </Card>

      <div style={{ height: 480 }}>
        <DataGrid
          rows={data}
          columns={[
            ...columns,
            {
              field: "actions",
              headerName: "",
              width: 100,
              sortable: false,
              filterable: false,
              renderCell: (params) => (
                <Button
                  size="small"
                  color="error"
                  disabled={del.isPending}
                  onClick={async () => {
                    if (!window.confirm(`Delete ${params.row.kind} ${params.row.version}?`)) {
                      return;
                    }
                    try {
                      await del.mutateAsync(params.row.id as string);
                      setMsg(`Deleted ${params.row.version}`);
                    } catch (e) {
                      setMsg(`Delete failed: ${e}`);
                    }
                  }}
                >
                  Delete
                </Button>
              ),
            },
          ]}
          loading={isLoading}
          getRowId={(r) => r.id}
        />
      </div>
    </Box>
  );
}
