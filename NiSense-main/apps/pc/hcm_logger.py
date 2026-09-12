"""
HCM BLE Logger
==============
CSV logger with firmware-identical column headers for all measurement types.
Files are created under ~/HCM_Logs/ with timestamped filenames.
"""

import csv
import logging
import os
import time
from datetime import datetime
from pathlib import Path
from typing import Optional

from hcm_protocol import (
    GlucoseAlgoData, GlucoseData, GlucoseSampleData, HrData, PpgSample, SpO2Data,
    TemperatureData, VitalsData,
    MEAS_TYPE_GLUCOSE, MEAS_TYPE_HR, MEAS_TYPE_SPO2,
)

LOG_ROOT = Path.home() / "HCM_Logs"
log = logging.getLogger(__name__)


def _log_dir() -> Path:
    LOG_ROOT.mkdir(parents=True, exist_ok=True)
    return LOG_ROOT


def _ts_prefix() -> str:
    return datetime.now().strftime("%Y_%m_%d-%H_%M_%S")


class HrLogger:
    """Logs HR results — mirrors firmware hr_logger CSV format."""

    HEADERS = [
        "Timestamp_unix", "Date", "Time", "Device_ID",
        "Sample_Rate_Hz", "Total_Samples", "Total_Time_sec",
        "HR_BPM", "Confidence",
    ]

    def __init__(self, device_id: str = "HCM") -> None:
        self._device_id = device_id
        self._path = _log_dir() / f"{_ts_prefix()}_HR.csv"
        self._file = open(self._path, "w", newline="", encoding="utf-8")
        self._writer = csv.DictWriter(self._file, fieldnames=self.HEADERS)
        self._writer.writeheader()

    def log(self, data: HrData) -> None:
        dt = datetime.fromtimestamp(data.timestamp)
        self._writer.writerow({
            "Timestamp_unix": data.timestamp,
            "Date": dt.strftime("%Y-%m-%d"),
            "Time": dt.strftime("%H:%M:%S"),
            "Device_ID": self._device_id,
            "Sample_Rate_Hz": 33,
            "Total_Samples": 0,
            "Total_Time_sec": 0,
            "HR_BPM": data.hr_bpm,
            "Confidence": data.confidence,
        })
        self._file.flush()

    def close(self) -> None:
        self._file.close()

    @property
    def path(self) -> Path:
        return self._path


class SpO2Logger:
    """Logs SpO2 results — mirrors firmware spo2_logger CSV format."""

    HEADERS = [
        "Timestamp_unix", "Date", "Time", "Device_ID",
        "Sample_Rate_Hz", "Total_Samples", "Total_Time_sec",
        "SpO2_Percent", "Confidence",
    ]

    def __init__(self, device_id: str = "HCM") -> None:
        self._device_id = device_id
        self._path = _log_dir() / f"{_ts_prefix()}_SpO2.csv"
        self._file = open(self._path, "w", newline="", encoding="utf-8")
        self._writer = csv.DictWriter(self._file, fieldnames=self.HEADERS)
        self._writer.writeheader()

    def log(self, data: SpO2Data) -> None:
        dt = datetime.fromtimestamp(data.timestamp)
        self._writer.writerow({
            "Timestamp_unix": data.timestamp,
            "Date": dt.strftime("%Y-%m-%d"),
            "Time": dt.strftime("%H:%M:%S"),
            "Device_ID": self._device_id,
            "Sample_Rate_Hz": 33,
            "Total_Samples": 0,
            "Total_Time_sec": 0,
            "SpO2_Percent": data.spo2_percent,
            "Confidence": data.confidence,
        })
        self._file.flush()

    def close(self) -> None:
        self._file.close()

    @property
    def path(self) -> Path:
        return self._path


