# name of the binary
if USE_OMX_TARGET_ZYNQ_USCALE_PLUS
bin_PROGRAMS = zynqmp_vcu_encode
endif

# list of source files
zynqmp_vcu_encode_SOURCES = test-vcu-encode.c

# CFLAGS and LDFLAGS used for compiling and linking
zynqmp_vcu_encode_CFLAGS = $(GST_CFLAGS)
zynqmp_vcu_encode_LDFLAGS = $(GST_LIBS) -lgstvideo-@GST_API_VERSION@
