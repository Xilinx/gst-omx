# name of the binary
bin_PROGRAMS = zynqmp_vcu_encode \
  zynqmp_sei_extraction

# list of source files
zynqmp_vcu_encode_SOURCES = test-vcu-encode.c
zynqmp_sei_extraction_SOURCES = test-sei-extraction.c

# CFLAGS and LDFLAGS used for compiling and linking
zynqmp_vcu_encode_CFLAGS = $(GST_CFLAGS)
zynqmp_vcu_encode_LDFLAGS = $(GST_LIBS) -lgstvideo-@GST_API_VERSION@

zynqmp_sei_extraction_CFLAGS = $(GST_CFLAGS)
zynqmp_sei_extraction_LDFLAGS = $(GST_LIBS)
