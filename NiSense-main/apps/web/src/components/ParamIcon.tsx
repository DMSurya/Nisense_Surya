import { Box } from "@mui/material";
import { paramColor } from "../theme/nisense-tokens";

const ICON_FILE: Record<string, string> = {
  hr: "heart.svg",
  spo2: "spo2.svg",
  glucose: "glucose.svg",
  hb: "hemoglobin.svg",
  resp: "resp.svg",
  insulin: "insulin.svg",
  homa: "homa.svg",
  temp: "temp.svg",
  hrv: "heart.svg",
  bp: "heart.svg",
};

/** Brand param icon from /public/icons (copied from mobile SVG set). */
export default function ParamIcon({
  paramKey,
  size = 22,
}: {
  paramKey: string;
  size?: number;
}) {
  const file = ICON_FILE[paramKey] ?? "heart.svg";
  return (
    <Box
      component="img"
      src={`/icons/${file}`}
      alt=""
      width={size}
      height={size}
      sx={{
        display: "block",
        // Tint via CSS filter approximate — SVGs are already colored; keep brand tint soft.
        filter: `drop-shadow(0 0 0 ${paramColor(paramKey)})`,
      }}
    />
  );
}
