/*
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <string.h>
#include <gst/allocators/gstdmabuf.h>
#include <stdlib.h>

#include "gstomxvideo.h"
#include "gstomxvideoenc.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_video_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_video_enc_debug_category

#define GST_TYPE_OMX_VIDEO_ENC_CONTROL_RATE (gst_omx_video_enc_control_rate_get_type ())
static GType
gst_omx_video_enc_control_rate_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
#ifndef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
      {OMX_Video_ControlRateDisable, "Disable", "disable"},
      {OMX_Video_ControlRateVariable, "Variable", "variable"},
      {OMX_Video_ControlRateConstant, "Constant", "constant"},
      {OMX_Video_ControlRateVariableSkipFrames, "Variable Skip Frames",
          "variable-skip-frames"},
      {OMX_Video_ControlRateConstantSkipFrames, "Constant Skip Frames",
          "constant-skip-frames"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
#else
      {OMX_Video_ControlRateDisable, "Disable", "CONST_QP"},
      {OMX_Video_ControlRateVariable, "Variable", "variable bitrate(VBR)"},
      {OMX_Video_ControlRateConstant, "Constant", "constant bitrate(CBR)"},
      {OMX_ALG_Video_ControlRateLowLatency, "Lowlatency", "Low Latency rate control"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
#endif
    };

    qtype = g_enum_register_static ("GstOMXVideoEncControlRate", values);
  }
  return qtype;
}

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS

#define GST_TYPE_OMX_VIDEO_ENC_INPUT_MODE_TYPE (gst_omx_video_enc_input_mode_type ())
static GType
gst_omx_video_enc_input_mode_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_Enc_InputMode_DefaultImplementation, "DefaultImplementation",
          "default"},
      {OMX_Enc_InputMode_ZeroCopy, "ZeroCopy", "zerocopy"},
      {OMX_Enc_InputMode_DMABufImport, "DMABufImport", "dma-import"},
      {OMX_Enc_InputMode_DMABufExport, "DMABufExport", "dma-export"},
      {0xffffffff, "DefaultImplementation", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncInputModeType", values);
  }
  return qtype;
}


#define GST_TYPE_OMX_VIDEO_ENC_QP_MODE (gst_omx_video_enc_qp_mode_get_type ())
static GType
gst_omx_video_enc_qp_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_ALG_UNIFORM_QP, "Use the same QP for all coding units of the frame",
          "uniform"},
      {OMX_ALG_AUTO_QP,
            "Let the VCU encoder change the QP for each coding unit according to its content",
          "auto"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncQpMode", values);
  }
  return qtype;
}

#define GST_TYPE_OMX_VIDEO_ENC_GOP_MODE (gst_omx_video_enc_gop_mode_get_type ())
static GType
gst_omx_video_enc_gop_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_ALG_GOP_MODE_DEFAULT, "Basic GOP settings","default"},
      {OMX_ALG_GOP_MODE_PYRAMIDAL,"Advanced Gop pattern with hierarchical B frame","pyramidal"},
      {OMX_ALG_GOP_MODE_LOW_DELAY_P,"In this Gop pattern,single I-frame followed by P-frames only","low_delay_p"},
      {OMX_ALG_GOP_MODE_LOW_DELAY_B,"In this Gop pattern,single I-frame followed by B-frames only ","low_delay_b"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncGopMode", values);
  }
  return qtype;
}

#define GST_TYPE_OMX_VIDEO_ENC_GDR_MODE (gst_omx_video_enc_gdr_mode_get_type ())
static GType
gst_omx_video_enc_gdr_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_ALG_GDR_OFF, "No GDR","disable"},
      {OMX_ALG_GDR_VERTICAL,"Gradual refresh using a vertical bar moving from left to right","vertical"},
      {OMX_ALG_GDR_HORIZONTAL,"Gradual refresh using a horizontal bar moving from top to bottom","horizontal"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncGdrMode", values);
  }
  return qtype;
}

#define GST_TYPE_OMX_VIDEO_ENC_SCALING_LIST (gst_omx_video_enc_scaling_list_get_type ())
static GType
gst_omx_video_enc_scaling_list_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_ALG_SCL_FLAT, "FLAT scaling list mode","flat"},
      {OMX_ALG_SCL_DEFAULT,"Default scaling list mode","default"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncScalingList", values);
  }
  return qtype;
}

#define GST_TYPE_OMX_VIDEO_ENC_ASPECT_RATIO (gst_omx_video_enc_aspect_ratio_get_type ())
static GType
gst_omx_video_enc_aspect_ratio_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_ALG_ASPECT_RATIO_AUTO, "4:3 for SD video,16:9 for HD video,unspecified for unknown format","auto"},
      {OMX_ALG_ASPECT_RATIO_4_3,"4:3 aspect ratio","aspect_ratio_4_3"},
      {OMX_ALG_ASPECT_RATIO_16_9,"16:9 aspect ratio","aspect_ratio_16_9"},
      {OMX_ALG_ASPECT_RATIO_NONE,"Aspect ratio information is not present in the stream","none"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncAspectRatio", values);
  }
  return qtype;
}

#define GST_TYPE_OMX_VIDEO_ENC_LATENCY_MODE (gst_omx_video_enc_latency_mode_get_type ())
static GType
gst_omx_video_enc_latency_mode_get_type ()
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {FALSE, "Normal mode", "normal"},
      {TRUE, "Low latency mode", "low-latency"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncLatencyMode", values);
  }
  return qtype;
}

#endif
/* prototypes */
static void gst_omx_video_enc_finalize (GObject * object);
static void gst_omx_video_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_video_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);


static GstStateChangeReturn
gst_omx_video_enc_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_omx_video_enc_open (GstVideoEncoder * encoder);
static gboolean gst_omx_video_enc_close (GstVideoEncoder * encoder);
static gboolean gst_omx_video_enc_start (GstVideoEncoder * encoder);
static gboolean gst_omx_video_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_omx_video_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static gboolean gst_omx_video_enc_flush (GstVideoEncoder * encoder);
static GstFlowReturn gst_omx_video_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static gboolean gst_omx_video_enc_finish (GstVideoEncoder * encoder);
static gboolean gst_omx_video_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static GstCaps *gst_omx_video_enc_getcaps (GstVideoEncoder * encoder,
    GstCaps * filter);

static GstFlowReturn gst_omx_video_enc_drain (GstOMXVideoEnc * self,
    gboolean at_eos);

static GstFlowReturn gst_omx_video_enc_handle_output_frame (GstOMXVideoEnc *
    self, GstOMXPort * port, GstOMXBuffer * buf, GstVideoCodecFrame * frame);

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
static gboolean *gst_omx_video_enc_sink_event (GstVideoEncoder * encoder,
    GstEvent * event);
#endif

enum
{
  PROP_0,
  PROP_CONTROL_RATE,
  PROP_TARGET_BITRATE,
  PROP_QUANT_I_FRAMES,
  PROP_QUANT_P_FRAMES,
  PROP_QUANT_B_FRAMES,
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  PROP_STRIDE,
  PROP_INPUT_MODE,
  PROP_L2CACHE,
  PROP_SLICEHEIGHT,
  PROP_QP_MODE,
  PROP_NUM_SLICES,
  PROP_MIN_QUANT,
  PROP_MAX_QUANT,
  PROP_GOP_MODE,
  PROP_GDR_MODE,
  PROP_INITIAL_DELAY,
  PROP_CPB_SIZE,
  PROP_SCALING_LIST,
  PROP_GOP_FREQ_IDR,
  PROP_LOW_BANDWIDTH,
  PROP_MAX_BITRATE,
  PROP_ASPECT_RATIO,
  PROP_FILLER_DATA,
  PROP_LATENCY_MODE
#endif
};

/* FIXME: Better defaults */
#define GST_OMX_VIDEO_ENC_CONTROL_RATE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_TARGET_BITRATE_DEFAULT (0xffffffff)
#ifndef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
#define GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT (0xffffffff)
#else
#define GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT (30)
#define GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT (30)
#define GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT (30)
#endif
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
#define GST_OMX_VIDEO_ENC_INPUT_MODE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_L2CACHE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_STRIDE_DEFAULT (1)
#define GST_OMX_VIDEO_ENC_SLICEHEIGHT_DEFAULT (1)
#define GST_OMX_VIDEO_ENC_QP_MODE_DEFAULT (0xffffffff) 
#define GST_OMX_VIDEO_ENC_NUM_SLICES_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_MIN_QP_DEFAULT (10)
#define GST_OMX_VIDEO_ENC_MAX_QP_DEFAULT (51)
#define GST_OMX_VIDEO_ENC_GOP_MODE_DEFAULT (0xffffffff) 
#define GST_OMX_VIDEO_ENC_GDR_MODE_DEFAULT (0xffffffff) 
#define GST_OMX_VIDEO_ENC_INITIAL_DELAY_DEFAULT (1500) 
#define GST_OMX_VIDEO_ENC_CPB_SIZE_DEFAULT (3000)
#define GST_OMX_VIDEO_ENC_SCALING_LIST_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_GOP_FREQ_IDR_DEFAULT (0x7FFFFFF)
#define GST_OMX_VIDEO_ENC_LOW_BANDWIDTH_DEFAULT (FALSE)
#define GST_OMX_VIDEO_ENC_MAX_BITRATE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_ASPECT_RATIO_DEFAULT (OMX_ALG_ASPECT_RATIO_AUTO)
#define GST_OMX_VIDEO_ENC_FILLER_DATA_DEFAULT (TRUE)
#define GST_OMX_VIDEO_ENC_LATENCY_MODE_DEFAULT (0xffffffff)
#endif
 

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_video_enc_debug_category, "omxvideoenc", 0, \
      "debug category for gst-omx video encoder base class");

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstOMXVideoEnc, gst_omx_video_enc,
    GST_TYPE_VIDEO_ENCODER, DEBUG_INIT);

gboolean (*sink_event_backup) (GstVideoEncoder * encoder, GstEvent * event);

