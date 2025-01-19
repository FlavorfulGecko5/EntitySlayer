#pragma once

#include <chrono>
#include "wx/log.h"

/* Standard Profiling */

#define TIMESTART(ID) auto EntityProfiling_ID  = std::chrono::high_resolution_clock::now();

#define TIMESTOP(ID, msg) { \
	auto timeStop = std::chrono::high_resolution_clock::now(); \
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - EntityProfiling_ID); \
	wxLogMessage("%s: %zu", msg, duration.count());\
}

/* Debug Build Only Profiling */

#ifdef _DEBUG

#define TIMESTART_DEBUG(ID) auto EntityProfiling_ID  = std::chrono::high_resolution_clock::now();

#define TIMESTOP_DEBUG(ID, msg) { \
	auto timeStop = std::chrono::high_resolution_clock::now(); \
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - EntityProfiling_ID); \
	wxLogMessage("%s: %zu", msg, duration.count());\
}

#else
#define TIMESTART_DEBUG
#define TIMESTOP_DEBUG
#endif

