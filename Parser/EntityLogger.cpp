#include "wx/wx.h"
#include "EntityLogger.h"

void EntityLogger::log(const std::string& data)
{
	wxLogMessage("%s", data);
}

void EntityLogger::logWarning(const std::string& data)
{
	wxMessageBox(wxString(data), "Parser Warning", wxICON_WARNING | wxOK);
}

void EntityLogger::logTimeStamps(const std::string& msg,
	const std::chrono::steady_clock::time_point startTime)
{
	auto stopTime = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);

	wxLogMessage("%s %zu", msg, duration.count());
}