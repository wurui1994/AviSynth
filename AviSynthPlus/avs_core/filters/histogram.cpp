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


#include "histogram.h"
#include "../core/info.h"
#include "../core/internal.h"
#include "../convert/convert_audio.h"
#include <avs/win.h>
#include <memory>
#include <avs/minmax.h>
#include <cstdio>
#include <cmath>
#include <stdint.h>


#define PI        3.141592653589793


/********************************************************************
***** Declare index of new filters for Avisynth's filter engine *****
********************************************************************/

extern const AVSFunction Histogram_filters[] = {
  { "Histogram", BUILTIN_FUNC_PREFIX, "c[mode]s[].[bits]i[keepsource]b[markers]b", Histogram::Create },   // src clip, avs+ new bits, keepsource and markers param
  { 0 }
};




/***********************************
 *******   Histogram Filter   ******
 **********************************/

Histogram::Histogram(PClip _child, Mode _mode, AVSValue _option, int _show_bits, bool _keepsource, bool _markers, IScriptEnvironment* env)
  : GenericVideoFilter(_child), mode(_mode), option(_option), show_bits(_show_bits), keepsource(_keepsource), markers(_markers)
{
  bool optionValid = false;

  pixelsize = vi.ComponentSize();
  bits_per_pixel = vi.BitsPerComponent();

  if(show_bits < 8 || show_bits>12)
    env->ThrowError("Histogram: bits parameter can only be 8, 9 .. 12");

  // until all histogram is ported
  bool non8bit = show_bits != 8 || bits_per_pixel != 8;

  if (non8bit && mode != ModeClassic && mode != ModeLevels && mode != ModeColor)
  {
    env->ThrowError("Histogram: this histogram type is available only for 8 bit formats and parameters");
  }

  origwidth = vi.width;
  origheight = vi.height;

  if (mode == ModeClassic) {
    if (!vi.IsYUV() && !vi.IsYUVA())
      env->ThrowError("Histogram: YUV(A) data only");
    if(keepsource)
      vi.width += (1 << show_bits);
    else
      vi.width = (1 << show_bits);
  }

  if (mode == ModeLevels) {
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Levels mode only available in PLANAR.");
    }
    if (vi.IsY()) {
      env->ThrowError("Histogram: Levels mode not available in greyscale.");
    }
    optionValid = option.IsFloat();
    const double factor = option.AsDblDef(100.0); // Population limit % factor
    if (factor < 0.0 || factor > 100.0) {
      env->ThrowError("Histogram: Levels population clamping must be between 0 and 100%");
    }
    // put diagram on the right side
    if (keepsource) {
      vi.width += (1 << show_bits); // 256 for 8 bit
      vi.height = max(256, vi.height);
    }
    else { // or keep it alone
      vi.width = (1 << show_bits);
      vi.height = 256;
    }
  }

  if (mode == ModeColor) {
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Color mode only available in PLANAR.");
    }
    if (vi.IsY()) {
      env->ThrowError("Histogram: Color mode not available in greyscale.");
    }
    // put diagram on the right side
    if (keepsource) {
      vi.width += (1 << show_bits); // 256 for 8 bit
      vi.height = max(1 << show_bits, vi.height);
    }
    else {
      vi.width = (1 << show_bits); // 256 for 8 bit
      vi.height = 1 << show_bits;
    }
  }

  if (mode == ModeColor2) {
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Color2 mode only available in PLANAR.");
    }
    if (vi.IsY()) {
      env->ThrowError("Histogram: Color2 mode not available in greyscale.");
    }

    // put circle on the right side
    if (keepsource) {
      vi.width += (1 << show_bits); // 256 for 8 bit
      vi.height = max((1 << show_bits), vi.height); // yes, height can change
    }
    else {
      vi.width = (1 << show_bits); // 256 for 8 bit
      vi.height = (1 << show_bits); // yes, height can change
    }
    int half = 1 << (show_bits - 1); // 127
    int R = half - 1; // 126
    for (int y=0; y<24; y++) { // just inside the big circle
      deg15c[y] = (int) ( R*cos(y*PI/12.) + 0.5) + half;
      deg15s[y] = (int) (-R*sin(y*PI/12.) + 0.5) + half;
    }
  }

  if (mode == ModeLuma && !vi.IsYUV() && !vi.IsYUVA()) {
      env->ThrowError("Histogram: Luma mode only available in YUV(A).");
  }

  if ((mode == ModeStereoY8)||(mode == ModeStereo)||(mode == ModeOverlay)) {

    child->SetCacheHints(CACHE_AUDIO,4096*1024);

    if (!vi.HasVideo()) {
      mode = ModeStereo; // force mode to ModeStereo.
      vi.fps_numerator = 25;
      vi.fps_denominator = 1;
      vi.num_frames = vi.FramesFromAudioSamples(vi.num_audio_samples);
    }
    if (mode == ModeOverlay)  {
      if (keepsource) {
        vi.height = max(512, vi.height);
        vi.width = max(512, vi.width);
      }
      else {
        vi.height = 512;
        vi.width = 512;
      }
      if (!vi.IsPlanar()) {
        env->ThrowError("Histogram: StereoOverlay requires a Planar video format (YV12, YV24, etc).");
      }
    } else if (mode == ModeStereoY8) {
      vi.pixel_type = VideoInfo::CS_Y8;
      vi.height = 512;
      vi.width = 512;
    } else {
      vi.pixel_type = VideoInfo::CS_YV12;
      vi.height = 512;
      vi.width = 512;
    }
    if (!vi.HasAudio()) {
      env->ThrowError("Histogram: Stereo mode requires samples!");
    }
    if (vi.AudioChannels() != 2) {
      env->ThrowError("Histogram: Stereo mode only works on two audio channels.");
    }

     aud_clip = ConvertAudio::Create(child,SAMPLE_INT16,SAMPLE_INT16);
  }

  if (mode == ModeAudioLevels) {
    child->SetCacheHints(CACHE_AUDIO, 4096*1024);
    if (!vi.IsPlanar()) {
      env->ThrowError("Histogram: Audiolevels mode only available in planar YUV.");
    }
    if (vi.IsY8()) {
      env->ThrowError("Histogram: AudioLevels mode not available in Y8.");
    }

    aud_clip = ConvertAudio::Create(child, SAMPLE_INT16, SAMPLE_INT16);
  }

  if (!optionValid && option.Defined())
    env->ThrowError("Histogram: Unknown optional value.");
}

PVideoFrame __stdcall Histogram::GetFrame(int n, IScriptEnvironment* env)
{
  switch (mode) {
  case ModeClassic:
    return DrawModeClassic(n, env);
  case ModeLevels:
    return DrawModeLevels(n, env);
  case ModeColor:
    return DrawModeColor(n, env);
  case ModeColor2:
    return DrawModeColor2(n, env);
  case ModeLuma:
    return DrawModeLuma(n, env);
  case ModeStereoY8:
  case ModeStereo:
    return DrawModeStereo(n, env);
  case ModeOverlay:
    return DrawModeOverlay(n, env);
  case ModeAudioLevels:
    return DrawModeAudioLevels(n, env);
  }
  return DrawModeClassic(n, env);
}

inline void MixLuma(BYTE &src, int value, int alpha) {
  src = src + BYTE(((value - (int)src) * alpha) >> 8);
}

