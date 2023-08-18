cmake_minimum_required(VERSION 3.13)

project(
	SparkleC
	VERSION 0.7
	DESCRIPTION "Yet another CLI download utility"
	HOMEPAGE_URL "https://github.com/Kartatz/SparkleC"
	LANGUAGES C
)

option(SPARKLEC_ENABLE_LTO "Turn on compiler Link Time Optimizations" OFF)

option(SPARKLEC_DISABLE_HOTMART "Disable support for Hotmart" OFF)
option(SPARKLEC_DISABLE_ESTRATEGIA "Disable support for Estratégia websites" OFF)

if (NOT SPARKLEC_DISABLE_ESTRATEGIA)
	option(SPARKLEC_DISABLE_ESTRATEGIA_CONCURSOS "Disable support for Estratégia Concursos" OFF)
	option(SPARKLEC_DISABLE_ESTRATEGIA_VESTIBULARES "Disable support for Estratégia Vestibulares" OFF)
	option(SPARKLEC_DISABLE_ESTRATEGIA_MILITARES "Disable support for Estratégia Militares" OFF)
endif()

option(SPARKLEC_DISABLE_IAEXPERT "Disable support for IA Expert Academy" OFF)
option(SPARKLEC_DISABLE_KIWIFY "Disable support for Kiwify" OFF)

option(SPARKLEC_DISABLE_CERTIFICATE_VALIDATION "Disable SSL certificate validation in libcurl_shared" OFF)

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)

# curl
set(PICKY_COMPILER OFF)
set(BUILD_CURL_EXE OFF)
set(HTTP_ONLY ON)
set(ENABLE_THREADED_RESOLVER OFF)
set(CURL_USE_BEARSSL ON)
set(CURL_USE_OPENSSL OFF)
set(CURL_USE_LIBPSL OFF)
set(CURL_DISABLE_ALTSVC ON)
set(CURL_DISABLE_CRYPTO_AUTH ON)
set(CURL_DISABLE_COOKIES OFF)
set(CURL_DISABLE_HSTS ON)
set(CURL_DISABLE_HTTP_AUTH ON)
set(CURL_DISABLE_NETRC ON)
set(USE_LIBIDN2 OFF)
set(ENABLE_UNIX_SOCKETS OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_CA_BUNDLE "none")
set(CURL_CA_PATH "none")
set(CURL_WERROR OFF)
set(CURL_DISABLE_DOH ON)

if (WIN32)
	set(ENABLE_UNICODE ON)
endif()

# jansson
set(JANSSON_BUILD_DOCS OFF CACHE BOOL "")
set(JANSSON_BUILD_SHARED_LIBS ON CACHE BOOL "")
set(SYMVER_WORKS OFF CACHE BOOL "")
set(VSCRIPT_WORKS OFF CACHE BOOL "")

if (NOT APPLE)
	if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
		add_link_options(-s)
	endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	add_compile_options(-Oz)
endif()

file(READ "${CMAKE_SOURCE_DIR}/submodules/tidy/include/tidyplatform.h" FILE_CONTENTS)
string(REPLACE "TIDY_THREAD_LOCAL __thread" "TIDY_THREAD_LOCAL" FILE_CONTENTS "${FILE_CONTENTS}")
file(WRITE "${CMAKE_SOURCE_DIR}/submodules/tidy/include/tidyplatform.h" "${FILE_CONTENTS}")

file(READ "${CMAKE_SOURCE_DIR}/submodules/tidy/CMakeLists.txt" FILE_CONTENTS) 
string(REPLACE "2.8.12" "2.8.12...3.13" FILE_CONTENTS "${FILE_CONTENTS}") 
file(WRITE "${CMAKE_SOURCE_DIR}/submodules/tidy/CMakeLists.txt" "${FILE_CONTENTS}") 
 
file(READ "${CMAKE_SOURCE_DIR}/submodules/curl/CMake/FindBearSSL.cmake" FILE_CONTENTS) 
string(REPLACE "args(BEARSSL" "args(BearSSL" FILE_CONTENTS "${FILE_CONTENTS}") 
file(WRITE "${CMAKE_SOURCE_DIR}/submodules/curl/CMake/FindBearSSL.cmake" "${FILE_CONTENTS}") 

file(READ "${CMAKE_SOURCE_DIR}/submodules/jansson/CMakeLists.txt" FILE_CONTENTS) 
string(REPLACE " 3.1" " 3.1...3.13" FILE_CONTENTS "${FILE_CONTENTS}") 
file(WRITE "${CMAKE_SOURCE_DIR}/submodules/jansson/CMakeLists.txt" "${FILE_CONTENTS}") 

if (SERENITYOS)
	file(READ "${CMAKE_SOURCE_DIR}/submodules/ffmpeg/libavutil/thread.h" FILE_CONTENTS) 
	string(REPLACE "#if HAVE_PRCTL" "#if !defined(__serenity__) && HAVE_PRCTL" FILE_CONTENTS "${FILE_CONTENTS}") 
	file(WRITE "${CMAKE_SOURCE_DIR}/submodules/ffmpeg/libavutil/thread.h" "${FILE_CONTENTS}") 
	
	file(READ "${CMAKE_SOURCE_DIR}/submodules/tidy/include/tidyplatform.h" FILE_CONTENTS) 
	string(REPLACE "if HAS_FUTIME" "if !defined(__serenity__) && HAS_FUTIME" FILE_CONTENTS "${FILE_CONTENTS}")
	string(REPLACE "ulong" "u_long" FILE_CONTENTS "${FILE_CONTENTS}")
	file(WRITE "${CMAKE_SOURCE_DIR}/submodules/tidy/include/tidyplatform.h" "${FILE_CONTENTS}") 
	
	file(READ "${CMAKE_SOURCE_DIR}/submodules/tidy/include/tidy.h" FILE_CONTENTS) 
	string(REPLACE "ulong" "u_long" FILE_CONTENTS "${FILE_CONTENTS}")
	file(WRITE "${CMAKE_SOURCE_DIR}/submodules/tidy/include/tidy.h" "${FILE_CONTENTS}")
endif()

# Workaround for build issues of Tidy on MacOS
if (APPLE)
	add_compile_options(-Wno-undef-prefix)
