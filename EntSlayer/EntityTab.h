#include "wx/wx.h"
#include "wx/collpane.h"
#include "wx/splitter.h"
#include "EntityParser.h"

class EntityEditor;
class FilterCtrl;
class SpawnFilter;
class SearchBar;
class EntityTab : public wxPanel
{
	public:
	wxString tabName;
	wxString filePath;
	bool fileUpToDate = true;
	bool compressOnSave;
	bool autoNumberLists = true;

	FilterCtrl* layerMenu;
	FilterCtrl* classMenu;
	FilterCtrl* inheritMenu;
	FilterCtrl* keyMenu;
	wxCheckBox* caseSensCheck;
	SpawnFilter* spawnMenu;
	SearchBar* searchBar;

	wxMenu viewMenu;

	EntNode* root;
	wxObjectDataPtr<EntityParser> Parser; // Need this or model leaks when tab destroyed
	wxDataViewCtrl* view;
	EntityEditor* editor;

	EntityTab(wxWindow* parent, const wxString name, const wxString& path = "");
	bool IsNewAndUntouched();
	bool UnsavedChanges();
	void refreshFilters();
	void onFilterCaseCheck(wxCommandEvent& event);
	void onFilterDelKeys(wxCommandEvent& event);
	void onFilterRefresh(wxCommandEvent& event);
	void onFilterClearAll(wxCommandEvent& event);
	void applyFilters(bool clearAll);
	void SearchForward();
	void SearchBackward();
	void saveFile();
	int CommitEdits();
	void UndoRedo(bool undo);
	void onDataviewChar(wxKeyEvent &event);
	bool dataviewMouseAction(wxDataViewItem item);
	void onNodeSelection(wxDataViewEvent& event);
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
	void onCopySelectedNodes(wxCommandEvent &event);
	void onPaste(wxCommandEvent& event);
	void onSelectAllEntities(wxCommandEvent &event);
	void onDeleteSelectedNodes(wxCommandEvent &event);
	void onSetSpawnPosition(wxCommandEvent &event);
	void onSetSpawnOrientation(wxCommandEvent &event);
	void onTeleportToEntity(wxCommandEvent &event);

	void onFilterMenuShowHide(wxCollapsiblePaneEvent& event);

	void action_PropMovers();

	private:
	wxDECLARE_EVENT_TABLE();
};