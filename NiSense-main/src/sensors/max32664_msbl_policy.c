/**
 * @file max32664_msbl_policy.c
 * @brief Host-side .msbl provenance gate (Level 1 allow-list + Level 2 SHA-256)
 *
 * Runs before bootloader erase. Maxim BL auth remains mandatory; this only
 * decides whether NiSense is willing to start the update.
 */

#include "max32664_update.h"

#include <ctype.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(max32664_msbl_policy, CONFIG_SENSOR_LOG_LEVEL);

#if IS_ENABLED(CONFIG_MAX32664_UPDATE_IMAGE_SHA256)

/* Compact SHA-256 (public-domain style) — avoids pulling mbedTLS/tinycrypt
 * into the app image solely for this gate. */

struct sha256_ctx {
	uint32_t state[8];
	uint64_t bitlen;
	uint8_t data[64];
	uint32_t datalen;
};

static uint32_t rotr32(uint32_t x, uint32_t n)
{
	return (x >> n) | (x << (32U - n));
}

static void sha256_transform(struct sha256_ctx *ctx, const uint8_t data[64])
{
	static const uint32_t k[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
		0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
		0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
		0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
		0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
		0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
		0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
		0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
		0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
		0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
	};
	uint32_t m[64];
	uint32_t a, b, c, d, e, f, g, h;
	uint32_t i;

	for (i = 0; i < 16U; i++) {
		m[i] = sys_get_be32(&data[i * 4U]);
	}
	for (; i < 64U; i++) {
		uint32_t s0 = rotr32(m[i - 15U], 7) ^ rotr32(m[i - 15U], 18) ^
			      (m[i - 15U] >> 3);
		uint32_t s1 = rotr32(m[i - 2U], 17) ^ rotr32(m[i - 2U], 19) ^
			      (m[i - 2U] >> 10);

		m[i] = m[i - 16U] + s0 + m[i - 7U] + s1;
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < 64U; i++) {
		uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
		uint32_t ch = (e & f) ^ ((~e) & g);
		uint32_t t1 = h + S1 + ch + k[i] + m[i];
		uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
		uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
		uint32_t t2 = S0 + maj;

		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
}

static void sha256_init(struct sha256_ctx *ctx)
{
	ctx->datalen = 0;
	ctx->bitlen = 0;
	ctx->state[0] = 0x6a09e667U;
	ctx->state[1] = 0xbb67ae85U;
	ctx->state[2] = 0x3c6ef372U;
	ctx->state[3] = 0xa54ff53aU;
	ctx->state[4] = 0x510e527fU;
	ctx->state[5] = 0x9b05688cU;
	ctx->state[6] = 0x1f83d9abU;
	ctx->state[7] = 0x5be0cd19U;
}

static void sha256_update(struct sha256_ctx *ctx, const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		ctx->data[ctx->datalen++] = data[i];
		if (ctx->datalen == 64U) {
			sha256_transform(ctx, ctx->data);
			ctx->bitlen += 512U;
			ctx->datalen = 0;
		}
	}
}

static void sha256_final(struct sha256_ctx *ctx, uint8_t hash[32])
{
	uint32_t i = ctx->datalen;

	if (ctx->datalen < 56U) {
		ctx->data[i++] = 0x80U;
		while (i < 56U) {
			ctx->data[i++] = 0x00U;
		}
	} else {
		ctx->data[i++] = 0x80U;
		while (i < 64U) {
			ctx->data[i++] = 0x00U;
		}
		sha256_transform(ctx, ctx->data);
		memset(ctx->data, 0, 56);
	}

	ctx->bitlen += (uint64_t)ctx->datalen * 8U;
	sys_put_be64(ctx->bitlen, &ctx->data[56]);
	sha256_transform(ctx, ctx->data);

	for (i = 0; i < 8U; i++) {
		sys_put_be32(ctx->state[i], &hash[i * 4U]);
	}
}

/* ADI pack MAX32664C_MAX86141 … 30.13.31 — C- and Z-keyed .msbl */
static const uint8_t approved_sha256[][32] = {
	{ /* …_C_30.13.31.msbl */
	  0x87, 0x47, 0xd9, 0x6a, 0x4c, 0x20, 0x1d, 0x02,
	  0x8a, 0x78, 0x0c, 0x52, 0x69, 0x17, 0xed, 0xca,
	  0x1e, 0xcd, 0xe8, 0x56, 0x1d, 0x03, 0x21, 0x07,
	  0x68, 0x4a, 0x9e, 0xf9, 0x2b, 0x11, 0x10, 0xc4 },
	{ /* …_Z_30.13.31.msbl */
	  0xf9, 0x18, 0x7f, 0xe1, 0xa4, 0xf7, 0x0f, 0xe6,
	  0xa5, 0x0e, 0x17, 0x8f, 0x39, 0x65, 0xe0, 0x75,
	  0xec, 0xc4, 0x9c, 0x04, 0x24, 0xa4, 0x27, 0xab,
	  0x53, 0x78, 0x6b, 0x5a, 0xe5, 0x72, 0xd2, 0x46 },
};

