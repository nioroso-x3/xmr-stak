/*
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>.
  *
  */
#pragma once

#include "cryptonight.h"
#include "xmrstak/backend/cryptonight.hpp"
#include <memory.h>
#include <stdio.h>
#include <cfenv>
#include <utility>
#include "soft_aes_altivec.hpp"
#include "FastSqrt_ppc64.h"
#include <altivec.h>
#include <math.h>
#undef vector
#undef pixel
#undef bool
typedef __vector unsigned char __m128i;
typedef __vector unsigned long long __m128ll;
typedef __vector double __m128d;


static inline __m128i v_rev(__m128i tmp1)
{
  return(vec_perm(tmp1,tmp1,(__m128i){ 0xf,0xe,0xd,0xc,0xb,0xa,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0 })); 
}

static inline __m128i v_rev(__m128i *tmp1)
{
  return(vec_perm(*tmp1,*tmp1,(__m128i){ 0xf,0xe,0xd,0xc,0xb,0xa,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0 })); 
}

static inline __m128i _mm_aesenc_si128(__m128i in, __m128i key)
{
  return v_rev(__builtin_crypto_vcipher(v_rev(in),v_rev(key)));
}

static inline __m128i _mm_aesenc_si128_be(__m128i in, __m128i key)
{
  return __builtin_crypto_vcipher(in,key);
}

static inline uint64_t _umul128(uint64_t a, uint64_t b, uint64_t* hi)
{
	unsigned __int128 r = (unsigned __int128)a * (unsigned __int128)b;
	*hi = r >> 64;
	return (uint64_t)r;
}


extern "C"
{
	void keccak(const uint8_t *in, int inlen, uint8_t *md, int mdlen);
	void keccakf(uint64_t st[25], int rounds);
	extern void(*const extra_hashes[4])(const void *, uint32_t, char *);
}

// This will shift and xor tmp1 into itself as 4 32-bit vals such as
// sl_xor(a1 a2 a3 a4) = a1 (a2^a1) (a3^a2^a1) (a4^a3^a2^a1)
static inline __m128i sl_xor(__m128i tmp1)
{
  __m128i tmp4;
  tmp4 = vec_slo(tmp1, (__m128i){0x20});
  tmp1 = vec_xor(tmp1, tmp4);
  tmp4 = vec_slo(tmp4, (__m128i){0x20});
  tmp1 = vec_xor(tmp1, tmp4);
  tmp4 = vec_slo(tmp4, (__m128i){0x20});
  tmp1 = vec_xor(tmp1, tmp4);
  return tmp1;
}

static inline __m128i sr_xor(__m128i tmp1)
{
  __m128i tmp4;
  tmp4 = vec_sro(tmp1, (__m128i){0x20});
  tmp1 = vec_xor(tmp1, tmp4);
  tmp4 = vec_sro(tmp4, (__m128i){0x20});
  tmp1 = vec_xor(tmp1, tmp4);
  tmp4 = vec_sro(tmp4, (__m128i){0x20});
  tmp1 = vec_xor(tmp1, tmp4);
  return tmp1;
}

template<uint8_t rcon>
static inline void aes_genkey_sub(__m128i* xout0, __m128i* xout2){
 __m128i xout1 = soft_aeskeygenassist(*xout2, rcon);
  xout1 = vec_perm(xout1,xout1,(__m128i){0xc,0xd,0xe,0xf, 0xc,0xd,0xe,0xf, 0xc,0xd,0xe,0xf, 0xc,0xd,0xe,0xf}); 
  *xout0 = sl_xor(*xout0);
 *xout0 = vec_xor(*xout0, xout1);
 xout1 = soft_aeskeygenassist(*xout0, 0x00);
  xout1 = vec_perm(xout1,xout1,(__m128i){0x8,0x9,0xa,0xb, 0x8,0x9,0xa,0xb, 0x8,0x9,0xa,0xb, 0x8,0x9,0xa,0xb});
  *xout2 = sl_xor(*xout2);
 *xout2 = vec_xor(*xout2, xout1);
}

template<uint8_t rcon>
static inline void aes_genkey_sub_be(__m128i* xout0, __m128i* xout2){
 __m128i xout1 = soft_aeskeygenassist_be(*xout2, rcon);
 xout1 = vec_perm(xout1,xout1,(__m128i){0x0,0x1,0x2,0x3, 0x0,0x1,0x2,0x3, 0x0,0x1,0x2,0x3, 0x0,0x1,0x2,0x3}); 
 *xout0 = sr_xor(*xout0);
 *xout0 = vec_xor(*xout0, xout1);
 xout1 = soft_aeskeygenassist_be(*xout0, 0x00);
 xout1 = vec_perm(xout1,xout1,(__m128i){0x4,0x5,0x6,0x7, 0x4,0x5,0x6,0x7, 0x4,0x5,0x6,0x7, 0x4,0x5,0x6,0x7});
 *xout2 = sr_xor(*xout2);
 *xout2 = vec_xor(*xout2, xout1);
 }

template<bool SOFT_AES>
static inline void aes_genkey(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3,
	__m128i* k4, __m128i* k5, __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9)
{
  __m128i xout0, xout2;

  xout0 = vec_ld(0,memory);
  xout2 = vec_ld(16,memory);
  *k0 = xout0;
  *k1 = xout2;
  aes_genkey_sub<0x01>(&xout0, &xout2);
  *k2 = xout0;
  *k3 = xout2;
  
  aes_genkey_sub<0x02>(&xout0, &xout2);
  *k4 = xout0;
  *k5 = xout2;

  aes_genkey_sub<0x04>(&xout0, &xout2);
  *k6 = xout0;
  *k7 = xout2;

  aes_genkey_sub<0x08>(&xout0, &xout2);
  *k8 = xout0;
  *k9 = xout2;
}
template<bool SOFT_AES>
static inline void aes_genkey_be(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3,
 __m128i* k4, __m128i* k5, __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9){
  __m128i xout0, xout2;
  xout0 = v_rev(vec_ld(0,memory));
  xout2 = v_rev(vec_ld(16,memory));
  *k0 = xout0;
  *k1 = xout2;
  aes_genkey_sub_be<0x01>(&xout0, &xout2);
  *k2 = xout0;
  *k3 = xout2;
  aes_genkey_sub_be<0x02>(&xout0, &xout2);
  *k4 = xout0;
  *k5 = xout2;
  aes_genkey_sub_be<0x04>(&xout0, &xout2);
  *k6 = xout0;
  *k7 = xout2;
  aes_genkey_sub_be<0x08>(&xout0, &xout2);
  *k8 = xout0;
  *k9 = xout2;
}

static inline void aes_round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7){
	*x0 = _mm_aesenc_si128(*x0, key);
	*x1 = _mm_aesenc_si128(*x1, key);
	*x2 = _mm_aesenc_si128(*x2, key);
	*x3 = _mm_aesenc_si128(*x3, key);
	*x4 = _mm_aesenc_si128(*x4, key);
	*x5 = _mm_aesenc_si128(*x5, key);
	*x6 = _mm_aesenc_si128(*x6, key);
	*x7 = _mm_aesenc_si128(*x7, key);
}

static inline void aes_round_be(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6, __m128i* x7){
  *x0 = _mm_aesenc_si128_be(*x0, key);
  *x1 = _mm_aesenc_si128_be(*x1, key);
  *x2 = _mm_aesenc_si128_be(*x2, key);
  *x3 = _mm_aesenc_si128_be(*x3, key);
  *x4 = _mm_aesenc_si128_be(*x4, key);
  *x5 = _mm_aesenc_si128_be(*x5, key);
  *x6 = _mm_aesenc_si128_be(*x6, key);
  *x7 = _mm_aesenc_si128_be(*x7, key);
}

inline void mix_and_propagate(__m128i& x0, __m128i& x1, __m128i& x2, __m128i& x3, __m128i& x4, __m128i& x5, __m128i& x6, __m128i& x7)
{
	__m128i tmp0 = x0;
	x0 = vec_xor(x0, x1);
	x1 = vec_xor(x1, x2);
	x2 = vec_xor(x2, x3);
	x3 = vec_xor(x3, x4);
	x4 = vec_xor(x4, x5);
	x5 = vec_xor(x5, x6);
	x6 = vec_xor(x6, x7);
	x7 = vec_xor(x7, tmp0);
}

