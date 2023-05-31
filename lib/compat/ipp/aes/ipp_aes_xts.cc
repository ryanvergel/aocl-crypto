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

#include "aes/ipp_aes_common.hh"

IppStatus
ippsAES_XTSEncrypt(const Ipp8u*           pSrc,
                   Ipp8u*                 pDst,
                   int                    bitSizeLen,
                   const IppsAES_XTSSpec* pCtx,
                   const Ipp8u*           pTweak,
                   int                    startCipherBlkNo)
{
    printMsg("XTSEncrypt Start");
    ipp_wrp_aes_ctx* context_enc =
        &(((ipp_wrp_aes_xts_ctx*)(pCtx))->encrypt_ctx);
    return alcp_encdecAES(pSrc,
                          pDst,
                          bitSizeLen / 8,
                          (IppsAESSpec*)context_enc,
                          pTweak,
                          startCipherBlkNo,
                          ALC_AES_MODE_XTS,
                          true);
    printMsg("XTSEncrypt End");
}

IppStatus
ippsAES_XTSDecrypt(const Ipp8u*           pSrc,
                   Ipp8u*                 pDst,
                   int                    bitSizeLen,
                   const IppsAES_XTSSpec* pCtx,
                   const Ipp8u*           pTweak,
                   int                    startCipherBlkNo)
{
    printMsg("XTSDecrypt Start");
    ipp_wrp_aes_ctx* context_dec =
        &(((ipp_wrp_aes_xts_ctx*)(pCtx))->decrypt_ctx);
    return alcp_encdecAES(pSrc,
                          pDst,
                          bitSizeLen / 8,
                          (IppsAESSpec*)context_dec,
                          pTweak,
                          startCipherBlkNo,
                          ALC_AES_MODE_XTS,
                          false);
    printMsg("XTSDecrypt End");
}
