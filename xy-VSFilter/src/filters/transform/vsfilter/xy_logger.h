/************************************************************************/
/* author: xy                                                           */
/* date: 20110511                                                       */
/************************************************************************/
#ifndef __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__
#define __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__

#include "log4cplus/logger.h"
#include "log4cplus/configurator.h"
#include "timing_logger.h"
#include "once_logger.h"
#include <stdio.h>

#ifdef __DO_LOG

#define XY_LOG_VAR_2_STR(var) " "#var"='"<<(var)<<"' "
#define XY_LOG_TRACE(msg) LOG4CPLUS_TRACE(xy_logger::g_logger, msg)
#define XY_LOG_DEBUG(msg) LOG4CPLUS_DEBUG(xy_logger::g_logger, msg)
#define XY_LOG_INFO(msg) LOG4CPLUS_INFO(xy_logger::g_logger, msg)
#define XY_LOG_WARN(msg) LOG4CPLUS_WARN(xy_logger::g_logger, msg)
#define XY_LOG_ERROR(msg) LOG4CPLUS_ERROR(xy_logger::g_logger, msg)
#define XY_LOG_FATAL(msg) LOG4CPLUS_FATAL(xy_logger::g_logger, msg)

extern int g_log_once_id;

#define XY_AUTO_TIMING(msg) TimingLogger(xy_logger::g_logger, msg, __FILE__, __LINE__)
#define XY_LOG_ONCE(id, msg) OnceLogger(id, xy_logger::g_logger, msg, __FILE__, __LINE__)
#define XY_LOG_ONCE2(msg) {\
    static bool entered=false;\
    if(!entered)\
    {\
        entered=true;\
        LOG4CPLUS_INFO(xy_logger::g_logger, msg);\
    }\
}

#define XY_DO_ONCE(expr) do {\
	static bool entered=false;\
    if(!entered) { \
        entered = true;\
	    {expr;}\
    }\
} while(0)


#define DO_FOR(num, expr) do {\
    static int repeat_num=(num);\
    if (repeat_num>0)\
    {\
        repeat_num--;\
        expr;\
    }\
} while(0)

#else //__DO_LOG

#define XY_LOG_VAR_2_STR(var)
#define XY_LOG_TRACE(msg)
#define XY_LOG_DEBUG(msg)
#define XY_LOG_INFO(msg)
#define XY_LOG_WARN(msg)
#define XY_LOG_ERROR(msg)
#define XY_LOG_FATAL(msg)

#define XY_AUTO_TIMING(msg)
#define XY_LOG_ONCE(id, msg)
#define XY_DO_ONCE(expr)
#define DO_FOR(num, expr)

#endif



namespace xy_logger
{
#ifdef __DO_LOG
extern log4cplus::Logger g_logger;
#endif

bool doConfigure(log4cplus::tistream& property_stream);
bool doConfigure(const log4cplus::tstring& configFilename);

void write_file(const char * filename, const void * buff, int size);

void DumpPackBitmap2File(POINT pos, SIZE size, LPCVOID pixels, int pitch, const char *filename);

} //namespace xy

#endif // end of __XY_LOGGER_H_3716A27F_2940_4636_8BC9_C764648362AE__
