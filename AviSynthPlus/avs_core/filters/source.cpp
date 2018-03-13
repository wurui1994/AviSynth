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


#include "../core/internal.h"
#include "../convert/convert.h"
#include "transform.h"
#include "AviSource/avi_source.h"
#include "../convert/convert_planar.h"

#define PI 3.1415926535897932384626433832795
#include <ctime>
#include <cmath>
#include <new>
#include <cassert>
#include <stdint.h>
#include <algorithm>

/********************************************************************
********************************************************************/

enum {
    COLOR_MODE_RGB = 0,
    COLOR_MODE_YUV
};

class StaticImage : public IClip {
  const VideoInfo vi;
  const PVideoFrame frame;
  bool parity;

public:
  StaticImage(const VideoInfo& _vi, const PVideoFrame& _frame, bool _parity)
    : vi(_vi), frame(_frame), parity(_parity) {}
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) { return frame; }
  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {
    memset(buf, 0, (size_t)vi.BytesFromAudioSamples(count));
  }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) { return (vi.IsFieldBased() ? (n&1) : false) ^ parity; }
  int __stdcall SetCacheHints(int cachehints,int frame_range)
  {
    switch (cachehints)
    {
    case CACHE_DONT_CACHE_ME:
      return 1;
    case CACHE_GET_MTMODE:
      return MT_NICE_FILTER;
    default:
      return 0;
    }
  };
};


static PVideoFrame CreateBlankFrame(const VideoInfo& vi, int color, int mode, const int *colors, const float *colors_f, bool color_is_array, IScriptEnvironment* env) {

  if (!vi.HasVideo()) return 0;

  PVideoFrame frame = env->NewVideoFrame(vi);

  // RGB 8->16 bit: not << 8 like YUV but 0..255 -> 0..65535 or 0..1023 for 10 bit
  int pixelsize = vi.ComponentSize();
  int bits_per_pixel = vi.BitsPerComponent();
  int max_pixel_value = (1 << bits_per_pixel) - 1;
  auto rgbcolor8to16 = [](uint8_t color8, int max_pixel_value) { return (uint16_t)(color8 * max_pixel_value / 255); };

  // int color holds the "old" 8 bit color values that are scaled automatically to the right bitmap
  // new in avs+: if color_is_array, color values are filled as-is, no conversion or any shift occurs

  if (vi.IsPlanar()) {

    bool isyuvlike = vi.IsYUV() || vi.IsYUVA();

    if (color_is_array) {
          // works from colors or colors_f: as-is
          // color order in the array: RGBA or YUVA
      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_R, PLANAR_G, PLANAR_B, PLANAR_A };
      int *planes = isyuvlike ? planes_y : planes_r;

      for (int p = 0; p < vi.NumComponents(); p++)
      {
        int plane = planes[p];
        BYTE *dstp = frame->GetWritePtr(plane);
        int pitch = frame->GetPitch(plane);
        int height = frame->GetHeight(plane);
        switch (pixelsize) {
        case 1: fill_plane<uint8_t>(dstp, height, pitch, clamp(colors[p], 0, 0xFF)); break;
        case 2: fill_plane<uint16_t>(dstp, height, pitch, clamp(colors[p], 0, (1 << vi.BitsPerComponent()) - 1)); break;
        case 4: fill_plane<float>(dstp, height, pitch, colors_f[p]); break;
        }
      }
    }
    else {
      int color_yuv = (mode == COLOR_MODE_YUV) ? color : RGB2YUV(color);

      int val_i;

      int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
      int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
      int *planes = isyuvlike ? planes_y : planes_r;

      for (int p = 0; p < vi.NumComponents(); p++)
      {
        int plane = planes[p];
        // color order: ARGB or AYUV
        // (8)-8-8-8 bit color from int parameter
        if (isyuvlike) {
          switch (plane) {
          case PLANAR_A: val_i = (color >> 24) & 0xff; break;
          case PLANAR_Y: val_i = (color_yuv >> 16) & 0xff; break;
          case PLANAR_U: val_i = (color_yuv >> 8) & 0xff; break;
          case PLANAR_V: val_i = color_yuv & 0xff; break;
          }
          if (bits_per_pixel != 32)
            val_i = val_i << (bits_per_pixel - 8);
        }
        else {
         // planar RGB
          switch (plane) {
          case PLANAR_A: val_i = (color >> 24) & 0xff; break;
          case PLANAR_R: val_i = (color >> 16) & 0xff; break;
          case PLANAR_G: val_i = (color >> 8) & 0xff; break;
          case PLANAR_B: val_i = color & 0xff; break;
          }
          if (bits_per_pixel != 32)
            val_i = rgbcolor8to16(val_i, max_pixel_value);
        }


        BYTE *dstp = frame->GetWritePtr(plane);
        int size = frame->GetPitch(plane) * frame->GetHeight(plane);

        switch (pixelsize) {
        case 1:
          memset(dstp, val_i, size);
          break;
        case 2: 
          val_i = clamp(val_i, 0, (1 << vi.BitsPerComponent()) - 1);
          std::fill_n((uint16_t *)dstp, size / sizeof(uint16_t), val_i);
          break; // 2 pixels at a time
        default: // case 4:
          float val_f;
          if (plane == PLANAR_U || plane == PLANAR_V) {
#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
            float shift = 0.0f;
#else
            float shift = 0.5f;
#endif
            val_f = float(val_i - 128) / 255.0f + shift; // 32 bit float chroma 128=0.5
          }
          else {
            val_f = float(val_i) / 255.0f;
          }
          std::fill_n((float *)dstp, size / sizeof(float), val_f);
        }
      }
    }
    return frame;
  } // if planar

  BYTE* p = frame->GetWritePtr();
  int size = frame->GetPitch() * frame->GetHeight();

  if (vi.IsYUY2()) {
    int color_yuv =(mode == COLOR_MODE_YUV) ? color : RGB2YUV(color);
    if (color_is_array) {
      color_yuv = (clamp(colors[0], 0, max_pixel_value) << 16) | (clamp(colors[1], 0, max_pixel_value) << 8) | (clamp(colors[2], 0, max_pixel_value));
    }
    uint32_t d = ((color_yuv>>16)&255) * 0x010001 + ((color_yuv>>8)&255) * 0x0100 + (color_yuv&255) * 0x01000000;
    for (int i=0; i<size; i+=4)
      *(uint32_t *)(p+i) = d;
  } else if (vi.IsRGB24()) {
    const uint8_t color_b  = color_is_array ? clamp(colors[2], 0, max_pixel_value) : (uint8_t)(color & 0xFF);
    const uint16_t color_rg = color_is_array ?
                              ((clamp(colors[0], 0, max_pixel_value) << 8) | clamp(colors[1], 0, max_pixel_value)) :
                              (uint16_t)(color >> 8);
    const int rowsize = frame->GetRowSize();
    const int pitch = frame->GetPitch();
    for (int y=frame->GetHeight();y>0;y--) {
      for (int i=0; i<rowsize; i+=3) {
        p[i] = color_b; *(uint16_t *)(p+i+1) = color_rg;
      }
      p+=pitch;
    }
  } else if (vi.IsRGB32()) {
    uint32_t c;
    c = color;
    if (color_is_array) {
      uint32_t r = clamp(colors[0], 0, max_pixel_value);
      uint32_t g = clamp(colors[1], 0, max_pixel_value);
      uint32_t b = clamp(colors[2], 0, max_pixel_value);
      uint32_t a = clamp(colors[3], 0, max_pixel_value);
      c = (a << 24) + (r << 16) + (g << 8) + (b);
    }
    std::fill_n((uint32_t *)p, size / 4, c);
    //for (int i=0; i<size; i+=4)
    //  *(unsigned*)(p+i) = color;
  } else if (vi.IsRGB48()) {
      const uint16_t color_b  = color_is_array ? clamp(colors[2], 0, max_pixel_value) : rgbcolor8to16(color & 0xFF, max_pixel_value);
      uint16_t r = color_is_array ? clamp(colors[0], 0, max_pixel_value) : rgbcolor8to16((color >> 16) & 0xFF, max_pixel_value);
      uint16_t g = color_is_array ? clamp(colors[1], 0, max_pixel_value) : rgbcolor8to16((color >> 8 ) & 0xFF, max_pixel_value);
      const uint32_t color_rg = (r << 16) + (g);
      const int rowsize = frame->GetRowSize() / sizeof(uint16_t);
      const int pitch = frame->GetPitch() / sizeof(uint16_t);
      uint16_t* p16 = reinterpret_cast<uint16_t*>(p);
      for (int y=frame->GetHeight();y>0;y--) {
          for (int i=0; i<rowsize; i+=3) {
              p16[i] = color_b;   // b
              *reinterpret_cast<uint32_t*>(p16+i+1) = color_rg; // gr
          }
          p16 += pitch;
      }
  } else if (vi.IsRGB64()) {
    uint64_t r, g, b, a;
    r = color_is_array ? clamp(colors[0], 0, max_pixel_value) : rgbcolor8to16((color >> 16) & 0xFF, max_pixel_value);
    g = color_is_array ? clamp(colors[1], 0, max_pixel_value) : rgbcolor8to16((color >> 8 ) & 0xFF, max_pixel_value);
    b = color_is_array ? clamp(colors[2], 0, max_pixel_value) : rgbcolor8to16((color      ) & 0xFF, max_pixel_value);
    a = color_is_array ? clamp(colors[3], 0, max_pixel_value) : rgbcolor8to16((color >> 24) & 0xFF, max_pixel_value);
    uint64_t color64 = (a << 48) + (r << 32) + (g << 16) + (b);
    std::fill_n(reinterpret_cast<uint64_t*>(p), size / sizeof(uint64_t), color64);
  }
  return frame;
}


