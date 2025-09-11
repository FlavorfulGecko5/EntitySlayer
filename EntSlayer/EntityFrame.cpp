#pragma warning(disable : 4996) // Deprecation errors
#include <chrono>
#include "wx/clipbrd.h"
#include "wx/dir.h"
#include "wx/filename.h"
#include "Meathook.h"
#include "Oodle.h"
#include "Config.h"
#include "EntityFrame.h"
#include "EntityTab.h"
#include "EntityFolderDialog.h"
#include "EntityProfiler.h"

enum FrameID
{
	FRAME_MINIMUM = wxID_HIGHEST + 1,
	FILE_NEW,
	FILE_OPEN_FILE,
	FILE_OPEN_FOLDER,
	FILE_OPEN_CONFIG,
	FILE_SAVE,
	FILE_SAVEAS,
	FILE_CLOSE_ALL,
	FILE_CLOSE_ALL_OTHERS,
	FILE_RELOAD,
	FILE_RELOAD_CONFIGFILE,
	
	TAB_SEARCHFORWARD,
	TAB_SEARCHBACKWARD,
	TAB_COMPRESS,
	TAB_NUMBERLISTS,
	TAB_EXPORTDIFF,
	TAB_IMPORTDIFF,

	HELP_ABOUT,
	HELP_NIGHTMODE,

	MEATHOOK_CHECKSTATUS,
	MEATHOOK_MAKEACTIVETAB,
	MEATHOOK_RELOAD,
	MEATHOOK_OPENFILE,
	MEATHOOK_GET_SPAWNPOSITION,
	MEATHOOK_GET_SPAWNPOSITION_FILTER,
	MEATHOOK_GET_SPAWNORIENTATION,
	MEATHOOK_SPAWNPOS_OFFSET,
	MEATHOOK_GET_CHECKPOINT,
	MEATHOOK_GET_ENCOUNTER,
	
	SPECIAL_DEBUG_DUMPBUFFERS,
	SPECIAL_PROPMOVERS,
	SPECIAL_TRAVERSALINHERIT,

	DEBUG_MENUONE
};

wxBEGIN_EVENT_TABLE(EntityFrame, wxFrame)
	EVT_CLOSE(EntityFrame::onWindowClose)

	// For detecting external changes. Not ready yet
	//EVT_ACTIVATE(EntityFrame::onWindowFocus)

	EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, EntityFrame::onTabChanged)
	EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, EntityFrame::onTabClosing)

	EVT_MENU(FILE_NEW, EntityFrame::onFileNew)
	EVT_MENU(FILE_OPEN_FILE, EntityFrame::onFileOpen)
	EVT_MENU(FILE_OPEN_FOLDER, EntityFrame::onFileOpenFolder)
	EVT_MENU(FILE_OPEN_CONFIG, EntityFrame::onOpenConfig)
	EVT_MENU(FILE_SAVE, EntityFrame::onFileSave)
	EVT_MENU(FILE_SAVEAS, EntityFrame::onFileSaveAs)
	EVT_MENU(FILE_CLOSE_ALL, EntityFrame::onFileCloseAll)
	EVT_MENU(FILE_CLOSE_ALL_OTHERS, EntityFrame::onFileCloseAll)
	EVT_MENU(FILE_RELOAD, EntityFrame::onFileReload)
	EVT_MENU(FILE_RELOAD_CONFIGFILE, EntityFrame::onReloadConfigFile)
	EVT_MENU(TAB_SEARCHFORWARD, EntityFrame::onSearchForward)
	EVT_MENU(TAB_SEARCHBACKWARD, EntityFrame::onSearchBackward)
	EVT_MENU(TAB_COMPRESS, EntityFrame::onCompressCheck)
	EVT_MENU(TAB_NUMBERLISTS, EntityFrame::onNumberListCheck)
	EVT_MENU(TAB_EXPORTDIFF, EntityFrame::onExportDiff)
	EVT_MENU(TAB_IMPORTDIFF, EntityFrame::onImportDiff)
	EVT_MENU(HELP_ABOUT, EntityFrame::onAbout)
	EVT_MENU(HELP_NIGHTMODE, EntityFrame::onNightModeCheck)

	EVT_TIMER(MEATHOOK_CHECKSTATUS, EntityFrame::onMHStatusCheck)
	EVT_MENU(MEATHOOK_MAKEACTIVETAB, EntityFrame::onSetMHTab)
	EVT_MENU(MEATHOOK_RELOAD, EntityFrame::onReloadMH)
	EVT_MENU(MEATHOOK_OPENFILE, EntityFrame::onMeathookOpen) 
	EVT_MENU(MEATHOOK_GET_ENCOUNTER, EntityFrame::onPrintActiveEncounters)
	EVT_MENU(MEATHOOK_GET_SPAWNPOSITION, EntityFrame::onGetSpawnPosition)
	EVT_MENU(MEATHOOK_GET_SPAWNPOSITION_FILTER, EntityFrame::onGetSpawnPosition)
	EVT_MENU(MEATHOOK_GET_SPAWNORIENTATION, EntityFrame::onGetSpawnOrientation)
	EVT_MENU(MEATHOOK_SPAWNPOS_OFFSET, EntityFrame::onSpawnOffsetCheck)

	EVT_MENU(SPECIAL_DEBUG_DUMPBUFFERS, EntityFrame::onSpecial_DumpAllocatorInfo)
	EVT_MENU(SPECIAL_TRAVERSALINHERIT, EntityFrame::onSpecial_TraversalInherits)
	EVT_MENU(SPECIAL_PROPMOVERS, EntityFrame::onSpecial_PropMovers)

	EVT_MENU(DEBUG_MENUONE, EntityFrame::onDebugMenuOne)
