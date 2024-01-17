#include <string>

/*
* A wrapper for the Meathook Interface object
* 
* All functions should return true if the operation is successful, and
* false if the operation failed
*/
namespace Meathook 
{
	bool IsOnline();
	bool RemoveZOffset();
	void RemoveZOffset(bool remove);

	bool GetCurrentMap(std::string& filepath, bool deleteFileImmediately);
	bool ReloadMap(const std::string& filepath);
	bool ExecuteCommand(const std::string& cmd);
	bool CopySpawnInfo();
	bool CopySpawnPosition();
	bool CopySpawnOrientation();
	bool GetActiveEncounters(std::string& encounterNames);
}