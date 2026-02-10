



/*
	Crypto - 加密算法模块 [Ver1.0]
	
	基于 mongoose 内建 TLS 的加密原语移植
	提供 SHA-256, HMAC-SHA256, ChaCha20-Poly1305, AES-128-GCM, X25519, HKDF
	
	算法来源与授权：
		SHA-256:            Brad Conte (Public Domain)
		ChaCha20-Poly1305:  portable8439 (CC0-1.0) + poly1305-donna (Public Domain)
		AES-128-GCM:        Steven M. Gibson / GRC.com (Public Domain)
		X25519:             Mike Hamburg / STROBE (MIT License)
*/



/* ============================== SHA-256 ============================== */

// xsha256_ctx 已在 xrt.h 中定义

// SHA-256 常量表
static const uint32 __xrt_sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define __XRT_SHA256_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define __XRT_SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define __XRT_SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define __XRT_SHA256_EP0(x) (__XRT_SHA256_ROR(x, 2) ^ __XRT_SHA256_ROR(x, 13) ^ __XRT_SHA256_ROR(x, 22))
#define __XRT_SHA256_EP1(x) (__XRT_SHA256_ROR(x, 6) ^ __XRT_SHA256_ROR(x, 11) ^ __XRT_SHA256_ROR(x, 25))
#define __XRT_SHA256_SIG0(x) (__XRT_SHA256_ROR(x, 7) ^ __XRT_SHA256_ROR(x, 18) ^ ((x) >> 3))
#define __XRT_SHA256_SIG1(x) (__XRT_SHA256_ROR(x, 17) ^ __XRT_SHA256_ROR(x, 19) ^ ((x) >> 10))