static void
gst_omx_video_enc_class_init (GstOMXVideoEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *video_encoder_class = GST_VIDEO_ENCODER_CLASS (klass);


  gobject_class->finalize = gst_omx_video_enc_finalize;
  gobject_class->set_property = gst_omx_video_enc_set_property;
  gobject_class->get_property = gst_omx_video_enc_get_property;

  g_object_class_install_property (gobject_class, PROP_CONTROL_RATE,
      g_param_spec_enum ("control-rate", "Control Rate",
          "Bitrate control method",
          GST_TYPE_OMX_VIDEO_ENC_CONTROL_RATE,
          GST_OMX_VIDEO_ENC_CONTROL_RATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

#ifndef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  g_object_class_install_property (gobject_class, PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate", "Target Bitrate",
          "Target bitrate (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_TARGET_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));
#else
  g_object_class_install_property (gobject_class, PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate", "Target Bitrate in Kbps",
          "Target bitrate in Kbps (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_TARGET_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
#endif

#ifndef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  g_object_class_install_property (gobject_class, PROP_QUANT_I_FRAMES,
      g_param_spec_uint ("quant-i-frames", "I-Frame Quantization",
          "Quantization parameter for I-frames (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_QUANT_P_FRAMES,
      g_param_spec_uint ("quant-p-frames", "P-Frame Quantization",
          "Quantization parameter for P-frames (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_QUANT_B_FRAMES,
      g_param_spec_uint ("quant-b-frames", "B-Frame Quantization",
          "Quantization parameter for B-frames (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
#else
  g_object_class_install_property (gobject_class, PROP_QUANT_I_FRAMES,
      g_param_spec_uint ("quant-i-frames", "I-Frame Quantization",
          "Quantization parameter for I-frames, Used in CONST_QP mode,"
	  "In other mode used as setting initial QP(30=component default)",
          0, 51, GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_QUANT_P_FRAMES,
      g_param_spec_uint ("quant-p-frames", "P-Frame Quantization",
          "Quantization parameter for P-frames, Used in CONST_QP mode,"
	  "In other mode used as setting initial QP(30=component default)",
          0, 51, GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_QUANT_B_FRAMES,
      g_param_spec_uint ("quant-b-frames", "B-Frame Quantization",
          "Quantization parameter for B-frames, Used in CONST_QP mode,"
	  "In other mode used as setting initial QP(30=component default)",
          0, 51, GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
#endif

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  g_object_class_install_property (gobject_class, PROP_STRIDE,
      g_param_spec_uint ("stride", "Value of stride",
          "Set it when input has alignment requirement in width (1=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_STRIDE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));


  g_object_class_install_property (gobject_class, PROP_INPUT_MODE,
      g_param_spec_enum ("ip-mode", "input mode",
          "input port's configuration mode",
          GST_TYPE_OMX_VIDEO_ENC_INPUT_MODE_TYPE,
          GST_OMX_VIDEO_ENC_INPUT_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_L2CACHE,
      g_param_spec_uint ("prefetch-buffer-size", "Value of L2Cache buffer size",
          "Value of encoder L2Cache buffer size in KB (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_L2CACHE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SLICEHEIGHT,
      g_param_spec_uint ("sliceHeight", "Value of nsliceHeight",
          "Set it when input has alignment requirement in height (1=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_SLICEHEIGHT_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_QP_MODE,
      g_param_spec_enum ("qp-mode", "QP mode",
          "QP control mode used by the VCU encoder",
          GST_TYPE_OMX_VIDEO_ENC_QP_MODE,
          GST_OMX_VIDEO_ENC_QP_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_NUM_SLICES,
      g_param_spec_uint ("num-slices", "Number of slices",
          "Number of slices used for each frame. Each slice contains one or more complete macroblock/CTU row(s). "
          "Slices are distributed over the frame as regularly as possible. (0xffffffff=component default)",
          1, G_MAXUINT, GST_OMX_VIDEO_ENC_NUM_SLICES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_MIN_QUANT,
      g_param_spec_uint ("min-qp", "min Quantization value",
          "Minimum QP value allowed in VBR rate control (10=component default)",
          0, 51, GST_OMX_VIDEO_ENC_MIN_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_MAX_QUANT,
      g_param_spec_uint ("max-qp", "max Quantization value",
          "Maximum QP value allowed in encoding session (51=component default)",
          0, 51, GST_OMX_VIDEO_ENC_MAX_QP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_GOP_MODE,
      g_param_spec_enum ("gop-mode", "GOP mode",
          "Specifies the Group Of Pictures configuration mode",
          GST_TYPE_OMX_VIDEO_ENC_GOP_MODE,
          GST_OMX_VIDEO_ENC_GOP_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_GDR_MODE,
      g_param_spec_enum ("gdr-mode", "GDR mode",
          "Specifies which Gradual Decoder Refresh scheme should be used or not"
	  "Only used if GopCtrlMode is set to LOW_DELAY_P", 
          GST_TYPE_OMX_VIDEO_ENC_GDR_MODE,
          GST_OMX_VIDEO_ENC_GDR_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_INITIAL_DELAY,
      g_param_spec_uint ("initial-delay", "value of initial-delay",
          "Specifies the initial removal delay as specified in the HRD model in msec"
	  "Not used when RateCtrlMode = CONST_QP (1500=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_INITIAL_DELAY_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_CPB_SIZE,
      g_param_spec_uint ("cpb-size", "value of cpb-size",
          "Specifies the Coded Picture Buffer as specified in the HRD model in msec"
	  "Not used when RateCtrlMode = CONST_QP (3000=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_CPB_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_SCALING_LIST,
      g_param_spec_enum ("scaling-list", "scaling list mode",
          "specifies the scaling list mode",
          GST_TYPE_OMX_VIDEO_ENC_SCALING_LIST,
          GST_OMX_VIDEO_ENC_SCALING_LIST_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_GOP_FREQ_IDR,
      g_param_spec_uint ("gop-freq-idr", "value of Gop-FreqIDR",
          "Specifies the number of frames between consequtive IDR pictures,"
	  "By default only the 1st frame is IDR (0x7FFFFFF=component default)",
          1, G_MAXUINT, GST_OMX_VIDEO_ENC_GOP_FREQ_IDR_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LOW_BANDWIDTH,
      g_param_spec_boolean ("low-bandwidth", "Enable low bandwidth mode",
          "Enable low bandwith mode, It will decrease the vertical search range"
	  "used for P-frame motion estimation to reduce the bandwith",
          GST_OMX_VIDEO_ENC_LOW_BANDWIDTH_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate in Kbps",
          "Max bitrate in Kbps,(Set it to 0 to keep max-bitrate same "
	  "as target-bitrate) (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_MAX_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class, PROP_ASPECT_RATIO,
      g_param_spec_enum ("aspect-ratio", "Aspect ratio",
          "selects the display aspect ratio of the video sequence to be written in SPS/VUI",
          GST_TYPE_OMX_VIDEO_ENC_ASPECT_RATIO,
          GST_OMX_VIDEO_ENC_ASPECT_RATIO_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_FILLER_DATA,
      g_param_spec_boolean ("filler-data", "Control filler data",
          "Enable/Disable filler data adding functanility",
          GST_OMX_VIDEO_ENC_FILLER_DATA_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LATENCY_MODE,
      g_param_spec_enum ("latency-mode", "latency mode",
          "Encoder latency mode",
          GST_TYPE_OMX_VIDEO_ENC_LATENCY_MODE,
          GST_OMX_VIDEO_ENC_LATENCY_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_change_state);

  video_encoder_class->open = GST_DEBUG_FUNCPTR (gst_omx_video_enc_open);
  video_encoder_class->close = GST_DEBUG_FUNCPTR (gst_omx_video_enc_close);
  video_encoder_class->start = GST_DEBUG_FUNCPTR (gst_omx_video_enc_start);
  video_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_omx_video_enc_stop);
  video_encoder_class->flush = GST_DEBUG_FUNCPTR (gst_omx_video_enc_flush);
  video_encoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_set_format);
  video_encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_handle_frame);
  video_encoder_class->finish = GST_DEBUG_FUNCPTR (gst_omx_video_enc_finish);
  video_encoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_propose_allocation);
  video_encoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_omx_video_enc_getcaps);
  sink_event_backup = video_encoder_class->sink_event;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  video_encoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_sink_event);
#endif

  klass->cdata.type = GST_OMX_COMPONENT_TYPE_FILTER;
  klass->cdata.default_sink_template_caps = "video/x-raw, "
      "width = " GST_VIDEO_SIZE_RANGE ", "
      "height = " GST_VIDEO_SIZE_RANGE ", " "framerate = " GST_VIDEO_FPS_RANGE;

  klass->handle_output_frame =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_handle_output_frame);
}

static void
gst_omx_video_enc_init (GstOMXVideoEnc * self)
{
  self->control_rate = GST_OMX_VIDEO_ENC_CONTROL_RATE_DEFAULT;
  self->target_bitrate = GST_OMX_VIDEO_ENC_TARGET_BITRATE_DEFAULT;
  self->quant_i_frames = GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT;
  self->quant_p_frames = GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT;
  self->quant_b_frames = GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  self->qp_mode = GST_OMX_VIDEO_ENC_QP_MODE_DEFAULT;
  self->num_slices = GST_OMX_VIDEO_ENC_NUM_SLICES_DEFAULT;
  self->min_qp = GST_OMX_VIDEO_ENC_MIN_QP_DEFAULT;
  self->max_qp = GST_OMX_VIDEO_ENC_MAX_QP_DEFAULT;
  self->gop_mode = GST_OMX_VIDEO_ENC_GOP_MODE_DEFAULT;
  self->gdr_mode = GST_OMX_VIDEO_ENC_GDR_MODE_DEFAULT;
  self->cpb_size = GST_OMX_VIDEO_ENC_CPB_SIZE_DEFAULT;
  self->initial_delay = GST_OMX_VIDEO_ENC_INITIAL_DELAY_DEFAULT;
  self->scaling_list = GST_OMX_VIDEO_ENC_SCALING_LIST_DEFAULT;
  self->gop_freq_idr = GST_OMX_VIDEO_ENC_GOP_FREQ_IDR_DEFAULT;
  self->max_bitrate = GST_OMX_VIDEO_ENC_MAX_BITRATE_DEFAULT;
  self->aspect_ratio = GST_OMX_VIDEO_ENC_ASPECT_RATIO_DEFAULT;
  self->filler_data = GST_OMX_VIDEO_ENC_FILLER_DATA_DEFAULT;
  self->latency_mode = GST_OMX_VIDEO_ENC_LATENCY_MODE_DEFAULT;
#endif

  g_mutex_init (&self->drain_lock);
  g_cond_init (&self->drain_cond);
}


#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS

#define CHECK_ERR(setting) \
  if (err == OMX_ErrorUnsupportedIndex || err == OMX_ErrorUnsupportedSetting) { \
    GST_WARNING_OBJECT (self, \
        "Setting " setting " parameters not supported by the component"); \
  } else if (err != OMX_ErrorNone) { \
    GST_ERROR_OBJECT (self, \
        "Failed to set " setting " parameters: %s (0x%08x)", \
        gst_omx_error_to_string (err), err); \
    return FALSE; \
  }

static gboolean
set_zynqultrascaleplus_props (GstOMXVideoEnc * self)
{
  OMX_ERRORTYPE err;

  if (self->qp_mode != GST_OMX_VIDEO_ENC_QP_MODE_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_QUANTIZATION_CONTROL quant;

    GST_OMX_INIT_STRUCT (&quant);
    quant.nPortIndex = self->enc_out_port->index;
    quant.eQpControlMode = self->qp_mode;

    GST_DEBUG_OBJECT (self, "setting QP mode to %d", self->qp_mode);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoQuantizationControl, &quant);
    CHECK_ERR ("quantization");
  }

  if (self->num_slices != GST_OMX_VIDEO_ENC_NUM_SLICES_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_SLICES slices;

    GST_OMX_INIT_STRUCT (&slices);
    slices.nPortIndex = self->enc_out_port->index;

    err = gst_omx_component_get_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSlices, &slices);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to get HEVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    slices.nNumSlices = self->num_slices;
    GST_DEBUG_OBJECT (self, "setting number of slices to %d", self->num_slices);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSlices, &slices);
    CHECK_ERR ("slices");
  }

  if (self->min_qp != GST_OMX_VIDEO_ENC_MIN_QP_DEFAULT ||
	self->max_qp != GST_OMX_VIDEO_ENC_MAX_QP_DEFAULT ) {
    OMX_ALG_VIDEO_PARAM_QUANTIZATION_EXTENSION qp_values;

    GST_OMX_INIT_STRUCT (&qp_values);
    qp_values.nPortIndex = self->enc_out_port->index;
    qp_values.nQpMin = self->min_qp;
    qp_values.nQpMax = self->max_qp;

    GST_DEBUG_OBJECT (self, "setting min QP as %d and max QP as %d", self->min_qp, self->max_qp);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE)OMX_ALG_IndexParamVideoQuantizationExtension, &qp_values);
    CHECK_ERR ("min-qp & max-qp");
  }

  if (self->l2cache != GST_OMX_VIDEO_ENC_L2CACHE_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_PREFETCH_BUFFER prefetch_buffer;

    GST_OMX_INIT_STRUCT (&prefetch_buffer);
    prefetch_buffer.nPortIndex = self->enc_out_port->index;
    prefetch_buffer.nPrefetchBufferSize = self->l2cache;

    GST_DEBUG_OBJECT (self, "setting prefetch buffer size %d", self->l2cache);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoPrefetchBuffer, &prefetch_buffer);
    CHECK_ERR ("prefetch");
  }

  if (self->gop_mode != GST_OMX_VIDEO_ENC_GOP_MODE_DEFAULT ||
	self->gdr_mode != GST_OMX_VIDEO_ENC_GDR_MODE_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_GOP_CONTROL gop_mode;

    if(self->gdr_mode == OMX_ALG_GDR_VERTICAL ||
	self->gdr_mode == OMX_ALG_GDR_HORIZONTAL) {
	if(self->gop_mode != OMX_ALG_GOP_MODE_LOW_DELAY_P) {
        	GST_ERROR_OBJECT (self,
                	"GDR mode only can be set in LOW_DELAY_P gop\n");
       	 	return FALSE;
	}
    }

    GST_OMX_INIT_STRUCT (&gop_mode);
    gop_mode.nPortIndex = self->enc_out_port->index;
    gop_mode.eGopControlMode = self->gop_mode;
    gop_mode.eGdrMode = self->gdr_mode;

    GST_DEBUG_OBJECT (self, "setting GOP mode to %d and GDR mode to %d", 
	self->gop_mode,self->gdr_mode);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoGopControl, &gop_mode);
    CHECK_ERR ("GOP & GDR");
  }

  if (self->cpb_size != GST_OMX_VIDEO_ENC_CPB_SIZE_DEFAULT || 
	self->initial_delay != GST_OMX_VIDEO_ENC_INITIAL_DELAY_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_CODED_PICTURE_BUFFER cpb;

    GST_OMX_INIT_STRUCT (&cpb);
    cpb.nPortIndex = self->enc_out_port->index;
    cpb.nCodedPictureBufferSize = self->cpb_size;
    cpb.nInitialRemovalDelay = self->initial_delay;

    GST_DEBUG_OBJECT (self, "setting cpb size as %d and initial delay as %d\n",
	 self->cpb_size,self->initial_delay);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoCodedPictureBuffer, &cpb);
    CHECK_ERR ("cpb size & initial delay");
  }
  
  if (self->scaling_list != GST_OMX_VIDEO_ENC_SCALING_LIST_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_SCALING_LIST scaling_list;

    GST_OMX_INIT_STRUCT (&scaling_list);
    scaling_list.nPortIndex = self->enc_out_port->index;
    scaling_list.eScalingListMode = self->scaling_list;

    GST_DEBUG_OBJECT (self, "setting scaling list mode as %d \n",
	 self->scaling_list);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoScalingList, &scaling_list);
    CHECK_ERR ("scaling-list");
  }

  if (self->gop_freq_idr != GST_OMX_VIDEO_ENC_GOP_FREQ_IDR_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_INSTANTANEOUS_DECODING_REFRESH gop_freq_idr;

    GST_OMX_INIT_STRUCT (&gop_freq_idr);
    gop_freq_idr.nPortIndex = self->enc_out_port->index;
    gop_freq_idr.nInstantaneousDecodingRefreshFrequency = self->gop_freq_idr;

    GST_DEBUG_OBJECT (self, "setting GopFreqIDR as %d \n",
	 self->gop_freq_idr);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoInstantaneousDecodingRefresh, &gop_freq_idr);
    CHECK_ERR ("GopFreqIDR");
  }

  if (self->low_bandwidth != GST_OMX_VIDEO_ENC_LOW_BANDWIDTH_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_LOW_BANDWIDTH low_bw;

    GST_OMX_INIT_STRUCT (&low_bw);
    low_bw.nPortIndex = self->enc_out_port->index;
    low_bw.bEnableLowBandwidth = self->low_bandwidth;

    GST_DEBUG_OBJECT (self, "setting LOW Bandwith mode as %d \n",
	 self->low_bandwidth);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoLowBandwidth, &low_bw);
    CHECK_ERR ("low-bandwidth");
  }

  if (self->max_bitrate != GST_OMX_VIDEO_ENC_MAX_BITRATE_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_MAX_BITRATE max_bitrate;

    GST_OMX_INIT_STRUCT (&max_bitrate);
    max_bitrate.nPortIndex = self->enc_out_port->index;
    max_bitrate.nMaxBitrate = self->max_bitrate;

    GST_DEBUG_OBJECT (self, "setting Max bitrate as %d \n",
	 self->max_bitrate);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoMaxBitrate, &max_bitrate);
    CHECK_ERR ("max-bitrate");
  }

  if (self->aspect_ratio != GST_OMX_VIDEO_ENC_ASPECT_RATIO_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_ASPECT_RATIO aspect_ratio;

    GST_OMX_INIT_STRUCT (&aspect_ratio);
    aspect_ratio.nPortIndex = self->enc_out_port->index;
    aspect_ratio.eAspectRatio = self->aspect_ratio;

    GST_DEBUG_OBJECT (self, "setting Aspect Ratio as %d \n",
	 self->aspect_ratio);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoAspectRatio, &aspect_ratio);
    CHECK_ERR ("aspect-ratio");
  }

  if (self->filler_data != GST_OMX_VIDEO_ENC_FILLER_DATA_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_FILLER_DATA filler_data;

    GST_OMX_INIT_STRUCT (&filler_data);
    filler_data.nPortIndex = self->enc_out_port->index;
    filler_data.bDisableFillerData = !(self->filler_data);

    GST_DEBUG_OBJECT (self, "setting Filler data mode as %d \n",
        self->filler_data);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoFillerData, &filler_data);
    CHECK_ERR ("filler-data");
  }

  if (self->latency_mode != GST_OMX_VIDEO_ENC_LATENCY_MODE_DEFAULT) {
    OMX_ALG_VIDEO_PARAM_SUBFRAME subframe_mode;
    GST_OMX_INIT_STRUCT (&subframe_mode);
    subframe_mode.nPortIndex = self->enc_out_port->index;
    subframe_mode.bEnableSubframe = self->latency_mode;

    GST_DEBUG_OBJECT (self, "setting latency mode to %d",
          self->latency_mode);

    err =
        gst_omx_component_set_parameter (self->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSubframe,
        &subframe_mode);
    CHECK_ERR ("latency mode");
  }

  return TRUE;
}
#endif



static gboolean
gst_omx_video_enc_open (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (encoder);
  GstOMXVideoEncClass *klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);
  gint in_port_index, out_port_index;

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  OMX_ALG_PORT_PARAM_BUFFER_MODE enable_dmabuf;
  OMX_ERRORTYPE err;

  static int use_dmabuf = 0;

  if ((self->input_mode == OMX_Enc_InputMode_DMABufImport) ||
      (self->input_mode == OMX_Enc_InputMode_DMABufExport))
    use_dmabuf = 1;
#endif

  self->enc =
      gst_omx_component_new (GST_OBJECT_CAST (self), klass->cdata.core_name,
      klass->cdata.component_name, klass->cdata.component_role,
      klass->cdata.hacks);
  self->started = FALSE;

  if (!self->enc)
    return FALSE;

  if (gst_omx_component_get_state (self->enc,
          GST_CLOCK_TIME_NONE) != OMX_StateLoaded)
    return FALSE;

  in_port_index = klass->cdata.in_port_index;
  out_port_index = klass->cdata.out_port_index;

  if (in_port_index == -1 || out_port_index == -1) {
    OMX_PORT_PARAM_TYPE param;
    OMX_ERRORTYPE err;

    GST_OMX_INIT_STRUCT (&param);

    err =
        gst_omx_component_get_parameter (self->enc, OMX_IndexParamVideoInit,
        &param);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self, "Couldn't get port information: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      /* Fallback */
      in_port_index = 0;
      out_port_index = 1;
    } else {
      GST_DEBUG_OBJECT (self, "Detected %u ports, starting at %u",
          (guint) param.nPorts, (guint) param.nStartPortNumber);
      in_port_index = param.nStartPortNumber + 0;
      out_port_index = param.nStartPortNumber + 1;
    }
  }

  self->enc_in_port = gst_omx_component_add_port (self->enc, in_port_index);
  self->enc_out_port = gst_omx_component_add_port (self->enc, out_port_index);

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  GST_INFO_OBJECT (self, "Custom settings needed for zynq VCU");

  GST_OMX_INIT_STRUCT (&enable_dmabuf);
  if(use_dmabuf)
  	enable_dmabuf.eMode = OMX_ALG_BUF_DMA;
  else
  	enable_dmabuf.eMode = OMX_ALG_BUF_NORMAL;
  enable_dmabuf.nPortIndex = self->enc_in_port->index;
  enable_dmabuf.nSize = sizeof(enable_dmabuf);
  enable_dmabuf.nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;
  enable_dmabuf.nVersion.s.nVersionMinor = OMX_VERSION_MINOR;
  enable_dmabuf.nVersion.s.nRevision = OMX_VERSION_REVISION;
  enable_dmabuf.nVersion.s.nStep = OMX_VERSION_STEP;

  err = OMX_SetParameter (self->enc->handle, OMX_ALG_IndexPortParamBufferMode, &enable_dmabuf);
  if(err != OMX_ErrorNone) {
  	GST_ERROR_OBJECT (self,
              "Failed to set DMA mode parameters: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
        return FALSE;
  }

#endif

  if (!self->enc_in_port || !self->enc_out_port)
    return FALSE;

  /* Set properties */
  {
    OMX_ERRORTYPE err;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    if (!set_zynqultrascaleplus_props (self))
      return FALSE;
#endif

    if (self->control_rate != 0xffffffff || self->target_bitrate != 0xffffffff) {
      OMX_VIDEO_PARAM_BITRATETYPE bitrate_param;

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
        /* max-bitrate setting is necessary property for VBR rate control-mode  */
        if (self->control_rate == OMX_Video_ControlRateVariable &&
	        self->max_bitrate == GST_OMX_VIDEO_ENC_MAX_BITRATE_DEFAULT) {
                GST_ERROR_OBJECT (self, "max-bitrate is necessary property for VBR rate control mode");
                g_warning("max-bitrate setting is required for VBR rate control-mode and it should be >= target-bitrate");
                return FALSE;
        }
#endif

      GST_OMX_INIT_STRUCT (&bitrate_param);
      bitrate_param.nPortIndex = self->enc_out_port->index;

      err = gst_omx_component_get_parameter (self->enc,
          OMX_IndexParamVideoBitrate, &bitrate_param);

      if (err == OMX_ErrorNone) {
#ifdef USE_OMX_TARGET_RPI
        /* FIXME: Workaround for RPi returning garbage for this parameter */
        if (bitrate_param.nVersion.nVersion == 0) {
          GST_OMX_INIT_STRUCT (&bitrate_param);
          bitrate_param.nPortIndex = self->enc_out_port->index;
        }
#endif
        if (self->control_rate != 0xffffffff)
          bitrate_param.eControlRate = self->control_rate;
        if (self->target_bitrate != 0xffffffff)
          bitrate_param.nTargetBitrate = self->target_bitrate;


        err =
            gst_omx_component_set_parameter (self->enc,
            OMX_IndexParamVideoBitrate, &bitrate_param);
        if (err == OMX_ErrorUnsupportedIndex) {
          GST_WARNING_OBJECT (self,
              "Setting a bitrate not supported by the component");
        } else if (err == OMX_ErrorUnsupportedSetting) {
          GST_WARNING_OBJECT (self,
              "Setting bitrate settings %u %u not supported by the component",
              self->control_rate, self->target_bitrate);
        } else if (err != OMX_ErrorNone) {
          GST_ERROR_OBJECT (self,
              "Failed to set bitrate parameters: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
          return FALSE;
        }
      } else {
        GST_ERROR_OBJECT (self, "Failed to get bitrate parameters: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
      }
    }

    if (self->quant_i_frames != GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT ||
        self->quant_p_frames != GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT ||
        self->quant_b_frames != GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT) {
      OMX_VIDEO_PARAM_QUANTIZATIONTYPE quant_param;

      GST_OMX_INIT_STRUCT (&quant_param);
      quant_param.nPortIndex = self->enc_out_port->index;

      err = gst_omx_component_get_parameter (self->enc,
          OMX_IndexParamVideoQuantization, &quant_param);

      if (err == OMX_ErrorNone) {

        if (self->quant_i_frames != 0xffffffff)
          quant_param.nQpI = self->quant_i_frames;
        if (self->quant_p_frames != 0xffffffff)
          quant_param.nQpP = self->quant_p_frames;
        if (self->quant_b_frames != 0xffffffff)
          quant_param.nQpB = self->quant_b_frames;

        err =
            gst_omx_component_set_parameter (self->enc,
            OMX_IndexParamVideoQuantization, &quant_param);
        if (err == OMX_ErrorUnsupportedIndex) {
          GST_WARNING_OBJECT (self,
              "Setting quantization parameters not supported by the component");
        } else if (err == OMX_ErrorUnsupportedSetting) {
          GST_WARNING_OBJECT (self,
              "Setting quantization parameters %u %u %u not supported by the component",
              self->quant_i_frames, self->quant_p_frames, self->quant_b_frames);
        } else if (err != OMX_ErrorNone) {
          GST_ERROR_OBJECT (self,
              "Failed to set quantization parameters: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
          return FALSE;
        }
      } else {
        GST_ERROR_OBJECT (self,
            "Failed to get quantization parameters: %s (0x%08x)",
            gst_omx_error_to_string (err), err);

      }
    }
  }

  return TRUE;
}

static gboolean
gst_omx_video_enc_shutdown (GstOMXVideoEnc * self)
{
  OMX_STATETYPE state;

  GST_DEBUG_OBJECT (self, "Shutting down encoder");

  state = gst_omx_component_get_state (self->enc, 0);
  if (state > OMX_StateLoaded || state == OMX_StateInvalid) {
    if (state > OMX_StateIdle) {
      gst_omx_component_set_state (self->enc, OMX_StateIdle);
      gst_omx_component_get_state (self->enc, 5 * GST_SECOND);
    }
    gst_omx_component_set_state (self->enc, OMX_StateLoaded);
    gst_omx_port_deallocate_buffers (self->enc_in_port);
    gst_omx_port_deallocate_buffers (self->enc_out_port);
    if (state > OMX_StateLoaded)
      gst_omx_component_get_state (self->enc, 5 * GST_SECOND);
  }

  return TRUE;
}

static gboolean
gst_omx_video_enc_close (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Closing encoder");

  if (!gst_omx_video_enc_shutdown (self))
    return FALSE;

  self->enc_in_port = NULL;
  self->enc_out_port = NULL;
  if (self->enc)
    gst_omx_component_free (self->enc);
  self->enc = NULL;

  return TRUE;
}

static void
gst_omx_video_enc_finalize (GObject * object)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (object);

  g_mutex_clear (&self->drain_lock);
  g_cond_clear (&self->drain_cond);

  G_OBJECT_CLASS (gst_omx_video_enc_parent_class)->finalize (object);
}

static void
gst_omx_video_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (object);

  switch (prop_id) {
    case PROP_CONTROL_RATE:
      self->control_rate = g_value_get_enum (value);
      break;
    case PROP_TARGET_BITRATE:
      self->target_bitrate = g_value_get_uint (value);
      if (self->enc) {
        OMX_VIDEO_CONFIG_BITRATETYPE config;
        OMX_ERRORTYPE err;

        GST_OMX_INIT_STRUCT (&config);
        config.nPortIndex = self->enc_out_port->index;
        config.nEncodeBitrate = self->target_bitrate;
        err =
            gst_omx_component_set_config (self->enc,
            OMX_IndexConfigVideoBitrate, &config);
        if (err != OMX_ErrorNone)
          GST_ERROR_OBJECT (self,
              "Failed to set bitrate parameter: %s (0x%08x)",
              gst_omx_error_to_string (err), err);
      }
      break;
    case PROP_QUANT_I_FRAMES:
      self->quant_i_frames = g_value_get_uint (value);
      break;
    case PROP_QUANT_P_FRAMES:
      self->quant_p_frames = g_value_get_uint (value);
      break;
    case PROP_QUANT_B_FRAMES:
      self->quant_b_frames = g_value_get_uint (value);
      break;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    case PROP_STRIDE:
      self->stride = g_value_get_uint (value);
      break;
    case PROP_INPUT_MODE:
      self->input_mode = g_value_get_enum (value);
      if (self->input_mode == OMX_Enc_InputMode_DMABufExport) {
        GST_ERROR_OBJECT (self, "ERROR: DMA export is not supported yet\n");
      }
      break;
    case PROP_L2CACHE:
      self->l2cache = g_value_get_uint (value);
      break;
    case PROP_SLICEHEIGHT:
      self->sliceHeight = g_value_get_uint (value);
      break;
    case PROP_QP_MODE:
      self->qp_mode = g_value_get_enum (value);
      break;
    case PROP_NUM_SLICES:
      self->num_slices = g_value_get_uint (value);
      break;
    case PROP_MIN_QUANT:
      self->min_qp = g_value_get_uint (value);
      break;
    case PROP_MAX_QUANT:
      self->max_qp = g_value_get_uint (value);
      break;
    case PROP_GOP_MODE:
      self->gop_mode = g_value_get_enum (value);
      break;
    case PROP_GDR_MODE:
      self->gdr_mode = g_value_get_enum (value);
      break;
    case PROP_INITIAL_DELAY:
      self->initial_delay = g_value_get_uint (value);
      break;
    case PROP_CPB_SIZE:
      self->cpb_size = g_value_get_uint (value);
      break;
    case PROP_SCALING_LIST:
      self->scaling_list = g_value_get_enum (value);
      break;
    case PROP_GOP_FREQ_IDR:
      self->gop_freq_idr = g_value_get_uint (value);
      break;
    case PROP_LOW_BANDWIDTH:
      self->low_bandwidth = g_value_get_boolean (value);
      break;
    case PROP_MAX_BITRATE:
      self->max_bitrate = g_value_get_uint (value);
      break;
    case PROP_ASPECT_RATIO:
      self->aspect_ratio = g_value_get_enum (value);
      break;
    case PROP_FILLER_DATA:
      self->filler_data = g_value_get_boolean (value);
      break;
    case PROP_LATENCY_MODE:
      self->latency_mode = g_value_get_enum (value);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_video_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (object);

  switch (prop_id) {
    case PROP_CONTROL_RATE:
      g_value_set_enum (value, self->control_rate);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_uint (value, self->target_bitrate);
      break;
    case PROP_QUANT_I_FRAMES:
      g_value_set_uint (value, self->quant_i_frames);
      break;
    case PROP_QUANT_P_FRAMES:
      g_value_set_uint (value, self->quant_p_frames);
      break;
    case PROP_QUANT_B_FRAMES:
      g_value_set_uint (value, self->quant_b_frames);
      break;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    case PROP_STRIDE:
      g_value_set_uint (value, self->stride);
      break;
    case PROP_INPUT_MODE:
      g_value_set_enum (value, self->input_mode);
      break;
    case PROP_L2CACHE:
      g_value_set_uint (value, self->l2cache);
      break;
    case PROP_SLICEHEIGHT:
      g_value_set_uint (value, self->sliceHeight);
      break;
    case PROP_QP_MODE:
      g_value_set_enum (value, self->qp_mode);
      break;
    case PROP_NUM_SLICES:
      g_value_set_uint (value, self->num_slices);
      break;
    case PROP_MIN_QUANT:
      g_value_set_uint (value, self->min_qp);
      break;
    case PROP_MAX_QUANT:
      g_value_set_uint (value, self->max_qp);
      break;
    case PROP_GOP_MODE:
      g_value_set_enum (value, self->gop_mode);
      break;
    case PROP_GDR_MODE:
      g_value_set_enum (value, self->gdr_mode);
      break;
    case PROP_INITIAL_DELAY:
      g_value_set_uint (value, self->initial_delay);
      break;
    case PROP_CPB_SIZE:
      g_value_set_uint (value, self->cpb_size);
      break;
    case PROP_SCALING_LIST:
      g_value_set_enum (value, self->scaling_list);
      break;
    case PROP_GOP_FREQ_IDR:
      g_value_set_uint (value, self->gop_freq_idr);
      break;
    case PROP_LOW_BANDWIDTH:
      g_value_set_boolean (value, self->low_bandwidth);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, self->max_bitrate);
      break;
    case PROP_ASPECT_RATIO:
      g_value_set_enum (value, self->aspect_ratio);
      break;
    case PROP_FILLER_DATA:
      g_value_set_boolean (value, self->filler_data);
      break;
    case PROP_LATENCY_MODE:
      g_value_set_enum (value, self->latency_mode);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_omx_video_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstOMXVideoEnc *self;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  g_return_val_if_fail (GST_IS_OMX_VIDEO_ENC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_OMX_VIDEO_ENC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->downstream_flow_ret = GST_FLOW_OK;

      self->draining = FALSE;
      self->started = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (self->enc_in_port)
        gst_omx_port_set_flushing (self->enc_in_port, 5 * GST_SECOND, TRUE);
      if (self->enc_out_port)
        gst_omx_port_set_flushing (self->enc_out_port, 5 * GST_SECOND, TRUE);

      g_mutex_lock (&self->drain_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      g_mutex_unlock (&self->drain_lock);
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  ret =
      GST_ELEMENT_CLASS (gst_omx_video_enc_parent_class)->change_state (element,
      transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->downstream_flow_ret = GST_FLOW_FLUSHING;
      self->started = FALSE;

      if (!gst_omx_video_enc_shutdown (self))
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
get_chroma_info_from_input (GstOMXVideoEnc * self, const gchar ** chroma_format,
    guint * bit_depth_luma, guint * bit_depth_chroma)
{
  switch (self->input_state->info.finfo->format) {
    case GST_VIDEO_FORMAT_GRAY8:
      *chroma_format = "4:0:0";
      *bit_depth_luma = 8;
      *bit_depth_chroma = 0;
      break;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_NV12:
      *chroma_format = "4:2:0";
      *bit_depth_luma = *bit_depth_chroma = 8;
      break;
    case GST_VIDEO_FORMAT_NV16:
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
      *chroma_format = "4:2:2";
      *bit_depth_luma = *bit_depth_chroma = 8;
      break;
#if 0
    /* From https://bugzilla.gnome.org/show_bug.cgi?id=789876 */
    case GST_VIDEO_FORMAT_GRAY10_LE32:
      *chroma_format = "4:0:0";
      *bit_depth_luma = 10;
      *bit_depth_chroma = 0;
      break;
    case GST_VIDEO_FORMAT_NV12_10LE32:
      *chroma_format = "4:2:0";
      *bit_depth_luma = *bit_depth_chroma = 10;
      break;
    case GST_VIDEO_FORMAT_NV16_10LE32:
      *chroma_format = "4:2:2";
      *bit_depth_luma = *bit_depth_chroma = 10;
      break;
#endif
    default:
      return FALSE;
  }

  return TRUE;
}

static GstCaps *
get_output_caps (GstOMXVideoEnc * self)
{
  GstOMXVideoEncClass *klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);
  GstCaps *caps;
  const gchar *chroma_format;
  guint bit_depth_luma, bit_depth_chroma;

  caps = klass->get_caps (self, self->enc_out_port, self->input_state);

  /* Add chroma info about the encoded stream inferred from the format of the input */
  if (get_chroma_info_from_input (self, &chroma_format, &bit_depth_luma,
          &bit_depth_chroma)) {
    GST_DEBUG_OBJECT (self,
        "adding chroma info to output caps: %s (luma %d bits) (chroma %d bits)",
        chroma_format, bit_depth_luma, bit_depth_chroma);

    gst_caps_set_simple (caps, "chroma-format", G_TYPE_STRING, chroma_format,
        "bit-depth-luma", G_TYPE_UINT, bit_depth_luma,
        "bit-depth-chroma", G_TYPE_UINT, bit_depth_chroma, NULL);
  }

  return caps;
}

static GstFlowReturn
gst_omx_video_enc_handle_output_frame (GstOMXVideoEnc * self, GstOMXPort * port,
    GstOMXBuffer * buf, GstVideoCodecFrame * frame)
{
  GstOMXVideoEncClass *klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);
  GstFlowReturn flow_ret = GST_FLOW_OK;

  if ((buf->omx_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
      && buf->omx_buf->nFilledLen > 0) {
    GstVideoCodecState *state;
    GstBuffer *codec_data;
    GstMapInfo map = GST_MAP_INFO_INIT;
    GstCaps *caps;

    GST_DEBUG_OBJECT (self, "Handling codec data");

    caps = get_output_caps (self);
    codec_data = gst_buffer_new_and_alloc (buf->omx_buf->nFilledLen);

    gst_buffer_map (codec_data, &map, GST_MAP_WRITE);
    memcpy (map.data,
        buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
        buf->omx_buf->nFilledLen);
    gst_buffer_unmap (codec_data, &map);
    state =
        gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (self), caps,
        self->input_state);
    state->codec_data = codec_data;
    gst_video_codec_state_unref (state);
    if (!gst_video_encoder_negotiate (GST_VIDEO_ENCODER (self))) {
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_NOT_NEGOTIATED;
    }
    flow_ret = GST_FLOW_OK;
  } else if (buf->omx_buf->nFilledLen > 0) {
    GstBuffer *outbuf;
    GstMapInfo map = GST_MAP_INFO_INIT;

    GST_DEBUG_OBJECT (self, "Handling output data");

    if (buf->omx_buf->nFilledLen > 0) {
      outbuf = gst_buffer_new_and_alloc (buf->omx_buf->nFilledLen);

      gst_buffer_map (outbuf, &map, GST_MAP_WRITE);

      /* FIXME: Instead of copying GstOMXbuffer into GstBuffer, Lets just assign pointer
         of GstOMXBuffer to GstBuffer, Need support of GstBufferPool, Ref implementation is
         there in gstomxvideodec.c */

      memcpy (map.data,
          buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
          buf->omx_buf->nFilledLen);

      gst_buffer_unmap (outbuf, &map);
    } else {
      outbuf = gst_buffer_new ();
    }

    GST_BUFFER_TIMESTAMP (outbuf) =
        gst_util_uint64_scale (buf->omx_buf->nTimeStamp, GST_SECOND,
        OMX_TICKS_PER_SECOND);
    if (buf->omx_buf->nTickCount != 0)
      GST_BUFFER_DURATION (outbuf) =
          gst_util_uint64_scale (buf->omx_buf->nTickCount, GST_SECOND,
          OMX_TICKS_PER_SECOND);

    /* Call gst_video_encoder_finish_frame() when we receive the end of the
     * frame not for subframes, so we don't mess with timestamps. */
    if (buf->omx_buf->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
      if (!frame)
        GST_ERROR_OBJECT (self, "No corresponding frame found");
    } else {
      /* Subframe, by-pass encoder bass class as it's now aware of subframes.
       * Also manually set the PTS/DTS from the input frame (more precise than the
       * OMX ts) as it won't be done by finish_frame() */
      GST_BUFFER_PTS (outbuf) = frame->pts;
      GST_BUFFER_DTS (outbuf) = frame->dts;

      gst_video_codec_frame_unref (frame);
      frame = NULL;

      GST_BUFFER_FLAG_SET (outbuf, GST_OMX_BUFFER_FLAG_SUBFRAME);
    }

    if ((klass->cdata.hacks & GST_OMX_HACK_SYNCFRAME_FLAG_NOT_USED)
        || (buf->omx_buf->nFlags & OMX_BUFFERFLAG_SYNCFRAME)) {
      if (frame)
        GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
      else
        GST_BUFFER_FLAG_UNSET (outbuf, GST_BUFFER_FLAG_DELTA_UNIT);
    } else {
      if (frame)
        GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);
      else
        GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DELTA_UNIT);
    }

    if (frame) {
      frame->output_buffer = outbuf;
      flow_ret =
          gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (self), frame);
    } else {
      flow_ret = gst_pad_push (GST_VIDEO_ENCODER_SRC_PAD (self), outbuf);
    }
  } else if (frame != NULL) {
    flow_ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (self), frame);
  }

  return flow_ret;
}

static gboolean
gst_omx_video_enc_allocate_in_buffers (GstOMXVideoEnc * self)
{
  if (!gst_omx_port_ensure_buffer_count_actual (self->enc_in_port))
    return FALSE;

  if (gst_omx_port_allocate_buffers (self->enc_in_port) != OMX_ErrorNone)
    return FALSE;

  return TRUE;
}

static gboolean
gst_omx_video_enc_allocate_out_buffers (GstOMXVideoEnc * self)
{
  if (!gst_omx_port_ensure_buffer_count_actual (self->enc_out_port))
    return FALSE;

  if (gst_omx_port_allocate_buffers (self->enc_out_port) != OMX_ErrorNone)
    return FALSE;

  return TRUE;
}

static void
gst_omx_video_enc_loop (GstOMXVideoEnc * self)
{
  GstOMXVideoEncClass *klass;
  GstOMXPort *port = self->enc_out_port;
  GstOMXBuffer *buf = NULL;
  GstVideoCodecFrame *frame;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstOMXAcquireBufferReturn acq_return;
  OMX_ERRORTYPE err;

  klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);

  acq_return = gst_omx_port_acquire_buffer (port, &buf);
  if (acq_return == GST_OMX_ACQUIRE_BUFFER_ERROR) {
    goto component_error;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
    goto flushing;
  } else if (acq_return == GST_OMX_ACQUIRE_BUFFER_EOS) {
    goto eos;
  }

  if (!gst_pad_has_current_caps (GST_VIDEO_ENCODER_SRC_PAD (self))
      || acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
    GstCaps *caps;
    GstVideoCodecState *state;

    GST_DEBUG_OBJECT (self, "Port settings have changed, updating caps");

    if (acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE
        && gst_omx_port_is_enabled (port)) {
      /* Reallocate all buffers */
      err = gst_omx_port_set_enabled (port, FALSE);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_wait_buffers_released (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_deallocate_buffers (port);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_wait_enabled (port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
    }

    GST_VIDEO_ENCODER_STREAM_LOCK (self);

    caps = get_output_caps (self);
    if (!caps) {
      if (buf)
        gst_omx_port_release_buffer (self->enc_out_port, buf);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
      goto caps_failed;
    }

    GST_DEBUG_OBJECT (self, "Setting output state: %" GST_PTR_FORMAT, caps);

    state =
        gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (self), caps,
        self->input_state);
    gst_video_codec_state_unref (state);

    if (!gst_video_encoder_negotiate (GST_VIDEO_ENCODER (self))) {
      if (buf)
        gst_omx_port_release_buffer (self->enc_out_port, buf);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
      goto caps_failed;
    }

    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

    if (acq_return == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      if (!gst_omx_video_enc_allocate_out_buffers (self))
        goto reconfigure_error;

      err = gst_omx_port_wait_enabled (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_populate (port);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;

      err = gst_omx_port_mark_reconfigured (port);
      if (err != OMX_ErrorNone)
        goto reconfigure_error;
    }

    /* Now get a buffer */
    if (acq_return != GST_OMX_ACQUIRE_BUFFER_OK) {
      return;
    }
  }

  g_assert (acq_return == GST_OMX_ACQUIRE_BUFFER_OK);

  /* This prevents a deadlock between the srcpad stream
   * lock and the videocodec stream lock, if ::flush()
   * is called at the wrong time
   */
  if (gst_omx_port_is_flushing (self->enc_out_port)) {
    GST_DEBUG_OBJECT (self, "Flushing");
    gst_omx_port_release_buffer (self->enc_out_port, buf);
    goto flushing;
  }

  GST_DEBUG_OBJECT (self, "Handling buffer: 0x%08x %" G_GUINT64_FORMAT,
      (guint) buf->omx_buf->nFlags, (guint64) buf->omx_buf->nTimeStamp);

  GST_VIDEO_ENCODER_STREAM_LOCK (self);
  frame = gst_omx_video_find_nearest_frame (buf,
      gst_video_encoder_get_frames (GST_VIDEO_ENCODER (self)));

  g_assert (klass->handle_output_frame);
  flow_ret = klass->handle_output_frame (self, self->enc_out_port, buf, frame);

  GST_DEBUG_OBJECT (self, "Finished frame: %s", gst_flow_get_name (flow_ret));

  err = gst_omx_port_release_buffer (port, buf);
  if (err != OMX_ErrorNone)
    goto release_error;

  self->downstream_flow_ret = flow_ret;

  GST_DEBUG_OBJECT (self, "Read frame from component");

  if (flow_ret != GST_FLOW_OK)
    goto flow_error;

  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  return;

component_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->enc),
            gst_omx_component_get_last_error (self->enc)));
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    return;
  }
flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_FLUSHING;
    self->started = FALSE;
    return;
  }

eos:
  {
    g_mutex_lock (&self->drain_lock);
    if (self->draining) {
      GST_DEBUG_OBJECT (self, "Drained");
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      flow_ret = GST_FLOW_OK;
      gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    } else {
      GST_DEBUG_OBJECT (self, "Component signalled EOS");
      flow_ret = GST_FLOW_EOS;
    }
    g_mutex_unlock (&self->drain_lock);

    GST_VIDEO_ENCODER_STREAM_LOCK (self);
    self->downstream_flow_ret = flow_ret;

    /* Here we fallback and pause the task for the EOS case */
    if (flow_ret != GST_FLOW_OK)
      goto flow_error;

    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

    return;
  }
flow_error:
  {
    if (flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (self, "EOS");

      gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
      self->started = FALSE;
    } else if (flow_ret < GST_FLOW_EOS) {
      GST_ELEMENT_ERROR (self, STREAM, FAILED, ("Internal data stream error."),
          ("stream stopped, reason %s", gst_flow_get_name (flow_ret)));

      gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
      self->started = FALSE;
    } else if (flow_ret == GST_FLOW_FLUSHING) {
      GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
      gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
      self->started = FALSE;
    }
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    return;
  }
reconfigure_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Unable to reconfigure output port"));
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_NOT_NEGOTIATED;
    self->started = FALSE;
    return;
  }
caps_failed:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL), ("Failed to set caps"));
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_NOT_NEGOTIATED;
    self->started = FALSE;
    return;
  }
