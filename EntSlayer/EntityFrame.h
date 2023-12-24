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
#include "EntityBookTab.h"
#include <mhclient.h>

enum FrameID 
{
	FRAME_MINIMUM = TAB_MINIMUM + 1,
	FILE_NEW,
	FILE_OPEN,
	FILE_SAVE,
	FILE_SAVEAS,
	FILE_COMPRESS,

	EDIT_UNDO,
	EDIT_REDO,

	HELP_ABOUT,
	HELP_MANUAL,

	MEATHOOK_CHECKSTATUS,

	MEATHOOK_MAKEACTIVETAB,
	MEATHOOK_OPENFILE,
	MEATHOOK_RELOAD,
};

class EntityFrame : public wxFrame
{
	private:
	wxMenu* fileMenu = new wxMenu;
	wxAuiNotebook* book;
	EntityBookTab* activeTab = nullptr;
	wxTextCtrl* log = nullptr;
	wxStatusBar* statusbar = nullptr;

	// Meathook
	MeathookInterface meathook;
	wxMenu* mhMenu = new wxMenu;
	wxTimer mhStatusTimer;
	wxString mhText_Preface;
	wxString mhText_Active = "Connected";
	wxString mhText_Inactive = "Inactive";
	EntityBookTab* mhTab = nullptr;
	

	public:
	EntityFrame() : wxFrame(nullptr, wxID_ANY, "EntitySlayer")
	{
		if(!Oodle::init()) 
			wxMessageBox("You can open uncompressed entities files but must decompress them separately.\nPut oo2core_8_win64.dll in the same folder as EntityHero.exe",
				"Warning: oo2core_8_win64.dll is missing or corrupted.",
				wxICON_WARNING | wxOK);

		/* Build Menu Bar */
		{
			// Note: These shortcuts override component-level shortcuts (such as in the editor)
			fileMenu->Append(FILE_NEW, "&New\tCtrl+N");
			fileMenu->Append(FILE_OPEN, "&Open\tCtrl+O");
			fileMenu->Append(FILE_SAVE, "&Save\tCtrl+S");
			fileMenu->Append(FILE_SAVEAS, "Save As\tCtrl+Shift+S");
			fileMenu->AppendSeparator();
			fileMenu->AppendCheckItem(FILE_COMPRESS, "Compress on Save");

			wxMenu* editMenu = new wxMenu;
			editMenu->Append(EDIT_UNDO, "Undo\tCtrl+Z");
			editMenu->Append(EDIT_REDO, "Redo\tCtrl+Y");

			mhMenu->AppendCheckItem(MEATHOOK_MAKEACTIVETAB, "Make Active Tab");
			mhMenu->Append(MEATHOOK_OPENFILE, "Open Current Map");
			mhMenu->Append(MEATHOOK_RELOAD, "Save and Reload Map\tCtrl+R");

			wxMenu* helpMenu = new wxMenu;
			helpMenu->Append(HELP_ABOUT, "About");
			helpMenu->Append(HELP_MANUAL, "&Manual\tCtrl+M");

			wxMenuBar* bar = new wxMenuBar;
			bar->Append(fileMenu, "File");
			bar->Append(editMenu, "Edit");
			bar->Append(mhMenu, "Meathook");
			bar->Append(helpMenu, "Help");
			SetMenuBar(bar);
		}

		/* Initialize Log */
		{
			log = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
				wxSize(-1, 100), wxTE_MULTILINE);
			delete wxLog::SetActiveTarget(new wxLogTextCtrl(log));
		}

		/* Status Bar for Meathook */
		/* (Need this before we init the notebook to prevent a status bar index crash) */
		{
			/*Per request from the creator of meathook, randomize the displayed meathook name.*/
			{
				srand(timeGetTime());
				struct PickRand_ { static char PickRand(const char* str) { size_t len = strlen(str); return str[rand() % (len - 1)]; } };
				mhText_Preface = wxString::Format("%c%c%c%c%c%c%c%c Interface: ",
					PickRand_::PickRand("Mm"),
					PickRand_::PickRand("eE3é"),
					PickRand_::PickRand("aA4"),
					PickRand_::PickRand("tT7"),
					PickRand_::PickRand("hH"),
					PickRand_::PickRand("oO0"),
					PickRand_::PickRand("oO0"),
					PickRand_::PickRand("kK"));
			}
			// TODO: field 0 will be overriden with help text upon mousing over a menu option
			// Consider doing something about this?
			statusbar = CreateStatusBar();
			statusbar->SetFieldsCount(1);

			// Start Meathook Status Checking
			RefreshMHMenu(); // Initial check so we don't need to wait for the first timer
			mhStatusTimer.SetOwner(this, MEATHOOK_CHECKSTATUS);
			mhStatusTimer.Start(5000);
		}

