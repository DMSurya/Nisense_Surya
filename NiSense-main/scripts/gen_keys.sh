#!/bin/bash
# =============================================================================
# MCUBoot RSA-2048 Key Generation Script
# =============================================================================
# Generates RSA-2048 private and public keys for MCUBoot image signing
# Usage: chmod +x scripts/gen_keys.sh && ./scripts/gen_keys.sh
# =============================================================================

set -e

KEY_DIR="mcuboot"
PRIVATE_KEY="${KEY_DIR}/root-rsa-2048.key"
PUBLIC_KEY="${KEY_DIR}/root-rsa-2048.pub"
PEM_KEY="${KEY_DIR}/root-rsa-2048.pem"

# Create mcuboot directory if it doesn't exist
mkdir -p "${KEY_DIR}"

# Check if keys already exist
if [ -f "${PRIVATE_KEY}" ] || [ -f "${PUBLIC_KEY}" ] || [ -f "${PEM_KEY}" ]; then
    echo "⚠️  Keys already exist. Skipping generation."
    echo "   Private key: ${PRIVATE_KEY}"
    echo "   Public key:  ${PUBLIC_KEY}"
    echo "   PEM key:     ${PEM_KEY}"
    exit 0
fi

# Check if imgtool is available
if ! command -v imgtool &> /dev/null; then
    echo "❌ Error: imgtool not found."
    echo "   Install MCUBoot tools: pip install imgtool"
    exit 1
fi

echo "🔑 Generating RSA-2048 keys for MCUBoot..."

# Generate RSA-2048 key pair using imgtool
imgtool keygen \
    --type rsa-2048 \
    --out "${PEM_KEY}"

# Extract public key
imgtool getpub -k "${PEM_KEY}" > "${PUBLIC_KEY}"

# Create private key copy for signing (same as PEM, just different extension)
cp "${PEM_KEY}" "${PRIVATE_KEY}"

# Secure the private key (read-only for owner)
chmod 600 "${PRIVATE_KEY}"
chmod 644 "${PUBLIC_KEY}"
chmod 644 "${PEM_KEY}"

echo "✅ Keys generated successfully!"
echo ""
echo "   Private key (SECURE): ${PRIVATE_KEY}"
echo "   Public key:           ${PUBLIC_KEY}"
echo "   PEM key:              ${PEM_KEY}"
echo ""
echo "⚠️  SECURITY WARNING:"
echo "   - Never commit ${PRIVATE_KEY} or ${PEM_KEY} to version control!"
echo "   - Store private keys securely (separate from code repository)"
echo "   - Use ${PUBLIC_KEY} for verification in production builds"
echo ""
echo "✅ Key generation complete."