template<size_t MEM, bool SOFT_AES, bool PREFETCH, xmrstak_algo ALGO>
void cn_explode_scratchpad(const __m128i* input, __m128i* output)
{
	// This is more than we have registers, compiler will assign 2 keys on the stack
	__m128i xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7;
	__m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

	aes_genkey<SOFT_AES>(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);


  xin0 = vec_ld(64,input);
  xin1 = vec_ld(80,input);
  xin2 = vec_ld(96,input);
  xin3 = vec_ld(112,input);
  xin4 = vec_ld(128,input);
  xin5 = vec_ld(144,input);
  xin6 = vec_ld(160,input);
  xin7 = vec_ld(176,input);

	if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
	{
		for(size_t i=0; i < 16; i++)
		{
  		aes_round(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);		
			mix_and_propagate(xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7);
		}
	}

	for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
	{
		aes_round(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
   
    vec_st(xin0,i*16,output);
    vec_st(xin1,(i+1)*16,output);
    vec_st(xin2,(i+2)*16,output);
    vec_st(xin3,(i+3)*16,output);
    vec_st(xin4,(i+4)*16,output);
    vec_st(xin5,(i+5)*16,output);
    vec_st(xin6,(i+6)*16,output);
    vec_st(xin7,(i+7)*16,output);
	}
}
template<size_t MEM, bool SOFT_AES, bool PREFETCH, xmrstak_algo ALGO>
void cn_explode_scratchpad_be(const __m128i* input, __m128i* output)
{
	// This is more than we have registers, compiler will assign 2 keys on the stack
	__m128i xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7;
	__m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

	aes_genkey_be<SOFT_AES>(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);


  xin0 = v_rev(vec_ld(64,input));
  xin1 = v_rev(vec_ld(80,input));
  xin2 = v_rev(vec_ld(96,input));
  xin3 = v_rev(vec_ld(112,input));
  xin4 = v_rev(vec_ld(128,input));
  xin5 = v_rev(vec_ld(144,input));
  xin6 = v_rev(vec_ld(160,input));
  xin7 = v_rev(vec_ld(176,input));

	if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
	{
		for(size_t i=0; i < 16; i++)
		{
			aes_round_be(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			aes_round_be(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
			mix_and_propagate(xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7);
		}
	}

	for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
	{
		aes_round_be(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
		aes_round_be(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);

    vec_st(v_rev(xin0),i*16,output);
    vec_st(v_rev(xin1),(i+1)*16,output);
    vec_st(v_rev(xin2),(i+2)*16,output);
    vec_st(v_rev(xin3),(i+3)*16,output);
    vec_st(v_rev(xin4),(i+4)*16,output);
    vec_st(v_rev(xin5),(i+5)*16,output);
    vec_st(v_rev(xin6),(i+6)*16,output);
    vec_st(v_rev(xin7),(i+7)*16,output);
	}
}
template<size_t MEM, bool SOFT_AES, bool PREFETCH, xmrstak_algo ALGO>
void cn_implode_scratchpad(const __m128i* input, __m128i* output)
{
	// This is more than we have registers, compiler will assign 2 keys on the stack
	__m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
	__m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

	aes_genkey<SOFT_AES>(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

	xout0 = vec_ld(64,output);
  xout1 = vec_ld(80,output);
  xout2 = vec_ld(96,output);
  xout3 = vec_ld(112,output);
  xout4 = vec_ld(128,output);
  xout5 = vec_ld(144,output);
  xout6 = vec_ld(160,output);
  xout7 = vec_ld(176,output);

	for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
	{
		xout0 = vec_xor(vec_ld(i*16,input), xout0);
    xout1 = vec_xor(vec_ld((i+1)*16,input), xout1);
    xout2 = vec_xor(vec_ld((i+2)*16,input), xout2);
    xout3 = vec_xor(vec_ld((i+3)*16,input), xout3);
    xout4 = vec_xor(vec_ld((i+4)*16,input), xout4);
    xout5 = vec_xor(vec_ld((i+5)*16,input), xout5);
    xout6 = vec_xor(vec_ld((i+6)*16,input), xout6);
    xout7 = vec_xor(vec_ld((i+7)*16,input), xout7);
  	aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
	  aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
			mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
	}

	if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
	{
		for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
		{
  		xout0 = vec_xor(vec_ld(i*16,input), xout0);
      xout1 = vec_xor(vec_ld((i+1)*16,input), xout1);
      xout2 = vec_xor(vec_ld((i+2)*16,input), xout2);
      xout3 = vec_xor(vec_ld((i+3)*16,input), xout3);
      xout4 = vec_xor(vec_ld((i+4)*16,input), xout4);
      xout5 = vec_xor(vec_ld((i+5)*16,input), xout5);
      xout6 = vec_xor(vec_ld((i+6)*16,input), xout6);
      xout7 = vec_xor(vec_ld((i+7)*16,input), xout7);
   		aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
				mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
		}

		for(size_t i=0; i < 16; i++)
		{
			aes_round(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
  		aes_round(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
		}
	}

  vec_st(xout0,64,output);
  vec_st(xout1,80,output);
  vec_st(xout2,96,output);
  vec_st(xout3,112,output);
  vec_st(xout4,128,output);
  vec_st(xout5,144,output);
  vec_st(xout6,160,output);
  vec_st(xout7,176,output);
}

template<size_t MEM, bool SOFT_AES, bool PREFETCH, xmrstak_algo ALGO>
void cn_implode_scratchpad_be(const __m128i* input, __m128i* output)
{
	// This is more than we have registers, compiler will assign 2 keys on the stack
	__m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
	__m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

	aes_genkey_be<SOFT_AES>(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

	xout0 = v_rev(vec_ld(64,output));
  xout1 = v_rev(vec_ld(80,output));
  xout2 = v_rev(vec_ld(96,output));
  xout3 = v_rev(vec_ld(112,output));
  xout4 = v_rev(vec_ld(128,output));
  xout5 = v_rev(vec_ld(144,output));
  xout6 = v_rev(vec_ld(160,output));
  xout7 = v_rev(vec_ld(176,output));

	for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
	{
		xout0 = vec_xor(v_rev(vec_ld(i*16,input)), xout0);
    xout1 = vec_xor(v_rev(vec_ld((i+1)*16,input)), xout1);
    xout2 = vec_xor(v_rev(vec_ld((i+2)*16,input)), xout2);
    xout3 = vec_xor(v_rev(vec_ld((i+3)*16,input)), xout3);
    xout4 = vec_xor(v_rev(vec_ld((i+4)*16,input)), xout4);
    xout5 = vec_xor(v_rev(vec_ld((i+5)*16,input)), xout5);
    xout6 = vec_xor(v_rev(vec_ld((i+6)*16,input)), xout6);
    xout7 = vec_xor(v_rev(vec_ld((i+7)*16,input)), xout7);
  	aes_round_be(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
	  aes_round_be(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		aes_round_be(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
		if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
			mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
	}

	if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
	{
		for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8)
		{
  		xout0 = vec_xor(v_rev(vec_ld(i*16,input)), xout0);
      xout1 = vec_xor(v_rev(vec_ld((i+1)*16,input)), xout1);
      xout2 = vec_xor(v_rev(vec_ld((i+2)*16,input)), xout2);
      xout3 = vec_xor(v_rev(vec_ld((i+3)*16,input)), xout3);
      xout4 = vec_xor(v_rev(vec_ld((i+4)*16,input)), xout4);
      xout5 = vec_xor(v_rev(vec_ld((i+5)*16,input)), xout5);
      xout6 = vec_xor(v_rev(vec_ld((i+6)*16,input)), xout6);
      xout7 = vec_xor(v_rev(vec_ld((i+7)*16,input)), xout7);
   		aes_round_be(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			if(ALGO == cryptonight_heavy || ALGO == cryptonight_haven || ALGO == cryptonight_bittube2)
				mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
		}

		for(size_t i=0; i < 16; i++)
		{
			aes_round_be(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
  		aes_round_be(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			aes_round_be(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
			mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
		}
	}
  vec_st(v_rev(xout0),64,output);
  vec_st(v_rev(xout1),80,output);
  vec_st(v_rev(xout2),96,output);
  vec_st(v_rev(xout3),112,output);
  vec_st(v_rev(xout4),128,output);
  vec_st(v_rev(xout5),144,output);
  vec_st(v_rev(xout6),160,output);
  vec_st(v_rev(xout7),176,output);
}

inline uint64_t int_sqrt33_1_double_precision(const uint64_t n0){
  uint64_t r = (n0 >> 12) + (1023ULL << 52);
  double rd = sqrt( *(double*)&r );
  r = *(uint64_t*)&rd;
  const uint64_t s = r >> 20;
	r >>= 19;
	uint64_t x2 = (s - (1022ULL << 32)) * (r - s - (1022ULL << 32) + 1);
	if (x2 < n0) ++r;
	return r;
}

inline __m128i int_sqrt33_1_double_precision2(uint64_t n0,uint64_t n1){
  __m128d x = (__m128d)((__m128ll){n0 >> 12,n1 >> 12} + (__m128ll){1023ULL << 52,1023ULL << 52});
  x = vec_sqrt(x);
  uint64_t r0 = ((uint64_t*)&x)[0];
  uint64_t r1 = ((uint64_t*)&x)[1];
  uint64_t s0 = r0 >> 20;
  uint64_t s1 = r1 >> 20;
	r0 >>= 19;
	r1 >>= 19;
	uint64_t x2_0 = (s0 - (1022ULL << 32)) * (r0 - s0 - (1022ULL << 32) + 1);
	uint64_t x2_1 = (s1 - (1022ULL << 32)) * (r1 - s1 - (1022ULL << 32) + 1);
	if (x2_0 < n0) ++r0;
	if (x2_1 < n1) ++r1;
  ((uint64_t*)&x)[0] = r0;
  ((uint64_t*)&x)[1] = r1;

  return (__m128i)x;
}
inline uint64_t int_sqrt33_1_double_precision_fast(const uint64_t n0){
   return SqrtV2::get(n0);
}

inline __m128i int_sqrt33_1_double_precision_fast2(const uint64_t n0,const uint64_t n1){
   return (__m128ll){SqrtV2::get(n0),SqrtV2::get(n1)};
}

inline __m128i aes_round_bittube2(const __m128i& val, const __m128i& key)
{
	alignas(16) uint32_t k[4];
	alignas(16) uint32_t x[4];
	vec_st(key,0,(__m128i*)k);
  *(__m128i*)x = vec_xor(val,(__m128i){0xFF}); // x = ~val
	#define BYTE(p, i) ((unsigned char*)&p)[i]
	k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
	x[0] ^= k[0];
	k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
	x[1] ^= k[1];
	k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
	x[2] ^= k[2];
	k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];
	#undef BYTE
	return vec_ld(0,(__m128i*)k);
}

template<xmrstak_algo ALGO>
inline void cryptonight_monero_tweak(uint64_t* mem_out, __m128i tmp)
{
        uint64_t* t = (uint64_t*)&tmp;
	mem_out[0] = t[0];
	uint64_t vh = t[1];
	uint8_t x = static_cast<uint8_t>(vh >> 24);
	static const uint16_t table = 0x7531;
	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2)
	{
		const uint8_t index = (((x >> 3) & 6) | (x & 1)) << 1;
		vh ^= ((table >> index) & 0x3) << 28;
		mem_out[1] = vh;
	}
	else if(ALGO == cryptonight_stellite)
	{
		const uint8_t index = (((x >> 4) & 6) | (x & 1)) << 1;
		vh ^= ((table >> index) & 0x3) << 28;
		mem_out[1] = vh;
	}

}

inline void set_float_rounding_mode()
{
	std::fesetround(FE_DOWNWARD);
}

template< size_t N>
struct Cryptonight_hash;

template< >
struct Cryptonight_hash<1>
{
	static constexpr size_t N = 1;
	template<xmrstak_algo ALGO, bool SOFT_AES, bool PREFETCH>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{
		constexpr size_t MASK = cn_select_mask<ALGO>();
		constexpr size_t ITERATIONS = cn_select_iter<ALGO>();
		constexpr size_t MEM = cn_select_memory<ALGO>();
  	uint64_t n = 0;
    if((ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) && len < 43) 
  	{ 
  		memset(output, 0, 32 * N); 
  		return; 
  	}
  	keccak((const uint8_t *)input + len * n, len, ctx[n]->hash_state, 200); 
  	uint64_t monero_const; 
  	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
   	{ 
    	monero_const =  *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + len * n + 35); 
   		monero_const ^=  *(reinterpret_cast<const uint64_t*>(ctx[n]->hash_state) + 24); 
  	} 
  	/* Optim - 99% time boundary */ 
  	if(SOFT_AES)
      cn_explode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[n]->hash_state, (__m128i*)ctx[n]->long_state); 
  	else
      cn_explode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[n]->hash_state, (__m128i*)ctx[n]->long_state); 

  	__m128i ax0; 
  	uint64_t idx0; 
  	__m128i bx0; 
  	uint8_t* l0 = ctx[n]->long_state; 
  	/* BEGIN cryptonight_monero_v8 variables */ 
  	__m128i bx1; 
	  uint64_t division_result; 
  	uint64_t sqrt_result; 
  	/* END cryptonight_monero_v8 variables */ 
  	{ 
  		uint64_t* h0 = (uint64_t*)ctx[n]->hash_state; 
  		idx0 = h0[0] ^ h0[4]; 
  		ax0 = (__m128ll){idx0, h0[1] ^ h0[5]}; 
  		bx0 = (__m128ll){h0[2] ^ h0[6], h0[3] ^ h0[7]}; 
  		if(ALGO == cryptonight_monero_v8) 
  		{ 
  			bx1 = (__m128ll){h0[8] ^ h0[10], h0[9] ^ h0[11]}; 
  			division_result = h0[12]; 
  			sqrt_result = h0[13]; 
  			set_float_rounding_mode(); 
  		} 
  	} 
  	__m128i *ptr0;


		// Optim - 90% time boundary
		for(size_t i = 0; i < ITERATIONS; i++)
		{
    	__m128i cx; 
    	ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    	cx = vec_ld(0,ptr0); 
    	if (ALGO == cryptonight_bittube2) 
    	{ 
    		cx = aes_round_bittube2(cx, ax0); 
    	} 
    	else 
    	{ 
    			cx = _mm_aesenc_si128(cx, ax0); 
    	} 
    	/* Shuffle the other 3x16 byte chunks in the current 64-byte cache line */ 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		const uint64_t idx1 = idx0 & MASK; 
    		__m128i chunk1 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x10]); 
    	 	__m128i chunk2 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x20]); 
    		__m128i chunk3 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x30]); 
    		*((__m128i *)&l0[idx1 ^ 0x10]) = (__m128ll)chunk3 + (__m128ll)bx1; 
        *((__m128i *)&l0[idx1 ^ 0x20]) = (__m128ll)chunk1 + (__m128ll)bx0;
        *((__m128i *)&l0[idx1 ^ 0x30]) = (__m128ll)chunk2 + (__m128ll)ax0;
    	}


    	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
    		cryptonight_monero_tweak<ALGO>((uint64_t*)ptr0, vec_xor(bx0, cx)); 
    	else 
    		vec_st(vec_xor(bx0, cx), 0, (__m128i *)ptr0 ); 
    	idx0 = ((uint64_t*)&cx)[0]; 
    	
    	ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    	if(ALGO != cryptonight_monero_v8) 
    		bx0 = cx;
    	uint64_t lo, cl, ch; 
    	uint64_t al0 = ((uint64_t*)&ax0)[0]; 
    	uint64_t ah0 = ((uint64_t*)&ax0)[1]; 
    	cl = ((uint64_t*)ptr0)[0]; 
    	ch = ((uint64_t*)ptr0)[1]; 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		uint64_t sqrt_result_tmp; 
    		sqrt_result_tmp = sqrt_result; 
    		/* Use division and square root results from the _previous_ iteration to hide the latency */ 
    		const uint64_t cx_64 = ((uint64_t*)&cx)[0]; 
    		cl ^= static_cast<uint64_t>(division_result) ^ (sqrt_result_tmp << 32); 
    		const uint32_t d = (cx_64 + (sqrt_result_tmp << 1)) | 0x80000001UL; 
    		/* Most and least significant bits in the divisor are set to 1 
    		 * to make sure we don't divide by a small or even number, 
    		 * so there are no shortcuts for such cases 
    		 * 
    		 * Quotient may be as large as (2^64 - 1)/(2^31 + 1) = 8589934588 = 2^33 - 4 
    		 * We drop the highest bit to fit both quotient and remainder in 32 bits 
		     */  
    		/* Compiler will optimize it to a single div instruction */ 
    		__m128i cx_v = vec_sro(cx,(__m128i){0x40}); 
        const uint64_t cx_s = ((uint64_t*)&cx_v)[0]; 
    		const uint64_t division_result_ = static_cast<uint32_t>(cx_s / d) + ((cx_s % d) << 32); 
    		division_result = static_cast<int64_t>(division_result_); 
    		/* Use division_result as an input for the square root to prevent parallel implementation in hardware */ 
    		if(PREFETCH)
          sqrt_result = int_sqrt33_1_double_precision(cx_64 + division_result_);
        else
          sqrt_result = int_sqrt33_1_double_precision_fast(cx_64 + division_result_); 

    	}

    	{ 
    	  uint64_t hi;
    		lo = _umul128(idx0, cl, &hi); 
    		/* Shuffle the other 3x16 byte chunks in the current 64-byte cache line */ 
      	if(ALGO == cryptonight_monero_v8) 
        { 
       		const uint64_t idx1 = idx0 & MASK; 
   		    __m128i chunk1 =  (__m128ll){hi, lo};
          chunk1 = vec_xor(*(__m128i *)&l0[idx1 ^ 0x10], chunk1); 
       		__m128i chunk2 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x20]); 
       		hi ^= ((uint64_t*)&chunk2)[0]; 
       		lo ^= ((uint64_t*)&chunk2)[1]; 
       		__m128i chunk3 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x30]); 
         	*((__m128i *)&l0[idx1 ^ 0x10]) = (__m128ll)chunk3 + (__m128ll)bx1; 
          *((__m128i *)&l0[idx1 ^ 0x20]) = (__m128ll)chunk1 + (__m128ll)bx0;
          *((__m128i *)&l0[idx1 ^ 0x30]) = (__m128ll)chunk2 + (__m128ll)ax0;
      	}

        ah0 += lo; 
    		al0 += hi; 
    	} 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		bx1 = bx0; 
    		bx0 = cx; 
    	} 
    	((uint64_t*)ptr0)[0] = al0;

    	if (ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
    	{ 
    		if (ALGO == cryptonight_ipbc || ALGO == cryptonight_bittube2) 
    			((uint64_t*)ptr0)[1] = ah0 ^ monero_const ^ ((uint64_t*)ptr0)[0]; 
    		else 
    			((uint64_t*)ptr0)[1] = ah0 ^ monero_const; 
    	} 
    	else 
    		((uint64_t*)ptr0)[1] = ah0; 
    	al0 ^= cl; 
    	ah0 ^= ch; 
    	ax0 = (__m128ll){al0, ah0}; 
    	idx0 = al0;
    	if(ALGO == cryptonight_heavy || ALGO == cryptonight_bittube2) 
    	{ 
    		ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    		int64_t u  = ((int64_t*)ptr0)[0]; 
    		int32_t d  = ((int32_t*)ptr0)[2]; 
    		int64_t q = u / (d | 0x5); 
    		
    		((int64_t*)ptr0)[0] = u ^ q; 
    		idx0 = d ^ q; 
    	} 
    	else if(ALGO == cryptonight_haven) 
    	{ 
    		ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    		int64_t u  = ((int64_t*)ptr0)[0]; 
    		int32_t d  = ((int32_t*)ptr0)[2]; 
    		int64_t q = u / (d | 0x5); 
    		
    		((int64_t*)ptr0)[0] = u ^ q; 
    		idx0 = (~d) ^ q; 
    	}

		}
  	/* Optim - 90% time boundary */ 
  	if(SOFT_AES)
      cn_implode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[n]->long_state, (__m128i*)ctx[n]->hash_state); 
    else
      cn_implode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[n]->long_state, (__m128i*)ctx[n]->hash_state); 
  	/* Optim - 99% time boundary */ 
  	keccakf((uint64_t*)ctx[n]->hash_state, 24); 
  	extra_hashes[ctx[n]->hash_state[0] & 3](ctx[n]->hash_state, 200, (char*)output + 32 * n);
	}
};