class GlucoseLogger:
    """Logs glucose data with firmware-compatible 40-column CSV schema."""

    HEADERS = [
        "Timestamp_unix", "Date", "Time", "Device_ID", "Patient_Code", "Patient_Name",
        "Total_Time_sec", "ADC_Samples", "Voltage_mV", "Total_Coeff", "Intercept",
        "Y1_Value", "Avg_Val", "Std_Dev", "Upper_Lim", "Lower_Lim", "P_Count", "N_Count",
        "P_Val", "N_Val", "P_Plus_N", "Y2_Val", "Y2_Percent", "Group_CD", "Y2_Factor",
        "Y2_Factor_Val", "Const", "Y3_Value", "Y3_Row_No", "Elim_Per", "Elim_Val", "Y_Value",
        "Calibration_Factor", "AG_Adjusted", "Normalized_Glucose", "Actual_Insulin",
        "Insulin_Correction", "Insulin_Ratio", "Inverse_Ratio", "HOMA_IR_Index",
    ]

    # Single persistent file — mirrors firmware /NAND:/glucose.csv behaviour.
    FIXED_FILENAME = "glucose.csv"

    def __init__(self, device_id: str = "HCM", patient_name: str = "") -> None:
        self._device_id = device_id
        self._patient_name = patient_name or "BLE"
        self._path = _log_dir() / self.FIXED_FILENAME
        self._ensure_file_header()
        # Accumulate sample data; written as semicolon-joined strings in log_result()
        self._adc_samples: list = []
        self._voltages_mv: list = []
        self._sample_timestamps: list = []
        self._pending_algo: Optional[GlucoseAlgoData] = None
        self._pending_result: Optional[tuple] = None
        self._deferred_rows: list = []
        self._last_file_lock_warn_ts = 0.0
        self._csv_locked = False

    def _ensure_file_header(self) -> bool:
        need_header = not self._path.exists() or self._path.stat().st_size == 0
        if not need_header:
            return True
        try:
            with open(self._path, "a", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=self.HEADERS)
                writer.writeheader()
            self._csv_locked = False
            return True
        except (PermissionError, OSError) as exc:
            self._csv_locked = True
            self._log_file_lock_warning(exc)
            return False

    def _log_file_lock_warning(self, exc: Exception) -> None:
        now = time.time()
        # Avoid flooding logs when OS/external app holds the file lock.
        if now - self._last_file_lock_warn_ts >= 5.0:
            self._last_file_lock_warn_ts = now
            log.warning(
                "Glucose CSV is locked (%s). Deferring write and will retry.",
                exc,
            )

    def _append_rows(self, rows: list) -> bool:
        if not self._ensure_file_header():
            return False
        try:
            with open(self._path, "a", newline="", encoding="utf-8") as f:
                writer = csv.DictWriter(f, fieldnames=self.HEADERS)
                writer.writerows(rows)
            self._csv_locked = False
            return True
        except (PermissionError, OSError) as exc:
            self._csv_locked = True
            self._log_file_lock_warning(exc)
            return False

    def get_write_status(self) -> tuple:
        """Return (locked, deferred_count) for UI status indication."""
        return self._csv_locked, len(self._deferred_rows)

    def set_patient_name(self, name: str) -> None:
        self._patient_name = (name or "").strip() or "BLE"

    def _write_row(self, row: dict) -> bool:
        # Flush deferred rows first to preserve order.
        if self._deferred_rows:
            payload = self._deferred_rows + [row]
            if self._append_rows(payload):
                self._deferred_rows.clear()
                return True
            self._deferred_rows.append(row)
            return False

        if self._append_rows([row]):
            return True

        self._deferred_rows.append(row)
        return False

    def _build_row(self, data: GlucoseData, adc_samples: list, voltages_mv: list, sample_timestamps: list) -> dict:
        row = self._base_row(data.timestamp, self._patient_name)
        # Best-effort mapping from BLE final packet to firmware-like fields.
        row["AG_Adjusted"] = float(data.glucose_mg_dl)
        row["Normalized_Glucose"] = float(data.glucose_mmol_l)
        row["Y_Value"] = int(data.glucose_mg_dl)
        row["P_Count"] = int(data.quality)
        if len(sample_timestamps) >= 2:
            row["Total_Time_sec"] = sample_timestamps[-1] - sample_timestamps[0]
        if adc_samples:
            row["ADC_Samples"] = ";".join(str(v) for v in adc_samples)
        if voltages_mv:
            row["Voltage_mV"] = ";".join(f"{v:.2f}" for v in voltages_mv)
        a = self._pending_algo
        if a is not None:
            row["Total_Coeff"] = round(a.tot_coeff, 4)
            row["Intercept"] = round(a.intercept, 6)
            row["Y1_Value"] = round(a.y1_value, 2)
            row["Avg_Val"] = round(a.avg_val, 4)
            row["Std_Dev"] = round(a.std_dev, 4)
            row["Upper_Lim"] = round(a.up_lim, 4)
            row["Lower_Lim"] = round(a.ll_lim, 4)
            row["P_Count"] = a.p_count
            row["N_Count"] = a.n_count
            row["P_Val"] = round(a.p_val, 4)
            row["N_Val"] = round(a.n_val, 4)
            row["P_Plus_N"] = round(a.p_plus_n, 4)
            row["Y2_Val"] = round(a.y2_val, 2)
            row["Y2_Percent"] = round(a.y2_percent, 4)
            row["Group_CD"] = a.group_cd
            row["Y2_Factor"] = round(a.y2_factor, 6)
            row["Y2_Factor_Val"] = round(a.y2_factor_val, 4)
            row["Const"] = round(a.const_val, 4)
            row["Y3_Value"] = round(a.y3_value, 4)
            row["Y3_Row_No"] = a.y3_row_no
            row["Elim_Per"] = round(a.elim_per, 4)
            row["Elim_Val"] = round(a.elim_val, 4)
            row["Y_Value"] = a.y_value
            row["Calibration_Factor"] = round(a.calibration_factor, 6)
            row["AG_Adjusted"] = round(a.ag_adjusted, 6)
            row["Normalized_Glucose"] = round(a.normalized_glucose, 6)
            row["Actual_Insulin"] = round(a.actual_insulin, 6)
            row["Insulin_Correction"] = round(a.insulin_correction, 6)
            row["Insulin_Ratio"] = round(a.insulin_ratio, 6)
            row["Inverse_Ratio"] = round(a.inverse_ratio, 6)
            row["HOMA_IR_Index"] = round(a.homa_ir_index, 6)
        return row

    def _flush_pending_result(self, allow_without_algo: bool) -> None:
        if self._pending_result is None:
            return
        if self._pending_algo is None and not allow_without_algo:
            return
        data, adc_samples, voltages_mv, sample_timestamps = self._pending_result
        used_algo = self._pending_algo is not None
        row = self._build_row(data, adc_samples, voltages_mv, sample_timestamps)
        if self._write_row(row):
            self._pending_result = None
            if used_algo:
                self._pending_algo = None

    def _base_row(self, timestamp: int, patient_name: str) -> dict:
        dt = datetime.fromtimestamp(timestamp)
        return {
            "Timestamp_unix": timestamp,
            "Date": dt.strftime("%Y-%m-%d"),
            "Time": dt.strftime("%H:%M:%S"),
            "Device_ID": self._device_id,
            "Patient_Code": "",
            "Patient_Name": patient_name,
            "Total_Time_sec": 0.0,
            "ADC_Samples": "",
            "Voltage_mV": "",
            "Total_Coeff": 0.0,
            "Intercept": 0.0,
            "Y1_Value": 0.0,
            "Avg_Val": 0.0,
            "Std_Dev": 0.0,
            "Upper_Lim": 0.0,
            "Lower_Lim": 0.0,
            "P_Count": 0,
            "N_Count": 0,
            "P_Val": 0.0,
            "N_Val": 0.0,
            "P_Plus_N": 0.0,
            "Y2_Val": 0.0,
            "Y2_Percent": 0.0,
            "Group_CD": 0,
            "Y2_Factor": 0.0,
            "Y2_Factor_Val": 0.0,
            "Const": 0.0,
            "Y3_Value": 0.0,
            "Y3_Row_No": 0,
            "Elim_Per": 0.0,
            "Elim_Val": 0.0,
            "Y_Value": 0,
            "Calibration_Factor": 0.0,
            "AG_Adjusted": 0.0,
            "Normalized_Glucose": 0.0,
            "Actual_Insulin": 0.0,
            "Insulin_Correction": 0.0,
            "Insulin_Ratio": 0.0,
            "Inverse_Ratio": 0.0,
            "HOMA_IR_Index": 0.0,
        }

    def set_algo(self, algo: GlucoseAlgoData) -> None:
        """Store algorithm details and flush pending result when available."""
        self._pending_algo = algo
        self._flush_pending_result(allow_without_algo=False)

    def log_result(self, data: GlucoseData) -> None:
        # If the previous result was waiting for algo but none arrived yet,
        # write it now with fallback values to avoid data loss.
        self._flush_pending_result(allow_without_algo=True)

        adc_samples = list(self._adc_samples)
        voltages_mv = list(self._voltages_mv)
        sample_timestamps = list(self._sample_timestamps)
        self._adc_samples.clear()
        self._voltages_mv.clear()
        self._sample_timestamps.clear()

        self._pending_result = (data, adc_samples, voltages_mv, sample_timestamps)
        # If algo already arrived first, this flushes immediately with full fields.
        self._flush_pending_result(allow_without_algo=False)

    def log_sample(self, data: GlucoseSampleData) -> None:
        """Accumulate a raw sample; data is written as part of the result row."""
        self._adc_samples.append(data.raw_adc_value)
        self._voltages_mv.append(data.voltage_mv)
        self._sample_timestamps.append(data.timestamp)

    # Backward-compatible alias: treat plain log() as final result logging.
    def log(self, data: GlucoseData) -> None:
        self.log_result(data)

    def close(self) -> None:
        # Ensure last pending result is not dropped when app closes.
        self._flush_pending_result(allow_without_algo=True)
        if self._deferred_rows:
            # Final best effort before shutdown.
            if self._append_rows(self._deferred_rows):
                self._deferred_rows.clear()

    @property
    def path(self) -> Path:
        return self._path


