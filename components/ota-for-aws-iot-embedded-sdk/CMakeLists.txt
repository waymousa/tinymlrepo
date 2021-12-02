
set(COMPONENT_SRCS 
    "source/ota_base64.c"
    "source/ota_cbor.c"
    "source/ota_interface.c"
    "source/ota_mqtt.c"
    "source/ota.c"
    "source/portable/os/ota_os_freertos.c"
    "source/dependency/3rdparty/tinycbor/src/cborerrorstrings.c"
    "source/dependency/3rdparty/tinycbor/src/cborparser.c"
    "source/dependency/3rdparty/tinycbor/src/cborparser_dup_string.c"
    "source/dependency/3rdparty/tinycbor/src/cborpretty.c"
    "source/dependency/3rdparty/tinycbor/src/cborpretty_stdio.c"
    "source/dependency/3rdparty/tinycbor/src/cbortojson.c"
    "source/dependency/3rdparty/tinycbor/src/cborvalidation.c"
    "source/dependency/3rdparty/tinycbor/src/open_memstream.c"
    "source/dependency/coreJSON/source/core_json.c"
)

set(COMPONENT_ADD_INCLUDEDIRS 
    "source/include"
    "source/portable/os"
    "source/dependency/3rdparty/tinycbor/src"
    "source/dependency/coreJSON/source/include"
)


register_component()