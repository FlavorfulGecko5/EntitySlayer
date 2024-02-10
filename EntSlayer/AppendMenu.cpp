#pragma warning(disable : 4996) // Deprecation errors
#include <vector>
#include "EntityParser.h"
#include "AppendMenu.h"

/*
 Parsed AppendMenu file data
*/
EntityParser* data = nullptr;

/*
 Menu to deep-copy every time a menu is requested

 (Building a menu by performing parse tree navigation each time would be horribly inefficient)
*/
wxMenu* templateMenu = new wxMenu; // Not nullptr, so if initial load fails append menu will be empty

/*
 To prevent conflicts with other menu ids, we start with
 a sizeable offset from wxID_HIGHEST
*/
const int MENU_ID_OFFSET = wxID_HIGHEST + 2000;

/*
 There is a 16-bit limit to menu item IDs. Must be < 32767

 Hence, we use a vector to map (menuID - offset) values to EntNodes
 instead of using a positionalID system
*/
std::vector<EntNode*> idMap;

void recursiveBuildMenu(wxMenu* parentMenu, EntNode* node)
{
	if (node->getChildCount() == 0)
		return;
	EntNode& textNode = (*node)["text"];

	// If missing text node, assume this should be a submenu
	if (&textNode == EntNode::SEARCH_404) {
		wxMenu* subMenu = new wxMenu;

		for (int i = 0, max = node->getChildCount(); i < max; i++)
			recursiveBuildMenu(subMenu, node->ChildAt(i));

		parentMenu->AppendSubMenu(subMenu, node->getNameWX());
	}
	else {
		int index = idMap.size();
		idMap.push_back(node);

		wxString hotkey = (*node)["hotkey"].getValueWXUQ();
		wxString menuText = node->getNameWX();
		if (hotkey.length() > 0)
			menuText.Append('\t' + hotkey);

		parentMenu->Append(index + MENU_ID_OFFSET, menuText);
	}
}

bool AppendMenuInterface::loadData()
{
	const std::string filepath = "EntitySlayer_AppendMenu.txt";

	try {
		EntityParser* newParser = new EntityParser(filepath, ParsingMode::PERMISSIVE, false);
		delete data;
		data = newParser;
	}
	catch (std::runtime_error e) {
		wxString msg = wxString::Format("Append Menu Reading Failed\n\n%s", e.what());
		wxMessageBox(msg, filepath, wxICON_ERROR | wxOK);
		return false;
	}

	idMap.clear();
	delete templateMenu;
	templateMenu = new wxMenu;

	EntNode* root = data->getRoot();
	for(int i = 0, max = root->getChildCount(); i < max; i++)
		recursiveBuildMenu(templateMenu, root->ChildAt(i));

	return true;
}

void recursiveCloneMenu(wxMenu* copyFrom, wxMenu* copyTo) 
{
	size_t max = copyFrom->GetMenuItemCount();
	for (size_t i = 0; i < max; i++) {
		wxMenuItem* item = copyFrom->FindItemByPosition(i);
		wxMenu* originalSubmenu = item->GetSubMenu();

		if (originalSubmenu == nullptr)
			copyTo->Append(item->GetId(), item->GetItemLabel());
		else {
			wxMenu* newSubmenu = new wxMenu;
			recursiveCloneMenu(originalSubmenu, newSubmenu);
			copyTo->AppendSubMenu(newSubmenu, item->GetItemLabel());
		}
	}
}

wxMenu* AppendMenuInterface::getMenu()
{
	wxMenu* appendMenu = new wxMenu;
	recursiveCloneMenu(templateMenu, appendMenu);
	return appendMenu;
}

void AppendMenuInterface::deleteData()
{
	delete data;
	delete templateMenu;
	idMap.clear();
}

/*
* PARAMETER BRAINSTORMING:
* - Inherit from wxDialog
* - use ShowModal - to create a "modal" dialog that blocks program flow until completed
* - If text column and textbox column: Textboxes have a default height of 23 pixels (use to maintain alignment)
* 
* Going with Layout #2 (text column and box column instead of horizontal sizer for each text/box pair): Important stuff to keep in mind
* > To maintain horizontal alignment, give the statictext vertical size value of 23 units (same value as textctrl's default)
* > To prevent outrageously large parameter names from running the textboxes off the page, we must do two things:
*	- Set the width of the statictexts to a set value - combined with the dialog size, use this to control the width proportion of the two columns
*   - Give the statictexts an ellipse style i.e. wxST_ELLIPSIZE_END
* > Currenty unaccounted for: how will scrolling work if there are a lot of parameters? - No sane template will need to be able to scroll, but should still handle it
* > Additionally: Investigate possibility of dynamically setting width of dialog based on largest text length (or capping it if it's too long)
*/

class ParameterDialog : public wxDialog {
	private:
	std::vector<wxTextCtrl*> boxes;

	public:
	std::vector<std::string> values;

