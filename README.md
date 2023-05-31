# About AOCL Crypto

**AOCL-Crypto** is a library consisting of basic cryptographic functions optimized and tuned for AMD Zen™ based microarchitecture. This library provides a unified solution for Cryptographic routines such as AES (Advanced Encryption Standard) encryption/decryption routines (CFB, CTR, CBC, CCM, GCM, OFB, SIV, XTS), SHA (Secure Hash Algorithms) routines (SHA2, SHA3, SHAKE), Message Authentication Code (CMAC, HMAC), ECDH (Elliptic-curve Diffie–Hellman) and RSA (Rivest, Shamir, and Adleman) key generation functions, etc.
AOCL Crypto supports a dynamic dispatcher feature that executes the most optimal function variant implemented using Function Multi-versioning thereby offering a single optimized library portable across different x86 CPU architectures. AOCL Crypto framework is developed in C / C++ for Unix and Windows based systems.

INSTALLATION
- For building and installation, please refer to the file [Build.md](./BUILD.md)

CONTACT
- AOCL Crypto is developed and maintained by AMD. For support of these libraries and the other tools of AMD Zen Software Studio, see https://www.amd.com/en/developer/aocc/compiler-technical-support.html