/*
 * Copyright (C) 2022-2023, Advanced Micro Devices. All rights reserved.
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

#include "alcp/cipher/aes.hh"
#include "alcp/cipher/cipher_wrapper.hh"

#include "alcp/utils/cpuid.hh"

using alcp::utils::CpuId;

namespace alcp::cipher {

alc_error_t
cryptCtr(const Uint8* pInputText, // ptr to plaintext for encrypt
                                  // and ciphertext for decrypt
         Uint8* pOutputText,      // ptr to ciphertext for encrypt and
                                  // plaintext for decrypt
         Uint64       len,        // message length in bytes
         const Uint8* pKey,       // ptr to Key
         int          nRounds,    // No. of rounds
         const Uint8* pIv,        // ptr to Initialization Vector
         bool         isVaes,
         bool         isAvx512Cap)
{
    alc_error_t err     = ALC_ERROR_NONE;
    Uint64      blocks  = len / Rijndael::cBlockSize;
    auto        pkey128 = reinterpret_cast<const __m128i*>(pKey);

    if (isVaes && isAvx512Cap) {
        blocks = vaes512::ctrProcessAvx512(
            pInputText, pOutputText, blocks, pkey128, pIv, nRounds);
    } else if (isVaes) {

        blocks = vaes::ctrProcessAvx256(
            pInputText, pOutputText, blocks, pkey128, pIv, nRounds);

    } else {
        blocks = aesni::ctrProcessAvx2(
            pInputText, pOutputText, blocks, pkey128, pIv, nRounds);
    }

    return err;
}

alc_error_t
Ctr::decrypt(const Uint8* pCipherText,
             Uint8*       pPlainText,
             Uint64       len,
             const Uint8* pIv) const
{
    alc_error_t err         = ALC_ERROR_NONE;
    bool        isVaes      = false;
    bool        isAvx512Cap = false;
    if (CpuId::cpuHasVaes()) {
        isVaes = true;
        if (CpuId::cpuHasAvx512(utils::AVX512_F)
            && CpuId::cpuHasAvx512(utils::AVX512_DQ)
            && CpuId::cpuHasAvx512(utils::AVX512_BW)) {
            isAvx512Cap = true;
        }
    }

    err = cryptCtr(pCipherText,
                   pPlainText,
                   len,
                   getEncryptKeys(),
                   getRounds(),
                   pIv,
                   isVaes,
                   isAvx512Cap);

    return err;
}

alc_error_t
Ctr::encrypt(const Uint8* pPlainText,
             Uint8*       pCipherText,
             Uint64       len,
             const Uint8* pIv) const
{
    alc_error_t err         = ALC_ERROR_NONE;
    bool        isVaes      = false;
    bool        isAvx512Cap = false;
    if (CpuId::cpuHasVaes()) {
        isVaes = true;
        if (CpuId::cpuHasAvx512(utils::AVX512_F)
            && CpuId::cpuHasAvx512(utils::AVX512_DQ)
            && CpuId::cpuHasAvx512(utils::AVX512_BW)) {
            isAvx512Cap = true;
        }
    }
    err = cryptCtr(pPlainText,
                   pCipherText,
                   len,
                   getEncryptKeys(),
                   getRounds(),
                   pIv,
                   isVaes,
                   isAvx512Cap);

    return err;
}

} // namespace alcp::cipher