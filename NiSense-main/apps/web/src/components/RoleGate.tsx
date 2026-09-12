import { Alert } from "@mui/material";
import type { ReactNode } from "react";
import { useAuth } from "../auth/AuthContext";

/** Renders children only if the user has one of `roles`; else an access notice.
 *  RBAC is enforced authoritatively server-side; this is UX gating. */
export default function RoleGate({
  roles,
  children,
}: {
  roles: string[];
  children: ReactNode;
}) {
  const { hasRole } = useAuth();
  if (roles.length > 0 && !hasRole(roles)) {
    return <Alert severity="warning">You do not have access to this section.</Alert>;
  }
  return <>{children}</>;
}
