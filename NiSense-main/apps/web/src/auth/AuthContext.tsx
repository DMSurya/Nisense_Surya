import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import { UserManager, type UserManagerSettings } from "oidc-client-ts";
import { api, setAuthToken } from "../api/client";

interface AuthState {
  token: string | null;
  username: string | null;
  roles: string[];
  isAuthenticated: boolean;
  hasRole: (roles: string[]) => boolean;
  devLogin: (username: string, roles: string[]) => Promise<void>;
  ssoLogin: () => Promise<void>;
  logout: () => void;
}

const AuthCtx = createContext<AuthState | null>(null);

function decodeRoles(token: string): { username: string | null; roles: string[] } {
  try {
    const payload = JSON.parse(atob(token.split(".")[1] ?? ""));
    const roles = new Set<string>();
    const ra = payload.realm_access;
    if (ra?.roles) (ra.roles as string[]).forEach((r) => roles.add(r));
    if (Array.isArray(payload.roles)) (payload.roles as string[]).forEach((r) => roles.add(r));
    if (Array.isArray(payload["cognito:groups"]))
      (payload["cognito:groups"] as string[]).forEach((r) => roles.add(r));
    return {
      username: payload.preferred_username ?? payload.email ?? payload.sub ?? null,
      roles: [...roles],
    };
  } catch {
    return { username: null, roles: [] };
  }
}

function makeOidcSettings(): UserManagerSettings {
  const authority =
    import.meta.env.VITE_OIDC_AUTHORITY ??
    `${window.location.origin}/auth/realms/nisense`;
  return {
    authority,
    client_id: import.meta.env.VITE_OIDC_CLIENT_ID ?? "nisense-web",
    redirect_uri: `${window.location.origin}/`,
    post_logout_redirect_uri: `${window.location.origin}/`,
    response_type: "code",
    scope: "openid profile email",
    automaticSilentRenew: false,
  };
}

export function AuthProvider({ children }: { children: ReactNode }) {
  const [token, setToken] = useState<string | null>(
    () => localStorage.getItem("nisense_token"),
  );

  const { username, roles } = useMemo(
    () => (token ? decodeRoles(token) : { username: null, roles: [] }),
    [token],
  );

  useEffect(() => {
    setAuthToken(token);
    if (token) localStorage.setItem("nisense_token", token);
    else localStorage.removeItem("nisense_token");
  }, [token]);

  // Complete OIDC redirect callback when Keycloak returns with ?code=
  useEffect(() => {
    if (!window.location.search.includes("code=")) return;
    let cancelled = false;
    (async () => {
      try {
        const mgr = new UserManager(makeOidcSettings());
        const user = await mgr.signinRedirectCallback();
        if (!cancelled && user.access_token) {
          setToken(user.access_token);
          window.history.replaceState({}, "", "/");
        }
      } catch (err) {
        console.error("OIDC callback failed", err);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  const devLogin = useCallback(async (user: string, wantRoles: string[]) => {
    const res = await api.post("/auth/dev-token", { username: user, roles: wantRoles });
    setToken(res.data.access_token as string);
  }, []);

  const ssoLogin = useCallback(async () => {
    const mgr = new UserManager(makeOidcSettings());
    await mgr.signinRedirect();
  }, []);

  const logout = useCallback(() => {
    setToken(null);
    // Best-effort Keycloak end-session (public client, no silent renew).
    try {
      const mgr = new UserManager(makeOidcSettings());
      void mgr.signoutRedirect();
    } catch {
      /* ignore */
    }
  }, []);

  const hasRole = useCallback(
    (want: string[]) => roles.includes("super_admin") || want.some((r) => roles.includes(r)),
    [roles],
  );

  const value: AuthState = {
    token,
    username,
    roles,
    isAuthenticated: !!token,
    hasRole,
    devLogin,
    ssoLogin,
    logout,
  };

  return <AuthCtx.Provider value={value}>{children}</AuthCtx.Provider>;
}

export function useAuth(): AuthState {
  const ctx = useContext(AuthCtx);
  if (!ctx) throw new Error("useAuth must be used within AuthProvider");
  return ctx;
}