class PpgStreamLogger:
    """Logs PPG raw stream — rolling per-session file."""

    HEADERS = [
        "Timestamp_unix_ms", "Sample_Num",
        "Raw_IR", "Raw_Red", "Raw_Green",
        "Accel_X_mg", "Accel_Y_mg", "Accel_Z_mg",
    ]

    def __init__(self) -> None:
        self._path = _log_dir() / f"{_ts_prefix()}_PPG_Stream.csv"
        self._file = open(self._path, "w", newline="", encoding="utf-8")
        self._writer = csv.DictWriter(self._file, fieldnames=self.HEADERS)
        self._writer.writeheader()

    def log(self, sample: PpgSample) -> None:
        try:
            self._writer.writerow({
                "Timestamp_unix_ms": sample.timestamp_ms,
                "Sample_Num": sample.sample_num,
                "Raw_IR": sample.raw_ir,
                "Raw_Red": sample.raw_red,
                "Raw_Green": sample.raw_green,
                "Accel_X_mg": sample.accel_x,
                "Accel_Y_mg": sample.accel_y,
                "Accel_Z_mg": sample.accel_z,
            })
            self._file.flush()
        except ValueError:
            # Typical when the CSV backing file was closed but a late BLE notify
            # still arrives (disconnect race). Drop the sample quietly.
            pass

    def close(self) -> None:
        try:
            self._file.close()
        except Exception:
            pass

    @property
    def path(self) -> Path:
        return self._path