	ParameterDialog(const wxString& name, EntNode& args) : wxDialog(nullptr, wxID_ANY, "Append: " + name, 
		wxDefaultPosition, wxDefaultSize) 
	{		
		wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
		wxScrolledWindow* parmWindow = new wxScrolledWindow(this, wxID_ANY,
			wxDefaultPosition, wxDefaultSize, wxVSCROLL); // This style doesn't actually work?
		wxBoxSizer* parmSizer = new wxBoxSizer(wxHORIZONTAL);
		wxBoxSizer* col0 = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer* col1 = new wxBoxSizer(wxVERTICAL);
		wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
		btnSizer->Add(new wxButton(this, wxID_OK, "Append"));
		btnSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxLEFT, 5);

		boxes.reserve(args.getChildCount());
		for (int i = 0, max = args.getChildCount(); i < max; i++) 
		{
			EntNode* child = args.ChildAt(i);
			if(child->getType() == NodeType::COMMENT)
				continue;

			// Goal: Maximum window width should be 1000
			wxString name = child->getNameWXUQ();
			int textWidth = wxWindow::GetTextExtent(name).x;
			if(textWidth > 750) textWidth = 750;

			wxStaticText* text = new wxStaticText(parmWindow, wxID_ANY, name,
				wxDefaultPosition, wxSize(textWidth, 23), wxST_ELLIPSIZE_END);
			wxTextCtrl* input = new wxTextCtrl(parmWindow, wxID_ANY, child->getValueWX(),
				wxDefaultPosition, wxSize(150, -1), wxTE_PROCESS_ENTER); // Otherwise Enter will trigger the last-highlighted button

			col0->Add(text);
			col1->Add(input, 1, wxEXPAND);
			boxes.push_back(input);
		}
		
		parmSizer->Add(col0, 0, wxRIGHT, 10);
		parmSizer->Add(col1, 1);
		parmWindow->SetScrollRate(0, 10);        // Scrollbars won't render without this, 0 to turn off horizontal scrollbar
		parmWindow->SetMaxSize(wxSize(-1, 500)); // If max size exceeded, scrolling will be activated
		parmWindow->SetSizerAndFit(parmSizer);   // Todo: Scrollbar will overlap with textboxes - doesn't seem like an easy way to fix it, but shouldn't be a big deal
		
		mainSizer->Add(parmWindow, 0, wxEXPAND | wxALL, 7);
		mainSizer->Add(btnSizer, 0, wxCENTER | wxALL, 5);
		SetSizerAndFit(mainSizer);
		CenterOnScreen();
		ShowModal();

		// Should this even be called when an instance is stack allocated? Todo: Figure this out, consider dynamic allocation for safety?
		Destroy(); // Queues this window to be destroyed after all events have been handled

		values.reserve(boxes.size());
		for(wxTextCtrl* t : boxes)
			values.emplace_back(t->GetValue());
	}
};

bool AppendMenuInterface::getText(const int menuID, std::string& buffer)
{
	int index = menuID - MENU_ID_OFFSET;
	if(index < 0 || index >= idMap.size())
		return false;

	// Get the raw text
	std::string unsubbed;
	EntNode* node = idMap[index];
	EntNode& textNode = (*node)["text"];
	for (int i = 0, max = textNode.getChildCount(); i < max; i++) { // Todo: Should really have a "Generate Children Text" function
		textNode.ChildAt(i)->generateText(unsubbed);
		unsubbed.push_back('\n');
	}

	// Parse arguments, if they were specified
	EntNode& args = (*node)["args"];

	// Ensures we don't wind up with a dialog with 0 text boxes
	bool hasValidArg = false;
	for(int i = 0, max = args.getChildCount(); i < max; i++)
		if (args.ChildAt(i)->getType() != NodeType::COMMENT) {
			hasValidArg = true;
			break;
		}

	if (hasValidArg)
	{
		ParameterDialog prompt(node->getNameWX(), args);
		if (prompt.GetReturnCode() == wxID_CANCEL)
			return false;

		std::vector<std::string>& values = prompt.values;
		size_t valueCount = values.size();
		char *inc = unsubbed.data(), 
			 *max = inc + unsubbed.length(),
			 *blockStart = inc;

		while (inc < max) {
			if(*inc != '%')  {
				inc++;
				continue;
			}

			// Append everything before percent
			buffer.append(blockStart, (size_t)(inc - blockStart));
			
			// Find second percent
			LABEL_INNER_LOOP:
			blockStart = inc++; // start points to first percent
			bool foundSecondPercent = false;
			while (inc < max) {
				if (*inc == '%') {
					foundSecondPercent = true;
					break;
				}
				else inc++;
			}

			// Edge Case: No second percent
			if (!foundSecondPercent) break;

			// Extract digit from percent block
			// Todo: Find a way to do this without creating a string?
			bool validIndex = false;
			int num = 0;
			try {
				std::string numString(blockStart + 1, (size_t)(inc - blockStart));
				num = std::stoi(numString);
				if(num > -1 && num < valueCount)
					validIndex = true;
			}
			catch (std::exception) {}

			// Not a valid index:
			if (!validIndex) { 
				buffer.append(blockStart, (size_t)(inc - blockStart));
				goto LABEL_INNER_LOOP; // Second parentheses may be used in a valid parameter
			}
			else { // Valid index
				buffer.append(values[num]);
				blockStart = ++inc;
			}
		}
		buffer.append(blockStart, (size_t)(max - blockStart));
	}
	else buffer.append(unsubbed);
	return true;
}

