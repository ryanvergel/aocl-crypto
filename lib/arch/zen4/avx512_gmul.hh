/*
 * Copyright (C) 2021-2023, Advanced Micro Devices. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <immintrin.h>

#include "alcp/error.h"

/*
 *
 * Galois Multiplicaiton we use below algorithms from "Intel carry-less
 * multiplication instruction in gcm mode"
 * https://www.intel.cn/content/dam/www/public/us/en/documents/white-papers/carry-less-multiplication-instruction-in-gcm-mode-paper.pdf
 *     1. Aggregated Reduction and
 *     2. ModuloReduction algorithms
 *     3. Avoiding bit-reflection by modifying precomputed HashKey table as per
 * below paper
 *          Vinodh Gopal et. al. Optimized Galois-Counter-Mode Implementation
 * on Intel® Architecture Processors. Intel White Paper, August 2010.
 *
 *
 */

namespace alcp::cipher { namespace vaes512 {

    /*
     * Modulo Reduction of 256bit to 128bit
     * Modulo reduction algorithm 4 (Montgomery Reduction) in
     *
     * "Shay Gueron. AES-GCM for Efficient Authenticated Encryption -
     * Ending the reign of HMAC-SHA-1?, Workshop on Real-World Cryptography,
     * 2013. https://crypto.stanford.edu/RealWorldCrypto/slides/gueron.pdf.
     */

#define SWIZZLE(a, b, c, d) (((a) << 0) | ((b) << 2) | ((c) << 4) | ((d) << 6))

    static const Uint64 const_factor[] = { 0x1, 0xC200000000000000,
                                           0x1, 0xC200000000000000,
                                           0x1, 0xC200000000000000,
                                           0x1, 0xC200000000000000 };

    static inline void montgomeryReduction(__m256i       input_256,
                                           __m128i*      result,
                                           const __m256i const_factor_256)
    {
        __m256i       mul, rev, high_256;
        constexpr int cSwizzle = SWIZZLE(2, 3, 0, 1);

        high_256 =
            _mm256_permute4x64_epi64(input_256, cSwizzle); // move hi to low.

        // A1:A0
        mul = _mm256_clmulepi64_epi128(input_256, const_factor_256, 0x10);

        // X0:X1
        rev = _mm256_shuffle_epi32(input_256, cSwizzle);
        // rev = _mm256_permute4x64_epi64(low_256, SWIZZLE(1, 0, 3, 2));

        // B1:B0
        input_256 = _mm256_xor_si256(mul, rev);

        mul       = _mm256_clmulepi64_epi128(input_256, const_factor_256, 0x10);
        rev       = _mm256_shuffle_epi32(input_256, cSwizzle);
        input_256 = _mm256_xor_si256(mul, rev);

        mul     = _mm256_xor_si256(high_256, input_256);
        *result = _mm256_castsi256_si128(mul);
    }

    static inline __m128i amd512_horizontal_sum128(__m512i x_512)
    {
        __m256i a_256, b_256;
        __m128i a_128, b_128;

        a_256 = _mm512_extracti64x4_epi64(x_512, 0);
        b_256 = _mm512_extracti64x4_epi64(x_512, 1);

        a_256 = _mm256_xor_si256(a_256, b_256);

        a_128 = _mm256_extracti32x4_epi32(a_256, 1);
        b_128 = _mm256_extracti32x4_epi32(a_256, 0);

        a_128 = _mm_xor_si128(a_128, b_128);

        return a_128;
    }

    static inline __m512i amd512xorLast128bit(__m512i a, __m128i b_128)
    {
        // a3:a2:a1:(a0 xor b_128)
        Uint64* b_64  = (Uint64*)&b_128;
        __m512i b_512 = _mm512_set_epi64(0, 0, 0, 0, 0, 0, b_64[1], b_64[0]);
        return _mm512_mask_xor_epi64(a, 3, a, b_512);
    }

    static inline __m512i amd512xorLast128bit(__m512i a, __m512i b)
    {
        // a3:a2:a1:(a0 xor b)
        return _mm512_mask_xor_epi64(a, 3, a, b);
    }