PVideoFrame Histogram::DrawModeAudioLevels(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  env->MakeWritable(&src);
  const int w = src->GetRowSize();
  const int channels = vi.AudioChannels();

  int bar_w = 60;  // Must be divideable by 4 (for subsampling)
  int total_width = (1+channels*2)*bar_w; // Total width in pixels.

  if (total_width > w) {
    bar_w = ((w / (1+channels*2)) / 4)* 4;
  }
  total_width = (1+channels*2)*bar_w; // Total width in pixels.
  int bar_h = vi.height;

  // Get audio for current frame.
  const __int64 start = vi.AudioSamplesFromFrames(n);
  const int count = (int)(vi.AudioSamplesFromFrames(1));
  signed short* samples = static_cast<signed short*>(_alloca(sizeof(signed short)* count * channels));

  aud_clip->GetAudio(samples, max(0ll,start), count, env);

  // Find maximum volume and rms.
  int*     channel_max = static_cast<int*>(_alloca(channels * sizeof(int)));
  __int64* channel_rms = static_cast<__int64*>(_alloca(channels * sizeof(__int64)));;

  const int c = count*channels;
  for (int ch = 0; ch<channels; ch++) {
    int max_vol = 0;
    __int64 rms_vol = 0;

    for (int i = ch; i < c; i += channels) {
      int sample = samples[i];
      sample *= sample;
      rms_vol += sample;
      max_vol = max(max_vol, sample);
    }
    channel_max[ch] = max_vol;
    channel_rms[ch] = rms_vol;
  }

  // Draw bars
  BYTE* srcpY = src->GetWritePtr(PLANAR_Y);
  int Ypitch = src->GetPitch(PLANAR_Y);
  BYTE* srcpU = src->GetWritePtr(PLANAR_U);
  BYTE* srcpV = src->GetWritePtr(PLANAR_V);
  int UVpitch = src->GetPitch(PLANAR_U);
  int xSubS = vi.GetPlaneWidthSubsampling(PLANAR_U);
  int ySubS = vi.GetPlaneHeightSubsampling(PLANAR_U);

  // Draw Dotted lines
  const int lines = 16;  // Line every 6dB  (96/6)
  int lines_y[lines];
  float line_every = (float)bar_h / (float)lines;
  char text[32];
  for (int i=0; i<lines; i++) {
    lines_y[i] = (int)(line_every*i);
    if (!(i&1)) {
      _snprintf(text, sizeof(text), "%3ddB", -i*6);
      DrawStringPlanar(src, 0, i ? lines_y[i]-10 : 0, text);
    }
  }
  for (int x=bar_w-16; x<total_width-bar_w+16; x++) {
    if (!(x&12)) {
      for (int i=0; i<lines; i++) {
        srcpY[x+lines_y[i]*Ypitch] = 200;
      }
    }
  }

  for (int ch = 0; ch<channels; ch++) {
    int max = channel_max[ch];
    double ch_db = 96;
    if (max > 0) {
      ch_db = -8.685889638/2.0 * log((double)max/(32768.0*32768.0));
    }

    __int64 rms = channel_rms[ch] / count;
    double ch_rms = 96;
    if (rms > 0) {
      ch_rms = -8.685889638/2.0 * log((double)rms/(32768.0*32768.0));
    }

    int x_pos = ((ch*2)+1)*bar_w+8;
    int x_end = x_pos+bar_w-8;
    int y_pos = (int)(((double)bar_h*ch_db) / 96.0);
    int y_mid = (int)(((double)bar_h*ch_rms) / 96.0);
    int y_end = src->GetHeight(PLANAR_Y);
    // Luma                          Red   Blue
    int y_val = (max>=32767*32767) ? 78 : 90;
    int a_val = (max>=32767*32767) ? 96 : 128;
    for (int y = y_pos; y<y_mid; y++) {
      for (int x = x_pos; x < x_end; x++) {
        MixLuma(srcpY[x+y*Ypitch], y_val, a_val);
      }
    } //                      Yellow Green
    y_val = (max>=32767*32767) ? 216 : 137;
    a_val = (max>=32767*32767) ? 160 : 128;
    for (int y = y_mid; y<y_end; y++) {
      for (int x = x_pos; x < x_end; x++) {
        MixLuma(srcpY[x+y*Ypitch], y_val, a_val);
      }
    }
    // Chroma
    x_pos >>= xSubS;
    x_end >>= xSubS;
    y_pos >>= ySubS;
    y_mid >>= ySubS;
    y_end = src->GetHeight(PLANAR_U);//Red  Blue
    BYTE u_val = (max>=32767*32767) ? 92 : 212;
    BYTE v_val = (max>=32767*32767) ? 233 : 114;
    for (int y = y_pos; y<y_mid; y++) {
      for (int x = x_pos; x < x_end; x++) {
        srcpU[x+y*UVpitch] = u_val;
        srcpV[x+y*UVpitch] = v_val;
      }
    } //                      Yellow Green
    u_val = (max>=32767*32767) ? 44 : 58;
    v_val = (max>=32767*32767) ? 142 : 40;
    for (int y = y_mid; y<y_end; y++) {
      for (int x = x_pos; x < x_end; x++) {
        srcpU[x+y*UVpitch] = u_val;
        srcpV[x+y*UVpitch] = v_val;
      }
    }
    // Draw text
    _snprintf(text, sizeof(text), "%6.2fdB", (float)-ch_db);
    DrawStringPlanar(src, ((ch*2)+1)*bar_w, vi.height-40, text);
    _snprintf(text, sizeof(text), "%6.2fdB", (float)-ch_rms);
    DrawStringPlanar(src, ((ch*2)+1)*bar_w, vi.height-20, text);

  }

  return src;
}

PVideoFrame Histogram::DrawModeOverlay(int n, IScriptEnvironment* env) {
  auto env2 = static_cast<IScriptEnvironment2*>(env);
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);

  __int64 start = vi.AudioSamplesFromFrames(n);
  __int64 end = vi.AudioSamplesFromFrames(n+1);
  __int64 count = end-start;
  signed short* samples = static_cast<signed short*>(
    env2->Allocate((int)count * vi.AudioChannels() * sizeof(unsigned short), 8, AVS_POOLED_ALLOC)
  );
  if (!samples) {
	  env2->ThrowError("Histogram: Could not reserve memory.");
  }

  int h = dst->GetHeight();
  int imgSize = h*dst->GetPitch();
  BYTE* dstp = dst->GetWritePtr();
  int p = dst->GetPitch(PLANAR_Y);

  if ((src->GetHeight()<dst->GetHeight()) || (src->GetRowSize() < dst->GetRowSize())) {
    memset(dstp, 16, imgSize);
    int imgSizeU = dst->GetHeight(PLANAR_U) * dst->GetPitch(PLANAR_U);
    if (imgSizeU) {
      memset(dst->GetWritePtr(PLANAR_U), 128, imgSizeU);
      memset(dst->GetWritePtr(PLANAR_V), 128, imgSizeU);
    }
  }

  env->BitBlt(dstp, dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
  env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
  env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));

  BYTE* _dstp = dstp;
  for (int iY = 0; iY<512; iY++) {
    for (int iX = 0; iX<512; iX++) {
      _dstp[iX] >>= 1;
    }
    _dstp+=p;
  }

  aud_clip->GetAudio(samples, max(0ll,start), count, env);

  int c = (int)count;
  for (int i=1; i < c;i++) {
    int l1 = samples[i*2-2];
    int r1 = samples[i*2-1];
    int l2 = samples[i*2];
    int r2 = samples[i*2+1];
    for (int s = 0 ; s < 8; s++) {  // 8 times supersampling (linear)
      int l = (l1*s) + (l2*(8-s));
      int r = (r1*s) + (r2*(8-s));
      int y = 256+((l+r)>>11);
      int x = 256+((l-r)>>11);
      BYTE v = dstp[x+y*p]+48;
      dstp[x+y*p] = min(v,(BYTE)235);
    }
  }

  int y_off = p*256;
  for (int x = 0; x < 512; x+=16)
    dstp[y_off + x] = (dstp[y_off + x] > 127) ? 16 : 235;

  for (int y = 0; y < 512;y+=16)
    dstp[y*p+256] = (dstp[y*p+256]>127) ? 16 : 235 ;

  env2->Free(samples);
  return dst;
}


PVideoFrame Histogram::DrawModeStereo(int n, IScriptEnvironment* env) {
  auto env2 = static_cast<IScriptEnvironment2*>(env);
  PVideoFrame src = env->NewVideoFrame(vi);
  __int64 start = vi.AudioSamplesFromFrames(n);
  __int64 end = vi.AudioSamplesFromFrames(n+1);
  __int64 count = end-start;
  signed short* samples = static_cast<signed short*>(
    env2->Allocate((int)count * vi.AudioChannels() * sizeof(unsigned short), 8, AVS_POOLED_ALLOC)
  );
  if (!samples) {
	  env2->ThrowError("Histogram: Could not reserve memory.");
  }

  int h = src->GetHeight();
  int imgSize = h*src->GetPitch();
  BYTE* srcp = src->GetWritePtr();
  memset(srcp, 16, imgSize);
  int p = src->GetPitch();

  aud_clip->GetAudio(samples, max(0ll,start), count, env);

  int c = (int)count;
  for (int i=1; i < c;i++) {
    int l1 = samples[i*2-2];
    int r1 = samples[i*2-1];
    int l2 = samples[i*2];
    int r2 = samples[i*2+1];
    for (int s = 0 ; s < 8; s++) {  // 8 times supersampling (linear)
      int l = (l1*s) + (l2*(8-s));
      int r = (r1*s) + (r2*(8-s));
      int y = 256+((l+r)>>11);
      int x = 256+((l-r)>>11);
      BYTE v = srcp[x+y*512]+48;
      srcp[x+y*512] = min(v, (BYTE)235);
    }
  }

  int y_off = p*256;
  for (int x = 0; x < 512; x+=16)
    srcp[y_off + x] = (srcp[y_off + x] > 127) ? 16 : 235;

  for (int y = 0; y < 512;y+=16)
    srcp[y*p+256] = (srcp[y*p+256]>127) ? 16 : 235 ;

  if (vi.IsYV12()) {
    srcp = src->GetWritePtr(PLANAR_U);
    imgSize = src->GetHeight(PLANAR_U) * src->GetPitch(PLANAR_U);
    memset(srcp, 128, imgSize);
    srcp = src->GetWritePtr(PLANAR_V);
    memset(srcp, 128, imgSize);
  }

  env2->Free(samples);
  return src;
}


PVideoFrame Histogram::DrawModeLuma(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  env->MakeWritable(&src);
  int h = src->GetHeight();
  int imgsize = h*src->GetPitch();
  BYTE* srcp = src->GetWritePtr();
  if (vi.IsYUY2()) {
    for (int i=0; i<imgsize; i+=2) {
      int p = srcp[i];
      p<<=4;
      srcp[i+1] = 128;
      srcp[i] = BYTE((p&256) ? (255-(p&0xff)) : p&0xff);
    }
  } else {
    for (int i=0; i<imgsize; i++) {
      int p = srcp[i];
      p<<=4;
      srcp[i] = BYTE((p&256) ? (255-(p&0xff)) : p&0xff);
    }
  }
  if (vi.IsPlanar()) {
    srcp = src->GetWritePtr(PLANAR_U);
    imgsize = src->GetHeight(PLANAR_U) * src->GetPitch(PLANAR_U);
    memset(srcp, 128, imgsize);
    srcp = src->GetWritePtr(PLANAR_V);
    memset(srcp, 128, imgsize);
  }
  return src;
}


