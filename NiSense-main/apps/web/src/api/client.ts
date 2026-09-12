import axios from "axios";

// Base URL: same-origin /api/v1 by default (Vite proxy in dev, Caddy in prod).
const baseURL = import.meta.env.VITE_API_BASE ?? "/api/v1";

let authToken: string | null = null;
export function setAuthToken(token: string | null) {
  authToken = token;
}

export const api = axios.create({ baseURL });

api.interceptors.request.use((config) => {
  if (authToken) {
    config.headers.Authorization = `Bearer ${authToken}`;
  }
  return config;
});