static int policy_sha256_allow(const uint8_t *msbl, size_t msbl_len)
{
	struct sha256_ctx ctx;
	uint8_t digest[32];

	sha256_init(&ctx);
	sha256_update(&ctx, msbl, msbl_len);
	sha256_final(&ctx, digest);

	for (size_t i = 0; i < ARRAY_SIZE(approved_sha256); i++) {
		if (memcmp(digest, approved_sha256[i], 32) == 0) {
			LOG_INF("MSBL SHA-256 matched approved image [%zu]", i);
			return 0;
		}
	}

	LOG_ERR("MSBL SHA-256 not in allow-list (Level 2)");
	LOG_HEXDUMP_ERR(digest, sizeof(digest), "computed:");
	return -EPERM;
}

#endif /* CONFIG_MAX32664_UPDATE_IMAGE_SHA256 */

static bool name_has_ci(const char *hay, const char *needle)
{
	size_t nlen;
	size_t hlen;

	if (!hay || !needle) {
		return false;
	}
	nlen = strlen(needle);
	hlen = strlen(hay);
	if (nlen == 0U || nlen > hlen) {
		return false;
	}
	for (size_t i = 0; i + nlen <= hlen; i++) {
		size_t j;

		for (j = 0; j < nlen; j++) {
			if (tolower((unsigned char)hay[i + j]) !=
			    tolower((unsigned char)needle[j])) {
				break;
			}
		}
		if (j == nlen) {
			return true;
		}
	}
	return false;
}

static int policy_level1(const char *name_hint, uint8_t ver_major,
			 bool *version_parsed)
{
	*version_parsed = (ver_major != 0U);

	if (name_hint) {
		if (name_has_ci(name_hint, "MAXM86161") ||
		    name_has_ci(name_hint, "MAX32664D") ||
		    name_has_ci(name_hint, "_40.") ||
		    name_has_ci(name_hint, "_32.") ||
		    name_has_ci(name_hint, "_20.")) {
			LOG_ERR("MSBL name rejected (Level 1): incompatible pack '%s'",
				name_hint);
			return -EPERM;
		}

		/* Maxim pack names usually embed the AFE; require MAX86141 when
		 * the name looks like a vendor package (contains MAX32664). */
		if (name_has_ci(name_hint, "MAX32664") &&
		    !name_has_ci(name_hint, "MAX86141") &&
		    !name_has_ci(name_hint, "app_partition")) {
			LOG_ERR("MSBL name rejected (Level 1): expected MAX86141 in '%s'",
				name_hint);
			return -EPERM;
		}
	}

	if (*version_parsed && ver_major != 30U) {
		LOG_ERR("MSBL version major %u rejected (Level 1: allow major 30 only)",
			ver_major);
		return -EPERM;
	}

	return 0;
}

int max32664_msbl_policy_check(const uint8_t *msbl, size_t msbl_len,
			       const char *name_hint, uint8_t ver_major,
			       uint8_t ver_minor, uint8_t ver_patch)
{
#if !IS_ENABLED(CONFIG_MAX32664_UPDATE_IMAGE_ALLOWLIST)
	ARG_UNUSED(msbl);
	ARG_UNUSED(msbl_len);
	ARG_UNUSED(name_hint);
	ARG_UNUSED(ver_major);
	ARG_UNUSED(ver_minor);
	ARG_UNUSED(ver_patch);
	return 0;
#else
	bool version_parsed = false;
	int ret;

	ARG_UNUSED(ver_minor);
	ARG_UNUSED(ver_patch);

	if (msbl == NULL || msbl_len == 0U) {
		return -EINVAL;
	}

	ret = policy_level1(name_hint, ver_major, &version_parsed);
	if (ret) {
		return ret;
	}

#if IS_ENABLED(CONFIG_MAX32664_UPDATE_IMAGE_SHA256)
	ret = policy_sha256_allow(msbl, msbl_len);
	if (ret) {
		return ret;
	}
#else
	if (!version_parsed) {
		LOG_ERR("MSBL has no parseable version and Level 2 SHA-256 is off — refusing");
		return -EPERM;
	}
	LOG_WRN("MSBL Level 2 SHA-256 disabled; Level 1 name/version only");
#endif

	LOG_INF("MSBL provenance OK (Level 1%s)",
#if IS_ENABLED(CONFIG_MAX32664_UPDATE_IMAGE_SHA256)
		"+2"
#else
		""
#endif
	);
	return 0;
#endif /* ALLOWLIST */
}
