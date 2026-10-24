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


/* v2.1 */
int      ts_base64url_encode(const void *data, int len, char *out, int out_cap);
int      ts_base64url_decode(const char *b64, int len, void *out, int out_cap);
void     ts_bytes_to_hex(const void *data, int len, char *hex);
int      ts_hex_to_bytes(const char *hex, void *out, int out_cap);
uint32_t ts_fnv1a(const void *data, int len);
uint32_t ts_fnv1a_string(const char *s);
uint32_t ts_adler32(const void *data, int len);

/* v2.3 */
uint32_t ts_crc32_string(const char *s);
void     ts_md5_string(const char *s, unsigned char *digest);
int      ts_md5_string_hex(const char *s, char *hex);
uint32_t ts_fnv1a_combine(uint32_t h1, uint32_t h2);
/*
 * ts_titan_hash.h — Fast File Hashing (CRC32, MD5)
 * ===================================================
 * File integrity, deduplication, quick comparison.
 * Single-header C99. Zero dependencies.
 *
 * #define TS_TITAN_HASH_IMPLEMENTATION in ONE .c file.
 *
 * SoftCAD C+C TOPO © 2026
 */
#ifndef TS_TITAN_HASH_H
#define TS_TITAN_HASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* CRC32 */
uint32_t ts_crc32(const void *data, size_t len);
uint32_t ts_crc32_file(const char *path);

/* MD5 */
void ts_md5(const void *data, size_t len, uint8_t hash[16]);
void ts_md5_file(const char *path, uint8_t hash[16]);
void ts_md5_hex(const uint8_t hash[16], char hex[33]);

/* Quick file compare (by hash, not byte-by-byte) */
int ts_files_identical(const char *path_a, const char *path_b);

/* Base64 encode/decode */
int  ts_base64_encode(const void *data, int len, char *out, int max_out);
int  ts_base64_decode(const char *b64, int len, void *out, int max_out);

/* File size */
long ts_file_size(const char *path);

#ifdef TS_TITAN_HASH_IMPLEMENTATION

/* ── CRC32 (ISO 3309 polynomial) ──────────────────────────────────────── */

static uint32_t _crc_table[256];
static int _crc_init = 0;

static void _crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        _crc_table[i] = c;
    }
    _crc_init = 1;
}

uint32_t ts_crc32(const void *data, size_t len) {
    if (!_crc_init) _crc32_init(); /* idempotent — safe even with races */
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = _crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

uint32_t ts_crc32_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (!_crc_init) _crc32_init();
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        for (size_t i = 0; i < n; i++)
            crc = _crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    fclose(f);
    return crc ^ 0xFFFFFFFF;
}

/* ── MD5 (RFC 1321) ────────────────────────────────────────────────────── */

#define _F(x,y,z) (((x)&(y))|((~(x))&(z)))
#define _G(x,y,z) (((x)&(z))|((y)&(~(z))))
#define _H(x,y,z) ((x)^(y)^(z))
#define _I(x,y,z) ((y)^((x)|(~(z))))
#define _RL(x,n) (((x)<<(n))|((x)>>(32-(n))))

static const uint32_t _md5_t[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391,
};
static const int _md5_s[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21,
};

static void _md5_block(uint32_t state[4], const uint8_t block[64]) {
    uint32_t M[16];
    for (int i = 0; i < 16; i++)
        M[i] = ((uint32_t)block[i*4]) | ((uint32_t)block[i*4+1]<<8) |
               ((uint32_t)block[i*4+2]<<16) | ((uint32_t)block[i*4+3]<<24);
    uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16)      { f = _F(b,c,d); g = (uint32_t)i; }
        else if (i < 32) { f = _G(b,c,d); g = (5*i+1) % 16; }
        else if (i < 48) { f = _H(b,c,d); g = (3*i+5) % 16; }
        else             { f = _I(b,c,d); g = (7*i) % 16; }
        f = f + a + _md5_t[i] + M[g];
        a = d; d = c; c = b; b = b + _RL(f, _md5_s[i]);
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
}