endif()

include_directories(
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/submodules/bearssl/src"
	"${CMAKE_SOURCE_DIR}/submodules/bearssl/inc"
	"${CMAKE_CURRENT_BINARY_DIR}/submodules/jansson/include"
)

add_library(
	bearssl
	SHARED
	submodules/bearssl/src/settings.c
	submodules/bearssl/src/aead/ccm.c
	submodules/bearssl/src/aead/eax.c
	submodules/bearssl/src/aead/gcm.c
	submodules/bearssl/src/codec/ccopy.c
	submodules/bearssl/src/codec/dec16be.c
	submodules/bearssl/src/codec/dec16le.c
	submodules/bearssl/src/codec/dec32be.c
	submodules/bearssl/src/codec/dec32le.c
	submodules/bearssl/src/codec/dec64be.c
	submodules/bearssl/src/codec/dec64le.c
	submodules/bearssl/src/codec/enc16be.c
	submodules/bearssl/src/codec/enc16le.c
	submodules/bearssl/src/codec/enc32be.c
	submodules/bearssl/src/codec/enc32le.c
	submodules/bearssl/src/codec/enc64be.c
	submodules/bearssl/src/codec/enc64le.c
	submodules/bearssl/src/codec/pemdec.c
	submodules/bearssl/src/codec/pemenc.c
	submodules/bearssl/src/ec/ec_all_m15.c
	submodules/bearssl/src/ec/ec_all_m31.c
	submodules/bearssl/src/ec/ec_c25519_i15.c
	submodules/bearssl/src/ec/ec_c25519_i31.c
	submodules/bearssl/src/ec/ec_c25519_m15.c
	submodules/bearssl/src/ec/ec_c25519_m31.c
	submodules/bearssl/src/ec/ec_c25519_m62.c
	submodules/bearssl/src/ec/ec_c25519_m64.c
	submodules/bearssl/src/ec/ec_curve25519.c
	submodules/bearssl/src/ec/ec_default.c
	submodules/bearssl/src/ec/ec_keygen.c
	submodules/bearssl/src/ec/ec_p256_m15.c
	submodules/bearssl/src/ec/ec_p256_m31.c
	submodules/bearssl/src/ec/ec_p256_m62.c
	submodules/bearssl/src/ec/ec_p256_m64.c
	submodules/bearssl/src/ec/ec_prime_i15.c
	submodules/bearssl/src/ec/ec_prime_i31.c
	submodules/bearssl/src/ec/ec_pubkey.c
	submodules/bearssl/src/ec/ec_secp256r1.c
	submodules/bearssl/src/ec/ec_secp384r1.c
	submodules/bearssl/src/ec/ec_secp521r1.c
	submodules/bearssl/src/ec/ecdsa_atr.c
	submodules/bearssl/src/ec/ecdsa_default_sign_asn1.c
	submodules/bearssl/src/ec/ecdsa_default_sign_raw.c
	submodules/bearssl/src/ec/ecdsa_default_vrfy_asn1.c
	submodules/bearssl/src/ec/ecdsa_default_vrfy_raw.c
	submodules/bearssl/src/ec/ecdsa_i15_bits.c
	submodules/bearssl/src/ec/ecdsa_i15_sign_asn1.c
	submodules/bearssl/src/ec/ecdsa_i15_sign_raw.c
	submodules/bearssl/src/ec/ecdsa_i15_vrfy_asn1.c
	submodules/bearssl/src/ec/ecdsa_i15_vrfy_raw.c
	submodules/bearssl/src/ec/ecdsa_i31_bits.c
	submodules/bearssl/src/ec/ecdsa_i31_sign_asn1.c
	submodules/bearssl/src/ec/ecdsa_i31_sign_raw.c
	submodules/bearssl/src/ec/ecdsa_i31_vrfy_asn1.c
	submodules/bearssl/src/ec/ecdsa_i31_vrfy_raw.c
	submodules/bearssl/src/ec/ecdsa_rta.c
	submodules/bearssl/src/hash/dig_oid.c
	submodules/bearssl/src/hash/dig_size.c
	submodules/bearssl/src/hash/ghash_ctmul.c
	submodules/bearssl/src/hash/ghash_ctmul32.c
	submodules/bearssl/src/hash/ghash_ctmul64.c
	submodules/bearssl/src/hash/ghash_pclmul.c
	submodules/bearssl/src/hash/ghash_pwr8.c
	submodules/bearssl/src/hash/md5.c
	submodules/bearssl/src/hash/md5sha1.c
	submodules/bearssl/src/hash/mgf1.c
	submodules/bearssl/src/hash/multihash.c
	submodules/bearssl/src/hash/sha1.c
	submodules/bearssl/src/hash/sha2big.c
	submodules/bearssl/src/hash/sha2small.c
	submodules/bearssl/src/int/i15_add.c
	submodules/bearssl/src/int/i15_bitlen.c
	submodules/bearssl/src/int/i15_decmod.c
	submodules/bearssl/src/int/i15_decode.c
	submodules/bearssl/src/int/i15_decred.c
	submodules/bearssl/src/int/i15_encode.c
	submodules/bearssl/src/int/i15_fmont.c
	submodules/bearssl/src/int/i15_iszero.c
	submodules/bearssl/src/int/i15_moddiv.c
	submodules/bearssl/src/int/i15_modpow.c
	submodules/bearssl/src/int/i15_modpow2.c
	submodules/bearssl/src/int/i15_montmul.c
	submodules/bearssl/src/int/i15_mulacc.c
	submodules/bearssl/src/int/i15_muladd.c
	submodules/bearssl/src/int/i15_ninv15.c
	submodules/bearssl/src/int/i15_reduce.c
	submodules/bearssl/src/int/i15_rshift.c
	submodules/bearssl/src/int/i15_sub.c
	submodules/bearssl/src/int/i15_tmont.c
	submodules/bearssl/src/int/i31_add.c
	submodules/bearssl/src/int/i31_bitlen.c
	submodules/bearssl/src/int/i31_decmod.c
	submodules/bearssl/src/int/i31_decode.c
	submodules/bearssl/src/int/i31_decred.c
	submodules/bearssl/src/int/i31_encode.c
	submodules/bearssl/src/int/i31_fmont.c
	submodules/bearssl/src/int/i31_iszero.c
	submodules/bearssl/src/int/i31_moddiv.c
	submodules/bearssl/src/int/i31_modpow.c
	submodules/bearssl/src/int/i31_modpow2.c
	submodules/bearssl/src/int/i31_montmul.c
	submodules/bearssl/src/int/i31_mulacc.c
	submodules/bearssl/src/int/i31_muladd.c
	submodules/bearssl/src/int/i31_ninv31.c
	submodules/bearssl/src/int/i31_reduce.c
	submodules/bearssl/src/int/i31_rshift.c
	submodules/bearssl/src/int/i31_sub.c
	submodules/bearssl/src/int/i31_tmont.c
	submodules/bearssl/src/int/i32_add.c
	submodules/bearssl/src/int/i32_bitlen.c
	submodules/bearssl/src/int/i32_decmod.c
	submodules/bearssl/src/int/i32_decode.c
	submodules/bearssl/src/int/i32_decred.c
	submodules/bearssl/src/int/i32_div32.c
	submodules/bearssl/src/int/i32_encode.c
	submodules/bearssl/src/int/i32_fmont.c
	submodules/bearssl/src/int/i32_iszero.c
	submodules/bearssl/src/int/i32_modpow.c
	submodules/bearssl/src/int/i32_montmul.c
	submodules/bearssl/src/int/i32_mulacc.c
	submodules/bearssl/src/int/i32_muladd.c
	submodules/bearssl/src/int/i32_ninv32.c
	submodules/bearssl/src/int/i32_reduce.c
	submodules/bearssl/src/int/i32_sub.c
	submodules/bearssl/src/int/i32_tmont.c
	submodules/bearssl/src/int/i62_modpow2.c
	submodules/bearssl/src/kdf/hkdf.c
	submodules/bearssl/src/kdf/shake.c
	submodules/bearssl/src/mac/hmac.c
	submodules/bearssl/src/mac/hmac_ct.c
	submodules/bearssl/src/rand/aesctr_drbg.c
	submodules/bearssl/src/rand/hmac_drbg.c
	submodules/bearssl/src/rand/sysrng.c
	submodules/bearssl/src/rsa/rsa_default_keygen.c
	submodules/bearssl/src/rsa/rsa_default_modulus.c
	submodules/bearssl/src/rsa/rsa_default_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_default_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_default_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_default_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_default_priv.c
	submodules/bearssl/src/rsa/rsa_default_privexp.c
	submodules/bearssl/src/rsa/rsa_default_pss_sign.c
	submodules/bearssl/src/rsa/rsa_default_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_default_pub.c
	submodules/bearssl/src/rsa/rsa_default_pubexp.c
	submodules/bearssl/src/rsa/rsa_i15_keygen.c
	submodules/bearssl/src/rsa/rsa_i15_modulus.c
	submodules/bearssl/src/rsa/rsa_i15_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i15_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i15_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i15_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i15_priv.c
	submodules/bearssl/src/rsa/rsa_i15_privexp.c
	submodules/bearssl/src/rsa/rsa_i15_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i15_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i15_pub.c
	submodules/bearssl/src/rsa/rsa_i15_pubexp.c
	submodules/bearssl/src/rsa/rsa_i31_keygen.c
	submodules/bearssl/src/rsa/rsa_i31_keygen_inner.c
	submodules/bearssl/src/rsa/rsa_i31_modulus.c
	submodules/bearssl/src/rsa/rsa_i31_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i31_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i31_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i31_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i31_priv.c
	submodules/bearssl/src/rsa/rsa_i31_privexp.c
	submodules/bearssl/src/rsa/rsa_i31_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i31_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i31_pub.c
	submodules/bearssl/src/rsa/rsa_i31_pubexp.c
	submodules/bearssl/src/rsa/rsa_i32_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i32_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i32_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i32_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i32_priv.c
	submodules/bearssl/src/rsa/rsa_i32_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i32_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i32_pub.c
	submodules/bearssl/src/rsa/rsa_i62_keygen.c
	submodules/bearssl/src/rsa/rsa_i62_oaep_decrypt.c
	submodules/bearssl/src/rsa/rsa_i62_oaep_encrypt.c
	submodules/bearssl/src/rsa/rsa_i62_pkcs1_sign.c
	submodules/bearssl/src/rsa/rsa_i62_pkcs1_vrfy.c
	submodules/bearssl/src/rsa/rsa_i62_priv.c
	submodules/bearssl/src/rsa/rsa_i62_pss_sign.c
	submodules/bearssl/src/rsa/rsa_i62_pss_vrfy.c
	submodules/bearssl/src/rsa/rsa_i62_pub.c
	submodules/bearssl/src/rsa/rsa_oaep_pad.c
	submodules/bearssl/src/rsa/rsa_oaep_unpad.c
	submodules/bearssl/src/rsa/rsa_pkcs1_sig_pad.c
	submodules/bearssl/src/rsa/rsa_pkcs1_sig_unpad.c
	submodules/bearssl/src/rsa/rsa_pss_sig_pad.c
	submodules/bearssl/src/rsa/rsa_pss_sig_unpad.c
	submodules/bearssl/src/rsa/rsa_ssl_decrypt.c
	submodules/bearssl/src/ssl/prf.c
	submodules/bearssl/src/ssl/prf_md5sha1.c
	submodules/bearssl/src/ssl/prf_sha256.c
	submodules/bearssl/src/ssl/prf_sha384.c
	submodules/bearssl/src/ssl/ssl_ccert_single_ec.c
	submodules/bearssl/src/ssl/ssl_ccert_single_rsa.c
	submodules/bearssl/src/ssl/ssl_client.c
	submodules/bearssl/src/ssl/ssl_client_default_rsapub.c
	submodules/bearssl/src/ssl/ssl_client_full.c
	submodules/bearssl/src/ssl/ssl_engine.c
	submodules/bearssl/src/ssl/ssl_engine_default_aescbc.c
	submodules/bearssl/src/ssl/ssl_engine_default_aesccm.c
	submodules/bearssl/src/ssl/ssl_engine_default_aesgcm.c
	submodules/bearssl/src/ssl/ssl_engine_default_chapol.c
	submodules/bearssl/src/ssl/ssl_engine_default_descbc.c
	submodules/bearssl/src/ssl/ssl_engine_default_ec.c
	submodules/bearssl/src/ssl/ssl_engine_default_ecdsa.c
	submodules/bearssl/src/ssl/ssl_engine_default_rsavrfy.c
	submodules/bearssl/src/ssl/ssl_hashes.c
	submodules/bearssl/src/ssl/ssl_hs_client.c
	submodules/bearssl/src/ssl/ssl_hs_server.c
	submodules/bearssl/src/ssl/ssl_io.c
	submodules/bearssl/src/ssl/ssl_keyexport.c
	submodules/bearssl/src/ssl/ssl_lru.c
	submodules/bearssl/src/ssl/ssl_rec_cbc.c
	submodules/bearssl/src/ssl/ssl_rec_ccm.c
	submodules/bearssl/src/ssl/ssl_rec_chapol.c
	submodules/bearssl/src/ssl/ssl_rec_gcm.c
	submodules/bearssl/src/ssl/ssl_scert_single_ec.c
	submodules/bearssl/src/ssl/ssl_scert_single_rsa.c
	submodules/bearssl/src/ssl/ssl_server.c
	submodules/bearssl/src/ssl/ssl_server_full_ec.c
	submodules/bearssl/src/ssl/ssl_server_full_rsa.c
	submodules/bearssl/src/ssl/ssl_server_mine2c.c
	submodules/bearssl/src/ssl/ssl_server_mine2g.c
	submodules/bearssl/src/ssl/ssl_server_minf2c.c
	submodules/bearssl/src/ssl/ssl_server_minf2g.c
	submodules/bearssl/src/ssl/ssl_server_minr2g.c
	submodules/bearssl/src/ssl/ssl_server_minu2g.c
	submodules/bearssl/src/ssl/ssl_server_minv2g.c
	submodules/bearssl/src/symcipher/aes_big_cbcdec.c
	submodules/bearssl/src/symcipher/aes_big_cbcenc.c
	submodules/bearssl/src/symcipher/aes_big_ctr.c
	submodules/bearssl/src/symcipher/aes_big_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_big_dec.c
	submodules/bearssl/src/symcipher/aes_big_enc.c
	submodules/bearssl/src/symcipher/aes_common.c
	submodules/bearssl/src/symcipher/aes_ct.c
	submodules/bearssl/src/symcipher/aes_ct64.c
	submodules/bearssl/src/symcipher/aes_ct64_cbcdec.c
	submodules/bearssl/src/symcipher/aes_ct64_cbcenc.c
	submodules/bearssl/src/symcipher/aes_ct64_ctr.c
	submodules/bearssl/src/symcipher/aes_ct64_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_ct64_dec.c
	submodules/bearssl/src/symcipher/aes_ct64_enc.c
	submodules/bearssl/src/symcipher/aes_ct_cbcdec.c
	submodules/bearssl/src/symcipher/aes_ct_cbcenc.c
	submodules/bearssl/src/symcipher/aes_ct_ctr.c
	submodules/bearssl/src/symcipher/aes_ct_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_ct_dec.c
	submodules/bearssl/src/symcipher/aes_ct_enc.c
	submodules/bearssl/src/symcipher/aes_pwr8.c
	submodules/bearssl/src/symcipher/aes_pwr8_cbcdec.c
	submodules/bearssl/src/symcipher/aes_pwr8_cbcenc.c
	submodules/bearssl/src/symcipher/aes_pwr8_ctr.c
	submodules/bearssl/src/symcipher/aes_pwr8_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_small_cbcdec.c
	submodules/bearssl/src/symcipher/aes_small_cbcenc.c
	submodules/bearssl/src/symcipher/aes_small_ctr.c
	submodules/bearssl/src/symcipher/aes_small_ctrcbc.c
	submodules/bearssl/src/symcipher/aes_small_dec.c
	submodules/bearssl/src/symcipher/aes_small_enc.c
	submodules/bearssl/src/symcipher/aes_x86ni.c
	submodules/bearssl/src/symcipher/aes_x86ni_cbcdec.c
	submodules/bearssl/src/symcipher/aes_x86ni_cbcenc.c
	submodules/bearssl/src/symcipher/aes_x86ni_ctr.c
	submodules/bearssl/src/symcipher/aes_x86ni_ctrcbc.c
	submodules/bearssl/src/symcipher/chacha20_ct.c
	submodules/bearssl/src/symcipher/chacha20_sse2.c
	submodules/bearssl/src/symcipher/des_ct.c
	submodules/bearssl/src/symcipher/des_ct_cbcdec.c
	submodules/bearssl/src/symcipher/des_ct_cbcenc.c
	submodules/bearssl/src/symcipher/des_support.c
	submodules/bearssl/src/symcipher/des_tab.c
	submodules/bearssl/src/symcipher/des_tab_cbcdec.c
	submodules/bearssl/src/symcipher/des_tab_cbcenc.c
	submodules/bearssl/src/symcipher/poly1305_ctmul.c
	submodules/bearssl/src/symcipher/poly1305_ctmul32.c
	submodules/bearssl/src/symcipher/poly1305_ctmulq.c
	submodules/bearssl/src/symcipher/poly1305_i15.c
	submodules/bearssl/src/x509/asn1enc.c
	submodules/bearssl/src/x509/encode_ec_pk8der.c
	submodules/bearssl/src/x509/encode_ec_rawder.c
	submodules/bearssl/src/x509/encode_rsa_pk8der.c
	submodules/bearssl/src/x509/encode_rsa_rawder.c
	submodules/bearssl/src/x509/skey_decoder.c
	submodules/bearssl/src/x509/x509_decoder.c
	submodules/bearssl/src/x509/x509_knownkey.c
	submodules/bearssl/src/x509/x509_minimal.c
	submodules/bearssl/src/x509/x509_minimal_full.c
)

