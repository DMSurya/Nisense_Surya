import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { api } from "./client";

export interface Device {
  id: string;
  device_code: string;
  device_type?: string;
  hw_id?: string;
  ble_id?: string;
  firmware_version?: string;
  provisioned: boolean;
  created_at: string;
}

export interface Sensor {
  id: string;
  sensor_id: string;
  part_number?: string;
  supplier?: string;
  cost?: number;
  moq?: number;
  currency?: string;
}

export interface User {
  id: string;
  username: string;
  email?: string;
  is_active: boolean;
  roles: string[];
}

export interface Reading {
  id: number;
  ts: string;
  device_id: string;
  type: string;
  value?: number;
  quality?: number;
  extra?: Record<string, unknown>;
}

export interface Artifact {
  id: string;
  kind: string;
  variant?: string;
  version: string;
  filename: string;
  sha256: string;
  size_bytes: number;
  created_at: string;
}

function list<T>(key: string, path: string, params?: Record<string, unknown>) {
  return useQuery({
    queryKey: [key, params],
    queryFn: async () => (await api.get<T[]>(path, { params })).data,
  });
}

// --- Users ---
export const useUsers = () => list<User>("users", "/users");
export function useSaveUser() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (u: Partial<User> & { password?: string }) =>
      (await api.post("/users", u)).data,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["users"] }),
  });
}

// --- Devices ---
export const useDevices = () => list<Device>("devices", "/devices");
export function useSaveDevice() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (d: Partial<Device>) => (await api.post("/devices", d)).data,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["devices"] }),
  });
}
export function useProvisionToken() {
  return useMutation({
    mutationFn: async (deviceId: string) =>
      (await api.post(`/devices/${deviceId}/provision-token`)).data,
  });
}

// --- Sensors ---
export const useSensors = () => list<Sensor>("sensors", "/sensors");
export function useSaveSensor() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (s: Partial<Sensor>) => (await api.post("/sensors", s)).data,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["sensors"] }),
  });
}

// --- Patients & mappings ---
export const usePatients = () => list<{ id: string; nisense_id: string; name?: string }>(
  "patients",
  "/patients",
);
export const useDevicePatientMaps = () =>
  list<Record<string, unknown>>("dpmap", "/mappings/device-patient");
export const useDeviceAlgorithmMaps = () =>
  list<Record<string, unknown>>("damap", "/mappings/device-algorithm");
export function useMapDevicePatient() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (m: Record<string, unknown>) =>
      (await api.post("/mappings/device-patient", m)).data,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["dpmap"] }),
  });
}
export function useMapDeviceAlgorithm() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (m: Record<string, unknown>) =>
      (await api.post("/mappings/device-algorithm", m)).data,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["damap"] }),
  });
}

// --- Artifacts ---
export const useArtifacts = () => list<Artifact>("artifacts", "/artifacts");
export function useUploadArtifact() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (form: FormData) =>
      (await api.post("/artifacts", form, {
        headers: { "Content-Type": "multipart/form-data" },
      })).data,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["artifacts"] }),
  });
}

export function useDeleteArtifact() {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: async (id: string) => {
      await api.delete(`/artifacts/${id}`);
    },
    onSuccess: () => qc.invalidateQueries({ queryKey: ["artifacts"] }),
  });
}

// --- Readings ---
export function useReadings(params: Record<string, unknown>) {
  return list<Reading>("readings", "/readings", params);
}
