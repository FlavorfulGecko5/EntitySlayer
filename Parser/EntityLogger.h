#include <string>
#include <chrono>
namespace EntityLogger 
{
	inline void log(const std::string& data)
	{
		//std::cout << data << std::endl;
	}

	inline void logWarning(const std::string& data)
	{
		//std::cout << data << std::endl;
	}
	
	inline void logTimeStamps( const std::string& msg,
						const std::chrono::steady_clock::time_point startTime,
						const std::chrono::steady_clock::time_point stopTime)
	{
		//auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);
		//std::cout << msg << duration.count() << std::endl;
	}
}