// Avisynth C Interface Version 0.20 stdcall
// Copyright 2003 Kevin Atkinson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include <avisynth.h>
#include <avisynth_c.h>
#include <avs/win.h>
#include <algorithm>
#include <cstdarg>


struct AVS_Clip
{
	PClip clip;
	IScriptEnvironment * env;
	const char * error;
	AVS_Clip() : env(0), error(0) {}
};

class C_VideoFilter : public IClip {
public: // but don't use
	AVS_Clip child;
	AVS_ScriptEnvironment env;
	AVS_FilterInfo d;
public:
	C_VideoFilter() {memset(&d,0,sizeof(d));}
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	void __stdcall GetAudio(void * buf, __int64 start, __int64 count, IScriptEnvironment* env);
	const VideoInfo & __stdcall GetVideoInfo();
	bool __stdcall GetParity(int n);
	int __stdcall SetCacheHints(int cachehints,int frame_range);
	AVSC_CC ~C_VideoFilter();
};

/////////////////////////////////////////////////////////////////////
//
//
//

extern "C"
int AVSC_CC avs_is_rgb48(const AVS_VideoInfo * p)
  { return ((p->pixel_type & AVS_CS_BGR24) == AVS_CS_BGR24) && ((p->pixel_type & AVS_CS_SAMPLE_BITS_MASK) == AVS_CS_SAMPLE_BITS_16); }

extern "C"
int AVSC_CC avs_is_rgb64(const AVS_VideoInfo * p)
  { return ((p->pixel_type & AVS_CS_BGR32) == AVS_CS_BGR32) && ((p->pixel_type & AVS_CS_SAMPLE_BITS_MASK) == AVS_CS_SAMPLE_BITS_16); }

