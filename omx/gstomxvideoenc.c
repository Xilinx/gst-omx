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
      {OMX_Video_ControlRateDisable, "Disable", "disable"},
      {OMX_Video_ControlRateVariable, "Variable", "variable"},
      {OMX_Video_ControlRateConstant, "Constant", "constant"},
      {OMX_Video_ControlRateVariableSkipFrames, "Variable Skip Frames",
          "variable-skip-frames"},
      {OMX_Video_ControlRateConstantSkipFrames, "Constant Skip Frames",
          "constant-skip-frames"},
      {0xffffffff, "Component Default", "default"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncControlRate", values);
  }
  return qtype;
}

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

#define GST_TYPE_OMX_VIDEO_ENC_QP_MODE_TYPE (gst_omx_video_enc_qp_mode_type ())

static GType
gst_omx_video_enc_qp_mode_type (void)
{
  static GType qtype = 0;

  if (qtype == 0) {
    static const GEnumValue values[] = {
      {AUTO_QP, "QpModeAuto",
          "auto"},
      {UNIFORM_QP, "QpModeUniform", "uniform"},
      {0, NULL, NULL}
    };

    qtype = g_enum_register_static ("GstOMXVideoEncQpModeType", values);
  }
  return qtype;
}
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

static gboolean *gst_omx_video_enc_sink_event (GstVideoEncoder * encoder,
    GstEvent * event);

enum
{
  PROP_0,
  PROP_CONTROL_RATE,
  PROP_TARGET_BITRATE,
  PROP_QUANT_I_FRAMES,
  PROP_QUANT_P_FRAMES,
  PROP_QUANT_B_FRAMES,
  PROP_STRIDE,
  PROP_INPUT_MODE,
  PROP_L2CACHE,
  PROP_SLICEHEIGHT,
  PROP_QPMODE
};

/* FIXME: Better defaults */
#define GST_OMX_VIDEO_ENC_CONTROL_RATE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_TARGET_BITRATE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_QUANT_I_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_QUANT_P_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_QUANT_B_FRAMES_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_INPUT_MODE_DEFAULT (0xffffffff)
#define GST_OMX_VIDEO_ENC_L2CACHE_DEFAULT (0)
#define GST_OMX_VIDEO_ENC_STRIDE_DEFAULT (1)
#define GST_OMX_VIDEO_ENC_SLICEHEIGHT_DEFAULT (1)
#define GST_OMX_VIDEO_ENC_QP_MODE_DEFAULT (0x0)

/* class initialization */

#define DEBUG_INIT \
  GST_DEBUG_CATEGORY_INIT (gst_omx_video_enc_debug_category, "omxvideoenc", 0, \
      "debug category for gst-omx video encoder base class");

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstOMXVideoEnc, gst_omx_video_enc,
    GST_TYPE_VIDEO_ENCODER, DEBUG_INIT);