		/* Initialize Notebook */
		{
			book = new wxAuiNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
				//wxAUI_NB_TAB_SPLIT | // Crashes when you hold left click, then move mouse from lower view to higher view tab list.
				//wxAUI_NB_TAB_MOVE  | // Caused by tab moving, becomes more frequent with tab splitting
									   // Report this to wxWidgets if present in latest version?
				wxAUI_NB_SCROLL_BUTTONS | wxAUI_NB_CLOSE_ON_ALL_TABS | wxAUI_NB_TOP);
			AddUntitledTab();
		}
		
		/* Configure Main Sizer */
		{
			wxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
			mainSizer->Add(book, 1, wxEXPAND);
			mainSizer->Add(log, 0, wxEXPAND);
			SetSizerAndFit(mainSizer);
		}

		/* Set Dimensions and Location */
		{
			wxDisplay screen((unsigned)0);
			wxRect screenSize = screen.GetClientArea();
			SetSize(screenSize.width * 3 / 5, screenSize.height * 3 / 5);
			CenterOnScreen();
		}
	}

	~EntityFrame()
	{
		delete wxLog::SetActiveTarget(NULL);
	}

	void AddUntitledTab()
	{
		EntityBookTab* newTab = new EntityBookTab(book, "Untitled");
		book->AddPage(newTab, "Untitled", true);
	}

	void onWindowClose(wxCloseEvent& event)
	{
		if(!event.CanVeto()) 
		{
			event.Skip();
			return;
		}

		int unsavedTabs = 0;
		for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
		{
			EntityBookTab* page = (EntityBookTab*)book->GetPage(i);
			if(page->UnsavedChanges()) 
				unsavedTabs++;
		}
		if (unsavedTabs > 0 && wxMessageBox(
			wxString::Format("%i tab(s) have unsaved edits. Exit without saving to file(s)?", unsavedTabs),
			"Confirm Exit", wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
		{
			event.Veto();
			return;
		}

		event.Skip();
	}

	void onTabClosing(wxAuiNotebookEvent& event)
	{
		int index = event.GetSelection();
		EntityBookTab* page = (EntityBookTab*)book->GetPage(index);

		if (page->UnsavedChanges() &&
			wxMessageBox("You have unsaved edits. Continue without saving to file?",
				"Confirm Tab Closure", wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
		{
			event.Veto();
			return;
		}

		if (page == mhTab) {
			 // Need these lines for newAndUntouched edge case
			book->SetPageText(index, mhTab->tabName);
			mhTab = nullptr;
			RefreshMHMenu(); 
		}

		if (book->GetPageCount() == 1)
		{
			if (activeTab->IsNewAndUntouched())
				event.Veto();
			else AddUntitledTab();
		}
	}

	void onTabChanged(wxAuiNotebookEvent& event)
	{
		int i = event.GetSelection();
		wxLogMessage("Switching to page %i out of %zu", i + 1, book->GetPageCount());
		activeTab = (EntityBookTab*)book->GetPage(i);
		if(activeTab->filePath == "") SetTitle("EntitySlayer");
		else SetTitle(activeTab->filePath + " - EntitySlayer");
		switch (activeTab->type)
		{
			case TabType::NEW_FILE:
			fileMenu->Enable(FILE_SAVE, false);
			fileMenu->Enable(FILE_SAVEAS, true);
			break;

			case TabType::OPENED_FILE:
			fileMenu->Enable(FILE_SAVE, true);
			fileMenu->Enable(FILE_SAVEAS, true);
			break;
		}
		fileMenu->Check(FILE_COMPRESS, activeTab->compressOnSave);
		RefreshMHMenu();
	}

	void onFileNew(wxCommandEvent& event)
	{
		AddUntitledTab();
	}

	void onFileOpen(wxCommandEvent& event)
	{
		wxFileDialog openFileDialog(this, "Open File", wxEmptyString, wxEmptyString,
			"All Files (*.entities;*.txt)|*.entities;*.txt|Doom Files (*.entities)|*.entities",
			wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (openFileDialog.ShowModal() == wxID_CANCEL)
			return;

		// Don't open the same file twice
		wxString filepath = openFileDialog.GetPath();
		for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
		{
			EntityBookTab* page = (EntityBookTab*)book->GetPage(i);
			if (page->filePath == filepath)
			{
				book->SetSelection(i);
				return;
			}
		}

		try {
			EntityBookTab* newTab = new EntityBookTab(book, openFileDialog.GetFilename(), filepath);

			// Replace an unused new page
			if (activeTab->IsNewAndUntouched())
			{
				int index = book->GetPageIndex(activeTab);
				book->InsertPage(index, newTab, newTab->tabName, true);
				book->RemovePage(index + 1);
			}
			else book->AddPage(newTab, newTab->tabName, true);
		}
		catch (runtime_error e) {
			wxString msg = wxString::Format("File Opening Cancelled\n\n%s", e.what());
			wxMessageBox(msg, "Error", wxICON_ERROR | wxOK | wxCENTER, this);
		}
	}

	void onMeathookOpen(wxCommandEvent& event)
	{
		char Path[MAX_PATH];
		size_t PathSize = sizeof(Path);
		bool success = meathook.GetEntitiesFile((unsigned char*)Path, &PathSize);

		// This will be a newly created temp. file dumped from meathook,
		// so we can skip some safety checks we need for ordinary files
		string filePath(Path, PathSize);

		size_t delimiter = filePath.find_last_of('\\') + 1;
		if(delimiter == string::npos)
			delimiter = filePath.find_last_of('/') + 1;

		string tabName = filePath.substr(delimiter, filePath.length() - delimiter);

		EntityBookTab* newTab = new EntityBookTab(book, "[TEMPORARY FILE] " + tabName, filePath);
		if (activeTab->IsNewAndUntouched())
		{
			int index = book->GetPageIndex(activeTab);
			book->InsertPage(index, newTab, newTab->tabName, true);
			book->RemovePage(index + 1);
		}
		else book->AddPage(newTab, newTab->tabName, true);
	}

	void onFileSave(wxCommandEvent& event)
	{
		activeTab->saveFile();
	}

	void onFileSaveAs(wxCommandEvent& event)
	{
		wxFileDialog saveFileDialog(this, "Save File", wxEmptyString, activeTab->filePath,
			"Entities File (*.entities)|*.entities|Text File (*.txt)|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (saveFileDialog.ShowModal() == wxID_CANCEL)
			return;

		wxString path = saveFileDialog.GetPath();

		// Don't allow file to be saved when another tab has it opened
		for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
		{
			EntityBookTab* page = (EntityBookTab*)book->GetPage(i);
			if (page->filePath == path)
			{
				wxMessageBox("Cannot save to a file that's opened in another tab", 
					"Save As Failed", wxICON_ERROR | wxOK | wxCENTER, this);
				return;
			}
		}

		// Reconfigure tab
		activeTab->type = TabType::OPENED_FILE;
		activeTab->tabName = saveFileDialog.GetFilename();
		activeTab->filePath = path;
		activeTab->fileUpToDate = false;
		activeTab->saveFile();

		// Reconfigure Book's perspective of tab
		int tabIndex = book->GetSelection();
		if (activeTab == mhTab) book->SetPageText(tabIndex, "[Meathook] " + activeTab->tabName);
		else book->SetPageText(tabIndex, activeTab->tabName);
		SetTitle(activeTab->filePath + " - EntitySlayer");
		fileMenu->Enable(FILE_SAVE, true);
	}

	void onCompressCheck(wxCommandEvent& event)
	{
		// TODO: NEED TO FORCE DECOMPRESSION WHEN USING MEATHOOK RELOAD
		activeTab->compressOnSave = event.IsChecked();
		activeTab->fileUpToDate = false; // Allows saving unedited file when compression toggled
	}

	void onUndoRedo(wxCommandEvent& event)
	{
		bool undo = event.GetId() == EDIT_UNDO;
		wxWindow* focused = FindFocus();
		wxTextCtrl* textWindow = dynamic_cast<wxTextCtrl*>(focused);

		if (textWindow != nullptr)
		{
			if(undo) textWindow->Undo();
			else textWindow->Redo();
		}
		else activeTab->UndoRedo(undo);
	}

	void onAbout(wxCommandEvent& event)
	{
		wxAboutDialogInfo info;
		info.SetName("EntitySlayer");
		info.SetVersion("Alpha 4.0 [Meathook Open/Reloading Demo (but it works this time)]");

		wxString description = 
"DOOM Eternal .entities file editor inspired by EntityHero and Elena.\n\n"
"Developed by FlavorfulGecko5\n\n"
"Credits:\n"
"Scorp0rX0r - Author of EntityHero, the chief source of inspiration for this project.\n"
"Alveraan - Author of Elena, which inspired this program's filtering systems.\n"
"Chrispy - Developer of Meathook\n"
"Wyo - Author of wxWidgets/samples/stc/ - the basis for the text editor.";

		info.SetDescription(description);
		wxAboutBox(info, this);
	}

	void onManual(wxCommandEvent& event)
	{
	}

	void onMHStatusCheck(wxTimerEvent& event)
	{
		RefreshMHMenu();
	}

	void onSetMHTab(wxCommandEvent& event)
	{
		int activeIndex = book->GetPageIndex(activeTab);
		if (event.IsChecked()) {
			book->SetPageText(activeIndex, "[Meathook] " + activeTab->tabName);

			int oldIndex = book->GetPageIndex(mhTab);
			book->SetPageText(oldIndex, mhTab->tabName);
			mhTab = activeTab;

			/*
			* Due to inadequacies in the RPC server code, we must call GetEntitiesFile
			* before we are able to use PushEntitiesFile. This causes the current .entities file
			* to get dumped to a temp file regardless of us not actually using it. To be conscious of
			* not filling up hard drive space, we perform this operation here, and delete the resulting file.
			* 
			* The following usage guidelines should be observed:
			* 1. Set your meathook tab AFTER loading into the level you want to edit.
			* 2. If you quit to main menu or play a different level, then load back into the level you were editing,
			* you should reload with EntitySlayer to ensure the game uses your file.
			* 3. Make sure your meathook tab has compression-on-save turned off (or your game will crash, as
			*	it happens when meathook tries loading a compressed file as an override).
			*/
			{
				auto start = std::chrono::high_resolution_clock::now();
				char Path[MAX_PATH];
				size_t PathSize = sizeof(Path);
				bool success = meathook.GetEntitiesFile((unsigned char*)Path, &PathSize);
				if(success) std::remove(Path);

				auto stop = std::chrono::high_resolution_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
				wxLogMessage("Time to Set active level: %zu", duration.count());
			}

		}
		else {
			book->SetPageText(activeIndex, mhTab->tabName);
			mhTab = nullptr;
		}
		RefreshMHMenu();
	}

	void onReloadMH(wxCommandEvent& event)
	{
		wxLogMessage("Reload function called");
		mhTab->saveFile();

		char Directory[MAX_PATH];
		string pathString = string(mhTab->filePath);
		memcpy(&Directory[0], pathString.data(), pathString.length());
		Directory[pathString.length()] = '\0'; 

		meathook.PushEntitiesFile(Directory, nullptr, 0);
	}

	void RefreshMHMenu()
	{
		int DEBUG_SIMULATE_ONLINE = 0;
		bool online = meathook.m_Initialized || DEBUG_SIMULATE_ONLINE;
		statusbar->SetStatusText(mhText_Preface + (online ? mhText_Active : mhText_Inactive));

		mhMenu->Enable(MEATHOOK_MAKEACTIVETAB, online);
		mhMenu->Check(MEATHOOK_MAKEACTIVETAB, activeTab == mhTab);
		mhMenu->Enable(MEATHOOK_OPENFILE, online);
		mhMenu->Enable(MEATHOOK_RELOAD, online && activeTab == mhTab);
	}

	private:
	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(EntityFrame, wxFrame)
	EVT_CLOSE(EntityFrame::onWindowClose)

	EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, EntityFrame::onTabChanged)
	EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, EntityFrame::onTabClosing)

	EVT_MENU(FILE_NEW, EntityFrame::onFileNew)
	EVT_MENU(FILE_OPEN, EntityFrame::onFileOpen)
	EVT_MENU(FILE_SAVE, EntityFrame::onFileSave)
	EVT_MENU(FILE_SAVEAS, EntityFrame::onFileSaveAs)
	EVT_MENU(FILE_COMPRESS, EntityFrame::onCompressCheck)
	EVT_MENU(EDIT_UNDO, EntityFrame::onUndoRedo)
	EVT_MENU(EDIT_REDO, EntityFrame::onUndoRedo)
	EVT_MENU(HELP_ABOUT, EntityFrame::onAbout)
	EVT_MENU(HELP_MANUAL, EntityFrame::onManual)

	EVT_TIMER(MEATHOOK_CHECKSTATUS, EntityFrame::onMHStatusCheck)
	EVT_MENU(MEATHOOK_MAKEACTIVETAB, EntityFrame::onSetMHTab)
	EVT_MENU(MEATHOOK_OPENFILE, EntityFrame::onMeathookOpen) 
	EVT_MENU(MEATHOOK_RELOAD, EntityFrame::onReloadMH)
wxEND_EVENT_TABLE()