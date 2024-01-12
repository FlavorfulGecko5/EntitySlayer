#include "wx/wx.h"
#include "wx/collpane.h"
#include "wx/splitter.h"
#include "EntityModel.h"

class EntityEditor;
class FilterCtrl;
class SpawnFilter;
class EntityTab : public wxPanel
{
	public:
	wxString tabName;
	wxString filePath;
	bool fileUpToDate = true;
	bool compressOnSave;
	bool autoNumberLists = true;
	bool usingMH = false;

	FilterCtrl* layerMenu;
	FilterCtrl* classMenu;
	FilterCtrl* inheritMenu;
	FilterCtrl* keyMenu;
	wxCheckBox* caseSensCheck;
	SpawnFilter* spawnMenu;

	wxMenu viewMenu;

	EntityParser* parser;
	EntNode* root;
	wxObjectDataPtr<EntityModel> model; // Need this or model leaks when tab destroyed
	wxDataViewCtrl* view;
	EntityEditor* editor;

	EntityTab(wxWindow* parent, const wxString name, const wxString& path = "");
	virtual ~EntityTab();
	bool IsNewAndUntouched();
	bool UnsavedChanges();
	void refreshFilters();
	void onFilterCaseCheck(wxCommandEvent& event);
	void onFilterDelKeys(wxCommandEvent& event);
	void onFilterRefresh(wxCommandEvent& event);
	void onFilterClearAll(wxCommandEvent& event);
	bool applyFilters(bool clearAll);
	void saveFile();
	int CommitEdits();
	void UndoRedo(bool undo);
	bool dataviewMouseAction(wxDataViewItem item);
	void onNodeSelection(wxDataViewEvent& event);
	void onViewRightMouseDown(wxMouseEvent& event);


	/* TODO: NOTABLE FLAW - WHAT IF ROOT IS SELECTED OR THE ACTIVE NODE IN THE DATAVIEW?
	* WHAT WILL THESE FUNCTIONS DO WITH ROOT? MUST CORRECT
	*	- Fixed for copy/copy all - must keep consistent in future functions
	*/
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
	void onCopyNode(wxCommandEvent& event);
	void onCopySelectedNodes(wxCommandEvent &event);
	void onPaste(wxCommandEvent& event);
	void onSelectAllEntities(wxCommandEvent &event);
	void onDeleteSelectedNodes(wxCommandEvent &event);

	void onFilterMenuShowHide(wxCollapsiblePaneEvent& event);

	void action_PropMovers();

	private:
	wxDECLARE_EVENT_TABLE();
};