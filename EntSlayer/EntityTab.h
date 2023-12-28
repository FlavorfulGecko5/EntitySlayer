#include "wx/wx.h"
#include "wx/collpane.h"
#include "wx/splitter.h"
#include "EntityModel.h"

class FilterCtrl;
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
	wxCheckBox* spawnCheck;
	wxTextCtrl* xInput;
	wxTextCtrl* yInput;
	wxTextCtrl* zInput;
	wxTextCtrl* rInput;

	FilterCtrl* keyMenu;
	wxCheckBox* caseSensCheck;

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
	void onApplyFilters(wxCommandEvent& event);
	void applyFilters();
	void saveFile();
	int CommitEdits();
	void UndoRedo(bool undo);
	void onNodeSelection(wxDataViewEvent& event);
	void onNodeActivation(wxDataViewEvent& event);
	void onNodeRightClick(wxDataViewEvent& event);
	void onNodeContextAction(wxCommandEvent& event);
	void onFilterMenuShowHide(wxCollapsiblePaneEvent& event);

	private:
	wxDECLARE_EVENT_TABLE();
};