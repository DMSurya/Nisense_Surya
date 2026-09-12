import { tokens } from "./nisense-tokens";

// Shared ECharts theme object so charts match the admin UI palette.
export const echartsTheme = {
  backgroundColor: "transparent",
  textStyle: { color: tokens.text.muted },
  color: [
    tokens.brand.secondary,
    tokens.param.glucose,
    tokens.param.hr,
    tokens.param.spo2,
    tokens.param.temp,
    tokens.param.hb,
    tokens.brand.health,
  ],
  legend: { textStyle: { color: tokens.text.muted } },
  grid: { borderColor: tokens.border.subtle },
  categoryAxis: {
    axisLine: { lineStyle: { color: tokens.text.dim } },
    splitLine: { lineStyle: { color: tokens.border.subtle } },
  },
  valueAxis: {
    axisLine: { lineStyle: { color: tokens.text.dim } },
    splitLine: { lineStyle: { color: tokens.border.subtle } },
  },
  tooltip: {
    backgroundColor: tokens.bg.elevated,
    borderColor: tokens.border.mid,
    textStyle: { color: tokens.text.primary },
  },
};
