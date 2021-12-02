
set(COMPONENT_SRCS 
    "src/abi_pack.c"
    "src/abi_read_contract.c"
    "src/config.c"
    "src/endian_conv.c"
    "src/iotex_emb.c"
    "src/parse.c"
    "src/pb_pack.c"
    "src/pb_proto.c"
    "src/response.c"
    "src/rule.c"
    "src/signer.c"
    "src/u128.c"
    "src/utils.c"
    "src/crypto/bignum.c"
    "src/crypto/ecdsa.c"
    "src/crypto/hmac_drbg.c"
    "src/crypto/memzero.c"
    "src/crypto/rand.c"
    "src/crypto/rfc6979.c"
    "src/crypto/secp256k1.c"
    "src/crypto/sha2.c"
    "src/crypto/sha3.c"
)

set(COMPONENT_ADD_INCLUDEDIRS 
    "src/"
    "src/crypto"
)

register_component()