if (HAIKU)
	target_compile_definitions(
		bearssl
		PRIVATE
		BR_USE_URANDOM BR_USE_UNIX_TIME
	)
endif()

set(BEARSSL_INCLUDE_DIRS submodules/bearssl/inc)
set(BEARSSL_LIBRARY $<TARGET_FILE:bearssl>)

add_subdirectory(submodules/curl EXCLUDE_FROM_ALL)
add_subdirectory(submodules/jansson EXCLUDE_FROM_ALL)
add_subdirectory(submodules/tidy EXCLUDE_FROM_ALL)

set_target_properties(
	libcurl_shared
	PROPERTIES
	IMPORTED_LOCATION $<TARGET_FILE:bearssl>
)

if (SERENITYOS)
	target_compile_definitions(
		tidy-share
		PRIVATE
		ulong=u_long
	)
endif()

set(FFMPEG_SOURCE_DIRECTORY "${CMAKE_SOURCE_DIR}/submodules/ffmpeg")
set(FFMPEG_BUILD_DIRECTORY "${CMAKE_BINARY_DIR}/ffmpeg-build")
set(FFMPEG_INSTALL_PREFIX "${FFMPEG_BUILD_DIRECTORY}/binaries")

set(
	FFMPEG_CONFIGURE_FLAGS
	--disable-all
	--disable-alsa
	--disable-appkit
	--disable-asm
	--disable-audiotoolbox
	--disable-avfoundation
	--disable-bzlib
	--disable-coreimage
	--disable-debug
	--disable-doc
	--disable-iconv
	--disable-libxcb
	--disable-lzma
	--disable-neon
	--disable-network
	--disable-pthreads
	--disable-schannel
	--disable-sdl2
	--disable-securetransport
	--disable-static
	--disable-symver
	--disable-videotoolbox
	--disable-vulkan
	--disable-w32threads
	--disable-xlib
	--disable-zlib
	--enable-avcodec
	--enable-avformat
	--enable-avutil
	--enable-decoder=aac,h264
	--enable-demuxer=aac,h264,hls
	--enable-ffmpeg
	--enable-muxer=h264,mpegts
	--enable-pic
	--enable-protocol=file,crypto
	--enable-shared
	--enable-small
	--enable-version3
	--extra-ldflags=-fPIC
	--prefix="${FFMPEG_INSTALL_PREFIX}"
)

