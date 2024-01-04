#include <string>
#include <chrono>

namespace EntityLogger
{
	void log(const std::string& data);
	void logWarning(const std::string& data);
	void logTimeStamps(const std::string& msg, 
		const std::chrono::steady_clock::time_point startTime);
}