template< >
struct Cryptonight_hash<2>
{
	static constexpr size_t N = 2;

	template<xmrstak_algo ALGO, bool SOFT_AES, bool PREFETCH>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{
		constexpr size_t MASK = cn_select_mask<ALGO>();
		constexpr size_t ITERATIONS = cn_select_iter<ALGO>();
		constexpr size_t MEM = cn_select_memory<ALGO>();
    if((ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) && len < 43) 
  	{ 
  		memset(output, 0, 32 * N); 
  		return; 
  	}
  	keccak((const uint8_t *)input + len * 0, len, ctx[0]->hash_state, 200); 
  	keccak((const uint8_t *)input + len * 1, len, ctx[1]->hash_state, 200); 
  	uint64_t monero_const_0; 
  	uint64_t monero_const_1; 
  	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
   	{ 
    	monero_const_0 =  *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + len * 0 + 35); 
    	monero_const_1 =  *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + len * 1 + 35); 
   		monero_const_0 ^=  *(reinterpret_cast<const uint64_t*>(ctx[0]->hash_state) + 24); 
   		monero_const_1 ^=  *(reinterpret_cast<const uint64_t*>(ctx[1]->hash_state) + 24); 
  	} 
  	/* Optim - 99% time boundary */ 
    if(SOFT_AES){
      cn_explode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->hash_state, (__m128i*)ctx[0]->long_state); 
   	  cn_explode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->hash_state, (__m128i*)ctx[1]->long_state); 
  	}
    else{
      cn_explode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->hash_state, (__m128i*)ctx[0]->long_state); 
   	  cn_explode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->hash_state, (__m128i*)ctx[1]->long_state); 
    }
  	__m128i ax0,ax0_; 
  	uint64_t idx0,idx0_; 
  	__m128i bx0,bx0_; 
  	uint8_t* l0 = ctx[0]->long_state;
  	uint8_t* l0_ = ctx[1]->long_state; 

  	/* BEGIN cryptonight_monero_v8 variables */ 
  	__m128i bx1,bx1_; 
	  __m128i division_result_xmm; 
  	__m128i sqrt_result; 
  	/* END cryptonight_monero_v8 variables */ 
  	{ 
  		uint64_t* h0 = (uint64_t*)ctx[0]->hash_state;
  		uint64_t* h0_ = (uint64_t*)ctx[1]->hash_state; 

  		idx0 = h0[0] ^ h0[4]; 
  		idx0_ = h0_[0] ^ h0_[4];
      ax0 = (__m128ll){idx0, h0[1] ^ h0[5]}; 
      ax0_ = (__m128ll){idx0_, h0_[1] ^ h0_[5]}; 
  		bx0 = (__m128ll){h0[2] ^ h0[6], h0[3] ^ h0[7]}; 
  		bx0_ = (__m128ll){h0_[2] ^ h0_[6], h0_[3] ^ h0_[7]}; 

      if(ALGO == cryptonight_monero_v8) 
  		{ 
  			bx1 = (__m128ll){h0[8] ^ h0[10], h0[9] ^ h0[11]}; 
  			bx1_ = (__m128ll){h0_[8] ^ h0_[10], h0_[9] ^ h0_[11]}; 
  			division_result_xmm = (__m128ll){h0[12],h0_[12]}; 
 	  		sqrt_result = (__m128ll){h0[13],h0_[13]};

  			set_float_rounding_mode(); 
  		} 
  	} 
  	__m128i *ptr0,*ptr0_;

		// Optim - 90% time boundary
		for(size_t i = 0; i < ITERATIONS; i++)
		{
    	__m128i cx,cx_; 
    	ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    	ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
    	cx = vec_ld(0,ptr0); 
    	cx_ = vec_ld(0,ptr0_); 
    	if (ALGO == cryptonight_bittube2) 
    	{ 
    		cx = aes_round_bittube2(cx, ax0); 
    		cx_ = aes_round_bittube2(cx_, ax0_); 
    	} 
    	else 
    	{ 
    			cx = _mm_aesenc_si128(cx, ax0); 
    			cx_ = _mm_aesenc_si128(cx_, ax0_); 
    	} 
    	/* Shuffle the other 3x16 byte chunks in the current 64-byte cache line */ 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		uint64_t idx1 = idx0 & MASK; 
    		__m128i chunk1 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x10]); 
    	 	__m128i chunk2 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x20]); 
    		__m128i chunk3 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x30]); 
    		*((__m128i *)&l0[idx1 ^ 0x10]) = (__m128ll)chunk3 + (__m128ll)bx1; 
        *((__m128i *)&l0[idx1 ^ 0x20]) = (__m128ll)chunk1 + (__m128ll)bx0;
        *((__m128i *)&l0[idx1 ^ 0x30]) = (__m128ll)chunk2 + (__m128ll)ax0;

        uint64_t idx1_ = idx0_ & MASK; 
    		__m128i chunk1_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x10]); 
    	  __m128i chunk2_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x20]); 
    	  __m128i chunk3_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x30]); 
    		*((__m128i *)&l0_[idx1_ ^ 0x10]) = (__m128ll)chunk3_ + (__m128ll)bx1_; 
        *((__m128i *)&l0_[idx1_ ^ 0x20]) = (__m128ll)chunk1_ + (__m128ll)bx0_;
        *((__m128i *)&l0_[idx1_ ^ 0x30]) = (__m128ll)chunk2_ + (__m128ll)ax0_;
    	}


    	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2){ 
    		cryptonight_monero_tweak<ALGO>((uint64_t*)ptr0, vec_xor(bx0, cx)); 
    		cryptonight_monero_tweak<ALGO>((uint64_t*)ptr0_, vec_xor(bx0_, cx_)); 
        }
    	else{ 
    		vec_st(vec_xor(bx0, cx), 0, (__m128i *)ptr0 );
    		vec_st(vec_xor(bx0_, cx_), 0, (__m128i *)ptr0_ ); 

        }
    	idx0 = ((uint64_t*)&cx)[0]; 
     	idx0_ = ((uint64_t*)&cx_)[0]; 
   	
    	ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    	ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
    	if(ALGO != cryptonight_monero_v8){ 
    		bx0 = cx;
    		bx0_ = cx_;
      }
    	uint64_t lo, cl, ch, lo_, cl_, ch_;  
    	uint64_t al0 = ((uint64_t*)&ax0)[0]; 
    	uint64_t al0_ = ((uint64_t*)&ax0_)[0]; 
    	uint64_t ah0 = ((uint64_t*)&ax0)[1]; 
    	uint64_t ah0_ = ((uint64_t*)&ax0_)[1]; 
    	cl = ((uint64_t*)ptr0)[0]; 
    	cl_ = ((uint64_t*)ptr0_)[0]; 
     	ch = ((uint64_t*)ptr0)[1]; 
    	ch_ = ((uint64_t*)ptr0_)[1]; 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		uint64_t sqrt_result_tmp_0,sqrt_result_tmp_1; 
    		sqrt_result_tmp_0 = ((uint64_t*)&sqrt_result)[0];
    		sqrt_result_tmp_1 = ((uint64_t*)&sqrt_result)[1];

    		/* Use division and square root results from the _previous_ iteration to hide the latency */ 
    		const uint64_t cx_64 = ((uint64_t*)&cx)[0];
    		const uint64_t cx_64_ = ((uint64_t*)&cx_)[0]; 

    		cl ^= static_cast<uint64_t>(((uint64_t*)&division_result_xmm)[0]) ^ (sqrt_result_tmp_0 << 32);
    		cl_ ^= static_cast<uint64_t>(((uint64_t*)&division_result_xmm)[1]) ^ (sqrt_result_tmp_1 << 32); 

    		const uint32_t d = (cx_64 + (sqrt_result_tmp_0 << 1)) | 0x80000001UL;
    		const uint32_t d_ = (cx_64_ + (sqrt_result_tmp_1 << 1)) | 0x80000001UL; 

    		/* Most and least significant bits in the divisor are set to 1 
    		 * to make sure we don't divide by a small or even number, 
    		 * so there are no shortcuts for such cases 
    		 * 
    		 * Quotient may be as large as (2^64 - 1)/(2^31 + 1) = 8589934588 = 2^33 - 4 
    		 * We drop the highest bit to fit both quotient and remainder in 32 bits 
		     */  
    		/* Compiler will optimize it to a single div instruction */ 
    		__m128i cx_v = vec_sro(cx,(__m128i){0x40}); 
    		__m128i cx_v_ = vec_sro(cx_,(__m128i){0x40}); 
        uint64_t cx_s = ((uint64_t*)&cx_v)[0]; 
        uint64_t cx_s_ = ((uint64_t*)&cx_v_)[0];
        //reuse cx_v and cx_v_
        cx_v = (__m128ll){cx_s,cx_s_};
        cx_v_ = (__m128ll)cx_v / (__m128ll){d,d_};
        cx_v = (__m128ll)cx_v % (__m128ll){d,d_};

    		uint64_t division_result_0 = static_cast<uint32_t>(((uint64_t*)&cx_v_)[0]) + (((uint64_t*)&cx_v)[0] << 32); 
    		uint64_t division_result_1 = static_cast<uint32_t>(((uint64_t*)&cx_v_)[1]) + (((uint64_t*)&cx_v)[1] << 32); 
        division_result_xmm = (__m128ll){division_result_0,division_result_1}; 
    		/* Use division_result as an input for the square root to prevent parallel implementation in hardware */ 
    	  if(PREFETCH)	
          sqrt_result =  int_sqrt33_1_double_precision2(cx_64 + division_result_0,cx_64_ + division_result_1); 
    	  else
          sqrt_result =  int_sqrt33_1_double_precision_fast2(cx_64 + division_result_0,cx_64_ + division_result_1); 

      }

    	{ 
    	  uint64_t hi,hi_;
    		lo = _umul128(idx0, cl, &hi); 
    		lo_ = _umul128(idx0_, cl_, &hi_); 
    		/* Shuffle the other 3x16 byte chunks in the current 64-byte cache line */ 
      	if(ALGO == cryptonight_monero_v8) 
        { 
       	  uint64_t idx1 = idx0 & MASK; 
   		    __m128i chunk1 =  (__m128ll){hi, lo};
          chunk1 = vec_xor(*(__m128i *)&l0[idx1 ^ 0x10], chunk1); 
       		__m128i chunk2 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x20]); 
       		hi ^= ((uint64_t*)&chunk2)[0]; 
       		lo ^= ((uint64_t*)&chunk2)[1]; 
       		__m128i chunk3 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x30]); 
         	*((__m128i *)&l0[idx1 ^ 0x10]) = (__m128ll)chunk3 + (__m128ll)bx1; 
          *((__m128i *)&l0[idx1 ^ 0x20]) = (__m128ll)chunk1 + (__m128ll)bx0;
          *((__m128i *)&l0[idx1 ^ 0x30]) = (__m128ll)chunk2 + (__m128ll)ax0;
        
          uint64_t idx1_ = idx0_ & MASK; 
   		    __m128i chunk1_ =  (__m128ll){hi_, lo_};
          chunk1_ = vec_xor(*(__m128i *)&l0_[idx1_ ^ 0x10], chunk1_); 
       		__m128i chunk2_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x20]); 
       		hi_ ^= ((uint64_t*)&chunk2_)[0]; 
       		lo_ ^= ((uint64_t*)&chunk2_)[1]; 
       		__m128i chunk3_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x30]); 
          *((__m128i *)&l0_[idx1_ ^ 0x10]) = (__m128ll)chunk3_ + (__m128ll)bx1_; 
          *((__m128i *)&l0_[idx1_ ^ 0x20]) = (__m128ll)chunk1_ + (__m128ll)bx0_;
          *((__m128i *)&l0_[idx1_ ^ 0x30]) = (__m128ll)chunk2_ + (__m128ll)ax0_;

      	}

        ah0 += lo; 
    		al0 += hi; 
        ah0_ += lo_; 
    		al0_ += hi_; 
    	} 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		bx1 = bx0; 
    		bx0 = cx; 
    		bx1_ = bx0_; 
    		bx0_ = cx_; 
    	} 
    	((uint64_t*)ptr0)[0] = al0;
    	((uint64_t*)ptr0_)[0] = al0_;

    	if (ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
    	{ 
    		if (ALGO == cryptonight_ipbc || ALGO == cryptonight_bittube2) {
    			((uint64_t*)ptr0)[1] = ah0 ^ monero_const_0 ^ ((uint64_t*)ptr0)[0]; 
    			((uint64_t*)ptr0_)[1] = ah0_ ^ monero_const_1 ^ ((uint64_t*)ptr0_)[0]; 
          }
    		else {
    			((uint64_t*)ptr0)[1] = ah0 ^ monero_const_0; 
    			((uint64_t*)ptr0_)[1] = ah0_ ^ monero_const_1; 
          }
    	} 
    	else {
    		((uint64_t*)ptr0)[1] = ah0; 
    		((uint64_t*)ptr0_)[1] = ah0_; 
        }
    	al0 ^= cl; 
    	al0_ ^= cl_; 
    	ah0 ^= ch; 
    	ah0_ ^= ch_; 
    	ax0 = (__m128ll){al0, ah0}; 
    	ax0_ = (__m128ll){al0_, ah0_}; 
    	idx0 = al0;
    	idx0_ = al0_;
    	if(ALGO == cryptonight_heavy || ALGO == cryptonight_bittube2) 
    	{ 
    		ptr0 = (__m128i *)&l0[idx0 & MASK]; 
      	ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
      	int64_t u  = ((int64_t*)ptr0)[0]; 
      	int64_t u_  = ((int64_t*)ptr0_)[0]; 
    		int32_t d  = ((int32_t*)ptr0)[2]; 
    		int32_t d_  = ((int32_t*)ptr0_)[2]; 
    		int64_t q = u / (d | 0x5); 
    		int64_t q_ = u_ / (d_ | 0x5); 
    		
    		((int64_t*)ptr0)[0] = u ^ q; 
    		((int64_t*)ptr0_)[0] = u_ ^ q_; 
    		idx0 = d ^ q; 
    		idx0_ = d_ ^ q_; 
    	} 
    	else if(ALGO == cryptonight_haven) 
    	{ 
    		ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    		ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
    		int64_t u  = ((int64_t*)ptr0)[0]; 
    		int64_t u_  = ((int64_t*)ptr0_)[0]; 
    		int32_t d  = ((int32_t*)ptr0)[2]; 
    		int32_t d_  = ((int32_t*)ptr0_)[2]; 
    		int64_t q = u / (d | 0x5); 
    		int64_t q_ = u_ / (d_ | 0x5); 
    		
    		((int64_t*)ptr0)[0] = u ^ q; 
    		((int64_t*)ptr0_)[0] = u_ ^ q_; 
    		idx0 = (~d) ^ q; 
    		idx0_ = (~d_) ^ q_; 
    	}

		}
  	/* Optim - 90% time boundary */ 
    if(SOFT_AES){
      cn_implode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->long_state, (__m128i*)ctx[0]->hash_state); 
    	cn_implode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->long_state, (__m128i*)ctx[1]->hash_state); 
    }
    else{
    	cn_implode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->long_state, (__m128i*)ctx[0]->hash_state); 
    	cn_implode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->long_state, (__m128i*)ctx[1]->hash_state); 
    }
  	/* Optim - 99% time boundary */ 
  	keccakf((uint64_t*)ctx[0]->hash_state, 24); 
  	keccakf((uint64_t*)ctx[1]->hash_state, 24); 
  	extra_hashes[ctx[0]->hash_state[0] & 3](ctx[0]->hash_state, 200, (char*)output + 32 * 0);
  	extra_hashes[ctx[1]->hash_state[0] & 3](ctx[1]->hash_state, 200, (char*)output + 32 * 1);

		}
};

