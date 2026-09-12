/* =============================================================================
 * Glucose Model Signing Public Key (Ed25519)
 * =============================================================================
 * 32-byte Ed25519 public key used to verify BLE glucose-model updates when
 * CONFIG_GLUCOSE_MODEL_SIGNED=y. The matching private key is generated and kept
 * ONLY on the provisioning server (see apps/server: nisense-sign / model
 * signing service). Signatures are produced there and never leave signed.
 *
 * PLACEHOLDER: regenerate for production with
 *   apps/server/scripts/gen_model_key.sh   (writes this header + private key)
 * ============================================================================= */

#ifndef GLUCOSE_MODEL_PUBKEY_H
#define GLUCOSE_MODEL_PUBKEY_H

#include <stdint.h>

/* All-zero placeholder key. Signature verification will fail until a real key
 * is provisioned; keep CONFIG_GLUCOSE_MODEL_SIGNED=n until then. */
static const uint8_t GLUCOSE_MODEL_PUBKEY[32] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#endif /* GLUCOSE_MODEL_PUBKEY_H */
