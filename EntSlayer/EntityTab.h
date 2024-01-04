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
	void onNodeSelection(wxDataViewEvent& event);
	void onNodeActivation(wxDataViewEvent& event);
	void onNodeRightClick(wxDataViewEvent& event);
	void onNodeContextAction(wxCommandEvent& event);
	void onFilterMenuShowHide(wxCollapsiblePaneEvent& event);

	void action_PropMovers();

	private:
	wxDECLARE_EVENT_TABLE();
};