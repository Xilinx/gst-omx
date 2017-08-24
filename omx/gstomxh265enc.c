/* GStreamer
 * Copyright (C) 2017 Xilinx, Inc. based on
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <gst/gst.h>

#include "gstomxh265enc.h"
#include "gstomxh265utils.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h265_enc_debug_category);
#define GST_CAT_DEFAULT gst_omx_h265_enc_debug_category

/* prototypes */
static gboolean gst_omx_h265_enc_set_format (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstCaps *gst_omx_h265_enc_get_caps (GstOMXVideoEnc * enc,
    GstOMXPort * port, GstVideoCodecState * state);
static GstFlowReturn gst_omx_h265_enc_handle_output_frame (GstOMXVideoEnc *
    self, GstOMXPort * port, GstOMXBuffer * buf, GstVideoCodecFrame * frame);

static OMX_ERRORTYPE gst_omx_h265_enc_set_insert_sps_pps (GstOMXVideoEnc * enc);

enum
{
  PROP_0,
  PROP_GOP_LENGTH,
  PROP_B_FRAMES,
  PROP_SLICE_SIZE,
  PROP_DEPENDENT_SLICE,
  PROP_INTRA_PREDICTION,
  PROP_LOOP_FILTER
};

#define GST_OMX_H265_ENC_GOP_LENGTH_DEFAULT (30)
#define GST_OMX_H265_ENC_B_FRAMES_DEFAULT (0)
#define GST_OMX_H265_ENC_SLICE_SIZE_DEFAULT (0)
#define GST_OMX_H265_ENC_DEPENDENT_SLICE_DEFAULT (FALSE)
#define GST_OMX_H265_ENC_INTRA_PREDICTION_DEFAULT (FALSE)
#define GST_OMX_H265_ENC_LOOP_FILTER_DEFAULT (0xffffffff)


/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h265_enc_debug_category, "omxh265enc", 0, \
      "debug category for gst-omx video encoder base class");

#define parent_class gst_omx_h265_enc_parent_class
G_DEFINE_TYPE_WITH_CODE (GstOMXH265Enc, gst_omx_h265_enc,
    GST_TYPE_OMX_VIDEO_ENC, DEBUG_INIT);

#define GST_TYPE_OMX_H265_ENC_LOOP_FILTER (gst_omx_h265_enc_loop_filter_get_type ())
static GType
gst_omx_h265_enc_loop_filter_get_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {OMX_ALG_VIDEO_HEVCLoopFilterEnable, "Enable deblocking filter","Enable"},
      {OMX_ALG_VIDEO_HEVCLoopFilterDisable,"Disable deblocking filter","Disable"},
      {OMX_ALG_VIDEO_HEVCLoopFilterDisableCrossSlice,"Disable Cross slice in deblocking filter","DisableCrossSlice"},
      {OMX_ALG_VIDEO_HEVCLoopFilterDisableCrossTile,"Disable Cross tile in deblocking filter","DisableCrossTile"},
      {OMX_ALG_VIDEO_HEVCLoopFilterDisableCrossSliceAndTile,"Disable slice & tile in deblocking filter","DisableSliceAndTile"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXH265EncLoopFilter", values);
  }
  return qtype;
}