extern "C"
int AVSC_CC avs_is_yv24(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YV24  & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yv16(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YV16  & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yv12(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YV12  & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yv411(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YV411 & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_y8(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_Y8    & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yuv444p16(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YUV444P16 & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yuv422p16(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YUV422P16 & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yuv420p16(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YUV420P16 & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_y16(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_Y16   & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yuv444ps(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YUV444PS & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yuv422ps(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YUV422PS & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_yuv420ps(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_YUV420PS & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_y32(const AVS_VideoInfo * p)
  { return (p->pixel_type & AVS_CS_PLANAR_MASK) == (AVS_CS_Y32   & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_444(const AVS_VideoInfo * p)
{ return ((p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_YUV444 & AVS_CS_PLANAR_FILTER)) ||
         ((p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_YUVA444 & AVS_CS_PLANAR_FILTER)); }

extern "C"
int AVSC_CC avs_is_422(const AVS_VideoInfo * p)
{ return ((p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_YUV422 & AVS_CS_PLANAR_FILTER)) ||
         ((p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_YUVA422 & AVS_CS_PLANAR_FILTER)); }

extern "C"
int AVSC_CC avs_is_420(const AVS_VideoInfo * p)
{ return ((p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_YUV420 & AVS_CS_PLANAR_FILTER)) ||
         ((p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_YUVA420 & AVS_CS_PLANAR_FILTER)); }

extern "C"
int AVSC_CC avs_is_y(const AVS_VideoInfo * p)
{ return (p->pixel_type & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == (AVS_CS_GENERIC_Y & AVS_CS_PLANAR_FILTER); }

extern "C"
int AVSC_CC avs_is_color_space(const AVS_VideoInfo * p, int c_space)
{
    return avs_is_planar(p) ?
    ((p->pixel_type & AVS_CS_PLANAR_MASK) == (c_space & AVS_CS_PLANAR_FILTER))
    :
    ( ((p->pixel_type & ~AVS_CS_SAMPLE_BITS_MASK & c_space) == (c_space & ~AVS_CS_SAMPLE_BITS_MASK)) && // RGB got sample bits
      ((p->pixel_type & AVS_CS_SAMPLE_BITS_MASK) == (c_space & AVS_CS_SAMPLE_BITS_MASK)) );
}

extern "C"
int AVSC_CC avs_is_yuva(const AVS_VideoInfo * p)
{ return !!(p->pixel_type&AVS_CS_YUVA ); }

extern "C"
int AVSC_CC avs_is_planar_rgb(const AVS_VideoInfo * p)
{ return !!(p->pixel_type&AVS_CS_PLANAR) && !!(p->pixel_type&AVS_CS_BGR) && !!(p->pixel_type&AVS_CS_RGB_TYPE); }

extern "C"
int AVSC_CC avs_is_planar_rgba(const AVS_VideoInfo * p)
{ return !!(p->pixel_type&AVS_CS_PLANAR) && !!(p->pixel_type&AVS_CS_BGR) && !!(p->pixel_type&AVS_CS_RGBA_TYPE); }

extern "C"
int AVSC_CC avs_get_plane_width_subsampling(const AVS_VideoInfo * p, int plane)
{
  try {
    return ((VideoInfo *)p)->GetPlaneWidthSubsampling(plane);
  }
  catch (const AvisynthError &err) {
    (void)err;  // silence warning about unused variable; variable is kept for debugging
    return -1;
  }
}

extern "C"
int AVSC_CC avs_get_plane_height_subsampling(const AVS_VideoInfo * p, int plane)
{
  try {
    return ((VideoInfo *)p)->GetPlaneHeightSubsampling(plane);
  }
  catch (const AvisynthError &err) {
    (void)err;  // silence warning about unused variable; variable is kept for debugging
    return -1;
  }
}

extern "C"
int AVSC_CC avs_bits_per_pixel(const AVS_VideoInfo * p)
{
  return ((VideoInfo *)p)->BitsPerPixel();
}

extern "C"
int AVSC_CC avs_bytes_from_pixels(const AVS_VideoInfo * p, int pixels)
{
  return ((VideoInfo *)p)->BytesFromPixels(pixels);
}

// This method should be called avs_row_size_p,
// but we won't change it anymore to avoid breaking
// the interface.
extern "C"
int AVSC_CC avs_row_size(const AVS_VideoInfo * p, int plane)
{
  return ((VideoInfo *)p)->RowSize(plane);
}

extern "C"
int AVSC_CC avs_bmp_size(const AVS_VideoInfo * vi)
{
  return ((VideoInfo *)vi)->BMPSize();
}


/////////////////////////////////////////////////////////////////////
//
//
//

extern "C"
int AVSC_CC avs_get_pitch_p(const AVS_VideoFrame * p, int plane)
{
  switch (plane) {
  case AVS_PLANAR_U: case AVS_PLANAR_V: return p->pitchUV;
  case AVS_PLANAR_A: return p->pitchA;}
  return p->pitch; // Y, G, B, R
}

extern "C"
int AVSC_CC avs_get_row_size_p(const AVS_VideoFrame * p, int plane)
{
  int r;

  switch (plane) {
  case AVS_PLANAR_U: case AVS_PLANAR_V:
    return (p->pitchUV) ? p->row_sizeUV : 0;

  case AVS_PLANAR_U_ALIGNED: case AVS_PLANAR_V_ALIGNED:
    if (p->pitchUV) {
      r = (p->row_sizeUV+FRAME_ALIGN-1)&(~(FRAME_ALIGN-1)); // Aligned rowsize
      return (r <= p->pitchUV) ? r : p->row_sizeUV;
    }
    else
      return 0;

  case AVS_PLANAR_ALIGNED: case AVS_PLANAR_Y_ALIGNED:
  case AVS_PLANAR_R_ALIGNED: case AVS_PLANAR_G_ALIGNED: case AVS_PLANAR_B_ALIGNED:
    r = (p->row_size+FRAME_ALIGN-1)&(~(FRAME_ALIGN-1)); // Aligned rowsize
       return (r <= p->pitch) ? r : p->row_size;
  case AVS_PLANAR_A:
    return (p->pitchA) ? p->row_sizeA : 0;
  case AVS_PLANAR_A_ALIGNED:
    if (p->pitchA) {
      r = (p->row_sizeA + FRAME_ALIGN - 1)&(~(FRAME_ALIGN - 1)); // Aligned rowsize
      return (r <= p->pitchA) ? r : p->row_sizeA;
    }
    else
      return 0;
  }
  return p->row_size;
}

extern "C"
int AVSC_CC avs_get_height_p(const AVS_VideoFrame * p, int plane)
{
  switch (plane) {
  case AVS_PLANAR_U: case AVS_PLANAR_V:
    return (p->pitchUV) ? p->heightUV : 0;
  case AVS_PLANAR_A:
    return (p->pitchA) ? p->height : 0;
  }
  return p->height; // Y, G, B, R, A
}

extern "C"
const BYTE * AVSC_CC avs_get_read_ptr_p(const AVS_VideoFrame * p, int plane)
{
  switch (plane) {
    case AVS_PLANAR_U: case AVS_PLANAR_B: return p->vfb->data + p->offsetU; // G is first. Then B,R order like U,V
    case AVS_PLANAR_V: case PLANAR_R:     return p->vfb->data + p->offsetV;
    case PLANAR_A: return p->vfb->data + p->offsetA;
    default:           return p->vfb->data + p->offset;} // PLANAR Y, PLANAR_G
}

extern "C"
int AVSC_CC avs_is_writable(const AVS_VideoFrame * p)
{
  if (p->refcount == 1 && p->vfb->refcount == 1) {
    InterlockedIncrement(&(p->vfb->sequence_number));
    return 1;
  }
  return 0;
}

extern "C"
BYTE * AVSC_CC avs_get_write_ptr_p(const AVS_VideoFrame * p, int plane)
{
  switch (plane) {
    case AVS_PLANAR_U: case AVS_PLANAR_B: return p->vfb->data + p->offsetU;
    case AVS_PLANAR_V: case AVS_PLANAR_R: return p->vfb->data + p->offsetV;
    case AVS_PLANAR_A: return p->vfb->data + p->offsetA;
    default:           break;
  }
  if (avs_is_writable(p)) {
    return p->vfb->data + p->offset; // Y,G
  }
  return 0;
}

extern "C"
void AVSC_CC avs_release_video_frame(AVS_VideoFrame * f)
{
  ((PVideoFrame *)&f)->~PVideoFrame();
}

extern "C"
AVS_VideoFrame * AVSC_CC avs_copy_video_frame(AVS_VideoFrame * f)
{
  AVS_VideoFrame * fnew;
  new ((PVideoFrame *)&fnew) PVideoFrame(*(PVideoFrame *)&f);
  return fnew;
}


extern "C"
int AVSC_CC avs_num_components(const AVS_VideoInfo * p)
{
    return ((VideoInfo *)p)->NumComponents();
}

extern "C"
int AVSC_CC avs_component_size(const AVS_VideoInfo * p)
{
    return ((VideoInfo *)p)->ComponentSize();
}

extern "C"
int AVSC_CC avs_bits_per_component(const AVS_VideoInfo * p)
{
    return ((VideoInfo *)p)->BitsPerComponent();
}



/////////////////////////////////////////////////////////////////////
//
// C_VideoFilter
//

PVideoFrame C_VideoFilter::GetFrame(int n, IScriptEnvironment* env)
{
	if (d.get_frame) {
		d.error = 0;
		AVS_VideoFrame * f = d.get_frame(&d, n);
		if (d.error)
			throw AvisynthError(d.error);
		PVideoFrame fr((VideoFrame *)f);
    ((PVideoFrame *)&f)->~PVideoFrame();
    return fr;
	} else {
		return d.child->clip->GetFrame(n, env);
	}
}

void __stdcall C_VideoFilter::GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env)
{
	if (d.get_audio) {
		d.error = 0;
		d.get_audio(&d, buf, start, count);
		if (d.error)
			throw AvisynthError(d.error);
	} else {
		d.child->clip->GetAudio(buf, start, count, env);
	}
}

const VideoInfo& __stdcall C_VideoFilter::GetVideoInfo()
{
	return *(VideoInfo *)&d.vi;
}

bool __stdcall C_VideoFilter::GetParity(int n)
{
	if (d.get_parity) {
		d.error = 0;
		int res = d.get_parity(&d, n);
		if (d.error)
			throw AvisynthError(d.error);
		return !!res;
	} else {
		return d.child->clip->GetParity(n);
	}
}

int __stdcall C_VideoFilter::SetCacheHints(int cachehints, int frame_range)
{
	if (d.set_cache_hints) {
		d.error = 0;
		int res = d.set_cache_hints(&d, cachehints, frame_range);
		if (d.error)
			throw AvisynthError(d.error);
		return res;
	}
	// We do not pass cache requests upwards, only to the hosted filter.
	return 0;
}

C_VideoFilter::~C_VideoFilter()
{
	if (d.free_filter)
		d.free_filter(&d);
}

/////////////////////////////////////////////////////////////////////
//
// AVS_Clip
//

extern "C"
void AVSC_CC avs_release_clip(AVS_Clip * p)
{
	delete p;
}

AVS_Clip * AVSC_CC avs_copy_clip(AVS_Clip * p)
{
	return new AVS_Clip(*p);
}

extern "C"
const char * AVSC_CC avs_clip_get_error(AVS_Clip * p) // return 0 if no error
{
	return p->error;
}

extern "C"
int AVSC_CC avs_get_version(AVS_Clip * p)
{
  return p->clip->GetVersion();
}

extern "C"
const AVS_VideoInfo * AVSC_CC avs_get_video_info(AVS_Clip  * p)
{
  return  (const AVS_VideoInfo  *)&p->clip->GetVideoInfo();
}


extern "C"
AVS_VideoFrame * AVSC_CC avs_get_frame(AVS_Clip * p, int n)
{
	p->error = 0;
	try {
		PVideoFrame f0 = p->clip->GetFrame(n,p->env);
		AVS_VideoFrame * f;
		new((PVideoFrame *)&f) PVideoFrame(f0);
		return f;
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return 0;
	}
}

extern "C"
int AVSC_CC avs_get_parity(AVS_Clip * p, int n) // return field parity if field_based, else parity of first field in frame
{
	try {
		p->error = 0;
		return p->clip->GetParity(n);
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

extern "C"
int AVSC_CC avs_get_audio(AVS_Clip * p, void * buf, INT64 start, INT64 count) // start and count are in samples
{
	try {
		p->error = 0;
		p->clip->GetAudio(buf, start, count, p->env);
		return 0;
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

extern "C"
int AVSC_CC avs_set_cache_hints(AVS_Clip * p, int cachehints, int frame_range)  // We do not pass cache requests upwards, only to the next filter.
{
	try {
		p->error = 0;
		return p->clip->SetCacheHints(cachehints, frame_range);
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

//////////////////////////////////////////////////////////////////
//
//
//
extern "C"
AVS_Clip * AVSC_CC avs_take_clip(AVS_Value v, AVS_ScriptEnvironment * env)
{
	AVS_Clip * c = new AVS_Clip;
	c->env  = env->env;
	c->clip = (IClip *)v.d.clip;
	return c;
}

extern "C"
void AVSC_CC avs_set_to_clip(AVS_Value * v, AVS_Clip * c)
{
	new(v) AVSValue(c->clip);
}

extern "C"
void AVSC_CC avs_copy_value(AVS_Value * dest, AVS_Value src)
{
  // true: don't copy array elements recursively
#ifdef NEW_AVSVALUE
  new(dest) AVSValue(*(const AVSValue *)&src, true);
#else
  new(dest) AVSValue(*(const AVSValue *)&src);
#endif
}

extern "C"
void AVSC_CC avs_release_value(AVS_Value v)
{
#ifdef NEW_AVSVALUE
  if (((AVSValue *)&v)->IsArray()) {
    // signing for destructor: don't free array elements
    ((AVSValue *)&v)->MarkArrayAsC();
}
#endif
  ((AVSValue *)&v)->~AVSValue();
}

//////////////////////////////////////////////////////////////////
//
//
//

extern "C"
AVS_Clip * AVSC_CC avs_new_c_filter(AVS_ScriptEnvironment * e,
							AVS_FilterInfo * * fi,
							AVS_Value child, int store_child)
{
	C_VideoFilter * f = new C_VideoFilter();
	AVS_Clip * ff = new AVS_Clip();
	ff->clip = f;
	ff->env  = e->env;
	f->env.env = e->env;
	f->d.env = &f->env;
	if (store_child) {
		_ASSERTE(child.type == 'c');
		f->child.clip = (IClip *)child.d.clip;
		f->child.env  = e->env;
		f->d.child = &f->child;
	}
	*fi = &f->d;
	if (child.type == 'c')
		f->d.vi = *(const AVS_VideoInfo *)(&((IClip *)child.d.clip)->GetVideoInfo());
	return ff;
}

/////////////////////////////////////////////////////////////////////
//
// AVS_ScriptEnvironment::add_function
//

struct C_VideoFilter_UserData {
	void * user_data;
	AVS_ApplyFunc func;
};

AVSValue __cdecl create_c_video_filter(AVSValue args, void * user_data,
									                     IScriptEnvironment * e0)
{
	C_VideoFilter_UserData * d = (C_VideoFilter_UserData *)user_data;
	AVS_ScriptEnvironment env;
	env.env = e0;
	env.error = NULL;

//	OutputDebugString("OK");
	AVS_Value res = (d->func)(&env, *(AVS_Value *)&args, d->user_data);
	if (res.type == 'e') {
    throw AvisynthError(res.d.string);
	} else {
    AVSValue val;
    val = (*(const AVSValue *)&res);
    ((AVSValue *)&res)->~AVSValue();
		return val;
	}
}

extern "C"
int AVSC_CC
  avs_add_function(AVS_ScriptEnvironment * p, const char * name, const char * params,
				   AVS_ApplyFunc applyf, void * user_data)
{
	C_VideoFilter_UserData *dd, *d = new C_VideoFilter_UserData;
	p->error = 0;
	d->func = applyf;
	d->user_data = user_data;
	dd = (C_VideoFilter_UserData *)p->env->SaveString((const char *)d, sizeof(C_VideoFilter_UserData));
	delete d;
	try {
		p->env->AddFunction(name, params, create_c_video_filter, dd);
	} catch (AvisynthError & err) {
		p->error = err.msg;
		return -1;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////
//
// AVS_ScriptEnvironment
//

extern "C"
const char * AVSC_CC avs_get_error(AVS_ScriptEnvironment * p) // return 0 if no error
{
	return p->error;
}

extern "C"
int AVSC_CC avs_get_cpu_flags(AVS_ScriptEnvironment * p)
{
	p->error = 0;
	return p->env->GetCPUFlags();
}

extern "C"
char * AVSC_CC avs_save_string(AVS_ScriptEnvironment * p, const char* s, int length)
{
	p->error = 0;
	return p->env->SaveString(s, length);
}

extern "C"
char * AVSC_CC avs_sprintf(AVS_ScriptEnvironment * p, const char* fmt, ...)
{
	p->error = 0;
	va_list vl;
	va_start(vl, fmt);
	char * v = p->env->VSprintf(fmt, (void *)vl);
	va_end(vl);
	return v;
}

 // note: val is really a va_list; I hope everyone typedefs va_list to a pointer
extern "C"
char * AVSC_CC avs_vsprintf(AVS_ScriptEnvironment * p, const char* fmt, void* val)
{
	p->error = 0;
	return p->env->VSprintf(fmt, val);
}

extern "C"
int AVSC_CC avs_function_exists(AVS_ScriptEnvironment * p, const char * name)
{
	p->error = 0;
	return p->env->FunctionExists(name);
}

extern "C"
AVS_Value AVSC_CC avs_invoke(AVS_ScriptEnvironment * p, const char * name, AVS_Value args, const char * * arg_names)
{
	AVS_Value v = {0,0};
	p->error = 0;
	try {
    AVSValue v0 = p->env->Invoke(name, *(AVSValue *)&args, arg_names);
		new ((AVSValue *)&v) AVSValue(v0);
	} catch (const IScriptEnvironment::NotFound&) {
    p->error = "Function Not Found";
	} catch (const AvisynthError &err) {
		p->error = err.msg;
	}
  if (p->error)
    v = avs_new_value_error(p->error);
	return v;
}

extern "C"
AVS_Value AVSC_CC avs_get_var(AVS_ScriptEnvironment * p, const char* name)
{
	AVS_Value v = {0,0};
	p->error = 0;
	try {
		AVSValue v0 = p->env->GetVar(name);
		new ((AVSValue *)&v) AVSValue(v0);
	}
	catch (const IScriptEnvironment::NotFound&) {}
	catch (const AvisynthError &err) {
		p->error = err.msg;
		v = avs_new_value_error(p->error);
	}
	return v;
}

extern "C"
int AVSC_CC avs_set_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val)
{
	p->error = 0;
	try {
		return p->env->SetVar(p->env->SaveString(name), *(const AVSValue *)(&val));
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

extern "C"
int AVSC_CC avs_set_global_var(AVS_ScriptEnvironment * p, const char* name, AVS_Value val)
{
	p->error = 0;
	try {
		return p->env->SetGlobalVar(p->env->SaveString(name), *(const AVSValue *)(&val));
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

extern "C"
AVS_VideoFrame * AVSC_CC avs_new_video_frame_a(AVS_ScriptEnvironment * p, const AVS_VideoInfo *  vi, int align)
{
	p->error = 0;
	try {
		PVideoFrame f0 = p->env->NewVideoFrame(*(const VideoInfo *)vi, align);
		AVS_VideoFrame * f;
		new((PVideoFrame *)&f) PVideoFrame(f0);
		return f;
	} catch (const AvisynthError &err) {
		p->error = err.msg;
	}
	return 0;
}

extern "C"
int AVSC_CC avs_make_writable(AVS_ScriptEnvironment * p, AVS_VideoFrame * * pvf)
{
	p->error = 0;
	try {
		return p->env->MakeWritable((PVideoFrame *)(pvf));
	} catch (const AvisynthError &err) {
		p->error = err.msg;
	}
	return -1;
}

extern "C"
void AVSC_CC avs_bit_blt(AVS_ScriptEnvironment * p, BYTE * dstp, int dst_pitch, const BYTE * srcp, int src_pitch, int row_size, int height)
{
	p->error = 0;
	try {
		p->env->BitBlt(dstp, dst_pitch, srcp, src_pitch, row_size, height);
	} catch (const AvisynthError &err) {
		p->error = err.msg;
	}
}

struct ShutdownFuncData
{
  AVS_ShutdownFunc func;
  void * user_data;
};

void __cdecl shutdown_func_bridge(void* user_data, IScriptEnvironment* env)
{
  ShutdownFuncData * d = (ShutdownFuncData *)user_data;
  AVS_ScriptEnvironment e;
  e.env = env;
  e.error = NULL;
  d->func(d->user_data, &e);
}

extern "C"
void AVSC_CC avs_at_exit(AVS_ScriptEnvironment * p,
                           AVS_ShutdownFunc function, void * user_data)
{
  p->error = 0;
  ShutdownFuncData *dd, *d = new ShutdownFuncData;
  d->func = function;
  d->user_data = user_data;
  dd = (ShutdownFuncData *)p->env->SaveString((const char *)d, sizeof(ShutdownFuncData));
  delete d;
  p->env->AtExit(shutdown_func_bridge, dd);
}

extern "C"
int AVSC_CC avs_check_version(AVS_ScriptEnvironment * p, int version)
{
	p->error = 0;
	try {
		p->env->CheckVersion(version);
		return 0;
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

extern "C"
AVS_VideoFrame * AVSC_CC avs_subframe(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0,
							  int rel_offset, int new_pitch, int new_row_size, int new_height)
{
	p->error = 0;
	try {
		PVideoFrame f0 = p->env->Subframe((VideoFrame *)src0, rel_offset, new_pitch, new_row_size, new_height);
		AVS_VideoFrame * f;
		new((PVideoFrame *)&f) PVideoFrame(f0);
		return f;
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return 0;
	}
}

extern "C"
AVS_VideoFrame * AVSC_CC avs_subframe_planar(AVS_ScriptEnvironment * p, AVS_VideoFrame * src0,
							  int rel_offset, int new_pitch, int new_row_size, int new_height,
							  int rel_offsetU, int rel_offsetV, int new_pitchUV)
{
	p->error = 0;
	try {
		PVideoFrame f0 = p->env->SubframePlanar((VideoFrame *)src0, rel_offset, new_pitch, new_row_size,
												new_height, rel_offsetU, rel_offsetV, new_pitchUV);
		AVS_VideoFrame * f;
		new((PVideoFrame *)&f) PVideoFrame(f0);
		return f;
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return 0;
	}
}

extern "C"
int AVSC_CC avs_set_memory_max(AVS_ScriptEnvironment * p, int mem)
{
	p->error = 0;
	try {
		return p->env->SetMemoryMax(mem);
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}

extern "C"
int AVSC_CC avs_set_working_dir(AVS_ScriptEnvironment * p, const char * newdir)
{
	p->error = 0;
	try {
		return p->env->SetWorkingDir(newdir);
	} catch (const AvisynthError &err) {
		p->error = err.msg;
		return -1;
	}
}
/////////////////////////////////////////////////////////////////////
//
//
//

extern "C"
AVS_ScriptEnvironment * AVSC_CC avs_create_script_environment(int version)
{
	AVS_ScriptEnvironment * e = new AVS_ScriptEnvironment;
	try {
		e->env = CreateScriptEnvironment(version);
		e->error = NULL;
	} catch (const AvisynthError &err) {
		e->error = err.msg;
		e->env = 0;
	}
	return e;
}


/////////////////////////////////////////////////////////////////////
//
//
//

extern "C"
void AVSC_CC avs_delete_script_environment(AVS_ScriptEnvironment * e)
{
	if (e) {
		if (e->env) {
			try {
				e->env->DeleteScriptEnvironment();
			} catch (const AvisynthError &err) {
                (void)err;  // silence warning about unused variable; variable is kept for debugging
            }
			e->env = 0;
		}
		delete e;
	}
}