class VitalsLogger:
    """Logs unified vitals (HR + SpO2 + Hb + RespRate) per session."""

    HEADERS = [
        "Timestamp_unix", "Date", "Time", "Device_ID",
        "HR_BPM", "HR_Confidence", "HR_Valid",
        "SpO2_Percent", "SpO2_Confidence", "SpO2_Valid",
        "Hb_g_dL", "Hb_Confidence", "Hb_Valid",
        "RespRate_BPM", "RespRate_Confidence", "RespRate_Valid",
        "R_Value_x1000", "Quality", "Flags",
    ]

    def __init__(self, device_id: str = "HCM") -> None:
        self._device_id = device_id
        self._path = _log_dir() / f"{_ts_prefix()}_Vitals.csv"
        self._file = open(self._path, "a", newline="", encoding="utf-8")
        self._writer = csv.DictWriter(self._file, fieldnames=self.HEADERS)
        if self._path.stat().st_size == 0:
            self._writer.writeheader()

    def log(self, data: VitalsData) -> None:
        try:
            dt = datetime.fromtimestamp(data.timestamp) if data.timestamp else datetime.utcnow()
            self._writer.writerow({
                "Timestamp_unix": data.timestamp,
                "Date": dt.strftime("%Y-%m-%d"),
                "Time": dt.strftime("%H:%M:%S"),
                "Device_ID": self._device_id,
                "HR_BPM": data.hr_bpm,
                "HR_Confidence": data.hr_confidence,
                "HR_Valid": int(data.hr_valid),
                "SpO2_Percent": data.spo2_percent,
                "SpO2_Confidence": data.spo2_confidence,
                "SpO2_Valid": int(data.spo2_valid),
                "Hb_g_dL": data.hb_g_dl,
                "Hb_Confidence": data.hb_confidence,
                "Hb_Valid": int(data.hb_valid),
                "RespRate_BPM": data.resp_rate_bpm,
                "RespRate_Confidence": data.resp_confidence,
                "RespRate_Valid": int(data.resp_valid),
                "R_Value_x1000": data.r_value_x1000,
                "Quality": data.quality,
                "Flags": data.flags,
            })
            self._file.flush()
        except ValueError:
            pass

    def close(self) -> None:
        try:
            self._file.close()
        except Exception:
            pass

    @property
    def path(self) -> Path:
        return self._path


