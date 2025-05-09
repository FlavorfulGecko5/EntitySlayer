#include "wx/wx.h"
#include "wx/collpane.h"
#include "wx/splitter.h"
#include "EntityParser.h"
//#include <filesystem>

class EntityEditor;
class FilterCtrl;
class SpawnFilter;
class SearchBar;
class EntityTab : public wxPanel
{
	public:
	wxString tabName;
	wxString filePath;
	//std::filesystem::file_time_type openTime = std::filesystem::file_time_type::min();
	bool compressOnSave;
	bool compressOnSave_ForceDisable = false; // If true, disables compression regardless of setting
	bool autoNumberLists = true;

	FilterCtrl* layerMenu;
	FilterCtrl* classMenu;
	FilterCtrl* inheritMenu;
	FilterCtrl* componentMenu;
	FilterCtrl* keyMenu;
	wxCheckBox* caseSensCheck;
	SpawnFilter* spawnMenu;
	SearchBar* searchBar;
	wxGenericCollapsiblePane* topWrapper; // Filter pane

	wxMenu viewMenu;
	wxMenu* appendMenu = nullptr;

	EntNode* root;
	wxObjectDataPtr<EntityParser> Parser; // Need this or model leaks when tab destroyed
	wxDataViewCtrl* view;
	EntityEditor* editor;

	EntityTab(wxWindow* parent, bool nightMode, const wxString name, const wxString& path = "");
	void NightMode(bool nightMode, bool recursive);
	bool IsNewAndUntouched();
	bool UnsavedChanges();
	void setAppendMenu();
	void refreshFilters();
	void onFilterCaseCheck(wxCommandEvent& event);
	void onFilterDelKeys(wxCommandEvent& event);
	void onFilterRefresh(wxCommandEvent& event);
	void onFilterClearAll(wxCommandEvent& event);
	void filterSetSpawninfo();
	void applyFilters(bool clearAll);
	void SearchForward();
	void SearchBackward();
	void reloadFile();
	bool saveFile();
	int CommitEdits();
	void UndoRedo(bool undo);
	void onDataviewChar(wxKeyEvent &event);
	bool dataviewMouseAction(wxDataViewItem item);
	void onNodeSelection(wxDataViewEvent& event);
	void onNodeDoubleClick(wxDataViewEvent& event);
	void onViewRightMouseDown(wxMouseEvent& event);

	/*
	* To add a new right click menu option:
	* - Define a function for it
	* - Define enums
	* - Define event table entries
	* - Add to the right click menu
	* - Add accelerator table entry (if wanted)
	* - Add condition for enabling the menu option in contextMenu function
	* - Add entry in ContextAccelerator function's switch statement (if wanted)
	*/
	void onNodeContextMenu(wxDataViewEvent& event);
	void onNodeContextAccelerator(wxCommandEvent& event);
	void onUndo(wxCommandEvent &event);
	void onRedo(wxCommandEvent &event);
	void onCutSelectedNodes(wxCommandEvent &event);
	void onCopySelectedNodes(wxCommandEvent &event);
	void onPaste(wxCommandEvent& event);
	void onExpandEntity(wxCommandEvent &event); // Double click action - just here for documentation purposes
	void onSelectAllEntities(wxCommandEvent &event);
	void onDeleteSelectedNodes(wxCommandEvent &event);
	void onSetSpawnPosition(wxCommandEvent &event);
	void onSetSpawnOrientation(wxCommandEvent &event);
	void onTeleportToEntity(wxCommandEvent &event);

	void onFilterMenuShowHide(wxCollapsiblePaneEvent& event);

	void action_PropMovers();
	void action_FixTraversals();

	private:
	wxDECLARE_EVENT_TABLE();
};