static AVSValue __cdecl Create_BlankClip(AVSValue args, void*, IScriptEnvironment* env) {
  VideoInfo vi_default;
  memset(&vi_default, 0, sizeof(VideoInfo));

  VideoInfo vi = vi_default;

  vi_default.fps_denominator=1;
  vi_default.fps_numerator=24;
  vi_default.height=480;
  vi_default.pixel_type=VideoInfo::CS_BGR32;
  vi_default.num_frames=240;
  vi_default.width=640;
  vi_default.audio_samples_per_second=44100;
  vi_default.nchannels=1;
  vi_default.num_audio_samples=44100*10;
  vi_default.sample_type=SAMPLE_INT16;
  vi_default.SetFieldBased(false);
  bool parity=false;

  if ((args[0].ArraySize() == 1) && (!args[12].Defined())) {
    vi_default = args[0][0].AsClip()->GetVideoInfo();
    parity = args[0][0].AsClip()->GetParity(0);
  }
  else if (args[0].ArraySize() != 0) {
    env->ThrowError("BlankClip: Only 1 Template clip allowed.");
  }
  else if (args[12].Defined()) {
    vi_default = args[12].AsClip()->GetVideoInfo();
    parity = args[12].AsClip()->GetParity(0);
  }

  bool defHasVideo = vi_default.HasVideo();
  bool defHasAudio = vi_default.HasAudio();

  // If no default video
  if ( !defHasVideo ) {
    vi_default.fps_numerator=24;
    vi_default.fps_denominator=1;

    vi_default.num_frames = 240;

    // If specify Width    or Height            or Pixel_Type
    if ( args[2].Defined() || args[3].Defined() || args[4].Defined() ) {
      vi_default.width=640;
      vi_default.height=480;
      vi_default.pixel_type=VideoInfo::CS_BGR32;

      vi_default.SetFieldBased(false);
      parity=false;
    }
  }

  // If no default audio but specify Audio_rate or Channels     or Sample_Type
  if ( !defHasAudio && ( args[7].Defined() || args[8].Defined() || args[9].Defined() ) ) {
    vi_default.audio_samples_per_second=44100;
    vi_default.nchannels=1;
    vi_default.sample_type=SAMPLE_INT16;
  }

  vi.width = args[2].AsInt(vi_default.width);
  vi.height = args[3].AsInt(vi_default.height);

  if (args[4].Defined()) {
      int pixel_type = GetPixelTypeFromName(args[4].AsString());
      if(pixel_type == VideoInfo::CS_UNKNOWN)
      {
          env->ThrowError("BlankClip: pixel_type must be \"RGB32\", \"RGB24\", \"YV12\", \"YV24\", \"YV16\", \"Y8\", \n"\
              "\"YUV420P?\",\"YUV422P?\",\"YUV444P?\",\"Y?\",\n"\
              "\"RGB48\",\"RGB64\",\"RGBP\",\"RGBP?\",\n"\
              "\"YV411\" or \"YUY2\"");
      }
      vi.pixel_type = pixel_type;
  }
  else {
    vi.pixel_type = vi_default.pixel_type;
  }

  if (!vi.pixel_type)
    vi.pixel_type = VideoInfo::CS_BGR32;


  double n = args[5].AsDblDef(double(vi_default.fps_numerator));

  if (args[5].Defined() && !args[6].Defined()) {
    unsigned d = 1;
    while (n < 16777216 && d < 16777216) { n*=2; d*=2; }
    vi.SetFPS(int(n+0.5), d);
  } else {
    vi.SetFPS(int(n+0.5), args[6].AsInt(vi_default.fps_denominator));
  }

  vi.image_type = vi_default.image_type; // Copy any field sense from template

  vi.audio_samples_per_second = args[7].AsInt(vi_default.audio_samples_per_second);

  if (args[8].IsBool())
    vi.nchannels = args[8].AsBool() ? 2 : 1; // stereo=True
  else if (args[8].IsInt())
    vi.nchannels = args[8].AsInt();          // channels=2
  else
    vi.nchannels = vi_default.nchannels;

  if (args[9].IsBool())
    vi.sample_type = args[9].AsBool() ? SAMPLE_INT16 : SAMPLE_FLOAT; // sixteen_bit=True
  else if (args[9].IsString()) {
    const char* sample_type_string = args[9].AsString();
    if        (!lstrcmpi(sample_type_string, "8bit" )) {  // sample_type="8Bit"
      vi.sample_type = SAMPLE_INT8;
    } else if (!lstrcmpi(sample_type_string, "16bit")) {  // sample_type="16Bit"
      vi.sample_type = SAMPLE_INT16;
    } else if (!lstrcmpi(sample_type_string, "24bit")) {  // sample_type="24Bit"
      vi.sample_type = SAMPLE_INT24;
    } else if (!lstrcmpi(sample_type_string, "32bit")) {  // sample_type="32Bit"
      vi.sample_type = SAMPLE_INT32;
    } else if (!lstrcmpi(sample_type_string, "float")) {  // sample_type="Float"
      vi.sample_type = SAMPLE_FLOAT;
    } else {
      env->ThrowError("BlankClip: sample_type must be \"8bit\", \"16bit\", \"24bit\", \"32bit\" or \"float\"");
    }
  } else
    vi.sample_type = vi_default.sample_type;

  // If we got an Audio only default clip make the default duration the same
  if (!defHasVideo && defHasAudio) {
    const __int64 denom = Int32x32To64(vi.fps_denominator, vi_default.audio_samples_per_second);
    vi_default.num_frames = int((vi_default.num_audio_samples * vi.fps_numerator + denom - 1) / denom); // ceiling
  }

  vi.num_frames = args[1].AsInt(vi_default.num_frames);

  vi.width++; // cheat HasVideo() call for Audio Only clips
  vi.num_audio_samples = vi.AudioSamplesFromFrames(vi.num_frames);
  vi.width--;

  int color = args[10].AsInt(0);
  int mode = COLOR_MODE_RGB;
  if (args[11].Defined()) {
    if (color != 0) // Not quite 100% test
      env->ThrowError("BlankClip: color and color_yuv are mutually exclusive");
    if (!vi.IsYUV() && !vi.IsYUVA())
      env->ThrowError("BlankClip: color_yuv only valid for YUV color spaces");
    color = args[11].AsInt();
    mode=COLOR_MODE_YUV;
    if (!vi.IsYUVA() && (unsigned)color > 0xffffff)
      env->ThrowError("BlankClip: color_yuv must be between 0 and %d($ffffff)", 0xffffff);
  }

  int colors[4] = { 0 };
  float colors_f[4] = { 0.0 };
  bool color_is_array = false;
#ifdef NEW_AVSVALUE
  if (args.ArraySize() >= 14) {
    // new colors parameter
    if (args[13].Defined()) // colors
    {
      if (!args[13].IsArray())
        env->ThrowError("BlankClip: colors must be an array");
      int color_count = args[13].ArraySize();
      if (vi.NumComponents() != color_count)
        env->ThrowError("BlankClip: color count %d does not match to component count %d", color_count, vi.NumComponents());
      int pixelsize = vi.ComponentSize();
      int bits_per_pixel = vi.BitsPerComponent();
      for (int i = 0; i < color_count; i++) {
        if (pixelsize == 4)
          colors_f[i] = args[13][i].AsFloatf(0.0);
        else {
          colors[i] = args[13][i].AsInt(0);
          if (colors[i] >= (1 << bits_per_pixel) || colors < 0)
            env->ThrowError("BlankClip: invalid color value (%d) for %d bits video format", colors[i], bits_per_pixel);
        }
      }
      color_is_array = true;
    }
  }
#endif

  return new StaticImage(vi, CreateBlankFrame(vi, color, mode, colors, colors_f, color_is_array, env), parity);
}


/********************************************************************
********************************************************************/

// in text-overlay.cpp
extern bool GetTextBoundingBox(const char* text, const char* fontname,
  int size, bool bold, bool italic, int align, int* width, int* height);