gboolean (*sink_event_backup) (GstVideoEncoder * encoder, GstEvent * event);
GList *buffer_list = NULL;
gint g_dmalist_count = 0;
gint bufpool_complete = 0;
guint32 qp_mode;

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

  g_object_class_install_property (gobject_class, PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate", "Target Bitrate",
          "Target bitrate (0xffffffff=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_TARGET_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

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
      g_param_spec_uint ("enc-buffer-size", "Value of L2Cache buffer size",
          "Value of encoder L2Cache buffer size in KB (0x0=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_L2CACHE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_SLICEHEIGHT,
      g_param_spec_uint ("sliceHeight", "Value of nsliceHeight",
          "Set it when input has alignment requirement in height (1=component default)",
          0, G_MAXUINT, GST_OMX_VIDEO_ENC_SLICEHEIGHT_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  g_object_class_install_property (gobject_class, PROP_QPMODE,
      g_param_spec_enum ("qpmode", "Qp mode type",
          "Type of QP mode selection for encoder",
          GST_TYPE_OMX_VIDEO_ENC_QP_MODE_TYPE,
          GST_OMX_VIDEO_ENC_QP_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

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
  video_encoder_class->sink_event =
      GST_DEBUG_FUNCPTR (gst_omx_video_enc_sink_event);

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

  g_mutex_init (&self->drain_lock);
  g_cond_init (&self->drain_cond);
}

static gboolean
gst_omx_video_enc_open (GstVideoEncoder * encoder)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (encoder);
  GstOMXVideoEncClass *klass = GST_OMX_VIDEO_ENC_GET_CLASS (self);
  gint in_port_index, out_port_index;

#ifdef USE_OMX_TARGET_ZYNQ_USCALE_PLUS
  OMX_INDEXTYPE DMAtype, CHANNELtype;
  OMX_PORT_PARAM_BUFFERMODE enable_dmabuf;
  OMX_VIDEO_PARAM_CHANNEL channel_setting;
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

  OMX_GetExtensionIndex (self->enc->handle,
      (OMX_STRING) "OMX.allegro.bufferMode", &DMAtype);

  GST_OMX_INIT_STRUCT (&enable_dmabuf);
  if(use_dmabuf)
  	enable_dmabuf.eMode = OMX_BUF_DMA;
  else
  	enable_dmabuf.eMode = OMX_BUF_NORMAL;
  enable_dmabuf.nPortIndex = self->enc_in_port->index;
  OMX_SetParameter (self->enc->handle, DMAtype, &enable_dmabuf);

  OMX_GetExtensionIndex (self->enc->handle,
	(OMX_STRING) "OMX.allegro.encoder.channel", &CHANNELtype);
  GST_OMX_INIT_STRUCT (&channel_setting);
  if(self->l2cache) {
	channel_setting.nL2CacheSize = (OMX_U32) self->l2cache;
  }
  if (self->enc_out_port) {
  	channel_setting.eQpControlMode = (OMX_U32) qp_mode; 
  	channel_setting.nPortIndex = self->enc_out_port->index;
  }
  channel_setting.nNumSlices = 1;
//FIXME: setting below parameters Break things 
#if 0
  err =
      gst_omx_component_set_parameter (self->enc,
      CHANNELtype, &channel_setting);
  if (err != OMX_ErrorNone) {
    GST_ERROR_OBJECT (self,
        "Failed to channel setting parameters: %s (0x%08x)",
        gst_omx_error_to_string (err), err);
    return FALSE;
  } else {
    GST_DEBUG_OBJECT (self, "encoded channel settings are updated");
  }
#endif
#endif

  if (!self->enc_in_port || !self->enc_out_port)
    return FALSE;

  /* Set properties */
  {
    OMX_ERRORTYPE err;

    if (self->control_rate != 0xffffffff || self->target_bitrate != 0xffffffff) {
      OMX_VIDEO_PARAM_BITRATETYPE bitrate_param;

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

    if (self->quant_i_frames != 0xffffffff ||
        self->quant_p_frames != 0xffffffff ||
        self->quant_b_frames != 0xffffffff) {
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
    case PROP_QPMODE:
      qp_mode = g_value_get_enum (value);
      break;
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
    case PROP_QPMODE:
      g_value_set_enum (value, qp_mode);
      break;
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

    caps = klass->get_caps (self, self->enc_out_port, self->input_state);
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
      GST_ERROR_OBJECT (self, "No corresponding frame found");
      flow_ret = gst_pad_push (GST_VIDEO_ENCODER_SRC_PAD (self), outbuf);
    }
  } else if (frame != NULL) {
    flow_ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (self), frame);
  }

  return flow_ret;
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

    caps = klass->get_caps (self, self->enc_out_port, self->input_state);
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

      err = gst_omx_port_allocate_buffers (port);
      if (err != OMX_ErrorNone)
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

static guint
get_latency_in_frames (GstOMXVideoEnc * self)
{
  if (g_getenv ("HACK_ENC_LATENCY")) {
    guint frames = atoi (g_getenv ("HACK_ENC_LATENCY"));
    GST_DEBUG_OBJECT (self, "HACK set %d buffers as latency", frames);
    return frames;
  }

  /* Processing time takes roughly one frame in common scenarios */
  return self->enc_in_port->port_def.nBufferCountMin + 1;
}

static void
gst_omx_video_enc_set_latency (GstOMXVideoEnc * self)
{
  GstVideoCodecState *state = gst_video_codec_state_ref (self->input_state);
  GstVideoInfo *info = &state->info;
  GstClockTime latency;
  gint max_delayed_frames;

  max_delayed_frames = get_latency_in_frames (self);

  if (info->fps_n) {
    latency = gst_util_uint64_scale_ceil (GST_SECOND * info->fps_d,
        max_delayed_frames, info->fps_n);
  } else {
    /* FIXME: Assume 25fps. This is better than reporting no latency at
     * all and then later failing in live pipelines
     */
    latency = gst_util_uint64_scale_ceil (GST_SECOND * 1,
        max_delayed_frames, 25);
  }

  GST_INFO_OBJECT (self,
      "Updating latency to %" GST_TIME_FORMAT " (%d frames)",
      GST_TIME_ARGS (latency), max_delayed_frames);

  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (self), latency, latency);

  gst_video_codec_state_unref (state);
}

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
  GList *buffer_list = NULL;
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
  if (self->stride) {
    port_def.nBufferAlignment = self->stride;
  }
  if (port_def.nBufferAlignment)
    port_def.format.video.nStride =
        (info->width + port_def.nBufferAlignment - 1) &
        (~(port_def.nBufferAlignment - 1));
  else
    port_def.format.video.nStride = GST_ROUND_UP_4 (info->width);       /* safe (?) default */

  port_def.format.video.nFrameHeight = info->height;
  if (self->sliceHeight) {
    port_def.format.video.nSliceHeight =
        (info->height + self->sliceHeight - 1) &
        (~(self->sliceHeight - 1));
  } else {
    port_def.format.video.nSliceHeight = info->height;
  }

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
    if (gst_omx_port_allocate_buffers (self->enc_in_port) != OMX_ErrorNone)
      return FALSE;

    if ((klass->cdata.hacks & GST_OMX_HACK_NO_DISABLE_OUTPORT)) {
      if (gst_omx_port_set_enabled (self->enc_out_port, TRUE) != OMX_ErrorNone)
        return FALSE;
      if (gst_omx_port_allocate_buffers (self->enc_out_port) != OMX_ErrorNone)
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
      if (gst_omx_port_allocate_buffers (self->enc_in_port) != OMX_ErrorNone)
        return FALSE;
    } else {
      if (gst_omx_component_set_state (self->enc,
              OMX_StateIdle) != OMX_ErrorNone)
        return FALSE;

      /* Need to allocate buffers to reach Idle state */

      if (self->input_mode == OMX_Enc_InputMode_DefaultImplementation) {
        if (gst_omx_port_allocate_buffers (self->enc_in_port) != OMX_ErrorNone)
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
          buffer_list = g_list_append (buffer_list, map_info.data);

        if (gst_omx_port_use_buffers (self->enc_in_port,
                buffer_list) != OMX_ErrorNone)
          return FALSE;

        gst_buffer_unmap (mem, &map_info);
        gst_buffer_unref (mem);
        g_list_free (buffer_list);
      }

      if (gst_omx_port_allocate_buffers (self->enc_out_port) != OMX_ErrorNone)
        return FALSE;
    }

    if (gst_omx_component_get_state (self->enc,
            GST_CLOCK_TIME_NONE) != OMX_StateIdle)
      return FALSE;

    if ((self->input_mode == OMX_Enc_InputMode_ZeroCopy) ||
        (self->input_mode == OMX_Enc_InputMode_DefaultImplementation)) {
      if (gst_omx_component_set_state (self->enc,
              OMX_StateExecuting) != OMX_ErrorNone)
        return FALSE;

      if (gst_omx_component_get_state (self->enc,
              GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
        return FALSE;
    }

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

  if ((self->input_mode == OMX_Enc_InputMode_ZeroCopy) ||
      (self->input_mode == OMX_Enc_InputMode_DefaultImplementation)) {
    /* Start the srcpad loop again */
    GST_DEBUG_OBJECT (self, "Starting task again");
    self->downstream_flow_ret = GST_FLOW_OK;
    gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
        (GstTaskFunction) gst_omx_video_enc_loop, encoder, NULL);
  }

  gst_omx_video_enc_set_latency (self);

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

    ret = TRUE;
    goto done;
  } else {
    /* Different strides */

    if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
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

    if (self->input_mode == OMX_Enc_InputMode_DefaultImplementation) {
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
  static gint count = 0;
  gint fd1 = 0, fd2 = 0;
  static GList *garray = NULL;
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


  if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
    count++;
    if ((count == 1) && (bufpool_complete == 0)) {
      fd1 =
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (frame->input_buffer,
              0));
      GST_FIXME_OBJECT (self, "Got 1st fd %d\n", fd1);
      garray = g_list_append (garray, fd1);
      frame->output_buffer = NULL;
      gst_video_encoder_finish_frame (self, frame);
      return self->downstream_flow_ret;
    }
    if (bufpool_complete == 0) {
      fd1 =
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (frame->input_buffer,
              0));
      GST_FIXME_OBJECT (self, "Got next fd %d\n", fd1);
      if (g_list_find (garray, fd1) != NULL) {
        GST_FIXME_OBJECT (self, "Bufferpool found completed \n");
        bufpool_complete = 1;
      } else {
        garray = g_list_append (garray, fd1);
        frame->output_buffer = NULL;
        gst_video_encoder_finish_frame (self, frame);
        return self->downstream_flow_ret;
      }
    }
    if (bufpool_complete == 1) {

      if (count == 1) {
        GST_FIXME_OBJECT (self,
            "Bufferpool found by event, It has %d buffers \n", g_dmalist_count);
        bufpool_complete = 2;
        buffer_list = g_list_reverse (buffer_list);

        gst_omx_port_update_port_definition (self->enc_in_port, NULL);
        GST_FIXME_OBJECT (self, "OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);
        self->enc_in_port->port_def.nBufferCountActual = g_dmalist_count;
        gst_omx_port_update_port_definition (self->enc_in_port,
            &self->enc_in_port->port_def);
        GST_FIXME_OBJECT (self, "Updated OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);

      } else {

        GST_FIXME_OBJECT (self,
            "Bufferpool found by fd tracing, It has %d buffers \n",
            g_list_length (garray));
        bufpool_complete = 2;

        for (i = 0; i < g_list_length (garray); i++)
          buffer_list =
              g_list_append (buffer_list,
              GPOINTER_TO_INT (g_list_nth_data (garray, i)));

        gst_omx_port_update_port_definition (self->enc_in_port, NULL);
        GST_FIXME_OBJECT (self, "OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);
        self->enc_in_port->port_def.nBufferCountActual = g_list_length (garray);
        gst_omx_port_update_port_definition (self->enc_in_port,
            &self->enc_in_port->port_def);
        GST_FIXME_OBJECT (self, "Updated OMX Encoder BuffercountActual is %d\n",
            self->enc_in_port->port_def.nBufferCountActual);
      }

      if (gst_omx_port_use_buffers (self->enc_in_port,
              buffer_list) != OMX_ErrorNone)
        return FALSE;

      g_list_free (buffer_list);

      if (gst_omx_component_set_state (self->enc,
              OMX_StateExecuting) != OMX_ErrorNone)
        return FALSE;

      if (gst_omx_component_get_state (self->enc,
              GST_CLOCK_TIME_NONE) != OMX_StateExecuting)
        return FALSE;

      /* Start the srcpad loop again */
      GST_DEBUG_OBJECT (self, "Starting task again");
      self->downstream_flow_ret = GST_FLOW_OK;
      gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
          (GstTaskFunction) gst_omx_video_enc_loop, self, NULL);
    }
  }


  while (acq_ret != GST_OMX_ACQUIRE_BUFFER_OK) {
    GstClockTime timestamp, duration;

    /* Make sure to release the base class stream lock, otherwise
     * _loop() can't call _finish_frame() and we might block forever
     * because no input buffers are released */
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    if (self->input_mode == OMX_Enc_InputMode_DMABufImport) {
      acq_ret = gst_omx_port_acquire_buffer_dma (port, &buf,
          gst_dmabuf_memory_get_fd (gst_buffer_peek_memory (frame->input_buffer,
                  0)));
    } else {
      acq_ret = gst_omx_port_acquire_buffer (port, &buf);
    }

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

      err = gst_omx_port_allocate_buffers (port);
      if (err != OMX_ErrorNone) {
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
            buffer_list =
                g_list_append (buffer_list, g_array_index (buffers, gint, i));
            g_dmalist_count++;
          }
          bufpool_complete = 1;
	  g_array_unref(buffers);
          GST_FIXME_OBJECT (self,
              "Custom event for DMA list is sucessful, got %d fd \n",
              g_dmalist_count);
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

static gboolean
gst_omx_video_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query)
{
  GstOMXVideoEnc *self = GST_OMX_VIDEO_ENC (encoder);
  GstVideoCodecState *state = gst_video_codec_state_ref (self->input_state);
  GstVideoInfo *info = &state->info;
  guint num_buffers;

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  num_buffers = get_latency_in_frames (self) + 1;
  GST_DEBUG_OBJECT (self, "request at least %d buffers", num_buffers);
  gst_query_add_allocation_pool (query, NULL, info->size, num_buffers, 0);

  gst_video_codec_state_unref (state);

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