    static inline void computeKaratsuba_Z0_Z2(const __m512i H_512,
                                              const __m512i abcd_512,
                                              __m512i*      z0_512,
                                              __m512i*      z2_512)
    {
        // compute x0y0
        // (Xi • H1) :  (Xi-1 • H2) : (Xi-2 • H3) : (Xi-3+Yi-4) •H4
        *z0_512 = _mm512_clmulepi64_epi128(H_512, abcd_512, 0x00);

        // compute x1y1
        *z2_512 = _mm512_clmulepi64_epi128(H_512, abcd_512, 0x11);
    }

    static inline void computeKaratsuba_Z1(const __m512i H_512,
                                           const __m512i abcd_512,
                                           __m512i*      pz1_512)
    {
        __m512i H_512_high, abcd_512_high;
        H_512_high    = _mm512_bsrli_epi128(H_512, 8);
        abcd_512_high = _mm512_bsrli_epi128(abcd_512, 8);

        H_512_high    = _mm512_xor_si512(H_512_high, H_512);
        abcd_512_high = _mm512_xor_si512(abcd_512_high, abcd_512);

        *pz1_512 = _mm512_clmulepi64_epi128(H_512_high, abcd_512_high, 0x00);
    }

    static inline void computeKaratsuba_Z0_Z2_acc(__m512i  H_512,
                                                  __m512i  abcd_512,
                                                  __m512i* pz0_512,
                                                  __m512i* pz2_512)
    {
        __m512i z0_temp;
        // compute x0y0
        // (Xi • H1) :  (Xi-1 • H2) : (Xi-2 • H3) : (Xi-3+Yi-4) •H4
        z0_temp = _mm512_clmulepi64_epi128(H_512, abcd_512, 0x00);

        // compute x1y1
        H_512 = _mm512_clmulepi64_epi128(H_512, abcd_512, 0x11);

        // accumulate with verious z0
        *pz0_512 = _mm512_xor_si512(z0_temp, *pz0_512);

        // accumulate with verious z2
        *pz2_512 = _mm512_xor_si512(H_512, *pz2_512);
    }

    static inline void computeKaratsuba_Z1_acc(__m512i  H_512,
                                               __m512i  abcd_512,
                                               __m512i* pz1_512)
    {
        __m512i H_512_high, abcd_512_high;
        H_512_high    = _mm512_bsrli_epi128(H_512, 8);
        abcd_512_high = _mm512_bsrli_epi128(abcd_512, 8);

        H_512_high    = _mm512_xor_si512(H_512_high, H_512);
        abcd_512_high = _mm512_xor_si512(abcd_512_high, abcd_512);

        H_512_high = _mm512_clmulepi64_epi128(H_512_high, abcd_512_high, 0x00);

        // accumulate with verious z1
        *pz1_512 = _mm512_xor_si512(H_512_high, *pz1_512);
    }

    /* Aggregated reduction method + Karatsuba algorithm */
    static inline void computeKaratsubaComponents(const __m512i H_512,
                                                  const __m512i abcd_512,
                                                  __m512i*      pz0_512,
                                                  __m512i*      pz1_512,
                                                  __m512i*      pz2_512)
    {
        /*
         *  Karatsuba algorithm to multiply two elements x,y
         *  Elements x,y are split as two equal 64 bit elements each.
         *  x = x1:x0
         *  y = y1:y0
         *
         *  compute z2 and z0
         *  z0 = x0y0
         *  z2 = x1y1
         *
         *  Reduce two multiplications in z1 to one.
         *  original: z1 = x1y0 + x0y1
         *  Reduced : z1 = (x1+x0) (y1+y0) - z2 - z0
         *
         *  Aggregrated Reduction:
         *  [(Xi • H1) + (Xi-1 • H2) + (Xi-2 • H3) + (Xi-3+Yi-4) •H4] modP
         *
         */
        computeKaratsuba_Z0_Z2(H_512, abcd_512, pz0_512, pz2_512);

        /* To compute: z1 = (x1+x0) (y1+y0) - z2 - z0
         * compute (x1+x0) (y1+y0) part in below function */
        computeKaratsuba_Z1(H_512, abcd_512, pz1_512);
    }