PClip Create_MessageClip(const char* message, int width, int height, int pixel_type, bool shrink,
                         int textcolor, int halocolor, int bgcolor, IScriptEnvironment* env) {
  int size;
  for (size = 24*8; /*size>=9*8*/; size-=4) {
    int text_width, text_height;
    GetTextBoundingBox(message, "Arial", size, true, false, TA_TOP | TA_CENTER, &text_width, &text_height);
    text_width = ((text_width>>3)+8+7) & ~7;
    text_height = ((text_height>>3)+8+1) & ~1;
    if (size<=9*8 || ((width<=0 || text_width<=width) && (height<=0 || text_height<=height))) {
      if (width <= 0 || (shrink && width>text_width))
        width = text_width;
      if (height <= 0 || (shrink && height>text_height))
        height = text_height;
      break;
    }
  }

  VideoInfo vi;
  memset(&vi, 0, sizeof(vi));
  vi.width = width;
  vi.height = height;
  vi.pixel_type = pixel_type;
  vi.fps_numerator = 24;
  vi.fps_denominator = 1;
  vi.num_frames = 240;

  PVideoFrame frame = CreateBlankFrame(vi, bgcolor, COLOR_MODE_RGB, nullptr, nullptr, false, env);
  env->ApplyMessage(&frame, vi, message, size, textcolor, halocolor, bgcolor);
  return new StaticImage(vi, frame, false);
};


AVSValue __cdecl Create_MessageClip(AVSValue args, void*, IScriptEnvironment* env) {
  return Create_MessageClip(args[0].AsString(), args[1].AsInt(-1),
      args[2].AsInt(-1), VideoInfo::CS_BGR32, args[3].AsBool(false),
      args[4].AsInt(0xFFFFFF), args[5].AsInt(0), args[6].AsInt(0), env);
}


/********************************************************************
********************************************************************/


