#include <fstream>
#include "wx/string.h"
#include "Oodle.h"
#include "EntityLogger.h"
#include "EntityNode.h"
/*
* This file should be used to define any fields or functions
* that can't be defined in the header files
*/

EntNode* EntNode::SEARCH_404 = new EntNode();

wxString EntNode::getNameWX() { return wxString(textPtr, nameLength); }

wxString EntNode::getValueWX() { return wxString(textPtr + nameLength, valLength); }

wxString EntNode::getNameWXUQ() {
	if(nameLength < 2 || *textPtr != '"')
		return wxString(textPtr, nameLength);
	return wxString(textPtr + 1, nameLength - 2);
}

wxString EntNode::getValueWXUQ() {
	if(valLength < 2 || textPtr[nameLength] != '"')
		return wxString(textPtr + nameLength, valLength);
	return wxString(textPtr + nameLength + 1, valLength - 2);
}

void EntNode::writeToFile(const std::string filepath, const bool oodleCompress, const bool debug_logTime)
{
	auto timeStart = std::chrono::high_resolution_clock::now();
	// 25% of time spent writing to output buffer, 75% on generateText

	std::string raw;
	generateText(raw);

	std::ofstream output(filepath, std::ios_base::binary);

	if (oodleCompress)
	{
		char* compressedData = new char[raw.length() + 65536];
		size_t compressedSize;
		if (Oodle::CompressBuffer(raw.data(), raw.length(), compressedData + 16, compressedSize)) {
			((size_t*)compressedData)[0] = raw.length();
			((size_t*)compressedData)[1] = compressedSize;
			output.write(compressedData, compressedSize + 16);
		}
		else {
			EntityLogger::logWarning("Failed to compress .entities file. Saving uncompressed version instead.");
			output << raw;
		}
		delete[] compressedData;
	}
	else output << raw;

	output.close();
	if (debug_logTime)
		EntityLogger::logTimeStamps("Writing Duration: ", timeStart);
}