release_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase output buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    self->started = FALSE;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    return;
  }
}

static gboolean
gst_omx_video_enc_start (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self;

  self = GST_OMX_VIDEO_ENC (encoder);

  self->last_upstream_ts = 0;
  self->eos = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;

  return TRUE;
}

static gboolean
gst_omx_video_enc_stop (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self;

  self = GST_OMX_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Stopping encoder");

  gst_omx_port_set_flushing (self->enc_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->enc_out_port, 5 * GST_SECOND, TRUE);

  gst_pad_stop_task (GST_VIDEO_ENCODER_SRC_PAD (encoder));

  if (gst_omx_component_get_state (self->enc, 0) > OMX_StateIdle)
    gst_omx_component_set_state (self->enc, OMX_StateIdle);

  self->downstream_flow_ret = GST_FLOW_FLUSHING;
  self->started = FALSE;
  self->eos = FALSE;

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = NULL;

  g_mutex_lock (&self->drain_lock);
  self->draining = FALSE;
  g_cond_broadcast (&self->drain_cond);
  g_mutex_unlock (&self->drain_lock);

  gst_omx_component_get_state (self->enc, 5 * GST_SECOND);

  return TRUE;
}

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
static void
gst_omx_video_enc_set_latency (GstOMXVideoEnc * self)
{
  GstVideoCodecState *state = gst_video_codec_state_ref (self->input_state);
  GstVideoInfo *info = &state->info;
  GstClockTime latency;
  OMX_ALG_PARAM_REPORTED_LATENCY param;
  OMX_ERRORTYPE err;

  GST_OMX_INIT_STRUCT (&param);
  err = gst_omx_component_get_parameter (self->enc, OMX_ALG_IndexParamReportedLatency,
      &param);

  if (err != OMX_ErrorNone) {
    GST_WARNING_OBJECT (self, "Couldn't retrieve latency: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return -1;
  }

  GST_LOG_OBJECT (self, "retrieved latency of %d milisecond",
      param.nLatency);

  /* Convert millisecond into GstClockTime */
  latency = param.nLatency * GST_MSECOND;

  GST_INFO_OBJECT (self,
      "Updating latency to %" GST_TIME_FORMAT ,GST_TIME_ARGS (latency));

  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (self), latency, latency);

out:
  gst_video_codec_state_unref (state);
}
#endif

