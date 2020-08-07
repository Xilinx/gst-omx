# name of the binary
bin_PROGRAMS = zynqmp_vcu_encode \
  zynqmp_sei_extraction \
  gst-n-launch-1.0 \
  zynqmp_relative_qp_insertion \
  zynqmp_appsrc_appsink_transcode

# list of source files
zynqmp_vcu_encode_SOURCES = test-vcu-encode.c
zynqmp_sei_extraction_SOURCES = test-sei-extraction.c
zynqmp_relative_qp_insertion_SOURCES = zynqmp_relative_qp_insertion.c
zynqmp_appsrc_appsink_transcode_SOURCES = zynqmp_appsrc_appsink_transcode.c

# CFLAGS and LDFLAGS used for compiling and linking
zynqmp_vcu_encode_CFLAGS = $(GST_CFLAGS)
zynqmp_vcu_encode_LDFLAGS = $(GST_LIBS) -lgstvideo-@GST_API_VERSION@

zynqmp_sei_extraction_CFLAGS = $(GST_CFLAGS)
zynqmp_sei_extraction_LDFLAGS = $(GST_LIBS)

gst_n_launch_1_0_SOURCES = gstnlaunch.c
gst_n_launch_1_0_CFLAGS = $(GST_CFLAGS)
gst_n_launch_1_0_LDFLAGS = $(GST_LIBS)

zynqmp_relative_qp_insertion_CFLAGS = $(GST_CFLAGS)
zynqmp_relative_qp_insertion_LDFLAGS = $(GST_LIBS) -lgstvideo-@GST_API_VERSION@

zynqmp_appsrc_appsink_transcode_CFLAGS = $(GST_CFLAGS)
zynqmp_appsrc_appsink_transcode_LDFLAGS = $(GST_LIBS) -lgstvideo-@GST_API_VERSION@ -lgstallocators-@GST_API_VERSION@ `pkg-config --libs gstreamer-app-1.0`