static void __xrt_sha256_chunk(xsha256_ctx *pCtx)
{
	int i, j;
	uint32 a, b, c, d, e, f, g, h;
	uint32 m[64];
	
	for ( i = 0, j = 0; i < 16; ++i, j += 4 ) {
		m[i] = ((uint32)pCtx->buffer[j] << 24) |
		       ((uint32)pCtx->buffer[j + 1] << 16) |
		       ((uint32)pCtx->buffer[j + 2] << 8) |
		       ((uint32)pCtx->buffer[j + 3]);
	}
	for ( ; i < 64; ++i ) {
		m[i] = __XRT_SHA256_SIG1(m[i - 2]) + m[i - 7] + __XRT_SHA256_SIG0(m[i - 15]) + m[i - 16];
	}
	
	a = pCtx->state[0];
	b = pCtx->state[1];
	c = pCtx->state[2];
	d = pCtx->state[3];
	e = pCtx->state[4];
	f = pCtx->state[5];
	g = pCtx->state[6];
	h = pCtx->state[7];
	
	for ( i = 0; i < 64; ++i ) {
		uint32 t1 = h + __XRT_SHA256_EP1(e) + __XRT_SHA256_CH(e, f, g) + __xrt_sha256_k[i] + m[i];
		uint32 t2 = __XRT_SHA256_EP0(a) + __XRT_SHA256_MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	
	pCtx->state[0] += a;
	pCtx->state[1] += b;
	pCtx->state[2] += c;
	pCtx->state[3] += d;
	pCtx->state[4] += e;
	pCtx->state[5] += f;
	pCtx->state[6] += g;
	pCtx->state[7] += h;
}

XXAPI void xrtSHA256Init(xsha256_ctx *pCtx)
{
	pCtx->len = 0;
	pCtx->bits = 0;
	pCtx->state[0] = 0x6a09e667;
	pCtx->state[1] = 0xbb67ae85;
	pCtx->state[2] = 0x3c6ef372;
	pCtx->state[3] = 0xa54ff53a;
	pCtx->state[4] = 0x510e527f;
	pCtx->state[5] = 0x9b05688c;
	pCtx->state[6] = 0x1f83d9ab;
	pCtx->state[7] = 0x5be0cd19;
}

XXAPI void xrtSHA256Update(xsha256_ctx *pCtx, const ptr pData, size_t iLen)
{
	const uint8 *pBytes = (const uint8 *)pData;
	size_t i;
	for ( i = 0; i < iLen; i++ ) {
		pCtx->buffer[pCtx->len] = pBytes[i];
		if ( (++(pCtx->len)) == 64 ) {
			__xrt_sha256_chunk(pCtx);
			pCtx->bits += 512;
			pCtx->len = 0;
		}
	}
}

XXAPI void xrtSHA256Final(xsha256_ctx *pCtx, uint8 *pOut)
{
	uint32 i = pCtx->len;
	
	if ( i < 56 ) {
		pCtx->buffer[i++] = 0x80;
		while ( i < 56 ) {
			pCtx->buffer[i++] = 0x00;
		}
	} else {
		pCtx->buffer[i++] = 0x80;
		while ( i < 64 ) {
			pCtx->buffer[i++] = 0x00;
		}
		__xrt_sha256_chunk(pCtx);
		memset(pCtx->buffer, 0, 56);
	}
	
	pCtx->bits += pCtx->len * 8;
	pCtx->buffer[63] = (uint8)((pCtx->bits) & 0xff);
	pCtx->buffer[62] = (uint8)((pCtx->bits >> 8) & 0xff);
	pCtx->buffer[61] = (uint8)((pCtx->bits >> 16) & 0xff);
	pCtx->buffer[60] = (uint8)((pCtx->bits >> 24) & 0xff);
	pCtx->buffer[59] = (uint8)((pCtx->bits >> 32) & 0xff);
	pCtx->buffer[58] = (uint8)((pCtx->bits >> 40) & 0xff);
	pCtx->buffer[57] = (uint8)((pCtx->bits >> 48) & 0xff);
	pCtx->buffer[56] = (uint8)((pCtx->bits >> 56) & 0xff);
	__xrt_sha256_chunk(pCtx);
	
	for ( i = 0; i < 4; ++i ) {
		pOut[i]      = (uint8)((pCtx->state[0] >> (24 - i * 8)) & 0xff);
		pOut[i + 4]  = (uint8)((pCtx->state[1] >> (24 - i * 8)) & 0xff);
		pOut[i + 8]  = (uint8)((pCtx->state[2] >> (24 - i * 8)) & 0xff);
		pOut[i + 12] = (uint8)((pCtx->state[3] >> (24 - i * 8)) & 0xff);
		pOut[i + 16] = (uint8)((pCtx->state[4] >> (24 - i * 8)) & 0xff);
		pOut[i + 20] = (uint8)((pCtx->state[5] >> (24 - i * 8)) & 0xff);
		pOut[i + 24] = (uint8)((pCtx->state[6] >> (24 - i * 8)) & 0xff);
		pOut[i + 28] = (uint8)((pCtx->state[7] >> (24 - i * 8)) & 0xff);
	}
}

XXAPI void xrtSHA256(const ptr pData, size_t iLen, uint8 *pOut)
{
	xsha256_ctx tCtx;
	xrtSHA256Init(&tCtx);
	xrtSHA256Update(&tCtx, pData, iLen);
	xrtSHA256Final(&tCtx, pOut);
}

XXAPI void xrtHMAC_SHA256(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut)
{
	xsha256_ctx tCtx;
	uint8 k[64] = {0};
	uint8 o_pad[64], i_pad[64];
	unsigned int i;
	
	memset(i_pad, 0x36, sizeof(i_pad));
	memset(o_pad, 0x5c, sizeof(o_pad));
	
	if ( iKeyLen <= 64 ) {
		if ( iKeyLen > 0 ) {
			memmove(k, pKey, iKeyLen);
		}
	} else {
		xrtSHA256Init(&tCtx);
		xrtSHA256Update(&tCtx, (ptr)pKey, iKeyLen);
		xrtSHA256Final(&tCtx, k);
	}
	
	for ( i = 0; i < sizeof(k); i++ ) {
		i_pad[i] ^= k[i];
		o_pad[i] ^= k[i];
	}
	
	xrtSHA256Init(&tCtx);
	xrtSHA256Update(&tCtx, i_pad, sizeof(i_pad));
	xrtSHA256Update(&tCtx, (ptr)pMsg, iMsgLen);
	xrtSHA256Final(&tCtx, pOut);
	
	xrtSHA256Init(&tCtx);
	xrtSHA256Update(&tCtx, o_pad, sizeof(o_pad));
	xrtSHA256Update(&tCtx, pOut, 32);
	xrtSHA256Final(&tCtx, pOut);
}



/* ============================== SHA-512 / SHA-384 ============================== */

// xsha512_ctx 已在 xrt.h 中定义 (同时用作 SHA-384 上下文)

// SHA-512 常量表 (80 个 uint64)
static const uint64 __xrt_sha512_k[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

#define __XRT_SHA512_ROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define __XRT_SHA512_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define __XRT_SHA512_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define __XRT_SHA512_EP0(x) (__XRT_SHA512_ROR64(x, 28) ^ __XRT_SHA512_ROR64(x, 34) ^ __XRT_SHA512_ROR64(x, 39))
#define __XRT_SHA512_EP1(x) (__XRT_SHA512_ROR64(x, 14) ^ __XRT_SHA512_ROR64(x, 18) ^ __XRT_SHA512_ROR64(x, 41))
#define __XRT_SHA512_SIG0(x) (__XRT_SHA512_ROR64(x, 1) ^ __XRT_SHA512_ROR64(x, 8) ^ ((x) >> 7))
#define __XRT_SHA512_SIG1(x) (__XRT_SHA512_ROR64(x, 19) ^ __XRT_SHA512_ROR64(x, 61) ^ ((x) >> 6))

static void __xrt_sha512_chunk(xsha512_ctx *pCtx)
{
	int i, j;
	uint64 a, b, c, d, e, f, g, h;
	uint64 m[80];
	
	for ( i = 0, j = 0; i < 16; ++i, j += 8 ) {
		m[i] = ((uint64)pCtx->buffer[j] << 56) |
		       ((uint64)pCtx->buffer[j + 1] << 48) |
		       ((uint64)pCtx->buffer[j + 2] << 40) |
		       ((uint64)pCtx->buffer[j + 3] << 32) |
		       ((uint64)pCtx->buffer[j + 4] << 24) |
		       ((uint64)pCtx->buffer[j + 5] << 16) |
		       ((uint64)pCtx->buffer[j + 6] << 8) |
		       ((uint64)pCtx->buffer[j + 7]);
	}
	for ( ; i < 80; ++i ) {
		m[i] = __XRT_SHA512_SIG1(m[i - 2]) + m[i - 7] + __XRT_SHA512_SIG0(m[i - 15]) + m[i - 16];
	}
	
	a = pCtx->state[0];
	b = pCtx->state[1];
	c = pCtx->state[2];
	d = pCtx->state[3];
	e = pCtx->state[4];
	f = pCtx->state[5];
	g = pCtx->state[6];
	h = pCtx->state[7];
	
	for ( i = 0; i < 80; ++i ) {
		uint64 t1 = h + __XRT_SHA512_EP1(e) + __XRT_SHA512_CH(e, f, g) + __xrt_sha512_k[i] + m[i];
		uint64 t2 = __XRT_SHA512_EP0(a) + __XRT_SHA512_MAJ(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	
	pCtx->state[0] += a;
	pCtx->state[1] += b;
	pCtx->state[2] += c;
	pCtx->state[3] += d;
	pCtx->state[4] += e;
	pCtx->state[5] += f;
	pCtx->state[6] += g;
	pCtx->state[7] += h;
}

XXAPI void xrtSHA512Init(xsha512_ctx *pCtx)
{
	pCtx->len = 0;
	pCtx->bits = 0;
	pCtx->state[0] = 0x6a09e667f3bcc908ULL;
	pCtx->state[1] = 0xbb67ae8584caa73bULL;
	pCtx->state[2] = 0x3c6ef372fe94f82bULL;
	pCtx->state[3] = 0xa54ff53a5f1d36f1ULL;
	pCtx->state[4] = 0x510e527fade682d1ULL;
	pCtx->state[5] = 0x9b05688c2b3e6c1fULL;
	pCtx->state[6] = 0x1f83d9abfb41bd6bULL;
	pCtx->state[7] = 0x5be0cd19137e2179ULL;
}

XXAPI void xrtSHA384Init(xsha512_ctx *pCtx)
{
	pCtx->len = 0;
	pCtx->bits = 0;
	pCtx->state[0] = 0xcbbb9d5dc1059ed8ULL;
	pCtx->state[1] = 0x629a292a367cd507ULL;
	pCtx->state[2] = 0x9159015a3070dd17ULL;
	pCtx->state[3] = 0x152fecd8f70e5939ULL;
	pCtx->state[4] = 0x67332667ffc00b31ULL;
	pCtx->state[5] = 0x8eb44a8768581511ULL;
	pCtx->state[6] = 0xdb0c2e0d64f98fa7ULL;
	pCtx->state[7] = 0x47b5481dbefa4fa4ULL;
}

XXAPI void xrtSHA512Update(xsha512_ctx *pCtx, const ptr pData, size_t iLen)
{
	const uint8 *pBytes = (const uint8 *)pData;
	size_t i;
	for ( i = 0; i < iLen; i++ ) {
		pCtx->buffer[pCtx->len] = pBytes[i];
		if ( (++(pCtx->len)) == 128 ) {
			__xrt_sha512_chunk(pCtx);
			pCtx->bits += 1024;
			pCtx->len = 0;
		}
	}
}

XXAPI void xrtSHA512Final(xsha512_ctx *pCtx, uint8 *pOut)
{
	uint32 i = pCtx->len;
	
	if ( i < 112 ) {
		pCtx->buffer[i++] = 0x80;
		while ( i < 112 ) {
			pCtx->buffer[i++] = 0x00;
		}
	} else {
		pCtx->buffer[i++] = 0x80;
		while ( i < 128 ) {
			pCtx->buffer[i++] = 0x00;
		}
		__xrt_sha512_chunk(pCtx);
		memset(pCtx->buffer, 0, 112);
	}
	
	pCtx->bits += pCtx->len * 8;
	// SHA-512 使用 128-bit 长度字段, 高 64 位为 0 (我们不处理超大消息)
	memset(pCtx->buffer + 112, 0, 8);
	pCtx->buffer[127] = (uint8)((pCtx->bits) & 0xff);
	pCtx->buffer[126] = (uint8)((pCtx->bits >> 8) & 0xff);
	pCtx->buffer[125] = (uint8)((pCtx->bits >> 16) & 0xff);
	pCtx->buffer[124] = (uint8)((pCtx->bits >> 24) & 0xff);
	pCtx->buffer[123] = (uint8)((pCtx->bits >> 32) & 0xff);
	pCtx->buffer[122] = (uint8)((pCtx->bits >> 40) & 0xff);
	pCtx->buffer[121] = (uint8)((pCtx->bits >> 48) & 0xff);
	pCtx->buffer[120] = (uint8)((pCtx->bits >> 56) & 0xff);
	__xrt_sha512_chunk(pCtx);
	
	for ( i = 0; i < 8; ++i ) {
		pOut[i * 8]     = (uint8)(pCtx->state[i] >> 56);
		pOut[i * 8 + 1] = (uint8)(pCtx->state[i] >> 48);
		pOut[i * 8 + 2] = (uint8)(pCtx->state[i] >> 40);
		pOut[i * 8 + 3] = (uint8)(pCtx->state[i] >> 32);
		pOut[i * 8 + 4] = (uint8)(pCtx->state[i] >> 24);
		pOut[i * 8 + 5] = (uint8)(pCtx->state[i] >> 16);
		pOut[i * 8 + 6] = (uint8)(pCtx->state[i] >> 8);
		pOut[i * 8 + 7] = (uint8)(pCtx->state[i]);
	}
}

XXAPI void xrtSHA384Final(xsha512_ctx *pCtx, uint8 *pOut)
{
	uint8 aFull[64];
	xrtSHA512Final(pCtx, aFull);
	memcpy(pOut, aFull, 48);  // SHA-384 截取前 48 字节
}

XXAPI void xrtSHA512(const ptr pData, size_t iLen, uint8 *pOut)
{
	xsha512_ctx tCtx;
	xrtSHA512Init(&tCtx);
	xrtSHA512Update(&tCtx, pData, iLen);
	xrtSHA512Final(&tCtx, pOut);
}

XXAPI void xrtSHA384(const ptr pData, size_t iLen, uint8 *pOut)
{
	xsha512_ctx tCtx;
	xrtSHA384Init(&tCtx);
	xrtSHA512Update(&tCtx, pData, iLen);
	xrtSHA384Final(&tCtx, pOut);
}

XXAPI void xrtHMAC_SHA384(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut)
{
	xsha512_ctx tCtx;
	uint8 k[128] = {0};
	uint8 o_pad[128], i_pad[128];
	unsigned int i;
	
	memset(i_pad, 0x36, sizeof(i_pad));
	memset(o_pad, 0x5c, sizeof(o_pad));
	
	if ( iKeyLen <= 128 ) {
		if ( iKeyLen > 0 ) {
			memmove(k, pKey, iKeyLen);
		}
	} else {
		xrtSHA384Init(&tCtx);
		xrtSHA512Update(&tCtx, (ptr)pKey, iKeyLen);
		xrtSHA384Final(&tCtx, k);
	}
	
	for ( i = 0; i < sizeof(k); i++ ) {
		i_pad[i] ^= k[i];
		o_pad[i] ^= k[i];
	}
	
	xrtSHA384Init(&tCtx);
	xrtSHA512Update(&tCtx, i_pad, sizeof(i_pad));
	xrtSHA512Update(&tCtx, (ptr)pMsg, iMsgLen);
	xrtSHA384Final(&tCtx, pOut);
	
	xrtSHA384Init(&tCtx);
	xrtSHA512Update(&tCtx, o_pad, sizeof(o_pad));
	xrtSHA512Update(&tCtx, pOut, 48);
	xrtSHA384Final(&tCtx, pOut);
}

XXAPI void xrtHMAC_SHA512(const uint8 *pKey, size_t iKeyLen, const uint8 *pMsg, size_t iMsgLen, uint8 *pOut)
{
	xsha512_ctx tCtx;
	uint8 k[128] = {0};
	uint8 o_pad[128], i_pad[128];
	unsigned int i;
	
	memset(i_pad, 0x36, sizeof(i_pad));
	memset(o_pad, 0x5c, sizeof(o_pad));
	
	if ( iKeyLen <= 128 ) {
		if ( iKeyLen > 0 ) {
			memmove(k, pKey, iKeyLen);
		}
	} else {
		xrtSHA512Init(&tCtx);
		xrtSHA512Update(&tCtx, (ptr)pKey, iKeyLen);
		xrtSHA512Final(&tCtx, k);
	}
	
	for ( i = 0; i < sizeof(k); i++ ) {
		i_pad[i] ^= k[i];
		o_pad[i] ^= k[i];
	}
	
	xrtSHA512Init(&tCtx);
	xrtSHA512Update(&tCtx, i_pad, sizeof(i_pad));
	xrtSHA512Update(&tCtx, (ptr)pMsg, iMsgLen);
	xrtSHA512Final(&tCtx, pOut);
	
	xrtSHA512Init(&tCtx);
	xrtSHA512Update(&tCtx, o_pad, sizeof(o_pad));
	xrtSHA512Update(&tCtx, pOut, 64);
	xrtSHA512Final(&tCtx, pOut);
}



/* ============================== ChaCha20-Poly1305 (RFC 8439) ============================== */

#define __XRT_CHACHA20_KEY_SIZE 32
#define __XRT_CHACHA20_NONCE_SIZE 12
#define __XRT_CHACHA20_BLOCK_SIZE 64
#define __XRT_CHACHA20_STATE_WORDS 16

#define __XRT_ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static inline uint32 __xrt_load32_le(const uint8 *p)
{
	return ((uint32)p[0]) | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

static inline void __xrt_store32_le(uint8 *p, uint32 v)
{
	p[0] = (uint8)(v & 0xff);
	p[1] = (uint8)((v >> 8) & 0xff);
	p[2] = (uint8)((v >> 16) & 0xff);
	p[3] = (uint8)((v >> 24) & 0xff);
}

static inline void __xrt_store64_le(uint8 *p, uint64 v)
{
	__xrt_store32_le(p, (uint32)v);
	__xrt_store32_le(p + 4, (uint32)(v >> 32));
}

#define __XRT_QR(a, b, c, d) \
	a += b; d ^= a; d = __XRT_ROTL32(d, 16); \
	c += d; b ^= c; b = __XRT_ROTL32(b, 12); \
	a += b; d ^= a; d = __XRT_ROTL32(d, 8); \
	c += d; b ^= c; b = __XRT_ROTL32(b, 7);

static void __xrt_chacha20_block(const uint32 *pState, uint32 *pOut)
{
	int i;
	uint32 s0 = pState[0], s1 = pState[1], s2 = pState[2], s3 = pState[3];
	uint32 s4 = pState[4], s5 = pState[5], s6 = pState[6], s7 = pState[7];
	uint32 s8 = pState[8], s9 = pState[9], s10 = pState[10], s11 = pState[11];
	uint32 s12 = pState[12], s13 = pState[13], s14 = pState[14], s15 = pState[15];
	
	for ( i = 0; i < 10; i++ ) {
		__XRT_QR(s0, s4, s8, s12);
		__XRT_QR(s1, s5, s9, s13);
		__XRT_QR(s2, s6, s10, s14);
		__XRT_QR(s3, s7, s11, s15);
		__XRT_QR(s0, s5, s10, s15);
		__XRT_QR(s1, s6, s11, s12);
		__XRT_QR(s2, s7, s8, s13);
		__XRT_QR(s3, s4, s9, s14);
	}
	
	pOut[0] = pState[0] + s0;
	pOut[1] = pState[1] + s1;
	pOut[2] = pState[2] + s2;
	pOut[3] = pState[3] + s3;
	pOut[4] = pState[4] + s4;
	pOut[5] = pState[5] + s5;
	pOut[6] = pState[6] + s6;
	pOut[7] = pState[7] + s7;
	pOut[8] = pState[8] + s8;
	pOut[9] = pState[9] + s9;
	pOut[10] = pState[10] + s10;
	pOut[11] = pState[11] + s11;
	pOut[12] = pState[12] + s12;
	pOut[13] = pState[13] + s13;
	pOut[14] = pState[14] + s14;
	pOut[15] = pState[15] + s15;
}

XXAPI void xrtChaCha20(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, uint32 iCounter, const uint8 *pIn, size_t iLen)
{
	uint32 tState[__XRT_CHACHA20_STATE_WORDS];
	uint32 tBlock[__XRT_CHACHA20_STATE_WORDS];
	size_t i, iFullBlocks, iLastBlock;
	
	// “constant” = "expand 32-byte k"
	tState[0] = 0x61707865;
	tState[1] = 0x3320646e;
	tState[2] = 0x79622d32;
	tState[3] = 0x6b206574;
	// key
	for ( i = 0; i < 8; i++ ) {
		tState[4 + i] = __xrt_load32_le(pKey + i * 4);
	}
	// counter + nonce
	tState[12] = iCounter;
	tState[13] = __xrt_load32_le(pNonce);
	tState[14] = __xrt_load32_le(pNonce + 4);
	tState[15] = __xrt_load32_le(pNonce + 8);
	
	iFullBlocks = iLen / __XRT_CHACHA20_BLOCK_SIZE;
	for ( i = 0; i < iFullBlocks; i++ ) {
		size_t j;
		__xrt_chacha20_block(tState, tBlock);
		for ( j = 0; j < __XRT_CHACHA20_STATE_WORDS; j++ ) {
			uint32 iVal = __xrt_load32_le(pIn + i * __XRT_CHACHA20_BLOCK_SIZE + j * 4);
			__xrt_store32_le(pOut + i * __XRT_CHACHA20_BLOCK_SIZE + j * 4, iVal ^ tBlock[j]);
		}
		tState[12]++;
	}
	
	iLastBlock = iLen % __XRT_CHACHA20_BLOCK_SIZE;
	if ( iLastBlock > 0 ) {
		size_t iOff = iFullBlocks * __XRT_CHACHA20_BLOCK_SIZE;
		uint8 arrKs[__XRT_CHACHA20_BLOCK_SIZE];
		size_t j;
		__xrt_chacha20_block(tState, tBlock);
		for ( j = 0; j < __XRT_CHACHA20_STATE_WORDS; j++ ) {
			__xrt_store32_le(arrKs + j * 4, tBlock[j]);
		}
		for ( j = 0; j < iLastBlock; j++ ) {
			pOut[iOff + j] = pIn[iOff + j] ^ arrKs[j];
		}
	}
}


// Poly1305 实现 (based on poly1305-donna, Public Domain)
typedef struct {
	uint32 r[5];
	uint32 h[5];
	uint32 pad[4];
	size_t leftover;
	uint8 buffer[16];
	uint8 final;
} __xrt_poly1305_ctx;

static void __xrt_poly1305_init(__xrt_poly1305_ctx *pCtx, const uint8 pKey[32])
{
	memset(pCtx, 0, sizeof(__xrt_poly1305_ctx));
	// r
	pCtx->r[0] = (__xrt_load32_le(pKey + 0)) & 0x3ffffff;
	pCtx->r[1] = (__xrt_load32_le(pKey + 3) >> 2) & 0x3ffff03;
	pCtx->r[2] = (__xrt_load32_le(pKey + 6) >> 4) & 0x3ffc0ff;
	pCtx->r[3] = (__xrt_load32_le(pKey + 9) >> 6) & 0x3f03fff;
	pCtx->r[4] = (__xrt_load32_le(pKey + 12) >> 8) & 0x00fffff;
	// pad
	pCtx->pad[0] = __xrt_load32_le(pKey + 16);
	pCtx->pad[1] = __xrt_load32_le(pKey + 20);
	pCtx->pad[2] = __xrt_load32_le(pKey + 24);
	pCtx->pad[3] = __xrt_load32_le(pKey + 28);
}

static void __xrt_poly1305_blocks(__xrt_poly1305_ctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	uint32 hibit = pCtx->final ? 0 : (1 << 24);
	uint32 r0 = pCtx->r[0], r1 = pCtx->r[1], r2 = pCtx->r[2], r3 = pCtx->r[3], r4 = pCtx->r[4];
	uint32 s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
	uint32 h0 = pCtx->h[0], h1 = pCtx->h[1], h2 = pCtx->h[2], h3 = pCtx->h[3], h4 = pCtx->h[4];
	uint64 d0, d1, d2, d3, d4;
	uint32 c;
	
	while ( iLen >= 16 ) {
		h0 += (__xrt_load32_le(pMsg + 0)) & 0x3ffffff;
		h1 += (__xrt_load32_le(pMsg + 3) >> 2) & 0x3ffffff;
		h2 += (__xrt_load32_le(pMsg + 6) >> 4) & 0x3ffffff;
		h3 += (__xrt_load32_le(pMsg + 9) >> 6) & 0x3ffffff;
		h4 += (__xrt_load32_le(pMsg + 12) >> 8) | hibit;
		
		d0 = ((uint64)h0 * r0) + ((uint64)h1 * s4) + ((uint64)h2 * s3) + ((uint64)h3 * s2) + ((uint64)h4 * s1);
		d1 = ((uint64)h0 * r1) + ((uint64)h1 * r0) + ((uint64)h2 * s4) + ((uint64)h3 * s3) + ((uint64)h4 * s2);
		d2 = ((uint64)h0 * r2) + ((uint64)h1 * r1) + ((uint64)h2 * r0) + ((uint64)h3 * s4) + ((uint64)h4 * s3);
		d3 = ((uint64)h0 * r3) + ((uint64)h1 * r2) + ((uint64)h2 * r1) + ((uint64)h3 * r0) + ((uint64)h4 * s4);
		d4 = ((uint64)h0 * r4) + ((uint64)h1 * r3) + ((uint64)h2 * r2) + ((uint64)h3 * r1) + ((uint64)h4 * r0);
		
		c = (uint32)(d0 >> 26); h0 = (uint32)d0 & 0x3ffffff;
		d1 += c; c = (uint32)(d1 >> 26); h1 = (uint32)d1 & 0x3ffffff;
		d2 += c; c = (uint32)(d2 >> 26); h2 = (uint32)d2 & 0x3ffffff;
		d3 += c; c = (uint32)(d3 >> 26); h3 = (uint32)d3 & 0x3ffffff;
		d4 += c; c = (uint32)(d4 >> 26); h4 = (uint32)d4 & 0x3ffffff;
		h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
		h1 += c;
		
		pMsg += 16;
		iLen -= 16;
	}
	
	pCtx->h[0] = h0;
	pCtx->h[1] = h1;
	pCtx->h[2] = h2;
	pCtx->h[3] = h3;
	pCtx->h[4] = h4;
}

static void __xrt_poly1305_update(__xrt_poly1305_ctx *pCtx, const uint8 *pMsg, size_t iLen)
{
	size_t i;
	if ( pCtx->leftover ) {
		size_t iWant = 16 - pCtx->leftover;
		if ( iWant > iLen ) { iWant = iLen; }
		for ( i = 0; i < iWant; i++ ) {
			pCtx->buffer[pCtx->leftover + i] = pMsg[i];
		}
		iLen -= iWant;
		pMsg += iWant;
		pCtx->leftover += iWant;
		if ( pCtx->leftover < 16 ) { return; }
		__xrt_poly1305_blocks(pCtx, pCtx->buffer, 16);
		pCtx->leftover = 0;
	}
	
	if ( iLen >= 16 ) {
		size_t iWant = iLen & ~(size_t)15;
		__xrt_poly1305_blocks(pCtx, pMsg, iWant);
		pMsg += iWant;
		iLen -= iWant;
	}
	
	if ( iLen ) {
		for ( i = 0; i < iLen; i++ ) {
			pCtx->buffer[pCtx->leftover + i] = pMsg[i];
		}
		pCtx->leftover += iLen;
	}
}

static void __xrt_poly1305_finish(__xrt_poly1305_ctx *pCtx, uint8 pMac[16])
{
	uint32 h0, h1, h2, h3, h4, c;
	uint32 g0, g1, g2, g3, g4;
	uint64 f;
	uint32 mask;
	
	if ( pCtx->leftover ) {
		size_t i = pCtx->leftover;
		pCtx->buffer[i++] = 1;
		for ( ; i < 16; i++ ) {
			pCtx->buffer[i] = 0;
		}
		pCtx->final = 1;
		__xrt_poly1305_blocks(pCtx, pCtx->buffer, 16);
	}
	
	h0 = pCtx->h[0]; h1 = pCtx->h[1]; h2 = pCtx->h[2]; h3 = pCtx->h[3]; h4 = pCtx->h[4];
	
	c = h1 >> 26; h1 = h1 & 0x3ffffff;
	h2 += c; c = h2 >> 26; h2 = h2 & 0x3ffffff;
	h3 += c; c = h3 >> 26; h3 = h3 & 0x3ffffff;
	h4 += c; c = h4 >> 26; h4 = h4 & 0x3ffffff;
	h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
	h1 += c;
	
	g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
	g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
	g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
	g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
	g4 = h4 + c - (1 << 26);
	
	mask = (g4 >> 31) - 1;
	g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
	mask = ~mask;
	h0 = (h0 & mask) | g0;
	h1 = (h1 & mask) | g1;
	h2 = (h2 & mask) | g2;
	h3 = (h3 & mask) | g3;
	h4 = (h4 & mask) | g4;
	
	h0 = ((h0) | (h1 << 26)) & 0xffffffff;
	h1 = ((h1 >> 6) | (h2 << 20)) & 0xffffffff;
	h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffff;
	h3 = ((h3 >> 18) | (h4 << 8)) & 0xffffffff;
	
	f = (uint64)h0 + pCtx->pad[0]; h0 = (uint32)f;
	f = (uint64)h1 + pCtx->pad[1] + (f >> 32); h1 = (uint32)f;
	f = (uint64)h2 + pCtx->pad[2] + (f >> 32); h2 = (uint32)f;
	f = (uint64)h3 + pCtx->pad[3] + (f >> 32); h3 = (uint32)f;
	
	__xrt_store32_le(pMac + 0, h0);
	__xrt_store32_le(pMac + 4, h1);
	__xrt_store32_le(pMac + 8, h2);
	__xrt_store32_le(pMac + 12, h3);
}

// 生成 Poly1305 密钥 (使用 ChaCha20 counter=0 输出的前 32 字节)
static void __xrt_chacha20_poly1305_keygen(uint8 pPolyKey[32], const uint8 pKey[32], const uint8 pNonce[12])
{
	uint32 tState[__XRT_CHACHA20_STATE_WORDS];
	uint32 tBlock[__XRT_CHACHA20_STATE_WORDS];
	size_t i;
	
	tState[0] = 0x61707865;
	tState[1] = 0x3320646e;
	tState[2] = 0x79622d32;
	tState[3] = 0x6b206574;
	for ( i = 0; i < 8; i++ ) {
		tState[4 + i] = __xrt_load32_le(pKey + i * 4);
	}
	tState[12] = 0;
	tState[13] = __xrt_load32_le(pNonce);
	tState[14] = __xrt_load32_le(pNonce + 4);
	tState[15] = __xrt_load32_le(pNonce + 8);
	
	__xrt_chacha20_block(tState, tBlock);
	for ( i = 0; i < 8; i++ ) {
		__xrt_store32_le(pPolyKey + i * 4, tBlock[i]);
	}
}

static void __xrt_poly1305_pad16(__xrt_poly1305_ctx *pCtx, size_t iLen)
{
	uint8 arrZeros[16] = {0};
	size_t iPad = iLen % 16;
	if ( iPad > 0 ) {
		__xrt_poly1305_update(pCtx, arrZeros, 16 - iPad);
	}
}

XXAPI bool xrtChaCha20Poly1305Encrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen)
{
	uint8 arrPolyKey[32];
	uint8 arrLens[16];
	__xrt_poly1305_ctx tPoly;
	
	// 生成 Poly1305 密钥
	__xrt_chacha20_poly1305_keygen(arrPolyKey, pKey, pNonce);
	
	// 加密（counter 从 1 开始）
	xrtChaCha20(pOut, pKey, pNonce, 1, pIn, iLen);
	
	// 计算 tag
	__xrt_poly1305_init(&tPoly, arrPolyKey);
	__xrt_poly1305_update(&tPoly, pAAD, iAADLen);
	__xrt_poly1305_pad16(&tPoly, iAADLen);
	__xrt_poly1305_update(&tPoly, pOut, iLen);
	__xrt_poly1305_pad16(&tPoly, iLen);
	__xrt_store64_le(arrLens, (uint64)iAADLen);
	__xrt_store64_le(arrLens + 8, (uint64)iLen);
	__xrt_poly1305_update(&tPoly, arrLens, 16);
	__xrt_poly1305_finish(&tPoly, pOut + iLen);  // 16 字节 tag 追加在密文后
	
	return true;
}

XXAPI bool xrtChaCha20Poly1305Decrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen)
{
	uint8 arrPolyKey[32];
	uint8 arrLens[16];
	uint8 arrTag[16];
	__xrt_poly1305_ctx tPoly;
	size_t iCipherLen;
	uint8 iDiff;
	int i;
	
	if ( iLen < 16 ) {
		return false;  // 至少需要 16 字节 tag
	}
	iCipherLen = iLen - 16;
	
	// 生成 Poly1305 密钥
	__xrt_chacha20_poly1305_keygen(arrPolyKey, pKey, pNonce);
	
	// 验证 tag
	__xrt_poly1305_init(&tPoly, arrPolyKey);
	__xrt_poly1305_update(&tPoly, pAAD, iAADLen);
	__xrt_poly1305_pad16(&tPoly, iAADLen);
	__xrt_poly1305_update(&tPoly, pIn, iCipherLen);
	__xrt_poly1305_pad16(&tPoly, iCipherLen);
	__xrt_store64_le(arrLens, (uint64)iAADLen);
	__xrt_store64_le(arrLens + 8, (uint64)iCipherLen);
	__xrt_poly1305_update(&tPoly, arrLens, 16);
	__xrt_poly1305_finish(&tPoly, arrTag);
	
	// 常量时间比较
	iDiff = 0;
	for ( i = 0; i < 16; i++ ) {
		iDiff |= arrTag[i] ^ pIn[iCipherLen + i];
	}
	if ( iDiff != 0 ) {
		return false;  // tag 验证失败
	}
	
	// 解密
	xrtChaCha20(pOut, pKey, pNonce, 1, pIn, iCipherLen);
	return true;
}



/* ============================== AES-128-GCM ============================== */

/*
	AES Rijndael 128-bit block cipher (Public Domain)
	Source: Steven M. Gibson / GRC.com
*/

typedef struct {
	int iDirection;
	int iRounds;
	uint32 arrRK[60];
} __xrt_aes_ctx;

typedef struct {
	int iMode;
	uint64 iLen;
	uint64 iAddLen;
	uint64 arrHL[16];
	uint64 arrHH[16];
	uint8 arrBaseEctr[16];
	uint8 arrY[16];
	uint8 arrBuf[16];
	__xrt_aes_ctx tAes;
} __xrt_gcm_ctx;

static int __xrt_aes_tables_inited = 0;
static uint8 __xrt_aes_FSb[256];
static uint32 __xrt_aes_FT0[256], __xrt_aes_FT1[256], __xrt_aes_FT2[256], __xrt_aes_FT3[256];
#if 1 /* AES decryption support */
static uint8 __xrt_aes_RSb[256];
static uint32 __xrt_aes_RT0[256], __xrt_aes_RT1[256], __xrt_aes_RT2[256], __xrt_aes_RT3[256];
#endif
static uint32 __xrt_aes_RCON[10];

#define __XRT_AES_ENCRYPT 1
#define __XRT_AES_DECRYPT 0

#define __XRT_AES_GET_UINT32(n, b, i) (n) = ((uint32)(b)[(i)] << 24) | ((uint32)(b)[(i)+1] << 16) | ((uint32)(b)[(i)+2] << 8) | ((uint32)(b)[(i)+3])
#define __XRT_AES_PUT_UINT32(n, b, i) { (b)[(i)] = (uint8)((n) >> 24); (b)[(i)+1] = (uint8)((n) >> 16); (b)[(i)+2] = (uint8)((n) >> 8); (b)[(i)+3] = (uint8)(n); }

// GF(2^8) 乘法
static uint8 __xrt_mul_gf(uint8 a, uint8 b)
{
	uint8 r = 0;
	int i;
	for ( i = 0; i < 8; i++ ) {
		if ( b & 1 ) { r ^= a; }
		if ( a & 0x80 ) { a = (uint8)((a << 1) ^ 0x1B); }
		else { a <<= 1; }
		b >>= 1;
	}
	return r;
}

static void __xrt_aes_init_tables(void)
{
	int i;
	uint8 x, y, z;
	uint8 pow_tab[256], log_tab[256];
	
	pow_tab[0] = 1;
	for ( i = 1; i < 256; i++ ) {
		pow_tab[i] = pow_tab[i - 1] ^ (uint8)((pow_tab[i - 1] << 1) ^ ((pow_tab[i - 1] & 0x80) ? 0x1B : 0));
	}
	for ( i = 0; i < 255; i++ ) {
		log_tab[pow_tab[i]] = (uint8)i;
	}
	log_tab[0] = 0;
	
	for ( i = 0; i < 256; i++ ) {
		x = (uint8)i;
		if ( x ) {
			x = pow_tab[255 - log_tab[x]];
		}
		y = (uint8)((x << 1) | (x >> 7));
		x ^= y;
		y = (uint8)((y << 1) | (y >> 7));
		x ^= y;
		y = (uint8)((y << 1) | (y >> 7));
		x ^= y;
		y = (uint8)((y << 1) | (y >> 7));
		x ^= y ^ 0x63;
		__xrt_aes_FSb[i] = x;
		__xrt_aes_RSb[x] = (uint8)i;
	}
	
	for ( i = 0; i < 256; i++ ) {
		x = __xrt_aes_FSb[i];
		y = (uint8)((x << 1) ^ ((x & 0x80) ? 0x1B : 0));
		z = (uint8)(y ^ x);
		__xrt_aes_FT0[i] = ((uint32)y << 24) | ((uint32)x << 16) | ((uint32)x << 8) | (uint32)z;
		__xrt_aes_FT1[i] = (__xrt_aes_FT0[i] >> 8) | (__xrt_aes_FT0[i] << 24);
		__xrt_aes_FT2[i] = (__xrt_aes_FT1[i] >> 8) | (__xrt_aes_FT1[i] << 24);
		__xrt_aes_FT3[i] = (__xrt_aes_FT2[i] >> 8) | (__xrt_aes_FT2[i] << 24);
		
		x = __xrt_aes_RSb[i];
		__xrt_aes_RT0[i] = ((uint32)__xrt_mul_gf(0x0E, x) << 24) |
		                     ((uint32)__xrt_mul_gf(0x09, x) << 16) |
		                     ((uint32)__xrt_mul_gf(0x0D, x) << 8) |
		                     (uint32)__xrt_mul_gf(0x0B, x);
		__xrt_aes_RT1[i] = (__xrt_aes_RT0[i] >> 8) | (__xrt_aes_RT0[i] << 24);
		__xrt_aes_RT2[i] = (__xrt_aes_RT1[i] >> 8) | (__xrt_aes_RT1[i] << 24);
		__xrt_aes_RT3[i] = (__xrt_aes_RT2[i] >> 8) | (__xrt_aes_RT2[i] << 24);
	}
	
	__xrt_aes_RCON[0] = 0x01000000;
	for ( i = 1; i < 10; i++ ) {
		__xrt_aes_RCON[i] = __xrt_aes_RCON[i - 1] << 1;
		if ( __xrt_aes_RCON[i - 1] & 0x80000000 ) {
			__xrt_aes_RCON[i] ^= 0x1B000000;
		}
	}
	__xrt_aes_tables_inited = 1;
}

static int __xrt_aes_setkey(__xrt_aes_ctx *pCtx, int iMode, const uint8 *pKey, uint32 iKeySize)
{
	uint32 *pRK;
	int i;
	int iNk;  // 密钥字数: AES-128=4, AES-256=8
	
	if ( !__xrt_aes_tables_inited ) {
		__xrt_aes_init_tables();
	}
	
	if ( iKeySize == 32 ) {
		pCtx->iRounds = 14;  // AES-256
		iNk = 8;
	} else {
		pCtx->iRounds = 10;  // AES-128
		iNk = 4;
	}
	pCtx->iDirection = iMode;
	pRK = pCtx->arrRK;
	
	for ( i = 0; i < iNk; i++ ) {
		__XRT_AES_GET_UINT32(pRK[i], pKey, i * 4);
	}
	
	if ( iNk == 4 ) {
		// AES-128 密钥扩展
		for ( i = 0; i < 10; i++ ) {
			pRK[4] = pRK[0] ^ __xrt_aes_RCON[i] ^
			         ((uint32)__xrt_aes_FSb[(pRK[3] >> 16) & 0xFF] << 24) ^
			         ((uint32)__xrt_aes_FSb[(pRK[3] >> 8) & 0xFF] << 16) ^
			         ((uint32)__xrt_aes_FSb[(pRK[3]) & 0xFF] << 8) ^
			         ((uint32)__xrt_aes_FSb[(pRK[3] >> 24) & 0xFF]);
			pRK[5] = pRK[1] ^ pRK[4];
			pRK[6] = pRK[2] ^ pRK[5];
			pRK[7] = pRK[3] ^ pRK[6];
			pRK += 4;
		}
	} else {
		// AES-256 密钥扩展
		for ( i = 0; i < 7; i++ ) {
			pRK[8] = pRK[0] ^ __xrt_aes_RCON[i] ^
			         ((uint32)__xrt_aes_FSb[(pRK[7] >> 16) & 0xFF] << 24) ^
			         ((uint32)__xrt_aes_FSb[(pRK[7] >> 8) & 0xFF] << 16) ^
			         ((uint32)__xrt_aes_FSb[(pRK[7]) & 0xFF] << 8) ^
			         ((uint32)__xrt_aes_FSb[(pRK[7] >> 24) & 0xFF]);
			pRK[9]  = pRK[1] ^ pRK[8];
			pRK[10] = pRK[2] ^ pRK[9];
			pRK[11] = pRK[3] ^ pRK[10];
			if ( i < 6 ) {
				pRK[12] = pRK[4] ^
				          ((uint32)__xrt_aes_FSb[(pRK[11] >> 24) & 0xFF] << 24) ^
				          ((uint32)__xrt_aes_FSb[(pRK[11] >> 16) & 0xFF] << 16) ^
				          ((uint32)__xrt_aes_FSb[(pRK[11] >> 8) & 0xFF] << 8) ^
				          ((uint32)__xrt_aes_FSb[(pRK[11]) & 0xFF]);
				pRK[13] = pRK[5] ^ pRK[12];
				pRK[14] = pRK[6] ^ pRK[13];
				pRK[15] = pRK[7] ^ pRK[14];
			}
			pRK += 8;
		}
	}
	
	if ( iMode == __XRT_AES_DECRYPT ) {
		__xrt_aes_ctx tEnc;
		uint32 *pSK;
		int iNrk = pCtx->iRounds * 4;
		__xrt_aes_setkey(&tEnc, __XRT_AES_ENCRYPT, pKey, iKeySize);
		pSK = tEnc.arrRK + iNrk;
		pRK = pCtx->arrRK;
		
		pRK[0] = pSK[0]; pRK[1] = pSK[1]; pRK[2] = pSK[2]; pRK[3] = pSK[3];
		pRK += 4; pSK -= 4;
		
		for ( i = 1; i < pCtx->iRounds; i++ ) {
			pRK[0] = __xrt_aes_RT0[__xrt_aes_FSb[(pSK[0] >> 24) & 0xFF]] ^
			         __xrt_aes_RT1[__xrt_aes_FSb[(pSK[0] >> 16) & 0xFF]] ^
			         __xrt_aes_RT2[__xrt_aes_FSb[(pSK[0] >> 8) & 0xFF]] ^
			         __xrt_aes_RT3[__xrt_aes_FSb[(pSK[0]) & 0xFF]];
			pRK[1] = __xrt_aes_RT0[__xrt_aes_FSb[(pSK[1] >> 24) & 0xFF]] ^
			         __xrt_aes_RT1[__xrt_aes_FSb[(pSK[1] >> 16) & 0xFF]] ^
			         __xrt_aes_RT2[__xrt_aes_FSb[(pSK[1] >> 8) & 0xFF]] ^
			         __xrt_aes_RT3[__xrt_aes_FSb[(pSK[1]) & 0xFF]];
			pRK[2] = __xrt_aes_RT0[__xrt_aes_FSb[(pSK[2] >> 24) & 0xFF]] ^
			         __xrt_aes_RT1[__xrt_aes_FSb[(pSK[2] >> 16) & 0xFF]] ^
			         __xrt_aes_RT2[__xrt_aes_FSb[(pSK[2] >> 8) & 0xFF]] ^
			         __xrt_aes_RT3[__xrt_aes_FSb[(pSK[2]) & 0xFF]];
			pRK[3] = __xrt_aes_RT0[__xrt_aes_FSb[(pSK[3] >> 24) & 0xFF]] ^
			         __xrt_aes_RT1[__xrt_aes_FSb[(pSK[3] >> 16) & 0xFF]] ^
			         __xrt_aes_RT2[__xrt_aes_FSb[(pSK[3] >> 8) & 0xFF]] ^
			         __xrt_aes_RT3[__xrt_aes_FSb[(pSK[3]) & 0xFF]];
			pRK += 4; pSK -= 4;
		}
		
		pRK[0] = pSK[0]; pRK[1] = pSK[1]; pRK[2] = pSK[2]; pRK[3] = pSK[3];
	}
	
	return 0;
}

#define __XRT_AES_FROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3) \
	X0 = *pRK++ ^ __xrt_aes_FT0[(Y0 >> 24) & 0xFF] ^ __xrt_aes_FT1[(Y1 >> 16) & 0xFF] ^ __xrt_aes_FT2[(Y2 >> 8) & 0xFF] ^ __xrt_aes_FT3[Y3 & 0xFF]; \
	X1 = *pRK++ ^ __xrt_aes_FT0[(Y1 >> 24) & 0xFF] ^ __xrt_aes_FT1[(Y2 >> 16) & 0xFF] ^ __xrt_aes_FT2[(Y3 >> 8) & 0xFF] ^ __xrt_aes_FT3[Y0 & 0xFF]; \
	X2 = *pRK++ ^ __xrt_aes_FT0[(Y2 >> 24) & 0xFF] ^ __xrt_aes_FT1[(Y3 >> 16) & 0xFF] ^ __xrt_aes_FT2[(Y0 >> 8) & 0xFF] ^ __xrt_aes_FT3[Y1 & 0xFF]; \
	X3 = *pRK++ ^ __xrt_aes_FT0[(Y3 >> 24) & 0xFF] ^ __xrt_aes_FT1[(Y0 >> 16) & 0xFF] ^ __xrt_aes_FT2[(Y1 >> 8) & 0xFF] ^ __xrt_aes_FT3[Y2 & 0xFF];

#define __XRT_AES_RROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3) \
	X0 = *pRK++ ^ __xrt_aes_RT0[(Y0 >> 24) & 0xFF] ^ __xrt_aes_RT1[(Y3 >> 16) & 0xFF] ^ __xrt_aes_RT2[(Y2 >> 8) & 0xFF] ^ __xrt_aes_RT3[Y1 & 0xFF]; \
	X1 = *pRK++ ^ __xrt_aes_RT0[(Y1 >> 24) & 0xFF] ^ __xrt_aes_RT1[(Y0 >> 16) & 0xFF] ^ __xrt_aes_RT2[(Y3 >> 8) & 0xFF] ^ __xrt_aes_RT3[Y2 & 0xFF]; \
	X2 = *pRK++ ^ __xrt_aes_RT0[(Y2 >> 24) & 0xFF] ^ __xrt_aes_RT1[(Y1 >> 16) & 0xFF] ^ __xrt_aes_RT2[(Y0 >> 8) & 0xFF] ^ __xrt_aes_RT3[Y3 & 0xFF]; \
	X3 = *pRK++ ^ __xrt_aes_RT0[(Y3 >> 24) & 0xFF] ^ __xrt_aes_RT1[(Y2 >> 16) & 0xFF] ^ __xrt_aes_RT2[(Y1 >> 8) & 0xFF] ^ __xrt_aes_RT3[Y0 & 0xFF];

static int __xrt_aes_cipher(__xrt_aes_ctx *pCtx, const uint8 pIn[16], uint8 pOut[16])
{
	uint32 *pRK = pCtx->arrRK;
	uint32 X0, X1, X2, X3, Y0, Y1, Y2, Y3;
	
	__XRT_AES_GET_UINT32(X0, pIn, 0);
	__XRT_AES_GET_UINT32(X1, pIn, 4);
	__XRT_AES_GET_UINT32(X2, pIn, 8);
	__XRT_AES_GET_UINT32(X3, pIn, 12);
	
	X0 ^= *pRK++; X1 ^= *pRK++; X2 ^= *pRK++; X3 ^= *pRK++;
	
	if ( pCtx->iDirection == __XRT_AES_ENCRYPT ) {
		int i;
		for ( i = (pCtx->iRounds >> 1) - 1; i > 0; i-- ) {
			__XRT_AES_FROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
			__XRT_AES_FROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3);
		}
		__XRT_AES_FROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
		
		X0 = *pRK++ ^ ((uint32)__xrt_aes_FSb[(Y0 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_FSb[(Y1 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_FSb[(Y2 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_FSb[Y3 & 0xFF]);
		X1 = *pRK++ ^ ((uint32)__xrt_aes_FSb[(Y1 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_FSb[(Y2 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_FSb[(Y3 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_FSb[Y0 & 0xFF]);
		X2 = *pRK++ ^ ((uint32)__xrt_aes_FSb[(Y2 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_FSb[(Y3 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_FSb[(Y0 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_FSb[Y1 & 0xFF]);
		X3 = *pRK++ ^ ((uint32)__xrt_aes_FSb[(Y3 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_FSb[(Y0 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_FSb[(Y1 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_FSb[Y2 & 0xFF]);
	} else {
		int i;
		for ( i = (pCtx->iRounds >> 1) - 1; i > 0; i-- ) {
			__XRT_AES_RROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
			__XRT_AES_RROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3);
		}
		__XRT_AES_RROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
		
		X0 = *pRK++ ^ ((uint32)__xrt_aes_RSb[(Y0 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_RSb[(Y3 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_RSb[(Y2 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_RSb[Y1 & 0xFF]);
		X1 = *pRK++ ^ ((uint32)__xrt_aes_RSb[(Y1 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_RSb[(Y0 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_RSb[(Y3 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_RSb[Y2 & 0xFF]);
		X2 = *pRK++ ^ ((uint32)__xrt_aes_RSb[(Y2 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_RSb[(Y1 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_RSb[(Y0 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_RSb[Y3 & 0xFF]);
		X3 = *pRK++ ^ ((uint32)__xrt_aes_RSb[(Y3 >> 24) & 0xFF] << 24) ^
		               ((uint32)__xrt_aes_RSb[(Y2 >> 16) & 0xFF] << 16) ^
		               ((uint32)__xrt_aes_RSb[(Y1 >> 8) & 0xFF] << 8) ^
		               ((uint32)__xrt_aes_RSb[Y0 & 0xFF]);
	}
	
	__XRT_AES_PUT_UINT32(X0, pOut, 0);
	__XRT_AES_PUT_UINT32(X1, pOut, 4);
	__XRT_AES_PUT_UINT32(X2, pOut, 8);
	__XRT_AES_PUT_UINT32(X3, pOut, 12);
	
	return 0;
}

// GCM 实现
// GF(2^128) 约简查找表: last4[x] = x * P^128 in GF(2^128)
// 其中 P(x) = x^128 + x^7 + x^2 + x + 1
static const uint64 __xrt_gcm_last4[16] = {
	0x0000, 0x1c20, 0x3840, 0x2460,
	0x7080, 0x6ca0, 0x48c0, 0x54e0,
	0xe100, 0xfd20, 0xd940, 0xc560,
	0x9180, 0x8da0, 0xa9c0, 0xb5e0
};

static void __xrt_gcm_mult(__xrt_gcm_ctx *pCtx, uint8 pX[16])
{
	uint64 zh, zl, v;
	int i;
	uint8 lo, hi;
	
	lo = pX[15] & 0x0f;
	
	zh = pCtx->arrHH[lo];
	zl = pCtx->arrHL[lo];
	
	for ( i = 15; i >= 0; i-- ) {
		lo = pX[i] & 0x0f;
		hi = pX[i] >> 4;
		
		if ( i != 15 ) {
			v = (uint64)zl & 0x0f;
			zl = (zl >> 4) | (zh << 60);
			zh >>= 4;
			zh ^= __xrt_gcm_last4[v] << 48;
			zh ^= pCtx->arrHH[lo];
			zl ^= pCtx->arrHL[lo];
		}
		
		v = (uint64)zl & 0x0f;
		zl = (zl >> 4) | (zh << 60);
		zh >>= 4;
		zh ^= __xrt_gcm_last4[v] << 48;
		zh ^= pCtx->arrHH[hi];
		zl ^= pCtx->arrHL[hi];
	}
	
	// 存储结果 (big-endian)
	for ( i = 0; i < 8; i++ ) {
		pX[i] = (uint8)(zh >> (56 - i * 8));
		pX[i + 8] = (uint8)(zl >> (56 - i * 8));
	}
}

static int __xrt_gcm_setkey(__xrt_gcm_ctx *pCtx, const uint8 *pKey, uint32 iKeySize)
{
	uint8 h[16] = {0};
	uint64 vh, vl;
	int i, j;
	
	memset(pCtx, 0, sizeof(__xrt_gcm_ctx));
	__xrt_aes_setkey(&pCtx->tAes, __XRT_AES_ENCRYPT, pKey, iKeySize);
	__xrt_aes_cipher(&pCtx->tAes, h, h);
	
	// H 存储为 big-endian
	vh = ((uint64)h[0] << 56) | ((uint64)h[1] << 48) | ((uint64)h[2] << 40) | ((uint64)h[3] << 32) |
	     ((uint64)h[4] << 24) | ((uint64)h[5] << 16) | ((uint64)h[6] << 8) | (uint64)h[7];
	vl = ((uint64)h[8] << 56) | ((uint64)h[9] << 48) | ((uint64)h[10] << 40) | ((uint64)h[11] << 32) |
	     ((uint64)h[12] << 24) | ((uint64)h[13] << 16) | ((uint64)h[14] << 8) | (uint64)h[15];
	
	pCtx->arrHL[0] = 0;
	pCtx->arrHH[0] = 0;
	pCtx->arrHH[8] = vh;
	pCtx->arrHL[8] = vl;
	
	for ( i = 4; i > 0; i >>= 1 ) {
		uint32 T = (uint32)(vl & 1) * 0xe1000000U;
		vl = (vl >> 1) | (vh << 63);
		vh = (vh >> 1) ^ ((uint64)T << 32);
		pCtx->arrHH[i] = vh;
		pCtx->arrHL[i] = vl;
	}
	
	for ( i = 2; i < 16; i <<= 1 ) {
		uint64 *pHH = pCtx->arrHH + i;
		uint64 *pHL = pCtx->arrHL + i;
		vh = *pHH;
		vl = *pHL;
		for ( j = 1; j < i; j++ ) {
			pHH[j] = vh ^ pCtx->arrHH[j];
			pHL[j] = vl ^ pCtx->arrHL[j];
		}
	}
	
	return 0;
}

static int __xrt_gcm_crypt_and_tag(__xrt_gcm_ctx *pCtx, int iMode,
	const uint8 *pIV, size_t iIVLen,
	const uint8 *pAdd, size_t iAddLen,
	const uint8 *pInput, uint8 *pOutput, size_t iLen,
	uint8 *pTag, size_t iTagLen)
{
	size_t i;
	uint8 arrWork[16], arrEctr[16];
	uint32 iCtr;
	
	// 初始化 Y = IV || 0^31 || 1 (when IV is 12 bytes)
	memset(pCtx->arrY, 0, 16);
	if ( iIVLen == 12 ) {
		memcpy(pCtx->arrY, pIV, 12);
		pCtx->arrY[15] = 1;
	} else {
		// 处理非标准长度 IV
		for ( i = 0; i < iIVLen / 16; i++ ) {
			size_t j;
			for ( j = 0; j < 16; j++ ) {
				pCtx->arrY[j] ^= pIV[i * 16 + j];
			}
			__xrt_gcm_mult(pCtx, pCtx->arrY);
		}
		if ( iIVLen % 16 ) {
			size_t j;
			for ( j = 0; j < iIVLen % 16; j++ ) {
				pCtx->arrY[j] ^= pIV[(iIVLen / 16) * 16 + j];
			}
			__xrt_gcm_mult(pCtx, pCtx->arrY);
		}
		{
			uint8 arrLenBuf[16] = {0};
			uint64 iIVBits = (uint64)iIVLen * 8;
			size_t j;
			for ( j = 0; j < 8; j++ ) {
				arrLenBuf[8 + j] = (uint8)(iIVBits >> (56 - j * 8));
			}
			for ( j = 0; j < 16; j++ ) {
				pCtx->arrY[j] ^= arrLenBuf[j];
			}
			__xrt_gcm_mult(pCtx, pCtx->arrY);
		}
	}
	
	// base_ectr = AES(K, Y0)
	__xrt_aes_cipher(&pCtx->tAes, pCtx->arrY, pCtx->arrBaseEctr);
	
	// 处理 AAD
	memset(pCtx->arrBuf, 0, 16);
	pCtx->iAddLen = iAddLen;
	for ( i = 0; i < iAddLen; i++ ) {
		pCtx->arrBuf[i % 16] ^= pAdd[i];
		if ( ((i + 1) % 16 == 0) || (i + 1 == iAddLen) ) {
			__xrt_gcm_mult(pCtx, pCtx->arrBuf);
		}
	}
	
	// 处理密文/明文
	pCtx->iLen = iLen;
	iCtr = ((uint32)pCtx->arrY[12] << 24) | ((uint32)pCtx->arrY[13] << 16) |
	       ((uint32)pCtx->arrY[14] << 8) | (uint32)pCtx->arrY[15];
	
	for ( i = 0; i < iLen; i++ ) {
		if ( (i % 16) == 0 ) {
			iCtr++;
			pCtx->arrY[12] = (uint8)(iCtr >> 24);
			pCtx->arrY[13] = (uint8)(iCtr >> 16);
			pCtx->arrY[14] = (uint8)(iCtr >> 8);
			pCtx->arrY[15] = (uint8)(iCtr);
			__xrt_aes_cipher(&pCtx->tAes, pCtx->arrY, arrEctr);
		}
		
		if ( iMode == __XRT_AES_ENCRYPT ) {
			pOutput[i] = pInput[i] ^ arrEctr[i % 16];
			pCtx->arrBuf[i % 16] ^= pOutput[i];
		} else {
			pCtx->arrBuf[i % 16] ^= pInput[i];
			pOutput[i] = pInput[i] ^ arrEctr[i % 16];
		}
		
		if ( ((i + 1) % 16 == 0) || (i + 1 == iLen) ) {
			__xrt_gcm_mult(pCtx, pCtx->arrBuf);
		}
	}
	
	// 计算 tag
	{
		uint8 arrLenBuf[16] = {0};
		uint64 iAddBits = (uint64)iAddLen * 8;
		uint64 iCipherBits = (uint64)iLen * 8;
		
		for ( i = 0; i < 8; i++ ) {
			arrLenBuf[i] = (uint8)(iAddBits >> (56 - i * 8));
			arrLenBuf[8 + i] = (uint8)(iCipherBits >> (56 - i * 8));
		}
		for ( i = 0; i < 16; i++ ) {
			pCtx->arrBuf[i] ^= arrLenBuf[i];
		}
		__xrt_gcm_mult(pCtx, pCtx->arrBuf);
		
		for ( i = 0; i < iTagLen; i++ ) {
			pTag[i] = pCtx->arrBuf[i] ^ pCtx->arrBaseEctr[i];
		}
	}
	
	return 0;
}

XXAPI bool xrtAES128GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen)
{
	__xrt_gcm_ctx tCtx;
	__xrt_gcm_setkey(&tCtx, pKey, 16);
	__xrt_gcm_crypt_and_tag(&tCtx, __XRT_AES_ENCRYPT, pNonce, iNonceLen, pAAD, iAADLen, pIn, pOut, iLen, pOut + iLen, 16);
	return true;
}

XXAPI bool xrtAES128GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen)
{
	__xrt_gcm_ctx tCtx;
	uint8 arrTag[16];
	uint8 iDiff;
	int i;
	size_t iCipherLen;
	
	if ( iLen < 16 ) {
		return false;
	}
	iCipherLen = iLen - 16;
	
	__xrt_gcm_setkey(&tCtx, pKey, 16);
	__xrt_gcm_crypt_and_tag(&tCtx, __XRT_AES_DECRYPT, pNonce, iNonceLen, pAAD, iAADLen, pIn, pOut, iCipherLen, arrTag, 16);
	
	// 常量时间比较
	iDiff = 0;
	for ( i = 0; i < 16; i++ ) {
		iDiff |= arrTag[i] ^ pIn[iCipherLen + i];
	}
	
	return (iDiff == 0);
}

XXAPI bool xrtAES256GCMEncrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen)
{
	__xrt_gcm_ctx tCtx;
	__xrt_gcm_setkey(&tCtx, pKey, 32);
	__xrt_gcm_crypt_and_tag(&tCtx, __XRT_AES_ENCRYPT, pNonce, iNonceLen, pAAD, iAADLen, pIn, pOut, iLen, pOut + iLen, 16);
	return true;
}

XXAPI bool xrtAES256GCMDecrypt(uint8 *pOut, const uint8 *pKey, const uint8 *pNonce, size_t iNonceLen, const uint8 *pAAD, size_t iAADLen, const uint8 *pIn, size_t iLen)
{
	__xrt_gcm_ctx tCtx;
	uint8 arrTag[16];
	uint8 iDiff;
	int i;
	size_t iCipherLen;
	
	if ( iLen < 16 ) {
		return false;
	}
	iCipherLen = iLen - 16;
	
	__xrt_gcm_setkey(&tCtx, pKey, 32);
	__xrt_gcm_crypt_and_tag(&tCtx, __XRT_AES_DECRYPT, pNonce, iNonceLen, pAAD, iAADLen, pIn, pOut, iCipherLen, arrTag, 16);
	
	// 常量时间比较
	iDiff = 0;
	for ( i = 0; i < 16; i++ ) {
		iDiff |= arrTag[i] ^ pIn[iCipherLen + i];
	}
	
	return (iDiff == 0);
}



/* ============================== 安全随机数 ============================== */

XXAPI void xrtRandomBytes(uint8 *pBuf, size_t iLen)
{
	#if defined(_WIN32) || defined(_WIN64)
		// Windows: 使用 RtlGenRandom (SystemFunction036)
		// RtlGenRandom 在 advapi32.dll 中, 无需额外链接
		typedef BOOLEAN (WINAPI *RtlGenRandom_t)(PVOID, ULONG);
		static RtlGenRandom_t procRtlGenRandom = NULL;
		if ( !procRtlGenRandom ) {
			HMODULE hLib = LoadLibraryA("advapi32.dll");
			if ( hLib ) {
				procRtlGenRandom = (RtlGenRandom_t)GetProcAddress(hLib, "SystemFunction036");
			}
		}
		if ( procRtlGenRandom ) {
			procRtlGenRandom(pBuf, (ULONG)iLen);
		} else {
			// fallback: 使用 xrt 随机数（非加密安全，仅作为最后手段）
			size_t i;
			for ( i = 0; i < iLen; i++ ) {
				pBuf[i] = (uint8)(xrtRand32() & 0xFF);
			}
		}
	#else
		// Linux: 使用 /dev/urandom
		FILE *fp = fopen("/dev/urandom", "rb");
		if ( fp ) {
			size_t iRead = fread(pBuf, 1, iLen, fp);
			fclose(fp);
			if ( iRead == iLen ) {
				return;
			}
		}
		// fallback
		{
			size_t i;
			for ( i = 0; i < iLen; i++ ) {
				pBuf[i] = (uint8)(xrtRand32() & 0xFF);
			}
		}
	#endif
}



/* ============================== HKDF (RFC 5869) ============================== */

XXAPI void xrtHKDFExtract(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen)
{
	uint8 arrDefaultSalt[32] = {0};
	if ( (pSalt == NULL) || (iSaltLen == 0) ) {
		pSalt = arrDefaultSalt;
		iSaltLen = 32;
	}
	xrtHMAC_SHA256(pSalt, iSaltLen, pIKM, iIKMLen, pPRK);
}

XXAPI void xrtHKDFExpand(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen)
{
	uint8 T[32] = {0};
	size_t iTLen = 0;
	uint8 iCounter = 1;
	size_t iOffset = 0;
	
	while ( iOffset < iOKMLen ) {
		xsha256_ctx tCtx;
		uint8 k[64] = {0};
		uint8 o_pad[64], i_pad[64];
		unsigned int i;
		size_t iCopyLen;
		
		// HMAC-SHA256(PRK, T || Info || counter)
		// 手动构建 HMAC 以避免分配临时缓冲区
		memset(i_pad, 0x36, sizeof(i_pad));
		memset(o_pad, 0x5c, sizeof(o_pad));
		
		if ( iPRKLen <= 64 ) {
			if ( iPRKLen > 0 ) {
				memmove(k, pPRK, iPRKLen);
			}
		} else {
			xrtSHA256Init(&tCtx);
			xrtSHA256Update(&tCtx, (ptr)pPRK, iPRKLen);
			xrtSHA256Final(&tCtx, k);
		}
		
		for ( i = 0; i < sizeof(k); i++ ) {
			i_pad[i] ^= k[i];
			o_pad[i] ^= k[i];
		}
		
		// inner hash: H(i_pad || T(prev) || info || counter)
		xrtSHA256Init(&tCtx);
		xrtSHA256Update(&tCtx, i_pad, sizeof(i_pad));
		if ( iTLen > 0 ) {
			xrtSHA256Update(&tCtx, T, iTLen);
		}
		if ( (pInfo != NULL) && (iInfoLen > 0) ) {
			xrtSHA256Update(&tCtx, (ptr)pInfo, iInfoLen);
		}
		xrtSHA256Update(&tCtx, &iCounter, 1);
		xrtSHA256Final(&tCtx, T);
		
		// outer hash: H(o_pad || inner_hash)
		xrtSHA256Init(&tCtx);
		xrtSHA256Update(&tCtx, o_pad, sizeof(o_pad));
		xrtSHA256Update(&tCtx, T, 32);
		xrtSHA256Final(&tCtx, T);
		
		iTLen = 32;
		iCopyLen = iOKMLen - iOffset;
		if ( iCopyLen > 32 ) {
			iCopyLen = 32;
		}
		memcpy(pOKM + iOffset, T, iCopyLen);
		iOffset += iCopyLen;
		iCounter++;
	}
}



/* ============================== HKDF-SHA384 (RFC 5869) ============================== */

XXAPI void xrtHKDFExtract_SHA384(uint8 *pPRK, const uint8 *pSalt, size_t iSaltLen, const uint8 *pIKM, size_t iIKMLen)
{
	uint8 arrDefaultSalt[48] = {0};
	if ( (pSalt == NULL) || (iSaltLen == 0) ) {
		pSalt = arrDefaultSalt;
		iSaltLen = 48;
	}
	xrtHMAC_SHA384(pSalt, iSaltLen, pIKM, iIKMLen, pPRK);
}

XXAPI void xrtHKDFExpand_SHA384(uint8 *pOKM, size_t iOKMLen, const uint8 *pPRK, size_t iPRKLen, const uint8 *pInfo, size_t iInfoLen)
{
	uint8 T[48] = {0};
	size_t iTLen = 0;
	uint8 iCounter = 1;
	size_t iOffset = 0;
	
	while ( iOffset < iOKMLen ) {
		xsha512_ctx tCtx;
		uint8 k[128] = {0};
		uint8 o_pad[128], i_pad[128];
		unsigned int i;
		size_t iCopyLen;
		
		// HMAC-SHA384(PRK, T || Info || counter)
		memset(i_pad, 0x36, sizeof(i_pad));
		memset(o_pad, 0x5c, sizeof(o_pad));
		
		if ( iPRKLen <= 128 ) {
			if ( iPRKLen > 0 ) {
				memmove(k, pPRK, iPRKLen);
			}
		} else {
			xrtSHA384Init(&tCtx);
			xrtSHA512Update(&tCtx, (ptr)pPRK, iPRKLen);
			xrtSHA384Final(&tCtx, k);
		}
		
		for ( i = 0; i < sizeof(k); i++ ) {
			i_pad[i] ^= k[i];
			o_pad[i] ^= k[i];
		}
		
		// inner hash: H(i_pad || T(prev) || info || counter)
		xrtSHA384Init(&tCtx);
		xrtSHA512Update(&tCtx, i_pad, sizeof(i_pad));
		if ( iTLen > 0 ) {
			xrtSHA512Update(&tCtx, T, iTLen);
		}
		if ( (pInfo != NULL) && (iInfoLen > 0) ) {
			xrtSHA512Update(&tCtx, (ptr)pInfo, iInfoLen);
		}
		xrtSHA512Update(&tCtx, &iCounter, 1);
		xrtSHA384Final(&tCtx, T);
		
		// outer hash: H(o_pad || inner_hash)
		xrtSHA384Init(&tCtx);
		xrtSHA512Update(&tCtx, o_pad, sizeof(o_pad));
		xrtSHA512Update(&tCtx, T, 48);
		xrtSHA384Final(&tCtx, T);
		
		iTLen = 48;
		iCopyLen = iOKMLen - iOffset;
		if ( iCopyLen > 48 ) {
			iCopyLen = 48;
		}
		memcpy(pOKM + iOffset, T, iCopyLen);
		iOffset += iCopyLen;
		iCounter++;
	}
}



/* ============================== X25519 密钥交换 (RFC 7748) ============================== */

/*
	基于 mongoose / STROBE 的 X25519 实现
	Copyright (c) 2015-2016 Cryptography Research, Inc.
	Author: Mike Hamburg
	License: MIT License
*/

#define __XRT_X25519_BYTES 32
#define __XRT_X25519_WBITS 32
#define __XRT_X25519_NLIMBS (256 / __XRT_X25519_WBITS)

typedef uint32 __xrt_x25519_limb;
typedef uint64 __xrt_x25519_dlimb;
typedef int64 __xrt_x25519_sdlimb;
typedef __xrt_x25519_limb __xrt_x25519_fe[__XRT_X25519_NLIMBS];

static const uint8 __xrt_x25519_base_point[__XRT_X25519_BYTES] = {9};

static __xrt_x25519_limb __xrt_x25519_umaal(__xrt_x25519_limb *pCarry, __xrt_x25519_limb acc, __xrt_x25519_limb mand, __xrt_x25519_limb mier)
{
	__xrt_x25519_dlimb tmp = (__xrt_x25519_dlimb)mand * mier + acc + *pCarry;
	*pCarry = (__xrt_x25519_limb)(tmp >> __XRT_X25519_WBITS);
	return (__xrt_x25519_limb)tmp;
}

static __xrt_x25519_limb __xrt_x25519_adc(__xrt_x25519_limb *pCarry, __xrt_x25519_limb acc, __xrt_x25519_limb mand)
{
	__xrt_x25519_dlimb total = (__xrt_x25519_dlimb)*pCarry + acc + mand;
	*pCarry = (__xrt_x25519_limb)(total >> __XRT_X25519_WBITS);
	return (__xrt_x25519_limb)total;
}

static __xrt_x25519_limb __xrt_x25519_adc0(__xrt_x25519_limb *pCarry, __xrt_x25519_limb acc)
{
	__xrt_x25519_dlimb total = (__xrt_x25519_dlimb)*pCarry + acc;
	*pCarry = (__xrt_x25519_limb)(total >> __XRT_X25519_WBITS);
	return (__xrt_x25519_limb)total;
}

static void __xrt_x25519_propagate(__xrt_x25519_fe x, __xrt_x25519_limb over)
{
	unsigned i;
	__xrt_x25519_limb carry;
	over = x[__XRT_X25519_NLIMBS - 1] >> (__XRT_X25519_WBITS - 1) | over << 1;
	x[__XRT_X25519_NLIMBS - 1] &= ~((__xrt_x25519_limb)1 << (__XRT_X25519_WBITS - 1));
	carry = over * 19;
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		x[i] = __xrt_x25519_adc0(&carry, x[i]);
	}
}

static void __xrt_x25519_add(__xrt_x25519_fe out, const __xrt_x25519_fe a, const __xrt_x25519_fe b)
{
	unsigned i;
	__xrt_x25519_limb carry = 0;
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		out[i] = __xrt_x25519_adc(&carry, a[i], b[i]);
	}
	__xrt_x25519_propagate(out, carry);
}

static void __xrt_x25519_sub(__xrt_x25519_fe out, const __xrt_x25519_fe a, const __xrt_x25519_fe b)
{
	unsigned i;
	__xrt_x25519_sdlimb carry = -38;
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		carry = carry + a[i] - b[i];
		out[i] = (__xrt_x25519_limb)carry;
		carry >>= __XRT_X25519_WBITS;
	}
	__xrt_x25519_propagate(out, (__xrt_x25519_limb)(1 + carry));
}

static void __xrt_x25519_mul(__xrt_x25519_fe out, const __xrt_x25519_fe a, const __xrt_x25519_limb *b, unsigned nb)
{
	__xrt_x25519_limb accum[2 * __XRT_X25519_NLIMBS] = {0};
	unsigned i, j;
	__xrt_x25519_limb carry2;
	
	for ( i = 0; i < nb; i++ ) {
		__xrt_x25519_limb mand = b[i];
		carry2 = 0;
		for ( j = 0; j < __XRT_X25519_NLIMBS; j++ ) {
			__xrt_x25519_limb tmp;
			memcpy(&tmp, &a[j], sizeof(tmp));
			accum[i + j] = __xrt_x25519_umaal(&carry2, accum[i + j], mand, tmp);
		}
		accum[i + j] = carry2;
	}
	
	carry2 = 0;
	for ( j = 0; j < __XRT_X25519_NLIMBS; j++ ) {
		out[j] = __xrt_x25519_umaal(&carry2, accum[j], 38, accum[j + __XRT_X25519_NLIMBS]);
	}
	__xrt_x25519_propagate(out, carry2);
}

static void __xrt_x25519_sqr(__xrt_x25519_fe out, const __xrt_x25519_fe a)
{
	__xrt_x25519_mul(out, a, a, __XRT_X25519_NLIMBS);
}
static void __xrt_x25519_mul1(__xrt_x25519_fe out, const __xrt_x25519_fe a)
{
	__xrt_x25519_mul(out, a, out, __XRT_X25519_NLIMBS);
}
static void __xrt_x25519_sqr1(__xrt_x25519_fe a)
{
	__xrt_x25519_mul1(a, a);
}

static void __xrt_x25519_condswap(__xrt_x25519_limb a[2 * __XRT_X25519_NLIMBS], __xrt_x25519_limb b[2 * __XRT_X25519_NLIMBS], __xrt_x25519_limb doswap)
{
	unsigned i;
	for ( i = 0; i < 2 * __XRT_X25519_NLIMBS; i++ ) {
		__xrt_x25519_limb xor_ab = (a[i] ^ b[i]) & doswap;
		a[i] ^= xor_ab;
		b[i] ^= xor_ab;
	}
}

static __xrt_x25519_limb __xrt_x25519_canon(__xrt_x25519_fe x)
{
	unsigned i;
	__xrt_x25519_limb carry0 = 19, res;
	__xrt_x25519_sdlimb carry;
	
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		x[i] = __xrt_x25519_adc0(&carry0, x[i]);
	}
	__xrt_x25519_propagate(x, carry0);
	
	carry = -19;
	res = 0;
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		carry += x[i];
		res |= x[i] = (__xrt_x25519_limb)carry;
		carry >>= __XRT_X25519_WBITS;
	}
	return (__xrt_x25519_limb)(((__xrt_x25519_dlimb)res - 1) >> __XRT_X25519_WBITS);
}

static const __xrt_x25519_limb __xrt_x25519_a24[1] = {121665};

static void __xrt_x25519_ladder_part1(__xrt_x25519_fe xs[5])
{
	__xrt_x25519_limb *x2 = xs[0], *z2 = xs[1], *x3 = xs[2], *z3 = xs[3], *t1 = xs[4];
	__xrt_x25519_add(t1, x2, z2);
	__xrt_x25519_sub(z2, x2, z2);
	__xrt_x25519_add(x2, x3, z3);
	__xrt_x25519_sub(z3, x3, z3);
	__xrt_x25519_mul1(z3, t1);
	__xrt_x25519_mul1(x2, z2);
	__xrt_x25519_add(x3, z3, x2);
	__xrt_x25519_sub(z3, z3, x2);
	__xrt_x25519_sqr1(t1);
	__xrt_x25519_sqr1(z2);
	__xrt_x25519_sub(x2, t1, z2);
	__xrt_x25519_mul(z2, x2, __xrt_x25519_a24, sizeof(__xrt_x25519_a24) / sizeof(__xrt_x25519_a24[0]));
	__xrt_x25519_add(z2, z2, t1);
}

static void __xrt_x25519_ladder_part2(__xrt_x25519_fe xs[5], const __xrt_x25519_fe x1)
{
	__xrt_x25519_limb *x2 = xs[0], *z2 = xs[1], *x3 = xs[2], *z3 = xs[3], *t1 = xs[4];
	__xrt_x25519_sqr1(z3);
	__xrt_x25519_mul1(z3, x1);
	__xrt_x25519_sqr1(x3);
	__xrt_x25519_mul1(z2, x2);
	__xrt_x25519_sub(x2, t1, x2);
	__xrt_x25519_mul1(x2, t1);
}

#define __XRT_MG_U32(a, b, c, d) ((uint32)(a) << 24 | (uint32)(b) << 16 | (uint32)(c) << 8 | (uint32)(d))

static void __xrt_x25519_core(__xrt_x25519_fe xs[5], const uint8 scalar[__XRT_X25519_BYTES], const uint8 *x1, int clamp)
{
	int i;
	__xrt_x25519_fe x1_limbs;
	__xrt_x25519_limb swap = 0;
	__xrt_x25519_limb *x2 = xs[0], *x3 = xs[2], *z3 = xs[3];
	
	memset(xs, 0, 4 * sizeof(__xrt_x25519_fe));
	x2[0] = z3[0] = 1;
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		x3[i] = x1_limbs[i] = __XRT_MG_U32(x1[i * 4 + 3], x1[i * 4 + 2], x1[i * 4 + 1], x1[i * 4]);
	}
	
	for ( i = 255; i >= 0; i-- ) {
		uint8 bytei = scalar[i / 8];
		__xrt_x25519_limb doswap;
		if ( clamp ) {
			if ( i / 8 == 0 ) {
				bytei &= (uint8)~7U;
			} else if ( i / 8 == __XRT_X25519_BYTES - 1 ) {
				bytei &= 0x7F;
				bytei |= 0x40;
			}
		}
		doswap = 0 - (__xrt_x25519_limb)((bytei >> (i % 8)) & 1);
		__xrt_x25519_condswap(x2, x3, swap ^ doswap);
		swap = doswap;
		
		__xrt_x25519_ladder_part1(xs);
		__xrt_x25519_ladder_part2(xs, (const __xrt_x25519_limb *)x1_limbs);
	}
	__xrt_x25519_condswap(x2, x3, swap);
}

static int __xrt_x25519_compute(uint8 out[__XRT_X25519_BYTES], const uint8 scalar[__XRT_X25519_BYTES], const uint8 x1[__XRT_X25519_BYTES], int clamp)
{
	int i, ret;
	__xrt_x25519_fe xs[5], out_limbs;
	__xrt_x25519_limb *x2, *z2, *z3, *prev;
	static const struct { uint8 a, c, n; } steps[13] = {
		{2, 1, 1}, {2, 1, 1}, {4, 2, 3}, {2, 4, 6}, {3, 1, 1},
		{3, 2, 12}, {4, 3, 25}, {2, 3, 25}, {2, 4, 50}, {3, 2, 125},
		{3, 1, 2}, {3, 1, 2}, {3, 1, 1}
	};
	
	__xrt_x25519_core(xs, scalar, x1, clamp);
	
	x2 = xs[0];
	z2 = xs[1];
	z3 = xs[3];
	
	prev = z2;
	for ( i = 0; i < 13; i++ ) {
		int j;
		__xrt_x25519_limb *a = xs[steps[i].a];
		for ( j = steps[i].n; j > 0; j-- ) {
			__xrt_x25519_sqr(a, prev);
			prev = a;
		}
		__xrt_x25519_mul1(a, xs[steps[i].c]);
	}
	
	__xrt_x25519_mul(out_limbs, x2, z3, __XRT_X25519_NLIMBS);
	ret = (int)__xrt_x25519_canon(out_limbs);
	if ( !clamp ) { ret = 0; }
	
	for ( i = 0; i < __XRT_X25519_NLIMBS; i++ ) {
		uint32 n = out_limbs[i];
		out[i * 4] = (uint8)(n & 0xff);
		out[i * 4 + 1] = (uint8)((n >> 8) & 0xff);
		out[i * 4 + 2] = (uint8)((n >> 16) & 0xff);
		out[i * 4 + 3] = (uint8)((n >> 24) & 0xff);
	}
	return ret;
}

XXAPI void xrtX25519Keypair(uint8 *pPrivKey, uint8 *pPubKey)
{
	xrtRandomBytes(pPrivKey, 32);
	__xrt_x25519_compute(pPubKey, pPrivKey, __xrt_x25519_base_point, 1);
}

XXAPI void xrtX25519SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey)
{
	__xrt_x25519_compute(pOut, pPrivKey, pPubKey, 1);
}



/* ============================== RSA 大整数运算 ============================== */

/*
	基于 axTLS bignum library (Cameron Rich, BSD License)
	经 mongoose tls_rsa.c 验证，仅用于 RSA 签名验证
	使用 classical reduction (最简单可靠的模式)
*/

// 大整数组件类型 (32-bit 组件, 支持最大 4096-bit RSA)
typedef uint32 __xrt_bi_comp;
typedef uint64 __xrt_bi_long_comp;
typedef int64  __xrt_bi_slong_comp;

#define __XRT_BI_COMP_RADIX       4294967296ULL
#define __XRT_BI_COMP_MAX         0xFFFFFFFFFFFFFFFFULL
#define __XRT_BI_COMP_BIT_SIZE    32
#define __XRT_BI_COMP_BYTE_SIZE   4
#define __XRT_BI_COMP_NUM_NIBBLES 8

#define __XRT_BI_M_OFFSET   0
#define __XRT_BI_NUM_MODS   1   // 验签仅需 1 个 modulus slot

#define __XRT_BI_PERMANENT  0x7FFF55AA

struct __xrt_bigint {
	struct __xrt_bigint *pNext;
	short iSize;
	short iMaxComps;
	int iRefs;
	__xrt_bi_comp *pComps;
};

struct __xrt_bi_ctx {
	struct __xrt_bigint *pActiveList;
	struct __xrt_bigint *pFreeList;
	struct __xrt_bigint *pMod[__XRT_BI_NUM_MODS];
	struct __xrt_bigint *pNormMod[__XRT_BI_NUM_MODS];
	struct __xrt_bigint **pG;            // sliding window 预计算表
	int iWindow;
	int iActiveCount;
	int iFreeCount;
	uint8 iModOffset;
};

// --- 大整数内存管理 ---

static struct __xrt_bigint* __xrt_bi_alloc(struct __xrt_bi_ctx *pCtx, int iCount)
{
	struct __xrt_bigint *pBi;

	// 先从 free list 找一个够大的
	if ( pCtx->pFreeList != NULL ) {
		pBi = pCtx->pFreeList;
		pCtx->pFreeList = pBi->pNext;
		pCtx->iFreeCount--;

		if ( pBi->iMaxComps < iCount ) {
			pBi->pComps = (__xrt_bi_comp*)xrtRealloc(pBi->pComps, iCount * __XRT_BI_COMP_BYTE_SIZE);
			pBi->iMaxComps = (short)iCount;
		}
	} else {
		pBi = (struct __xrt_bigint*)xrtMalloc(sizeof(struct __xrt_bigint));
		pBi->pComps = (__xrt_bi_comp*)xrtMalloc(iCount * __XRT_BI_COMP_BYTE_SIZE);
		pBi->iMaxComps = (short)iCount;
	}

	pBi->pNext = pCtx->pActiveList;
	pCtx->pActiveList = pBi;
	pCtx->iActiveCount++;
	pBi->iSize = (short)iCount;
	pBi->iRefs = 1;
	memset(pBi->pComps, 0, iCount * __XRT_BI_COMP_BYTE_SIZE);
	return pBi;
}

static struct __xrt_bigint* __xrt_bi_copy(struct __xrt_bigint *pBi)
{
	if ( pBi->iRefs != __XRT_BI_PERMANENT ) {
		pBi->iRefs++;
	}
	return pBi;
}

static void __xrt_bi_free(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pBi)
{
	struct __xrt_bigint *pPrev, *pCur;

	if ( pBi == NULL ) return;
	if ( pBi->iRefs == __XRT_BI_PERMANENT ) return;
	if ( --(pBi->iRefs) > 0 ) return;

	// 从 active list 移除
	pPrev = NULL;
	for ( pCur = pCtx->pActiveList; pCur != NULL; pCur = pCur->pNext ) {
		if ( pCur == pBi ) {
			if ( pPrev ) pPrev->pNext = pCur->pNext;
			else pCtx->pActiveList = pCur->pNext;
			pCtx->iActiveCount--;
			break;
		}
		pPrev = pCur;
	}

	// 加入 free list
	pBi->pNext = pCtx->pFreeList;
	pCtx->pFreeList = pBi;
	pCtx->iFreeCount++;
}

static struct __xrt_bi_ctx* __xrt_bi_initialize(void)
{
	struct __xrt_bi_ctx *pCtx = (struct __xrt_bi_ctx*)xrtCalloc(1, sizeof(struct __xrt_bi_ctx));
	return pCtx;
}

static void __xrt_bi_terminate(struct __xrt_bi_ctx *pCtx)
{
	struct __xrt_bigint *pBi, *pNext;

	// 释放 active list
	for ( pBi = pCtx->pActiveList; pBi != NULL; pBi = pNext ) {
		pNext = pBi->pNext;
		xrtFree(pBi->pComps);
		xrtFree(pBi);
	}

	// 释放 free list
	for ( pBi = pCtx->pFreeList; pBi != NULL; pBi = pNext ) {
		pNext = pBi->pNext;
		xrtFree(pBi->pComps);
		xrtFree(pBi);
	}

	xrtFree(pCtx);
}

static void __xrt_bi_permanent(struct __xrt_bigint *pBi)
{
	pBi->iRefs = __XRT_BI_PERMANENT;
}

static void __xrt_bi_depermanent(struct __xrt_bigint *pBi)
{
	pBi->iRefs = 1;
}

// 去除高位零
static struct __xrt_bigint* __xrt_bi_trim(struct __xrt_bigint *pBi)
{
	while ( (pBi->iSize > 1) && (pBi->pComps[pBi->iSize - 1] == 0) ) {
		pBi->iSize--;
	}
	return pBi;
}

static struct __xrt_bigint* __xrt_bi_clone(struct __xrt_bi_ctx *pCtx, const struct __xrt_bigint *pSrc)
{
	struct __xrt_bigint *pDst = __xrt_bi_alloc(pCtx, pSrc->iSize);
	memcpy(pDst->pComps, pSrc->pComps, pSrc->iSize * __XRT_BI_COMP_BYTE_SIZE);
	return pDst;
}

// --- 大整数导入导出 ---

static struct __xrt_bigint* __xrt_bi_import(struct __xrt_bi_ctx *pCtx, const uint8 *pData, int iLen)
{
	struct __xrt_bigint *pBi;
	int iNumComps = (iLen + __XRT_BI_COMP_BYTE_SIZE - 1) / __XRT_BI_COMP_BYTE_SIZE;
	int i, j, iOffset;

	pBi = __xrt_bi_alloc(pCtx, iNumComps);

	for ( i = iLen - 1, j = 0, iOffset = 0; i >= 0; i-- ) {
		pBi->pComps[iOffset] += (__xrt_bi_comp)pData[i] << (j * 8);
		if ( ++j == __XRT_BI_COMP_BYTE_SIZE ) {
			j = 0;
			iOffset++;
		}
	}

	return __xrt_bi_trim(pBi);
}

static void __xrt_bi_export(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pBi, uint8 *pData, int iSize)
{
	int i, j, k;
	(void)pCtx;

	memset(pData, 0, iSize);

	for ( i = 0, k = iSize - 1; i < pBi->iSize && k >= 0; i++ ) {
		__xrt_bi_comp v = pBi->pComps[i];
		for ( j = 0; j < __XRT_BI_COMP_BYTE_SIZE && k >= 0; j++ ) {
			pData[k--] = (uint8)(v & 0xFF);
			v >>= 8;
		}
	}

	__xrt_bi_free(pCtx, pBi);
}

static struct __xrt_bigint* __xrt_bi_int_to_bi(struct __xrt_bi_ctx *pCtx, __xrt_bi_comp iVal)
{
	struct __xrt_bigint *pBi = __xrt_bi_alloc(pCtx, 1);
	pBi->pComps[0] = iVal;
	return pBi;
}

// --- 大整数比较 ---

static int __xrt_bi_compare(struct __xrt_bigint *pA, struct __xrt_bigint *pB)
{
	int i;

	if ( pA->iSize > pB->iSize ) return 1;
	if ( pA->iSize < pB->iSize ) return -1;

	for ( i = pA->iSize - 1; i >= 0; i-- ) {
		if ( pA->pComps[i] > pB->pComps[i] ) return 1;
		if ( pA->pComps[i] < pB->pComps[i] ) return -1;
	}

	return 0;
}

// --- 大整数加法 ---

static struct __xrt_bigint* __xrt_bi_add(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pA, struct __xrt_bigint *pB)
{
	int iMaxSize = (pA->iSize > pB->iSize) ? pA->iSize : pB->iSize;
	struct __xrt_bigint *pR = __xrt_bi_alloc(pCtx, iMaxSize + 1);
	__xrt_bi_long_comp iCarry = 0;
	int i;

	for ( i = 0; i < iMaxSize; i++ ) {
		__xrt_bi_long_comp iSum = iCarry;
		if ( i < pA->iSize ) iSum += pA->pComps[i];
		if ( i < pB->iSize ) iSum += pB->pComps[i];
		pR->pComps[i] = (__xrt_bi_comp)iSum;
		iCarry = iSum >> __XRT_BI_COMP_BIT_SIZE;
	}

	pR->pComps[iMaxSize] = (__xrt_bi_comp)iCarry;

	__xrt_bi_free(pCtx, pA);
	__xrt_bi_free(pCtx, pB);
	return __xrt_bi_trim(pR);
}

// --- 大整数减法 (pA >= pB 时) ---

static struct __xrt_bigint* __xrt_bi_subtract(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pA, struct __xrt_bigint *pB, int *pIsNeg)
{
	int iMaxSize = (pA->iSize > pB->iSize) ? pA->iSize : pB->iSize;
	struct __xrt_bigint *pR = __xrt_bi_alloc(pCtx, iMaxSize);
	__xrt_bi_slong_comp iBorrow = 0;
	int i;

	if ( pIsNeg ) *pIsNeg = 0;

	for ( i = 0; i < iMaxSize; i++ ) {
		__xrt_bi_slong_comp iDiff = iBorrow;
		if ( i < pA->iSize ) iDiff += (__xrt_bi_slong_comp)pA->pComps[i];
		if ( i < pB->iSize ) iDiff -= (__xrt_bi_slong_comp)pB->pComps[i];

		if ( iDiff < 0 ) {
			pR->pComps[i] = (__xrt_bi_comp)(iDiff + (__xrt_bi_slong_comp)__XRT_BI_COMP_RADIX);
			iBorrow = -1;
		} else {
			pR->pComps[i] = (__xrt_bi_comp)iDiff;
			iBorrow = 0;
		}
	}

	if ( iBorrow < 0 && pIsNeg ) *pIsNeg = 1;

	__xrt_bi_free(pCtx, pA);
	__xrt_bi_free(pCtx, pB);
	return __xrt_bi_trim(pR);
}

// --- 大整数乘法 ---

static struct __xrt_bigint* __xrt_bi_multiply(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pA, struct __xrt_bigint *pB)
{
	int iRSize = pA->iSize + pB->iSize;
	struct __xrt_bigint *pR = __xrt_bi_alloc(pCtx, iRSize);
	int i, j;

	for ( i = 0; i < pA->iSize; i++ ) {
		__xrt_bi_long_comp iCarry = 0;
		__xrt_bi_comp iMand = pA->pComps[i];

		for ( j = 0; j < pB->iSize; j++ ) {
			__xrt_bi_long_comp iProd = (__xrt_bi_long_comp)iMand * pB->pComps[j]
				+ pR->pComps[i + j] + iCarry;
			pR->pComps[i + j] = (__xrt_bi_comp)iProd;
			iCarry = iProd >> __XRT_BI_COMP_BIT_SIZE;
		}

		pR->pComps[i + j] += (__xrt_bi_comp)iCarry;
	}

	__xrt_bi_free(pCtx, pA);
	__xrt_bi_free(pCtx, pB);
	return __xrt_bi_trim(pR);
}

// --- 大整数平方 ---

static struct __xrt_bigint* __xrt_bi_square(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pA)
{
	return __xrt_bi_multiply(pCtx, __xrt_bi_copy(pA), pA);
}

// --- 大整数除法/取模 (classical) ---

// 左移 n 个 component 位置 (乘以 RADIX^n)
static struct __xrt_bigint* __xrt_bi_comp_left_shift(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pBi, int iShift)
{
	int iNewSize = pBi->iSize + iShift;
	struct __xrt_bigint *pR = __xrt_bi_alloc(pCtx, iNewSize);
	memcpy(&pR->pComps[iShift], pBi->pComps, pBi->iSize * __XRT_BI_COMP_BYTE_SIZE);
	__xrt_bi_free(pCtx, pBi);
	return pR;
}

// 右移 n 个 component 位置 (除以 RADIX^n)
static struct __xrt_bigint* __xrt_bi_comp_right_shift(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pBi, int iShift)
{
	int iNewSize = pBi->iSize - iShift;
	if ( iNewSize <= 0 ) {
		__xrt_bi_free(pCtx, pBi);
		return __xrt_bi_int_to_bi(pCtx, 0);
	}
	{
		struct __xrt_bigint *pR = __xrt_bi_alloc(pCtx, iNewSize);
		memcpy(pR->pComps, &pBi->pComps[iShift], iNewSize * __XRT_BI_COMP_BYTE_SIZE);
		__xrt_bi_free(pCtx, pBi);
		return __xrt_bi_trim(pR);
	}
}

// 除法: pA / pM, is_mod=1 返回余数, is_mod=0 返回商
static struct __xrt_bigint* __xrt_bi_divide(struct __xrt_bi_ctx *pCtx,
	struct __xrt_bigint *pU, struct __xrt_bigint *pV, int bIsMod)
{
	int iCmp = __xrt_bi_compare(pU, pV);
	int n, m, j;
	struct __xrt_bigint *pQ, *pInnerV;
	__xrt_bi_comp d;

	if ( iCmp < 0 ) {
		if ( bIsMod ) {
			__xrt_bi_free(pCtx, pV);
			return pU;
		} else {
			__xrt_bi_free(pCtx, pU);
			__xrt_bi_free(pCtx, pV);
			return __xrt_bi_int_to_bi(pCtx, 0);
		}
	}

	if ( iCmp == 0 ) {
		__xrt_bi_free(pCtx, pU);
		__xrt_bi_free(pCtx, pV);
		return bIsMod ? __xrt_bi_int_to_bi(pCtx, 0) : __xrt_bi_int_to_bi(pCtx, 1);
	}

	// 单组件除数快速路径 (n=1, Knuth Algorithm D 要求 n>=2)
	if ( pV->iSize == 1 ) {
		__xrt_bi_comp iDenom = pV->pComps[0];
		__xrt_bi_long_comp iRem = 0;
		int i;

		pQ = __xrt_bi_alloc(pCtx, pU->iSize);
		for ( i = pU->iSize - 1; i >= 0; i-- ) {
			iRem = (iRem << __XRT_BI_COMP_BIT_SIZE) + pU->pComps[i];
			pQ->pComps[i] = (__xrt_bi_comp)(iRem / iDenom);
			iRem %= iDenom;
		}

		__xrt_bi_free(pCtx, pU);
		__xrt_bi_free(pCtx, pV);

		if ( bIsMod ) {
			__xrt_bi_free(pCtx, pQ);
			return __xrt_bi_int_to_bi(pCtx, (__xrt_bi_comp)iRem);
		} else {
			return __xrt_bi_trim(pQ);
		}
	}

	// Knuth's Algorithm D (n >= 2)
	n = pV->iSize;
	m = pU->iSize - n;

	pQ = __xrt_bi_alloc(pCtx, m + 1);

	// 正规化: 计算 d 使得 v[n-1] >= RADIX/2
	d = (__xrt_bi_comp)((__xrt_bi_long_comp)__XRT_BI_COMP_RADIX / ((__xrt_bi_long_comp)pV->pComps[n - 1] + 1));
	if ( d == 0 ) d = 1;

	if ( d > 1 ) {
		struct __xrt_bigint *pD = __xrt_bi_int_to_bi(pCtx, d);
		pU = __xrt_bi_multiply(pCtx, pU, __xrt_bi_copy(pD));
		pV = __xrt_bi_multiply(pCtx, pV, pD);
		// 正规化乘法可能改变位数，必须重新计算 n 和 m
		n = pV->iSize;
		m = pU->iSize - n;
		if ( m < 0 ) m = 0;
		// 重新分配商
		__xrt_bi_free(pCtx, pQ);
		pQ = __xrt_bi_alloc(pCtx, m + 1);
	}

	// 确保 pU 有足够的空间 (但保持正确的 iSize)
	if ( pU->iSize <= m + n ) {
		struct __xrt_bigint *pE = __xrt_bi_alloc(pCtx, m + n + 1);
		memcpy(pE->pComps, pU->pComps, pU->iSize * __XRT_BI_COMP_BYTE_SIZE);
		__xrt_bi_free(pCtx, pU);
		pU = __xrt_bi_trim(pE);
	}

	pInnerV = __xrt_bi_comp_left_shift(pCtx, __xrt_bi_copy(pV), m);

	if ( __xrt_bi_compare(pU, pInnerV) >= 0 ) {
		pQ->pComps[m] = 1;
		pU = __xrt_bi_subtract(pCtx, pU, pInnerV, NULL);
	} else {
		__xrt_bi_free(pCtx, pInnerV);
	}

	for ( j = m - 1; j >= 0; j-- ) {
		__xrt_bi_long_comp iQhat;
		struct __xrt_bigint *pQBi;
		struct __xrt_bigint *pProd;

		// 估算 q_hat
		if ( pU->iSize > j + n ) {
			iQhat = ((__xrt_bi_long_comp)pU->pComps[j + n] << __XRT_BI_COMP_BIT_SIZE)
				+ ((j + n - 1 < pU->iSize) ? pU->pComps[j + n - 1] : 0);
		} else {
			iQhat = (j + n - 1 < pU->iSize) ? pU->pComps[j + n - 1] : 0;
		}

		iQhat /= pV->pComps[n - 1];
		if ( iQhat >= (__xrt_bi_long_comp)__XRT_BI_COMP_RADIX ) {
			iQhat = __XRT_BI_COMP_RADIX - 1;
		}

		pQBi = __xrt_bi_int_to_bi(pCtx, (__xrt_bi_comp)iQhat);
		pProd = __xrt_bi_multiply(pCtx, pQBi, __xrt_bi_copy(pV));
		pProd = __xrt_bi_comp_left_shift(pCtx, pProd, j);

		// 修正: 先比较, 避免负数减法导致二进制补码增长
		while ( __xrt_bi_compare(pU, pProd) < 0 ) {
			struct __xrt_bigint *pFix = __xrt_bi_comp_left_shift(pCtx, __xrt_bi_copy(pV), j);
			pU = __xrt_bi_add(pCtx, pU, pFix);
			iQhat--;
		}
		pU = __xrt_bi_subtract(pCtx, pU, pProd, NULL);

		pQ->pComps[j] = (__xrt_bi_comp)iQhat;
	}

	__xrt_bi_free(pCtx, pV);

	if ( bIsMod ) {
		__xrt_bi_free(pCtx, pQ);
		if ( d > 1 ) {
			struct __xrt_bigint *pD2 = __xrt_bi_int_to_bi(pCtx, d);
			pU = __xrt_bi_divide(pCtx, pU, pD2, 0);  // 去正规化
		}
		return __xrt_bi_trim(pU);
	} else {
		__xrt_bi_free(pCtx, pU);
		return __xrt_bi_trim(pQ);
	}
}

// bi_mod 宏: 对已设置的 modulus 取模
#define __xrt_bi_mod(ctx, bi) __xrt_bi_divide(ctx, bi, __xrt_bi_copy((ctx)->pMod[(ctx)->iModOffset]), 1)

static void __xrt_bi_set_mod(struct __xrt_bi_ctx *pCtx, struct __xrt_bigint *pMod, int iModOffset)
{
	pCtx->pMod[iModOffset] = pMod;
	__xrt_bi_permanent(pMod);
	pCtx->iModOffset = (uint8)iModOffset;

	// 保存正规化副本
	pCtx->pNormMod[iModOffset] = __xrt_bi_clone(pCtx, pMod);
	__xrt_bi_permanent(pCtx->pNormMod[iModOffset]);
}

static void __xrt_bi_free_mod(struct __xrt_bi_ctx *pCtx, int iModOffset)
{
	if ( pCtx->pMod[iModOffset] ) {
		__xrt_bi_depermanent(pCtx->pMod[iModOffset]);
		__xrt_bi_free(pCtx, pCtx->pMod[iModOffset]);
		pCtx->pMod[iModOffset] = NULL;
	}
	if ( pCtx->pNormMod[iModOffset] ) {
		__xrt_bi_depermanent(pCtx->pNormMod[iModOffset]);
		__xrt_bi_free(pCtx, pCtx->pNormMod[iModOffset]);
		pCtx->pNormMod[iModOffset] = NULL;
	}
}

// --- 模幂运算 (sliding window) ---

static struct __xrt_bigint* __xrt_bi_mod_power(struct __xrt_bi_ctx *pCtx,
	struct __xrt_bigint *pBase, struct __xrt_bigint *pExp)
{
	int i, j;
	int iNumBits;
	struct __xrt_bigint *pResult;

	// 确定指数的位数
	{
		__xrt_bi_comp iTopComp = pExp->pComps[pExp->iSize - 1];
		iNumBits = (pExp->iSize - 1) * __XRT_BI_COMP_BIT_SIZE;
		while ( iTopComp ) {
			iNumBits++;
			iTopComp >>= 1;
		}
	}

	// 简单的 square-and-multiply (right-to-left 二进制法)
	pResult = __xrt_bi_clone(pCtx, pBase);

	// 对 pBase 取模
	pResult = __xrt_bi_mod(pCtx, pResult);

	// 存储 base mod m
	{
		struct __xrt_bigint *pBaseMod = __xrt_bi_clone(pCtx, pResult);
		struct __xrt_bigint *pR = __xrt_bi_int_to_bi(pCtx, 1);

		for ( i = iNumBits - 1; i >= 0; i-- ) {
			int iBitIdx = i % __XRT_BI_COMP_BIT_SIZE;
			int iCompIdx = i / __XRT_BI_COMP_BIT_SIZE;

			// r = r * r mod m
			pR = __xrt_bi_square(pCtx, pR);
			pR = __xrt_bi_mod(pCtx, pR);

			// 如果 exp 的第 i 位为 1: r = r * base mod m
			if ( (iCompIdx < pExp->iSize) && (pExp->pComps[iCompIdx] & ((__xrt_bi_comp)1 << iBitIdx)) ) {
				pR = __xrt_bi_multiply(pCtx, pR, __xrt_bi_copy(pBaseMod));
				pR = __xrt_bi_mod(pCtx, pR);
			}
		}

		__xrt_bi_free(pCtx, pBaseMod);
		__xrt_bi_free(pCtx, pResult);
		__xrt_bi_free(pCtx, pExp);
		__xrt_bi_free(pCtx, pBase);
		return pR;
	}
}

// --- 公共 API: RSA 模幂运算 ---

XXAPI int xrtRSAModPow(const uint8 *pMod, size_t iModSz,
	const uint8 *pExp, size_t iExpSz,
	const uint8 *pMsg, size_t iMsgSz,
	uint8 *pOut, size_t iOutSz)
{
	struct __xrt_bi_ctx *pCtx = __xrt_bi_initialize();
	struct __xrt_bigint *pBiMod, *pBiExp, *pBiMsg, *pBiResult;

	if ( !pCtx ) return -1;

	pBiMod = __xrt_bi_import(pCtx, pMod, (int)iModSz);
	pBiExp = __xrt_bi_import(pCtx, pExp, (int)iExpSz);
	pBiMsg = __xrt_bi_import(pCtx, pMsg, (int)iMsgSz);

	__xrt_bi_set_mod(pCtx, pBiMod, __XRT_BI_M_OFFSET);
	pBiResult = __xrt_bi_mod_power(pCtx, pBiMsg, pBiExp);
	__xrt_bi_export(pCtx, pBiResult, pOut, (int)iOutSz);

	__xrt_bi_free_mod(pCtx, __XRT_BI_M_OFFSET);
	__xrt_bi_terminate(pCtx);

	return 0;
}



/* ============================== RSA-PSS 签名验证 (RFC 8017) ============================== */

// MGF1 掩码生成函数 (RFC 8017 B.2.1, 基于 SHA-256)
static void __xrt_mgf1_sha256(uint8 *pOut, size_t iOutLen, const uint8 *pSeed, size_t iSeedLen)
{
	uint8 aCounter[4];
	uint8 aHash[32];
	size_t iOffset = 0;
	uint32 iCtr = 0;

	while ( iOffset < iOutLen ) {
		xsha256_ctx tCtx;
		size_t iCopyLen;

		aCounter[0] = (uint8)(iCtr >> 24);
		aCounter[1] = (uint8)(iCtr >> 16);
		aCounter[2] = (uint8)(iCtr >> 8);
		aCounter[3] = (uint8)(iCtr);

		xrtSHA256Init(&tCtx);
		xrtSHA256Update(&tCtx, (ptr)pSeed, iSeedLen);
		xrtSHA256Update(&tCtx, aCounter, 4);
		xrtSHA256Final(&tCtx, aHash);

		iCopyLen = iOutLen - iOffset;
		if ( iCopyLen > 32 ) iCopyLen = 32;
		memcpy(pOut + iOffset, aHash, iCopyLen);

		iOffset += iCopyLen;
		iCtr++;
	}
}

// EMSA-PSS-VERIFY (RFC 8017 Section 9.1.2)
// pHash: 被签名的消息哈希 (SHA-256, 32 bytes)
// pEM: RSA 解密后的编码消息
// iEMLen: EM 长度 (等于 RSA modulus 字节数)
// iSaltLen: 盐长度 (通常等于 hash 长度 = 32)
static bool __xrt_emsa_pss_verify(const uint8 *pHash, size_t iHashLen,
	const uint8 *pEM, size_t iEMLen, size_t iSaltLen)
{
	size_t iDBLen;
	size_t i;
	const uint8 *pH;
	uint8 *pDBMask;
	uint8 *pDB;
	uint8 aHP[32];  // H'
	xsha256_ctx tCtx;
	uint8 iDiff;
	const uint8 aPad8[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	// 检查 EM 最后一个字节为 0xbc
	if ( iEMLen < iHashLen + iSaltLen + 2 ) return false;
	if ( pEM[iEMLen - 1] != 0xbc ) return false;

	// 分离 maskedDB 和 H
	iDBLen = iEMLen - iHashLen - 1;  // maskedDB 长度
	pH = pEM + iDBLen;               // H 起始位置

	// 检查高位为零 (emBits = 8*emLen - 1 的情况)
	if ( pEM[0] & 0x80 ) return false;

	// 生成 dbMask = MGF1(H, dbLen)
	pDBMask = (uint8*)xrtMalloc(iDBLen);
	pDB = (uint8*)xrtMalloc(iDBLen);
	if ( !pDBMask || !pDB ) {
		if ( pDBMask ) xrtFree(pDBMask);
		if ( pDB ) xrtFree(pDB);
		return false;
	}

	__xrt_mgf1_sha256(pDBMask, iDBLen, pH, iHashLen);

	// DB = maskedDB XOR dbMask
	for ( i = 0; i < iDBLen; i++ ) {
		pDB[i] = pEM[i] ^ pDBMask[i];
	}

	// 清除 DB 的最高位
	pDB[0] &= 0x7f;

	// 验证 DB 格式: 0x00...00 || 0x01 || salt
	{
		size_t iPadLen = iDBLen - iSaltLen - 1;
		for ( i = 0; i < iPadLen; i++ ) {
			if ( pDB[i] != 0x00 ) {
				xrtFree(pDBMask);
				xrtFree(pDB);
				return false;
			}
		}
		if ( pDB[iPadLen] != 0x01 ) {
			xrtFree(pDBMask);
			xrtFree(pDB);
			return false;
		}
	}

	// H' = SHA-256(0x00*8 || mHash || salt)
	xrtSHA256Init(&tCtx);
	xrtSHA256Update(&tCtx, (ptr)aPad8, 8);
	xrtSHA256Update(&tCtx, (ptr)pHash, iHashLen);
	xrtSHA256Update(&tCtx, (ptr)(pDB + iDBLen - iSaltLen), iSaltLen);
	xrtSHA256Final(&tCtx, aHP);

	// 常量时间比较 H == H'
	iDiff = 0;
	for ( i = 0; i < iHashLen; i++ ) {
		iDiff |= pH[i] ^ aHP[i];
	}

	xrtFree(pDBMask);
	xrtFree(pDB);

	return (iDiff == 0);
}

// RSA-PSS-RSAE-SHA256 签名验证
XXAPI bool xrtRSAPSSVerify(const uint8 *pHash, size_t iHashLen,
	const uint8 *pSig, size_t iSigLen,
	const uint8 *pMod, size_t iModSz,
	const uint8 *pExp, size_t iExpSz)
{
	uint8 *pEM;
	bool bResult;

	if ( (iHashLen != 32) || (iSigLen != iModSz) ) return false;

	// 分配 EM 缓冲区
	pEM = (uint8*)xrtMalloc(iModSz);
	if ( !pEM ) return false;

	// RSA 原始操作: EM = sig^e mod n
	if ( xrtRSAModPow(pMod, iModSz, pExp, iExpSz, pSig, iSigLen, pEM, iModSz) != 0 ) {
		xrtFree(pEM);
		return false;
	}

	// EMSA-PSS-VERIFY
	bResult = __xrt_emsa_pss_verify(pHash, iHashLen, pEM, iModSz, iHashLen);

	xrtFree(pEM);
	return bResult;
}



/* ============================== RSA PKCS#1 v1.5 签名验证 (RFC 8017) ============================== */

// DigestInfo DER 前缀 (RFC 8017 Section 9.2 Note 1)
// SHA-256: 30 31 30 0d 06 09 60 86 48 01 65 03 04 02 01 05 00 04 20
static const uint8 __xrt_pkcs1_sha256_prefix[] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};
// SHA-384: 30 41 30 0d 06 09 60 86 48 01 65 03 04 02 02 05 00 04 30
static const uint8 __xrt_pkcs1_sha384_prefix[] = {
	0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
	0x00, 0x04, 0x30
};
// SHA-512: 30 51 30 0d 06 09 60 86 48 01 65 03 04 02 03 05 00 04 40
static const uint8 __xrt_pkcs1_sha512_prefix[] = {
	0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
	0x00, 0x04, 0x40
};

// EMSA-PKCS1-v1_5 验证 (RFC 8017 Section 9.2)
// 格式: 0x00 || 0x01 || PS (0xFF padding) || 0x00 || DigestInfo
XXAPI bool xrtRSAPKCS1Verify(const uint8 *pHash, size_t iHashLen,
	const uint8 *pSig, size_t iSigLen,
	const uint8 *pMod, size_t iModSz,
	const uint8 *pExp, size_t iExpSz)
{
	uint8 *pEM;
	const uint8 *pPrefix;
	size_t iPrefixLen;
	size_t iPos;
	size_t iTLen;
	uint8 iDiff;
	size_t i;
	
	if ( (iSigLen != iModSz) || (iModSz < 11) ) return false;
	
	// 确定 DigestInfo 前缀
	if ( iHashLen == 32 ) {
		pPrefix = __xrt_pkcs1_sha256_prefix;
		iPrefixLen = sizeof(__xrt_pkcs1_sha256_prefix);
	} else if ( iHashLen == 48 ) {
		pPrefix = __xrt_pkcs1_sha384_prefix;
		iPrefixLen = sizeof(__xrt_pkcs1_sha384_prefix);
	} else if ( iHashLen == 64 ) {
		pPrefix = __xrt_pkcs1_sha512_prefix;
		iPrefixLen = sizeof(__xrt_pkcs1_sha512_prefix);
	} else {
		return false;
	}
	
	iTLen = iPrefixLen + iHashLen;  // DigestInfo 总长度
	if ( iModSz < iTLen + 11 ) return false;  // 至少需要 0x00+0x01+8字节PS+0x00+DigestInfo
	
	// RSA 解密: EM = sig^e mod n
	pEM = (uint8*)xrtMalloc(iModSz);
	if ( !pEM ) return false;
	
	if ( xrtRSAModPow(pMod, iModSz, pExp, iExpSz, pSig, iSigLen, pEM, iModSz) != 0 ) {
		xrtFree(pEM);
		return false;
	}
	
	// 验证格式: 0x00 || 0x01 || PS || 0x00 || DigestInfo
	iPos = 0;
	if ( pEM[iPos++] != 0x00 ) { xrtFree(pEM); return false; }
	if ( pEM[iPos++] != 0x01 ) { xrtFree(pEM); return false; }
	
	// PS: 至少 8 字节的 0xFF
	{
		size_t iPsLen = iModSz - 3 - iTLen;
		for ( i = 0; i < iPsLen; i++ ) {
			if ( pEM[iPos++] != 0xFF ) { xrtFree(pEM); return false; }
		}
	}
	if ( pEM[iPos++] != 0x00 ) { xrtFree(pEM); return false; }
	
	// 比较 DigestInfo 前缀
	if ( memcmp(pEM + iPos, pPrefix, iPrefixLen) != 0 ) {
		xrtFree(pEM);
		return false;
	}
	iPos += iPrefixLen;
	
	// 常量时间比较哈希值
	iDiff = 0;
	for ( i = 0; i < iHashLen; i++ ) {
		iDiff |= pEM[iPos + i] ^ pHash[i];
	}
	
	xrtFree(pEM);
	return (iDiff == 0);
}



/* ============================== secp256r1 (NIST P-256) ECDH + ECDSA ============================== */

/*
	secp256r1 (P-256) 椭圆曲线: ECDH 密钥交换 + ECDSA 签名验证
	参考: NIST FIPS 186-4, SEC 2 v2
	
	内部表示: uint32[8], 小端字序 ([0]=最低 32-bit)
	p  = FFFFFFFF 00000001 00000000 00000000 00000000 FFFFFFFF FFFFFFFF FFFFFFFF
	n  = FFFFFFFF 00000000 FFFFFFFF FFFFFFFF BCE6FAAD A7179E84 F3B9CAC2 FC632551
	Gx = 6B17D1F2 E12C4247 F8BCE6E5 63A440F2 77037D81 2DEB33A0 F4A13945 D898C296
	Gy = 4FE342E2 FE1A7F9B 8EE7EB4A 7C0F9E16 2BCE3357 6B315ECE CBB64068 37BF51F5
	a  = -3 (mod p)
*/

/* ---- 256-bit 整数基本运算 (uint32[8], 小端字序) ---- */

static void __xrt_u256_from_be(uint32 *r, const uint8 *b)
{
	int i;
	for ( i = 0; i < 8; i++ ) {
		r[7 - i] = ((uint32)b[i*4] << 24) | ((uint32)b[i*4+1] << 16) |
		           ((uint32)b[i*4+2] << 8)  | (uint32)b[i*4+3];
	}
}

static void __xrt_u256_to_be(uint8 *b, const uint32 *a)
{
	int i;
	for ( i = 0; i < 8; i++ ) {
		b[i*4]   = (uint8)(a[7-i] >> 24);
		b[i*4+1] = (uint8)(a[7-i] >> 16);
		b[i*4+2] = (uint8)(a[7-i] >> 8);
		b[i*4+3] = (uint8)(a[7-i]);
	}
}

static void __xrt_u256_copy(uint32 *r, const uint32 *a) { memcpy(r, a, 32); }
static void __xrt_u256_zero(uint32 *r) { memset(r, 0, 32); }

static int __xrt_u256_is_zero(const uint32 *a)
{
	uint32 v = 0;
	int i;
	for ( i = 0; i < 8; i++ ) v |= a[i];
	return v == 0;
}

static uint32 __xrt_u256_add(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint64 c = 0;
	int i;
	for ( i = 0; i < 8; i++ ) {
		c += (uint64)a[i] + b[i];
		r[i] = (uint32)c;
		c >>= 32;
	}
	return (uint32)c;
}

static uint32 __xrt_u256_sub(uint32 *r, const uint32 *a, const uint32 *b)
{
	int64 c = 0;
	int i;
	for ( i = 0; i < 8; i++ ) {
		c += (int64)(uint64)a[i] - (int64)(uint64)b[i];
		r[i] = (uint32)c;
		c >>= 32;
	}
	return (c < 0) ? 1 : 0;  // 1 = borrow
}

// 返回 1 如果 a >= b, 0 如果 a < b
static int __xrt_u256_gte(const uint32 *a, const uint32 *b)
{
	int i;
	for ( i = 7; i >= 0; i-- ) {
		if ( a[i] > b[i] ) return 1;
		if ( a[i] < b[i] ) return 0;
	}
	return 1; // 相等
}

static int __xrt_u256_cmp(const uint32 *a, const uint32 *b)
{
	int i;
	for ( i = 7; i >= 0; i-- ) {
		if ( a[i] > b[i] ) return 1;
		if ( a[i] < b[i] ) return -1;
	}
	return 0;
}

/* ---- P-256 曲线参数 (uint32[8] 小端字序) ---- */

// p = FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF
static const uint32 __xrt_p256_P[8] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
	0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF
};

// n = FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
static const uint32 __xrt_p256_N[8] = {
	0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD,
	0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF
};

// Gx
static const uint32 __xrt_p256_Gx[8] = {
	0xD898C296, 0xF4A13945, 0x2DEB33A0, 0x77037D81,
	0x63A440F2, 0xF8BCE6E5, 0xE12C4247, 0x6B17D1F2
};

// Gy
static const uint32 __xrt_p256_Gy[8] = {
	0x37BF51F5, 0xCBB64068, 0x6B315ECE, 0x2BCE3357,
	0x7C0F9E16, 0x8EE7EB4A, 0xFE1A7F9B, 0x4FE342E2
};

/* ---- NIST P-256 模约简 ---- */

/*
	512-bit 乘法结果 c[0..15] 对 p 取模
	使用 2^256 mod p = (2^256 - p) 进行迭代约简
	2^256 - p = 2^224 - 2^192 - 2^96 + 1
*/

// 256x256 -> 512 bit 乘法
static void __xrt_u256_mul512(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint64 t[16];
	int i, j;
	memset(t, 0, sizeof(t));
	for ( i = 0; i < 8; i++ ) {
		uint64 carry = 0;
		for ( j = 0; j < 8; j++ ) {
			uint64 uv = t[i+j] + (uint64)a[i] * b[j] + carry;
			t[i+j] = uv & 0xFFFFFFFF;
			carry = uv >> 32;
		}
		t[i+8] = carry;
	}
	for ( i = 0; i < 16; i++ ) r[i] = (uint32)t[i];
}

// 2^256 - p 的 uint32[8] 表示 (硬编码, 避免运行时计算错误)
static const uint32 __xrt_p256_C[8] = {
	0x00000001, 0x00000000, 0x00000000, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0x00000000
};

static void __xrt_p256_mod_p(uint32 *r, const uint32 *c)
{
	// NIST FIPS 186-4, Section D.2: Fast reduction for P-256
	// p = 2^256 - 2^224 + 2^192 + 2^96 - 1
	// Input: c[0..15] (512-bit), Output: r[0..7] (256-bit, reduced mod p)
	int64 acc[9] = {0};  // 9 words to handle overflow
	int i;
	
	// s1 = T: (c7, c6, c5, c4, c3, c2, c1, c0)
	for (i = 0; i < 8; i++) acc[i] = (int64)(uint64)c[i];
	
	// +2*s2: 2*(c15, c14, c13, c12, c11, 0, 0, 0)
	acc[3] += 2*(int64)(uint64)c[11];
	acc[4] += 2*(int64)(uint64)c[12];
	acc[5] += 2*(int64)(uint64)c[13];
	acc[6] += 2*(int64)(uint64)c[14];
	acc[7] += 2*(int64)(uint64)c[15];
	
	// +2*s3: 2*(0, c15, c14, c13, c12, 0, 0, 0)
	acc[3] += 2*(int64)(uint64)c[12];
	acc[4] += 2*(int64)(uint64)c[13];
	acc[5] += 2*(int64)(uint64)c[14];
	acc[6] += 2*(int64)(uint64)c[15];
	
	// +s4: (c15, c14, 0, 0, 0, c10, c9, c8)
	acc[0] += (int64)(uint64)c[8];
	acc[1] += (int64)(uint64)c[9];
	acc[2] += (int64)(uint64)c[10];
	acc[6] += (int64)(uint64)c[14];
	acc[7] += (int64)(uint64)c[15];
	
	// +s5: (c8, c13, c15, c14, c13, c11, c10, c9)
	acc[0] += (int64)(uint64)c[9];
	acc[1] += (int64)(uint64)c[10];
	acc[2] += (int64)(uint64)c[11];
	acc[3] += (int64)(uint64)c[13];
	acc[4] += (int64)(uint64)c[14];
	acc[5] += (int64)(uint64)c[15];
	acc[6] += (int64)(uint64)c[13];
	acc[7] += (int64)(uint64)c[8];
	
	// -s6: (c10, c8, 0, 0, 0, c13, c12, c11)
	acc[0] -= (int64)(uint64)c[11];
	acc[1] -= (int64)(uint64)c[12];
	acc[2] -= (int64)(uint64)c[13];
	acc[6] -= (int64)(uint64)c[8];
	acc[7] -= (int64)(uint64)c[10];
	
	// -s7: (c11, c9, 0, 0, c15, c14, c13, c12)
	acc[0] -= (int64)(uint64)c[12];
	acc[1] -= (int64)(uint64)c[13];
	acc[2] -= (int64)(uint64)c[14];
	acc[3] -= (int64)(uint64)c[15];
	acc[6] -= (int64)(uint64)c[9];
	acc[7] -= (int64)(uint64)c[11];
	
	// -s8: (c12, 0, c10, c9, c8, c15, c14, c13)
	acc[0] -= (int64)(uint64)c[13];
	acc[1] -= (int64)(uint64)c[14];
	acc[2] -= (int64)(uint64)c[15];
	acc[3] -= (int64)(uint64)c[8];
	acc[4] -= (int64)(uint64)c[9];
	acc[5] -= (int64)(uint64)c[10];
	acc[7] -= (int64)(uint64)c[12];
	
	// -s9: (c13, 0, c11, c10, c9, 0, c15, c14)
	acc[0] -= (int64)(uint64)c[14];
	acc[1] -= (int64)(uint64)c[15];
	acc[3] -= (int64)(uint64)c[9];
	acc[4] -= (int64)(uint64)c[10];
	acc[5] -= (int64)(uint64)c[11];
	acc[7] -= (int64)(uint64)c[13];
	
	// Carry propagation (signed)
	for (i = 0; i < 8; i++) {
		acc[i+1] += (acc[i] >> 32);
		r[i] = (uint32)((uint64)acc[i] & 0xFFFFFFFF);
	}
	
	// acc[8] is the residual carry (can be positive or negative)
	int64 carry = acc[8];
	while ( carry < 0 ) {
		__xrt_u256_add(r, r, __xrt_p256_P);
		carry++;
	}
	while ( carry > 0 ) {
		__xrt_u256_sub(r, r, __xrt_p256_P);
		carry--;
	}
	while ( __xrt_u256_gte(r, __xrt_p256_P) ) {
		__xrt_u256_sub(r, r, __xrt_p256_P);
	}
}

/* ---- 模 p 算术 ---- */

static void __xrt_p256_add_mod_p(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint32 carry = __xrt_u256_add(r, a, b);
	if ( carry || __xrt_u256_gte(r, __xrt_p256_P) ) {
		__xrt_u256_sub(r, r, __xrt_p256_P);
	}
}

static void __xrt_p256_sub_mod_p(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint32 borrow = __xrt_u256_sub(r, a, b);
	if ( borrow ) {
		__xrt_u256_add(r, r, __xrt_p256_P);
	}
}

static void __xrt_p256_mul_mod_p(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint32 t[16];
	__xrt_u256_mul512(t, a, b);
	__xrt_p256_mod_p(r, t);
}

static void __xrt_p256_sqr_mod_p(uint32 *r, const uint32 *a)
{
	__xrt_p256_mul_mod_p(r, a, a);
}

// 模 p 逆元: r = a^(p-2) mod p (费马小定理)
static void __xrt_p256_inv_mod_p(uint32 *r, const uint32 *a)
{
	uint32 exp[8], base[8], result[8];
	int bit;
	
	// exp = p - 2
	__xrt_u256_copy(exp, __xrt_p256_P);
	exp[0] -= 2;
	__xrt_u256_copy(base, a);
	__xrt_u256_zero(result);
	result[0] = 1;
	
	for ( bit = 0; bit < 256; bit++ ) {
		if ( exp[bit / 32] & ((uint32)1 << (bit % 32)) ) {
			__xrt_p256_mul_mod_p(result, result, base);
		}
		__xrt_p256_sqr_mod_p(base, base);
	}
	__xrt_u256_copy(r, result);
}

/* ---- 模 n (曲线阶) 算术 ---- */

// 512-bit 对 n 取模 (通用方法, n 没有特殊结构)
static void __xrt_p256_mod_n(uint32 *r, const uint32 *c)
{
	// 使用 2^256 mod n = 2^256 - n 进行约简
	uint32 hi[8], c256[8], prod[16];
	uint32 carry = 0;
	int i;
	uint32 hi_nz = 0;
	
	for ( i = 0; i < 8; i++ ) r[i] = c[i];
	for ( i = 0; i < 8; i++ ) { hi[i] = c[i+8]; hi_nz |= hi[i]; }
	
	// 计算 c256 = 2^256 - n
	__xrt_u256_zero(c256);
	__xrt_u256_sub(c256, c256, __xrt_p256_N);  // c256 = 0 - n (mod 2^256) = 2^256 - n
	
	// 迭代约简 (与 mod_p 相同的逻辑)
	while ( hi_nz ) {
		__xrt_u256_mul512(prod, hi, c256);
		carry += __xrt_u256_add(r, r, prod);
		hi_nz = 0;
		for ( i = 0; i < 8; i++ ) { hi[i] = prod[i+8]; hi_nz |= hi[i]; }
	}
	// 最后确保 r < n
	while ( carry ) {
		uint32 borrow = __xrt_u256_sub(r, r, __xrt_p256_N);
		if ( borrow ) carry--;
	}
	while ( __xrt_u256_gte(r, __xrt_p256_N) ) {
		__xrt_u256_sub(r, r, __xrt_p256_N);
	}
}

static void __xrt_p256_mul_mod_n(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint32 t[16];
	__xrt_u256_mul512(t, a, b);
	__xrt_p256_mod_n(r, t);
}

static void __xrt_p256_add_mod_n(uint32 *r, const uint32 *a, const uint32 *b)
{
	uint32 carry = __xrt_u256_add(r, a, b);
	if ( carry || __xrt_u256_gte(r, __xrt_p256_N) ) {
		__xrt_u256_sub(r, r, __xrt_p256_N);
	}
}

static void __xrt_p256_inv_mod_n(uint32 *r, const uint32 *a)
{
	uint32 exp[8], base[8], result[8];
	int bit;
	
	__xrt_u256_copy(exp, __xrt_p256_N);
	exp[0] -= 2;  // n - 2
	__xrt_u256_copy(base, a);
	__xrt_u256_zero(result);
	result[0] = 1;
	
	for ( bit = 0; bit < 256; bit++ ) {
		if ( exp[bit / 32] & ((uint32)1 << (bit % 32)) ) {
			__xrt_p256_mul_mod_n(result, result, base);
		}
		__xrt_p256_mul_mod_n(base, base, base);
	}
	__xrt_u256_copy(r, result);
}

/* ---- Jacobian 坐标点运算 ---- */

// Jacobian 坐标点 (X, Y, Z)
// 仿射坐标 x = X/Z^2, y = Y/Z^3
// 无穷远点表示为 Z=0
typedef struct {
	uint32 x[8];
	uint32 y[8];
	uint32 z[8];
} __xrt_p256_jpt;

// 点倍乘: R = 2*P
// 安全支持 pR == pP (就地操作)
// 算法: 完整公式 (a=-3 优化)
// M = 3*(X + Z^2)*(X - Z^2)
// S = 4*X*Y^2
// X' = M^2 - 2*S
// Y' = M*(S - X') - 8*Y^4
// Z' = 2*Y*Z
static void __xrt_p256_pt_dbl(__xrt_p256_jpt *pR, const __xrt_p256_jpt *pP)
{
	uint32 m[8], s[8], t1[8], t2[8], y2[8], z_save[8], y_save[8];
	
	if ( __xrt_u256_is_zero(pP->z) ) {
		memset(pR, 0, sizeof(*pR));
		return;
	}
	
	// 保存原始 Y, Z (防止 aliasing)
	__xrt_u256_copy(y_save, pP->y);
	__xrt_u256_copy(z_save, pP->z);
	
	// Y^2
	__xrt_p256_sqr_mod_p(y2, y_save);
	
	// S = 4*X*Y^2
	__xrt_p256_mul_mod_p(s, pP->x, y2);
	__xrt_p256_add_mod_p(s, s, s);
	__xrt_p256_add_mod_p(s, s, s);
	
	// M = 3*(X + Z^2)*(X - Z^2) = 3*X^2 + a*Z^4, a=-3
	__xrt_p256_sqr_mod_p(t1, z_save);       // Z^2
	__xrt_p256_add_mod_p(t2, pP->x, t1);   // X + Z^2
	__xrt_p256_sub_mod_p(t1, pP->x, t1);   // X - Z^2
	__xrt_p256_mul_mod_p(m, t2, t1);        // (X+Z^2)(X-Z^2)
	__xrt_p256_add_mod_p(t1, m, m);          // 2*(...)
	__xrt_p256_add_mod_p(m, m, t1);          // 3*(...)
	
	// 8*Y^4
	__xrt_p256_sqr_mod_p(t1, y2);           // Y^4
	__xrt_p256_add_mod_p(t1, t1, t1);       // 2*Y^4
	__xrt_p256_add_mod_p(t1, t1, t1);       // 4*Y^4
	__xrt_p256_add_mod_p(t1, t1, t1);       // 8*Y^4
	
	// Z' = 2*Y*Z (必须在写入 pR 之前计算)
	__xrt_p256_mul_mod_p(pR->z, y_save, z_save);
	__xrt_p256_add_mod_p(pR->z, pR->z, pR->z);
	
	// X' = M^2 - 2*S
	__xrt_p256_sqr_mod_p(pR->x, m);
	__xrt_p256_sub_mod_p(pR->x, pR->x, s);
	__xrt_p256_sub_mod_p(pR->x, pR->x, s);
	
	// Y' = M*(S - X') - 8*Y^4
	__xrt_p256_sub_mod_p(t2, s, pR->x);
	__xrt_p256_mul_mod_p(pR->y, m, t2);
	__xrt_p256_sub_mod_p(pR->y, pR->y, t1);
}

// 混合点加法: R = P + Q
// P: Jacobian, Q: 仿射 (qx, qy), 即 Q 的 Z=1
// 安全支持 pR == pP
static void __xrt_p256_pt_add_aff(__xrt_p256_jpt *pR, const __xrt_p256_jpt *pP, const uint32 *qx, const uint32 *qy)
{
	uint32 u1[8], u2[8], s1[8], s2[8], h[8], hh[8], hhh[8], rr[8], t[8];
	__xrt_p256_jpt P_save;
	
	if ( __xrt_u256_is_zero(pP->z) ) {
		__xrt_u256_copy(pR->x, qx);
		__xrt_u256_copy(pR->y, qy);
		__xrt_u256_zero(pR->z);
		pR->z[0] = 1;
		return;
	}
	
	// 保存 P (防止 pR == pP 时的 aliasing)
	memcpy(&P_save, pP, sizeof(P_save));
	
	// U1 = X1, U2 = X2*Z1^2, S1 = Y1, S2 = Y2*Z1^3
	__xrt_p256_sqr_mod_p(t, P_save.z);       // Z1^2
	__xrt_p256_mul_mod_p(u2, qx, t);         // X2*Z1^2
	__xrt_u256_copy(u1, P_save.x);
	
	__xrt_p256_mul_mod_p(s2, t, P_save.z);   // Z1^3
	__xrt_p256_mul_mod_p(s2, s2, qy);        // Y2*Z1^3
	__xrt_u256_copy(s1, P_save.y);
	
	// H = U2 - U1, R = S2 - S1
	__xrt_p256_sub_mod_p(h, u2, u1);
	__xrt_p256_sub_mod_p(rr, s2, s1);
	
	if ( __xrt_u256_is_zero(h) ) {
		#ifdef DEBUG_TRACE
		printf("  [P256] pt_add_aff: H==0! rr_zero=%d\n", __xrt_u256_is_zero(rr));
		#endif
		if ( __xrt_u256_is_zero(rr) ) {
			__xrt_p256_pt_dbl(pR, &P_save);
		} else {
			memset(pR, 0, sizeof(*pR)); // 无穷远点
		}
		return;
	}
	
	__xrt_p256_sqr_mod_p(hh, h);              // H^2
	__xrt_p256_mul_mod_p(hhh, hh, h);         // H^3
	__xrt_p256_mul_mod_p(t, u1, hh);          // U1*H^2
	
	// X3 = R^2 - H^3 - 2*U1*H^2
	__xrt_p256_sqr_mod_p(pR->x, rr);
	__xrt_p256_sub_mod_p(pR->x, pR->x, hhh);
	__xrt_p256_sub_mod_p(pR->x, pR->x, t);
	__xrt_p256_sub_mod_p(pR->x, pR->x, t);
	
	// Y3 = R*(U1*H^2 - X3) - S1*H^3
	__xrt_p256_sub_mod_p(t, t, pR->x);        // U1*H^2 - X3
	__xrt_p256_mul_mod_p(pR->y, rr, t);
	__xrt_p256_mul_mod_p(t, s1, hhh);
	__xrt_p256_sub_mod_p(pR->y, pR->y, t);
	
	// Z3 = Z1 * H
	__xrt_p256_mul_mod_p(pR->z, P_save.z, h);
}

// 标量乘法: R = k * (px, py)
// 左到右二进制方法
static void __xrt_p256_scalar_mult(__xrt_p256_jpt *pR, const uint32 *k, const uint32 *px, const uint32 *py)
{
	int i;
	memset(pR, 0, sizeof(*pR));  // 无穷远点 (Z=0)
	
	for ( i = 255; i >= 0; i-- ) {
		__xrt_p256_pt_dbl(pR, pR);
		if ( k[i / 32] & ((uint32)1 << (i % 32)) ) {
			__xrt_p256_pt_add_aff(pR, pR, px, py);
		}
	}
}

// Jacobian -> 仿射坐标
static void __xrt_p256_to_affine(uint32 *ax, uint32 *ay, const __xrt_p256_jpt *pP)
{
	uint32 zinv[8], zinv2[8], zinv3[8];
	
	if ( __xrt_u256_is_zero(pP->z) ) {
		__xrt_u256_zero(ax);
		__xrt_u256_zero(ay);
		return;
	}
	
	__xrt_p256_inv_mod_p(zinv, pP->z);
	__xrt_p256_sqr_mod_p(zinv2, zinv);
	__xrt_p256_mul_mod_p(zinv3, zinv2, zinv);
	__xrt_p256_mul_mod_p(ax, pP->x, zinv2);
	__xrt_p256_mul_mod_p(ay, pP->y, zinv3);
}

/* ============================== ECDH secp256r1 + ECDSA 公共 API ============================== */

XXAPI void xrtECDHSecp256r1Keypair(uint8 *pPrivKey, uint8 *pPubKey)
{
	uint32 k[8], rx[8], ry[8];
	__xrt_p256_jpt tR;
	
	// 生成随机私钥 (1 < k < n)
	do {
		xrtRandomBytes(pPrivKey, 32);
		__xrt_u256_from_be(k, pPrivKey);
	} while ( __xrt_u256_is_zero(k) || __xrt_u256_gte(k, __xrt_p256_N) );
	
	// Q = k * G
	__xrt_p256_scalar_mult(&tR, k, __xrt_p256_Gx, __xrt_p256_Gy);
	__xrt_p256_to_affine(rx, ry, &tR);
	
	// 输出未压缩公钥: 0x04 || X || Y (65 字节)
	pPubKey[0] = 0x04;
	__xrt_u256_to_be(pPubKey + 1, rx);
	__xrt_u256_to_be(pPubKey + 33, ry);
}

XXAPI void xrtECDHSecp256r1SharedSecret(uint8 *pOut, const uint8 *pPrivKey, const uint8 *pPubKey)
{
	uint32 k[8], px[8], py[8], rx[8], ry[8];
	__xrt_p256_jpt tR;
	
	__xrt_u256_from_be(k, pPrivKey);
	
	if ( pPubKey[0] == 0x04 ) {
		__xrt_u256_from_be(px, pPubKey + 1);
		__xrt_u256_from_be(py, pPubKey + 33);
	} else {
		memset(pOut, 0, 32);
		return;
	}
	
	// S = k * PeerPub
	__xrt_p256_scalar_mult(&tR, k, px, py);
	__xrt_p256_to_affine(rx, ry, &tR);
	
	// 输出 X 坐标
	__xrt_u256_to_be(pOut, rx);
}

// ECDSA secp256r1 签名验证
// pSig: DER 编码的签名 (SEQUENCE { INTEGER r, INTEGER s })
// pPubKey: 未压缩公钥 (0x04 || X || Y, 65 字节)
XXAPI bool xrtECDSAVerify(const uint8 *pHash, size_t iHashLen, const uint8 *pSig, size_t iSigLen, const uint8 *pPubKey, size_t iPubKeyLen)
{
	uint32 r_u[8], s_u[8], z[8], sinv[8], u1[8], u2[8];
	uint32 px[8], py[8], rx[8], ry[8];
	__xrt_p256_jpt tR1, tR2;
	const uint8 *p;
	size_t rLen, sLen;
	uint8 rBuf[33], sBuf[33];
	
	// 解析公钥
	if ( (iPubKeyLen < 65) || (pPubKey[0] != 0x04) ) return false;
	__xrt_u256_from_be(px, pPubKey + 1);
	__xrt_u256_from_be(py, pPubKey + 33);
	
	// 解析 DER 签名
	p = pSig;
	if ( (iSigLen < 8) || (*p++ != 0x30) ) return false;
	{ size_t iSeqLen = *p++; (void)iSeqLen; }
	
	if ( *p++ != 0x02 ) return false;
	rLen = *p++;
	if ( rLen > 33 ) return false;
	memset(rBuf, 0, sizeof(rBuf));
	memcpy(rBuf + (33 - rLen), p, rLen);
	p += rLen;
	{ const uint8 *rS = rBuf; if ( rBuf[0] == 0 ) rS = rBuf + 1; __xrt_u256_from_be(r_u, rS); }
	
	if ( *p++ != 0x02 ) return false;
	sLen = *p++;
	if ( sLen > 33 ) return false;
	memset(sBuf, 0, sizeof(sBuf));
	memcpy(sBuf + (33 - sLen), p, sLen);
	{ const uint8 *sS = sBuf; if ( sBuf[0] == 0 ) sS = sBuf + 1; __xrt_u256_from_be(s_u, sS); }
	
	// 检查 r, s 在 [1, n-1]
	if ( __xrt_u256_is_zero(r_u) || __xrt_u256_gte(r_u, __xrt_p256_N) ) return false;
	if ( __xrt_u256_is_zero(s_u) || __xrt_u256_gte(s_u, __xrt_p256_N) ) return false;
	
	// z = hash 截取前 32 字节
	{
		uint8 zBuf[32] = {0};
		size_t iCopy = iHashLen > 32 ? 32 : iHashLen;
		memcpy(zBuf, pHash, iCopy);
		__xrt_u256_from_be(z, zBuf);
	}
	
	// s^(-1) mod n
	__xrt_p256_inv_mod_n(sinv, s_u);
	// u1 = z * s^(-1) mod n
	__xrt_p256_mul_mod_n(u1, z, sinv);
	// u2 = r * s^(-1) mod n
	__xrt_p256_mul_mod_n(u2, r_u, sinv);
	
	// R = u1*G + u2*Q
	__xrt_p256_scalar_mult(&tR1, u1, __xrt_p256_Gx, __xrt_p256_Gy);
	__xrt_p256_scalar_mult(&tR2, u2, px, py);
	
	// R = R1 + R2
	{
		uint32 ax[8], ay[8];
		__xrt_p256_to_affine(ax, ay, &tR2);
		if ( !__xrt_u256_is_zero(ax) || !__xrt_u256_is_zero(ay) ) {
			__xrt_p256_pt_add_aff(&tR1, &tR1, ax, ay);
		}
	}
	
	// 获取 R 的 x 坐标
	__xrt_p256_to_affine(rx, ry, &tR1);
	
	// 验证: r == rx mod n
	if ( __xrt_u256_gte(rx, __xrt_p256_N) ) {
		__xrt_u256_sub(rx, rx, __xrt_p256_N);
	}
	
	return (__xrt_u256_cmp(rx, r_u) == 0);
}

XXAPI bool xrtEd25519Verify(const uint8 *pMsg, size_t iMsgLen, const uint8 *pSig, const uint8 *pPubKey)
{
	// TODO: Ed25519 签名验证待实现
	(void)pMsg; (void)iMsgLen; (void)pSig; (void)pPubKey;
	return false;
}
