/*
 * libtitangfx — motorul grafic deschis al ecosistemului SoftCAD (TVW/TitanView).
 * Copyright (C) 2026 SoftCAD C+C TOPO S.R.L.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details. You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */
/*
 * ts_crypto_sha256.c — SHA-256 (FIPS 180-4 / RFC 6234) pure C99 zero deps.
 *
 * For PDF digital signatures (PAS 7) — modern hash superior to SHA-1.
 *
 * Public API:
 *   void ts_sha256(const unsigned char* data, long len, unsigned char digest[32]);
 *   void ts_sha256_init(ts_sha256_ctx*);
 *   void ts_sha256_update(ts_sha256_ctx*, const unsigned char* data, long len);
 *   void ts_sha256_final(ts_sha256_ctx*, unsigned char digest[32]);
 *
 * SoftCAD C+C TOPO © 2026
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>


#ifndef TS_SHA256_CTX_DEFINED
#define TS_SHA256_CTX_DEFINED
typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    unsigned char buffer[64];
    int buf_used;
} ts_sha256_ctx;
#endif


static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};


#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))


void ts_sha256_init(ts_sha256_ctx* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buf_used = 0;
}


static void sha256_transform(ts_sha256_ctx* ctx, const unsigned char* block) {
    uint32_t W[64];
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
                ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR32(W[i-15], 7) ^ ROR32(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = ROR32(W[i-2], 17) ^ ROR32(W[i-2], 19) ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];
    
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + W[i];
        uint32_t S0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    
    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}


void ts_sha256_update(ts_sha256_ctx* ctx, const unsigned char* data, long len) {
    ctx->bitcount += (uint64_t)len * 8;
    
    while (len > 0) {
        int take = 64 - ctx->buf_used;
        if (take > len) take = (int)len;
        memcpy(ctx->buffer + ctx->buf_used, data, take);
        ctx->buf_used += take;
        data += take;
        len -= take;
        
        if (ctx->buf_used == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buf_used = 0;
        }
    }
}


void ts_sha256_final(ts_sha256_ctx* ctx, unsigned char digest[32]) {
    uint64_t total_bits = ctx->bitcount;
    
    /* Pad with 0x80 then zeros */
    ctx->buffer[ctx->buf_used++] = 0x80;
    if (ctx->buf_used > 56) {
        while (ctx->buf_used < 64) ctx->buffer[ctx->buf_used++] = 0;
        sha256_transform(ctx, ctx->buffer);
        ctx->buf_used = 0;
    }
    while (ctx->buf_used < 56) ctx->buffer[ctx->buf_used++] = 0;
    
    /* Append 64-bit big-endian length */
    for (int i = 7; i >= 0; i--) {
        ctx->buffer[ctx->buf_used++] = (unsigned char)((total_bits >> (i * 8)) & 0xFF);
    }
    sha256_transform(ctx, ctx->buffer);
    
    /* Output state in big-endian */
    for (int i = 0; i < 8; i++) {
        digest[i*4]   = (unsigned char)((ctx->state[i] >> 24) & 0xFF);
        digest[i*4+1] = (unsigned char)((ctx->state[i] >> 16) & 0xFF);
        digest[i*4+2] = (unsigned char)((ctx->state[i] >> 8) & 0xFF);
        digest[i*4+3] = (unsigned char)((ctx->state[i]) & 0xFF);
    }
}


void ts_sha256(const unsigned char* data, long len, unsigned char digest[32]) {
    ts_sha256_ctx ctx;
    ts_sha256_init(&ctx);
    ts_sha256_update(&ctx, data, len);
    ts_sha256_final(&ctx, digest);
}