static gboolean
gst_omx_video_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstOMXVideoEnc *self;
  GstOMXVideoEncClass *klass;
  gboolean needs_disable = FALSE;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  GstVideoInfo *info = &state->info;
  GList *negotiation_map = NULL, *l;
  GstMapInfo map_info;
  GstBuffer *mem = NULL;
  gint i;

  self = GST_OMX_VIDEO_ENC (encoder);
  klass = GST_OMX_VIDEO_ENC_GET_CLASS (encoder);

  GST_DEBUG_OBJECT (self, "Setting new format %s",
      gst_video_format_to_string (info->finfo->format));

  gst_omx_port_get_port_definition (self->enc_in_port, &port_def);

  needs_disable =
      gst_omx_component_get_state (self->enc,
      GST_CLOCK_TIME_NONE) != OMX_StateLoaded;
  /* If the component is not in Loaded state and a real format change happens
   * we have to disable the port and re-allocate all buffers. If no real
   * format change happened we can just exit here.
   */
  if (needs_disable) {
    GST_DEBUG_OBJECT (self, "Need to disable and drain encoder");
    gst_omx_video_enc_drain (self, FALSE);
    gst_omx_port_set_flushing (self->enc_out_port, 5 * GST_SECOND, TRUE);

    /* Wait until the srcpad loop is finished,
     * unlock GST_VIDEO_ENCODER_STREAM_LOCK to prevent deadlocks
     * caused by using this lock from inside the loop function */
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    gst_pad_stop_task (GST_VIDEO_ENCODER_SRC_PAD (encoder));
    GST_VIDEO_ENCODER_STREAM_LOCK (self);

    if (gst_omx_port_set_enabled (self->enc_in_port, FALSE) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_set_enabled (self->enc_out_port, FALSE) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_buffers_released (self->enc_in_port,
            5 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_buffers_released (self->enc_out_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_deallocate_buffers (self->enc_in_port) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_deallocate_buffers (self->enc_out_port) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_enabled (self->enc_in_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_wait_enabled (self->enc_out_port,
            1 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;

    GST_DEBUG_OBJECT (self, "Encoder drained and disabled");
  }

  negotiation_map =
      gst_omx_video_get_supported_colorformats (self->enc_in_port,
      self->input_state);
  if (!negotiation_map) {
    /* Fallback */
    switch (info->finfo->format) {
      case GST_VIDEO_FORMAT_I420:
        port_def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
        break;
      case GST_VIDEO_FORMAT_NV12:
        port_def.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        break;
      default:
        GST_ERROR_OBJECT (self, "Unsupported format %s",
            gst_video_format_to_string (info->finfo->format));
        return FALSE;
        break;
    }
  } else {
    for (l = negotiation_map; l; l = l->next) {
      GstOMXVideoNegotiationMap *m = l->data;

      if (m->format == info->finfo->format) {
        port_def.format.video.eColorFormat = m->type;
        break;
      }
    }
    g_list_free_full (negotiation_map,
        (GDestroyNotify) gst_omx_video_negotiation_map_free);
  }

  port_def.format.video.nFrameWidth = info->width;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  if (self->stride) {
    port_def.nBufferAlignment = self->stride;
  }
#endif
  if (port_def.nBufferAlignment)
    port_def.format.video.nStride =
        (info->width + port_def.nBufferAlignment - 1) &
        (~(port_def.nBufferAlignment - 1));
  else
    port_def.format.video.nStride = GST_ROUND_UP_4 (info->width);       /* safe (?) default */

  port_def.format.video.nFrameHeight = info->height;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  if (self->sliceHeight) {
    port_def.format.video.nSliceHeight =
        (info->height + self->sliceHeight - 1) &
        (~(self->sliceHeight - 1));
  } else {
    port_def.format.video.nSliceHeight = info->height;
  }
#else
    port_def.format.video.nSliceHeight = info->height;
#endif

  switch (port_def.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420PackedPlanar:
      port_def.nBufferSize =
          (port_def.format.video.nStride * port_def.format.video.nFrameHeight) +
          2 * ((port_def.format.video.nStride / 2) *
          ((port_def.format.video.nFrameHeight + 1) / 2));
      break;

    case OMX_COLOR_FormatYUV420SemiPlanar:
      port_def.nBufferSize =
          (port_def.format.video.nStride * port_def.format.video.nFrameHeight) +
          (port_def.format.video.nStride *
          ((port_def.format.video.nFrameHeight + 1) / 2));
      break;

    default:
      g_assert_not_reached ();
  }

  if (info->fps_n == 0) {
    port_def.format.video.xFramerate = 0;
  } else {
    if (!(klass->cdata.hacks & GST_OMX_HACK_VIDEO_FRAMERATE_INTEGER))
      port_def.format.video.xFramerate = (info->fps_n << 16) / (info->fps_d);
    else
      port_def.format.video.xFramerate = (info->fps_n) / (info->fps_d);
  }

  GST_DEBUG_OBJECT (self, "Setting inport port definition");
  if (gst_omx_port_update_port_definition (self->enc_in_port,
          &port_def) != OMX_ErrorNone)
    return FALSE;

  if (klass->set_format) {
    if (!klass->set_format (self, self->enc_in_port, state)) {
      GST_ERROR_OBJECT (self, "Subclass failed to set the new format");
      return FALSE;
    }
  }

  GST_DEBUG_OBJECT (self, "Updating outport port definition");
  if (gst_omx_port_update_port_definition (self->enc_out_port,
          NULL) != OMX_ErrorNone)
    return FALSE;

  if (self->target_bitrate != 0xffffffff) {
    OMX_VIDEO_PARAM_BITRATETYPE config;
    OMX_ERRORTYPE err;

    GST_OMX_INIT_STRUCT (&config);
    config.nPortIndex = self->enc_out_port->index;
    config.nTargetBitrate = self->target_bitrate;
    config.eControlRate = self->control_rate;
    err = gst_omx_component_set_parameter (self->enc,
        OMX_IndexParamVideoBitrate, &config);
    if (err != OMX_ErrorNone)
      GST_ERROR_OBJECT (self, "Failed to set bitrate parameter: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
  }

  GST_DEBUG_OBJECT (self, "Enabling component");
  if (needs_disable) {
    if (gst_omx_port_set_enabled (self->enc_in_port, TRUE) != OMX_ErrorNone)
      return FALSE;
    if (!gst_omx_video_enc_allocate_in_buffers (self))
      return FALSE;

    if ((klass->cdata.hacks & GST_OMX_HACK_NO_DISABLE_OUTPORT)) {
      if (gst_omx_port_set_enabled (self->enc_out_port, TRUE) != OMX_ErrorNone)
        return FALSE;
      if (!gst_omx_video_enc_allocate_out_buffers (self))
        return FALSE;

      if (gst_omx_port_wait_enabled (self->enc_out_port,
              5 * GST_SECOND) != OMX_ErrorNone)
        return FALSE;
    }

    if (gst_omx_port_wait_enabled (self->enc_in_port,
            5 * GST_SECOND) != OMX_ErrorNone)
      return FALSE;
    if (gst_omx_port_mark_reconfigured (self->enc_in_port) != OMX_ErrorNone)
      return FALSE;
  } else {
    if (!(klass->cdata.hacks & GST_OMX_HACK_NO_DISABLE_OUTPORT)) {
      /* Disable output port */
      if (gst_omx_port_set_enabled (self->enc_out_port, FALSE) != OMX_ErrorNone)
        return FALSE;

      if (gst_omx_port_wait_enabled (self->enc_out_port,
              1 * GST_SECOND) != OMX_ErrorNone)
        return FALSE;

      if (gst_omx_component_set_state (self->enc,
              OMX_StateIdle) != OMX_ErrorNone)
        return FALSE;

      /* Need to allocate buffers to reach Idle state */
      if (!gst_omx_video_enc_allocate_in_buffers (self))
        return FALSE;
    } else {
      if (gst_omx_component_set_state (self->enc,
              OMX_StateIdle) != OMX_ErrorNone)
        return FALSE;

      /* Need to allocate buffers to reach Idle state */
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
      if (self->input_mode == OMX_Enc_InputMode_DefaultImplementation) {
        if (!gst_omx_video_enc_allocate_in_buffers (self))
          return FALSE;
      }

      /* Moving from OMX_AllocateBuffers to  OMX_UseBuffers,
         Idea is to give address of GstBuffer's data received from previos element in pipeline
         to GstOMXBuffer's data pointer. But at _set_format stage we do not have those GstBuffer
         So below is hack of dummpy memory for OMX component initializing */
      if (self->input_mode == OMX_Enc_InputMode_ZeroCopy) {
        mem = gst_buffer_new_allocate (NULL, 1024, NULL);
        gst_buffer_map (mem, &map_info, GST_MAP_READ);

        gst_omx_port_update_port_definition (self->enc_in_port, NULL);

        for (i = 0; i < self->enc_in_port->port_def.nBufferCountActual; i++)
          self->buffer_list = g_list_append (self->buffer_list, map_info.data);

        if (gst_omx_port_use_buffers (self->enc_in_port,
                self->buffer_list) != OMX_ErrorNone)
          return FALSE;

        gst_buffer_unmap (mem, &map_info);
        gst_buffer_unref (mem);
        g_list_free (self->buffer_list);
      }
#else 
      if (!gst_omx_video_enc_allocate_in_buffers (self))
        return FALSE;
#endif

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
      /* HACK: For now, ensuring we have at least 4 output buffers for encoder output port
	 but proper fix is to add extra buffers depending upon peer element */
      self->enc_out_port->port_def.nBufferCountActual = MAX(self->enc_out_port->port_def.nBufferCountActual,4);
      gst_omx_port_update_port_definition (self->enc_out_port,
		      &self->enc_out_port->port_def);

      if (!gst_omx_video_enc_allocate_out_buffers (self))
	 return FALSE;
#endif
    }

/* When OMX component is asked to switch from Loaded->Idle,
ZYNQ_USCALE_PLUS omx implementation does not return this call untill 
its input & output  port buffers get allocated. So let's skip this check here*/   
#ifndef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    if (gst_omx_component_get_state (self->enc,
            GST_CLOCK_TIME_NONE) != OMX_StateIdle)
      return FALSE;
#endif

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    if ((self->input_mode == OMX_Enc_InputMode_ZeroCopy) ||
        (self->input_mode == OMX_Enc_InputMode_DefaultImplementation)) {
      if (gst_omx_component_set_state (self->enc,
              OMX_StateExecuting) != OMX_ErrorNone)
        return FALSE;

      if (gst_omx_component_get_state (self->enc,
              GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
        return FALSE;
    }
#else
    if (gst_omx_component_set_state (self->enc,
            OMX_StateExecuting) != OMX_ErrorNone)
      return FALSE;

    if (gst_omx_component_get_state (self->enc,
            GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
      return FALSE;
#endif

  }

  /* Unset flushing to allow ports to accept data again */
  gst_omx_port_set_flushing (self->enc_in_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_set_flushing (self->enc_out_port, 5 * GST_SECOND, FALSE);

  if (gst_omx_component_get_last_error (self->enc) != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Component in error state: %s (0x%08x)",
        gst_omx_component_get_last_error_string (self->enc),
        gst_omx_component_get_last_error (self->enc));
    return FALSE;
  }

  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = gst_video_codec_state_ref (state);

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  if ((self->input_mode == OMX_Enc_InputMode_ZeroCopy) ||
      (self->input_mode == OMX_Enc_InputMode_DefaultImplementation)) {
    /* HACK: this should be done as part of the normal code flow (see #1340) */
    gst_omx_port_populate (self->enc_out_port);

    /* Start the srcpad loop again */
    GST_DEBUG_OBJECT (self, "Starting task again");
    self->downstream_flow_ret = GST_FLOW_OK;
    gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
        (GstTaskFunction) gst_omx_video_enc_loop, encoder, NULL);
  }
#else
    /* Start the srcpad loop again */
    GST_DEBUG_OBJECT (self, "Starting task again");
    self->downstream_flow_ret = GST_FLOW_OK;
    gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
        (GstTaskFunction) gst_omx_video_enc_loop, encoder, NULL);
#endif

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  gst_omx_video_enc_set_latency (self);
#endif

  return TRUE;
}


static gboolean
gst_omx_video_enc_flush (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self;

  self = GST_OMX_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Flushing encoder");

  if (gst_omx_component_get_state (self->enc, 0) == OMX_StateLoaded)
    return TRUE;

  gst_omx_port_set_flushing (self->enc_in_port, 5 * GST_SECOND, TRUE);
  gst_omx_port_set_flushing (self->enc_out_port, 5 * GST_SECOND, TRUE);

  /* Wait until the srcpad loop is finished,
   * unlock GST_VIDEO_ENCODER_STREAM_LOCK to prevent deadlocks
   * caused by using this lock from inside the loop function */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  GST_PAD_STREAM_LOCK (GST_VIDEO_ENCODER_SRC_PAD (self));
  GST_PAD_STREAM_UNLOCK (GST_VIDEO_ENCODER_SRC_PAD (self));
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  gst_omx_port_set_flushing (self->enc_in_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_set_flushing (self->enc_out_port, 5 * GST_SECOND, FALSE);
  gst_omx_port_populate (self->enc_out_port);

  /* Start the srcpad loop again */
  self->last_upstream_ts = 0;
  self->eos = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
      (GstTaskFunction) gst_omx_video_enc_loop, encoder, NULL);

  return TRUE;
}

static gboolean
gst_omx_video_enc_fill_buffer (GstOMXVideoEnc * self, GstBuffer * inbuf,
    GstOMXBuffer * outbuf)
{
  GstVideoCodecState *state = gst_video_codec_state_ref (self->input_state);
  GstVideoInfo *info = &state->info;
  OMX_PARAM_PORTDEFINITIONTYPE *port_def = &self->enc_in_port->port_def;
  gboolean ret = FALSE;
  GstVideoFrame frame;

  if (info->width != port_def->format.video.nFrameWidth ||
      info->height != port_def->format.video.nFrameHeight) {
    GST_ERROR_OBJECT (self, "Width or height do not match");
    goto done;
  }

  /*Same strides and everything */
  if (gst_buffer_get_size (inbuf) ==
      outbuf->omx_buf->nAllocLen - outbuf->omx_buf->nOffset) {
    outbuf->omx_buf->nFilledLen = gst_buffer_get_size (inbuf);

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    if (self->input_mode == OMX_Enc_InputMode_DefaultImplementation) {
      gst_buffer_extract (inbuf, 0,
          outbuf->omx_buf->pBuffer + outbuf->omx_buf->nOffset,
          outbuf->omx_buf->nFilledLen);
    }

    if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
      GST_FIXME_OBJECT (self,
          "Stride matches,Decoder o/p FD is %d, Encoder I/P FD is %d\n",
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (inbuf, 0)),
          outbuf->omx_buf->pBuffer);
      gst_buffer_ref (inbuf);
      outbuf->input_buffer = inbuf;
    }

    if (self->input_mode == OMX_Enc_InputMode_ZeroCopy) {

      GstMapInfo map = GST_MAP_INFO_INIT;
      if (!gst_buffer_map (inbuf, &map, GST_MAP_READ)) {
        GST_ERROR_OBJECT (self, "Failed to map input buffer");
      }
      outbuf->omx_buf->pBuffer = map.data;
      gst_buffer_unmap (inbuf, &map);
      gst_buffer_ref (inbuf);
      outbuf->input_buffer = inbuf;
    }
#else
      gst_buffer_extract (inbuf, 0,
          outbuf->omx_buf->pBuffer + outbuf->omx_buf->nOffset,
          outbuf->omx_buf->nFilledLen);
#endif

    ret = TRUE;
    goto done;
  } else {
    /* Different strides */
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
      outbuf->omx_buf->nFilledLen = gst_buffer_get_size (inbuf);
      GST_FIXME_OBJECT (self,
          "Stride does not matches,Decoder o/p FD is %d, Encoder I/P FD is %d\n",
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (inbuf, 0)),
          outbuf->omx_buf->pBuffer);
      gst_buffer_ref (inbuf);
      outbuf->input_buffer = inbuf;
      ret = TRUE;
      goto done;
    }

    if (self->input_mode == OMX_Enc_InputMode_ZeroCopy) {
      GST_ERROR_OBJECT (self,
          "Enable stride=true in pipeline or This should not happen\n");
      ret = FALSE;
      goto done;
    }

    if (self->input_mode == OMX_Enc_InputMode_DefaultImplementation)
#endif 
    {
      switch (info->finfo->format) {
        case GST_VIDEO_FORMAT_I420:{
          gint i, j, height, width;
          guint8 *src, *dest;
          gint src_stride, dest_stride;

          outbuf->omx_buf->nFilledLen = 0;

          if (!gst_video_frame_map (&frame, info, inbuf, GST_MAP_READ)) {
            GST_ERROR_OBJECT (self, "Invalid input buffer size");
            ret = FALSE;
            break;
          }

          for (i = 0; i < 3; i++) {
            if (i == 0) {
              dest_stride = port_def->format.video.nStride;
              src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 0);

              /* XXX: Try this if no stride was set */
              if (dest_stride == 0)
                dest_stride = src_stride;
            } else {
              dest_stride = port_def->format.video.nStride / 2;
              src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 1);

              /* XXX: Try this if no stride was set */
              if (dest_stride == 0)
                dest_stride = src_stride;
            }

            dest = outbuf->omx_buf->pBuffer + outbuf->omx_buf->nOffset;
            if (i > 0)
              dest +=
                  port_def->format.video.nSliceHeight *
                  port_def->format.video.nStride;
            if (i == 2)
              dest +=
                  (port_def->format.video.nSliceHeight / 2) *
                  (port_def->format.video.nStride / 2);

            src = GST_VIDEO_FRAME_COMP_DATA (&frame, i);
            height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, i);
            width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, i);

            if (dest + dest_stride * height >
                outbuf->omx_buf->pBuffer + outbuf->omx_buf->nAllocLen) {
              gst_video_frame_unmap (&frame);
              GST_ERROR_OBJECT (self, "Invalid output buffer size");
              ret = FALSE;
              break;
            }

            for (j = 0; j < height; j++) {
              memcpy (dest, src, width);
              outbuf->omx_buf->nFilledLen += dest_stride;
              src += src_stride;
              dest += dest_stride;
            }
          }
          gst_video_frame_unmap (&frame);
          ret = TRUE;
          break;
        }
        case GST_VIDEO_FORMAT_NV12:{
          gint i, j, height, width;
          guint8 *src, *dest;
          gint src_stride, dest_stride;

          outbuf->omx_buf->nFilledLen = 0;

          if (!gst_video_frame_map (&frame, info, inbuf, GST_MAP_READ)) {
            GST_ERROR_OBJECT (self, "Invalid input buffer size");
            ret = FALSE;
            break;
          }

          for (i = 0; i < 2; i++) {
            if (i == 0) {
              dest_stride = port_def->format.video.nStride;
              src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 0);
              /* XXX: Try this if no stride was set */
              if (dest_stride == 0)
                dest_stride = src_stride;
            } else {
              dest_stride = port_def->format.video.nStride;
              src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&frame, 1);

              /* XXX: Try this if no stride was set */
              if (dest_stride == 0)
                dest_stride = src_stride;
            }

            dest = outbuf->omx_buf->pBuffer + outbuf->omx_buf->nOffset;
            if (i == 1)
              dest +=
                  port_def->format.video.nSliceHeight *
                  port_def->format.video.nStride;

            src = GST_VIDEO_FRAME_COMP_DATA (&frame, i);
            height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, i);
            width = GST_VIDEO_FRAME_COMP_WIDTH (&frame, i) * (i == 0 ? 1 : 2);

            if (dest + dest_stride * height >
                outbuf->omx_buf->pBuffer + outbuf->omx_buf->nAllocLen) {
              gst_video_frame_unmap (&frame);
              GST_ERROR_OBJECT (self, "Invalid output buffer size");
              ret = FALSE;
              break;
            }

            for (j = 0; j < height; j++) {
              memcpy (dest, src, width);
              outbuf->omx_buf->nFilledLen += dest_stride;
              src += src_stride;
              dest += dest_stride;
            }

          }
          gst_video_frame_unmap (&frame);
          ret = TRUE;
          break;
        }
        default:
          GST_ERROR_OBJECT (self, "Unsupported format");
          goto done;
          break;
      }
    }
  }