class TemperatureLogger:
    """Logs periodic temperature snapshots — one row per BLE notify (every 5 s)."""

    HEADERS = [
        "Timestamp_unix", "Date", "Time", "Device_ID",
        "Temp_C",
    ]

    def __init__(self, device_id: str = "HCM") -> None:
        self._device_id = device_id
        self._path = _log_dir() / f"{_ts_prefix()}_Temperature.csv"
        self._file = open(self._path, "w", newline="", encoding="utf-8")
        self._writer = csv.DictWriter(self._file, fieldnames=self.HEADERS)
        self._writer.writeheader()

    def log(self, data: TemperatureData) -> None:
        try:
            ts = data.timestamp or int(time.time())
            dt = datetime.fromtimestamp(ts)
            self._writer.writerow({
                "Timestamp_unix": ts,
                "Date": dt.strftime("%Y-%m-%d"),
                "Time": dt.strftime("%H:%M:%S"),
                "Device_ID": self._device_id,
                "Temp_C": data.temp_c,
            })
            self._file.flush()
        except ValueError:
            pass

    def close(self) -> None:
        try:
            self._file.close()
        except Exception:
            pass

    @property
    def path(self) -> Path:
        return self._path


class SessionLogger:
    """
    Composite logger: manages one logger per measurement type.
    Open with a context manager or call open()/close() manually.
    """

    def __init__(self, device_id: str = "HCM") -> None:
        self._device_id = device_id
        self._patient_name: str = ""
        self.hr: Optional[HrLogger] = None
        self.spo2: Optional[SpO2Logger] = None
        self.vitals: Optional[VitalsLogger] = None
        self.glucose: Optional[GlucoseLogger] = None
        self.ppg: Optional[PpgStreamLogger] = None
        self.temperature: Optional[TemperatureLogger] = None

    def set_device_id(self, device_id: str) -> None:
        """Update hardware device ID used in CSV Device_ID column."""
        did = (device_id or "").strip()
        if not did:
            return
        self._device_id = did
        for logger in (
            self.hr, self.spo2, self.vitals, self.glucose,
            self.ppg, self.temperature,
        ):
            if logger is not None:
                logger._device_id = did

    def set_patient_name(self, name: str) -> None:
        """Update patient name and propagate to active glucose logger."""
        self._patient_name = name.strip()
        if self.glucose is not None:
            self.glucose.set_patient_name(self._patient_name)

    def open_hr(self) -> None:
        self.hr = HrLogger(self._device_id)

    def open_spo2(self) -> None:
        self.spo2 = SpO2Logger(self._device_id)

    def open_vitals(self) -> None:
        self.vitals = VitalsLogger(self._device_id)

    def open_glucose(self) -> None:
        self.glucose = GlucoseLogger(self._device_id, self._patient_name)

    def set_glucose_algo(self, algo: GlucoseAlgoData) -> None:
        """Forward algorithm details to the active glucose logger."""
        if self.glucose is None:
            self.open_glucose()
        glucose = self.glucose
        if glucose is not None:
            glucose.set_algo(algo)

    def glucose_write_status(self) -> tuple:
        """Return glucose CSV status as (locked, deferred_count)."""
        if self.glucose is None:
            return False, 0
        return self.glucose.get_write_status()

    def open_ppg_stream(self) -> None:
        self.ppg = PpgStreamLogger()

    def open_temperature(self) -> None:
        self.temperature = TemperatureLogger(self._device_id)

    def close_all(self) -> None:
        for logger in (self.hr, self.spo2, self.vitals, self.glucose, self.ppg, self.temperature):
            if logger:
                try:
                    logger.close()
                except Exception:
                    pass
        # Drop handles so late callbacks can't keep using a closed logger object.
        self.hr = None
        self.spo2 = None
        self.vitals = None
        self.glucose = None
        self.ppg = None
        self.temperature = None

    def log_hr(self, data: HrData) -> None:
        if self.hr is None:
            self.open_hr()
        hr = self.hr
        if hr is not None:
            hr.log(data)

    def log_spo2(self, data: SpO2Data) -> None:
        if self.spo2 is None:
            self.open_spo2()
        spo2 = self.spo2
        if spo2 is not None:
            spo2.log(data)

    def log_vitals(self, data: VitalsData) -> None:
        if self.vitals is None:
            self.open_vitals()
        vitals = self.vitals
        if vitals is not None:
            vitals.log(data)

    def log_glucose(self, data: GlucoseData) -> None:
        if self.glucose is None:
            self.open_glucose()
        glucose = self.glucose
        if glucose is not None:
            glucose.log(data)

    def log_glucose_sample(self, data: GlucoseSampleData) -> None:
        if self.glucose is None:
            self.open_glucose()
        glucose = self.glucose
        if glucose is not None:
            glucose.log_sample(data)

    def log_ppg(self, sample: PpgSample) -> None:
        if self.ppg is None:
            self.open_ppg_stream()
        ppg = self.ppg
        if ppg is not None:
            ppg.log(sample)

    def log_temperature(self, data: TemperatureData) -> None:
        if self.temperature is None:
            self.open_temperature()
        temp = self.temperature
        if temp is not None:
            temp.log(data)

    @property
    def log_dir(self) -> Path:
        return LOG_ROOT
