﻿/*
  ConditionalReader  (c) 2004 by Klaus Post

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  The author can be contacted at:
  sh0dan[at]stofanet.dk
*/

#include <avisynth.h>
#include <cstdio>
#include <cstdlib>



enum {
  MODE_UNKNOWN = -1,
  MODE_INT = 1,
  MODE_FLOAT = 2,
  MODE_BOOL = 3,
  MODE_STRING = 4
};

struct StringCache {
  char* string;
  StringCache *next;
};

class ConditionalReader : public GenericVideoFilter
{
private:
  const bool show;
  const char* variableName;
  int mode;
  int offset;
  StringCache* stringcache;
  union {
    int* intVal;
    bool* boolVal;
    float* floatVal;
    const char* *stringVal;
  };

  AVSValue ConvertType(const char* content, int line, IScriptEnvironment* env);
  void SetRange(int start_frame, int stop_frame, AVSValue v);
  void SetFrame(int framenumber, AVSValue v);
  void ThrowLine(const char* err, int line, IScriptEnvironment* env);
  AVSValue GetFrameValue(int framenumber);
  void CleanUp(void);

public:
  ConditionalReader(PClip _child, const char* filename, const char _varname[], bool _show, IScriptEnvironment* env);
  ~ConditionalReader(void);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);
};


/* ------------------------------------------------------------------------------
** Write function to evaluate expressions per frame and write the results to file
** Ernst Peché, 2004
*/

class Write : public GenericVideoFilter
{
private:
	FILE * fout;
	int linecheck;	// 0=write each line, 1=write only if first expression == true, -1 = write at start, -2 = write at end
	bool flush;
	bool append;

	char filename[_MAX_PATH];
	int arrsize;
	struct exp_res {
		const char* expression;
		const char* string;
	};
	exp_res* arglist;

	bool DoEval(IScriptEnvironment* env);
	void FileOut(IScriptEnvironment* env, const char* mode);

public:
    Write(PClip _child, const char* _filename, AVSValue args, int _linecheck, bool _flush, bool _append, IScriptEnvironment* env);
	~Write(void);
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
	static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);
	static AVSValue __cdecl Create_If(AVSValue args, void* user_data, IScriptEnvironment* env);
	static AVSValue __cdecl Create_Start(AVSValue args, void* user_data, IScriptEnvironment* env);
	static AVSValue __cdecl Create_End(AVSValue args, void* user_data, IScriptEnvironment* env);
};