done:

  gst_video_codec_state_unref (state);

  return ret;
}

static GstFlowReturn
gst_omx_video_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstOMXAcquireBufferReturn acq_ret = GST_OMX_ACQUIRE_BUFFER_ERROR;
  GstOMXVideoEnc *self;
  GstOMXPort *port;
  GstOMXBuffer *buf;
  OMX_ERRORTYPE err;
  gint fd1 = 0, fd2 = 0;
  gint i;
  self = GST_OMX_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Handling frame");

  if (self->eos) {
    GST_WARNING_OBJECT (self, "Got frame after EOS");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_EOS;
  }

  if (self->downstream_flow_ret != GST_FLOW_OK) {
    gst_video_codec_frame_unref (frame);
    return self->downstream_flow_ret;
  }

  port = self->enc_in_port;


#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
    self->count++;
    if ((self->count == 1) && (self->bufpool_complete == 0)) {
      fd1 =
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (frame->input_buffer,
              0));
      GST_FIXME_OBJECT (self, "Got 1st fd %d\n", fd1);
      self->garray = g_list_append (self->garray, fd1);
      frame->output_buffer = NULL;
      gst_video_encoder_finish_frame (self, frame);
      return self->downstream_flow_ret;
    }
    if (self->bufpool_complete == 0) {
      fd1 =
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (frame->input_buffer,
              0));
      GST_FIXME_OBJECT (self, "Got next fd %d\n", fd1);
      if (g_list_find (self->garray, fd1) != NULL) {
        GST_FIXME_OBJECT (self, "Bufferpool found completed \n");
        self->bufpool_complete = 1;
      } else {
        self->garray = g_list_append (self->garray, fd1);
        frame->output_buffer = NULL;
        gst_video_encoder_finish_frame (self, frame);
        return self->downstream_flow_ret;
      }
    }
    if (self->bufpool_complete == 1) {

      if (self->count == 1) {
        GST_FIXME_OBJECT (self,
            "Bufferpool found by event, It has %d buffers \n", self->g_dmalist_count);
        self->bufpool_complete = 2;
        self->buffer_list = g_list_reverse (self->buffer_list);

        gst_omx_port_update_port_definition (self->enc_in_port, NULL);
        GST_FIXME_OBJECT (self, "OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);
        self->enc_in_port->port_def.nBufferCountActual = self->g_dmalist_count;
        gst_omx_port_update_port_definition (self->enc_in_port,
            &self->enc_in_port->port_def);
        GST_FIXME_OBJECT (self, "Updated OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);

      } else {

        GST_FIXME_OBJECT (self,
            "Bufferpool found by fd tracing, It has %d buffers \n",
            g_list_length (self->garray));
        self->bufpool_complete = 2;

        for (i = 0; i < g_list_length (self->garray); i++)
          self->buffer_list =
              g_list_append (self->buffer_list,
              GPOINTER_TO_INT (g_list_nth_data (self->garray, i)));

        gst_omx_port_update_port_definition (self->enc_in_port, NULL);
        GST_FIXME_OBJECT (self, "OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);
        self->enc_in_port->port_def.nBufferCountActual = g_list_length (self->garray);
        gst_omx_port_update_port_definition (self->enc_in_port,
            &self->enc_in_port->port_def);
        GST_FIXME_OBJECT (self, "Updated OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);
      }

      if (gst_omx_port_use_buffers (self->enc_in_port,
              self->buffer_list) != OMX_ErrorNone)
        return FALSE;

      g_list_free (self->buffer_list);

      if (gst_omx_component_set_state (self->enc,
              OMX_StateExecuting) != OMX_ErrorNone)
        return FALSE;

      if (gst_omx_component_get_state (self->enc,
              GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
        return FALSE;

      /* HACK: this should be done as part of the normal code flow (see #1340) */
      gst_omx_port_populate (self->enc_out_port);

      /* Start the srcpad loop again */
      GST_DEBUG_OBJECT (self, "Starting task again");
      self->downstream_flow_ret = GST_FLOW_OK;
      gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
          (GstTaskFunction) gst_omx_video_enc_loop, self, NULL);
    }
  }
#endif

  while (acq_ret != GST_OMX_ACQUIRE_BUFFER_OK) {
    GstClockTime timestamp, duration;

    /* Make sure to release the base class stream lock, otherwise
     * _loop() can't call _finish_frame() and we might block forever
     * because no input buffers are released */
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
    if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
      acq_ret = gst_omx_port_acquire_buffer_dma (port, &buf,
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (frame->input_buffer,
                  0)));
    } else {
      acq_ret = gst_omx_port_acquire_buffer (port, &buf);
    }
#else
      acq_ret = gst_omx_port_acquire_buffer (port, &buf);
#endif

    if (acq_ret == GST_OMX_ACQUIRE_BUFFER_ERROR) {
      GST_VIDEO_ENCODER_STREAM_LOCK (self);
      goto component_error;
    } else if (acq_ret == GST_OMX_ACQUIRE_BUFFER_FLUSHING) {
      GST_VIDEO_ENCODER_STREAM_LOCK (self);
      goto flushing;
    } else if (acq_ret == GST_OMX_ACQUIRE_BUFFER_RECONFIGURE) {
      /* Reallocate all buffers */
      err = gst_omx_port_set_enabled (port, FALSE);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_buffers_released (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_deallocate_buffers (port);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_enabled (port, 1 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_set_enabled (port, TRUE);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      if (!gst_omx_video_enc_allocate_in_buffers (self)) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_wait_enabled (port, 5 * GST_SECOND);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      err = gst_omx_port_mark_reconfigured (port);
      if (err != OMX_ErrorNone) {
        GST_VIDEO_ENCODER_STREAM_LOCK (self);
        goto reconfigure_error;
      }

      /* Now get a new buffer and fill it */
      GST_VIDEO_ENCODER_STREAM_LOCK (self);
      continue;
    }
    GST_VIDEO_ENCODER_STREAM_LOCK (self);

    g_assert (acq_ret == GST_OMX_ACQUIRE_BUFFER_OK && buf != NULL);

    if (buf->omx_buf->nAllocLen - buf->omx_buf->nOffset <= 0) {
      gst_omx_port_release_buffer (port, buf);
      goto full_buffer;
    }

    if (self->downstream_flow_ret != GST_FLOW_OK) {
      gst_omx_port_release_buffer (port, buf);
      goto flow_error;
    }

    /* Now handle the frame */
    GST_DEBUG_OBJECT (self, "Handling frame");

    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
      OMX_CONFIG_INTRAREFRESHVOPTYPE config;

      GST_OMX_INIT_STRUCT (&config);
      config.nPortIndex = port->index;
      config.IntraRefreshVOP = OMX_TRUE;

      GST_DEBUG_OBJECT (self, "Forcing a keyframe");
      err =
          gst_omx_component_set_config (self->enc,
          OMX_IndexConfigVideoIntraVOPRefresh, &config);
      if (err != OMX_ErrorNone)
        GST_ERROR_OBJECT (self, "Failed to force a keyframe: %s (0x%08x)",
            gst_omx_error_to_string (err), err);
    }

    /* Copy the buffer content in chunks of size as requested
     * by the port */
    if (!gst_omx_video_enc_fill_buffer (self, frame->input_buffer, buf)) {
      gst_omx_port_release_buffer (port, buf);
      goto buffer_fill_error;
    }

    timestamp = frame->pts;
    if (timestamp != GST_CLOCK_TIME_NONE) {
      buf->omx_buf->nTimeStamp =
          gst_util_uint64_scale (timestamp, OMX_TICKS_PER_SECOND, GST_SECOND);
      self->last_upstream_ts = timestamp;
    }

    duration = frame->duration;
    if (duration != GST_CLOCK_TIME_NONE) {
      buf->omx_buf->nTickCount =
          gst_util_uint64_scale (duration, OMX_TICKS_PER_SECOND, GST_SECOND);
      self->last_upstream_ts += duration;
    } else {
      buf->omx_buf->nTickCount = 0;
    }

    self->started = TRUE;
    err = gst_omx_port_release_buffer (port, buf);
    if (err != OMX_ErrorNone)
      goto release_error;

    GST_DEBUG_OBJECT (self, "Passed frame to component");
  }

  gst_video_codec_frame_unref (frame);

  return self->downstream_flow_ret;

full_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("Got OpenMAX buffer with no free space (%p, %u/%u)", buf,
            (guint) buf->omx_buf->nOffset, (guint) buf->omx_buf->nAllocLen));
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

flow_error:
  {
    gst_video_codec_frame_unref (frame);
    return self->downstream_flow_ret;
  }

component_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("OpenMAX component in error state %s (0x%08x)",
            gst_omx_component_get_last_error_string (self->enc),
            gst_omx_component_get_last_error (self->enc)));
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- returning FLUSHING");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_FLUSHING;
  }
