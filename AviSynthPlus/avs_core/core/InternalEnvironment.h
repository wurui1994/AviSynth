#ifndef _AVS_SCRIPTENVIRONMENT_H_INCLUDED
#define _AVS_SCRIPTENVIRONMENT_H_INCLUDED

#include <avisynth.h>
#include <algorithm>
#include <string>

class ClipDataStore;

typedef enum _ELogLevel
{
    LOGLEVEL_NONE = 0,
    LOGLEVEL_ERROR = 1,
    LOGLEVEL_WARNING = 2,
    LOGLEVEL_INFO = 3,
    LOGLEVEL_DEBUG = 4
} ELogLevel;

typedef enum _ELogTicketType
{
    LOGTICKET_W1000 = 1000, // leaks during shutdown
    LOGTICKET_W1001 = 1001, // source plugin with no mt-mode
    LOGTICKET_W1002 = 1002, // buggy SetCacheHints()
    LOGTICKET_W1003 = 1003, // too stringent memory limit
    LOGTICKET_W1004 = 1004, // filter completely without mt-mode
    LOGTICKET_W1005 = 1005, // filter with inconsequent MT-modes
    LOGTICKET_W1006 = 1006, // filter with redundant MT-modes
    LOGTICKET_W1007 = 1007, // user should try 64-bit AVS for more memory
    LOGTICKET_W1008 = 1008, // multiple plugins define the same function
    LOGTICKET_W1009 = 1009, // a filter is using forced alignment
    LOGTICKET_W1010 = 1010, // MT-mode specified for script function
} ELogTicketType;

class OneTimeLogTicket
{
public:
    ELogTicketType _type;
    const AVSFunction *_function = nullptr;
    const std::string _string;

    OneTimeLogTicket(ELogTicketType type);
    OneTimeLogTicket(ELogTicketType type, const AVSFunction *func);
    OneTimeLogTicket(ELogTicketType type, const std::string &str);
    bool operator==(const OneTimeLogTicket &other) const;
};

// Strictly for Avisynth core only.
// Neither host applications nor plugins should use
// these interfaces.
class InternalEnvironment : public IScriptEnvironment2 {
public:
    virtual ~InternalEnvironment() {}

    virtual int __stdcall IncrImportDepth() = 0;
    virtual int __stdcall DecrImportDepth() = 0;
    virtual void __stdcall AdjustMemoryConsumption(size_t amount, bool minus) = 0;
    virtual bool __stdcall FilterHasMtMode(const AVSFunction* filter) const = 0;
    virtual MtMode __stdcall GetFilterMTMode(const AVSFunction* filter, bool* is_forced) const = 0; // If filter is "", gets the default MT mode
    virtual void __stdcall SetPrefetcher(Prefetcher *p) = 0;
    virtual ClipDataStore* __stdcall ClipData(IClip *clip) = 0;
    virtual MtMode __stdcall GetDefaultMtMode() const = 0;
    virtual void __stdcall SetLogParams(const char *target, int level) = 0;
    virtual void __stdcall LogMsg(int level, const char* fmt, ...) = 0;
    virtual void __stdcall LogMsg_valist(int level, const char* fmt, va_list va) = 0;
    virtual void __stdcall LogMsgOnce(const OneTimeLogTicket &ticket, int level, const char* fmt, ...) = 0;
    virtual void __stdcall LogMsgOnce_valist(const OneTimeLogTicket &ticket, int level, const char* fmt, va_list va) = 0;
    virtual void __stdcall VThrowError(const char* fmt, va_list va) = 0;
    virtual PVideoFrame __stdcall SubframePlanarA(PVideoFrame src, int rel_offset, int new_pitch, int new_row_size, int new_height, int rel_offsetU, int rel_offsetV, int new_pitchUV, int rel_offsetA) = 0;
};

#endif // _AVS_SCRIPTENVIRONMENT_H_INCLUDED