PVideoFrame Histogram::DrawModeColor2(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  BYTE* pdst = dst->GetWritePtr();

  int imgSize = dst->GetHeight()*dst->GetPitch();

#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
  const float middle_f = 0.0f;
#else
  const float middle_f = 0.5f;
#endif

  // clear everything
  if (keepsource) {
    if (src->GetHeight() < dst->GetHeight()) {
      int imgSizeU = dst->GetHeight(PLANAR_U) * dst->GetPitch(PLANAR_U);
      switch (pixelsize) {
      case 1:
        memset(pdst, 16, imgSize);
        memset(dst->GetWritePtr(PLANAR_U), 128, imgSizeU);
        memset(dst->GetWritePtr(PLANAR_V), 128, imgSizeU);
        break;
      case 2:
        std::fill_n((uint16_t *)pdst, imgSize / sizeof(uint16_t), 16 << (bits_per_pixel - 8));
        std::fill_n((uint16_t *)dst->GetWritePtr(PLANAR_U), imgSizeU / sizeof(uint16_t), 128 << (bits_per_pixel - 8));
        std::fill_n((uint16_t *)dst->GetWritePtr(PLANAR_V), imgSizeU / sizeof(uint16_t), 128 << (bits_per_pixel - 8));
        break;
      case 4: // 32 bit float
        std::fill_n((float *)pdst, imgSize / sizeof(float), 16 / 255.0f);
        std::fill_n((float *)dst->GetWritePtr(PLANAR_U), imgSizeU / sizeof(float), middle_f);
        std::fill_n((float *)dst->GetWritePtr(PLANAR_V), imgSizeU / sizeof(float), middle_f);
        break;
      }
    }
  }

  if (keepsource) {
    env->BitBlt(pdst, dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
  }
  if (vi.IsPlanar()) {
    if (keepsource) {
      env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
      env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    }
    unsigned char* pdstb = pdst;
    unsigned char* pdstbU = dst->GetWritePtr(PLANAR_U);
    unsigned char* pdstbV = dst->GetWritePtr(PLANAR_V);
    
    if (keepsource)
      pdstb += src->GetRowSize(PLANAR_Y);

    int swidth = vi.GetPlaneWidthSubsampling(PLANAR_U);
    int sheight = vi.GetPlaneHeightSubsampling(PLANAR_U);

    int dstPitchY = dst->GetPitch(PLANAR_Y);
    int dstPitchUV = dst->GetPitch(PLANAR_U);

    // Erase all - luma
    for (int y = 0; y<dst->GetHeight(PLANAR_Y); y++) {
      memset(&pdstb[y*dstPitchY], 16, 256);
    }

    // Erase all - chroma
    pdstbU = dst->GetWritePtr(PLANAR_U);
    pdstbV = dst->GetWritePtr(PLANAR_V);
    if (keepsource) {
      pdstbU += src->GetRowSize(PLANAR_U);
      pdstbV += src->GetRowSize(PLANAR_V);
    }

    for (int y = 0; y<dst->GetHeight(PLANAR_U); y++) {
      memset(&pdstbU[y*dst->GetPitch(PLANAR_U)], 128, (256>>swidth));
      memset(&pdstbV[y*dst->GetPitch(PLANAR_V)], 128, (256>>swidth));
    }


    // plot valid grey ccir601 square
    pdstb = pdst;
    if (keepsource)
      pdstb += src->GetRowSize(PLANAR_Y);

    memset(&pdstb[(16*dstPitchY)+16], 128, 225);
    memset(&pdstb[(240*dstPitchY)+16], 128, 225);
    for (int y = 17; y<240; y++) {
      pdstb[16+y*dstPitchY] = 128;
      pdstb[240+y*dstPitchY] = 128;
    }

    // plot circles
    pdstb = pdst;
    pdstbU = dst->GetWritePtr(PLANAR_U);
    pdstbV = dst->GetWritePtr(PLANAR_V);
    if (keepsource) {
      pdstb += src->GetRowSize(PLANAR_Y);
      pdstbU += src->GetRowSize(PLANAR_U);
      pdstbV += src->GetRowSize(PLANAR_V);
    }

    // six hues in the color-wheel:
    // LC[3j,3j+1,3j+2], RC[3j,3j+1,3j+2] in YRange[j]+1 and YRange[j+1]
    int YRange[8] = { -1, 26, 104, 127, 191, 197, 248, 256 };
    // 2x green, 2x yellow, 3x red
    int LC[21] = { 145, 54, 34, 145, 54, 34, 210, 16, 146, 210, 16, 146, 81, 90, 240, 81, 90, 240, 81, 90, 240 };
    // cyan, 4x blue, magenta, red:
    int RC[21] = { 170, 166, 16, 41, 240, 110, 41, 240, 110, 41, 240, 110, 41, 240, 110, 106, 202, 222, 81, 90, 240 };

    // example boundary of cyan and blue:
    // red = min(r,g,b), blue if g < 2/3 b, green if b < 2/3 g.
    // cyan between green and blue.
    // thus boundary of cyan and blue at (r,g,b) = (0,170,255), since 2/3*255 = 170.
    // => yuv = (127,190,47); hue = -52 degr; sat = 103
    // => u'v' = (207,27) (same hue, sat=128)
    // similar for the other hues.
    // luma

    float innerF = 124.9f;  // .9 is for better visuals in subsampled mode
    float thicknessF = 1.5f;
    float oneOverThicknessF = 1.0f/thicknessF;
    float outerF = innerF + thicknessF*2.0f;
    float centerF = innerF + thicknessF;
    int innerSq = (int)(innerF*innerF);
    int outerSq = (int)(outerF*outerF);
    int activeY = 0;
    int xRounder = (1<<swidth) / 2;
    int yRounder = (1<<sheight) / 2;

    for (int y = -127; y<128; y++) {
      if (y+127 > YRange[activeY+1]) activeY++;
      for (int x = -127; x<=0; x++) {
        int distSq = x*x+y*y;
        if (distSq <= outerSq && distSq >= innerSq) {
          int interp = (int)(256.0f - (255.9f * (oneOverThicknessF * fabs(sqrt((float)distSq)- centerF))));
          // 255.9 is to account for float inprecision, which could cause underflow.

          int xP = 127 + x;
          int yP = 127 + y;

          pdstb[xP+yP*dstPitchY]     = (unsigned char)((interp*LC[3*activeY])>>8); // left upper half
          pdstb[255-xP+yP*dstPitchY] = (unsigned char)((interp*RC[3*activeY])>>8); // right upper half

          xP = (xP+xRounder) >> swidth;
          yP = (yP+yRounder) >> sheight;

          interp = min(256, interp);
          int invInt = (256-interp);

          pdstbU[xP+yP*dstPitchUV] = (unsigned char)((pdstbU[xP+yP*dstPitchUV] * invInt + interp * LC[3*activeY+1])>>8); // left half
          pdstbV[xP+yP*dstPitchUV] = (unsigned char)((pdstbV[xP+yP*dstPitchUV] * invInt + interp * LC[3*activeY+2])>>8); // left half

          xP = ((255)>>swidth) -xP;
          pdstbU[xP+yP*dstPitchUV] = (unsigned char)((pdstbU[xP+yP*dstPitchUV] * invInt + interp * RC[3*activeY+1])>>8); // right half
          pdstbV[xP+yP*dstPitchUV] = (unsigned char)((pdstbV[xP+yP*dstPitchUV] * invInt + interp * RC[3*activeY+2])>>8); // right half
        }
      }
    }

    // plot white 15 degree marks
    pdstb = pdst;
    if (keepsource)
      pdstb += src->GetRowSize(PLANAR_Y);

    for (int y = 0; y<24; y++) {
      pdstb[deg15c[y]+deg15s[y]*dstPitchY] = 235;
    }

    // plot vectorscope
    pdstb = pdst;
    if (keepsource)
      pdstb += src->GetRowSize(PLANAR_Y);

    const int src_pitch = src->GetPitch(PLANAR_Y);

    const int src_heightUV = src->GetHeight(PLANAR_U);
    const int src_widthUV = src->GetRowSize(PLANAR_U);
    const int src_pitchUV = src->GetPitch(PLANAR_U);

    const BYTE* pY = src->GetReadPtr(PLANAR_Y);
    const BYTE* pU = src->GetReadPtr(PLANAR_U);
    const BYTE* pV = src->GetReadPtr(PLANAR_V);

    for (int y=0; y<src_heightUV; y++) {
      for (int x=0; x<src_widthUV; x++) {
        const unsigned char uval = pU[x];
        const unsigned char vval = pV[x];
        pdstb[uval+vval*dstPitchY] = pY[x<<swidth];
        pdstbU[(uval>>swidth)+(vval>>sheight)*dstPitchUV] = uval;
        pdstbV[(uval>>swidth)+(vval>>sheight)*dstPitchUV] = vval;
      }
      pY += (src_pitch<<sheight);
      pU += src_pitchUV;
      pV += src_pitchUV;
    }

  }

  return dst;
}


PVideoFrame Histogram::DrawModeColor(int n, IScriptEnvironment* env) {
  // This mode will display the chroma values(U / V color placement) in a two dimensional graph(called a vectorscope)
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  BYTE* p = dst->GetWritePtr();

  int imgSize = dst->GetHeight()*dst->GetPitch();

#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
  const float middle_f = 0.0f;
#else
  const float middle_f = 0.5f;
#endif

  // clear everything
  if (keepsource) {
    if (src->GetHeight() < dst->GetHeight()) {
      int imgSizeU = dst->GetHeight(PLANAR_U) * dst->GetPitch(PLANAR_U);
      switch (pixelsize) {
      case 1:
        memset(p, 16, imgSize);
        memset(dst->GetWritePtr(PLANAR_U), 128, imgSizeU);
        memset(dst->GetWritePtr(PLANAR_V), 128, imgSizeU);
        break;
      case 2:
        std::fill_n((uint16_t *)p, imgSize / sizeof(uint16_t), 16 << (bits_per_pixel - 8));
        std::fill_n((uint16_t *)dst->GetWritePtr(PLANAR_U), imgSizeU / sizeof(uint16_t), 128 << (bits_per_pixel - 8));
        std::fill_n((uint16_t *)dst->GetWritePtr(PLANAR_V), imgSizeU / sizeof(uint16_t), 128 << (bits_per_pixel - 8));
        break;
      case 4: // 32 bit float
        std::fill_n((float *)p, imgSize / sizeof(float), 16 / 255.0f);
        std::fill_n((float *)dst->GetWritePtr(PLANAR_U), imgSizeU / sizeof(float), middle_f);
        std::fill_n((float *)dst->GetWritePtr(PLANAR_V), imgSizeU / sizeof(float), middle_f);
        break;
      }
    }
  }

  if (keepsource) {
    env->BitBlt(p, dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
  }
  if (vi.IsPlanar()) {
    if (keepsource) {
      env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
      env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    }

    int show_size = 1 << show_bits; // 256 for 8 bits, max 1024x1024 (10 bit resolution) found

    int *histUV = new(std::nothrow) int[show_size *show_size];
    if (!histUV)
      env->ThrowError("Histogram: malloc failure!");

    memset(histUV, 0, sizeof(int)*show_size *show_size);

    const BYTE* pU = src->GetReadPtr(PLANAR_U);
    const BYTE* pV = src->GetReadPtr(PLANAR_V);

    int w = origwidth;
    int h = src->GetHeight(PLANAR_U);
    int p = src->GetPitch(PLANAR_U) / pixelsize;

    if (pixelsize == 1) {
      if (show_bits == bits_per_pixel) {
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            int u = pU[y*p + x];
            int v = pV[y*p + x];
            histUV[(v << 8) + u]++;
          }
        }
      }
      else {
        // 8 bit data on 10 bit sized screen
        int shift_bits = show_bits - 8;
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            int u = pU[y*p + x] << shift_bits;
            int v = pV[y*p + x] << shift_bits;
            histUV[(v << show_bits) + u]++;
          }
        }
      }
    }
    else if (pixelsize == 2) {
      if (show_bits == bits_per_pixel) {
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            int u = reinterpret_cast<const uint16_t *>(pU)[y*p + x];
            int v = reinterpret_cast<const uint16_t *>(pV)[y*p + x];
            histUV[(v << show_bits) + u]++;
          }
        }
      }
      else if (show_bits < bits_per_pixel) {
        // 10 bit data on 8 bit sized screen
        int shift_bits = bits_per_pixel - show_bits;
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            int u = reinterpret_cast<const uint16_t *>(pU)[y*p + x] >> shift_bits;
            int v = reinterpret_cast<const uint16_t *>(pV)[y*p + x] >> shift_bits;
            histUV[(v << show_bits) + u]++;
          }
        }
      }
      else {
        // show_bits > bits_per_pixel
        // 10 bit data on 12bit sized screen
        int shift_bits = show_bits - bits_per_pixel;
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            int u = reinterpret_cast<const uint16_t *>(pU)[y*p + x] << shift_bits;
            int v = reinterpret_cast<const uint16_t *>(pV)[y*p + x] << shift_bits;
            histUV[(v << show_bits) + u]++;
          }
        }
      }
    }
    else { // float
#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
      const float shift = 0.5;
#else
      const float shift = 0.0;
#endif
      // 32 bit data on show_bits bit sized screen
      for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
          const float u_f = reinterpret_cast<const float *>(pU)[y*p + x] + shift;
          const float v_f = reinterpret_cast<const float *>(pV)[y*p + x] + shift;
          int u = (int)(u_f * show_size + 0.5f); // no rounding
          int v = (int)(v_f * show_size + 0.5f);
          u = clamp(u, 0, show_size - 1);
          v = clamp(v, 0, show_size - 1);
          histUV[(v << show_bits) + u]++;
        }
      }
    }


    // Plot Histogram on Y.
    int maxval = 1;

    // Should we adjust the divisor (maxval)??

    unsigned char* pdstb = dst->GetWritePtr(PLANAR_Y);
    if (keepsource) {
      pdstb += src->GetRowSize(PLANAR_Y); // right of the clip

    // Erase all
      for (int y = show_size; y < dst->GetHeight(); y++) {
        int p = dst->GetPitch(PLANAR_Y) / pixelsize;
        if (pixelsize == 1) {
          for (int x = 0; x < show_size; x++) {
            pdstb[x + y * p] = 16;
          }
        }
        else if (pixelsize == 2) {
          for (int x = 0; x < show_size; x++) {
            reinterpret_cast<uint16_t *>(pdstb)[x + y * p] = 16 << (bits_per_pixel - 8);
          }
        }
        else { // float
          for (int x = 0; x < show_size; x++) {
            reinterpret_cast<float *>(pdstb)[x + y * p] = 16 / 255.0f;
          }
        }
      }
    }

    if (pixelsize == 1) {
      int limit16 = 16 << (show_bits - 8);
      int limit16_pixel = 16 << (bits_per_pixel - 8);
      int limit240 = 240 << (show_bits - 8); // chroma danger
      int luma235 = 235 << (bits_per_pixel - 8);
      int dstpitch = dst->GetPitch(PLANAR_Y);
      for (int y = 0; y < show_size; y++) {
        const bool ylimited = y < limit16 || y>limit240;
        for (int x = 0; x < show_size; x++) {
          int disp_val = histUV[x + y * show_size] / maxval;
          if (ylimited || x < limit16 || x>limit240)
            disp_val -= limit16_pixel;

          pdstb[x] = (uint8_t)min(luma235, limit16_pixel + disp_val);
        }
        pdstb += dst->GetPitch(PLANAR_Y);
      }
    }
    else if (pixelsize == 2) {
      int limit16 = 16 << (show_bits - 8);
      int limit16_pixel = 16 << (bits_per_pixel - 8);
      int limit240 = 240 << (show_bits - 8); // chroma danger
      int luma235 = 235 << (bits_per_pixel - 8);
      int dstpitch = dst->GetPitch(PLANAR_Y);
      for (int y = 0; y < show_size; y++) {
        const bool ylimited = y < limit16 || y>limit240;
        for (int x = 0; x < show_size; x++) {
          int disp_val = (histUV[x + y * show_size] << (bits_per_pixel - 8)) / maxval;
          if (ylimited || x < limit16 || x>limit240)
            disp_val -= limit16_pixel;

          reinterpret_cast<uint16_t *>(pdstb)[x] = (uint16_t)min(luma235, limit16_pixel + disp_val);

        }
        pdstb += dstpitch;
      }
    }
    else { // 32 bit float
      int limit16 = 16 << (show_bits - 8);
      int limit16_pixel = 16; // keep 8 bit like
      int limit240 = 240 << (show_bits - 8); // chroma danger
      int luma235 = 235;// keep 8 bit like
      int dstpitch = dst->GetPitch(PLANAR_Y);
      for (int y = 0; y < show_size; y++) {
        const bool ylimited = y < limit16 || y>limit240;
        for (int x = 0; x < show_size; x++) {
          int disp_val = (histUV[x + y * show_size]) / maxval; // float was 16 bit 
          if (ylimited || x < limit16 || x>limit240)
            disp_val -= limit16_pixel;

          reinterpret_cast<float *>(pdstb)[x] = (float)(min(luma235, (limit16_pixel + disp_val))) / 255.0f;

        }
        pdstb += dstpitch;
      }
    }

    // Draw colors.
    for (int i = 0; i < 2; i++) {
      int plane = i == 0 ? PLANAR_U : PLANAR_V;
      pdstb = dst->GetWritePtr(plane);
      int swidth = vi.GetPlaneWidthSubsampling(plane);
      int sheight = vi.GetPlaneHeightSubsampling(plane);
      int dstpitch = dst->GetPitch(plane);

      if (keepsource) {
        pdstb += src->GetRowSize(plane);

        // Erase all
        if (pixelsize == 1) {
          for (int y = (show_size >> sheight); y < dst->GetHeight(plane); y++) {
            memset(&pdstb[y*dstpitch], 128, (show_size >> swidth) - 1);
          }
        }
        else if (pixelsize == 2) {
          for (int y = (show_size >> sheight); y < dst->GetHeight(plane); y++) {
            std::fill_n((uint16_t *)&pdstb[y*dstpitch], (show_size >> swidth) - 1, 128 << (bits_per_pixel - 8));
          }
        }
        else { // float
          for (int y = (show_size >> sheight); y < dst->GetHeight(plane); y++) {
            std::fill_n((float *)&pdstb[y*dstpitch], (show_size >> swidth) - 1, middle_f);
          }
        }
      }

      // PLANAR_U: x << swidth
      // PLANAR_V: y << sheight
      if (plane == PLANAR_U) {
        if (pixelsize == 1) {
          const int shiftCount = show_bits - bits_per_pixel;
          for (int y = 0; y < (show_size >> sheight); y++) {
            for (int x = 0; x < (show_size >> swidth); x++) {
              pdstb[x] = (unsigned char)((x << swidth) >> shiftCount) ;
            }
            pdstb += dstpitch;
          }
        }
        else if (pixelsize == 2) {
          const int shiftCount = show_bits - bits_per_pixel;
          for (int y = 0; y < (show_size >> sheight); y++) {
            for (int x = 0; x < (show_size >> swidth); x++) {
              if(shiftCount >= 0)
                reinterpret_cast<uint16_t *>(pdstb)[x] = (uint16_t)((x << swidth) >> shiftCount);
              else
                reinterpret_cast<uint16_t *>(pdstb)[x] = (uint16_t)((x << swidth) << -shiftCount);
            }
            pdstb += dstpitch;
          }
        }
        else {
#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
          const float shift = 0.5;
#else
          const float shift = 0.0;
#endif
          const int shiftCount = show_bits - 8;
          for (int y = 0; y < (show_size >> sheight); y++) {
            for (int x = 0; x < (show_size >> swidth); x++) {
              reinterpret_cast<float *>(pdstb)[x] = (float)(((x << swidth) >> shiftCount) / 255.0f - shift);
            }
            pdstb += dstpitch;
          }
        }
      } // PLANAR_U end
      else {
        // PLANAR_V
        if (pixelsize == 1) {
          const int shiftCount = show_bits - bits_per_pixel;
          for (int y = 0; y < (show_size >> sheight); y++) {
            for (int x = 0; x < (show_size >> swidth); x++) {
              pdstb[x] = (unsigned char)((y << sheight) >> shiftCount);
            }
            pdstb += dstpitch;
          }
        }
        else if (pixelsize == 2) {
          const int shiftCount = show_bits - bits_per_pixel;
          for (int y = 0; y < (show_size >> sheight); y++) {
            for (int x = 0; x < (show_size >> swidth); x++) {
              if(shiftCount >= 0)
                reinterpret_cast<uint16_t *>(pdstb)[x] = (uint16_t)((y << sheight) >> shiftCount);
              else
                reinterpret_cast<uint16_t *>(pdstb)[x] = (uint16_t)((y << sheight) << -shiftCount);
            }
            pdstb += dstpitch;
          }
        }
        else {
#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
          const float shift = 0.5;
#else
          const float shift = 0.0;
#endif
          const int shiftCount = show_bits - 8;
          for (int y = 0; y < (show_size >> sheight); y++) {
            for (int x = 0; x < (show_size >> swidth); x++) {
              reinterpret_cast<float *>(pdstb)[x] = (float)(((y << sheight) >> shiftCount) / 255.0f - shift);
            }
            pdstb += dstpitch;
          }
        }
      } // PLANAR_V end
    }

    delete[] histUV;
  }
  return dst;
}


