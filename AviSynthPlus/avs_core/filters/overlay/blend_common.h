// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
// http://www.avisynth.org

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

// Overlay (c) 2003, 2004 by Klaus Post

#ifndef __blend_common_h
#define __blend_common_h

#include <avs/types.h>

// Mode: Overlay
void overlay_blend_c_plane_masked_f(BYTE *p1, const BYTE *p2, const BYTE *mask,
  const int p1_pitch, const int p2_pitch, const int mask_pitch,
  const int width, const int height);

template<typename pixel_t, int bits_per_pixel>
void overlay_blend_c_plane_masked(BYTE *p1, const BYTE *p2, const BYTE *mask,
                                  const int p1_pitch, const int p2_pitch, const int mask_pitch,
                                  const int width, const int height);
#ifdef X86_32
void overlay_blend_mmx_plane_masked(BYTE *p1, const BYTE *p2, const BYTE *mask,
                                    const int p1_pitch, const int p2_pitch, const int mask_pitch,
                                    const int width, const int height);
#endif
template<typename pixel_t, int bits_per_pixel, bool hasSSE4>
void overlay_blend_sse2_plane_masked(BYTE *p1, const BYTE *p2, const BYTE *mask,
                                     const int p1_pitch, const int p2_pitch, const int mask_pitch,
                                     const int width, const int height);

template<typename pixel_t, int bits_per_pixel>
void overlay_blend_c_plane_opacity(BYTE *p1, const BYTE *p2,
                                   const int p1_pitch, const int p2_pitch,
                                   const int width, const int height, const int opacity);
void overlay_blend_c_plane_opacity_f(BYTE *p1, const BYTE *p2,
  const int p1_pitch, const int p2_pitch,
  const int width, const int height, const float opacity_f);
#ifdef X86_32
void overlay_blend_mmx_plane_opacity(BYTE *p1, const BYTE *p2,
                                     const int p1_pitch, const int p2_pitch,
                                     const int width, const int height, const int opacity);
#endif
template<typename pixel_t, int bits_per_pixel>
void overlay_blend_sse2_plane_opacity(BYTE *p1, const BYTE *p2,
                                      const int p1_pitch, const int p2_pitch,
                                      const int width, const int height, const int opacity, const float opacity_f);

template<typename pixel_t, int bits_per_pixel>
void overlay_blend_c_plane_masked_opacity(BYTE *p1, const BYTE *p2, const BYTE *mask,
                                  const int p1_pitch, const int p2_pitch, const int mask_pitch,
                                  const int width, const int height, const int opacity);
void overlay_blend_c_plane_masked_opacity_f(BYTE *p1, const BYTE *p2, const BYTE *mask,
  const int p1_pitch, const int p2_pitch, const int mask_pitch,
  const int width, const int height, const float opacity);

#ifdef X86_32
void overlay_blend_mmx_plane_masked_opacity(BYTE *p1, const BYTE *p2, const BYTE *mask,
                                    const int p1_pitch, const int p2_pitch, const int mask_pitch,
                                    const int width, const int height, const int opacity);
#endif
template<typename pixel_t, int bits_per_pixel, bool hasSSE4>
void overlay_blend_sse2_plane_masked_opacity(BYTE *p1, const BYTE *p2, const BYTE *mask,
                                     const int p1_pitch, const int p2_pitch, const int mask_pitch,
                                     const int width, const int height, const int opacity, const float opacity_f);

// Mode: Darken/lighten
template<typename pixel_t>
void overlay_darken_c(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);
template<typename pixel_t>
void overlay_lighten_c(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);

#ifdef X86_32
void overlay_darken_mmx(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);
void overlay_lighten_mmx(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);
#endif

void overlay_darken_sse2(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);
void overlay_lighten_sse2(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);

void overlay_darken_sse41(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);
void overlay_lighten_sse41(BYTE *p1Y, BYTE *p1U, BYTE *p1V, const BYTE *p2Y, const BYTE *p2U, const BYTE *p2V, int p1_pitch, int p2_pitch, int width, int height);


#endif // __blend_common_h