reconfigure_error:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Unable to reconfigure input port"));
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
buffer_fill_error:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE, (NULL),
        ("Failed to write input into the OpenMAX buffer"));
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
release_error:
  {
    gst_video_codec_frame_unref (frame);
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to relase input buffer to component: %s (0x%08x)",
            gst_omx_error_to_string (err), err));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_omx_video_enc_finish (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self;

  self = GST_OMX_VIDEO_ENC (encoder);

  return gst_omx_video_enc_drain (self, TRUE);
}

static GstFlowReturn
gst_omx_video_enc_drain (GstOMXVideoEnc * self, gboolean at_eos)
{
  GstOMXVideoEncClass *klass;
  GstOMXBuffer *buf;
  GstOMXAcquireBufferReturn acq_ret;
  OMX_ERRORTYPE err;

  GST_DEBUG_OBJECT (self, "Draining component");

  klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Component not started yet");
    return GST_FLOW_OK;
  }
  self->started = FALSE;

  /* Don't send EOS buffer twice, this doesn't work */
  if (self->eos) {
    GST_DEBUG_OBJECT (self, "Component is EOS already");
    return GST_FLOW_OK;
  }
  if (at_eos)
    self->eos = TRUE;

  if ((klass->cdata.hacks & GST_OMX_HACK_NO_EMPTY_EOS_BUFFER)) {
    GST_WARNING_OBJECT (self, "Component does not support empty EOS buffers");
    return GST_FLOW_OK;
  }

  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  /* Send an EOS buffer to the component and let the base
   * class drop the EOS event. We will send it later when
   * the EOS buffer arrives on the output port. */
  acq_ret = gst_omx_port_acquire_buffer (self->enc_in_port, &buf);
  if (acq_ret != GST_OMX_ACQUIRE_BUFFER_OK) {
    GST_VIDEO_ENCODER_STREAM_LOCK (self);
    GST_ERROR_OBJECT (self, "Failed to acquire buffer for draining: %d",
        acq_ret);
    return GST_FLOW_ERROR;
  }

  g_mutex_lock (&self->drain_lock);
  self->draining = TRUE;
  buf->omx_buf->nFilledLen = 0;
  buf->omx_buf->nTimeStamp =
      gst_util_uint64_scale (self->last_upstream_ts, OMX_TICKS_PER_SECOND,
      GST_SECOND);
  buf->omx_buf->nTickCount = 0;
  buf->omx_buf->nFlags |= OMX_BUFFERFLAG_EOS;
  err = gst_omx_port_release_buffer (self->enc_in_port, buf);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self, "Failed to drain component: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    GST_VIDEO_ENCODER_STREAM_LOCK (self);
    return GST_FLOW_ERROR;
  }
  GST_DEBUG_OBJECT (self, "Waiting until component is drained");
  g_cond_wait (&self->drain_cond, &self->drain_lock);
  GST_DEBUG_OBJECT (self, "Drained component");
  g_mutex_unlock (&self->drain_lock);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  self->started = FALSE;

  return GST_FLOW_OK;
}

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
static gboolean *
gst_omx_video_enc_sink_event (GstVideoEncoder * encoder, GstEvent * event)
{
  GstOMXVideoEnc *self;

  self = GST_OMX_VIDEO_ENC (encoder);
  GstOMXVideoEncClass *klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  gint value;
  gint i = 0;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:{

      if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {

        const GstStructure *structure = gst_event_get_structure (event);
        GArray *buffers;

        if (gst_structure_has_name (structure, "dmaStruct")) {
          gst_structure_get (structure, "dmaPtrArray", G_TYPE_ARRAY, &buffers,
              NULL);

          for (i = 0; i < buffers->len; i++) {
            self->buffer_list =
                g_list_append (self->buffer_list, g_array_index (buffers, gint, i));
            self->g_dmalist_count++;
          }
          self->bufpool_complete = 1;
	  g_array_unref(buffers);
          GST_FIXME_OBJECT (self,
              "Custom event for DMA list is sucessful, got %d fd \n",
              self->g_dmalist_count);
        }
      }
      return sink_event_backup (encoder, event);
      break;
    }
    default:
      return sink_event_backup (encoder, event);
      break;
  }

  return 0;
}
#endif