static void
gst_omx_h265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (object);

  switch (prop_id) {
    case PROP_GOP_LENGTH:
      self->gop_length = g_value_get_uint (value);
      break;
    case PROP_B_FRAMES:
      self->b_frames = g_value_get_uint (value);
      break;
    case PROP_SLICE_SIZE:
      self->slice_size = g_value_get_uint (value);
      break;
    case PROP_DEPENDENT_SLICE:
      self->dependent_slice = g_value_get_boolean (value);
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
gst_omx_h265_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (object);

  switch (prop_id) {
    case PROP_GOP_LENGTH:
      g_value_set_uint (value, self->gop_length);
      break;
    case PROP_B_FRAMES:
      g_value_set_uint (value, self->b_frames);
      break;
    case PROP_SLICE_SIZE:
      g_value_set_uint (value, self->slice_size);
      break;
    case PROP_DEPENDENT_SLICE:
      g_value_set_boolean (value, self->dependent_slice);
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

static gboolean
gst_omx_h265_enc_open (GstVideoEncoder * encoder)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (encoder);
  GstOMXVideoEnc *omx_enc = GST_OMX_VIDEO_ENC (encoder);
  guint32 p_frames;
  guint32 b_frames;
  OMX_ALG_VIDEO_PARAM_HEVCTYPE hevc_param;
  OMX_ERRORTYPE err;

  if (!GST_VIDEO_ENCODER_CLASS (parent_class)->open (encoder))
    return FALSE;

  GST_OMX_INIT_STRUCT (&hevc_param);
  hevc_param.nPortIndex = omx_enc->enc_out_port->index;

  err = gst_omx_component_get_parameter (omx_enc->enc,
      OMX_ALG_IndexParamVideoHevc, &hevc_param);

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
		self->gop_length = GST_OMX_H265_ENC_GOP_LENGTH_DEFAULT;
		self->b_frames = GST_OMX_H265_ENC_B_FRAMES_DEFAULT;
	}
  }

  p_frames = ( (self->gop_length) / (self->b_frames + 1) )  - 1;
  b_frames = self->gop_length - (p_frames + 1);

  if (err == OMX_ErrorNone) {
    if (p_frames != hevc_param.nPFrames) {
      GST_LOG_OBJECT (self, "Changing number of P-Frame to %d",
          p_frames);
      hevc_param.nPFrames = p_frames;
    }
    if (b_frames != hevc_param.nBFrames) {
      GST_LOG_OBJECT (self, "Changing number of B-Frame to %d",
          b_frames);
      hevc_param.nBFrames = b_frames;
    }
    if (self->intra_pred != GST_OMX_H265_ENC_INTRA_PREDICTION_DEFAULT) {
       hevc_param.bConstIpred = self->intra_pred;
    }
    if (self->loop_filter != GST_OMX_H265_ENC_LOOP_FILTER_DEFAULT) {
       hevc_param.eLoopFilterMode = self->loop_filter;
    }

    err =
        gst_omx_component_set_parameter (omx_enc->enc,
        OMX_ALG_IndexParamVideoHevc, &hevc_param);
    if (err == OMX_ErrorUnsupportedIndex) {
      GST_WARNING_OBJECT (self,
          "Setting HEVC parameters not supported by the component");
    } else if (err == OMX_ErrorUnsupportedSetting) {
      GST_WARNING_OBJECT (self,
          "Setting HEVC parameters %u %u not supported by the component",
          self->gop_length, self->b_frames);
    } else if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to set HEVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  } else {
    GST_ERROR_OBJECT (self,
        "Failed to get HEVC parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
  }

  if (self->slice_size != GST_OMX_H265_ENC_SLICE_SIZE_DEFAULT) { 
    OMX_ALG_VIDEO_PARAM_SLICES slices;

    GST_OMX_INIT_STRUCT (&slices);
    slices.nPortIndex = omx_enc->enc_out_port->index;

    err = gst_omx_component_get_parameter (omx_enc->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSlices, &slices);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to get HEVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    slices.nSlicesSize = self->slice_size;
    GST_DEBUG_OBJECT (self, "setting size of slices to %d", self->slice_size);
    
    err =
        gst_omx_component_set_parameter (omx_enc->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSlices, &slices);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to set HEVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }

  if (self->dependent_slice != GST_OMX_H265_ENC_DEPENDENT_SLICE_DEFAULT) { 
    OMX_ALG_VIDEO_PARAM_SLICES slices;

    GST_OMX_INIT_STRUCT (&slices);
    slices.nPortIndex = omx_enc->enc_out_port->index;

    err = gst_omx_component_get_parameter (omx_enc->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSlices, &slices);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to get HEVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }

    slices.bDependentSlices = self->dependent_slice;
    GST_DEBUG_OBJECT (self, "setting dependent slice flag to %d", self->dependent_slice);
    
    err =
        gst_omx_component_set_parameter (omx_enc->enc,
        (OMX_INDEXTYPE) OMX_ALG_IndexParamVideoSlices, &slices);
    if (err != OMX_ErrorNone) {
      GST_ERROR_OBJECT (self,
          "Failed to set HEVC parameters: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }

  return TRUE;
}

static void
gst_omx_h265_enc_class_init (GstOMXH265EncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *basevideoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstOMXVideoEncClass *videoenc_class = GST_OMX_VIDEO_ENC_CLASS (klass);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h265_enc_set_format);
  videoenc_class->get_caps = GST_DEBUG_FUNCPTR (gst_omx_h265_enc_get_caps);
  videoenc_class->handle_output_frame =
      GST_DEBUG_FUNCPTR (gst_omx_h265_enc_handle_output_frame);

  gobject_class->set_property = gst_omx_h265_enc_set_property;
  gobject_class->get_property = gst_omx_h265_enc_get_property;

  g_object_class_install_property (gobject_class, PROP_GOP_LENGTH,
      g_param_spec_uint ("gop-length", "Number of all frames in 1 GOP, Must be in multiple of (b-frames+1)",
          "Distance between two consecutive I frames(30=component default)",
          0, 1000, GST_OMX_H265_ENC_GOP_LENGTH_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_B_FRAMES,
      g_param_spec_uint ("b-frames", "Number of B-Frames",
          "Number of B-Frames between two consecutive P-frames(0=component default)",
          0, 4, GST_OMX_H265_ENC_B_FRAMES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SLICE_SIZE,
      g_param_spec_uint ("slice-size", "size of each slices",
          "Target Slice Size, If set to 0, slices are defined by the num-slices parameter,"
	  "Otherwise it specifies the target slice size, in bytes, that the encoder uses to"
	   "automatically split the bitstream into approximately equally-sized slices (0=component default)",
          0, 65535 , GST_OMX_H265_ENC_SLICE_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_DEPENDENT_SLICE,
      g_param_spec_boolean ("dependent-slice", "Enable/disable dependent slice",
          "In Multiple slices encoding,specify whether the additional slices are"
          "Dependent slice segments or regular slices (false=component default)",
	  GST_OMX_H265_ENC_DEPENDENT_SLICE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_PREDICTION,
      g_param_spec_boolean ("constrained-intra-pred",
          "Constrained Intra Pred",
          "Enable(true)/Disables(false) constrained_intra_pred_flag syntax element",
          GST_OMX_H265_ENC_INTRA_PREDICTION_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_LOOP_FILTER,
      g_param_spec_enum ("loop-filter", "Loop Filter mode",
          "enables/disables the deblocking filter",
          GST_TYPE_OMX_H265_ENC_LOOP_FILTER,
          GST_OMX_H265_ENC_LOOP_FILTER_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  basevideoenc_class->open = gst_omx_h265_enc_open;

  videoenc_class->cdata.default_src_template_caps = "video/x-h265, "
      "width=(int) [ 1, MAX ], " "height=(int) [ 1, MAX ], "
      "framerate = (fraction) [0, MAX], "
      "stream-format=(string) byte-stream, "
      "alignment=(string) au ";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.265 Video Encoder",
      "Codec/Encoder/Video",
      "Encode H.265 video streams", "Sanket Kothari <skothari@nvidia.com>");

  gst_omx_set_default_role (&videoenc_class->cdata, "video_encoder.hevc");
}

static void
gst_omx_h265_enc_init (GstOMXH265Enc * self)
{
  self->gop_length = GST_OMX_H265_ENC_GOP_LENGTH_DEFAULT;
  self->b_frames = GST_OMX_H265_ENC_B_FRAMES_DEFAULT;
  self->slice_size = GST_OMX_H265_ENC_SLICE_SIZE_DEFAULT; 
  self->dependent_slice = GST_OMX_H265_ENC_DEPENDENT_SLICE_DEFAULT;	
  self->intra_pred = GST_OMX_H265_ENC_INTRA_PREDICTION_DEFAULT;

  self->insert_sps_pps = TRUE;
}

static gboolean
gst_omx_h265_enc_set_format (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (enc);
  GstCaps *peercaps;
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  OMX_ERRORTYPE err;
  const gchar *profile_string = NULL, *level_string = NULL, *tier_string;

  gst_omx_port_get_port_definition (GST_OMX_VIDEO_ENC (self)->enc_out_port,
      &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingVendorStartUnused;
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

    if (err == OMX_ErrorNone) {
      profile_string = gst_structure_get_string (s, "profile");
      if (profile_string) {
        param.eProfile =
            gst_omx_h265_utils_get_profile_from_str (profile_string);
        if (param.eProfile == OMX_ALG_VIDEO_HEVCProfileMax)
          goto unsupported_profile;
      }

      tier_string = gst_structure_get_string (s, "tier");
      level_string = gst_structure_get_string (s, "level");
      if (tier_string && level_string) {
        param.eLevel = gst_omx_h265_utils_get_level_from_str (tier_string, level_string);
        if (param.eLevel == OMX_ALG_VIDEO_HEVCLevelMax)
          goto unsupported_level;
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
    }

    gst_caps_unref (peercaps);
  }

#if 0
  if (self->insert_sps_pps) {
    err = gst_omx_h265_enc_set_insert_sps_pps (enc);
    if (err != OMX_ErrorNone) {
      GST_WARNING_OBJECT (self,
          "Error setting insert sps pps: %s (0x%08x)",
          gst_omx_error_to_string (err), err);
      return FALSE;
    }
  }
#endif

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
gst_omx_h265_enc_get_caps (GstOMXVideoEnc * enc, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH265Enc *self = GST_OMX_H265_ENC (enc);
  GstCaps *caps;
  OMX_ERRORTYPE err;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  const gchar *profile, *tier, *level;

  caps = gst_caps_new_simple ("video/x-h265",
      "alignment", G_TYPE_STRING, "au", "stream-format", G_TYPE_STRING,
      "byte-stream", NULL);

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_ENC (self)->enc_out_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err != OMX_ErrorNone && err != OMX_ErrorUnsupportedIndex
      && err != OMX_ErrorNotImplemented)
    return NULL;

  if (err == OMX_ErrorNone) {
    switch (param.eProfile) {
      case OMX_ALG_VIDEO_HEVCProfileMain:
        profile = "main";
        break;
      case OMX_ALG_VIDEO_HEVCProfileMain10:
        profile = "main-10";
        break;
#if !USE_OMX_TARGET_ZYNQ_USCALE_PLUS
      case OMX_ALG_VIDEO_HEVCProfileMainStillPicture:
        profile = "main-still-picture";
        break;
#else
      case OMX_ALG_VIDEO_HEVCProfileMainStill:
        profile = "mainstill";
        break;
      case OMX_ALG_VIDEO_HEVCProfileMain422:
        profile = "main422";
        break;
#endif
      default:
        g_assert_not_reached ();
        return NULL;
    }

    switch (param.eLevel) {
      case OMX_ALG_VIDEO_HEVCMainTierLevel1:
        tier = "main";
        level = "1";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel2:
        tier = "main";
        level = "2";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel21:
        tier = "main";
        level = "2.1";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel3:
        tier = "main";
        level = "3";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel31:
        tier = "main";
        level = "3.1";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel4:
        tier = "main";
        level = "4";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel41:
        tier = "main";
        level = "4.1";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel5:
        tier = "main";
        level = "5";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel51:
        tier = "main";
        level = "5.1";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel52:
        tier = "main";
        level = "5.2";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel6:
        tier = "main";
        level = "6";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel61:
        tier = "main";
        level = "6.1";
        break;
      case OMX_ALG_VIDEO_HEVCMainTierLevel62:
        tier = "main";
        level = "6.2";
        break;
#if 0
      case OMX_VIDEO_HEVCHighTierLevel1:
        tier = "high";
        level = "1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel2:
        tier = "high";
        level = "2";
        break;
      case OMX_VIDEO_HEVCHighTierLevel21:
        tier = "high";
        level = "2.1";
        break;
      case OMX_VIDEO_HEVCHighTierLevel3:
        tier = "high";
        level = "3";
        break;
      case OMX_VIDEO_HEVCHighTierLevel31:
        tier = "high";
        level = "3.1";
        break;
#endif
      case OMX_ALG_VIDEO_HEVCHighTierLevel4:
        tier = "high";
        level = "4";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel41:
        tier = "high";
        level = "4.1";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel5:
        tier = "high";
        level = "5";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel51:
        tier = "high";
        level = "5.1";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel52:
        tier = "high";
        level = "5.2";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel6:
        tier = "high";
        level = "6";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel61:
        tier = "high";
        level = "6.1";
        break;
      case OMX_ALG_VIDEO_HEVCHighTierLevel62:
        tier = "high";
        level = "6.2";
        break;

      default:
        g_assert_not_reached ();
        return NULL;
    }

    gst_caps_set_simple (caps,
        "profile", G_TYPE_STRING, profile, "tier", G_TYPE_STRING, tier, "level",
        G_TYPE_STRING, level, NULL);
  }

  /* FIXME: Should we enable this using a hack flag ? */
#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  gst_caps_set_simple (caps, "parsed", G_TYPE_BOOLEAN, TRUE, NULL);
#endif

  return caps;
}

static GstFlowReturn
gst_omx_h265_enc_handle_output_frame (GstOMXVideoEnc * self, GstOMXPort * port,
    GstOMXBuffer * buf, GstVideoCodecFrame * frame)
{
  if (buf->omx_buf->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    /* The codec data is SPS/PPS with a startcode => bytestream stream format
     * For bytestream stream format the SPS/PPS is only in-stream and not
     * in the caps!
     */

    if (buf->omx_buf->nFilledLen >= 4 &&
        GST_READ_UINT32_BE (buf->omx_buf->pBuffer +
            buf->omx_buf->nOffset) == 0x00000001) {
#ifndef USE_OMX_TARGET_TEGRA
      GList *l = NULL;
      GstBuffer *hdrs;
      GstMapInfo map = GST_MAP_INFO_INIT;

      GST_DEBUG_OBJECT (self, "got codecconfig in byte-stream format");
      buf->omx_buf->nFlags &= ~OMX_BUFFERFLAG_CODECCONFIG;

      hdrs = gst_buffer_new_and_alloc (buf->omx_buf->nFilledLen);

      gst_buffer_map (hdrs, &map, GST_MAP_WRITE);
      memcpy (map.data,
          buf->omx_buf->pBuffer + buf->omx_buf->nOffset,
          buf->omx_buf->nFilledLen);
      gst_buffer_unmap (hdrs, &map);
      l = g_list_append (l, hdrs);
      gst_video_encoder_set_headers (GST_VIDEO_ENCODER (self), l);
#else
      /* No need to send headers in case of byte-stream.
       * Attach SPS and PPS instead */
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_OK;
#endif
    }
  }

  return
      GST_OMX_VIDEO_ENC_CLASS
      (gst_omx_h265_enc_parent_class)->handle_output_frame (self, port, buf,
      frame);
}

#if 0
static OMX_ERRORTYPE
gst_omx_h265_enc_set_insert_sps_pps (GstOMXVideoEnc * enc)
{
  OMX_INDEXTYPE eIndex;
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  NVX_PARAM_VIDENCPROPERTY oEncodeProp;
  GstOMXH265Enc *self = GST_OMX_H265_ENC (enc);

  if (self->insert_sps_pps) {
    GST_OMX_INIT_STRUCT (&oEncodeProp);
    oEncodeProp.nPortIndex = enc->enc_out_port->index;

    eError = gst_omx_component_get_index (GST_OMX_VIDEO_ENC (self)->enc,
        (gpointer) NVX_INDEX_PARAM_VIDEO_ENCODE_PROPERTY, &eIndex);

    if (eError == OMX_ErrorNone) {
      eError =
          gst_omx_component_get_parameter (GST_OMX_VIDEO_ENC (self)->enc,
          eIndex, &oEncodeProp);
      if (eError == OMX_ErrorNone) {
        oEncodeProp.bInsertSPSPPSAtIDR = self->insert_sps_pps;

        eError =
            gst_omx_component_set_parameter (GST_OMX_VIDEO_ENC (self)->enc,
            eIndex, &oEncodeProp);
      }
    }
  }
  return eError;
}
#endif