PVideoFrame Histogram::DrawModeLevels(int n, IScriptEnvironment* env) {
  PVideoFrame src = child->GetFrame(n, env);
  PVideoFrame dst = env->NewVideoFrame(vi);
  BYTE* dstp = dst->GetWritePtr();

  int show_size = 1 << show_bits;

  // of source
  int src_width = src->GetRowSize() / pixelsize;
  int src_height = src->GetHeight();

  bool RGB = vi.IsRGB();
  int color_shift = (bits_per_pixel == 32) ? 0 : (bits_per_pixel - 8);
  int plane_default_black[3] = {
    RGB ? 0 : (16 << color_shift),
    RGB ? 0 : (128 << color_shift),
    RGB ? 0 : (128 << color_shift)
  };

  const int planesYUV[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A};
  const int planesRGB[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A};
  const int *planes = vi.IsYUV() || vi.IsYUVA() ? planesYUV : planesRGB;
  
  if (keepsource) {
    if (src->GetHeight() < dst->GetHeight()) {
      // fill empty area in the right bottom part
      const int fillSize = (dst->GetHeight() - src->GetHeight()) * dst->GetPitch();
      const int fillStart = src->GetHeight() * dst->GetPitch();

      switch (pixelsize) {
      case 1: memset(dstp + fillStart, plane_default_black[0], fillSize); break;
      case 2: std::fill_n((uint16_t *)(dstp + fillStart), fillSize / sizeof(uint16_t), plane_default_black[0]); break;
      case 4: std::fill_n((float *)(dstp + fillStart), fillSize / sizeof(float), (float)plane_default_black[0] / 255.0f); break;
      }

      // first plane is already processed
      // dont't touch Alpha
      for (int p = 1; p < 3; p++) {
        const int plane = planes[p];
        BYTE *ptr = dst->GetWritePtr(plane);

        const int fillSize = (dst->GetHeight(plane) - src->GetHeight(plane)) * dst->GetPitch(plane);
        const int fillStart = src->GetHeight(plane) * dst->GetPitch(plane);
        int chroma_fill = plane_default_black[p];
        switch (pixelsize) {
        case 1: memset(ptr + fillStart, RGB ? 0 : chroma_fill, fillSize); break;
        case 2: std::fill_n((uint16_t *)(ptr + fillStart), fillSize / sizeof(uint16_t), chroma_fill); break;
        case 4: std::fill_n((float *)(ptr + fillStart), fillSize / sizeof(float), RGB ? 0.0f : 0.5f); break;
        }
      }
    }
  }

  // counters
  auto env2 = static_cast<IScriptEnvironment2*>(env);
  int bufsize = sizeof(uint32_t)*show_size;
  uint32_t *histPlane1 = static_cast<uint32_t*>(env2->Allocate(bufsize * 3, 16, AVS_NORMAL_ALLOC));
  uint32_t *histPlanes[3] = { histPlane1, histPlane1 + show_size, histPlane1 + 2 * show_size };
  if (!histPlane1)
    env->ThrowError("Histogram: Could not reserve memory.");
  std::fill_n(histPlane1, show_size*3, 0);

  const int source_width = origwidth;
  const int xstart = keepsource ? origwidth : 0; // drawing starts at this column

  // copy planes
  // luma or G
  if (keepsource) {
    env->BitBlt(dstp, dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
  }
  if (vi.IsPlanar()) {
    // copy rest planes
    if (keepsource) {
      for (int p = 1; p < vi.NumComponents(); p++) {
        const int plane = planes[p];
        env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane), src->GetPitch(plane), src->GetRowSize(plane), src->GetHeight(plane));
      }
    }

    // accumulate population
    for (int p = 0; p < 3; p++) {
      const int plane = planes[p];
      const BYTE* srcp = src->GetReadPtr(plane);

      const int w = src->GetRowSize(plane) / pixelsize;
      const int h = src->GetHeight(plane);
      const int pitch = src->GetPitch(plane) / pixelsize;

      // accumulator of current plane
      // size: show_size (256 or 1024)
      uint32_t *hist = histPlanes[p];

      if(pixelsize==1) {
        const uint8_t *srcp8 = reinterpret_cast<const uint8_t *>(srcp);
        int invshift = show_bits - bits_per_pixel;
        // 8 bit clip into 8,9,... bit histogram
        for (int y = 0; y < h; y++) {
          for (int x = 0; x < w; x++) {
            hist[(int)srcp8[x] << invshift]++;
            //hist[srcp[y*dstpitch + x]]++;
          }
          srcp8 += pitch;
        }
      }
      else if (pixelsize == 2) {
        const uint16_t *srcp16 = reinterpret_cast<const uint16_t *>(srcp);
        int shift = bits_per_pixel - show_bits;
        int max_pixel_value = show_size - 1;
        if (shift < 0) {
          // 10 bit clip into 11 bit histogram
          int invshift = -shift;
          for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
              hist[srcp16[x] << invshift]++;
            }
            srcp16 += pitch;
          }
        } else {
          // e.g.10 bit clip into 8-9-10 bit histogram
          for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
              hist[min(srcp16[x] >> shift, max_pixel_value)]++;
            }
            srcp16 += pitch;
          }
        }
      }
      else {
        // float
        const float *srcp32 = reinterpret_cast<const float *>(srcp);
        const float multiplier = (float)(show_size - 1);
#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
        const float preshift = 0.5f;
#else
        const float preshift = 0.0f;
#endif
        if (plane == PLANAR_U || plane == PLANAR_V) {
          for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
              hist[(int)(clamp(srcp32[x] + preshift, 0.0f, 1.0f)*multiplier + 0.5f)]++;
            }
            srcp32 += pitch;
          }
        }
        else {
          for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
              hist[(int)(clamp(srcp32[x], 0.0f, 1.0f)*multiplier + 0.5f)]++;
            }
            srcp32 += pitch;
          }
        }
      }
    } // accumulate end

    int width = source_width;
    int pos_shift = (show_bits - 8);
    int show_middle_pos = (128 << pos_shift);
    // draw planes
    for (int p = 0; p < 3; p++) {
      const int plane = planes[p];
      const BYTE* srcp = src->GetReadPtr(plane);

      int swidth = vi.GetPlaneWidthSubsampling(plane);
      int sheight = vi.GetPlaneHeightSubsampling(plane);

      // Draw Unsafe zone (UV-graph)

      unsigned char* pdstb = dst->GetWritePtr(plane);
      pdstb += (xstart*pixelsize) >> swidth; // next to the source image if kept

      const int dstPitch = dst->GetPitch(plane);

      // Clear Y/U/V or B, R G
      BYTE *ptr = pdstb;
      int color = plane_default_black[p];
      for (int y = 0; y < dst->GetHeight() >> sheight; y++) {
        switch (pixelsize) {
        case 1: memset(ptr, color, show_size >> swidth); break;
        case 2: std::fill_n((uint16_t *)(ptr), show_size >> swidth, color); break;
        case 4: std::fill_n((float *)(ptr), show_size >> swidth, (float)color / 255); break;
        }
        ptr += dstPitch;
      }

      if (!RGB && markers) {
        // Draw Unsafe zone (Y-graph)
        int color_unsafeZones[3] = { 32, 16, 160 };

        int color_usz = color_unsafeZones[p];
        int color_i = color_usz << color_shift;
        float color_f = color_usz / 255.0f;
        ptr = pdstb + 0 * dstPitch;;
        for (int y = 0; y <= 64 >> sheight; y++) {
          int x = 0;
          for (; x < (16 << pos_shift) >> swidth; x++) {
            if (pixelsize == 1)
              ptr[x] = color_i;
            else if (pixelsize == 2)
              reinterpret_cast<uint16_t *>(ptr)[x] = color_i;
            else
              reinterpret_cast<float *>(ptr)[x] = color_f;
          }
          for (x = (236 << pos_shift) >> swidth; x < (show_size >> swidth); x++) { // or (235 << pos_shift) + 1?
            if (pixelsize == 1)
              ptr[x] = color_i;
            else if (pixelsize == 2)
              reinterpret_cast<uint16_t *>(ptr)[x] = color_i;
            else
              reinterpret_cast<float *>(ptr)[x] = color_f;
          }
          ptr += dstPitch;
        }
      }

      if (markers) {
        if (RGB) {
          // nice gradients
          int StartY;
          switch (plane) {
          case PLANAR_R: StartY = 0 + 0; break;
          case PLANAR_G: StartY = 64 + 16; break;
          case PLANAR_B: StartY = 128 + 32; break;
          }
          ptr = pdstb + ((StartY) >> sheight) * dstPitch;
          for (int y = (StartY) >> sheight; y <= (StartY + 64) >> sheight; y++) {
            if (pixelsize == 1) {
              for (int x = 0; x < (show_size >> swidth); x++) {
                int color = x >> pos_shift;
                int color_i = color << color_shift;
                ptr[x] = color_i;
              }
            }
            else if (pixelsize == 2) {
              for (int x = 0; x < (show_size >> swidth); x++) {
                int color = x >> pos_shift;
                int color_i = color << color_shift;
                reinterpret_cast<uint16_t *>(ptr)[x] = color_i;
              }
            }
            else { // pixelsize == 4 float
              for (int x = 0; x < (show_size >> swidth); x++) {
                int color = x >> pos_shift;
                float color_f = color / 255.0f;
                reinterpret_cast<float *>(ptr)[x] = color_f;
              }
            }
            ptr += dstPitch;
          }
        }
        else {
          // UV gradients plus danger zones
          for (int gradient_upper_lower = 0; gradient_upper_lower < 2; gradient_upper_lower++)
          {
            // Draw upper and lower gradient
          // upper: x=0-16, R=G=255, B=0; x=128, R=G=B=0; x=240-255, R=G=0, B=255
          // lower: x=0-16, R=0, G=B=255; x=128, R=G=B=0; x=240-255, R=255, G=B=0
            int color1_upper_lower_gradient[2][3] = { { 210 / 2, 16 + 112 / 2, 128 },{ 170 / 2, 128, 16 + 112 / 2 } };
            int color = color1_upper_lower_gradient[gradient_upper_lower][p];
            int color_i = color << color_shift;
            float color_f = color / 255.0f;

            int color2_upper_lower_gradient[2][3] = { { 41 / 2, 240 - 112 / 2, 128 },{ 81 / 2, 128, 240 - 112 / 2 } };
            int color2 = color2_upper_lower_gradient[gradient_upper_lower][p];
            int color2_i = color2 << color_shift;
            float color2_f = color2 / 255.0f;

            // upper only for planar U and Y
            if (plane == PLANAR_V && gradient_upper_lower == 0)
              continue;
            // lower only for planar V and Y
            if (plane == PLANAR_U && gradient_upper_lower == 1)
              continue;
            int StartY = gradient_upper_lower == 0 ? 64 + 16 : 128 + 32;
            ptr = pdstb + ((StartY) >> sheight) * dstPitch;
            for (int y = (StartY) >> sheight; y <= (StartY + 64) >> sheight; y++) {
              int x = 0;
              // 0..15, (scaled) left danger area
              const int left_limit = ((16 << pos_shift) >> swidth) - 1;
              if (pixelsize == 1) {
                for (; x < left_limit; x++)
                  ptr[x] = color_i;
              }
              else if (pixelsize == 2) {
                for (; x < left_limit; x++)
                  reinterpret_cast<uint16_t *>(ptr)[x] = color_i;
              }
              else { // float
                for (; x < left_limit; x++)
                  reinterpret_cast<float *>(ptr)[x] = color_f;
              }

              if (plane == PLANAR_Y) {
                // from 16 to middle point
                if (pixelsize == 1) {
                  for (; x <= show_middle_pos; x++) {
                    int color3 =
                      (gradient_upper_lower == 0) ?
                      (((show_middle_pos - x) * 15) >> 3) >> pos_shift : // *1.875
                      ((show_middle_pos - x) * 99515) >> 16 >> pos_shift; // *1.518
                    int color3_i = color3 << color_shift;
                    ptr[x] = color3_i;
                  }
                }
                else if (pixelsize == 2) {
                  for (; x <= show_middle_pos; x++) {
                    int color3 =
                      (gradient_upper_lower == 0) ?
                      (((show_middle_pos - x) * 15) >> 3) >> pos_shift : // *1.875
                      ((show_middle_pos - x) * 99515) >> 16 >> pos_shift; // *1.518
                    int color3_i = color3 << color_shift;
                    reinterpret_cast<uint16_t *>(ptr)[x] = color3_i;
                  }
                }
                else { // float
                  for (; x <= show_middle_pos; x++) {
                    int color3 =
                      (gradient_upper_lower == 0) ?
                      (((show_middle_pos - x) * 15) >> 3) >> pos_shift : // *1.875
                      ((show_middle_pos - x) * 99515) >> 16 >> pos_shift; // *1.518
                    float color3_f = color3 / 255.0f;
                    reinterpret_cast<float *>(ptr)[x] = color3_f;
                  }
                }
              }

              // Y: from middle point to white point
              // other plane: gradient
              if (plane == PLANAR_Y) {
                if (pixelsize == 1) {
                  for (; x <= (240 << pos_shift) >> swidth; x++) {
                    int color4 =
                      (gradient_upper_lower == 0) ?
                      ((x - show_middle_pos) * 24001) >> 16 >> pos_shift :  // *0.366
                      ((x - show_middle_pos) * 47397) >> 16 >> pos_shift; // *0.723
                    int color4_i = color4 << color_shift;
                    ptr[x] = color4_i;
                  }
                }
                else if (pixelsize == 2) {
                  for (; x <= (240 << pos_shift) >> swidth; x++) {
                    int color4 =
                      (gradient_upper_lower == 0) ?
                      ((x - show_middle_pos) * 24001) >> 16 >> pos_shift :  // *0.366
                      ((x - show_middle_pos) * 47397) >> 16 >> pos_shift; // *0.723
                    int color4_i = color4 << color_shift;
                    reinterpret_cast<uint16_t *>(ptr)[x] = color4_i;
                  }
                }
                else { // float
                  for (; x <= (240 << pos_shift) >> swidth; x++) {
                    int color4 =
                      (gradient_upper_lower == 0) ?
                      ((x - show_middle_pos) * 24001) >> 16 >> pos_shift :  // *0.366
                      ((x - show_middle_pos) * 47397) >> 16 >> pos_shift; // *0.723
                    float color4_f = color4 / 255.0f;
                    reinterpret_cast<float *>(ptr)[x] = color4_f;
                  }
                }
              }
              else {
                // plane == U or V
                if (pixelsize == 1) {
                  for (; x <= (240 << pos_shift) >> swidth; x++) {
                    int color4 = (x << swidth) >> pos_shift;
                    int color4_i = color4 << color_shift;
                    ptr[x] = color4_i;
                  }
                }
                else if (pixelsize == 2) {
                  for (; x <= (240 << pos_shift) >> swidth; x++) {
                    int color4 = (x << swidth) >> pos_shift;
                    int color4_i = color4 << color_shift;
                    reinterpret_cast<uint16_t *>(ptr)[x] = color4_i;
                  }
                }
                else { // float
                  for (; x <= (240 << pos_shift) >> swidth; x++) {
                    int color4 = (x << swidth) >> pos_shift;
                    float color4_f = color4 / 255.0f;
                    reinterpret_cast<float *>(ptr)[x] = color4_f;
                  }
                }
              }

              if (pixelsize == 1) {
                for (; x < (show_size >> swidth); x++)
                  ptr[x] = color2_i;
              }
              else if (pixelsize == 2) {
                for (; x < (show_size >> swidth); x++)
                  reinterpret_cast<uint16_t *>(ptr)[x] = color2_i;
              }
              else { // float
                for (; x < (show_size >> swidth); x++)
                  reinterpret_cast<float *>(ptr)[x] = color2_f;
              }

              ptr += dstPitch;
            } // for y gradient draw
          } // gradient for upper lower
        } // gradients for RGB/UV
      } // if markers
    } // planes for

    // Draw dotted centerline
    // YUV: only 1 plane (PLANAR_Y)
    for (int p = 0; p < (RGB ? 3 : 1); p++) {
      const int plane = planes[p];

      int color = 128; // also good for RGB
      int color_i = color << color_shift;
      float color_f = 0.5f;

      const int dstPitch = dst->GetPitch(plane);

      unsigned char* pdstb = dst->GetWritePtr(plane);
      pdstb += (xstart*pixelsize); // next to the original clip (if kept), working only on Y plane: no ">> swidth" needed
      BYTE *ptr = pdstb;

      if (markers) {
        // omit centerline if markers == false
        for (int y = 0; y <= 256 - 32; y++) {
          if ((y & 3) > 1) {
            if (pixelsize == 1)       ptr[show_middle_pos] = color_i;
            else if (pixelsize == 2)  reinterpret_cast<uint16_t *>(ptr)[show_middle_pos] = color_i;
            else                   reinterpret_cast<float *>(ptr)[show_middle_pos] = color_f;

          }
          ptr += dstPitch;
        }
      }


      for (int n = 0; n < 3; n++) {

        // Draw histograms
        const uint32_t clampval = (int)((src_width*src_height)*option.AsDblDef(100.0) / 100.0); // Population limit % factor
        uint32_t maxval = 0;
        uint32_t *hist;

        hist = histPlanes[n];
        for (int i = 0; i < show_size; i++) {
          if (hist[i] > clampval) hist[i] = clampval;
          maxval = max(hist[i], maxval);
        }

        float scale = float(64.0 / maxval);

        int color = 235; // also good for RGB
        int color_i = color << color_shift; // igaz�b�l max_luma
        float color_f = color / 255.0f;

        int Y_pos;
        switch (n) { // n: YUV 012, GBR 012
        case 0: Y_pos = RGB ? 128 + 16 :  64 + 0; break;  // Y or G
        case 1: Y_pos = RGB ? 192 + 32 : 128 + 16; break; // U or B
        case 2: Y_pos = RGB ?  64 +  0 : 192 + 32; break; // V or R
        }

        for (int x = 0; x < show_size; x++) {
          float scaled_h = (float)hist[x] * scale;
          int h = Y_pos - min((int)scaled_h, 64) + 1;
          int left = (int)(220.0f*(scaled_h - (float)((int)scaled_h))); // color, scaled later

          ptr = pdstb + (Y_pos + 1) * dstPitch;
          for (int y = Y_pos + 1; y > h; y--) {
            if (pixelsize == 1)       ptr[x] = color_i;
            else if (pixelsize == 2)  reinterpret_cast<uint16_t *>(ptr)[x] = color_i;
            else                   reinterpret_cast<float *>(ptr)[x] = color_f;
            ptr -= dstPitch;
          }
          int color_top = (16 + left);
          int color_top_i = color_top << color_shift; // igaz�b�l max_luma
          float color_top_f = color_top / 255.0f;

          ptr = pdstb + h*dstPitch;
          if (pixelsize == 1)       ptr[x] = color_top_i;
          else if (pixelsize == 2)  reinterpret_cast<uint16_t *>(ptr)[x] = color_top_i;
          else                   reinterpret_cast<float *>(ptr)[x] = color_top_f;
        }
      }
    }
  }

  env2->Free(histPlane1);

  return dst;
}


