#include <string>

class wxWindow;
class wxMenu;
namespace AppendMenuInterface 
{
	bool loadData();
	void deleteData();

	wxMenu* getMenu();
	bool getText(const int menuID, std::string &buffer);
}