if (SPARKLEC_ENABLE_LTO)
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--enable-lto")
endif()

if (CMAKE_CROSSCOMPILING)
	string(TOLOWER "${CMAKE_SYSTEM_NAME}" system_name)
	
	if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
		set(system_name "mingw32")
	elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "SerenityOS")
		set(system_name "none")
	endif()
	
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--cross-prefix=${TOOLCHAIN_PREFIX}-")
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--target-os=${system_name}")
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--arch=${CMAKE_SYSTEM_PROCESSOR}")
	
	if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
		list(APPEND FFMPEG_CONFIGURE_FLAGS "--as=${ANDROID_TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-clang")
		list(APPEND FFMPEG_CONFIGURE_FLAGS "--ld=${ANDROID_TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-clang")
		list(APPEND FFMPEG_CONFIGURE_FLAGS "--cc=${ANDROID_TOOLCHAIN_ROOT}/bin/${TOOLCHAIN_PREFIX}-clang")
	else()
		list(APPEND FFMPEG_CONFIGURE_FLAGS "--as=${CMAKE_C_COMPILER}")
		list(APPEND FFMPEG_CONFIGURE_FLAGS "--ld=${CMAKE_C_COMPILER}")
		list(APPEND FFMPEG_CONFIGURE_FLAGS "--cc=${CMAKE_C_COMPILER}")
	endif()
	
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--nm=${CMAKE_NM}")
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--strip=${CMAKE_STRIP}")
else()
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--cc=${CMAKE_C_COMPILER}")
	list(APPEND FFMPEG_CONFIGURE_FLAGS "--ld=${CMAKE_C_COMPILER}")