PVideoFrame Histogram::DrawModeClassic(int n, IScriptEnvironment* env)
{
  static uint16_t exptab[1 << 12]; // max bits=12
  static bool init = false;
  static int E167;

  int show_size = 1 << show_bits;

  int lookup_size = 1 << show_bits; // 256, 1024, 4096, 16384, 65536

  int hist_max_pixel_value = (1 << show_bits) - 1;
  int hist_tv_range_low = 16 << (show_bits - 8); // 16
  int hist_tv_range_hi_luma = 235 << (show_bits - 8); // 16-235
  int hist_range_luma = hist_tv_range_hi_luma - hist_tv_range_low; // 219
  int hist_mid_range_luma = (hist_range_luma + 1) / 2 + hist_tv_range_low - 1; // in Classic Avisynth somehow 124 was fixed for this
  // 235-16 = 219 / 2 => 110; 110 + 16 - 1 = 125.0
  int hist_tv_range_hi_chroma = 240 << (show_bits - 8); // 16-240,64?60, 256?852,... 4096-61692
  int hist_range_chroma = hist_tv_range_hi_chroma - hist_tv_range_low; // 224

  int internal_bits_per_pixel = (pixelsize == 4) ? 16 : bits_per_pixel; // hack for float

  int max_pixel_value = (1 << internal_bits_per_pixel) - 1;
  int tv_range_low = 16 << (internal_bits_per_pixel - 8); // 16
  int tv_range_hi_luma = 235 << (internal_bits_per_pixel - 8); // 16-235
  int range_luma = tv_range_hi_luma - tv_range_low; // 219
  int tv_range_hi_chroma = 240 << (internal_bits_per_pixel - 8); // 16-240,64?60, 256?852,... 4096-61692
  int range_chroma = tv_range_hi_chroma - tv_range_low; // 224
  int middle_chroma = 1 << (internal_bits_per_pixel - 1); // 128

  if (!init) {
    init = true;

    const double K = log(0.5 / hist_range_luma) / hist_max_pixel_value; // approx -1/42
    const int limit68 = 68 << (internal_bits_per_pixel - 8);
    // exptab: pixel values for final drawing
    exptab[0] = tv_range_low;
    for (int i = 1; i < show_size; i++) {
      exptab[i] = uint16_t(tv_range_low + 0.5 + range_luma * (1 - exp(i*K))); // 16.5 + 219*
      if (exptab[i] <= tv_range_hi_luma - limit68)
        E167 = i; // index of last value less than...  for drawing lower extremes
      /*
      if (internal_bits_per_pixel < show_bits)
        exptab[i] >>= (show_bits - internal_bits_per_pixel); // scale intensity down
      else
        exptab[i] <<= (internal_bits_per_pixel - show_bits); // scale intensity up
        */
    }
    exptab[hist_max_pixel_value] = tv_range_hi_luma;
  }

  const int source_width = origwidth;
  const int xstart = keepsource ? origwidth : 0; // drawing starts at this column

  PVideoFrame src = child->GetFrame(n, env);
  const BYTE* srcp = src->GetReadPtr();
  const int srcpitch = src->GetPitch();

  const bool isRGB = vi.IsRGB();

  PVideoFrame dst = env->NewVideoFrame(vi);
  BYTE* pdst = dst->GetWritePtr();
  const int dstpitch = dst->GetPitch();

  if (keepsource) {
    env->BitBlt(pdst, dst->GetPitch(), src->GetReadPtr(), src->GetPitch(), src->GetRowSize(), src->GetHeight());
  }
  if (vi.IsPlanar()) {
    if (keepsource) {
      env->BitBlt(dst->GetWritePtr(PLANAR_U), dst->GetPitch(PLANAR_U), src->GetReadPtr(PLANAR_U), src->GetPitch(PLANAR_U), src->GetRowSize(PLANAR_U), src->GetHeight(PLANAR_U));
      env->BitBlt(dst->GetWritePtr(PLANAR_V), dst->GetPitch(PLANAR_V), src->GetReadPtr(PLANAR_V), src->GetPitch(PLANAR_V), src->GetRowSize(PLANAR_V), src->GetHeight(PLANAR_V));
    }

    // luma
    const int height = src->GetHeight(PLANAR_Y);
    for (int y = 0; y<height; ++y) {
      int hist[1<<12] = { 0 }; // allocate max 12 bit res. todo

      // accumulate line population
      if(pixelsize==1) {
        // 8 bit clip into 8,9,... bit histogram
        int invshift = show_bits - 8;
        for (int x = 0; x<source_width; ++x) {
          hist[(int)srcp[x] << invshift]++;
        }
      }
      else if (pixelsize == 2) {
        const uint16_t *srcp16 = reinterpret_cast<const uint16_t *>(srcp);
        int shift = bits_per_pixel - show_bits;
        int max_pixel_value = show_size - 1;
        if (shift < 0) {
          // 10 bit clip into 11 bit histogram
          int invshift = -shift;
          for (int x = 0; x < source_width; x++) {
            hist[srcp16[x] << invshift]++;
          }
        } else {
          // e.g.10 bit clip into 8-9-10 bit histogram
          for (int x = 0; x < source_width; x++) {
            hist[min(srcp16[x] >> shift, max_pixel_value)]++;
          }
        }
      }
      else // pixelsize == 4
      {
        // float
        const float *srcp32 = reinterpret_cast<const float *>(srcp);
        const float multiplier = (float)(show_size - 1);
        for (int x = 0; x < source_width; x++) {
          hist[(int)(clamp(srcp32[x], 0.0f, 1.0f)*multiplier + 0.5f)]++;
        }
      }
      // accumulate end
      BYTE* const q = pdst + xstart * pixelsize; // write to frame
      if (markers) {
        if (pixelsize == 1) {
          for (int x = 0; x < show_size; ++x) {
            if (x<hist_tv_range_low || x == hist_mid_range_luma || x>hist_tv_range_hi_luma) {
              q[x] = (BYTE)exptab[min(E167, hist[x])] + 68; // brighter danger zone
            }
            else {
              q[x] = (BYTE)exptab[min(255, hist[x])];
            }
          }
        }
        else if (pixelsize == 2) {
          uint16_t *dstp16 = reinterpret_cast<uint16_t *>(q);
          for (int x = 0; x < show_size; ++x) {
            if (x<hist_tv_range_low || x == hist_mid_range_luma || x>hist_tv_range_hi_luma) {
              dstp16[x] = exptab[min(E167, hist[x])] + (68 << (bits_per_pixel - 8));
            }
            else {
              dstp16[x] = exptab[min(hist_max_pixel_value, hist[x])];
            }
          }
        }
        else { // pixelsize == 4
          float *dstp32 = reinterpret_cast<float *>(q);
          for (int x = 0; x < show_size; ++x) {
            if (x<hist_tv_range_low || x == hist_mid_range_luma || x>hist_tv_range_hi_luma) {
              dstp32[x] = (exptab[min(E167, hist[x])] + (68 << (internal_bits_per_pixel - 8))) / 65536.0f;
            }
            else {
              dstp32[x] = exptab[min(hist_max_pixel_value, hist[x])] / 65536.0f;
            }
          }
        }
      }
      else {
        if (pixelsize == 1) {
          for (int x = 0; x < show_size; ++x)
            q[x] = (BYTE)exptab[min(255, hist[x])];
        }
        else if (pixelsize == 2) {
          uint16_t *dstp16 = reinterpret_cast<uint16_t *>(q);
          for (int x = 0; x < show_size; ++x)
            dstp16[x] = exptab[min(hist_max_pixel_value, hist[x])];
        }
        else { // pixelsize == 4
          float *dstp32 = reinterpret_cast<float *>(q);
          for (int x = 0; x < show_size; ++x)
            dstp32[x] = exptab[min(hist_max_pixel_value, hist[x])] / 65536.0f;
        }
      }
      srcp += srcpitch;
      pdst += dstpitch;
    } // end of pixel accumulation + luma

    // chroma
    const int pitchUV = dst->GetPitch(PLANAR_U);

    if (pitchUV != 0) {
      const int subs = vi.GetPlaneWidthSubsampling(PLANAR_U);
      const int fact = 1<<subs;

      BYTE* p2 = dst->GetWritePtr(PLANAR_U) + ((xstart*pixelsize) >> subs); // put it on the right
      BYTE* p3 = dst->GetWritePtr(PLANAR_V) + ((xstart*pixelsize) >> subs); // put it on the right

      // if markers==false parameter, keep neutral coloring
      const uint16_t color_u_offlimit8 = markers ? 16 : 128;
      const uint16_t color_v_offlimit8 = markers ? 160 : 128;
      const uint16_t color_u_centermark8 = markers ? 160 : 128;
      const uint16_t color_v_centermark8 = markers ? 16 : 128;

      const uint16_t color_u_offlimit = color_u_offlimit8 << (internal_bits_per_pixel - 8);
      const uint16_t color_v_offlimit = color_v_offlimit8 << (internal_bits_per_pixel - 8);
      const uint16_t color_u_centermark = color_u_centermark8 << (internal_bits_per_pixel - 8);
      const uint16_t color_v_centermark = color_v_centermark8 << (internal_bits_per_pixel - 8);
#ifdef FLOAT_CHROMA_IS_ZERO_CENTERED
      const float middle_chroma_f = 0.0f;
#else
      const float middle_chroma_f = 0.5f;
#endif
      const float color_u_offlimit_f = (float)(color_u_offlimit8 - 128) / 255.0f + middle_chroma_f;
      const float color_v_offlimit_f = (float)(color_v_offlimit8 - 128) / 255.0f + middle_chroma_f;
      const float color_u_centermark_f = (float)(color_u_centermark8 - 128) / 255.0f + middle_chroma_f;
      const float color_v_centermark_f = (float)(color_v_centermark8 - 128) / 255.0f + middle_chroma_f;

      const int height = src->GetHeight(PLANAR_U);
      for (int y2 = 0; y2<height; ++y2) {
        if(pixelsize==1) {
          for (int x = 0; x<show_size; x += fact) {
            if (x<hist_tv_range_low || x>hist_tv_range_hi_luma) {
              p2[x >> subs] = (BYTE)color_u_offlimit8;
              p3[x >> subs] = (BYTE)color_v_offlimit8;
            } else if (x==hist_mid_range_luma) {
              p2[x >> subs] = (BYTE)color_u_centermark8;
              p3[x >> subs] = (BYTE)color_v_centermark8;
            } else {
              p2[x >> subs] = 128;
              p3[x >> subs] = 128;
            }
          }
        }
        else if (pixelsize == 2) {
          for (int x = 0; x<show_size; x += fact) {
            if (x<hist_tv_range_low || x>hist_tv_range_hi_luma) {
              reinterpret_cast<uint16_t *>(p2)[x >> subs] = color_u_offlimit;
              reinterpret_cast<uint16_t *>(p3)[x >> subs] = color_v_offlimit;
            } else if (x==hist_mid_range_luma) {
              reinterpret_cast<uint16_t *>(p2)[x >> subs] = color_u_centermark;
              reinterpret_cast<uint16_t *>(p3)[x >> subs] = color_v_centermark;
            } else {
              reinterpret_cast<uint16_t *>(p2)[x >> subs] = middle_chroma;
              reinterpret_cast<uint16_t *>(p3)[x >> subs] = middle_chroma;
            }
          }
        } else { // pixelsize==4
          for (int x = 0; x<show_size; x += fact) {
            if (x<hist_tv_range_low || x>hist_tv_range_hi_luma) {
              reinterpret_cast<float *>(p2)[x >> subs] = color_u_offlimit_f;
              reinterpret_cast<float *>(p3)[x >> subs] = color_u_offlimit_f;
            } else if (x==hist_mid_range_luma) {
              reinterpret_cast<float *>(p2)[x >> subs] = color_u_centermark_f;
              reinterpret_cast<float *>(p3)[x >> subs] = color_v_centermark_f;
            } else {
              reinterpret_cast<float *>(p2)[x >> subs] = middle_chroma_f;
              reinterpret_cast<float *>(p3)[x >> subs] = middle_chroma_f;
            }
          }

        }
        p2 += pitchUV;
        p3 += pitchUV;
      }
    }
  } else {
    const int pitch = dst->GetPitch();
    for (int y = 0; y<src->GetHeight(); ++y) { // YUY2
      int hist[256] = { 0 };
      for (int x = 0; x<source_width; ++x) {
        hist[srcp[x*2]]++;
      }
      BYTE* const q = pdst + xstart*2;
      if (markers) {
        for (int x = 0; x < 256; x += 2) {
          if (x < 16 || x>235) {
            q[x * 2 + 0] = (BYTE)exptab[min(E167, hist[x])] + 68;
            q[x * 2 + 1] = 16;
            q[x * 2 + 2] = (BYTE)exptab[min(E167, hist[x + 1])] + 68;
            q[x * 2 + 3] = 160;
          }
          else if (x == 124) {
            q[x * 2 + 0] = (BYTE)exptab[min(E167, hist[x])] + 68;
            q[x * 2 + 1] = 160;
            q[x * 2 + 2] = (BYTE)exptab[min(255, hist[x + 1])];
            q[x * 2 + 3] = 16;
          }
          else {
            q[x * 2 + 0] = (BYTE)exptab[min(255, hist[x])];
            q[x * 2 + 1] = 128;
            q[x * 2 + 2] = (BYTE)exptab[min(255, hist[x + 1])];
            q[x * 2 + 3] = 128;
          }
        }
      }
      else {
        for (int x = 0; x < 256; x += 2) {
          q[x * 2 + 0] = (BYTE)exptab[min(255, hist[x])];
          q[x * 2 + 1] = 128;
          q[x * 2 + 2] = (BYTE)exptab[min(255, hist[x + 1])];
          q[x * 2 + 3] = 128;
        }
      }
      pdst += pitch;
      srcp += srcpitch;
    }
  }
  return dst;
}


