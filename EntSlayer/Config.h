#include <string>

class wxWindow;
class wxMenu;
namespace ConfigInterface 
{
	bool loadData();
	void deleteData();

	wxMenu* getMenu();
	bool getText(const int menuID, std::string &buffer);
}