void ts_md5(const void *data, size_t len, uint8_t hash[16]) {
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    const uint8_t *p = (const uint8_t *)data;
    size_t i;
    for (i = 0; i + 64 <= len; i += 64)
        _md5_block(state, p + i);
    uint8_t last[128]; memset(last, 0, 128);
    size_t rem = len - i;
    memcpy(last, p + i, rem);
    last[rem] = 0x80;
    int blocks = (rem >= 56) ? 2 : 1;
    uint64_t bits = (uint64_t)len * 8;
    memcpy(last + blocks * 64 - 8, &bits, 8);
    for (int b = 0; b < blocks; b++)
        _md5_block(state, last + b * 64);
    for (int j = 0; j < 4; j++) {
        hash[j*4]   = (uint8_t)(state[j]);
        hash[j*4+1] = (uint8_t)(state[j]>>8);
        hash[j*4+2] = (uint8_t)(state[j]>>16);
        hash[j*4+3] = (uint8_t)(state[j]>>24);
    }
}

void ts_md5_file(const char *path, uint8_t hash[16]) {
    FILE *f = fopen(path, "rb");
    if (!f) { memset(hash, 0, 16); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = (uint8_t *)malloc((size_t)sz);
    if(fread(data, 1, (size_t)sz, f) != (size_t)sz) { free(data); fclose(f); return; }
    fclose(f);
    ts_md5(data, (size_t)sz, hash);
    free(data);
}

void ts_md5_hex(const uint8_t hash[16], char hex[33]) {
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        hex[i*2]   = hx[hash[i] >> 4];
        hex[i*2+1] = hx[hash[i] & 0xF];
    }
    hex[32] = 0;
}

int ts_files_identical(const char *a, const char *b) {
    uint8_t ha[16], hb[16];
    ts_md5_file(a, ha);
    ts_md5_file(b, hb);
    return memcmp(ha, hb, 16) == 0;
}

#undef _F
#undef _G
#undef _H
#undef _I
#undef _RL

/* ── Base64 ─────────────────────────────────────────────────────────────── */

static const char _b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int ts_base64_encode(const void *data, int len, char *out, int max_out) {
    const uint8_t *d = (const uint8_t *)data;
    int o = 0;
    for (int i = 0; i < len && o < max_out - 4; i += 3) {
        uint32_t n = ((uint32_t)d[i]) << 16;
        if (i+1 < len) n |= ((uint32_t)d[i+1]) << 8;
        if (i+2 < len) n |= ((uint32_t)d[i+2]);
        out[o++] = _b64_chars[(n>>18)&63];
        out[o++] = _b64_chars[(n>>12)&63];
        out[o++] = (i+1 < len) ? _b64_chars[(n>>6)&63] : '=';
        out[o++] = (i+2 < len) ? _b64_chars[n&63] : '=';
    }
    out[o] = 0;
    return o;
}

static int _b64_val(char c) {
    if (c>='A'&&c<='Z') return c-'A';
    if (c>='a'&&c<='z') return c-'a'+26;
    if (c>='0'&&c<='9') return c-'0'+52;
    if (c=='+') return 62; if (c=='/') return 63;
    return -1;
}

int ts_base64_decode(const char *b64, int len, void *out, int max_out) {
    uint8_t *d = (uint8_t *)out;
    int o = 0;
    for (int i = 0; i < len && o < max_out; i += 4) {
        int a=_b64_val(b64[i]), b=_b64_val(b64[i+1]);
        int c=(i+2<len)?_b64_val(b64[i+2]):-1;
        int e=(i+3<len)?_b64_val(b64[i+3]):-1;
        if (a<0||b<0) break;
        d[o++] = (uint8_t)((a<<2)|(b>>4));
        if (c>=0 && o<max_out) d[o++] = (uint8_t)(((b&15)<<4)|(c>>2));
        if (e>=0 && o<max_out) d[o++] = (uint8_t)(((c&3)<<6)|e);
    }
    return o;
}

long ts_file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}


/* ═══════════════════════════════════════════════════════════ */
/* v2.1: base64url, hex utils, FNV-1a, Adler32               */
/* ═══════════════════════════════════════════════════════════ */

/* ── Base64 URL-safe (RFC 4648 §5) ─────────────────────── */