wxEND_EVENT_TABLE()

EntityFrame::EntityFrame() : wxFrame(nullptr, wxID_ANY, "EntitySlayer")
{
	#ifdef _DEBUG
	BlockAllocatorUnitTest();
	#endif

	if (!Oodle::init())
		wxMessageBox("You can open uncompressed entities files but must decompress them separately.\nPut oo2core_8_win64.dll in the same folder as EntitySlayer.exe",
			"Warning: oo2core_8_win64.dll is missing or corrupted.",
			wxICON_WARNING | wxOK);

	ConfigInterface::loadData();

	/* Build Menu Bar */
	{
		// Note: These shortcuts override component-level shortcuts (such as in the editor)
		fileMenu->Append(FILE_NEW, "New\tCtrl+N");
		fileMenu->Append(FILE_OPEN_FILE, "Open Files\tCtrl+O");
		fileMenu->Append(FILE_OPEN_FOLDER, "Open Folder\tAlt+O");
		fileMenu->Append(FILE_SAVE, "Save\tCtrl+S");
		fileMenu->Append(FILE_SAVEAS, "Save As\tCtrl+Shift+S");
		fileMenu->Append(FILE_CLOSE_ALL, "Close All Tabs");
		fileMenu->Append(FILE_CLOSE_ALL_OTHERS, "Close All Other Tabs", 
			"Close all tabs except the one you're currently viewing");
		fileMenu->Append(FILE_RELOAD, "Reload From Disk");
		fileMenu->AppendSeparator();
		fileMenu->Append(FILE_OPEN_CONFIG, "Open Config File");
		fileMenu->Append(FILE_RELOAD_CONFIGFILE, "Reload Config File");
		
		tabMenu->Append(TAB_SEARCHFORWARD, "Search Forward\tCtrl+F");
		tabMenu->Append(TAB_SEARCHBACKWARD, "Search Backward\tCtrl+Space");
		tabMenu->AppendSeparator();
		tabMenu->AppendCheckItem(TAB_COMPRESS, "Compress on Save\tF1");
		tabMenu->AppendCheckItem(TAB_NUMBERLISTS, "Auto-Renumber idLists");
		tabMenu->Append(TAB_EXPORTDIFF, "Export Entity Diff");
		tabMenu->Append(TAB_IMPORTDIFF, "Import Entity Diff");

		mhMenu->AppendCheckItem(MEATHOOK_MAKEACTIVETAB, "Use as Reload Tab\tF4",
			"Enables level reloads using this tab. YOU MUST USE THIS AFTER LOADING INTO THE LEVEL YOU WANT TO EDIT!");
		mhMenu->Append(MEATHOOK_RELOAD, "Save and Reload Map\tF5",
			"Have Meathook reload the level using your Reload Tab");
		mhMenu->AppendSeparator();
		mhMenu->Append(MEATHOOK_OPENFILE, "Open Current Map\tCtrl+Shift+O",
			"Write's the current level's entities to a temporary file and opens it in a new tab");
		mhMenu->Append(MEATHOOK_GET_ENCOUNTER, "Find Active Encounters\tF3");
		mhMenu->AppendSeparator();
		mhMenu->Append(MEATHOOK_GET_SPAWNPOSITION, "Copy spawnPosition");
		mhMenu->Append(MEATHOOK_GET_SPAWNPOSITION_FILTER, "Copy spawnPosition and Set Filter\tF2",
			"Sets this tab's Spawn Position Distance filter using Meathook's spawnInfo");
		mhMenu->Append(MEATHOOK_GET_SPAWNORIENTATION, "Copy spawnOrientation");
		mhMenu->AppendCheckItem(MEATHOOK_SPAWNPOS_OFFSET, "Remove spawnPosition Z Offset",
			"Subtract the player's height from copied spawnPosition Z");
		//mhMenu->Append(MEATHOOK_GET_CHECKPOINT, "Goto Checkpoint");
		//mhMenu->Append(MEATHOOK_GET_ENCOUNTER, "Goto Current Encounter");

		wxMenu* specialMenu = new wxMenu;
		specialMenu->Append(SPECIAL_PROPMOVERS, "Bind idProp2 Entities to idMovers",
			"For Modded Multiplayer developers. Use this to fix idProp2 entity offsets");
		specialMenu->Append(SPECIAL_TRAVERSALINHERIT, "Fix Generated Traversals",
			"For Proteh's Traversal Generator. Adds 'inherit' statements missing from traversal entities");
		specialMenu->AppendSeparator();
		specialMenu->Append(SPECIAL_DEBUG_DUMPBUFFERS, "Write Allocator Data",
			"For debugging. Writes parser allocation data for the current tab to a file.");

		wxMenu* helpMenu = new wxMenu;
		helpMenu->Append(HELP_ABOUT, "About");
		NightModeItem = helpMenu->AppendCheckItem(HELP_NIGHTMODE, "Night Mode");

		//wxMenu* debugMenu = new wxMenu;
		//debugMenu->Append(DEBUG_MENUONE, "One");

		wxMenuBar* bar = new wxMenuBar;
		bar->Append(fileMenu, "File");
		bar->Append(tabMenu, "Tab");
		bar->Append(mhMenu, "Meathook");
		bar->Append(specialMenu, "Advanced");
		bar->Append(helpMenu, "Help");
		//bar->Append(debugMenu, "Debug");
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

		statusbar = CreateStatusBar();
		int widths[2] = {175, -1}; // Positive means size in pixels, negative means proportion of remaining space
		statusbar->SetFieldsCount(2, widths);
		SetStatusBarPane(1); // Changes what pane help text is written to

		// Start Meathook Status Checking
		// Initial check performed when first tab is added
		// so we don't have to wait 5 seconds on launch
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
		SetMinSize(wxSize(1000, 600)); // Minimum width for filter menus to not get offset or cut off
		CenterOnScreen(); // Todo: Is there a better way to enforce this without setting the entire application's minimum size?
	}

	NightModeItem->Check(ConfigInterface::NightMode());
	NightModeToggle(false);
}