endif()

set(FFMPEG_INCLUDE_DIRS "${FFMPEG_INSTALL_PREFIX}/include")

if (NOT EXISTS "${FFMPEG_BUILD_DIRECTORY}")
	file(MAKE_DIRECTORY "${FFMPEG_BUILD_DIRECTORY}")
endif()

if (NOT EXISTS "${FFMPEG_BUILD_DIRECTORY}/Makefile")
	message("-- Configuring FFmpeg")
	
	execute_process(
		COMMAND bash
		"${FFMPEG_SOURCE_DIRECTORY}/configure"
		${FFMPEG_CONFIGURE_FLAGS}
		WORKING_DIRECTORY "${FFMPEG_BUILD_DIRECTORY}"
		COMMAND_ERROR_IS_FATAL ANY
	)
endif()

if (NOT EXISTS "${FFMPEG_INSTALL_PREFIX}")
	add_custom_command(
		OUTPUT "${FFMPEG_INSTALL_PREFIX}"
		COMMAND make
		COMMAND make install
		WORKING_DIRECTORY "${FFMPEG_BUILD_DIRECTORY}"
		COMMENT "-- Building FFmpeg"
	)
endif()

add_custom_target(ffmpeg ALL DEPENDS "${FFMPEG_INSTALL_PREFIX}")

# Extract libavformat version
file(READ "${FFMPEG_SOURCE_DIRECTORY}/libavformat/version_major.h" FILE_CONTENTS)
string(REGEX MATCH "#define LIBAVFORMAT_VERSION_MAJOR *([0-9]+)" _ ${FILE_CONTENTS})

set(LIBAVFORMAT_VERSION_MAJOR "${CMAKE_MATCH_1}")

file(READ "${FFMPEG_SOURCE_DIRECTORY}/libavformat/version.h" FILE_CONTENTS)