template<typename pixel_t, int bits_per_pixel>
static void draw_colorbars_444(uint8_t *pY8, uint8_t *pU8, uint8_t *pV8, int pitchY, int pitchUV, int w, int h)
{
  pixel_t *pY = reinterpret_cast<pixel_t *>(pY8);
  pixel_t *pU = reinterpret_cast<pixel_t *>(pU8);
  pixel_t *pV = reinterpret_cast<pixel_t *>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
  int chroma_offset_i = 0;
  float chroma_offset_f = 0.0f;
#else
  int chroma_offset_i = 128;
  float chroma_offset_f = 0.5f;
#endif

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);
  typedef typename std::conditional < sizeof(pixel_t) == 4, float, int>::type factor_t; // float support
  factor_t factor = (pixel_t)(sizeof(pixel_t) == 4 ? 1 / 255.0f : 1);

  //                                              LtGrey  Yellow    Cyan   Green Magenta     Red    Blue
  static const BYTE top_two_thirdsY[] = { 0xb4,   0xa2,   0x83,   0x70,   0x54,   0x41,   0x23 };
  static const BYTE top_two_thirdsU[] = { 0x80,   0x2c,   0x9c,   0x48,   0xb8,   0x64,   0xd4 };
  static const BYTE top_two_thirdsV[] = { 0x80,   0x8e,   0x2c,   0x3a,   0xc6,   0xd4,   0x72 };

  int y = 0;

  for (; y * 3 < h * 2; ++y) {
    int x = 0;
    for (int i = 0; i < 7; i++) {
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pY[x] = factor*(top_two_thirdsY[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (top_two_thirdsU[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (top_two_thirdsV[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (top_two_thirdsU[i] << shift);
          pV[x] = factor * (top_two_thirdsV[i] << shift);
        }
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  } //                                                    Blue   Black Magenta   Black    Cyan   Black  LtGrey
  static const BYTE two_thirds_to_three_quartersY[] = { 0x23,   0x10,   0x54,   0x10,   0x83,   0x10,   0xb4 };
  static const BYTE two_thirds_to_three_quartersU[] = { 0xd4,   0x80,   0xb8,   0x80,   0x9c,   0x80,   0x80 };
  static const BYTE two_thirds_to_three_quartersV[] = { 0x72,   0x80,   0xc6,   0x80,   0x2c,   0x80,   0x80 };
  for (; y * 4 < h * 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; i++) {
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pY[x] = factor*(two_thirds_to_three_quartersY[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (two_thirds_to_three_quartersU[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (two_thirds_to_three_quartersV[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (two_thirds_to_three_quartersU[i] << shift);
          pV[x] = factor * (two_thirds_to_three_quartersV[i] << shift);
        }
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  } //                                        -I   white      +Q   Black   -4ire   Black   +4ire   Black
  static const BYTE bottom_quarterY[] = { 0x10,   0xeb,   0x10,   0x10,   0x07,   0x10,   0x19,   0x10 };
  static const BYTE bottom_quarterU[] = { 0x9e,   0x80,   0xae,   0x80,   0x80,   0x80,   0x80,   0x80 };
  static const BYTE bottom_quarterV[] = { 0x5f,   0x80,   0x95,   0x80,   0x80,   0x80,   0x80,   0x80 };
  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i) {
      for (; x < (w*(i + 1) * 5 + 14) / 28; ++x) {
        pY[x] = factor*(bottom_quarterY[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (bottom_quarterU[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (bottom_quarterV[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (bottom_quarterU[i] << shift);
          pV[x] = factor * (bottom_quarterV[i] << shift);
        }
      }
    }
    for (int j = 4; j < 7; ++j) {
      for (; x < (w*(j + 12) + 10) / 21; ++x) {
        pY[x] = factor*(bottom_quarterY[j] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (bottom_quarterU[j] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (bottom_quarterV[j] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (bottom_quarterU[j] << shift);
          pV[x] = factor * (bottom_quarterV[j] << shift);
        }
      }
    }
    for (; x < w; ++x) {
      pY[x] = factor*(bottom_quarterY[7] << shift);
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (bottom_quarterU[7] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (bottom_quarterV[7] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (bottom_quarterU[7] << shift);
        pV[x] = factor * (bottom_quarterV[7] << shift);
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }
}

template<typename pixel_t, int bits_per_pixel>
static void draw_colorbars_420(uint8_t *pY8, uint8_t *pU8, uint8_t *pV8, int pitchY, int pitchUV, int w, int h)
{
  pixel_t *pY = reinterpret_cast<pixel_t *>(pY8);
  pixel_t *pU = reinterpret_cast<pixel_t *>(pU8);
  pixel_t *pV = reinterpret_cast<pixel_t *>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);
  typedef typename std::conditional < sizeof(pixel_t) == 4, float, int>::type factor_t; // float support
  factor_t factor = (pixel_t)(sizeof(pixel_t) == 4 ? 1 / 255.0f : 1);

#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
  int chroma_offset_i = 0;
  float chroma_offset_f = 0.0f;
#else
  int chroma_offset_i = 128;
  float chroma_offset_f = 0.5f;
#endif


//                                                        LtGrey  Yellow    Cyan   Green Magenta     Red    Blue
  static const BYTE top_two_thirdsY[] = { 0xb4,   0xa2,   0x83,   0x70,   0x54,   0x41,   0x23 };
  static const BYTE top_two_thirdsU[] = { 0x80,   0x2c,   0x9c,   0x48,   0xb8,   0x64,   0xd4 };
  static const BYTE top_two_thirdsV[] = { 0x80,   0x8e,   0x2c,   0x3a,   0xc6,   0xd4,   0x72 };
  w >>= 1; // 4:2:0 !!!
  h >>= 1;

  int y = 0;

  for (; y * 3 < h * 2; ++y) {
    int x = 0;
    for (int i = 0; i < 7; i++) {
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pY[x*2+0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = factor*(top_two_thirdsY[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (top_two_thirdsU[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (top_two_thirdsV[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (top_two_thirdsU[i] << shift);
          pV[x] = factor * (top_two_thirdsV[i] << shift);
        }
      }
    }
    pY += pitchY * 2; pU += pitchUV; pV += pitchUV;
  }
  //                                                    Blue   Black Magenta   Black    Cyan   Black  LtGrey
  static const BYTE two_thirds_to_three_quartersY[] = { 0x23,   0x10,   0x54,   0x10,   0x83,   0x10,   0xb4 };
  static const BYTE two_thirds_to_three_quartersU[] = { 0xd4,   0x80,   0xb8,   0x80,   0x9c,   0x80,   0x80 };
  static const BYTE two_thirds_to_three_quartersV[] = { 0x72,   0x80,   0xc6,   0x80,   0x2c,   0x80,   0x80 };
  for (; y * 4 < h * 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; i++) {
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pY[x * 2 + 0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = factor*(two_thirds_to_three_quartersY[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (two_thirds_to_three_quartersU[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (two_thirds_to_three_quartersV[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (two_thirds_to_three_quartersU[i] << shift);
          pV[x] = factor * (two_thirds_to_three_quartersV[i] << shift);
        }
      }
    }
    pY += pitchY * 2; pU += pitchUV; pV += pitchUV;
  }
  //                                        -I   white      +Q   Black   -4ire   Black   +4ire   Black
  static const BYTE bottom_quarterY[] = { 0x10,   0xeb,   0x10,   0x10,   0x07,   0x10,   0x19,   0x10 };
  static const BYTE bottom_quarterU[] = { 0x9e,   0x80,   0xae,   0x80,   0x80,   0x80,   0x80,   0x80 };
  static const BYTE bottom_quarterV[] = { 0x5f,   0x80,   0x95,   0x80,   0x80,   0x80,   0x80,   0x80 };
  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i) {
      for (; x < (w*(i + 1) * 5 + 14) / 28; ++x) {
        pY[x * 2 + 0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = factor*(bottom_quarterY[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (bottom_quarterU[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (bottom_quarterV[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (bottom_quarterU[i] << shift);
          pV[x] = factor * (bottom_quarterV[i] << shift);
        }
      }
    }
    for (int j = 4; j < 7; ++j) {
      for (; x < (w*(j + 12) + 10) / 21; ++x) {
        pY[x * 2 + 0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = factor*(bottom_quarterY[j] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (bottom_quarterU[j] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (bottom_quarterV[j] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (bottom_quarterU[j] << shift);
          pV[x] = factor * (bottom_quarterV[j] << shift);
        }
      }
    }
    for (; x < w; ++x) {
      pY[x * 2 + 0] = pY[x * 2 + 1] = pY[x * 2 + pitchY] = pY[x * 2 + 1 + pitchY] = factor*(bottom_quarterY[7] << shift);
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (bottom_quarterU[7] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (bottom_quarterV[7] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (bottom_quarterU[7] << shift);
        pV[x] = factor * (bottom_quarterV[7] << shift);
      }
    }
    pY += pitchY *2 ; pU += pitchUV; pV += pitchUV;
  }
}

template<typename pixel_t, int bits_per_pixel>
static void draw_colorbarsHD_444(uint8_t *pY8, uint8_t *pU8, uint8_t *pV8, int pitchY, int pitchUV, int w, int h)
{
  pixel_t *pY = reinterpret_cast<pixel_t *>(pY8);
  pixel_t *pU = reinterpret_cast<pixel_t *>(pU8);
  pixel_t *pV = reinterpret_cast<pixel_t *>(pV8);
  pitchY /= sizeof(pixel_t);
  pitchUV /= sizeof(pixel_t);

  const int shift = sizeof(pixel_t) == 4 ? 0 : (bits_per_pixel - 8);
  typedef typename std::conditional < sizeof(pixel_t) == 4, float, int>::type factor_t; // float support
  factor_t factor = (pixel_t)(sizeof(pixel_t) == 4 ? 1 / 255.0f : 1);

#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
  int chroma_offset_i = 0;
  float chroma_offset_f = 0.0f;
#else
  int chroma_offset_i = 128;
  float chroma_offset_f = 0.5f;
#endif


//		Nearest 16:9 pixel exact sizes
//		56*X x 12*Y
//		 728 x  480  ntsc anamorphic
//		 728 x  576  pal anamorphic
//		 840 x  480
//		1008 x  576
//		1288 x  720 <- default
//		1456 x 1080  hd anamorphic
//		1904 x 1080
  int y = 0;

  const int c = (w * 3 + 14) / 28; // 1/7th of 3/4 of width
  const int d = (w - c * 7 + 1) / 2; // remaining 1/8th of width

  const int p4 = (3 * h + 6) / 12; // 3/12th of height
  const int p23 = (h + 6) / 12;  // 1/12th of height
  const int p1 = h - p23 * 2 - p4; // remaining 7/12th of height

  //               75%  Rec709 -- Grey40 Grey75 Yellow  Cyan   Green Magenta  Red   Blue
  static const BYTE pattern1Y[] = { 104,   180,   168,   145,   134,    63,    51,    28 };
  static const BYTE pattern1U[] = { 128,   128,    44,   147,    63,   193,   109,   212 };
  static const BYTE pattern1V[] = { 128,   128,   136,    44,    52,   204,   212,   120 };
  for (; y < p1; ++y) { // Pattern 1
    int x = 0;
    for (; x < d; ++x) {
      pY[x] = factor*(pattern1Y[0] << shift); // 40% Grey
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern1U[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern1V[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern1U[0] << shift);
        pV[x] = factor * (pattern1V[0] << shift);
      }
    }
    for (int i = 1; i < 8; i++) {
      for (int j = 0; j < c; ++j, ++x) {
        pY[x] = factor*(pattern1Y[i] << shift); // 75% Colour bars
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (pattern1U[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (pattern1V[i] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (pattern1U[i] << shift);
          pV[x] = factor * (pattern1V[i] << shift);
        }
      }
    }
    for (; x < w; ++x) {
      pY[x] = factor*(pattern1Y[0] << shift); // 40% Grey
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern1U[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern1V[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern1U[0] << shift);
        pV[x] = factor * (pattern1V[0] << shift);
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }
  //              100% Rec709       Cyan  Blue Yellow  Red    +I Grey75  White
  static const BYTE pattern23Y[] = { 188,   32,  219,   63,   16,  180,  235 };
  static const BYTE pattern23U[] = { 154,  240,   16,  102,   98,  128,  128 };
  static const BYTE pattern23V[] = {  16,  118,  138,  240,  161,  128,  128 };
  for (; y < p1 + p23; ++y) { // Pattern 2
    int x = 0;
    for (; x < d; ++x) {
      pY[x] = factor*(pattern23Y[0] << shift); // 100% Cyan
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern23U[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern23V[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern23U[0] << shift);
        pV[x] = factor * (pattern23V[0] << shift);
      }
    }
    for (; x < c + d; ++x) {
      pY[x] = factor*(pattern23Y[4] << shift); // +I or Grey75 or White ???
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern23U[4] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern23V[4] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern23U[4] << shift);
        pV[x] = factor * (pattern23V[4] << shift);
      }
    }
    for (; x < c * 7 + d; ++x) {
      pY[x] = factor*(pattern23Y[5] << shift); // 75% White
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern23U[5] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern23V[5] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern23U[5] << shift);
        pV[x] = factor * (pattern23V[5] << shift);
      }
    }
    for (; x < w; ++x) {
      pY[x] = factor*(pattern23Y[1] << shift); // 100% Blue
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern23U[1] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern23V[1] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern23U[1] << shift);
        pV[x] = factor * (pattern23V[1] << shift);
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }
  for (; y < p1 + p23 * 2; ++y) { // Pattern 3
    int x = 0;
    for (; x < d; ++x) {
      pY[x] = factor*(pattern23Y[2] << shift); // 100% Yellow
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern23U[2] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern23V[2] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern23U[2] << shift);
        pV[x] = factor * (pattern23V[2] << shift);
      }
    }
    for (int j = 0; j < c * 7; ++j, ++x) { // Y-Ramp
      pY[x] = pixel_t(factor*(16 << shift) + (factor * (220 << shift) * j) / (c * 7));
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (128 - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (128 - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (128 << shift);
        pV[x] = factor * (128 << shift);
      }
    }
    for (; x < w; ++x) {
      pY[x] = factor*(pattern23Y[3] << shift); // 100% Red
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern23U[3] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern23V[3] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern23U[3] << shift);
        pV[x] = factor * (pattern23V[3] << shift);
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  } //                           Grey15 Black White Black   -2% Black   +2% Black   +4% Black
  static const BYTE pattern4Y[] = {  49,   16,  235,   16,   12,   16,   20,   16,   25,   16 };
  static const BYTE pattern4U[] = { 128,  128,  128,  128,  128,  128,  128,  128,  128,  128 };
  static const BYTE pattern4V[] = { 128,  128,  128,  128,  128,  128,  128,  128,  128,  128 };
  static const BYTE pattern4W[] = {   0,    9,   21,   26,   28,   30,   32,   34,   36,   42 }; // in 6th's
  for (; y < h; ++y) { // Pattern 4
    int x = 0;
    for (; x < d; ++x) {
      pY[x] = factor*(pattern4Y[0] << shift); // 15% Grey
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern4U[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern4V[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern4U[0] << shift);
        pV[x] = factor * (pattern4V[0] << shift);
      }
    }
    for (int i = 1; i <= 9; i++) {
      for (; x < d + (pattern4W[i] * c + 3) / 6; ++x) {
        pY[x] = factor*(pattern4Y[i] << shift);
        if (sizeof(pixel_t) == 4) {
          pU[x] = factor * (pattern4U[1] - chroma_offset_i) + (factor_t)chroma_offset_f;
          pV[x] = factor * (pattern4V[1] - chroma_offset_i) + (factor_t)chroma_offset_f;
        }
        else {
          pU[x] = factor * (pattern4U[i] << shift);
          pV[x] = factor * (pattern4V[i] << shift);
        }
      }
    }
    for (; x < w; ++x) {
      pY[x] = factor*(pattern4Y[0] << shift); // 15% Grey
      if (sizeof(pixel_t) == 4) {
        pU[x] = factor * (pattern4U[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
        pV[x] = factor * (pattern4V[0] - chroma_offset_i) + (factor_t)chroma_offset_f;
      }
      else {
        pU[x] = factor * (pattern4U[0] << shift);
        pV[x] = factor * (pattern4V[0] << shift);
      }
    }
    pY += pitchY; pU += pitchUV; pV += pitchUV;
  }
}

static uint16_t rgbcolor8to16(uint8_t color8, int max_pixel_value)
{
  return (uint16_t)(color8 * max_pixel_value / 255);
};


static uint64_t rgbcolor32to64(uint32_t color32_8)
{
  return (uint64_t)(
    (uint64_t(rgbcolor8to16(uint8_t((color32_8 >> 24) & 0xFF), 65535)) << 48) +
    (uint64_t(rgbcolor8to16(uint8_t((color32_8 >> 16) & 0xFF), 65535)) << 32) +
    (uint64_t(rgbcolor8to16(uint8_t((color32_8 >> 8) & 0xFF), 65535)) << 16) +
    (uint64_t(rgbcolor8to16(uint8_t((color32_8) & 0xFF), 65535)))
    );
}

template<typename pixel_t>
static void draw_colorbars_rgb3264(uint8_t *p8, int pitch, int w, int h)
{
  typedef typename std::conditional < sizeof(pixel_t) == 2, uint64_t, uint32_t>::type internal_pixel_t;

  internal_pixel_t *p = reinterpret_cast<internal_pixel_t *>(p8);
  pitch /= sizeof(pixel_t);

  // note we go bottom->top
  static const uint32_t bottom_quarter[] =
  // RGB[16..235]     -I     white        +Q     Black     -4ire     Black     +4ire     Black
  { 0x003a62, 0xebebeb, 0x4b0f7e, 0x101010,  0x070707, 0x101010, 0x191919,  0x101010 }; // Qlum=Ilum=13.4%

  int y = 0;

  bool isRGB32 = sizeof(pixel_t) == 1;
  const int max_pixel_value = isRGB32 ? 255 : 65535;

  for (; y < h / 4; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i) {
      for (; x < (w*(i + 1) * 5 + 14) / 28; ++x) {
        p[x] = (internal_pixel_t)(isRGB32 ? bottom_quarter[i] : rgbcolor32to64(bottom_quarter[i]));
      }
    }
    for (int j = 4; j < 7; ++j) {
      for (; x < (w*(j + 12) + 10) / 21; ++x) {
        p[x] = (internal_pixel_t)(isRGB32 ? bottom_quarter[j] : rgbcolor32to64(bottom_quarter[j]));
      }
    }
    for (; x < w; ++x) {
      p[x] = (internal_pixel_t)(isRGB32 ? bottom_quarter[7] : rgbcolor32to64(bottom_quarter[7]));
    }
    p += pitch;
  }

  static const int two_thirds_to_three_quarters[] =
  // RGB[16..235]   Blue     Black  Magenta      Black      Cyan     Black    LtGrey
  { 0x1010b4, 0x101010, 0xb410b4, 0x101010, 0x10b4b4, 0x101010, 0xb4b4b4 };
  for (; y < h / 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i) {
      for (; x < (w*(i + 1) + 3) / 7; ++x)
        p[x] = (internal_pixel_t)(isRGB32 ? two_thirds_to_three_quarters[i] : rgbcolor32to64(two_thirds_to_three_quarters[i]));
    }
    p += pitch;
  }

  static const int top_two_thirds[] =
  // RGB[16..235] LtGrey    Yellow      Cyan     Green   Magenta       Red      Blue
  { 0xb4b4b4, 0xb4b410, 0x10b4b4, 0x10b410, 0xb410b4, 0xb41010, 0x1010b4 };
  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i) {
      for (; x < (w*(i + 1) + 3) / 7; ++x)
        p[x] = (internal_pixel_t)(isRGB32 ? top_two_thirds[i] : rgbcolor32to64(top_two_thirds[i]));
    }
    p += pitch;
  }
}

template<typename pixel_t, int bits_per_pixel>
static void draw_colorbars_rgbp(uint8_t *pR8, uint8_t *pG8, uint8_t *pB8, int pitch, int w, int h)
{
  pixel_t *pR = reinterpret_cast<pixel_t *>(pR8);
  pixel_t *pG = reinterpret_cast<pixel_t *>(pG8);
  pixel_t *pB = reinterpret_cast<pixel_t *>(pB8);
  pitch /= sizeof(pixel_t);

  // note we go bottom->top
  // original code sample came from packed RGB32 so we are going backward for planar

  pR += (h - 1)*pitch;
  pG += (h - 1)*pitch;
  pB += (h - 1)*pitch;

  static const uint32_t bottom_quarter[] =
    // RGB[16..235]     -I     white        +Q     Black     -4ire     Black     +4ire     Black
  { 0x003a62, 0xebebeb, 0x4b0f7e, 0x101010,  0x070707, 0x101010, 0x191919,  0x101010 }; // Qlum=Ilum=13.4%

  int y = 0;

  const int max_pixel_value = (1 << bits_per_pixel) - 1;

  const bool is8bit = sizeof(pixel_t) == 1;

  for (; y < h / 4; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i) {
      int color = bottom_quarter[i];

      int color_r = (color >> 16) & 0xFF;
      int color_g = (color >> 8) & 0xFF;
      int color_b = (color) & 0xFF;
      if (!is8bit) {
        color_r = rgbcolor8to16(color_r, max_pixel_value);
        color_g = rgbcolor8to16(color_g, max_pixel_value);
        color_b = rgbcolor8to16(color_b, max_pixel_value);
      }
      for (; x < (w*(i + 1) * 5 + 14) / 28; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }
    for (int j = 4; j < 7; ++j) {
      int color = bottom_quarter[j];
      int color_r = (color >> 16) & 0xFF;
      int color_g = (color >> 8) & 0xFF;
      int color_b = (color) & 0xFF;
      if (!is8bit) {
        color_r = rgbcolor8to16(color_r, max_pixel_value);
        color_g = rgbcolor8to16(color_g, max_pixel_value);
        color_b = rgbcolor8to16(color_b, max_pixel_value);
      }

      for (; x < (w*(j + 12) + 10) / 21; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }

    int color = bottom_quarter[7];
    int color_r = (color >> 16) & 0xFF;
    int color_g = (color >> 8) & 0xFF;
    int color_b = (color) & 0xFF;
    if (!is8bit) {
      color_r = rgbcolor8to16(color_r, max_pixel_value);
      color_g = rgbcolor8to16(color_g, max_pixel_value);
      color_b = rgbcolor8to16(color_b, max_pixel_value);
    }
    for (; x < w; ++x) {
      pR[x] = color_r;
      pG[x] = color_g;
      pB[x] = color_b;
    }
    pR -= pitch;
    pG -= pitch;
    pB -= pitch;
  }

  static const int two_thirds_to_three_quarters[] =
    // RGB[16..235]   Blue     Black  Magenta      Black      Cyan     Black    LtGrey
  { 0x1010b4, 0x101010, 0xb410b4, 0x101010, 0x10b4b4, 0x101010, 0xb4b4b4 };
  for (; y < h / 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i) {
      int color = two_thirds_to_three_quarters[i];
      int color_r = (color >> 16) & 0xFF;
      int color_g = (color >> 8) & 0xFF;
      int color_b = (color) & 0xFF;
      if (!is8bit) {
        color_r = rgbcolor8to16(color_r, max_pixel_value);
        color_g = rgbcolor8to16(color_g, max_pixel_value);
        color_b = rgbcolor8to16(color_b, max_pixel_value);
      }
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }
    pR -= pitch;
    pG -= pitch;
    pB -= pitch;
  }

  static const int top_two_thirds[] =
    // RGB[16..235] LtGrey    Yellow      Cyan     Green   Magenta       Red      Blue
  { 0xb4b4b4, 0xb4b410, 0x10b4b4, 0x10b410, 0xb410b4, 0xb41010, 0x1010b4 };

  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i) {
      int color = top_two_thirds[i];
      int color_r = (color >> 16) & 0xFF;
      int color_g = (color >> 8) & 0xFF;
      int color_b = (color) & 0xFF;
      if (!is8bit) {
        color_r = rgbcolor8to16(color_r, max_pixel_value);
        color_g = rgbcolor8to16(color_g, max_pixel_value);
        color_b = rgbcolor8to16(color_b, max_pixel_value);
      }
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }
    pR -= pitch;
    pG -= pitch;
    pB -= pitch;
  }
}

static void draw_colorbars_rgbp_f(uint8_t *pR8, uint8_t *pG8, uint8_t *pB8, int pitch, int w, int h)
{
  float *pR = reinterpret_cast<float *>(pR8);
  float *pG = reinterpret_cast<float *>(pG8);
  float *pB = reinterpret_cast<float *>(pB8);
  pitch /= sizeof(float);

  // note we go bottom->top
  // original code sample from packed RGB32 so we are going backward for planar

  pR += (h - 1)*pitch;
  pG += (h - 1)*pitch;
  pB += (h - 1)*pitch;

  static const uint32_t bottom_quarter[] =
  // RGB[16..235]     -I     white        +Q     Black     -4ire     Black     +4ire     Black
  { 0x003a62, 0xebebeb, 0x4b0f7e, 0x101010,  0x070707, 0x101010, 0x191919,  0x101010 }; // Qlum=Ilum=13.4%

  int y = 0;

  const float factor = 1.0f / 255;

  for (; y < h / 4; ++y) {
    int x = 0;
    for (int i = 0; i < 4; ++i) {
      int color = bottom_quarter[i];

      float color_r = factor * ((color >> 16) & 0xFF);
      float color_g = factor * ((color >> 8) & 0xFF);
      float color_b = factor * ((color) & 0xFF);
      for (; x < (w*(i + 1) * 5 + 14) / 28; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }
    for (int j = 4; j < 7; ++j) {
      int color = bottom_quarter[j];
      float color_r = factor * ((color >> 16) & 0xFF);
      float color_g = factor * ((color >> 8) & 0xFF);
      float color_b = factor * ((color) & 0xFF);

      for (; x < (w*(j + 12) + 10) / 21; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }

    int color = bottom_quarter[7];
    float color_r = factor * ((color >> 16) & 0xFF);
    float color_g = factor * ((color >> 8) & 0xFF);
    float color_b = factor * ((color) & 0xFF);
    for (; x < w; ++x) {
      pR[x] = color_r;
      pG[x] = color_g;
      pB[x] = color_b;
    }
    pR -= pitch;
    pG -= pitch;
    pB -= pitch;
  }

  static const int two_thirds_to_three_quarters[] =
    // RGB[16..235]   Blue     Black  Magenta      Black      Cyan     Black    LtGrey
  { 0x1010b4, 0x101010, 0xb410b4, 0x101010, 0x10b4b4, 0x101010, 0xb4b4b4 };
  for (; y < h / 3; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i) {
      int color = two_thirds_to_three_quarters[i];
      float color_r = factor * ((color >> 16) & 0xFF);
      float color_g = factor * ((color >> 8) & 0xFF);
      float color_b = factor * ((color) & 0xFF);
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }
    pR -= pitch;
    pG -= pitch;
    pB -= pitch;
  }

  static const int top_two_thirds[] =
    // RGB[16..235] LtGrey    Yellow      Cyan     Green   Magenta       Red      Blue
  { 0xb4b4b4, 0xb4b410, 0x10b4b4, 0x10b410, 0xb410b4, 0xb41010, 0x1010b4 };

  for (; y < h; ++y) {
    int x = 0;
    for (int i = 0; i < 7; ++i) {
      int color = top_two_thirds[i];
      float color_r = factor * ((color >> 16) & 0xFF);
      float color_g = factor * ((color >> 8) & 0xFF);
      float color_b = factor * ((color) & 0xFF);
      for (; x < (w*(i + 1) + 3) / 7; ++x) {
        pR[x] = color_r;
        pG[x] = color_g;
        pB[x] = color_b;
      }
    }
    pR -= pitch;
    pG -= pitch;
    pB -= pitch;
  }
}

class ColorBars : public IClip {
  VideoInfo vi;
  PVideoFrame frame;
  SFLOAT *audio;
  unsigned nsamples;
  bool staticframes; // P.F. false: a bit better for synthetic tests. Still defaults to true (one static frame is served)

  enum { Hz = 440 } ;

public:

  ~ColorBars() {
    delete audio;
  }

  ColorBars(int w, int h, const char* pixel_type, bool _staticframes, int type, IScriptEnvironment* env) {
    memset(&vi, 0, sizeof(VideoInfo));
    staticframes = _staticframes; // P.F.
    vi.width = w;
    vi.height = h;
    vi.fps_numerator = 30000;
    vi.fps_denominator = 1001;
    vi.num_frames = 107892;   // 1 hour
    int i_pixel_type = GetPixelTypeFromName(pixel_type);
    vi.pixel_type = i_pixel_type;
    int bits_per_pixel = vi.BitsPerComponent();

    if (type) { // ColorbarsHD
        if (!vi.Is444())
          env->ThrowError("ColorBarsHD: pixel_type must be \"YV24\" or other 4:4:4 video format");
    }
    else if (vi.IsRGB32() || vi.IsRGB64()) {
      // no special check
    }
    else if (vi.IsRGB() && vi.IsPlanar()) { // planar RGB
      // no special check
    }
    else if (vi.IsYUY2()) { // YUY2
        if (w & 1)
          env->ThrowError("ColorBars: YUY2 width must be even!");
    }
    else if (vi.Is420()) { // 4:2:0
        if ((w & 1) || (h & 1))
          env->ThrowError("ColorBars: for 4:2:0 both height and width must be even!");
    }
    else if (vi.Is444()) { // 4:4:4
        // no special check
    }
    else {
      env->ThrowError("ColorBars: pixel_type must be \"RGB32\", \"RGB64\", \"YUY2\", planar RGB, 4:2:0 or 4:4:4 formats");
    }
    vi.sample_type = SAMPLE_FLOAT;
    vi.nchannels = 2;
    vi.audio_samples_per_second = 48000;
    vi.num_audio_samples=vi.AudioSamplesFromFrames(vi.num_frames);

    frame = env->NewVideoFrame(vi);
    uint32_t* p = (uint32_t *)frame->GetWritePtr();
    const int pitch = frame->GetPitch()/4;

    int y = 0;

	// HD colorbars arib_std_b28
	// Rec709 yuv values calculated by jmac698, Jan 2010, for Midzuki
	if (type) { // ColorbarsHD
		BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
		BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
		BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
		const int pitchY  = frame->GetPitch(PLANAR_Y);
		const int pitchUV = frame->GetPitch(PLANAR_U);

    switch (bits_per_pixel) {
    case 8: draw_colorbarsHD_444<uint8_t, 8>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 10: draw_colorbarsHD_444<uint16_t, 10>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 12: draw_colorbarsHD_444<uint16_t, 12>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 14: draw_colorbarsHD_444<uint16_t, 14>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 16: draw_colorbarsHD_444<uint16_t, 16>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 32: draw_colorbarsHD_444<float, 8 /* n/a */>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    }

	}
	// Rec. ITU-R BT.801-1
  else if (vi.Is444()) {
    BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
    BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
    BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
    const int pitchY = frame->GetPitch(PLANAR_Y);
    const int pitchUV = frame->GetPitch(PLANAR_U);

    switch (bits_per_pixel) {
    case 8: draw_colorbars_444<uint8_t, 8>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 10: draw_colorbars_444<uint16_t, 10>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 12: draw_colorbars_444<uint16_t, 12>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 14: draw_colorbars_444<uint16_t, 14>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 16: draw_colorbars_444<uint16_t, 16>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 32: draw_colorbars_444<float, 8 /* n/a */>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    }
	}
  else if (vi.IsRGB() && vi.IsPlanar()) {
    BYTE* pG = (BYTE*)frame->GetWritePtr(PLANAR_G);
    BYTE* pB = (BYTE*)frame->GetWritePtr(PLANAR_B);
    BYTE* pR = (BYTE*)frame->GetWritePtr(PLANAR_R);
    const int pitch = frame->GetPitch(PLANAR_G);

    switch (bits_per_pixel) {
    case 8: draw_colorbars_rgbp<uint8_t, 8>(pR, pG, pB, pitch, w, h); break;
    case 10: draw_colorbars_rgbp<uint16_t, 10>(pR, pG, pB, pitch, w, h); break;
    case 12: draw_colorbars_rgbp<uint16_t, 12>(pR, pG, pB, pitch, w, h); break;
    case 14: draw_colorbars_rgbp<uint16_t, 14>(pR, pG, pB, pitch, w, h); break;
    case 16: draw_colorbars_rgbp<uint16_t, 16>(pR, pG, pB, pitch, w, h); break;
    case 32: draw_colorbars_rgbp_f(pR, pG, pB, pitch, w, h); break;
    }
  }
  else if (vi.IsRGB32() || vi.IsRGB64()) {
    switch (bits_per_pixel) {
    case 8: draw_colorbars_rgb3264<uint8_t>((uint8_t *)p, pitch, w, h); break;
    case 16: draw_colorbars_rgb3264<uint16_t>((uint8_t *)p, pitch, w, h); break;
    }
	}
  else if (vi.IsYUY2()) {
    static const unsigned int top_two_thirds[] =
//                LtGrey      Yellow        Cyan       Green     Magenta         Red        Blue
    { 0x80b480b4, 0x8ea22ca2, 0x2c839c83, 0x3a704870, 0xc654b854, 0xd4416441, 0x7223d423 }; //VYUY
    w >>= 1;
    for (; y * 3 < h * 2; ++y) {
      int x = 0;
      for (int i = 0; i < 7; i++) {
        for (; x < (w*(i + 1) + 3) / 7; ++x)
          p[x] = top_two_thirds[i];
      }
      p += pitch;
    }

    static const unsigned  int two_thirds_to_three_quarters[] =
//                 Blue        Black     Magenta       Black        Cyan       Black      LtGrey
    { 0x7223d423, 0x80108010, 0xc654b854, 0x80108010, 0x2c839c83, 0x80108010, 0x80b480b4 }; //VYUY
    for (; y * 4 < h * 3; ++y) {
      int x = 0;
      for (int i = 0; i < 7; i++) {
        for (; x < (w*(i + 1) + 3) / 7; ++x)
          p[x] = two_thirds_to_three_quarters[i];
      }
      p += pitch;
    }

    static const unsigned int bottom_quarter[] =
//                    -I       white          +Q       Black       -4ire       Black       +4ire       Black
    { 0x5f109e10, 0x80eb80eb, 0x9510ae10, 0x80108010, 0x80078007, 0x80108010, 0x80198019, 0x80108010 }; //VYUY
    for (; y < h; ++y) {
      int x = 0;
      for (int i = 0; i < 4; ++i) {
        for (; x < (w*(i + 1) * 5 + 14) / 28; ++x)
          p[x] = bottom_quarter[i];
      }
      for (int j = 4; j < 7; ++j) {
        for (; x < (w*(j + 12) + 10) / 21; ++x)
          p[x] = bottom_quarter[j];
      }
      for (; x < w; ++x)
        p[x] = bottom_quarter[7];
      p += pitch;
    }
  }
  else if (vi.Is420()) {

    BYTE* pY = (BYTE*)frame->GetWritePtr(PLANAR_Y);
    BYTE* pU = (BYTE*)frame->GetWritePtr(PLANAR_U);
    BYTE* pV = (BYTE*)frame->GetWritePtr(PLANAR_V);
    const int pitchY = frame->GetPitch(PLANAR_Y);
    const int pitchUV = frame->GetPitch(PLANAR_U);

    switch (bits_per_pixel) {
    case 8: draw_colorbars_420<uint8_t, 8>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 10: draw_colorbars_420<uint16_t, 10>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 12: draw_colorbars_420<uint16_t, 12>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 14: draw_colorbars_420<uint16_t, 14>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 16: draw_colorbars_420<uint16_t, 16>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    case 32: draw_colorbars_420<float, 8 /* n/a */>(pY, pU, pV, pitchY, pitchUV, w, h); break;
    }
  }

  if (vi.IsYUVA() || vi.IsPlanarRGBA()) {
    // initialize planar alpha planes with full transparency
    BYTE *dstp = frame->GetWritePtr(PLANAR_A);
    int pitch = frame->GetPitch(PLANAR_A);
    int height = frame->GetHeight(PLANAR_A);
    switch (bits_per_pixel) {
    case 8: fill_plane<uint8_t>(dstp, height, pitch, 255); break;
    case 10: case 12: case 14: case 16: fill_plane<uint16_t>(dstp, height, pitch, (1 << bits_per_pixel) - 1); break;
    case 32: fill_plane<float>(dstp, height, pitch, 1.0f); break;
    }
  }

  // Generate Audio buffer
  {
    unsigned x = vi.audio_samples_per_second, y = Hz;
    while (y) {     // find gcd
      unsigned t = x%y; x = y; y = t;
    }
    nsamples = vi.audio_samples_per_second / x; // 1200
    const unsigned ncycles = Hz / x; // 11

    audio = new(std::nothrow) SFLOAT[nsamples];
    if (!audio)
      env->ThrowError("ColorBars: insufficient memory");

    const double add_per_sample = ncycles / (double)nsamples;
    double second_offset = 0.0;
    for (unsigned i = 0; i < nsamples; i++) {
      audio[i] = (SFLOAT)sin(PI * 2.0 * second_offset);
      second_offset += add_per_sample;
    }
  }
  }

  // By the new "staticframes" parameter: colorbars we generate (copy) real new frames instead of a ready-to-use static one
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env)
  {
    _RPT1(0, "ColorBars::GetFrame %d\n", n);
    if (staticframes)
      return frame; // original default method returns precomputed static frame.
    else {
      PVideoFrame result = env->NewVideoFrame(vi);
      _RPT3(0, "ColorBars GetFrame origframe=%p vfb=%p newframe=%p\n", (void *)frame, (void *)result->GetFrameBuffer(), (void *)result);
      env->BitBlt(result->GetWritePtr(), result->GetPitch(), frame->GetReadPtr(), frame->GetPitch(), frame->GetRowSize(), frame->GetHeight());
      env->BitBlt(result->GetWritePtr(PLANAR_V), result->GetPitch(PLANAR_V), frame->GetReadPtr(PLANAR_V), frame->GetPitch(PLANAR_V), frame->GetRowSize(PLANAR_V), frame->GetHeight(PLANAR_V));
      env->BitBlt(result->GetWritePtr(PLANAR_U), result->GetPitch(PLANAR_U), frame->GetReadPtr(PLANAR_U), frame->GetPitch(PLANAR_U), frame->GetRowSize(PLANAR_U), frame->GetHeight(PLANAR_U));
      env->BitBlt(result->GetWritePtr(PLANAR_A), result->GetPitch(PLANAR_A), frame->GetReadPtr(PLANAR_U), frame->GetPitch(PLANAR_A), frame->GetRowSize(PLANAR_A), frame->GetHeight(PLANAR_A));
      return result;
    }
  }

  bool __stdcall GetParity(int n) { return false; }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  int __stdcall SetCacheHints(int cachehints,int frame_range)
  {
      switch (cachehints)
      {
      case CACHE_GET_MTMODE:
          return MT_NICE_FILTER;
      case CACHE_DONT_CACHE_ME:
          return 1;
      default:
          return 0;
      }
  };

  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {
#if 1
	const int d_mod = vi.audio_samples_per_second*2;
	float* samples = (float*)buf;

	unsigned j = (unsigned)(start % nsamples);
	for (int i=0;i<count;i++) {
	  samples[i*2]=audio[j];
	  if (((start+i)%d_mod)>vi.audio_samples_per_second) {
		samples[i*2+1]=audio[j];
	  } else {
		samples[i*2+1]=0;
	  }
	  if (++j >= nsamples) j = 0;
	}
#else
    __int64 Hz=440;
    // Calculate what start equates in cycles.
    // This is the number of cycles (rounded down) that has already been taken.
    __int64 startcycle = (start*Hz) /  vi.audio_samples_per_second;

    // Move offset down - this is to avoid float rounding errors
    int start_offset = (int)(start - ((startcycle * vi.audio_samples_per_second) / Hz));

    double add_per_sample=Hz/(double)vi.audio_samples_per_second;
    double second_offset=((double)start_offset*add_per_sample);
    int d_mod=vi.audio_samples_per_second*2;
    float* samples = (float*)buf;

    for (int i=0;i<count;i++) {
        samples[i*2]=(SFLOAT)sin(PI * 2.0 * second_offset);
        if (((start+i)%d_mod)>vi.audio_samples_per_second) {
          samples[i*2+1]=samples[i*2];
        } else {
          samples[i*2+1]=0;
        }
        second_offset+=add_per_sample;
    }
#endif
  }

  static AVSValue __cdecl Create(AVSValue args, void* _type, IScriptEnvironment* env) {
    const int type = (int)(size_t)_type;

    return new ColorBars(args[0].AsInt(   type ? 1288 : 640),
                         args[1].AsInt(   type ?  720 : 480),
                         args[2].AsString(type ? "YV24" : "RGB32"),
                         args[3].AsBool(true), // new staticframes parameter
                         type, env);
  }
};


/********************************************************************
********************************************************************/



/********************************************************************
********************************************************************/

#if 0
class QuickTimeSource : public IClip {
public:
  QuickTimeSource() {
//    extern void foo();
//    foo();
  }
  PVideoFrame GetFrame(int n, IScriptEnvironment* env) {}
  void GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {}
  const VideoInfo& GetVideoInfo() {}
  bool GetParity(int n) { return false; }

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env) {
    return new QuickTimeSource;
  }
};
#endif

/********************************************************************
********************************************************************/

AVSValue __cdecl Create_SegmentedSource(AVSValue args, void* use_directshow, IScriptEnvironment* env) {
  bool bAudio = !use_directshow && args[1].AsBool(true);
  const char* pixel_type = 0;
  const char* fourCC = 0;
  int vtrack = 0;
  int atrack = 0;
  const int inv_args_count = args.ArraySize();
  AVSValue inv_args[9];
  if (!use_directshow) {
    pixel_type = args[2].AsString("");
    fourCC = args[3].AsString("");
    vtrack = args[4].AsInt(0);
    atrack = args[5].AsInt(0);
  }
  else {
    for (int i=1; i<inv_args_count ;i++)
      inv_args[i] = args[i];
  }
  args = args[0];
  PClip result = 0;
  const char* error_msg=0;
  for (int i = 0; i < args.ArraySize(); ++i) {
    char basename[260];
    strcpy(basename, args[i].AsString());
    char* extension = strrchr(basename, '.');
    if (extension)
      *extension++ = 0;
    else
      extension[0] = 0;
    for (int j = 0; j < 100; ++j) {
      char filename[260];
      wsprintf(filename, "%s.%02d.%s", basename, j, extension);
      if (GetFileAttributes(filename) != (DWORD)-1) {   // check if file exists
          PClip clip;
        try {
          if (use_directshow) {
            inv_args[0] = filename;
            clip = env->Invoke("DirectShowSource",AVSValue(inv_args, inv_args_count)).AsClip();
          } else {
            clip =  (IClip*)(new AVISource(filename, bAudio, pixel_type, fourCC, vtrack, atrack, AVISource::MODE_NORMAL, env));
          }
          result = !result ? clip : new_Splice(result, clip, false, env);
        } catch (const AvisynthError &e) {
          error_msg=e.msg;
        }
      }
    }
  }
  if (!result) {
    if (!error_msg) {
      env->ThrowError("Segmented%sSource: no files found!", use_directshow ? "DirectShow" : "AVI");
    } else {
      env->ThrowError("Segmented%sSource: decompressor returned error:\n%s!", use_directshow ? "DirectShow" : "AVI",error_msg);
    }
  }
  return result;
}

/**********************************************************
 *                         TONE                           *
 **********************************************************/
class SampleGenerator {
public:
  SampleGenerator() {}
  virtual SFLOAT getValueAt(double where) {return 0.0f;}
};

class SineGenerator : public SampleGenerator {
public:
  SineGenerator() {}
  SFLOAT getValueAt(double where) {return (SFLOAT)sin(PI * where * 2.0);}
};


class NoiseGenerator : public SampleGenerator {
public:
  NoiseGenerator() {
    srand( (unsigned)time( NULL ) );
  }

  SFLOAT getValueAt(double where) {return (float) rand()*(2.0f/RAND_MAX) -1.0f;}
};

class SquareGenerator : public SampleGenerator {
public:
  SquareGenerator() {}

  SFLOAT getValueAt(double where) {
    if (where<=0.5) {
      return 1.0f;
    } else {
      return -1.0f;
    }
  }
};

class TriangleGenerator : public SampleGenerator {
public:
  TriangleGenerator() {}

  SFLOAT getValueAt(double where) {
    if (where<=0.25) {
      return (SFLOAT)(where*4.0);
    } else if (where<=0.75) {
      return (SFLOAT)((-4.0*(where-0.50)));
    } else {
      return (SFLOAT)((4.0*(where-1.00)));
    }
  }
};

class SawtoothGenerator : public SampleGenerator {
public:
  SawtoothGenerator() {}

  SFLOAT getValueAt(double where) {
    return (SFLOAT)(2.0*(where-0.5));
  }
};


class Tone : public IClip {
  VideoInfo vi;
  SampleGenerator *s;
  const double freq;            // Frequency in Hz
  const double samplerate;      // Samples per second
  const int ch;                 // Number of channels
  const double add_per_sample;  // How much should we add per sample in seconds
  const float level;

public:

  Tone(double _length, double _freq, int _samplerate, int _ch, const char* _type, float _level, IScriptEnvironment* env):
             freq(_freq), samplerate(_samplerate), ch(_ch), add_per_sample(_freq/_samplerate), level(_level) {
    memset(&vi, 0, sizeof(VideoInfo));
    vi.sample_type = SAMPLE_FLOAT;
    vi.nchannels = _ch;
    vi.audio_samples_per_second = _samplerate;
    vi.num_audio_samples=(__int64)(_length*vi.audio_samples_per_second+0.5);

    if (!lstrcmpi(_type, "Sine"))
      s = new SineGenerator();
    else if (!lstrcmpi(_type, "Noise"))
      s = new NoiseGenerator();
    else if (!lstrcmpi(_type, "Square"))
      s = new SquareGenerator();
    else if (!lstrcmpi(_type, "Triangle"))
      s = new TriangleGenerator();
    else if (!lstrcmpi(_type, "Sawtooth"))
      s = new SawtoothGenerator();
    else if (!lstrcmpi(_type, "Silence"))
      s = new SampleGenerator();
    else
      env->ThrowError("Tone: Type was not recognized!");
  }

  void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {

    // Where in the cycle are we in?
    const double cycle = (freq * start) / samplerate;
    double period_place = cycle - floor(cycle);

    SFLOAT* samples = (SFLOAT*)buf;

    for (int i=0;i<count;i++) {
      SFLOAT v = s->getValueAt(period_place) * level;
      for (int o=0;o<ch;o++) {
        samples[o+i*ch] = v;
      }
      period_place += add_per_sample;
      if (period_place >= 1.0)
        period_place -= floor(period_place);
    }
  }

  static AVSValue __cdecl Create(AVSValue args, void*, IScriptEnvironment* env)
  {
	  return new Tone(args[0].AsFloat(10.0f), args[1].AsFloat(440.0f), args[2].AsInt(48000),
          args[3].AsInt(2), args[4].AsString("Sine"), args[5].AsFloatf(1.0f), env);
  }

  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) { return NULL; }
  const VideoInfo& __stdcall GetVideoInfo() { return vi; }
  bool __stdcall GetParity(int n) { return false; }
  int __stdcall SetCacheHints(int cachehints,int frame_range) { return 0; };

};


AVSValue __cdecl Create_Version(AVSValue args, void*, IScriptEnvironment* env) {
  return Create_MessageClip(AVS_FULLVERSION AVS_COPYRIGHT, -1, -1, VideoInfo::CS_BGR24, false, 0xECF2BF, 0, 0x404040, env);
}


extern const AVSFunction Source_filters[] = {
  { "AVISource",     BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i", AVISource::Create, (void*) AVISource::MODE_NORMAL },
  { "AVIFileSource", BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i", AVISource::Create, (void*) AVISource::MODE_AVIFILE },
  { "WAVSource",     BUILTIN_FUNC_PREFIX, "s+", AVISource::Create, (void*) AVISource::MODE_WAV },
  { "OpenDMLSource", BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i", AVISource::Create, (void*) AVISource::MODE_OPENDML },
  { "SegmentedAVISource", BUILTIN_FUNC_PREFIX, "s+[audio]b[pixel_type]s[fourCC]s[vtrack]i[atrack]i", Create_SegmentedSource, (void*)0 },
  { "SegmentedDirectShowSource", BUILTIN_FUNC_PREFIX,
// args               0      1      2       3       4            5          6         7            8
                     "s+[fps]f[seek]b[audio]b[video]b[convertfps]b[seekzero]b[timeout]i[pixel_type]s",
                     Create_SegmentedSource, (void*)1 },
// args             0         1       2        3            4     5                 6            7        8             9       10          11     12
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c", Create_BlankClip },
#ifdef NEW_AVSVALUE
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c[colors]a", Create_BlankClip },
  { "BlankClip", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c[colors]a", Create_BlankClip },
#endif
  { "Blackness", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[stereo]b[sixteen_bit]b[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "Blackness", BUILTIN_FUNC_PREFIX, "[]c*[length]i[width]i[height]i[pixel_type]s[fps]f[fps_denominator]i[audio_rate]i[channels]i[sample_type]s[color]i[color_yuv]i[clip]c", Create_BlankClip },
  { "MessageClip", BUILTIN_FUNC_PREFIX, "s[width]i[height]i[shrink]b[text_color]i[halo_color]i[bg_color]i", Create_MessageClip },
  { "ColorBars", BUILTIN_FUNC_PREFIX, "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, (void*)0 },
  { "ColorBarsHD", BUILTIN_FUNC_PREFIX, "[width]i[height]i[pixel_type]s[staticframes]b", ColorBars::Create, (void*)1 },
  { "Tone", BUILTIN_FUNC_PREFIX, "[length]f[frequency]f[samplerate]i[channels]i[type]s[level]f", Tone::Create },

  { "Version", BUILTIN_FUNC_PREFIX, "", Create_Version },

  { NULL }
};
