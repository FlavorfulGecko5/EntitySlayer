// wxWidgets
#include "wx/wx.h"
#include "wx/aboutdlg.h"
#include "wx/aui/auibook.h"
#include "wx/display.h"
#include "wx/timer.h"

// STL
#include <vector>
#include <string>
#include <cassert>

// Custom
#include <mhclient.h>

class EntityTab;
class EntityFrame : public wxFrame
{
	private:
	wxMenu* fileMenu = new wxMenu;
	wxMenu* editMenu = new wxMenu;
	wxAuiNotebook* book;
	EntityTab* activeTab = nullptr;
	wxTextCtrl* log = nullptr;
	wxStatusBar* statusbar = nullptr;

	// Meathook
	MeathookInterface meathook;
	wxMenu* mhMenu = new wxMenu;
	wxTimer mhStatusTimer;
	wxString mhText_Preface;
	wxString mhText_Active = "Connected";
	wxString mhText_Inactive = "Inactive";

	public:
	EntityFrame();
	~EntityFrame();
	void AddUntitledTab();
	void onWindowClose(wxCloseEvent& event);
	void onTabClosing(wxAuiNotebookEvent& event);
	void onTabChanged(wxAuiNotebookEvent& event);
	void onFileNew(wxCommandEvent& event);
	void onFileOpen(wxCommandEvent& event);
	void onMeathookOpen(wxCommandEvent& event);
	void onFileSave(wxCommandEvent& event);
	void onFileSaveAs(wxCommandEvent& event);
	void onCompressCheck(wxCommandEvent& event);
	void onUndoRedo(wxCommandEvent& event);
	void onNumberListCheck(wxCommandEvent& event);
	void onAbout(wxCommandEvent& event);
	void onManual(wxCommandEvent& event);
	void onMHStatusCheck(wxTimerEvent& event);
	void onSetMHTab(wxCommandEvent& event);
	void onReloadMH(wxCommandEvent& event);
	void RefreshMHMenu();

	private:
	wxDECLARE_EVENT_TABLE();
};