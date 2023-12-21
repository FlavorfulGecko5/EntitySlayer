// wxWidgets
#include "wx/wx.h"
#include "wx/aboutdlg.h"
#include "wx/aui/auibook.h"
#include "wx/display.h"

// STL
#include <vector>
#include <string>
#include <cassert>

// Custom
#include "EntityBookTab.h"

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
	HELP_MANUAL
};

class EntityFrame : public wxFrame
{
	private:
	wxMenu* fileMenu = new wxMenu;
	wxAuiNotebook* book;
	EntityBookTab* activeTab = nullptr;
	wxTextCtrl* log = nullptr;

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

			wxMenu* helpMenu = new wxMenu;
			helpMenu->Append(HELP_ABOUT, "About");
			helpMenu->Append(HELP_MANUAL, "&Manual\tCtrl+M");

			wxMenuBar* bar = new wxMenuBar;
			bar->Append(fileMenu, "File");
			bar->Append(editMenu, "Edit");
			bar->Append(helpMenu, "Help");
			SetMenuBar(bar);
		}

		/* Initialize Log */
		{
			log = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
				wxSize(-1, 100), wxTE_MULTILINE);
			delete wxLog::SetActiveTarget(new wxLogTextCtrl(log));
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
		EntityBookTab* newTab = new EntityBookTab(book);
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
		SetTitle(activeTab->filePath + " - EntitySlayer");
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
			EntityBookTab* newTab = new EntityBookTab(book, filepath);

			// Replace an unused new page
			if (activeTab->IsNewAndUntouched())
			{
				int index = book->GetPageIndex(activeTab);
				book->InsertPage(index, newTab, openFileDialog.GetFilename(), true);
				book->RemovePage(index + 1);
			}
			else book->AddPage(newTab, openFileDialog.GetFilename(), true);
		}
		catch (runtime_error e) {
			wxString msg = wxString::Format("File Opening Cancelled\n\n%s", e.what());
			wxMessageBox(msg, "Error", wxICON_ERROR | wxOK | wxCENTER, this);
		}
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
		activeTab->filePath = path;
		activeTab->fileUpToDate = false;
		activeTab->saveFile();

		// Reconfigure Book's perspective of tab
		int tabIndex = book->GetSelection();
		book->SetPageText(tabIndex, saveFileDialog.GetFilename());
		SetTitle(activeTab->filePath + " - EntitySlayer");
		fileMenu->Enable(FILE_SAVE, true);
	}

	void onCompressCheck(wxCommandEvent& event)
	{
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
		info.SetVersion("Alpha 2.0 [Undo/Redo Demo]");

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
wxEND_EVENT_TABLE()