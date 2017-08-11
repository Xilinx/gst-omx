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

#include "gstomxh264enc.h"
#include "gstomxh264utils.h"

#ifdef USE_OMX_TARGET_RPI
#include <OMX_Broadcom.h>
#include <OMX_Index.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_omx_h264_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_h264_enc_debug_category

/* prototypes */
static gboolean gst_omx_h264_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_h264_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstFlowReturn gst_omx_h264_enc_handle_output_frame (GstOMXVideoEnc *
    self, GstOMXPort * port, GstOMXBuffer * buf, GstVideoCodecFrame * frame);
static gboolean gst_omx_h264_enc_flush (GstVideoEncoder * enc);
static gboolean gst_omx_h264_enc_stop (GstVideoEncoder * enc);
static void gst_omx_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_omx_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

enum
{
  PROP_0,
#ifdef USE_OMX_TARGET_RPI
  PROP_INLINESPSPPSHEADERS,
#endif
  PROP_PERIODICITYOFIDRFRAMES,
  PROP_INTERVALOFCODINGINTRAFRAMES,
  PROP_GOP_LENGTH,
  PROP_B_FRAMES,
  PROP_ENTROPY_MODE,
  PROP_INTRA_PREDICTION,
  PROP_LOOP_FILTER
};

#ifdef USE_OMX_TARGET_RPI
#define GST_OMX_H264_VIDEO_ENC_INLINE_SPS_PPS_HEADERS_DEFAULT      TRUE
#endif
#define GST_OMX_H264_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT    (0xffffffff)
#define GST_OMX_H264_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_H264_ENC_GOP_LENGTH_DEFAULT (30)
#define GST_OMX_H264_ENC_B_FRAMES_DEFAULT (0)
#define GST_OMX_H264_ENC_ENTROPY_MODE_DEFAULT (0xffffffff)
#define GST_OMX_H264_ENC_INTRA_PREDICTION_DEFAULT (TRUE)
#define GST_OMX_H264_ENC_LOOP_FILTER_DEFAULT (0xffffffff)

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h264_enc_debug_category, "omxh264enc", 0, \
      "debug category for gst-omx video encoder base class");

#define parent_class gst_omx_h264_enc_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOMXH264Enc, gst_omx_h264_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);


#define GST_TYPE_OMX_H264_ENC_ENTROPY_MODE (gst_omx_h264_enc_entropy_mode_get_type ())
static GType
gst_omx_h264_enc_entropy_mode_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {FALSE, "CAVLC entropy mode","CAVLC"},
      {TRUE,"CABAC entropy mode","CABAC"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXH264EncEntropyMode", values);
  }
  return qtype;
}

#define GST_TYPE_OMX_H264_ENC_LOOP_FILTER (gst_omx_h264_enc_loop_filter_get_type ())
static GType
gst_omx_h264_enc_loop_filter_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_VIDEO_AVCLoopFilterEnable, "Enable deblocking filter","Enable"},
      {OMX_VIDEO_AVCLoopFilterDisable,"Disable deblocking filter","Disable"},
      {OMX_VIDEO_AVCLoopFilterDisableSliceBoundary,"Disable slice boundary in deblocking filter","DisableSliceBoundary"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXH264EncLoopFilter", values);
  }
  return qtype;
}