template< >
struct Cryptonight_hash<3>
{
	static constexpr size_t N = 3;

	template<xmrstak_algo ALGO, bool SOFT_AES, bool PREFETCH>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{
  	constexpr size_t MASK = cn_select_mask<ALGO>();
		constexpr size_t ITERATIONS = cn_select_iter<ALGO>();
		constexpr size_t MEM = cn_select_memory<ALGO>();
    if((ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) && len < 43) 
  	{ 
  		memset(output, 0, 32 * N); 
  		return; 
  	}
  	keccak((const uint8_t *)input + len * 0, len, ctx[0]->hash_state, 200); 
  	keccak((const uint8_t *)input + len * 1, len, ctx[1]->hash_state, 200);
   	keccak((const uint8_t *)input + len * 2, len, ctx[2]->hash_state, 200); 

  	uint64_t monero_const_0; 
  	uint64_t monero_const_1;
    uint64_t monero_const_2; 
  	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
   	{ 
    	monero_const_0 =  *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + len * 0 + 35); 
    	monero_const_1 =  *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + len * 1 + 35); 
    	monero_const_2 =  *reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + len * 2 + 35); 
   		monero_const_0 ^=  *(reinterpret_cast<const uint64_t*>(ctx[0]->hash_state) + 24); 
   		monero_const_1 ^=  *(reinterpret_cast<const uint64_t*>(ctx[1]->hash_state) + 24); 
   		monero_const_2 ^=  *(reinterpret_cast<const uint64_t*>(ctx[2]->hash_state) + 24); 
  	} 
  	/* Optim - 99% time boundary */ 
    if(SOFT_AES){
      cn_explode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->hash_state, (__m128i*)ctx[0]->long_state); 
   	  cn_explode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->hash_state, (__m128i*)ctx[1]->long_state); 
   	  cn_explode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[2]->hash_state, (__m128i*)ctx[2]->long_state); 
  	}
    else{
      cn_explode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->hash_state, (__m128i*)ctx[0]->long_state); 
   	  cn_explode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->hash_state, (__m128i*)ctx[1]->long_state); 
   	  cn_explode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[2]->hash_state, (__m128i*)ctx[2]->long_state); 
    }
  	__m128i ax0,ax0_,ax0__; 
  	uint64_t idx0,idx0_,idx0__; 
  	__m128i bx0,bx0_,bx0__; 
  	uint8_t* l0 = ctx[0]->long_state;
  	uint8_t* l0_ = ctx[1]->long_state; 
  	uint8_t* l0__ = ctx[2]->long_state; 

  	/* BEGIN cryptonight_monero_v8 variables */ 
  	__m128i bx1,bx1_,bx1__; 
	  __m128i division_result_xmm; 
  	__m128i sqrt_result;
    uint64_t sqrt_result_ , division_result_;
  	/* END cryptonight_monero_v8 variables */ 
  	{ 
  		uint64_t* h0 = (uint64_t*)ctx[0]->hash_state;
  		uint64_t* h0_ = (uint64_t*)ctx[1]->hash_state; 
    	uint64_t* h0__ = (uint64_t*)ctx[2]->hash_state; 

  		idx0 = h0[0] ^ h0[4]; 
      ax0 = (__m128ll){idx0, h0[1] ^ h0[5]}; 
  		bx0 = (__m128ll){h0[2] ^ h0[6], h0[3] ^ h0[7]}; 
  		
      idx0_ = h0_[0] ^ h0_[4];
      ax0_ = (__m128ll){idx0_, h0_[1] ^ h0_[5]}; 
  		bx0_ = (__m128ll){h0_[2] ^ h0_[6], h0_[3] ^ h0_[7]}; 
     
      idx0__ = h0__[0] ^ h0__[4];
      ax0__ = (__m128ll){idx0__, h0__[1] ^ h0__[5]}; 
  		bx0__ = (__m128ll){h0__[2] ^ h0__[6], h0__[3] ^ h0__[7]}; 

      if(ALGO == cryptonight_monero_v8) 
  		{ 
  			bx1 = (__m128ll){h0[8] ^ h0[10], h0[9] ^ h0[11]}; 
  			bx1_ = (__m128ll){h0_[8] ^ h0_[10], h0_[9] ^ h0_[11]}; 
      	bx1__ = (__m128ll){h0__[8] ^ h0__[10], h0__[9] ^ h0__[11]}; 
  			division_result_xmm = (__m128ll){h0[12],h0_[12]}; 
 	  		sqrt_result = (__m128ll){h0[13],h0_[13]};
        division_result_ = h0__[12];
        sqrt_result_ = h0__[13];

  			set_float_rounding_mode(); 
  		} 
  	} 
  	__m128i *ptr0,*ptr0_,*ptr0__;

		// Optim - 90% time boundary
		for(size_t i = 0; i < ITERATIONS; i++)
		{
    	__m128i cx,cx_,cx__; 
    	ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    	cx = vec_ld(0,ptr0); 
    	
      ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
    	cx_ = vec_ld(0,ptr0_); 

      ptr0__ = (__m128i *)&l0__[idx0__ & MASK]; 
    	cx__ = vec_ld(0,ptr0__); 

    	if (ALGO == cryptonight_bittube2) 
    	{ 
    		cx = aes_round_bittube2(cx, ax0); 
    		cx_ = aes_round_bittube2(cx_, ax0_); 
    		cx__ = aes_round_bittube2(cx__, ax0__); 
    	} 
    	else 
    	{ 
    			cx = _mm_aesenc_si128(cx, ax0); 
    			cx_ = _mm_aesenc_si128(cx_, ax0_); 
    			cx__ = _mm_aesenc_si128(cx__, ax0__); 
    	} 
    	/* Shuffle the other 3x16 byte chunks in the current 64-byte cache line */ 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		uint64_t idx1 = idx0 & MASK; 
    		__m128i chunk1 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x10]); 
    	 	__m128i chunk2 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x20]); 
    		__m128i chunk3 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x30]); 
    		*((__m128i *)&l0[idx1 ^ 0x10]) = (__m128ll)chunk3 + (__m128ll)bx1; 
        *((__m128i *)&l0[idx1 ^ 0x20]) = (__m128ll)chunk1 + (__m128ll)bx0;
        *((__m128i *)&l0[idx1 ^ 0x30]) = (__m128ll)chunk2 + (__m128ll)ax0;

        uint64_t idx1_ = idx0_ & MASK; 
    		__m128i chunk1_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x10]); 
    	  __m128i chunk2_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x20]); 
    	  __m128i chunk3_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x30]); 
    		*((__m128i *)&l0_[idx1_ ^ 0x10]) = (__m128ll)chunk3_ + (__m128ll)bx1_; 
        *((__m128i *)&l0_[idx1_ ^ 0x20]) = (__m128ll)chunk1_ + (__m128ll)bx0_;
        *((__m128i *)&l0_[idx1_ ^ 0x30]) = (__m128ll)chunk2_ + (__m128ll)ax0_;

        uint64_t idx1__ = idx0__ & MASK; 
    		__m128i chunk1__ = vec_ld(0,(__m128i *)&l0__[idx1__ ^ 0x10]); 
    	  __m128i chunk2__ = vec_ld(0,(__m128i *)&l0__[idx1__ ^ 0x20]); 
    	  __m128i chunk3__ = vec_ld(0,(__m128i *)&l0__[idx1__ ^ 0x30]); 
    		*((__m128i *)&l0__[idx1__ ^ 0x10]) = (__m128ll)chunk3__ + (__m128ll)bx1__; 
        *((__m128i *)&l0__[idx1__ ^ 0x20]) = (__m128ll)chunk1__ + (__m128ll)bx0__;
        *((__m128i *)&l0__[idx1__ ^ 0x30]) = (__m128ll)chunk2__ + (__m128ll)ax0__;
    	}


    	if(ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2){ 
    		cryptonight_monero_tweak<ALGO>((uint64_t*)ptr0, vec_xor(bx0, cx)); 
    		cryptonight_monero_tweak<ALGO>((uint64_t*)ptr0_, vec_xor(bx0_, cx_));
    		cryptonight_monero_tweak<ALGO>((uint64_t*)ptr0__, vec_xor(bx0__, cx__)); 
      }
    	else{ 
    		vec_st(vec_xor(bx0, cx), 0, (__m128i *)ptr0 );
    		vec_st(vec_xor(bx0_, cx_), 0, (__m128i *)ptr0_ ); 
    		vec_st(vec_xor(bx0__, cx__), 0, (__m128i *)ptr0__ ); 
      }
    	idx0 = ((uint64_t*)&cx)[0]; 
   	  ptr0 = (__m128i *)&l0[idx0 & MASK]; 
     	
      idx0_ = ((uint64_t*)&cx_)[0]; 
    	ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
     
      idx0__ = ((uint64_t*)&cx__)[0]; 
    	ptr0__ = (__m128i *)&l0__[idx0__ & MASK]; 
	
      if(ALGO != cryptonight_monero_v8){ 
    		bx0 = cx;
    		bx0_ = cx_;
    		bx0__ = cx__;
      }
    	uint64_t lo, cl, ch, lo_, cl_, ch_, lo__,cl__,ch__;  
    	uint64_t al0 = ((uint64_t*)&ax0)[0]; 
    	uint64_t ah0 = ((uint64_t*)&ax0)[1]; 
    	cl = ((uint64_t*)ptr0)[0]; 
     	ch = ((uint64_t*)ptr0)[1]; 
    	
      uint64_t al0_ = ((uint64_t*)&ax0_)[0]; 
    	uint64_t ah0_ = ((uint64_t*)&ax0_)[1]; 
    	cl_ = ((uint64_t*)ptr0_)[0]; 
    	ch_ = ((uint64_t*)ptr0_)[1]; 
     
      uint64_t al0__ = ((uint64_t*)&ax0__)[0]; 
    	uint64_t ah0__ = ((uint64_t*)&ax0__)[1]; 
    	cl__ = ((uint64_t*)ptr0__)[0]; 
    	ch__ = ((uint64_t*)ptr0__)[1]; 
	
      if(ALGO == cryptonight_monero_v8) 
    	{ 
    		uint64_t sqrt_result_tmp_0,sqrt_result_tmp_1,sqrt_result_tmp_2; 
    		sqrt_result_tmp_0 = ((uint64_t*)&sqrt_result)[0];
    		sqrt_result_tmp_1 = ((uint64_t*)&sqrt_result)[1];
        sqrt_result_tmp_2 = sqrt_result_;
    		/* Use division and square root results from the _previous_ iteration to hide the latency */ 
    		const uint64_t cx_64 = ((uint64_t*)&cx)[0];
       	cl ^= static_cast<uint64_t>(((uint64_t*)&division_result_xmm)[0]) ^ (sqrt_result_tmp_0 << 32);
    	
        const uint64_t cx_64_ = ((uint64_t*)&cx_)[0]; 
    		cl_ ^= static_cast<uint64_t>(((uint64_t*)&division_result_xmm)[1]) ^ (sqrt_result_tmp_1 << 32); 

        const uint64_t cx_64__ = ((uint64_t*)&cx__)[0]; 
    		cl__ ^= static_cast<uint64_t>(division_result_) ^ (sqrt_result_tmp_2 << 32); 


    		const uint32_t d = (cx_64 + (sqrt_result_tmp_0 << 1)) | 0x80000001UL;
    		const uint32_t d_ = (cx_64_ + (sqrt_result_tmp_1 << 1)) | 0x80000001UL; 
    		const uint32_t d__ = (cx_64__ + (sqrt_result_tmp_2 << 1)) | 0x80000001UL; 

    		/* Most and least significant bits in the divisor are set to 1 
    		 * to make sure we don't divide by a small or even number, 
    		 * so there are no shortcuts for such cases 
    		 * 
    		 * Quotient may be as large as (2^64 - 1)/(2^31 + 1) = 8589934588 = 2^33 - 4 
    		 * We drop the highest bit to fit both quotient and remainder in 32 bits 
		     */  
    		/* Compiler will optimize it to a single div instruction */ 
    		__m128i cx_v = vec_sro(cx,(__m128i){0x40}); 
    		__m128i cx_v_ = vec_sro(cx_,(__m128i){0x40}); 
    		__m128i cx_v__ = vec_sro(cx__,(__m128i){0x40}); 
        uint64_t cx_s = ((uint64_t*)&cx_v)[0]; 
        uint64_t cx_s_ = ((uint64_t*)&cx_v_)[0];
        uint64_t cx_s__ = ((uint64_t*)&cx_v__)[0];
        //reuse cx_v and cx_v_
        cx_v = (__m128ll){cx_s,cx_s_};
        cx_v_ = (__m128ll)cx_v / (__m128ll){d,d_};
        cx_v = (__m128ll)cx_v % (__m128ll){d,d_};


    		uint64_t division_result_0 = static_cast<uint32_t>(((uint64_t*)&cx_v_)[0]) + (((uint64_t*)&cx_v)[0] << 32); 
    		uint64_t division_result_1 = static_cast<uint32_t>(((uint64_t*)&cx_v_)[1]) + (((uint64_t*)&cx_v)[1] << 32); 
        uint64_t division_result_2 = static_cast<uint32_t>(cx_s__ / d__) + ((cx_s__ % d__) << 32); 

        division_result_xmm = (__m128ll){division_result_0,division_result_1};
        division_result_ = division_result_2;
    		/* Use division_result as an input for the square root to prevent parallel implementation in hardware */ 
    	  if(PREFETCH){	
          sqrt_result =  int_sqrt33_1_double_precision2(cx_64 + division_result_0,cx_64_ + division_result_1); 
          sqrt_result_ =  int_sqrt33_1_double_precision(cx_64__ + division_result_2); 
        }
    	  else{
          sqrt_result =  int_sqrt33_1_double_precision_fast2(cx_64 + division_result_0,cx_64_ + division_result_1); 
          sqrt_result_ =  int_sqrt33_1_double_precision_fast(cx_64__ + division_result_2); 
        }
      }

    	{ 
    	  uint64_t hi,hi_,hi__;
    		lo = _umul128(idx0, cl, &hi); 
    		lo_ = _umul128(idx0_, cl_, &hi_); 
    		lo__ = _umul128(idx0__, cl__, &hi__); 
    		/* Shuffle the other 3x16 byte chunks in the current 64-byte cache line */ 
      	if(ALGO == cryptonight_monero_v8) 
        { 
       	  uint64_t idx1 = idx0 & MASK; 
   		    __m128i chunk1 =  (__m128ll){hi, lo};
          chunk1 = vec_xor(*(__m128i *)&l0[idx1 ^ 0x10], chunk1); 
       		__m128i chunk2 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x20]); 
       		hi ^= ((uint64_t*)&chunk2)[0]; 
       		lo ^= ((uint64_t*)&chunk2)[1]; 
       		__m128i chunk3 = vec_ld(0,(__m128i *)&l0[idx1 ^ 0x30]); 
         	*((__m128i *)&l0[idx1 ^ 0x10]) = (__m128ll)chunk3 + (__m128ll)bx1; 
          *((__m128i *)&l0[idx1 ^ 0x20]) = (__m128ll)chunk1 + (__m128ll)bx0;
          *((__m128i *)&l0[idx1 ^ 0x30]) = (__m128ll)chunk2 + (__m128ll)ax0;
        
          uint64_t idx1_ = idx0_ & MASK; 
   		    __m128i chunk1_ =  (__m128ll){hi_, lo_};
          chunk1_ = vec_xor(*(__m128i *)&l0_[idx1_ ^ 0x10], chunk1_); 
       		__m128i chunk2_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x20]); 
       		hi_ ^= ((uint64_t*)&chunk2_)[0]; 
       		lo_ ^= ((uint64_t*)&chunk2_)[1]; 
       		__m128i chunk3_ = vec_ld(0,(__m128i *)&l0_[idx1_ ^ 0x30]); 
          *((__m128i *)&l0_[idx1_ ^ 0x10]) = (__m128ll)chunk3_ + (__m128ll)bx1_; 
          *((__m128i *)&l0_[idx1_ ^ 0x20]) = (__m128ll)chunk1_ + (__m128ll)bx0_;
          *((__m128i *)&l0_[idx1_ ^ 0x30]) = (__m128ll)chunk2_ + (__m128ll)ax0_;

          uint64_t idx1__ = idx0__ & MASK; 
   		    __m128i chunk1__ =  (__m128ll){hi__, lo__};
          chunk1__ = vec_xor(*(__m128i *)&l0__[idx1__ ^ 0x10], chunk1__); 
       		__m128i chunk2__ = vec_ld(0,(__m128i *)&l0__[idx1__ ^ 0x20]); 
       		hi__ ^= ((uint64_t*)&chunk2__)[0]; 
       		lo__ ^= ((uint64_t*)&chunk2__)[1]; 
       		__m128i chunk3__ = vec_ld(0,(__m128i *)&l0__[idx1__ ^ 0x30]); 
          *((__m128i *)&l0__[idx1__ ^ 0x10]) = (__m128ll)chunk3__ + (__m128ll)bx1__; 
          *((__m128i *)&l0__[idx1__ ^ 0x20]) = (__m128ll)chunk1__ + (__m128ll)bx0__;
          *((__m128i *)&l0__[idx1__ ^ 0x30]) = (__m128ll)chunk2__ + (__m128ll)ax0__;
      	}

        ah0 += lo; 
    		al0 += hi; 
        ah0_ += lo_; 
    		al0_ += hi_; 
        ah0__ += lo__; 
    		al0__ += hi__; 
    	} 
    	if(ALGO == cryptonight_monero_v8) 
    	{ 
    		bx1 = bx0; 
    		bx0 = cx; 
    		bx1_ = bx0_; 
    		bx0_ = cx_; 
    		bx1__ = bx0__; 
    		bx0__ = cx__; 
    	} 
    	((uint64_t*)ptr0)[0] = al0;
    	((uint64_t*)ptr0_)[0] = al0_;
    	((uint64_t*)ptr0__)[0] = al0__;

    	if (ALGO == cryptonight_monero || ALGO == cryptonight_aeon || ALGO == cryptonight_ipbc || ALGO == cryptonight_stellite || ALGO == cryptonight_masari || ALGO == cryptonight_bittube2) 
    	{ 
    		if (ALGO == cryptonight_ipbc || ALGO == cryptonight_bittube2) {
    			((uint64_t*)ptr0)[1] = ah0 ^ monero_const_0 ^ ((uint64_t*)ptr0)[0]; 
    			((uint64_t*)ptr0_)[1] = ah0_ ^ monero_const_1 ^ ((uint64_t*)ptr0_)[0]; 
    			((uint64_t*)ptr0__)[1] = ah0__ ^ monero_const_2 ^ ((uint64_t*)ptr0__)[0]; 
        }
    		else {
    			((uint64_t*)ptr0)[1] = ah0 ^ monero_const_0; 
    			((uint64_t*)ptr0_)[1] = ah0_ ^ monero_const_1; 
    			((uint64_t*)ptr0__)[1] = ah0__ ^ monero_const_2; 
        }
    	} 
    	else {
    		((uint64_t*)ptr0)[1] = ah0; 
    		((uint64_t*)ptr0_)[1] = ah0_; 
     		((uint64_t*)ptr0__)[1] = ah0__; 
      }
    	al0 ^= cl; 
    	ah0 ^= ch; 
    	ax0 = (__m128ll){al0, ah0}; 
    	idx0 = al0;
    	
      al0_ ^= cl_; 
    	ah0_ ^= ch_; 
    	ax0_ = (__m128ll){al0_, ah0_}; 
    	idx0_ = al0_;
      
      al0__ ^= cl__; 
    	ah0__ ^= ch__; 
    	ax0__ = (__m128ll){al0__, ah0__}; 
    	idx0__ = al0__;
   
      if(ALGO == cryptonight_heavy || ALGO == cryptonight_bittube2) 
    	{ 
    		ptr0 = (__m128i *)&l0[idx0 & MASK]; 
      	int64_t u  = ((int64_t*)ptr0)[0]; 
    		int32_t d  = ((int32_t*)ptr0)[2]; 
    	  int64_t q = u / (d | 0x5); 
    		((int64_t*)ptr0)[0] = u ^ q; 
    		idx0 = d ^ q; 
      	
        ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
      	int64_t u_  = ((int64_t*)ptr0_)[0]; 
    		int32_t d_  = ((int32_t*)ptr0_)[2]; 
    		int64_t q_ = u_ / (d_ | 0x5); 
    		((int64_t*)ptr0_)[0] = u_ ^ q_; 
    		idx0_ = d_ ^ q_;

        ptr0__ = (__m128i *)&l0__[idx0__ & MASK]; 
      	int64_t u__  = ((int64_t*)ptr0__)[0]; 
    		int32_t d__  = ((int32_t*)ptr0__)[2]; 
    		int64_t q__ = u__ / (d__ | 0x5); 
    		((int64_t*)ptr0__)[0] = u__ ^ q__; 
    		idx0__ = d__ ^ q__; 

    	} 
    	else if(ALGO == cryptonight_haven) 
    	{ 
    		ptr0 = (__m128i *)&l0[idx0 & MASK]; 
    		int64_t u  = ((int64_t*)ptr0)[0]; 
    		int32_t d  = ((int32_t*)ptr0)[2]; 
    		int64_t q = u / (d | 0x5); 
    		((int64_t*)ptr0)[0] = u ^ q; 
    		idx0 = (~d) ^ q; 
    		
        ptr0_ = (__m128i *)&l0_[idx0_ & MASK]; 
    		int64_t u_  = ((int64_t*)ptr0_)[0]; 
    		int32_t d_  = ((int32_t*)ptr0_)[2]; 
    		int64_t q_ = u_ / (d_ | 0x5); 
    		((int64_t*)ptr0_)[0] = u_ ^ q_; 
    		idx0_ = (~d_) ^ q_;

        ptr0__ = (__m128i *)&l0__[idx0__ & MASK]; 
    		int64_t u__  = ((int64_t*)ptr0__)[0]; 
    		int32_t d__  = ((int32_t*)ptr0__)[2]; 
    		int64_t q__ = u__ / (d__ | 0x5); 
    		((int64_t*)ptr0__)[0] = u__ ^ q__; 
    		idx0__ = (~d__) ^ q__; 
    	}

		}
  	/* Optim - 90% time boundary */ 
    if(SOFT_AES){
      cn_implode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->long_state, (__m128i*)ctx[0]->hash_state); 
    	cn_implode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->long_state, (__m128i*)ctx[1]->hash_state); 
    	cn_implode_scratchpad<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[2]->long_state, (__m128i*)ctx[2]->hash_state); 
    }
    else{
    	cn_implode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[0]->long_state, (__m128i*)ctx[0]->hash_state); 
    	cn_implode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[1]->long_state, (__m128i*)ctx[1]->hash_state); 
    	cn_implode_scratchpad_be<MEM, SOFT_AES, PREFETCH, ALGO>((__m128i*)ctx[2]->long_state, (__m128i*)ctx[2]->hash_state); 
    }
  	/* Optim - 99% time boundary */ 
  	keccakf((uint64_t*)ctx[0]->hash_state, 24); 
  	keccakf((uint64_t*)ctx[1]->hash_state, 24); 
  	keccakf((uint64_t*)ctx[2]->hash_state, 24); 
  	extra_hashes[ctx[0]->hash_state[0] & 3](ctx[0]->hash_state, 200, (char*)output + 32 * 0);
  	extra_hashes[ctx[1]->hash_state[0] & 3](ctx[1]->hash_state, 200, (char*)output + 32 * 1);
  	extra_hashes[ctx[2]->hash_state[0] & 3](ctx[2]->hash_state, 200, (char*)output + 32 * 2);
	}
};

template< >
struct Cryptonight_hash<4>
{
	static constexpr size_t N = 4;

	template<xmrstak_algo ALGO, bool SOFT_AES, bool PREFETCH>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{

	}
};

template< >
struct Cryptonight_hash<5>
{
	static constexpr size_t N = 5;

	template<xmrstak_algo ALGO, bool SOFT_AES, bool PREFETCH>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{
	

	}
};


template< size_t N, size_t asm_version>
struct Cryptonight_hash_asm;

template<size_t asm_version>
struct Cryptonight_hash_asm<1, asm_version>
{
	static constexpr size_t N = 1;

	template<xmrstak_algo ALGO>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{
	}
};

// double hash only for intel
template< >
struct Cryptonight_hash_asm<2, 0>
{
	static constexpr size_t N = 2;

	template<xmrstak_algo ALGO>
	static void hash(const void* input, size_t len, void* output, cryptonight_ctx** ctx)
	{
	}
};
