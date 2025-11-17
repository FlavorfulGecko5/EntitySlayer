// wxWidgets
#include "wx/wx.h"
#include "wx/aboutdlg.h"
#include "wx/aui/auibook.h"
#include "wx/display.h"
#include "wx/timer.h"

class EntityTab;
class EntityFrame : public wxFrame
{
	private:
	wxMenu* fileMenu = new wxMenu;
	wxMenu* tabMenu = new wxMenu;
	wxMenuItem* NightModeItem = nullptr;
	wxAuiNotebook* book;
	EntityTab* activeTab = nullptr;
	wxTextCtrl* log = nullptr;
	wxStatusBar* statusbar = nullptr;
	std::vector<std::string> activeEncounters;

	// Meathook
	EntityTab* mhTab = nullptr;
	wxMenu* mhMenu = new wxMenu;
	wxTimer mhStatusTimer;
	wxString mhText_Preface;

	public:
	EntityFrame();
	~EntityFrame();
	void AddUntitledTab();
	void AddOpenedTab(EntityTab* tab);
	void onWindowClose(wxCloseEvent& event);
	void onTabClosing(wxAuiNotebookEvent& event);
	void onFileCloseAll(wxCommandEvent& event);
	void onTabChanged(wxAuiNotebookEvent& event);
	void onFileNew(wxCommandEvent& event);
	void openFiles(const wxArrayString& filepaths);
	void onOpenConfig(wxCommandEvent& event);
	void onFileOpen(wxCommandEvent& event);
	void onFileOpenFolder(wxCommandEvent& event);
	void detectExternalEdits();
	void onFileReload(wxCommandEvent& event);
	void onWindowFocus(wxActivateEvent& event);
	void onMeathookOpen(wxCommandEvent& event);
	void onFileSave(wxCommandEvent& event);
	void onFileSaveAs(wxCommandEvent& event);
	void onReloadConfigFile(wxCommandEvent &event);
	void onCompressCheck(wxCommandEvent& event);
	void onNumberListCheck(wxCommandEvent& event);
	void onExportDiff(wxCommandEvent& event);
	void onImportDiff(wxCommandEvent& event);
	void onSearchForward(wxCommandEvent &event);
	void onSearchBackward(wxCommandEvent &event);
	void onAbout(wxCommandEvent& event);
	void onNightModeCheck(wxCommandEvent& event);
	void NightModeToggle(bool recursive);
	void onMHStatusCheck(wxTimerEvent& event);
	void onSetMHTab(wxCommandEvent& event);
	void onReloadMH(wxCommandEvent& event);
	void onPrintActiveEncounters(wxCommandEvent &event);
	void onActiveEncounterMenu(wxCommandEvent &event);
	void onGetSpawnPosition(wxCommandEvent &event);
	void onGetSpawnOrientation(wxCommandEvent &event);
	void onSpawnOffsetCheck(wxCommandEvent &event);

	void RefreshMHMenu();
	bool ClearMHTab();
	void onSpecial_PropMovers(wxCommandEvent &event);
	void onSpecial_TraversalInherits(wxCommandEvent &event);
	void onSpecial_DumpAllocatorInfo(wxCommandEvent &event);

	void onDebugMenuOne(wxCommandEvent &event);

	private:
	wxDECLARE_EVENT_TABLE();
};