static gboolean
gst_omx_h264_enc_open (GstVideoEncoder * encoder)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (encoder);
  GstOMXVideoEnc *omx_enc = GST_OMX_VIDEO_ENC (encoder);
  guint32 p_frames;
  guint32 b_frames;
  OMX_VIDEO_PARAM_AVCTYPE avc_param;
  OMX_ERRORTYPE err;

  if (!GST_VIDEO_ENCODER_CLASS (parent_class)->open (encoder))
    return FALSE;

  GST_OMX_INIT_STRUCT (&avc_param);
  avc_param.nPortIndex = omx_enc->enc_out_port->index;

  err = gst_omx_component_get_parameter (omx_enc->enc,
      OMX_IndexParamVideoAvc, &avc_param);

  /* As per some historical reasons, GopLength=0 should be treated as GopLength=1 only*/
  if(!self->gop_length)
	self->gop_length = 1;

  /* We will set OMX il's nPframes & nBframes parameter as below calculation
     based on user's input of GopLength & NumBframes
     nPframes(Number of P frames between each I frame) = GopLength/(NumBframes+1) - 1
     nBframes(Number of B frames between each I frame) = GopLength - (nPframes+1)
     NOTE: We have one constraint that user must provide GopLength in multiple of NumBframes+1.
  */
  if(self->gop_length % (self->b_frames + 1)) {
        if(self->gop_length == 1) {
                GST_LOG_OBJECT (self, "GopLength is 1 so its only Intra. Setting b_frames as 0\n");
                self->b_frames = 0;
        } else {
                GST_ERROR_OBJECT (self, "GopLength should be in multiple of (b-frames + 1).Now setting it to default value");
		g_warning("GopLength should be in multiple of (b-frames + 1).Now setting it to default value");
                self->gop_length = GST_OMX_H264_ENC_GOP_LENGTH_DEFAULT;
                self->b_frames = GST_OMX_H264_ENC_B_FRAMES_DEFAULT;
        }
  }

  p_frames = ( (self->gop_length) / (self->b_frames + 1) )  - 1;
  b_frames = self->gop_length - (p_frames + 1);

  if (err == OMX_ErrorNone) {
    if (p_frames != avc_param.nPFrames) {
      GST_LOG_OBJECT (self, "Changing number of P-Frame to %d",
          p_frames);
      avc_param.nPFrames = p_frames;
    }
    if (b_frames != avc_param.nBFrames) {
      GST_LOG_OBJECT (self, "Changing number of B-Frame to %d",
          b_frames);
      avc_param.nBFrames = b_frames;
    }
    if (self->entropy_mode != GST_OMX_H264_ENC_ENTROPY_MODE_DEFAULT) {
	avc_param.bEntropyCodingCABAC = self->entropy_mode;	
    }	
    if (self->intra_pred != GST_OMX_H264_ENC_INTRA_PREDICTION_DEFAULT) {
	avc_param.bconstIpred = self->intra_pred;	
    }	
    if (self->loop_filter != GST_OMX_H264_ENC_LOOP_FILTER_DEFAULT) {
	avc_param.eLoopFilterMode = self->loop_filter;	
    }	
    err =
        gst_omx_component_set_parameter (omx_enc->enc,
        OMX_IndexParamVideoAvc, &avc_param);
    if (err == OMX_ErrorUnsupportedIndex) {
      GST_WARNING_OBJECT (self,
          "Setting AVC parameters not supported by the component");
    } else if (err == OMX_ErrorUnsupportedSetting) {
      GST_WARNING_OBJECT (self,
          "Setting AVC parameters %u %u not supported by the component",
          self->gop_length, self->b_frames);
    } else if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to set AVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  } else {
    GST_ERROR_OBJECT (self,
        "Failed to get AVC parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
  }

  return TRUE;
}

static void
gst_omx_h264_enc_class_init (GstOMXH264EncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *basevideoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h264_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_h264_enc_get_caps);

  gobject_class->set_property = gst_omx_h264_enc_set_property;
  gobject_class->get_property = gst_omx_h264_enc_get_property;