int ts_base64url_encode(const void *data, int len, char *out, int out_cap) {
    int n = ts_base64_encode(data, len, out, out_cap);
    /* Replace +→- /→_ and strip = padding */
    for(int i=0;i<n;i++){
        if(out[i]=='+')out[i]='-';
        else if(out[i]=='/')out[i]='_';
    }
    while(n>0 && out[n-1]=='=') n--;
    out[n]=0;
    return n;
}

int ts_base64url_decode(const char *b64, int len, void *out, int out_cap) {
    /* Convert back to standard base64 */
    char *tmp=(char*)malloc((size_t)(len+4));
    if(!tmp)return 0;
    memcpy(tmp,b64,(size_t)len);
    for(int i=0;i<len;i++){
        if(tmp[i]=='-')tmp[i]='+';
        else if(tmp[i]=='_')tmp[i]='/';
    }
    /* Add padding */
    int pad=(4-len%4)%4;
    for(int i=0;i<pad;i++)tmp[len+i]='=';
    tmp[len+pad]=0;
    int r=ts_base64_decode(tmp,len+pad,out,out_cap);
    free(tmp);
    return r;
}

/* ── Hex utilities ─────────────────────────────────────── */

void ts_bytes_to_hex(const void *data, int len, char *hex) {
    const uint8_t *p=(const uint8_t*)data;
    for(int i=0;i<len;i++) snprintf(hex+i*2,3,"%02x",p[i]);
    hex[len*2]=0;
}

int ts_hex_to_bytes(const char *hex, void *out, int out_cap) {
    int hlen=(int)strlen(hex);
    int blen=hlen/2;
    if(blen>out_cap) blen=out_cap;
    uint8_t *p=(uint8_t*)out;
    for(int i=0;i<blen;i++){
        unsigned int b;
        if(sscanf(hex+i*2,"%2x",&b)==1) p[i]=(uint8_t)b;
        else p[i]=0;
    }
    return blen;
}

/* ── FNV-1a hash (fast non-crypto, 32-bit) ─────────────── */

uint32_t ts_fnv1a(const void *data, int len) {
    uint32_t h=2166136261u;
    const uint8_t *p=(const uint8_t*)data;
    for(int i=0;i<len;i++){h^=p[i];h*=16777619u;}
    return h;
}

uint32_t ts_fnv1a_string(const char *s) {
    return ts_fnv1a(s,(int)strlen(s));
}

/* ── Adler-32 checksum ─────────────────────────────────── */

uint32_t ts_adler32(const void *data, int len) {
    uint32_t a=1,b=0;
    const uint8_t *p=(const uint8_t*)data;
    for(int i=0;i<len;i++){a=(a+p[i])%65521;b=(b+a)%65521;}
    return (b<<16)|a;
}


/* v2.3: convenience functions */

uint32_t ts_crc32_string(const char *s) {
    return ts_crc32((const void *)s, (int)strlen(s));
}

void ts_md5_string(const char *s, unsigned char *digest) {
    ts_md5(s, strlen(s), digest);
}

int ts_md5_string_hex(const char *s, char *hex) {
    unsigned char digest[16];
    ts_md5_string(s, digest);
    ts_md5_hex(digest, hex);
    return 32;
}

uint32_t ts_fnv1a_combine(uint32_t h1, uint32_t h2) {
    return (h1 ^ h2) * 16777619u;
}



/* Constant-time compare (prevents timing attacks on HMAC verify) */
int ts_hash_constant_compare(const unsigned char *a, const unsigned char *b, int len) {
    unsigned char result = 0;
    for (int i = 0; i < len; i++) result |= a[i] ^ b[i];
    return result == 0;
}


/* SHA-256 of string (convenience) */
void ts_sha256_string(const char *str, unsigned char *hash) {
    ts_sha256((const void*)str, (int)strlen(str), hash);
}

/* SHA-256 string → hex string */
int ts_sha256_string_hex(const char *str, char *hex) {
    unsigned char hash[32];
    ts_sha256_string(str, hash);
    ts_sha256_hex(hash, hex);
    return 64;
}

#endif /* TS_TITAN_HASH_IMPLEMENTATION */

#endif