    /* Aggregated reduction method + Karatsuba algorithm */
    static inline void computeKaratsubaComponentsAccumulate(__m512i  H_512,
                                                            __m512i  abcd_512,
                                                            __m512i* pz0_512,
                                                            __m512i* pz1_512,
                                                            __m512i* pz2_512)
    {
        /*
         *  Karatsuba algorithm to multiply two elements x,y
         *  Elements x,y are split as two equal 64 bit elements each.
         *  x = x1:x0
         *  y = y1:y0
         *
         *  compute z2 and z0
         *  z0 = x0y0
         *  z2 = x1y1
         *
         *  Reduce two multiplications in z1 to one.
         *  original: z1 = x1y0 + x0y1
         *  Reduced : z1 = (x1+x0) (y1+y0) - z2 - z0
         *
         *  Aggregrated Reduction:
         *  [(Xi • H1) + (Xi-1 • H2) + (Xi-2 • H3) + (Xi-3+Yi-4) •H4] modP
         *
         */
        computeKaratsuba_Z0_Z2_acc(H_512, abcd_512, pz0_512, pz2_512);

        /* To compute: z1 = (x1+x0) (y1+y0) - z2 - z0
         * compute (x1+x0) (y1+y0) part in below function */
        computeKaratsuba_Z1_acc(H_512, abcd_512, pz1_512);
    }

    static inline __m512i amd512_reverse512_xorLast128bit(
        __m512i a, __m512i reverse_mask_512, __m128i res)
    {
        a = _mm512_shuffle_epi8(a, reverse_mask_512);
        return amd512xorLast128bit(a, res);
    }

    static inline void computeKaratsubaMul(__m128i  z0,
                                           __m128i  z1,
                                           __m128i  z2,
                                           __m256i* res)
    {
        __m128i a1;
        *res = _mm256_set_m128i(z2, z0);
        /*
         * compute:    z1 = (x1+x0) (y1+y0) - z2 - z0
         *
         * inputParam: z1 = (x1+x0) (y1+y0) */

        // (x1+x0) (y1+y0) - zo -z2 = (x1+x0) (y1+y0) xor z0 xor z2
        z1 = _mm_xor_si128(z1, z0);
        z1 = _mm_xor_si128(z1, z2);

        a1 = _mm_slli_si128(z1, 8);
        z1 = _mm_srli_si128(z1, 8);

        __m256i temp = _mm256_set_m128i(z1, a1);
        *res         = _mm256_xor_si256(temp, *res);
    }

    static inline void gMulR(__m512i       H_512,
                             __m512i       abcd_512,
                             __m512i       reverse_mask_512,
                             __m128i*      res,
                             const __m256i const_factor_256)
    {
        __m512i z0_512, z1_512, z2_512;
        __m128i z0, z1, z2;

        abcd_512 =
            amd512_reverse512_xorLast128bit(abcd_512, reverse_mask_512, *res);
        computeKaratsubaComponents(H_512, abcd_512, &z0_512, &z1_512, &z2_512);

        /* compute: z0 = x0y0
         *        z0 component of below equation:
         *        [(Xi • H1) + (Xi-1 • H2) + (Xi-2 • H3) + (Xi-3+Yi-4) •H4]
         *
         *  compute: z2 = x1y1
         *        z2 component of below equation:
         *        [(Xi • H1) + (Xi-1 • H2) + (Xi-2 • H3) + (Xi-3+Yi-4) •H4]
         */
        z0 = amd512_horizontal_sum128(z0_512);
        z2 = amd512_horizontal_sum128(z2_512);
        z1 = amd512_horizontal_sum128(z1_512);

        __m256i res_256;
        computeKaratsubaMul(z0, z1, z2, &res_256);
        montgomeryReduction(res_256, res, const_factor_256);
    }