#ifdef USE_OMX_TARGET_RPI
  g_object_class_install_property (gobject_class, PROP_INLINESPSPPSHEADERS,
      g_param_spec_boolean ("inline-header",
          "Inline SPS/PPS headers before IDR",
          "Inline SPS/PPS header before IDR",
          GST_OMX_H264_VIDEO_ENC_INLINE_SPS_PPS_HEADERS_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
#endif

  g_object_class_install_property (gobject_class, PROP_PERIODICITYOFIDRFRAMES,
      g_param_spec_uint ("periodicty-idr", "Target Bitrate",
          "Periodicity of IDR frames (0xffffffff=component default)",
          0, G_MAXUINT,
          GST_OMX_H264_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class,
      PROP_INTERVALOFCODINGINTRAFRAMES,
      g_param_spec_uint ("interval-intraframes",
          "Interval of coding Intra frames",
          "Interval of coding Intra frames (0xffffffff=component default)", 0,
          G_MAXUINT,
          GST_OMX_H264_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_GOP_LENGTH,
      g_param_spec_uint ("Gop-Length", "Number of all frames in 1 GOP, Must be in multiple of (b-frames+1)",
          "Distance between two consecutive I frames(30=component default)",
          0, 1000, GST_OMX_H264_ENC_GOP_LENGTH_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_B_FRAMES,
      g_param_spec_uint ("b-frames", "Number of B-Frames",
          "Number of B-Frames between two consecutive P-frames(0=component default)",
          0, 4, GST_OMX_H264_ENC_B_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_ENTROPY_MODE,
      g_param_spec_enum ("entropy-mode", "Entropy Mode",
          "Specifies the entropy mode for encoding process",
          GST_TYPE_OMX_H264_ENC_ENTROPY_MODE,
          GST_OMX_H264_ENC_ENTROPY_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_INTRA_PREDICTION,
      g_param_spec_boolean ("intra-pred",
          "Constrained Intra Pred",
          "Enable(true)/Disables(false) constrained_intra_pred_flag syntax element",
          GST_OMX_H264_ENC_INTRA_PREDICTION_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
  
  g_object_class_install_property (gobject_class, PROP_LOOP_FILTER,
      g_param_spec_enum ("loop-filter", "Loop Filter mode",
          "enables/disables the deblocking filter",
          GST_TYPE_OMX_H264_ENC_LOOP_FILTER,
          GST_OMX_H264_ENC_LOOP_FILTER_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  basevideoenc_class->open = gst_omx_h264_enc_open;
  basevideoenc_class->flush = gst_omx_h264_enc_flush;
  basevideoenc_class->stop = gst_omx_h264_enc_stop;

  videoenc_class->cdata.default_src_template_caps = "video/x-h264, "
      "width=(int) [ 16, 4096 ], " "height=(int) [ 16, 4096 ]";
  videoenc_class->handle_output_frame =
      GST_DEBUG_FUNCPTR (gst_omx_h264_enc_handle_output_frame);

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.264 Video Encoder",
      "Codec/Encoder/Video",
      "Encode H.264 video streams",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.avc");
}

static void
gst_omx_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (object);

  switch (prop_id) {
#ifdef USE_OMX_TARGET_RPI
    case PROP_INLINESPSPPSHEADERS:
      self->inline_sps_pps_headers = g_value_get_boolean (value);
      break;
#endif
    case PROP_PERIODICITYOFIDRFRAMES:
      self->periodicty_idr = g_value_get_uint (value);
      break;
    case PROP_INTERVALOFCODINGINTRAFRAMES:
      self->interval_intraframes = g_value_get_uint (value);
      break;
    case PROP_GOP_LENGTH:
      self->gop_length = g_value_get_uint (value);
      break;
    case PROP_B_FRAMES:
      self->b_frames = g_value_get_uint (value);
      break;
    case PROP_ENTROPY_MODE:
      self->entropy_mode = g_value_get_enum (value);
      break;
    case PROP_INTRA_PREDICTION:
      self->intra_pred = g_value_get_boolean (value);
      break;
    case PROP_LOOP_FILTER:
      self->loop_filter = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_h264_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (object);

  switch (prop_id) {
#ifdef USE_OMX_TARGET_RPI
    case PROP_INLINESPSPPSHEADERS:
      g_value_set_boolean (value, self->inline_sps_pps_headers);
      break;
#endif
    case PROP_PERIODICITYOFIDRFRAMES:
      g_value_set_uint (value, self->periodicty_idr);
      break;
    case PROP_INTERVALOFCODINGINTRAFRAMES:
      g_value_set_uint (value, self->interval_intraframes);
      break;
    case PROP_GOP_LENGTH:
      g_value_set_uint (value, self->gop_length);
      break;
    case PROP_B_FRAMES:
      g_value_set_uint (value, self->b_frames);
      break;
    case PROP_ENTROPY_MODE:
      g_value_set_enum (value, self->entropy_mode);
      break;
    case PROP_INTRA_PREDICTION:
      g_value_set_boolean (value, self->intra_pred);
      break;
    case PROP_LOOP_FILTER:
      g_value_set_enum (value, self->loop_filter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_omx_h264_enc_init (GstOMXH264Enc * self)
{
#ifdef USE_OMX_TARGET_RPI
  self->inline_sps_pps_headers =
      GST_OMX_H264_VIDEO_ENC_INLINE_SPS_PPS_HEADERS_DEFAULT;
#endif
  self->periodicty_idr =
      GST_OMX_H264_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT;
  self->interval_intraframes =
      GST_OMX_H264_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT;
  self->gop_length = GST_OMX_H264_ENC_GOP_LENGTH_DEFAULT;
  self->b_frames = GST_OMX_H264_ENC_B_FRAMES_DEFAULT;
  self->entropy_mode = GST_OMX_H264_ENC_ENTROPY_MODE_DEFAULT;
  self->intra_pred = GST_OMX_H264_ENC_INTRA_PREDICTION_DEFAULT;
  self->loop_filter = GST_OMX_H264_ENC_LOOP_FILTER_DEFAULT;
}

static gboolean
gst_omx_h264_enc_flush (GstVideoEncoder * enc)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);

  g_list_free_full (self->headers, (GDestroyNotify) gst_buffer_unref);
  self->headers = NULL;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->flush (enc);
}

static gboolean
gst_omx_h264_enc_stop (GstVideoEncoder * enc)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);

  g_list_free_full (self->headers, (GDestroyNotify) gst_buffer_unref);
  self->headers = NULL;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->stop (enc);
}

static gboolean
gst_omx_h264_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);
  GstCaps *peercaps;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  OMX_VIDEO_CONFIG_AVCINTRAPERIOD config_avcintraperiod;
#ifdef USE_OMX_TARGET_RPI
  OMX_CONFIG_PORTBOOLEANTYPE config_inline_header;
#endif
  OMX_ERRORTYPE err;
  const gchar *profile_string, *level_string;

#ifdef USE_OMX_TARGET_RPI
  GST_OMX_INIT_STRUCT (&config_inline_header);
  config_inline_header.nPortIndex =
      GST_OMX_VIDEO_ENC (self)->enc_out_port->index;
  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &config_inline_header);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "can't get OMX_IndexParamBrcmVideoAVCInlineHeaderEnable %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  if (self->inline_sps_pps_headers) {
    config_inline_header.bEnabled = OMX_TRUE;
  } else {
    config_inline_header.bEnabled = OMX_FALSE;
  }

  err =
      gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &config_inline_header);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "can't set OMX_IndexParamBrcmVideoAVCInlineHeaderEnable %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  }
#endif

  if (self->periodicty_idr !=
      GST_OMX_H264_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT
      || self->interval_intraframes !=
      GST_OMX_H264_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT) {


    GST_OMX_INIT_STRUCT (&config_avcintraperiod);
    config_avcintraperiod.nPortIndex =
        GST_OMX_VIDEO_ENC (self)->enc_out_port->index;
    err =
        gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexConfigVideoAVCIntraPeriod, &config_avcintraperiod);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "can't get OMX_IndexConfigVideoAVCIntraPeriod %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    GST_DEBUG_OBJECT (self, "default nPFrames:%u, nIDRPeriod:%u",
        (guint) config_avcintraperiod.nPFrames,
        (guint) config_avcintraperiod.nIDRPeriod);

    if (self->periodicty_idr !=
        GST_OMX_H264_VIDEO_ENC_PERIODICITY_OF_IDR_FRAMES_DEFAULT) {
      config_avcintraperiod.nIDRPeriod = self->periodicty_idr;
    }

    if (self->interval_intraframes !=
        GST_OMX_H264_VIDEO_ENC_INTERVAL_OF_CODING_INTRA_FRAMES_DEFAULT) {
      config_avcintraperiod.nPFrames = self->interval_intraframes;
    }

    err =
        gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
        OMX_IndexConfigVideoAVCIntraPeriod, &config_avcintraperiod);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "can't set OMX_IndexConfigVideoAVCIntraPeriod %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  err =
      gst_omx_port_update_port_definition (GST_OMX_VIDEO_ENC
      (self)->enc_out_port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err != OMX_ErrorNone) {
    GST_WARNING_OBJECT (self,
        "Setting profile/level not supported by component");
    return TRUE;
  }

  peercaps = gst_pad_peer_query_caps (GST_VIDEO_ENCODER_SRC_PAD (enc),
      gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (enc)));
  if (peercaps) {
    GstStructure *s;

    if (gst_caps_is_empty (peercaps)) {
      gst_caps_unref (peercaps);
      GST_ERROR_OBJECT (self, "Empty caps");
      return FALSE;
    }

    s = gst_caps_get_structure (peercaps, 0);
    profile_string = gst_structure_get_string (s, "profile");
    if (profile_string) {
      param.eProfile = gst_omx_h264_utils_get_profile_from_str (profile_string);

      if (param.eProfile == OMX_VIDEO_AVCProfileMax)
        goto unsupported_profile;
    }
    level_string = gst_structure_get_string (s, "level");
    if (level_string) {
      param.eLevel = gst_omx_h264_utils_get_level_from_str (level_string);
      if (param.eLevel == OMX_VIDEO_AVCLevelMax)
        goto unsupported_level;
    }
    gst_caps_unref (peercaps);
  }

  err =
      gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err == OMX_ErrorUnsupportedIndex) {
    GST_WARNING_OBJECT (self,
        "Setting profile/level not supported by component");
  } else if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Error setting profile %u and level %u: %s (0x%08x)",
        (guint) param.eProfile, (guint) param.eLevel,
        gst_omx_error_to_string (err), err);
    return FALSE;
  }

  if( (param.eProfile == OMX_VIDEO_AVCProfileBaseline) && self->b_frames )
	g_warning("B frames are not supported in AVC baseline profile,Going with b-frames=0 \n");

  return TRUE;