string(REGEX MATCH "#define LIBAVFORMAT_VERSION_MINOR *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVFORMAT_VERSION_MINOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "#define LIBAVFORMAT_VERSION_MICRO *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVFORMAT_VERSION_MICRO "${CMAKE_MATCH_1}")

# Extract libavcodec version
file(READ "${FFMPEG_SOURCE_DIRECTORY}/libavcodec/version_major.h" FILE_CONTENTS)
string(REGEX MATCH "#define LIBAVCODEC_VERSION_MAJOR *([0-9]+)" _ ${FILE_CONTENTS})

set(LIBAVCODEC_VERSION_MAJOR "${CMAKE_MATCH_1}")

file(READ "${FFMPEG_SOURCE_DIRECTORY}/libavcodec/version.h" FILE_CONTENTS)

string(REGEX MATCH "#define LIBAVCODEC_VERSION_MINOR *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVCODEC_VERSION_MINOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "#define LIBAVCODEC_VERSION_MICRO *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVCODEC_VERSION_MICRO "${CMAKE_MATCH_1}")

# Extract libavutil version
file(READ "${FFMPEG_SOURCE_DIRECTORY}/libavutil/version.h" FILE_CONTENTS)

string(REGEX MATCH "#define LIBAVUTIL_VERSION_MAJOR *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVUTIL_VERSION_MAJOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "#define LIBAVUTIL_VERSION_MINOR *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVUTIL_VERSION_MINOR "${CMAKE_MATCH_1}")

string(REGEX MATCH "#define LIBAVUTIL_VERSION_MICRO *([0-9]+)" _ ${FILE_CONTENTS})
set(LIBAVUTIL_VERSION_MICRO "${CMAKE_MATCH_1}")

if (UNIX)
	if ("${CMAKE_SYSTEM_NAME}" STREQUAL "OpenBSD")
		set(LIBAVFORMAT_SOVERSION "${LIBAVFORMAT_VERSION_MAJOR}.${LIBAVFORMAT_VERSION_MINOR}")
		set(LIBAVCODEC_SOVERSION "${LIBAVCODEC_VERSION_MAJOR}.${LIBAVCODEC_VERSION_MINOR}")
		set(LIBAVUTIL_SOVERSION "${LIBAVUTIL_VERSION_MAJOR}.${LIBAVUTIL_VERSION_MINOR}")
	else()
		set(LIBAVFORMAT_SOVERSION "${LIBAVFORMAT_VERSION_MAJOR}.${LIBAVFORMAT_VERSION_MINOR}.${LIBAVFORMAT_VERSION_MICRO}")
		set(LIBAVCODEC_SOVERSION "${LIBAVCODEC_VERSION_MAJOR}.${LIBAVCODEC_VERSION_MINOR}.${LIBAVCODEC_VERSION_MICRO}")
		set(LIBAVUTIL_SOVERSION "${LIBAVUTIL_VERSION_MAJOR}.${LIBAVUTIL_VERSION_MINOR}.${LIBAVUTIL_VERSION_MICRO}")
	endif()
	
	set(SOVERSION_SEPARATOR ".")
	
	if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
		set(AVFORMAT_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avformat${CMAKE_SHARED_LIBRARY_SUFFIX}")
		set(AVCODEC_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avcodec${CMAKE_SHARED_LIBRARY_SUFFIX}")
		set(AVUTIL_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avutil${CMAKE_SHARED_LIBRARY_SUFFIX}")
	elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
		set(AVFORMAT_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avformat${SOVERSION_SEPARATOR}${LIBAVFORMAT_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
		set(AVCODEC_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avcodec${SOVERSION_SEPARATOR}${LIBAVCODEC_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
		set(AVUTIL_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avutil${SOVERSION_SEPARATOR}${LIBAVUTIL_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	else()
		set(AVFORMAT_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avformat${CMAKE_SHARED_LIBRARY_SUFFIX}${SOVERSION_SEPARATOR}${LIBAVFORMAT_SOVERSION}")
		set(AVCODEC_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avcodec${CMAKE_SHARED_LIBRARY_SUFFIX}${SOVERSION_SEPARATOR}${LIBAVCODEC_SOVERSION}")
		set(AVUTIL_LIBRARY_NAME "${CMAKE_SHARED_LIBRARY_PREFIX}avutil${CMAKE_SHARED_LIBRARY_SUFFIX}${SOVERSION_SEPARATOR}${LIBAVUTIL_SOVERSION}")
	endif()
elseif (WIN32)
	set(LIBAVFORMAT_SOVERSION "${LIBAVFORMAT_VERSION_MAJOR}")
	set(LIBAVCODEC_SOVERSION "${LIBAVCODEC_VERSION_MAJOR}")
	set(LIBAVUTIL_SOVERSION "${LIBAVUTIL_VERSION_MAJOR}")
	
	set(SOVERSION_SEPARATOR "-")
	
	set(AVFORMAT_LIBRARY_NAME "avformat${SOVERSION_SEPARATOR}${LIBAVFORMAT_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(AVCODEC_LIBRARY_NAME "avcodec${SOVERSION_SEPARATOR}${LIBAVCODEC_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(AVUTIL_LIBRARY_NAME "avutil${SOVERSION_SEPARATOR}${LIBAVUTIL_SOVERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

if (UNIX)
	set(SHARED_LIBRARY_DIRECTORY "lib")
elseif(WIN32)
	set(SHARED_LIBRARY_DIRECTORY "bin")
endif()

set(
	libraries
	"${FFMPEG_INSTALL_PREFIX}/${SHARED_LIBRARY_DIRECTORY}/${AVFORMAT_LIBRARY_NAME}"
	"${FFMPEG_INSTALL_PREFIX}/${SHARED_LIBRARY_DIRECTORY}/${AVCODEC_LIBRARY_NAME}"
	"${FFMPEG_INSTALL_PREFIX}/${SHARED_LIBRARY_DIRECTORY}/${AVUTIL_LIBRARY_NAME}"
)

set(FFMPEG_LIBRARIES)

foreach(source ${libraries})
	get_filename_component(dirname "${source}" DIRECTORY)
	get_filename_component(basename "${source}" NAME)
	
	string(FIND "${basename}" "${SOVERSION_SEPARATOR}" position)
	string(SUBSTRING "${basename}" 0 "${position}" destination)
	
	set(target "${destination}")
	set(target_name "${destination}")
	
	if (${target} STREQUAL "libavformat" OR ${target} STREQUAL "avformat")
		set(major_version "${LIBAVFORMAT_VERSION_MAJOR}")
	elseif (${target} STREQUAL "libavcodec" OR ${target} STREQUAL "avcodec")
		set(major_version "${LIBAVCODEC_VERSION_MAJOR}")
	elseif (${target} STREQUAL "libavutil" OR ${target} STREQUAL "avutil")
		set(major_version "${LIBAVUTIL_VERSION_MAJOR}")
	endif()
	
	if (UNIX)
		if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
			string(APPEND destination "${CMAKE_SHARED_LIBRARY_SUFFIX}")
		elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
			string(APPEND destination "${SOVERSION_SEPARATOR}${major_version}${CMAKE_SHARED_LIBRARY_SUFFIX}")
		else()
			string(APPEND destination "${CMAKE_SHARED_LIBRARY_SUFFIX}${SOVERSION_SEPARATOR}${major_version}")
		endif()
	elseif (WIN32)
		string(APPEND destination "${SOVERSION_SEPARATOR}${major_version}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	endif()
	
	string(PREPEND destination "${CMAKE_BINARY_DIR}/")
	
	if (NOT EXISTS "${destination}")
		add_custom_command(
			TARGET ffmpeg POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy "${source}" "${destination}"
			WORKING_DIRECTORY "${FFMPEG_BUILD_DIRECTORY}"
			COMMENT "-- Copying shared library from '${source}' to '${destination}'"
		)
	endif()
	
	if (WIN32)
		string(PREPEND target "${CMAKE_SHARED_LIBRARY_PREFIX}")
	endif()
	
	add_library("${target}" SHARED IMPORTED)
	
	set_target_properties(
		"${target}"
		PROPERTIES IMPORTED_LOCATION "${destination}"
	)
	
	if (WIN32)
		set_target_properties(
			"${target}"
			PROPERTIES IMPORTED_IMPLIB "${dirname}/${target_name}.lib"
		)
	endif()
	
	list(APPEND FFMPEG_LIBRARIES "${destination}")
endforeach()

# Disable SO versioning for all third-party packages
foreach(target jansson libcurl_shared tidy-share)
	set_target_properties(
		${target}
		PROPERTIES
		NO_SONAME 1
	)
endforeach()

add_executable(
	sparklec
	src/callbacks.c
	src/cleanup.c
	src/buffer.c
	src/curl.c
	src/errors.c
	src/html.c
	src/m3u8.c
	src/main.c
	src/query.c
	src/types.c
	src/stringu.c
	src/filesystem.c
	src/os.c
	src/vimeo.c
	src/youtube.c
	src/panda.c
	src/fstream.c
	src/credentials.c
	src/input.c
	src/terminal.c
	src/cir.c
	src/ffmpeg.c
	src/walkdir.c
	src/ttidy.c
)

if (NOT (WIN32 OR SERENITYOS))
	target_sources(
		sparklec
		PRIVATE
		src/getdents.c
	)
endif()

add_executable(
	sparklec-install
	src/install.c
	src/filesystem.c
	src/os.c
	src/stringu.c
	src/errors.c
	src/fstream.c
	src/cleanup.c
	src/walkdir.c
)

if (NOT (WIN32 OR SERENITYOS))
	target_sources(
		sparklec-install
		PRIVATE
		src/getdents.c
	)
endif()

if (SPARKLEC_DISABLE_CERTIFICATE_VALIDATION) 
	target_compile_definitions(
		sparklec
		PRIVATE
		SPARKLEC_DISABLE_CERTIFICATE_VALIDATION
	)
endif()

if (SPARKLEC_DISABLE_HOTMART)
	target_compile_definitions(
		sparklec
		PRIVATE
		SPARKLEC_DISABLE_HOTMART
	)
else()
	target_sources(
		sparklec
		PRIVATE
		src/hotmart.c
	)
endif()

if (NOT SPARKLEC_DISABLE_ESTRATEGIA)
	target_sources(
		sparklec
		PRIVATE
		src/estrategia.c
	)
	
	if (SPARKLEC_DISABLE_ESTRATEGIA_CONCURSOS)
		target_compile_definitions(
			sparklec
			PRIVATE
			SPARKLEC_DISABLE_ESTRATEGIA_CONCURSOS
		)
	else()
		target_sources(
			sparklec
			PRIVATE
			src/estrategia_concursos.c
		)
	endif()
	
	if (SPARKLEC_DISABLE_ESTRATEGIA_VESTIBULARES)
		target_compile_definitions(
			sparklec
			PRIVATE
			SPARKLEC_DISABLE_ESTRATEGIA_VESTIBULARES
		)
	else()
		target_sources(
			sparklec
			PRIVATE
			src/estrategia_vestibulares.c
		)
	endif()
	
	if (SPARKLEC_DISABLE_ESTRATEGIA_MILITARES)
		target_compile_definitions(
			sparklec
			PRIVATE
			SPARKLEC_DISABLE_ESTRATEGIA_MILITARES
		)
	else()
		target_sources(
			sparklec
			PRIVATE
			src/estrategia_militares.c
		)
	endif()
else()
	target_compile_definitions(
		sparklec
		PRIVATE
		SPARKLEC_DISABLE_ESTRATEGIA
	)
endif()

if (SPARKLEC_DISABLE_IAEXPERT)
	target_compile_definitions(
		sparklec
		PRIVATE
		SPARKLEC_DISABLE_IAEXPERT
	)
else()
	target_sources(
		sparklec
		PRIVATE
		src/iaexpert.c
	)
endif()

if (SPARKLEC_DISABLE_KIWIFY)
	target_compile_definitions(
		sparklec
		PRIVATE
		SPARKLEC_DISABLE_KIWIFY
	)
else()
	target_sources(
		sparklec
		PRIVATE
		src/kiwify.c
	)
endif()

foreach(target sparklec sparklec-install)
	target_compile_options(
		${target}
		PRIVATE
		-Wall -Wextra
	)
	
	if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
		target_link_options(
			${target}
			BEFORE
			PRIVATE
			-Wl,-rpath-link,.
		)
	endif()
	
	if (APPLE)
		foreach(property BUILD_RPATH INSTALL_RPATH)
			set_target_properties(
				${target}
				PROPERTIES
				${property} "@loader_path/../lib"
			)
		endforeach()
	elseif (UNIX)
		foreach(property BUILD_RPATH INSTALL_RPATH)
			set_target_properties(
				${target}
				PROPERTIES
				${property} "$ORIGIN/../lib:$ORIGIN"
			)
		endforeach()
	endif()
	
	target_include_directories(
		${target}
		PRIVATE
		"${FFMPEG_INCLUDE_DIRS}"
	)
endforeach()

if (APPLE)
	foreach(property BUILD_RPATH INSTALL_RPATH)
		set_target_properties(
			libcurl_shared
			PROPERTIES
			${property} "@loader_path"
		)
	endforeach()
elseif (UNIX)
	foreach(property BUILD_RPATH INSTALL_RPATH)
		set_target_properties(
			libcurl_shared
			PROPERTIES
			${property} "$ORIGIN"
		)
	endforeach()
endif()

foreach(property RUNTIME_OUTPUT_DIRECTORY LIBRARY_OUTPUT_DIRECTORY)
	foreach(target jansson libcurl_shared tidy-share)
		set_target_properties(
			${target}
			PROPERTIES
			${property} $<TARGET_FILE_DIR:sparklec>
		)
	endforeach()
endforeach()

# Enable Unicode on Windows builds
if (WIN32)
	foreach(target sparklec sparklec-install)
		target_sources(
			${target}
			PRIVATE
			src/wio.c
		)
		
		target_compile_definitions(
			${target}
			PRIVATE
			UNICODE _UNICODE
		)
		
		target_link_options(
			${target}
			BEFORE
			PRIVATE
			-municode
		)
		
		target_link_libraries(
			${target}
			shlwapi
		)
	endforeach()
	
	target_sources(
		sparklec-install
		PRIVATE
		src/wregistry.c
	)
endif()

if (SPARKLEC_ENABLE_LTO)
	set(SPARKLEC_HAS_LTO OFF)
	
	include(CheckIPOSupported)
	check_ipo_supported(RESULT SPARKLEC_HAS_LTO LANGUAGES C)
	
	if (SPARKLEC_HAS_LTO)
		foreach(target sparklec sparklec-install bearssl jansson libcurl_shared tidy-share)
			set_target_properties(
				${target}
				PROPERTIES
				INTERPROCEDURAL_OPTIMIZATION TRUE
			)
		endforeach()
	endif()
endif()

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	target_link_options(
		sparklec
		BEFORE
		PRIVATE
		-Wl,-rpath-link,. -L.
	)
elseif (WIN32)
	target_link_options(
		sparklec
		BEFORE
		PRIVATE
		-L.
	)
endif()

target_link_libraries(
	sparklec
	jansson
	libcurl_shared
	tidy-share
	libavformat
	libavcodec
	libavutil
)

target_link_libraries(
	sparklec-install
	bearssl
)

foreach(target sparklec sparklec-install bearssl jansson libcurl_shared tidy-share)
	install(
		TARGETS ${target}
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib NAMELINK_SKIP
	)
endforeach()

install(
	FILES "${CMAKE_SOURCE_DIR}/submodules/cabundle/pem/cert.pem"
	DESTINATION etc/tls
	RENAME cert.pem
)

install(
	FILES ${FFMPEG_LIBRARIES}
	PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
	DESTINATION "${SHARED_LIBRARY_DIRECTORY}"
)

if (APPLE)
	set(
		names
		"../../libjansson.dylib"
		"../../../libcurl_shared.dylib"
		"../../libtidy.dylib"
	)
	
	set(index 0)
	
	foreach(library ${libraries})
		get_filename_component(dirname "${library}" DIRECTORY)
		list(GET FFMPEG_LIBRARIES "${index}" library)
		get_filename_component(basename "${library}" NAME)
		
		list(APPEND names "${dirname}/${basename}")
		
		MATH(EXPR index "${index}+1")
	endforeach()
	
	set(executable $<TARGET_FILE:sparklec>)
	
	foreach(old_rpath ${names})
		get_filename_component(basename "${old_rpath}" NAME)
		set(new_rpath "@loader_path/../lib/${basename}")
		
		add_custom_command(
			TARGET sparklec POST_BUILD
			COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -change "${old_rpath}" "${new_rpath}" "${executable}"
			COMMENT "-- Changing hardcoded rpath of '${executable}' from '${old_rpath}' to '${new_rpath}'"
		)
	endforeach()
	
	foreach(library ${FFMPEG_LIBRARIES})
		set(names)
		set(index -1)
		
		get_filename_component(basename "${library}" NAME)
		
		if (basename MATCHES "libavformat${SOVERSION_SEPARATOR}${LIBAVFORMAT_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
			foreach(library ${libraries})
				MATH(EXPR index "${index}+1")
				
				get_filename_component(dirname "${library}" DIRECTORY)
				list(GET FFMPEG_LIBRARIES "${index}" library)
				get_filename_component(basename "${library}" NAME)
				
				if (NOT (basename STREQUAL "libavcodec${SOVERSION_SEPARATOR}${LIBAVCODEC_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}" OR basename STREQUAL "libavutil${SOVERSION_SEPARATOR}${LIBAVUTIL_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}"))
					continue()
				endif()
				
				list(APPEND names "${dirname}/${basename}")
			endforeach()
		elseif (basename MATCHES "libavcodec${SOVERSION_SEPARATOR}${LIBAVCODEC_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
			foreach(library ${libraries})
				MATH(EXPR index "${index}+1")
				
				get_filename_component(dirname "${library}" DIRECTORY)
				list(GET FFMPEG_LIBRARIES ${index} library)
				get_filename_component(basename "${library}" NAME)
				
				if (NOT basename STREQUAL "libavutil${SOVERSION_SEPARATOR}${LIBAVUTIL_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}")
					continue()
				endif()
				
				list(APPEND names "${dirname}/${basename}")
			endforeach()
		else()
			continue()
		endif()
		
		foreach(old_rpath ${names})
			get_filename_component(basename "${old_rpath}" NAME)
			set(new_rpath "@loader_path/../lib/${basename}")
			
			add_custom_command(
				TARGET ffmpeg POST_BUILD
				COMMAND "${CMAKE_INSTALL_NAME_TOOL}" -change "${old_rpath}" "${new_rpath}" "${library}"
				COMMENT "-- Changing hardcoded rpath of '${library}' from '${old_rpath}' to '${new_rpath}'"
			)
		endforeach()
	endforeach()
endif()