    static inline void gMulParallel4(__m512i*      res,
                                     __m512i       H4321_512,
                                     __m512i       H4444_512,
                                     const __m512i const_factor_512)
    {
        __m512i z0_512, z1_512, z1L_512, z2_512;

        computeKaratsubaComponents(
            H4321_512, H4444_512, &z0_512, &z1_512, &z2_512);

        /* compute: z0 = x0y0
         *        z0 component of below equation:
         *        [(Xi • H1) + (Xi-1 • H2) + (Xi-2 • H3) + (Xi-3+Yi-4) •H4]
         *
         *  compute: z2 = x1y1
         *        z2 component of below equation:
         *        [(Xi • H1) + (Xi-1 • H2) + (Xi-2 • H3) + (Xi-3+Yi-4) •H4]
         */

        // z1 - zo -z2 = z1 xor z0 xor z2
        z1_512 = _mm512_xor_si512(z1_512, z0_512);
        z1_512 = _mm512_xor_si512(z1_512, z2_512);

        // z1Low64bit
        z1L_512 = _mm512_bslli_epi128(z1_512, 8);
        // z1High64bit
        z1_512 = _mm512_bsrli_epi128(z1_512, 8);

        // low 128bit CLMul result for 4 GHASH
        z0_512 = _mm512_xor_si512(z0_512, z1L_512);
        // high 128bit CLMul result for 4 GHASH
        z2_512 = _mm512_xor_si512(z2_512, z1_512);

        /* Modulo reduction of (high 128bit: low 128bit)  components to 128bit
         * Fast modulo reduction  Algorithm 4 in
         * https://crypto.stanford.edu/RealWorldCrypto/slides/gueron.pdf
         *
         */
        // A1:A0 = X0 *  0xc200000000000000
        z1_512 = _mm512_clmulepi64_epi128(z0_512, const_factor_512, 0x10);
        // shuffle to X0:X1
        z0_512 = _mm512_shuffle_epi32(z0_512, _MM_PERM_BADC);
        // B1:B0 = X0 + A1: X1 + A0
        z1_512 = _mm512_xor_epi64(z1_512, z0_512);
        // C1:C0 = B0 *  0xc200000000000000
        z0_512 = _mm512_clmulepi64_epi128(z1_512, const_factor_512, 0x10);
        // shuffle to B0:B1
        z1_512 = _mm512_shuffle_epi32(z1_512, _MM_PERM_BADC);

        // D1:D0 = B0 + C1: B1 + C0
        z0_512 = _mm512_xor_epi64(z1_512, z0_512);
        // D1 + X3: D0 + X2
        *res = _mm512_xor_epi64(z2_512, z0_512);
    }

    static inline __m512i amd512_xor_all(__m512i x0,
                                         __m512i x1,
                                         __m512i x2,
                                         __m512i x3)
    {
        x0 = _mm512_xor_si512(x0, x1);
        x0 = _mm512_xor_si512(x0, x2);
        return _mm512_xor_si512(x0, x3);
    }

    /* 16 blocks aggregated reduction
     * Galois field Multiplication of 16 blocks followed by one modulo
     * Reducation
     */
    static inline void gMulR(__m512i       H1,
                             __m512i       H2,
                             __m512i       H3,
                             __m512i       H4,
                             __m512i       a,
                             __m512i       b,
                             __m512i       c,
                             __m512i       d,
                             __m512i       reverse_mask_512,
                             __m128i*      res,
                             const __m256i const_factor_256)
    {

        __m128i z0, z1, z2;
        __m512i z0_512, z1_512, z2_512;

        // reverseInput
        a = _mm512_shuffle_epi8(a, reverse_mask_512);
        b = _mm512_shuffle_epi8(b, reverse_mask_512);
        c = _mm512_shuffle_epi8(c, reverse_mask_512);
        d = _mm512_shuffle_epi8(d, reverse_mask_512);

        a = amd512xorLast128bit(a, *res);

        computeKaratsubaComponents(H4, a, &z0_512, &z1_512, &z2_512);

        // b
        computeKaratsubaComponentsAccumulate(H3, b, &z0_512, &z1_512, &z2_512);

        // c
        computeKaratsubaComponentsAccumulate(H2, c, &z0_512, &z1_512, &z2_512);

        // d
        computeKaratsubaComponentsAccumulate(H1, d, &z0_512, &z1_512, &z2_512);

        z0 = amd512_horizontal_sum128(z0_512);
        z1 = amd512_horizontal_sum128(z1_512);
        z2 = amd512_horizontal_sum128(z2_512);

        __m256i res_256;
        computeKaratsubaMul(z0, z1, z2, &res_256);
        montgomeryReduction(res_256, res, const_factor_256);
    }