static gboolean
gst_omx_video_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query)
{
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (encoder);
  guint num_buffers;
  gsize size = self->input_state->info.size;
#endif

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  num_buffers = self->enc_in_port->port_def.nBufferCountMin + 1;
  GST_DEBUG_OBJECT (self,
      "request at least %d buffers of size %" G_GSIZE_FORMAT, num_buffers,
      size);
  gst_query_add_allocation_pool (query, NULL, size, num_buffers, 0);
#endif

  return
      GST_VIDEO_ENCODER_CLASS
      (gst_omx_video_enc_parent_class)->propose_allocation (encoder, query);
}

static GstCaps *
gst_omx_video_enc_getcaps (GstVideoEncoder * encoder, GstCaps * filter)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (encoder);
  GList *negotiation_map = NULL;
  GstCaps *comp_supported_caps;

  if (!self->enc)
    return gst_video_encoder_proxy_getcaps (encoder, NULL, filter);

  negotiation_map =
      gst_omx_video_get_supported_colorformats (self->enc_in_port,
      self->input_state);
  comp_supported_caps = gst_omx_video_get_caps_for_map (negotiation_map);
  g_list_free_full (negotiation_map,
      (GDestroyNotify) gst_omx_video_negotiation_map_free);

  if (!gst_caps_is_empty (comp_supported_caps)) {
    GstCaps *ret =
        gst_video_encoder_proxy_getcaps (encoder, comp_supported_caps, filter);

    gst_caps_unref (comp_supported_caps);
    return ret;
  } else {
    gst_caps_unref (comp_supported_caps);
    return gst_video_encoder_proxy_getcaps (encoder, NULL, filter);
  }
}
