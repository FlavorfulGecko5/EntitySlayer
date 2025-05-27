#include <string>

class wxWindow;
class wxMenu;

struct EditorConfig_t {
	int fontSize = 10;
	bool nightMode = false;
};

namespace ConfigInterface 
{
	const char* ConfigPath();

	bool loadData();
	void deleteData();

	EditorConfig_t GetEditorConfig();
	bool NightMode();
	void SetNightMode(bool newVal);

	wxMenu* getMenu();
	bool getText(const int menuID, std::string &buffer);
}