    /*
     * For 16 blocks (4*512 bit), Compute Karatsuba comonents z0, z1 and z2
     *
     * Each 512 bit zmm register contains 4 blocks of 128 bit.
     * 4 * 512 bit = 4 * 4 blocks = 16 blocks
     */
    static inline void get_aggregated_karatsuba_components(
        __m512i  H1,
        __m512i  H2,
        __m512i  H3,
        __m512i  H4,
        __m512i  a,
        __m512i  b,
        __m512i  c,
        __m512i  d,
        __m512i  reverse_mask_512,
        __m512i* pz0_512,
        __m512i* pz1_512,
        __m512i* pz2_512,
        __m128i  res,
        bool     isFirst)
    {
        // reverseInput
        a = _mm512_shuffle_epi8(a, reverse_mask_512);
        if (isFirst) {
            a = amd512xorLast128bit(a, res);
        }
        b = _mm512_shuffle_epi8(b, reverse_mask_512);
        c = _mm512_shuffle_epi8(c, reverse_mask_512);
        d = _mm512_shuffle_epi8(d, reverse_mask_512);

        __m512i z0_512_a, z1_512_a, z2_512_a;
        __m512i z0_512_b, z1_512_b, z2_512_b;
        __m512i z0_512_c, z1_512_c, z2_512_c;
        __m512i z0_512_d, z1_512_d, z2_512_d;
        computeKaratsubaComponents(H4, a, &z0_512_a, &z1_512_a, &z2_512_a);
        // b
        computeKaratsubaComponents(H3, b, &z0_512_b, &z1_512_b, &z2_512_b);
        // c
        computeKaratsubaComponents(H2, c, &z0_512_c, &z1_512_c, &z2_512_c);
        // d
        computeKaratsubaComponents(H1, d, &z0_512_d, &z1_512_d, &z2_512_d);
        *pz0_512 = amd512_xor_all(z0_512_a, z0_512_b, z0_512_c, z0_512_d);
        *pz1_512 = amd512_xor_all(z1_512_a, z1_512_b, z1_512_c, z1_512_d);
        *pz2_512 = amd512_xor_all(z2_512_a, z2_512_b, z2_512_c, z2_512_d);
    }

    static inline void getGhash(__m512i       z0_512,
                                __m512i       z1_512,
                                __m512i       z2_512,
                                __m128i*      res,
                                const __m256i const_factor_256)
    {

        __m128i z0 = amd512_horizontal_sum128(z0_512);
        __m128i z1 = amd512_horizontal_sum128(z1_512);
        __m128i z2 = amd512_horizontal_sum128(z2_512);

        __m256i res_256;
        computeKaratsubaMul(z0, z1, z2, &res_256);
        montgomeryReduction(res_256, res, const_factor_256);
    }

    /* 128 bit gMul with montogomery reduction */
    static inline void carrylessMul(__m128i  a,
                                    __m128i  b,
                                    __m128i* c,
                                    __m128i* d)
    {
        __m128i e, f;
        /* carryless multiplication of a1:a0 * b1:b0 */
        *c = _mm_clmulepi64_si128(a, b, 0x00); // C1:C0 = a0*b0
        *d = _mm_clmulepi64_si128(a, b, 0x11); // D1:D0 = a1*b1
        e  = _mm_clmulepi64_si128(a, b, 0x10); // E1:E0 = a0*b1
        f  = _mm_clmulepi64_si128(a, b, 0x01); // F1:F0 = a1*b0
        /*
         * compute D1  :  D0+E1+F1 : C1+E0+F0: C0
         */
        e = _mm_xor_si128(e, f);  // E1+F1 : E0+F0
        f = _mm_slli_si128(e, 8); // E0+F0:0
        e = _mm_srli_si128(e, 8); // 0:E1+F1

        /* d : c = D1 : D0+E1+F1 : C1+E0+F1 : C0 */
        *c = _mm_xor_si128(*c, f); // C1+(E0+F1):C0
        *d = _mm_xor_si128(*d, e); // D1:D0+(E1+F1)
    }

    static inline void gMul(__m128i       a,
                            __m128i       b,
                            __m128i*      res,
                            const __m256i const_factor_256)
    {
        __m128i c, d;
        carrylessMul(a, b, &c, &d);
        __m256i cd = _mm256_set_m128i(d, c);
        montgomeryReduction(cd, res, const_factor_256);
    }

    static inline void gMulR(__m128i       a,
                             __m128i       b,
                             __m128i       reverse_mask_128,
                             __m128i*      res,
                             const __m256i const_factor_256)
    {
        a    = _mm_shuffle_epi8(a, reverse_mask_128);
        *res = _mm_xor_si128(a, *res);

        __m128i c, d;
        carrylessMul(*res, b, &c, &d);
        __m256i cd = _mm256_set_m128i(d, c);
        montgomeryReduction(cd, res, const_factor_256);
    }

}} // namespace alcp::cipher::vaes512