EntityFrame::~EntityFrame()
{
	delete wxLog::SetActiveTarget(NULL);
}

void EntityFrame::AddUntitledTab()
{
	EntityTab* newTab = new EntityTab(book, "Untitled");
	book->AddPage(newTab, "Untitled", true);
}

void EntityFrame::AddOpenedTab(EntityTab* tab)
{
	// Replace an unused new page
	if (activeTab->IsNewAndUntouched())
	{
		int index = book->GetPageIndex(activeTab);
		book->InsertPage(index, tab, tab->tabName, true);
		book->DeletePage(index + 1); // RemovePage does not delete the tab object, use DeletePage to prevent memory leak
	}
	else book->AddPage(tab, tab->tabName, true);
}

void EntityFrame::onWindowClose(wxCloseEvent& event)
{
	if (!event.CanVeto())
	{
		event.Skip();
		return;
	}

	int unsavedTabs = 0;
	for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
	{
		EntityTab* page = (EntityTab*)book->GetPage(i);
		if (page->UnsavedChanges())
			unsavedTabs++;
	}
	if (unsavedTabs > 0 && wxMessageBox(
		wxString::Format("%i tab(s) have unsaved edits. Exit without saving to file(s)?", unsavedTabs),
		"Confirm Exit", wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
	{
		event.Veto();
		return;
	}
	ConfigInterface::deleteData(); // Debugger will detect large memory leaks if we don't do this
	event.Skip();
}

void EntityFrame::onTabClosing(wxAuiNotebookEvent& event)
{
	int index = event.GetSelection();
	EntityTab* page = (EntityTab*)book->GetPage(index);

	if (page->UnsavedChanges() &&
		wxMessageBox("You have unsaved edits. Continue without saving to file?",
			"Confirm Tab Closure", wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
	{
		event.Veto();
		return;
	}

	if(page == mhTab)
		mhTab = nullptr;

	if (book->GetPageCount() == 1)
	{
		if (activeTab->IsNewAndUntouched())
			event.Veto();
		else AddUntitledTab();
	}
}

void EntityFrame::onFileCloseAll(wxCommandEvent& event)
{
	bool excludeActive = event.GetId() == FILE_CLOSE_ALL_OTHERS;
	size_t removedPages = 0;
	for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
	{
		size_t index = i - removedPages;
		EntityTab* page = (EntityTab*)book->GetPage(index);
		if(excludeActive && page == activeTab)
			continue;

		if(page->UnsavedChanges() && wxMessageBox("Tab has unsaved changes. Close it anyways?", 
			page->tabName, wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
			continue;

		if(page == mhTab)
			mhTab = nullptr;

		//if(book->GetPageCount() == 1 && !activeTab->IsNewAndUntouched())
		if(book->GetPageCount() == 1)
			AddUntitledTab();

		book->DeletePage(index);
		removedPages++;
	}
}

void EntityFrame::onTabChanged(wxAuiNotebookEvent& event)
{
	int i = event.GetSelection();
	//wxLogMessage("Switching to page %i out of %zu", i + 1, book->GetPageCount());
	activeTab = (EntityTab*)book->GetPage(i);
	if (activeTab->filePath == "") {
		SetTitle("EntitySlayer");
		fileMenu->Enable(FILE_SAVE, false);
		fileMenu->Enable(FILE_RELOAD, false);
	}
	else {
		SetTitle(activeTab->filePath + " - EntitySlayer");
		fileMenu->Enable(FILE_SAVE, true);
		fileMenu->Enable(FILE_RELOAD, true);
	}
	tabMenu->Check(TAB_COMPRESS, activeTab->compressOnSave);
	tabMenu->Check(TAB_NUMBERLISTS, activeTab->autoNumberLists);
	RefreshMHMenu();
}

void EntityFrame::onFileNew(wxCommandEvent& event)
{
	AddUntitledTab();
}

void EntityFrame::openFiles(const wxArrayString& filepaths)
{
	for (const wxString& path : filepaths)
	{
		wxString name = wxFileNameFromPath(path);

		// Don't open the same file twice
		for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
		{
			EntityTab* page = (EntityTab*)book->GetPage(i);
			if (page->filePath == path)
			{
				book->SetSelection(i);
				goto LABEL_CONTINUE_OUTER;
			}
		}

		try {
			// If we want to multithread this function (which we should)
			// Then we'll need to create the EntityParsers here and pass them
			// to tabs as a constructor parm - the GUI classes don't appear to be
			// thread safe (which is pretty stupid), so we have to separate
			// the tab creation from the parser creation
			// Questionable whether this would even be worthwhile
			// Tab creation takes ~150 MS excluding the Parser anyways, meaning
			// Parser operations only take ~40% of the total tab build time for average entity files
			EntityTab* newTab = new EntityTab(book, name, path);
			AddOpenedTab(newTab);
		}
		catch (std::runtime_error e) {
			wxString msg = wxString::Format("File Opening Cancelled\n\n%s", e.what());
			wxMessageBox(msg, name, wxICON_ERROR | wxOK | wxCENTER, this);
		}

		LABEL_CONTINUE_OUTER:;
	}
}

void EntityFrame::onOpenConfig(wxCommandEvent& event)
{
	wxFileName path;
	path.AssignCwd();
	path.SetFullName(ConfigInterface::ConfigPath());

	wxArrayString container;
	container.push_back(path.GetFullPath());
	openFiles(container);
}

void EntityFrame::onFileOpen(wxCommandEvent& event)
{
	wxFileDialog openFileDialog(this, "Open File", wxEmptyString, wxEmptyString,
		"All Files (*.*)|*.*",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
	if (openFileDialog.ShowModal() == wxID_CANCEL)
		return;

	wxArrayString allPaths;
	openFileDialog.GetPaths(allPaths);
	openFiles(allPaths);
}

void EntityFrame::onFileOpenFolder(wxCommandEvent& event)
{
	// Safeguards against users opening stupidly large directories
	class EarlyOutTraverser : public wxDirTraverser
	{
		public:
		// Should be a completely reasonable item limit
		// For reference, the injectable portion of Kaiser Campaign is ~8700 items
		const size_t ITEM_LIMIT = 25000;

		// Number of files and folders iterated over
		size_t itemCount = 0;

		wxArrayString& files;

		EarlyOutTraverser(wxArrayString& p_files) : files(p_files) {}

		wxDirTraverseResult OnFile(const wxString& filename)
		{
			if(++itemCount > ITEM_LIMIT)
				return wxDIR_STOP;

			//if(filename.EndsWith(".entities") || filename.EndsWith(".txt") || filename.EndsWith(".json"))
				files.push_back(filename);
			return wxDIR_CONTINUE;
		}

		wxDirTraverseResult OnDir(const wxString& dirname)
		{
			if(++itemCount > ITEM_LIMIT)
				return wxDIR_STOP;
			return wxDIR_CONTINUE;
		}

		bool EarliedOut() const
		{
			return itemCount > ITEM_LIMIT;
		}
	};

	wxDirDialog openDialog(this, "Open Folder", wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST); // Todo: Can select multiple
	if(openDialog.ShowModal() == wxID_CANCEL)
		return;

	wxString dirString(openDialog.GetPath());
	size_t dirLength = dirString.length();
	wxDir dir(dirString);
	wxArrayString files;
	EarlyOutTraverser traverser(files);

	// Cannot use filespec - filtered out files won't get passed to the traverser, meaning they won't get counted
	// and we can't properly early-out. We must do the extension check manually in the traverser functions
	// 
	// This function seems to log errors if reading certain streamed vertexpaint files in a resource archive's maps folder
	// These errors don't appear to cause crashes/memory leaks - so it should be safe to ignore them
	dir.Traverse(traverser, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_NO_FOLLOW);
	wxLogMessage("Navigated through %zu files/folders", traverser.itemCount);
	if (traverser.EarliedOut()) {
		wxLogMessage("Selected folder has too many items! To prevent freezing, opening this folder has been cancelled");
		return;
	}
	if (files.IsEmpty()) {
		wxLogMessage("Found no files to open");
		return;
	}

	for (wxString& f : files)
		f.Remove(0, dirLength + 1); // dirString does not come with a separator at the end

	EntityFolderDialog selector(dirString, files);
	if(selector.GetReturnCode() == wxID_CANCEL)
		return;

	wxArrayString selections;
	selector.GetCheckedPaths(selections);
	if(selections.IsEmpty())
		return;

	for (wxString& s : selections) {
		s.Prepend(wxFileName::GetPathSeparator());
		s.Prepend(dirString);
	}
		
	openFiles(selections);
}

void EntityFrame::detectExternalEdits()
{
	//std::error_code code;
	//std::filesystem::file_time_type modTime = std::filesystem::last_write_time(std::string(activeTab->filePath), code);
	//wxLogMessage("%s", code.message());
	//if(modTime > activeTab->openTime)
	//	wxLogMessage("External edits detected");
	//if(modTime == std::filesystem::file_time_type::min())
	//	wxLogMessage("Minimum");
	//else
	//	wxLogMessage("No External Edits detected");

	//if(!code)
	//	wxLogMessage("No Error");
}

void EntityFrame::onWindowFocus(wxActivateEvent& event)
{
	if(event.GetActive())
		detectExternalEdits();
}

void EntityFrame::onFileReload(wxCommandEvent& event)
{
	if (activeTab->filePath == "")
		return;
	if (wxMessageBox(activeTab->filePath, "Reloading will discard unsaved changes. Proceed?",
		wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
		return;
	activeTab->reloadFile();
}

void EntityFrame::onMeathookOpen(wxCommandEvent& event)
{
	// This will be a newly created temp. file dumped from meathook,
	// so we can skip some safety checks we need for ordinary files
	std::string filePath;
	if (!Meathook::GetCurrentMap(filePath, false)) {
		wxMessageBox("Meathook open failed. Is Meathook offline?", "Meathook Interface", wxICON_WARNING | wxOK);
		return;
	}

	size_t delimiter = filePath.find_last_of('\\') + 1;
	if (delimiter == std::string::npos)
		delimiter = filePath.find_last_of('/') + 1;

	std::string tabName = filePath.substr(delimiter, filePath.length() - delimiter);

	EntityTab* newTab = new EntityTab(book, "[TEMPORARY FILE] " + tabName, filePath);
	AddOpenedTab(newTab);
}

void EntityFrame::onFileSave(wxCommandEvent& event)
{
	// Technically this is error-prone if users deliberately do things wrong
	// (opening another file of the same name, or setting this as their Meathook tab)
	// but it should be fine
	if(activeTab->saveFile() && activeTab->tabName == ConfigInterface::ConfigPath())
		onReloadConfigFile(event);
}

void EntityFrame::onFileSaveAs(wxCommandEvent& event)
{
	wxFileDialog saveFileDialog(this, "Save File", wxEmptyString, activeTab->filePath,
		wxFileSelectorDefaultWildcardStr, 
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (saveFileDialog.ShowModal() == wxID_CANCEL)
		return;

	wxString path = saveFileDialog.GetPath();

	// Don't allow file to be saved when another tab has it opened
	for (size_t i = 0, max = book->GetPageCount(); i < max; i++)
	{
		EntityTab* page = (EntityTab*)book->GetPage(i);
		if (page->filePath == path)
		{
			wxMessageBox("Cannot save to a file that's opened in another tab",
				"Save As Failed", wxICON_ERROR | wxOK | wxCENTER, this);
			return;
		}
	}

	// Reconfigure tab
	activeTab->tabName = saveFileDialog.GetFilename();
	activeTab->filePath = path;
	activeTab->Parser->MarkFileOutdated();
	activeTab->saveFile();

	// Reconfigure Book's perspective of tab
	int tabIndex = book->GetSelection();
	if (activeTab == mhTab) book->SetPageText(tabIndex, "[Meathook] " + activeTab->tabName);
	else book->SetPageText(tabIndex, activeTab->tabName);
	SetTitle(activeTab->filePath + " - EntitySlayer");
	fileMenu->Enable(FILE_SAVE, true);
	fileMenu->Enable(FILE_RELOAD, true);
}

void EntityFrame::onReloadConfigFile(wxCommandEvent& event)
{
	if(ConfigInterface::loadData()) {
		TIMESTART(ReloadTime)
		for (size_t i = 0, max = book->GetPageCount(); i < max; i++) {
			EntityTab* t = (EntityTab*)book->GetPage(i);
			t->setAppendMenu();
		}
		NightModeToggle(true);
		TIMESTOP(ReloadTime, "Config File Reload Time: ")
	}
}

void EntityFrame::onCompressCheck(wxCommandEvent& event)
{
	activeTab->compressOnSave = event.IsChecked();
	activeTab->Parser->MarkFileOutdated(); // Allows saving unedited file when compression toggled
}

void EntityFrame::onNumberListCheck(wxCommandEvent& event)
{
	activeTab->autoNumberLists = event.IsChecked();
}

void EntityFrame::onExportDiff(wxCommandEvent& event)
{
	activeTab->exportdiff();
}

void EntityFrame::onImportDiff(wxCommandEvent& event)
{
	activeTab->importdiff();
}

void EntityFrame::onSearchForward(wxCommandEvent& event)
{
	// A little boiler plate so we don't have to introduce the filter menu header into this file
	activeTab->SearchForward();
}

void EntityFrame::onSearchBackward(wxCommandEvent& event)
{
	activeTab->SearchBackward();
}

void EntityFrame::onAbout(wxCommandEvent& event)
{
	wxAboutDialogInfo info;
	info.SetName("EntitySlayer");
	info.SetVersion("Beta 9 Release Build");

	wxString description =
		"DOOM Eternal .entities file editor inspired by EntityHero and Elena.\n\n"
		"Developed by FlavorfulGecko5\n\n"
		"Credits:\n"
		"Velser - Extensive Alpha testing, feedback and feature suggestions\n"
		"Scorp0rX0r - Author of EntityHero, the chief source of inspiration for this project.\n"
		"Alveraan - Author of Elena, which inspired this program's filtering systems.\n"
		"Chrispy - Developer of Meathook\n"
		"Wyo - Author of wxWidgets/samples/stc/ - the basis for the text editor.";

	info.SetDescription(description);
	wxAboutBox(info, this);
}

void EntityFrame::onNightModeCheck(wxCommandEvent& event)
{
	NightModeToggle(true);
}

void EntityFrame::NightModeToggle(bool recursive) {
	// Text Box: Background: 255/255/255  - Foreground: 0/0/0
	// Status bar: Background: 240/240/240 - Foreground: 0/0/0 - Changing text color seems impossible
	// Frame: Background: 171/171/171 - Foreground: 0/0/0 - Not used anywhere, propagates to children when modified

	// Very important we do this here, and in this order. Keeps night mode from flip flopping upon config reload
	bool nightMode = NightModeItem->IsChecked();
	ConfigInterface::SetNightMode(nightMode);

	log->SetBackgroundColour(nightMode ? wxColour(32, 32, 32) : wxColour(255, 255, 255));
	log->SetForegroundColour(nightMode ? wxColour(216, 216, 216) : wxColour(0, 0, 0));
	log->Refresh();


	statusbar->SetBackgroundColour(nightMode ? wxColour(64, 64, 64) : wxColour(240, 240, 240));
	statusbar->Refresh();

	// FIX FOR NIGHTMARE GETTING RAN TWICE FOR THE INITIAL TAB ON BOOT UP: ONCE ON CONSTRUCTION,
	// AND ONCE WHEN ENTITYFRAME'S CONSTRUCTOR CALLS NIGHT MODE
	if(!recursive)
		return;

	for (int i = 0; i < book->GetPageCount(); i++) {
		EntityTab* t = (EntityTab*)book->GetPage(i);
		t->NightMode(true);
	}

}

void EntityFrame::onMHStatusCheck(wxTimerEvent& event)
{
	RefreshMHMenu();
}

// Returns true if a meathook tab was cleared, otherwise false
bool EntityFrame::ClearMHTab() 
{
	if(mhTab != nullptr) 
	{
		size_t index = book->GetPageIndex(mhTab);
		book->SetPageText(index, mhTab->tabName);
		mhTab->compressOnSave_ForceDisable = false;
		if(mhTab->compressOnSave)
			mhTab->Parser->MarkFileOutdated();
		mhTab = nullptr;
		return true;
	}
	return false;
}

void EntityFrame::onSetMHTab(wxCommandEvent& event)
{
	// Clear the old mhTab if there is one
	ClearMHTab();

	if (event.IsChecked()) {
		int activeIndex = book->GetPageIndex(activeTab);
		book->SetPageText(activeIndex, "[Meathook] " + activeTab->tabName);
		mhTab = activeTab;

		activeTab->compressOnSave_ForceDisable = true;
		if (activeTab->compressOnSave) {
			wxMessageBox("Meathook cannot load compressed files. Compression-on-save has been disabled for this file.",
				"Oodle Compression", wxICON_WARNING | wxOK);
			activeTab->Parser->MarkFileOutdated();
		}

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
		*/
		{
			auto start = std::chrono::high_resolution_clock::now();
			std::string dummy;
			Meathook::GetCurrentMap(dummy, true);

			auto stop = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
			wxLogMessage("Time to Set active level: %zu", duration.count());
		}

	}
	RefreshMHMenu();
}

void EntityFrame::onReloadMH(wxCommandEvent& event)
{
	wxLogMessage("Reload function called");
	mhTab->saveFile();

	if(!Meathook::ReloadMap(std::string(mhTab->filePath)))
		wxMessageBox("Map reload failed. Is Meathook offline?", "Meathook Interface", wxICON_WARNING | wxOK);
}

void EntityFrame::onPrintActiveEncounters(wxCommandEvent& event)
{
	activeEncounters.clear();
	std::string encounterString;
	if (!Meathook::GetActiveEncounters(encounterString)) {
		wxMessageBox("Couldn't get activeEncounters. Is Meathook offline?", "Meathook Interface", wxICON_WARNING | wxOK);
		return;
	}
		
	if (encounterString.length() == 0) {
		wxLogMessage("No Active Encounters");
		return;
	}
	wxLogMessage("Active Encounter Names: %s", encounterString);

	size_t index = encounterString.find(';');
	if (index == std::string::npos) { // Only one encounter, find it immediately
		wxLogMessage("Found one active encounter, searching for it now...");
		activeTab->Parser->FilteredSearch("entityDef" + encounterString, false, true, true);
		return;
	}

	// Multiple active encounters, we must prompt the user to choose one to jump to
	size_t lastIndex = 0;
	while (index != std::string::npos) {
		activeEncounters.emplace_back(encounterString.substr(lastIndex, index - lastIndex));
		lastIndex = index + 1;
		index = encounterString.find(';', lastIndex);
	}
	activeEncounters.emplace_back(encounterString.substr(lastIndex, encounterString.length() - lastIndex));

	wxMenu encounterMenu;
	for(int i = 0, max = activeEncounters.size(); i < max; i++)
		encounterMenu.Append(i, activeEncounters[i]);

	encounterMenu.Bind(wxEVT_COMMAND_MENU_SELECTED, &EntityFrame::onActiveEncounterMenu, this);
	PopupMenu(&encounterMenu);
}

void EntityFrame::onActiveEncounterMenu(wxCommandEvent& event)
{
	wxLogMessage("Searching this tab for encounter...");
	activeTab->Parser->FilteredSearch("entityDef" + activeEncounters[event.GetId()], false, true, true);
}

void EntityFrame::onGetSpawnPosition(wxCommandEvent &event) 
{
	if(!Meathook::CopySpawnPosition())
		wxMessageBox("Couldn't get spawnPosition. Is Meathook offline?", "Meathook Interface", wxICON_WARNING | wxOK);

	if(event.GetId() == MEATHOOK_GET_SPAWNPOSITION_FILTER)
		activeTab->filterSetSpawninfo();
}

void EntityFrame::onGetSpawnOrientation(wxCommandEvent& event)
{
	if(!Meathook::CopySpawnOrientation())
		wxMessageBox("Couldn't get spawnOrientation. Is Meathook offline?", "Meathook Interface", wxICON_WARNING | wxOK);
}

void EntityFrame::onSpawnOffsetCheck(wxCommandEvent& event)
{
	Meathook::RemoveZOffset(event.IsChecked());
}

void EntityFrame::RefreshMHMenu()
{
	int DEBUG_SIMULATE_ONLINE = 0;
	bool online = Meathook::IsOnline() || DEBUG_SIMULATE_ONLINE;
	statusbar->SetStatusText(mhText_Preface + (online ? mhText_Active : mhText_Inactive));

	if (!online && ClearMHTab())
		wxMessageBox("Connection lost with Meathook. Your Meathook tab has been automatically disabled", 
			"Meathook", wxICON_WARNING | wxOK);

	tabMenu->Enable(TAB_COMPRESS, !activeTab->compressOnSave_ForceDisable);

	mhMenu->Enable(MEATHOOK_MAKEACTIVETAB, online && activeTab->filePath != ""); // Tabs with no file shouldn't be useable with mh
	mhMenu->Check(MEATHOOK_MAKEACTIVETAB, activeTab == mhTab);
	mhMenu->Enable(MEATHOOK_RELOAD, online && activeTab == mhTab); // For simplicity, only enable this option when mhTab is the activeTab
	mhMenu->Enable(MEATHOOK_OPENFILE, online);
	mhMenu->Enable(MEATHOOK_GET_ENCOUNTER, online);
	mhMenu->Enable(MEATHOOK_GET_SPAWNPOSITION, online);
	mhMenu->Enable(MEATHOOK_GET_SPAWNPOSITION_FILTER, online);
	mhMenu->Enable(MEATHOOK_GET_SPAWNORIENTATION, online);
	mhMenu->Enable(MEATHOOK_SPAWNPOS_OFFSET, online);
	mhMenu->Check(MEATHOOK_SPAWNPOS_OFFSET, Meathook::RemoveZOffset());
}

void EntityFrame::onSpecial_PropMovers(wxCommandEvent &event)
{
	activeTab->action_PropMovers();
}

void EntityFrame::onSpecial_TraversalInherits(wxCommandEvent& event)
{
	activeTab->action_FixTraversals();
}

void EntityFrame::onSpecial_DumpAllocatorInfo(wxCommandEvent& event)
{
	wxFileDialog saveFileDialog(this, "Save File", wxEmptyString, "EntitySlayer_AllocData.txt",
		"Text Files (*.txt)|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

	if(saveFileDialog.ShowModal() == wxID_CANCEL)
		return;

	std::string path(saveFileDialog.GetPath());
	activeTab->Parser->logAllocatorInfo(true, false, true, path);
}

void EntityFrame::onDebugMenuOne(wxCommandEvent& event)
{
}