unsupported_profile:
  GST_ERROR_OBJECT (self, "Unsupported profile %s", profile_string);
  gst_caps_unref (peercaps);
  return FALSE;

unsupported_level:
  GST_ERROR_OBJECT (self, "Unsupported level %s", level_string);
  gst_caps_unref (peercaps);
  return FALSE;
}

static GstCaps *
gst_omx_h264_enc_get_caps (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);
  GstCaps *caps;
  OMX_ERRORTYPE err;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  const gchar *profile, *level;

  caps = gst_caps_new_simple ("video/x-h264",
      "stream-format", G_TYPE_STRING, "byte-stream",
      "alignment", G_TYPE_STRING, "au", NULL);

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err != OMX_ErrorNone && err != OMX_ErrorUnsupportedIndex)
    return NULL;

  if (err == OMX_ErrorNone) {
    switch (param.eProfile) {
      case OMX_VIDEO_AVCProfileBaseline:
        profile = "baseline";
        break;
      case OMX_VIDEO_AVCProfileMain:
        profile = "main";
        break;
      case OMX_VIDEO_AVCProfileExtended:
        profile = "extended";
        break;
      case OMX_VIDEO_AVCProfileHigh:
        profile = "high";
        break;
      case OMX_VIDEO_AVCProfileHigh10:
        profile = "high-10";
        break;
      case OMX_VIDEO_AVCProfileHigh422:
        profile = "high-4:2:2";
        break;
      case OMX_VIDEO_AVCProfileHigh444:
        profile = "high-4:4:4";
        break;
      default:
        g_assert_not_reached ();
        return NULL;
    }

    switch (param.eLevel) {
      case OMX_VIDEO_AVCLevel1:
        level = "1";
        break;
      case OMX_VIDEO_AVCLevel1b:
        level = "1b";
        break;
      case OMX_VIDEO_AVCLevel11:
        level = "1.1";
        break;
      case OMX_VIDEO_AVCLevel12:
        level = "1.2";
        break;
      case OMX_VIDEO_AVCLevel13:
        level = "1.3";
        break;
      case OMX_VIDEO_AVCLevel2:
        level = "2";
        break;
      case OMX_VIDEO_AVCLevel21:
        level = "2.1";
        break;
      case OMX_VIDEO_AVCLevel22:
        level = "2.2";
        break;
      case OMX_VIDEO_AVCLevel3:
        level = "3";
        break;
      case OMX_VIDEO_AVCLevel31:
        level = "3.1";
        break;
      case OMX_VIDEO_AVCLevel32:
        level = "3.2";
        break;
      case OMX_VIDEO_AVCLevel4:
        level = "4";
        break;
      case OMX_VIDEO_AVCLevel41:
        level = "4.1";
        break;
      case OMX_VIDEO_AVCLevel42:
        level = "4.2";
        break;
      case OMX_VIDEO_AVCLevel5:
        level = "5";
        break;
      case OMX_VIDEO_AVCLevel51:
        level = "5.1";
        break;
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
      case OMX_ALG_VIDEO_AVCLevel52:
        level = "5.2";
        break;
      case OMX_ALG_VIDEO_AVCLevel60:
        level = "6.0";
        break;
      case OMX_ALG_VIDEO_AVCLevel61:
        level = "6.1";
        break;
      case OMX_ALG_VIDEO_AVCLevel62:
        level = "6.2";
        break;
#endif
      default:
        g_assert_not_reached ();
        return NULL;
    }
    gst_caps_set_simple (caps,
        "profile", G_TYPE_STRING, profile, "level", G_TYPE_STRING, level, NULL);
  }

  /* FIXME: Should we enable this using a hack flag ? */
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  gst_caps_set_simple (caps, "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
#endif

  return caps;
}

