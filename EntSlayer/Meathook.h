#include <string>
#include <vector>

/*
* A wrapper for the Meathook Interface object
* 
* All functions should return true if the operation is successful, and
* false if the operation failed
*/

enum GameInterface
{
	game_none = 0,
	game_eternal = 1,
	game_darkages = 2
};

namespace Meathook 
{
	GameInterface IsOnline();
	bool RemoveZOffset();
	void RemoveZOffset(bool remove);

	bool GetCurrentMap(std::string& filepath, bool deleteFileImmediately);
	bool ReloadMap(const std::string& filepath);
	bool ExecuteCommand(const std::string& cmd_mh, const std::string& cmd_kaibz, bool EditsClipboard);
	bool CopySpawnInfo();
	bool CopySpawnPosition();
	bool CopySpawnOrientation();
	bool GetActiveEncounters(std::vector<std::string>& encounterNames);
}