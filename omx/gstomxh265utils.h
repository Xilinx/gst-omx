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

#ifndef __GST_OMX_H265_UTILS_H__
#define __GST_OMX_H265_UTILS_H__

#include "gstomx.h"

G_BEGIN_DECLS

OMX_VIDEO_HEVCPROFILETYPE gst_omx_h265_utils_get_profile_from_str (const gchar * profile);
OMX_VIDEO_HEVCLEVELTYPE gst_omx_h265_utils_get_level_from_str (const gchar * tier, const gchar * level);

G_END_DECLS
#endif /* __GST_OMX_H265_UTILS_H__ */
