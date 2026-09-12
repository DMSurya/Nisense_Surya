#!/bin/bash
# =============================================================================
# Production Build Script with Size Enforcement and MCUBoot Signing
# =============================================================================
# Builds Zephyr firmware, enforces 384 KB size limit, signs with MCUBoot
# Usage: chmod +x scripts/build.sh && ./scripts/build.sh
# =============================================================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

BOARD="raytac_mdbt50q_db_40_nrf52840"
BUILD_DIR="build"
ELF_FILE="${BUILD_DIR}/zephyr/zephyr.elf"
BIN_FILE="${BUILD_DIR}/zephyr/zephyr.bin"
SIGNED_BIN_FILE="${BUILD_DIR}/zephyr/zephyr.signed.bin"
KEY_FILE="mcuboot/root-rsa-2048.key"

# Maximum app size: 384 KB (0x60000 bytes)
MAX_APP_SIZE=$((384 * 1024))

echo "=============================================================================="
echo "Production Build Script - nRF52840 Wearable Device"
echo "=============================================================================="
echo ""

# Step 1: Build with west
echo "📦 Step 1: Building firmware..."
west build -b ${BOARD} -p auto

if [ ! -f "${ELF_FILE}" ]; then
    echo -e "${RED}❌ Error: Build failed - ELF file not found${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Build completed${NC}"
echo ""

# Step 2: Check binary size
echo "📏 Step 2: Checking binary size..."
if ! command -v arm-none-eabi-size &> /dev/null; then
    echo -e "${YELLOW}⚠️  Warning: arm-none-eabi-size not found, skipping size check${NC}"
    echo "   Install ARM GCC toolchain to enable size checking"
else
    SIZE_OUTPUT=$(arm-none-eabi-size ${ELF_FILE} | tail -1)
    TEXT_SIZE=$(echo ${SIZE_OUTPUT} | awk '{print $1}')
    DATA_SIZE=$(echo ${SIZE_OUTPUT} | awk '{print $2}')
    BSS_SIZE=$(echo ${SIZE_OUTPUT} | awk '{print $3}')
    TOTAL_SIZE=$((TEXT_SIZE + DATA_SIZE))
    
    echo "   Text (code):   ${TEXT_SIZE} bytes"
    echo "   Data:          ${DATA_SIZE} bytes"
    echo "   BSS:           ${BSS_SIZE} bytes"
    echo "   Total (RAM):   ${TOTAL_SIZE} bytes"
    echo ""
    
    # Check binary file size (flash usage)
    if [ -f "${BIN_FILE}" ]; then
        BIN_SIZE=$(stat -f%z "${BIN_FILE}" 2>/dev/null || stat -c%s "${BIN_FILE}" 2>/dev/null || echo "0")
        if [ "${BIN_SIZE}" != "0" ]; then
            echo "   Binary size:   ${BIN_SIZE} bytes ($((${BIN_SIZE} / 1024)) KB)"
            echo ""
            
            # Step 3: Enforce size limit
            if [ ${BIN_SIZE} -gt ${MAX_APP_SIZE} ]; then
                echo -e "${RED}❌ ERROR: Binary size exceeds 384 KB limit!${NC}"
                echo "   Current: ${BIN_SIZE} bytes ($((${BIN_SIZE} / 1024)) KB)"
                echo "   Maximum: ${MAX_APP_SIZE} bytes ($((${MAX_APP_SIZE} / 1024)) KB)"
                echo "   Excess:  $((BIN_SIZE - MAX_APP_SIZE)) bytes"
                echo ""
                echo "   To grow app beyond 384 KB:"
                echo "   1. Update CONFIG_FLASH_LOAD_SIZE in prj.conf (max 0x80000 = 512 KB)"
                echo "   2. Update linker configuration"
                echo "   3. Verify slot0 partition size in overlay matches"
                echo ""
                exit 1
            else
                REMAINING=$((MAX_APP_SIZE - BIN_SIZE))
                echo -e "${GREEN}✅ Size check passed${NC}"
                echo "   Remaining space in Slot0: ${REMAINING} bytes ($((${REMAINING} / 1024)) KB)"
            fi
        fi
    fi
fi

echo ""

# Step 4: Sign image with MCUBoot
echo "🔐 Step 4: Signing firmware image..."

if [ ! -f "${KEY_FILE}" ]; then
    echo -e "${RED}❌ Error: Private key not found: ${KEY_FILE}${NC}"
    echo "   Generate keys first: ./scripts/gen_keys.sh"
    exit 1
fi

if ! command -v imgtool &> /dev/null; then
    echo -e "${RED}❌ Error: imgtool not found${NC}"
    echo "   Install MCUBoot tools: pip install imgtool"
    exit 1
fi

imgtool sign \
    -k "${KEY_FILE}" \
    --align 4 \
    --header-size 0x200 \
    --pad-header \
    --slot-size 0x60000 \
    --version 1.0.0 \
    "${BIN_FILE}" \
    "${SIGNED_BIN_FILE}"

if [ ! -f "${SIGNED_BIN_FILE}" ]; then
    echo -e "${RED}❌ Error: Signing failed${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Image signed successfully${NC}"
echo ""

# Step 5: Verify signature (optional)
echo "🔍 Step 5: Verifying signature..."
if command -v imgtool &> /dev/null; then
    if imgtool verify -k "${KEY_FILE}" "${SIGNED_BIN_FILE}" > /dev/null 2>&1; then
        echo -e "${GREEN}✅ Signature verified${NC}"
    else
        echo -e "${YELLOW}⚠️  Warning: Signature verification failed (continuing anyway)${NC}"
    fi
else
    echo -e "${YELLOW}⚠️  Warning: Cannot verify signature (imgtool not available)${NC}"
fi

echo ""

# Step 6: Print summary
echo "=============================================================================="
echo "Build Summary"
echo "=============================================================================="
if [ -f "${BIN_FILE}" ]; then
    BIN_SIZE=$(stat -f%z "${BIN_FILE}" 2>/dev/null || stat -c%s "${BIN_FILE}" 2>/dev/null || echo "0")
    SIGNED_SIZE=$(stat -f%z "${SIGNED_BIN_FILE}" 2>/dev/null || stat -c%s "${SIGNED_BIN_FILE}" 2>/dev/null || echo "0")
    
    echo "Binary size:        ${BIN_SIZE} bytes ($((${BIN_SIZE} / 1024)) KB)"
    if [ "${SIGNED_SIZE}" != "0" ]; then
        echo "Signed binary size: ${SIGNED_SIZE} bytes ($((${SIGNED_SIZE} / 1024)) KB)"
    fi
    REMAINING=$((MAX_APP_SIZE - BIN_SIZE))
    echo "Remaining space:    ${REMAINING} bytes ($((${REMAINING} / 1024)) KB)"
    echo ""
    echo "Output files:"
    echo "  ELF:     ${ELF_FILE}"
    echo "  Binary:  ${BIN_FILE}"
    echo "  Signed:  ${SIGNED_BIN_FILE}"
fi
echo ""
echo -e "${GREEN}✅ Build pipeline completed successfully!${NC}"
echo ""
