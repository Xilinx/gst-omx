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

#include <gst/gst.h>

#include "gstomxh265dec.h"
#include "gstomxh265utils.h"

GST_DEBUG_CATEGORY_STATIC (gst_omx_h265_dec_debug_category);
#define GST_CAT_DEFAULT gst_omx_h265_dec_debug_category

/* prototypes */
static gboolean gst_omx_h265_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);
static gboolean gst_omx_h265_dec_set_format (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state);

enum
{
  PROP_0
};

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_h265_dec_debug_category, "omxh265dec", 0, \
      "debug category for gst-omx video decoder base class");

G_DEFINE_TYPE_WITH_CODE (GstOMXH265Dec, gst_omx_h265_dec,
    GST_TYPE_OMX_VIDEO_DEC, DEBUG_INIT);

static void
gst_omx_h265_dec_class_init (GstOMXH265DecClass * klass)
{
  GstOMXVideoDecClass *videodec_class = GST_OMX_VIDEO_DEC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  videodec_class->is_format_change =
      GST_DEBUG_FUNCPTR (gst_omx_h265_dec_is_format_change);
  videodec_class->set_format = GST_DEBUG_FUNCPTR (gst_omx_h265_dec_set_format);

  videodec_class->cdata.default_sink_template_caps = "video/x-h265, "
      "parsed=(boolean) true, "
      "alignment=(string) au, "
      "stream-format=(string) byte-stream, "
      "width=(int) [1,MAX], " "height=(int) [1,MAX]";

  gst_element_class_set_static_metadata (element_class,
      "OpenMAX H.265 Video Decoder",
      "Codec/Decoder/Video",
      "Decode H.265 video streams", "Sanket Kothari <skothari@nvidia.com>");

  gst_omx_set_default_role (&videodec_class->cdata, "video_decoder.hevc");
}

static void
gst_omx_h265_dec_init (GstOMXH265Dec * self)
{

}

static gboolean
gst_omx_h265_dec_is_format_change (GstOMXVideoDec * dec,
    GstOMXPort * port, GstVideoCodecState * state)
{
  return FALSE;
}


static gboolean
gst_omx_h265_dec_set_format (GstOMXVideoDec * dec, GstOMXPort * port,
    GstVideoCodecState * state)
{
  GstOMXH265Dec *self = GST_OMX_H265_DEC (dec);
  OMX_PARAM_PORTDEFINITIONTYPE port_def;
  OMX_ERRORTYPE err;
  OMX_VIDEO_PARAM_PROFILELEVELTYPE param;
  GstCaps *peercaps;
  const gchar *profile_string = NULL, *level_string = NULL, *tier_string;

  gst_omx_port_get_port_definition (port, &port_def);
  port_def.format.video.eCompressionFormat = OMX_VIDEO_CodingVendorStartUnused;

  err = gst_omx_port_update_port_definition (port, &port_def);
  if (err != OMX_ErrorNone)
    return FALSE;

  GST_OMX_INIT_STRUCT (&param);
  param.nPortIndex = GST_OMX_VIDEO_DEC (self)->dec_in_port->index;

  err =
      gst_omx_component_get_parameter (GST_OMX_VIDEO_DEC (self)->dec,
      OMX_IndexParamVideoProfileLevelCurrent, &param);
  if (err != OMX_ErrorNone) {
    GST_WARNING_OBJECT (self,
        "Setting profile/level not supported by component");
  }

  peercaps = gst_pad_peer_query_caps (GST_VIDEO_DECODER_SINK_PAD (dec),
      gst_pad_get_pad_template_caps (GST_VIDEO_DECODER_SINK_PAD (dec)));
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
        param.eLevel =
            gst_omx_h265_utils_get_level_from_str (tier_string, level_string);

        if (param.eLevel == OMX_ALG_VIDEO_HEVCLevelMax)
          goto unsupported_level;
      }

      err =
          gst_omx_component_set_parameter (GST_OMX_VIDEO_DEC (self)->dec,
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
