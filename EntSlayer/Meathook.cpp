#include "wx/clipbrd.h"
#include "mhclient.h"
#include "Meathook.h"
#include <WinUser.h>

MeathookInterface mh;
bool removeZOffset = true;
const float POSITION_Z_OFFSET = 1.65375f;

bool Meathook::IsOnline() 
{
	return mh.m_Initialized;
}

bool Meathook::RemoveZOffset() {return removeZOffset;}

void Meathook::RemoveZOffset(bool remove) {removeZOffset = remove;}

bool Meathook::GetCurrentMap(std::string& filepath, bool deleteFileImmediately)
{
	char Path[MAX_PATH];
	size_t PathSize = sizeof(Path);

	if(!mh.GetEntitiesFile((unsigned char*)Path, &PathSize))
		return false;

	if(deleteFileImmediately)
		std::remove(Path);
	filepath.append(Path, PathSize);
	return true;
}

bool Meathook::ReloadMap(const std::string& filepath)
{
	const size_t MAX = MAX_PATH;
	char Path[MAX];

	if(filepath.length() + 1 > MAX) return false;

	memcpy(&Path[0], filepath.data(), filepath.length());
	Path[filepath.length()] = '\0';

	return mh.PushEntitiesFile(Path, nullptr, 0);
}

bool Meathook::ExecuteCommand(const std::string& cmd)
{
	const size_t MAX = 1024;
	char buffer[MAX];

	if(cmd.length() + 1 > MAX) return false;

	memcpy(&buffer[0], cmd.data(), cmd.length());
	buffer[cmd.length()] = '\0';

	return mh.ExecuteConsoleCommand((unsigned char*) buffer);
}

bool Meathook::CopySpawnInfo()
{
	// NEED to clear the clipboard or any command that writes to it
	// will have a long delay and not work - contact Chrispy
	// Should further consider having safeguards in ExecuteCommand for this
	OpenClipboard(NULL);
	EmptyClipboard();
	CloseClipboard();
	
	if(!ExecuteCommand("mh_spawninfo")) return false;

	if (removeZOffset) 
	{
		wxClipboard* clipboard = wxTheClipboard->Get();
		if(!clipboard->Open())
			return false;

		wxTextDataObject data;
		clipboard->GetData(data);

		wxString s(data.GetText());
		size_t zIndex = s.rfind("z = ") + 4; // spawninfo lists position
		size_t semiIndex = s.rfind(';');     // after orientation

		wxString zString = s.substr(zIndex, semiIndex - zIndex);
		float zValue = std::stof(std::string(zString)) - POSITION_Z_OFFSET;

		wxString newString = s.substr(0, zIndex) + std::to_string(zValue)
			+ s.substr(semiIndex, s.length() - semiIndex);

		clipboard->SetData(new wxTextDataObject(newString));
		clipboard->Close();
	}
	return true;
}

bool Meathook::CopySpawnPosition()
{
	if(!CopySpawnInfo()) return false;

	wxClipboard* clipboard = wxTheClipboard->Get();
	if(!clipboard->Open()) 
		return false;

	wxTextDataObject data;
	clipboard->GetData(data);

	wxString s(data.GetText());
	size_t spawnIndex = s.rfind("spawnPosition");
	
	wxString newString = s.substr(spawnIndex, s.length() - spawnIndex);

	clipboard->SetData(new wxTextDataObject(newString));
	clipboard->Close();
	return true;
}

bool Meathook::CopySpawnOrientation()
{
	if(!CopySpawnInfo()) return false;

	wxClipboard* clipboard = wxTheClipboard->Get();
	if (!clipboard->Open())
		return false;

	wxTextDataObject data;
	clipboard->GetData(data);

	wxString s(data.GetText());
	// Makes sure we don't copy the newline after spawnOrientation's end brace
	size_t orientationLength = s.rfind('}', s.length() - 2) + 1;
	wxString newString = s.substr(0, orientationLength);

	clipboard->SetData(new wxTextDataObject(newString));
	clipboard->Close();
	return true;
}