AVSValue __cdecl Histogram::Create(AVSValue args, void*, IScriptEnvironment* env)
{
  const char* st_m = args[1].AsString("classic");

  Mode mode = ModeClassic;

  if (!lstrcmpi(st_m, "classic"))
    mode = ModeClassic;

  if (!lstrcmpi(st_m, "levels"))
    mode = ModeLevels;

  if (!lstrcmpi(st_m, "color"))
    mode = ModeColor;

  if (!lstrcmpi(st_m, "color2"))
    mode = ModeColor2;

  if (!lstrcmpi(st_m, "luma"))
    mode = ModeLuma;

  if (!lstrcmpi(st_m, "stereoY8"))
    mode = ModeStereoY8;

  if (!lstrcmpi(st_m, "stereo"))
    mode = ModeStereo;

  if (!lstrcmpi(st_m, "stereooverlay"))
    mode = ModeOverlay;

  if (!lstrcmpi(st_m, "audiolevels"))
    mode = ModeAudioLevels;

  const VideoInfo& vi_orig = args[0].AsClip()->GetVideoInfo();

  if (mode == ModeLevels && vi_orig.IsRGB() && !vi_orig.IsPlanar()) {
    // as Levels can work for PlanarRGB, convert packed RGB to planar, then back
    // better that nothing
    AVSValue new_args[1] = { args[0].AsClip() };
    PClip clip;
    if (vi_orig.IsRGB24() || vi_orig.IsRGB48()) {
      clip = env->Invoke("ConvertToPlanarRGB", AVSValue(new_args, 1)).AsClip();
    }
    else if (vi_orig.IsRGB32() || vi_orig.IsRGB64()) {
      clip = env->Invoke("ConvertToPlanarRGBA", AVSValue(new_args, 1)).AsClip();
    }
    Histogram* Result = new Histogram(clip, mode, args[2], args[3].AsInt(8), args[4].AsBool(true), args[5].AsBool(true), env);

    AVSValue new_args2[1] = { Result };
    if (vi_orig.IsRGB24()) {
      return env->Invoke("ConvertToRGB24", AVSValue(new_args2, 1)).AsClip();
    }
    else if (vi_orig.IsRGB48()) {
      return env->Invoke("ConvertToRGB48", AVSValue(new_args2, 1)).AsClip();
    }
    else if (vi_orig.IsRGB32()) {
      return env->Invoke("ConvertToRGB32", AVSValue(new_args2, 1)).AsClip();
    }
    else { // if (vi_orig.IsRGB64())
      return env->Invoke("ConvertToRGB64", AVSValue(new_args2, 1)).AsClip();
    }
  }
  else {
    return new Histogram(args[0].AsClip(), mode, args[2], args[3].AsInt(8), args[4].AsBool(true), args[5].AsBool(true), env);
  }
}
