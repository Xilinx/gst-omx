/* GStreamer
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

#include "gstomxh265utils.h"

OMX_VIDEO_HEVCPROFILETYPE
gst_omx_h265_utils_get_profile_from_str (const gchar * profile)
{
  if (g_str_equal (profile, "main")) {
    return OMX_VIDEO_HEVCProfileMain;
  } else if (g_str_equal (profile, "main-10")) {
    return OMX_VIDEO_HEVCProfileMain10;
#if !USE_OMX_TARGET_ZYNQ_USCALE_PLUS
 } else if (g_str_equal (profile_string, "main-still-picture")) {
   param.eProfile = OMX_VIDEO_HEVCProfileMainStillPicture;
#else
  } else if (g_str_equal (profile, "mainstill")) {
    return OMX_VIDEO_HEVCProfileMainStill;
  } else if (g_str_equal (profile, "main422")) {
    return OMX_VIDEO_HEVCProfileMain422;
#endif
  }
  return OMX_VIDEO_HEVCProfileMax;
}

OMX_VIDEO_HEVCLEVELTYPE
gst_omx_h265_utils_get_level_from_str (const gchar * tier, const gchar * level)
{
  if (g_str_equal (tier, "main")) {
    if (g_str_equal (level, "1"))
      return OMX_VIDEO_HEVCMainTierLevel1;
    else if (g_str_equal (level, "2"))
      return OMX_VIDEO_HEVCMainTierLevel2;
    else if (g_str_equal (level, "2.1"))
      return OMX_VIDEO_HEVCMainTierLevel21;
    else if (g_str_equal (level, "3"))
      return OMX_VIDEO_HEVCMainTierLevel3;
    else if (g_str_equal (level, "3.1"))
      return OMX_VIDEO_HEVCMainTierLevel31;
    else if (g_str_equal (level, "4"))
      return OMX_VIDEO_HEVCMainTierLevel4;
    else if (g_str_equal (level, "4.1"))
      return OMX_VIDEO_HEVCMainTierLevel41;
    else if (g_str_equal (level, "5"))
      return OMX_VIDEO_HEVCMainTierLevel5;
    else if (g_str_equal (level, "5.1"))
      return OMX_VIDEO_HEVCMainTierLevel51;
    else if (g_str_equal (level, "5.2"))
      return OMX_VIDEO_HEVCMainTierLevel52;
    else if (g_str_equal (level, "6"))
      return OMX_VIDEO_HEVCMainTierLevel6;
    else if (g_str_equal (level, "6.1"))
      return OMX_VIDEO_HEVCMainTierLevel61;
    else if (g_str_equal (level, "6.2"))
      return OMX_VIDEO_HEVCMainTierLevel62;
  } else if (g_str_equal (tier, "high")) {
    if (g_str_equal (level, "1"))
      return OMX_VIDEO_HEVCHighTierLevel1;
    else if (g_str_equal (level, "2"))
      return OMX_VIDEO_HEVCHighTierLevel2;
    else if (g_str_equal (level, "2.1"))
      return OMX_VIDEO_HEVCHighTierLevel21;
    else if (g_str_equal (level, "3"))
      return OMX_VIDEO_HEVCHighTierLevel3;
    else if (g_str_equal (level, "3.1"))
      return OMX_VIDEO_HEVCHighTierLevel31;
    else if (g_str_equal (level, "4"))
      return OMX_VIDEO_HEVCHighTierLevel4;
    else if (g_str_equal (level, "4.1"))
      return OMX_VIDEO_HEVCHighTierLevel41;
    else if (g_str_equal (level, "5"))
      return OMX_VIDEO_HEVCHighTierLevel5;
    else if (g_str_equal (level, "5.1"))
      return OMX_VIDEO_HEVCHighTierLevel51;
    else if (g_str_equal (level, "5.2"))
      return OMX_VIDEO_HEVCHighTierLevel52;
    else if (g_str_equal (level, "6"))
      return OMX_VIDEO_HEVCHighTierLevel6;
    else if (g_str_equal (level, "6.1"))
      return OMX_VIDEO_HEVCHighTierLevel61;
    else if (g_str_equal (level, "6.2"))
      return OMX_VIDEO_HEVCHighTierLevel62;
  }

  return OMX_VIDEO_HEVCHighTierMax;
}
