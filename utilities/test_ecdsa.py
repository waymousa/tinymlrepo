import ecdsa
from hashlib import sha256

message = '68656c6c6f2066726f6d20415753204564754b6974'
public_key = '9A94C6A3F7922DB74E0552C09D9C5529438379D8797E8CE448DDB953F02A653097B23E971BE0EC700B07A255FED0FEFB4527FE4DEAEE071A7A99B40F8193AAC0'
sig = '68D27AAC01D7768013E01D672958B7889C9F667DA1E7797CBFE21E23D7BE120892FE12388202CFE2B922D5EF57BBB5D377B9FE893A6DC0B084417846CCE0B266'

vk = ecdsa.VerifyingKey.from_string(bytes.fromhex(public_key), curve=ecdsa.NIST256p, hashfunc=sha256)
vk.verify(bytes.fromhex(sig), bytes.fromhex(message))