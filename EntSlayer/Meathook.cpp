#include "wx/clipbrd.h"
#include "mhclient.h"
#include "Meathook.h"
#include <WinUser.h>

MeathookInterface mh;                     // Meathook RPC Interface
HANDLE kaibzpipe = INVALID_HANDLE_VALUE;  // Pipe for Kaibz Interface

bool removeZOffset = true;
bool useAtlanZOffset = false;
const float POSITION_Z_OFFSET = 1.65375f;
const float POSITION_Z_OFFSET_ATLAN = 1.65375f; // TODO: When k_spawninfo works on Atlan, add checkbox to use Atlan offset

bool kaibzpipe_runcommand(std::string_view command)
{
	bool success = true;
	char buffer[1024];
	DWORD bytesRead, bytesWritten;
	DWORD available = 0;

	// --- Non-blocking read from server ---
	if (PeekNamedPipe(kaibzpipe, NULL, 0, NULL, &available, NULL) && available > 0) {
		ZeroMemory(buffer, sizeof(buffer));
		if (ReadFile(kaibzpipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
			//std::cout << "[Server] " << std::string(buffer, bytesRead) << "\n";
		}
	}

	if (!WriteFile(kaibzpipe, command.data(), (DWORD)command.length(), &bytesWritten, NULL)) {
		success = false;
	}

	// Flip-flops between connected and disconnected if we're constantly closing
	// and re-opening the connection. Hence, just shutdown only when a command fails to execute
	if (!success)
	{
		CloseHandle(kaibzpipe);
		kaibzpipe = INVALID_HANDLE_VALUE;
	}
	return success;
}

GameInterface Meathook::IsOnline()
{
	// TODO: At program start, this seems to return true for one cycle on some occassions
	// When we're not even running Eternal. Why?
	if(mh.m_Initialized)
		return game_eternal;

	if (kaibzpipe == INVALID_HANDLE_VALUE) {
		kaibzpipe = CreateFileA(R"(\\.\pipe\KaibzModPipe)", GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);
	}

	return kaibzpipe == INVALID_HANDLE_VALUE ? game_none : game_darkages;
}

bool Meathook::RemoveZOffset() {return removeZOffset;}

void Meathook::RemoveZOffset(bool remove) {removeZOffset = remove;}

bool Meathook::GetCurrentMap(std::string& filepath, bool deleteFileImmediately)
{
	if (mh.m_Initialized) {
		char Path[MAX_PATH];
		size_t PathSize = sizeof(Path);

		if (!mh.GetEntitiesFile((unsigned char*)Path, &PathSize))
			return false;

		if (deleteFileImmediately)
			std::remove(Path);
		filepath.append(Path, PathSize);
		return true;
	}

	// Kaibz interface is unlikely to ever support this
	// due to the need to deserialize the file (even if it could
	// be dumped to a temporary file)
	return false;
}

bool Meathook::ReloadMap(const std::string& filepath)
{
	if (mh.m_Initialized)
	{
		const size_t MAX = MAX_PATH;
		char Path[MAX];

		if (filepath.length() + 1 > MAX) return false;

		memcpy(&Path[0], filepath.data(), filepath.length());
		Path[filepath.length()] = '\0';

		return mh.PushEntitiesFile(Path, nullptr, 0);
	}

	// Hopefully temporary until Kaibz hot reloading is brought online
	return false;
}

bool Meathook::ExecuteCommand(const std::string& cmd_mh, const std::string& cmd_kaibz, bool EditsClipboard)
{
	// Meathook: NEED to clear the clipboard or any command that writes to it
	// will have a long delay and not work - contact Chrispy
	// Should further consider having safeguards in ExecuteCommand for this
	if (EditsClipboard)
	{
		OpenClipboard(NULL);
		EmptyClipboard();
		CloseClipboard();
	}

	if (mh.m_Initialized) {
		std::string copy = cmd_mh;
		return mh.ExecuteConsoleCommand((unsigned char*) copy.c_str());
	}
	
	if (kaibzpipe != INVALID_HANDLE_VALUE) {
		if(!kaibzpipe_runcommand(cmd_kaibz))
			return false;

		// Kaibz: Must wait for data to become available on the clipboard
		// Using this function ensures we're not blocking the clipboard from being accessed by other processes
		// If k_spawninfo silently fails this loop will never exit without a timeout
		if (EditsClipboard)
		{
			ULONGLONG timeout_starttime = GetTickCount64();
			while (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
				ULONGLONG timeout_current = GetTickCount64();
				if (timeout_current - timeout_starttime > 1000)
					return false;
			}
		}

		return true;
	}

	return false;
}

bool Meathook::CopySpawnInfo()
{

	if (!ExecuteCommand("mh_spawninfo", "k_spawninfo", true)) {
		return false;
	}

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

bool Meathook::GetActiveEncounters(std::vector<std::string>& encounterNames)
{
	if (mh.m_Initialized)
	{
		const size_t MAX = 2048;
		char buffer[MAX];
		int resultLength = MAX;

		if (!mh.GetActiveEncounter(&resultLength, buffer))
			return false;

		std::string output(buffer, resultLength);

		// Parse Output string for encounter names
		// Length 0: No encounters, Multiple encounter names separated by semicolons
		if (output.length() > 0)
		{
			size_t lastindex = 0;
			size_t index = output.find(';');
			while (index != std::string::npos) {
				encounterNames.push_back(output.substr(lastindex, index - lastindex));
				lastindex = index + 1;
				index = output.find(';', lastindex);
			}
			encounterNames.push_back(output.substr(lastindex, index - lastindex));
		}
		
		return true;
	}

	if (kaibzpipe != INVALID_HANDLE_VALUE) {

		if (!ExecuteCommand("", "k_activeEncounters", true))
			return false;

		wxClipboard* clipboard = wxTheClipboard->Get();
		if (!clipboard->Open())
			return false;

		wxTextDataObject data;
		clipboard->GetData(data);

		std::string output(data.GetText().ToStdString());
		clipboard->Close();

		// Parse Output string for encounter names
		size_t bracket = output.find(']');
		while (bracket != std::string::npos) {
			size_t namestart = bracket + 8; // "] Name: "

			size_t eol = namestart;
			while(output[eol] != '\r' && output[eol] != '\n')
				eol++;

			encounterNames.push_back(output.substr(namestart, eol - namestart));
			
			bracket = output.find(']', bracket + 1);
		}

		return true;
	}

	return false;
}