static GstFlowReturn
gst_omx_h264_enc_handle_output_frame (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstOMXBuffer * buf, GstVideoCodecFrame * frame)
{
  GstOMXH264Enc *self = GST_OMX_H264_ENC (enc);

  if (buf->omx_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    /* The codec data is SPS/PPS with a startcode => bytestream stream format
     * For bytestream stream format the SPS/PPS is only in-stream and not
     * in the caps!
     */
    if (buf->omx_buf->nFilledLen >= 4 &&
        GST_READ_UINT32_BE (buf->omx_buf->pBuffer +
            buf->omx_buf->nOffset) == 0x00000001) {
      GstBuffer *hdrs;
      GstMapInfo map = GST_MAP_INFO_INIT;

      GST_DEBUG_OBJECT (self, "got codecconfig in byte-stream format");

      hdrs = gst_buffer_new_and_alloc (buf->omx_buf->nFilledLen);

      gst_buffer_map (hdrs, &map, GST_MAP_WRITE);
      memcpy (map.data,
          buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
          buf->omx_buf->nFilledLen);
      gst_buffer_unmap (hdrs, &map);
      self->headers = g_list_append (self->headers, hdrs);

      if (frame)
        gst_video_codec_frame_unref (frame);

      return GST_FLOW_OK;
    }
  } else if (self->headers) {
    gst_video_encoder_set_headers (GST_VIDEO_ENCODER (self), self->headers);
    self->headers = NULL;
  }

  return
      GST_OMX_VIDEO_ENC_CLASS
      (gst_omx_h264_enc_parent_class)->handle_output_frame (enc, port, buf,
      frame);
}
