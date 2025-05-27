#include "wx/clipbrd.h"
#include "EntityTab.h"
#include "EntityEditor.h"
#include "FilterMenus.h"
#include "Config.h"
#include "Meathook.h"

enum TabID 
{
	// wxPanel will not use an accelerator entry whose integer value is 1....WTF?
	TABID_MINIMUM = wxID_HIGHEST + 1,

	NODEVIEW_UNDO,
	NODEVIEW_REDO,
	NODEVIEW_CUTSELECTED,
	NODEVIEW_CUTSELECTED_ACCEL,
	NODEVIEW_COPYSELECTED,
	NODEVIEW_COPYSELECTED_ACCEL,
	NODEVIEW_PASTE,
	NODEVIEW_PASTE_ACCEL,
	NODEVIEW_EXPANDENT,
	NODEVIEW_SELECTALLENTS,
	NODEVIEW_SELECTALLENTS_ACCEL,
	NODEVIEW_DELETESELECTED,
	NODEVIEW_DELETESELECTED_ACCEL,
	NODEVIEW_SETPOSITION,
	NODEVIEW_SETPOSITION_ACCEL,
	NODEVIEW_SETORIENTATION,
	NODEVIEW_SETORIENTATION_ACCEL,
	NODEVIEW_TELEPORTPOSITION,
	NODEVIEW_TELEPORTPOSITION_ACCEL,

	NODEVIEW_APPENDMENU_ID, // AppendMenu needs a unique ID to be found by the Remove function

	TABID_MAXIMUM
};

wxBEGIN_EVENT_TABLE(EntityTab, wxPanel)
	EVT_COLLAPSIBLEPANE_CHANGED(wxID_ANY, onFilterMenuShowHide)

	EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, onNodeSelection)
	EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, onNodeDoubleClick)
	EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, onNodeContextMenu)

	EVT_MENU(NODEVIEW_UNDO, onUndo)
	EVT_MENU(NODEVIEW_REDO, onRedo)
	EVT_MENU(NODEVIEW_CUTSELECTED, onCutSelectedNodes)
	EVT_MENU(NODEVIEW_COPYSELECTED, onCopySelectedNodes)
	EVT_MENU(NODEVIEW_PASTE, onPaste)
	EVT_MENU(NODEVIEW_EXPANDENT, onExpandEntity)
	EVT_MENU(NODEVIEW_SELECTALLENTS, onSelectAllEntities)
	EVT_MENU(NODEVIEW_DELETESELECTED, EntityTab::onDeleteSelectedNodes)
	EVT_MENU(NODEVIEW_SETPOSITION, EntityTab::onSetSpawnPosition)
	EVT_MENU(NODEVIEW_SETORIENTATION, EntityTab::onSetSpawnOrientation)
	EVT_MENU(NODEVIEW_TELEPORTPOSITION, EntityTab::onTeleportToEntity)

	EVT_MENU(NODEVIEW_CUTSELECTED_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_COPYSELECTED_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_PASTE_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_SELECTALLENTS_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_DELETESELECTED_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_SETPOSITION_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_SETORIENTATION_ACCEL, onNodeContextAccelerator)
	EVT_MENU(NODEVIEW_TELEPORTPOSITION_ACCEL, onNodeContextAccelerator)

	// This technically causes a double-commit test if append is executed via the menus
	// and not through a shortcut. However, this shouldn't have unwanted side effects since
	// the second check won't find anything to commit. And if the first check finds errors or commits, the
	// menu won't show up the first place. So overall, it's just some very minor double-execution of code,
	// though would still be nice to rectify this somehow
	// Todo: Investigate cleanliness of executing all right click menu actions through nodeContextAccelerator
	EVT_MENU_RANGE(TABID_MAXIMUM, MAXSHORT, onNodeContextAccelerator)
wxEND_EVENT_TABLE()

EntityTab::EntityTab(wxWindow* parent, const wxString name, const wxString& path)
	: wxPanel(parent, wxID_ANY), tabName(name), filePath(path)
{
	/* Initialize parser */
	if (path == "")
		Parser = new EntityParser();
	else  { // Todo: Hide filters if permissive mode is enabled?
		ParsingMode mode = ParsingMode::PERMISSIVE;
		wxString lowercase = filePath.Lower();

		if(lowercase.EndsWith(".entities")) {
			mode = ParsingMode::ENTITIES;
		}
		else if (lowercase.EndsWith(".json")) {
			mode = ParsingMode::JSON;
		}
		else if (!lowercase.EndsWith(".txt")) {
			wxLogMessage("WARNING: Unsupported filetype detected. Permissive parsing mode enabled. Please verify data integrity when done editing.");
		}

		if (mode != ParsingMode::ENTITIES) {
			wxLogMessage("WARNING: Non-entities file detected. Automatic list renumbering has been disabled (re-enable it in the 'Tab' menu)");
			autoNumberLists = false;
		}

		Parser = new EntityParser(std::string(path), mode, true);
		//openTime = std::filesystem::last_write_time(std::string(path));
	}

	compressOnSave = Parser->wasFileCompressed();
	root = Parser->getRoot();

	/* Filter Menu (should be initialized before model is associated with view) */
	topWrapper = new wxGenericCollapsiblePane(this, wxID_ANY, "Filter Menu",
		wxDefaultPosition, wxDefaultSize, wxCP_NO_TLW_RESIZE);
	{
		wxWindow* topWindow = topWrapper->GetPane();

		/* Layer, Class and Inherit filter lists */
		wxBoxSizer* checklistSizer = new wxBoxSizer(wxHORIZONTAL);
		layerMenu = new FilterCtrl(this, topWindow, "Layers", false);
		classMenu = new FilterCtrl(this, topWindow, "Classes", false);
		inheritMenu = new FilterCtrl(this, topWindow, "Inherits", false);
		componentMenu = new FilterCtrl(this, topWindow, "Components", false);
		checklistSizer->Add(componentMenu->container, 1, wxLEFT | wxRIGHT, 10);
		checklistSizer->Add(layerMenu->container, 1, wxLEFT | wxRIGHT, 10);
		checklistSizer->Add(classMenu->container, 1, wxLEFT | wxRIGHT, 10);
		checklistSizer->Add(inheritMenu->container, 1, wxLEFT | wxRIGHT, 10);
		refreshFilters();
		if (componentMenu->list->GetCount() > 1) { // No Components option gives a minimum value of 1
			checklistSizer->Hide(layerMenu->container);
		}
		else {
			checklistSizer->Hide(componentMenu->container);
		}
			

		/* Spawn Position Filter */
		spawnMenu = new SpawnFilter(this, topWindow);

		/* Text Filter */
		wxBoxSizer* textFilterSizer = new wxBoxSizer(wxVERTICAL);
		{
			keyMenu = new FilterCtrl(this, topWindow, "Text Key", true);

			caseSensCheck = new wxCheckBox(topWindow, wxID_ANY, "Case Sensitive");
			caseSensCheck->SetValue(false);
			caseSensCheck->Bind(wxEVT_CHECKBOX, &EntityTab::onFilterCaseCheck, this);

			wxButton* delKeysBtn = new wxButton(topWindow, wxID_ANY, "Delete Unchecked Text Keys");
			delKeysBtn->Bind(wxEVT_BUTTON, &EntityTab::onFilterDelKeys, this);

			wxBoxSizer* bottomRow = new wxBoxSizer(wxHORIZONTAL);
			bottomRow->Add(caseSensCheck);
			bottomRow->Add(delKeysBtn);

			textFilterSizer->Add(keyMenu->container, 1, wxEXPAND);
			//textFilterSizer->Add(caseSensCheck);
			textFilterSizer->Add(bottomRow, 0, wxTOP, 2);
		}

		// TODO: Find a more polished layout for these buttons
		wxBoxSizer* otherButtons = new wxBoxSizer(wxVERTICAL);
		{
			wxButton* clearButton = new wxButton(topWindow, wxID_ANY, "Clear All Filters");
			clearButton->Bind(wxEVT_BUTTON, &EntityTab::onFilterClearAll, this);

			wxButton* refreshButton = new wxButton(topWindow, wxID_ANY, "Refresh Filter Lists");
			refreshButton->Bind(wxEVT_BUTTON, &EntityTab::onFilterRefresh, this);
			
			otherButtons->Add(clearButton, 0, wxALL, 10);
			otherButtons->Add(refreshButton, 0, wxALL, 10);
		}
		wxBoxSizer* compact = new wxBoxSizer(wxHORIZONTAL);
		compact->Add(spawnMenu);
		compact->Add(otherButtons, 0, wxLEFT, 5);

		/* Search Bar */
		searchBar = new SearchBar(this, topWindow);

		wxBoxSizer* secondRowSizer = new wxBoxSizer(wxHORIZONTAL);
		secondRowSizer->Add(textFilterSizer, 1, wxALL, 10);
		secondRowSizer->Add(compact, 1, wxALL, 10);
		secondRowSizer->Add(searchBar, 1, wxALL, 10);

		/* Put everything together */
		wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
		topSizer->Add(checklistSizer, 0, wxEXPAND);
		topSizer->Add(secondRowSizer, 0, wxEXPAND);
		topWindow->SetSizerAndFit(topSizer);
	}

	/* Initialize controls */
	wxSplitterWindow* splitter = new wxSplitterWindow(this);
	editor = new EntityEditor(splitter, wxID_ANY, wxDefaultPosition, wxSize(300, 300));
	view = new wxDataViewCtrl(splitter, wxID_ANY, wxDefaultPosition, wxSize(300, 300), wxDV_MULTIPLE);
	Parser->view = view;
	view->GetMainWindow()->Bind(wxEVT_CHAR, &EntityTab::onDataviewChar, this);
	view->GetMainWindow()->Bind(wxEVT_RIGHT_DOWN, &EntityTab::onViewRightMouseDown, this);
	/* View right click menu and accelerator table */
	{
		// These menu shortcuts don't do anything - they're given for documentation purposes
		viewMenu.Append(NODEVIEW_UNDO, "Undo\tCtrl+Z");
		viewMenu.Append(NODEVIEW_REDO, "Redo\tCtrl+Y");
		viewMenu.AppendSeparator();
		viewMenu.Append(NODEVIEW_CUTSELECTED, "Cut Selected\tCtrl+X");
		viewMenu.Append(NODEVIEW_COPYSELECTED, "Copy Selected\tCtrl+C");
		viewMenu.Append(NODEVIEW_PASTE, "Paste\tCtrl+V");
		viewMenu.AppendSeparator();
		viewMenu.Append(NODEVIEW_EXPANDENT, "Expand/Collapse Entity\tDouble Click or Enter");
		viewMenu.Append(NODEVIEW_SELECTALLENTS, "Select All Entities\tCtrl+A");
		viewMenu.Append(NODEVIEW_DELETESELECTED, "Delete Selected\tCtrl+D or Del");
		viewMenu.AppendSeparator();
		viewMenu.Append(NODEVIEW_SETPOSITION, "Copy and Set spawnPosition\tCtrl+E");
		viewMenu.Append(NODEVIEW_SETORIENTATION, "Copy and Set spawnOrientation\tCtrl+R");
		viewMenu.Append(NODEVIEW_TELEPORTPOSITION, "Teleport to spawnPosition\tCtrl+W");
		setAppendMenu();
	}
	/* Initialize View */
	{
		// Column 0
		wxDataViewTextRenderer* tr = new wxDataViewTextRenderer("string");
		wxDataViewColumn* col = new wxDataViewColumn("Key", tr, 0, FromDIP(350), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
		col->SetMinWidth(FromDIP(150));
		view->AppendColumn(col);

		// Column 1
		tr = new wxDataViewTextRenderer("string");
		col = new wxDataViewColumn("Value", tr, 1, FromDIP(350), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
		col->SetMinWidth(FromDIP(150));
		view->AppendColumn(col);

		//#if wxUSE_DRAG_AND_DROP && wxUSE_UNICODE
		//e_ctrl->EnableDragSource(wxDF_UNICODETEXT);
		//e_ctrl->EnableDropTarget(wxDF_UNICODETEXT);
		//#endif
		view->AssociateModel(Parser.get());
		view->Expand(wxDataViewItem(root));
	}

	splitter->SetMinimumPaneSize(20);
	splitter->SetSashGravity(0.5);
	splitter->SplitVertically(view, editor);

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(topWrapper, 0, wxEXPAND | wxALL, 5);
	sizer->Add(splitter, 1, wxEXPAND | wxALL, 5);
	SetSizerAndFit(sizer);
	NightMode(false);
}

// You can think of this more as a "reload all preferences" function rather than just
// a Night Mode function
void EntityTab::NightMode(bool recursive) {
	// Tab: Background: 240/240/240 - Foreground: 0/0/0 - Can propagate to children
	// Text Box: Background: 255/255/255  - Foreground: 0/0/0
	// Dataview: Background: 255/255/255 - Foreground: 0/0/0

	bool nightMode = ConfigInterface::NightMode();
	wxColour LabelColor = nightMode ? wxColour(255, 255, 255) : wxColour(0, 0, 0);

	//wxColour test = searchBar->label->GetForegroundColour();
	//wxColour test2 = splitter->GetForegroundColour();
	//wxLogMessage("%d %d %d", test.Red(), test.Green(), test.Blue());
	//wxLogMessage("%d %d %d", test2.Red(), test2.Green(), test2.Blue());

	SetOwnBackgroundColour(nightMode ? wxColour(32, 32, 32) : wxColour(240, 240, 240));
	Refresh();

	// TODO: Decide on final color for dataview, sync it with editor
	// Seems font color reverts to black while node is selected - not sure how to change this
	view->SetBackgroundColour(nightMode ? wxColour(64, 64, 64) : wxColour(255, 255, 255));
	//view->SetBackgroundColour(nightMode ? wxColour(42, 42, 42) : wxColour(255, 255, 255));
	view->SetForegroundColour(nightMode ? wxColour(216, 216, 216) : wxColour(0, 0, 0));
	view->Refresh();

	// Filter Menu
	topWrapper->SetBackgroundColour(nightMode ? wxColour(64, 64, 64) : wxColour(255, 255, 255));
	view->Refresh();

	// Search Bar
	//searchBar->input->SetBackgroundColour(nightMode ? wxColour(32, 32, 32) : wxColour(255, 255, 255));
	//searchBar->input->SetForegroundColour(nightMode ? wxColour(255, 255, 255) : wxColour(0, 0, 0));
	//searchBar->input->Refresh();
	searchBar->label->SetForegroundColour(LabelColor);
	searchBar->caseSensitiveCheck->SetForegroundColour(LabelColor);

	FilterCtrl* filters[] = {layerMenu, classMenu, inheritMenu, componentMenu, keyMenu};
	for (int i = 0; i < sizeof(filters) / sizeof(filters[0]); i++) {
		FilterCtrl* f = filters[i];

		f->label->SetForegroundColour(LabelColor);

		// This requires using SetItems to refresh the list (or the Refresh Filters button)
		// Plus foreground color doesn't seem to work on it
		//f->list->SetBackgroundColour(wxColor(83, 83, 83));

		// I change the background color on quickInputEnter - so I'd need to modify that behavior more for this to work
		//f->quickInput->SetBackgroundColour(nightMode ? wxColour(32, 32, 32) : wxColour(255, 255, 255));
		//f->quickInput->SetForegroundColour(nightMode ? wxColour(255, 255, 255) : wxColour(0, 0, 0));
		//f->quickInput->Refresh();
	}
	
	caseSensCheck->SetForegroundColour(LabelColor);
	spawnMenu->toggle->SetForegroundColour(LabelColor);
	for (int i = 0; i < sizeof(spawnMenu->labels) / sizeof(spawnMenu->labels[0]); i++) {
		spawnMenu->labels[i]->SetForegroundColour(LabelColor);
	}

	if(!recursive)
		return;

	// Call everything instead of only setting the colors, as a way of ensuring
	// everything is properly set on config reload
	editor->ReloadPreferences(); 
}

bool EntityTab::IsNewAndUntouched()
{
	return filePath == "" && Parser->FileUpToDate() && !editor->Modified();
}

bool EntityTab::UnsavedChanges()
{
	return !Parser->FileUpToDate() || editor->Modified();
}

void EntityTab::setAppendMenu()
{
	const size_t index = 8;
	if (appendMenu != nullptr) // Destroy deletes the wxMenuItem *and* any attached submenu, preventing memory leaks
		viewMenu.Destroy(NODEVIEW_APPENDMENU_ID); 

	appendMenu = ConfigInterface::getMenu();
	viewMenu.Insert(index, NODEVIEW_APPENDMENU_ID, "Append", appendMenu);

	size_t bindCount = appendMenu->GetAccelCount() + 11;
	wxAcceleratorEntry* entries = new wxAcceleratorEntry[bindCount];
	wxAcceleratorEntry* inc = entries;

	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'Z', NODEVIEW_UNDO);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'Y', NODEVIEW_REDO);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'X', NODEVIEW_CUTSELECTED_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'C', NODEVIEW_COPYSELECTED_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'V', NODEVIEW_PASTE_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'A', NODEVIEW_SELECTALLENTS_ACCEL);
	new (inc++)	wxAcceleratorEntry(wxACCEL_CTRL, 'D', NODEVIEW_DELETESELECTED_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_NORMAL, WXK_DELETE, NODEVIEW_DELETESELECTED_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'E', NODEVIEW_SETPOSITION_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'R', NODEVIEW_SETORIENTATION_ACCEL);
	new (inc++) wxAcceleratorEntry(wxACCEL_CTRL, 'W', NODEVIEW_TELEPORTPOSITION_ACCEL);
	appendMenu->CopyAccels(inc);

	wxAcceleratorTable accel(bindCount, entries);
	view->SetAcceleratorTable(accel);

	// Must delete entries after, accelerator table seems to copy array instead of taking ownership
	delete[] entries; 
}

void EntityTab::onFilterRefresh(wxCommandEvent& event)
{
	// Edge Case: should we force a commit before refreshing?
	refreshFilters();
}

void EntityTab::refreshFilters() {
	//TIMESTART
	Parser->refreshFilterMenus(layerMenu, classMenu, inheritMenu, componentMenu);
	//TIMESTOP("Refresh Filters")
}

void EntityTab::onFilterCaseCheck(wxCommandEvent& event)
{
	applyFilters(false);
}

void EntityTab::onFilterDelKeys(wxCommandEvent& event)
{
	keyMenu->deleteUnchecked();
}

void EntityTab::onFilterClearAll(wxCommandEvent& event)
{
	applyFilters(true);
}

/* Populates the Spawn Position Filter data from mh_spawninfo spawnposition clipboard data*/
void EntityTab::filterSetSpawninfo()
{
	// Todo: should probably add more safety checks
	wxClipboard* clipboard = wxTheClipboard->Get();
	wxTextDataObject data;
	clipboard->GetData(data);
	std::string text(data.GetText());
	clipboard->Close();

	// We assume the clipboard only contains spawnPosition, not spawnOrientation too
	size_t xIndex = text.find("x = ") + 4;
	size_t xLen = text.find(';', xIndex) - xIndex;
	std::string x = text.substr(xIndex, xLen);

	size_t yIndex = text.find("y = ") + 4;
	size_t yLen = text.find(';', yIndex) - yIndex;
	std::string y = text.substr(yIndex, yLen);

	size_t zIndex = text.find("z = ") + 4;
	size_t zLen = text.find(';', zIndex) - zIndex;
	std::string z = text.substr(zIndex, zLen);

	spawnMenu->setData(x, y, z);
}

void EntityTab::applyFilters(bool clearAll)
{
	/*
	* With the refactored filter system, it should no longer be necessary to force a commit,
	* clear the editor, or clear undo/redo history before changing filters
	*/

	if (clearAll)
	{
		inheritMenu->uncheckAll();
		componentMenu->uncheckAll();
		classMenu->uncheckAll();
		layerMenu->uncheckAll();
		keyMenu->uncheckAll();
		spawnMenu->deactivate();
	}

	Sphere newSphere;
	bool filterSpawns = spawnMenu->activated() && spawnMenu->getData(newSphere);

	Parser->SetFilters(layerMenu->list, classMenu->list, inheritMenu->list, componentMenu->list,
		filterSpawns, newSphere, keyMenu->list, caseSensCheck->IsChecked());
	wxDataViewItem p(nullptr); // Todo: should try to improve this so we don't destroy entire root
	wxDataViewItem r(root);
	Parser->ItemDeleted(p, r);
	Parser->ItemAdded(p, r);
	view->Expand(r);

	/*
	* Weird issue reproduced by using shift-click to select a large block of nodes,
	* deleting them, then changing the filters. Large block of unrelated nodes would
	* become selected afterwards, and would be more "difficult" to deselect than normal
	* (i.e. simply left clicking on one wouldn't deselect all the others). UnselectAll
	* also doesn't seem to work on these, must instead set selections to nothing.
	* 
	* Todo: Monitor for more reports of this issue, see if it crops up elsewhere.
	* Try to find the actual root cause instead of applying this bandaid fix
	*/
	wxDataViewItemArray empty;
	view->SetSelections(empty);
}

void EntityTab::SearchForward()
{
	searchBar->initiateSearch(false);
}

void EntityTab::SearchBackward()
{
	searchBar->initiateSearch(true);
}

void EntityTab::reloadFile()
{
	try {
		EntityParser* reloaded = new EntityParser(std::string(filePath), Parser->getMode(), true);
		editor->SetActiveNode(nullptr);
		Parser = reloaded;
		root = reloaded->getRoot();
		reloaded->view = view;
		//fileUpToDate = true;

		view->AssociateModel(reloaded);
		view->Expand(wxDataViewItem(root));
		applyFilters(false);
	}
	catch (std::runtime_error e) {
		wxString msg = wxString::Format("File Reload Cancelled\n\n%s", e.what());
		wxMessageBox(msg, filePath, wxICON_ERROR | wxOK | wxCENTER, this);
	}
}

/*
* Returns true if the file was saved, otherwise false
*/
bool EntityTab::saveFile()
{
	if (filePath == "") return false;

	int commitResult = CommitEdits();
	if (commitResult > 0)
		editor->SetActiveNode(nullptr);

	if (Parser->FileUpToDate()) return false; // Need to check this when commitResult <= 0

	Parser->WriteToFile(std::string(filePath), compressOnSave && !compressOnSave_ForceDisable);

	if (commitResult < 0) // Logic Error: This won't pop up if editor is bugged while file is up to date
		wxMessageBox("File was saved. But you must fix syntax errors before saving contents of text box.",
			"File Saved", wxICON_WARNING | wxOK);
	//fileUpToDate = true;
	wxLogMessage("Saving Finished");
	return true;
}

/*
* Returns 0 if there was nothing to commit
* Returns Positive value if commit was successful
* Returns Negative value if commit failed due to parse error
*/
int EntityTab::CommitEdits()
{
	//wxLogMessage("CommitEdits");
	if (!editor->Modified()) return 0;

	EntNode* replacing = editor->Node();
	EntNode* parent = replacing->getParent();
	int replacingIndex = parent->getChildIndex(replacing);

	ParseResult outcome = Parser->EditTree(std::string(editor->GetText()), parent, replacingIndex, 1, autoNumberLists, false);
	if (!outcome.success)
	{
		editor->setAnnotationError(outcome.errorLineNum, outcome.errorMessage);
		return -1;
	}
	Parser->PushGroupCommand();

	// Note: Originally, I wanted to show the text of the first node added to the tree
	// after a commit, but that's likely too complicated
	return 1;
}

void EntityTab::UndoRedo(bool undo)
{
	wxString msg = undo ? "Undo" : "Redo";

	if (editor->Modified() && wxMessageBox("This action will discard your current text edits. Proceed?",
		msg, wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
		return;

	bool result = undo? Parser->Undo() : Parser->Redo();
	if (!result)
	{
		wxMessageBox("Nothing to " + msg, msg, wxOK | wxICON_INFORMATION);
		return;
	}

	wxLogMessage("Undo/Redo on node tree executed successfully");

	// Note: Originally, I wanted undo / redo to set the active node to
	// one that was restored by the undo / redo operation. Not really
	// possible with group commands at the moment though
	editor->SetActiveNode(nullptr);
	//fileUpToDate = false;
}

void EntityTab::onDataviewChar(wxKeyEvent& event)
{
	/*
	* The dataview control's normal behavior with shift + up/down navigation is inadequate.
	* The selection events generated when navigating in this manner always contain the top-most
	* selected item, instead of the item we actually navigated to. This means the current item
	* is forcibly set to the highest selected item, making shift + down navigation unusable
	* and shift + up multi-selection less flexible than it could be.
	* 
	* We remedy this by catching these events, and swapping the shift behavior with control behavior
	* This lets us simulate ordinary selection event behavior to make shift up/down navigation function as desired
	* 
	* Todo: Do we want shift + up/down onto an already-selected node to deselect it?
	*/
	int c = event.GetKeyCode();
	if ((c == WXK_UP || c == WXK_DOWN) && event.ShiftDown()) 
	{
		event.SetShiftDown(false);
		event.SetControlDown(true);

		/*
		* If we just use event.skip(), the wxDataViewItem returned by
		* GetCurrentItem() will be the one from before this key event is processed
		* To get the updated data, we must forcibly process our reconfigured event right now
		*/
		view->GetMainWindow()->GetEventHandler()->ProcessEvent(event);

		wxDataViewItem current = view->GetCurrentItem();
		view->Select(current);
		dataviewMouseAction(current);
	}
	else event.Skip();
}

/* Returns true if it's safe to show the right click menu or perform accelerator actions */
bool EntityTab::dataviewMouseAction(wxDataViewItem item) 
{
	//wxLogMessage("Mouse Action");
	EntNode* selection = (EntNode*)item.GetID();

	// Goals:
	// - If committing fails from syntax error, do not show menu
	// - If the clicked node was modified by commit, do not show menu
	if (selection == nullptr) {
		if(CommitEdits() < 0) return false;
		editor->SetActiveNode(nullptr);
	} else {
		bool isDescendant = selection->IsDescendantOf(editor->Node());
		int commitResult = CommitEdits();
		if(commitResult < 0) return false;

		if (isDescendant && commitResult > 0) {
			editor->SetActiveNode(nullptr); // Test if the selected node has been modified after committing
			return false; // Shouldn't show right-click menu if node has been modified during commit
		} else {
			// Use SetCurrentItem to ensure the node we operate on via right click or accelerator actions
			// is always the most recently selected node, and not those highlighted by the parser
			editor->SetActiveNode(selection);
			view->SetCurrentItem(item);
		}
	}
	return true;
}

/*
* This event fires when we:
* - Left mouse down on an unselected node
* - Left mouse up on a selected node
* - Left mouse down on nothing, if at least one node is selected
*/
void EntityTab::onNodeSelection(wxDataViewEvent& event)
{
	//wxLogMessage("Selection change event fired");
	dataviewMouseAction(event.GetItem());
}

void EntityTab::onNodeDoubleClick(wxDataViewEvent& event)
{
	// Implementation Note: If edit/components are not containers, then it will always attempt
	// to expand instead of collapse. But realistically...this should be acceptable
	//wxLogMessage("Double click");
	EntNode* node = (EntNode*)event.GetItem().GetID();
	if(node == nullptr || node == Parser->getRoot())
		return;

	EntNode* entity = node->getEntity();
	EntNode& editNode = (*entity)["entityDef"]["edit"];
	if (&editNode == EntNode::SEARCH_404)
		return;

	// NEW
	EntNode* options[] = {
		&editNode["components"], // idComponentEntity
		&editNode["componentTimeLine"]["entityEvents"], // idTarget_Timeline
		&editNode["encounterComponent"]["entityEvents"]["item[0]"]["events"], // idEncounterManager
		&editNode // Other entities
	};

	for (int i = 0; i < sizeof(options) / sizeof(EntNode*); i++) {
		if(options[i] == EntNode::SEARCH_404)
			continue;

		wxDataViewItem optionItem(options[i]);
		if(view->IsExpanded(optionItem))
			view->Collapse(wxDataViewItem(entity));
		else {
			view->Expand(optionItem);
			view->EnsureVisible(optionItem);
		}

		return;
	}
}

void EntityTab::onExpandEntity(wxCommandEvent& event)
{
	// This is probably okay to do...
	wxDataViewEvent temp(wxEVT_DATAVIEW_ITEM_ACTIVATED, view, view->GetCurrentItem());
	onNodeDoubleClick(temp);
}

/* 
* It's best that we nullify the normal behavior when right-mouse-down occurs in the dataview,
* to prevent duplicate commit attempts and the context menu popping up for the wrong node.
*/
void EntityTab::onViewRightMouseDown(wxMouseEvent& event) 
{}

/*
* Context menu event fires on right mouse up
*/
void EntityTab::onNodeContextMenu(wxDataViewEvent& event)
{
	//wxLogMessage("Context Menu event fired");
	wxDataViewItem item(event.GetItem());
	if (!view->IsSelected(item)) {
		view->UnselectAll();
		view->Select(item);
	}

	if (dataviewMouseAction(item))
	{
		EntNode* node = (EntNode*)item.GetID();
		bool online = Meathook::IsOnline();

		// To prevent errors from occurring with accelerator hotkeys,
		// we should also ensure these conditions are checked when executing
		// the functions
		viewMenu.Enable(NODEVIEW_UNDO, true);
		viewMenu.Enable(NODEVIEW_REDO, true);
		viewMenu.Enable(NODEVIEW_CUTSELECTED, view->HasSelection());
		viewMenu.Enable(NODEVIEW_COPYSELECTED, view->HasSelection());
		viewMenu.Enable(NODEVIEW_PASTE, node != nullptr && node != root);
		viewMenu.Enable(NODEVIEW_EXPANDENT, node!= nullptr && node != root);
		viewMenu.Enable(NODEVIEW_SELECTALLENTS, true);
		viewMenu.Enable(NODEVIEW_DELETESELECTED, view->HasSelection());
		viewMenu.Enable(NODEVIEW_APPENDMENU_ID, node != nullptr && node != root);

		// Todo: Verify edit node exists
		viewMenu.Enable(NODEVIEW_SETPOSITION, online && node != nullptr && node != root);
		viewMenu.Enable(NODEVIEW_SETORIENTATION, online && node != nullptr && node != root);
		viewMenu.Enable(NODEVIEW_TELEPORTPOSITION, online && node != nullptr && node != root);


		view->PopupMenu(&viewMenu);
	}
}

void EntityTab::onNodeContextAccelerator(wxCommandEvent& event)
{
	//wxLogMessage("Dataview accelerator action");
	if (!dataviewMouseAction(view->GetCurrentItem())) {
		wxMessageBox("Couldn't perform this action due to a parse error (fix it) or a commit (try again).",
			"Hotkey Action Cancelled", wxICON_WARNING | wxOK);
		return;
	}

	switch (event.GetId())
	{
		case NODEVIEW_CUTSELECTED_ACCEL:
		onCutSelectedNodes(event);
		break;

		case NODEVIEW_COPYSELECTED_ACCEL:
		onCopySelectedNodes(event);
		break;

		case NODEVIEW_PASTE_ACCEL:
		onPaste(event);
		break;

		case NODEVIEW_SELECTALLENTS_ACCEL:
		onSelectAllEntities(event);
		break;

		case NODEVIEW_DELETESELECTED_ACCEL:
		onDeleteSelectedNodes(event);
		break;

		case NODEVIEW_SETPOSITION_ACCEL:
		onSetSpawnPosition(event);
		break;

		case NODEVIEW_SETORIENTATION_ACCEL:
		onSetSpawnOrientation(event);
		break;

		case NODEVIEW_TELEPORTPOSITION_ACCEL:
		onTeleportToEntity(event);
		break;

		default: 
		{ // Assume all other values are an append menu action
			std::string text;
			EntNode* node = (EntNode*)view->GetCurrentItem().GetID();
			if (node == nullptr || node == root) {
				wxLogMessage("Cannot attempt an Append at this location");
				return;
			}
			EntNode* parent = node->getParent();

			if (ConfigInterface::getText(event.GetId(), text)) 
			{
				ParseResult outcome = Parser->EditTree(text, parent, parent->getChildIndex(node) + 1, 0, autoNumberLists, true);
				if(!outcome.success)
					wxMessageBox(outcome.errorMessage, "Append Failed", wxICON_ERROR | wxOK);
				else {
					Parser->PushGroupCommand();
					editor->SetActiveNode(nullptr); // Todo: Adjust this? (Also perhaps merge this and paste function? This basically copies from it's logic)
					//fileUpToDate = false;
					wxLogMessage("Append Successful");
				}
			}
			else wxLogMessage("Append canceled or could not find text for option");
		}
		break;
	}
}

void EntityTab::onUndo(wxCommandEvent& event)
{
	UndoRedo(true);
}

void EntityTab::onRedo(wxCommandEvent& event)
{
	UndoRedo(false);
}

void EntityTab::onCutSelectedNodes(wxCommandEvent& event)
{
	onCopySelectedNodes(event);
	onDeleteSelectedNodes(event);
}

void EntityTab::onCopySelectedNodes(wxCommandEvent& event)
{
	wxDataViewItemArray selections;
	view->GetSelections(selections);

	if(selections.IsEmpty()) return;

	wxClipboard* clipboard = wxTheClipboard->Get();
	if (clipboard->Open())
	{
		std::string text;
		int numCopies = 0;
		for (wxDataViewItem item : selections)
		{
			EntNode* node = (EntNode*)item.GetID();
			if (node == root)
				wxLogMessage("Cannot copy root node");
			else {
				node->generateText(text);
				text.push_back('\n');
				numCopies++;
			}
		}
		clipboard->SetData(new wxTextDataObject(text));
		clipboard->Close();
		wxLogMessage("%i nodes copied to clipboard as text.", numCopies);
	}
	else wxLogMessage("Could not access clipboard");
}

void EntityTab::onPaste(wxCommandEvent& event)
{
	EntNode* node = (EntNode*)view->GetCurrentItem().GetID();
	if(node == nullptr || node == root) return;
	EntNode* parent = node->getParent();

	wxClipboard* clipboard = wxTheClipboard->Get();
	if (clipboard->Open())
	{
		if (clipboard->IsSupported(wxDF_TEXT))
		{
			wxTextDataObject data;
			clipboard->GetData(data);

			std::string text(data.GetText());
			
			ParseResult outcome = Parser->EditTree(text, parent, parent->getChildIndex(node) + 1, 0, autoNumberLists, true);
			if (!outcome.success)
				wxMessageBox(outcome.errorMessage, "Paste Failed ", wxICON_ERROR | wxOK);
			else {
				Parser->PushGroupCommand();
				editor->SetActiveNode(nullptr); // Todo: Adjust this?
				//fileUpToDate = false;
			}
		}
		else wxLogMessage("Could not paste non-text content");

		clipboard->Close();
	}
	else wxLogMessage("Could not access clipboard");
}

void EntityTab::onSelectAllEntities(wxCommandEvent& event)
{
	wxDataViewItemArray rootChildren;
	Parser->GetChildren(wxDataViewItem(root), rootChildren);
	view->SetSelections(rootChildren);
}

void EntityTab::onDeleteSelectedNodes(wxCommandEvent& event)
{
	wxDataViewItemArray selections;
	view->GetSelections(selections);
	if(selections.IsEmpty()) return;

	int numDeletions = 0;
	editor->SetActiveNode(nullptr);
	for (wxDataViewItem item : selections) {
		EntNode* node = (EntNode*)item.GetID();
		if (node == root)
			wxLogMessage("Cannot delete the root node");
		else if (view->IsSelected(item)) { // Tests if the node was removed in a previous deletion
			EntNode* parent = node->getParent();
			Parser->EditTree("", parent, parent->getChildIndex(node), 1, autoNumberLists, false);
			numDeletions++;
		}
	}
	Parser->PushGroupCommand();
	//fileUpToDate = false;
	wxLogMessage("Deleted %i nodes and their children", numDeletions);
}

void EntityTab::onSetSpawnPosition(wxCommandEvent& event)
{
	// Verify the selection is valid
	EntNode* selection = (EntNode*)view->GetCurrentItem().GetID();
	if (selection == nullptr || selection == root) return;
	EntNode* entity = selection->getEntity();

	EntNode& edit = (*entity)["entityDef"]["edit"];
	if (&edit == EntNode::SEARCH_404) return;

	// Copy and retrieve from clipboard
	if(!Meathook::CopySpawnPosition()) return;
	editor->SetActiveNode(nullptr);

	wxClipboard* clipboard = wxTheClipboard->Get();
	clipboard->Open();
	wxTextDataObject data;
	clipboard->GetData(data);

	std::string text(data.GetText());
	clipboard->Close();

	// Perform Edit operation
	EntNode& spawnPosition = edit["spawnPosition"];
	if (&spawnPosition == EntNode::SEARCH_404)
		Parser->EditTree(text, &edit, 0, 0, false, true);
	else Parser->EditTree(text, &edit, edit.getChildIndex(&spawnPosition), 1, false, true);

	Parser->PushGroupCommand();
	//fileUpToDate = false;
}

void EntityTab::onSetSpawnOrientation(wxCommandEvent& event)
{
	// Verify the selection is valid
	EntNode* selection = (EntNode*)view->GetCurrentItem().GetID();
	if (selection == nullptr || selection == root) return;
	EntNode* entity = selection->getEntity();

	EntNode& edit = (*entity)["entityDef"]["edit"];
	if (&edit == EntNode::SEARCH_404) return;

	// Copy and retrieve from clipboard
	if(!Meathook::CopySpawnOrientation()) return;
	editor->SetActiveNode(nullptr);

	wxClipboard* clipboard = wxTheClipboard->Get();
	clipboard->Open();
	wxTextDataObject data;
	clipboard->GetData(data);

	std::string text(data.GetText());
	clipboard->Close();

	// Perform Edit operation
	EntNode& spawnOrientation = edit["spawnOrientation"];
	if (&spawnOrientation == EntNode::SEARCH_404)
		Parser->EditTree(text, &edit, 0, 0, false, true);
	else Parser->EditTree(text, &edit, edit.getChildIndex(&spawnOrientation), 1, false, true);

	Parser->PushGroupCommand();
	//fileUpToDate = false;
}

void EntityTab::onTeleportToEntity(wxCommandEvent& event)
{
	// Verify the selection is valid
	EntNode* selection = (EntNode*)view->GetCurrentItem().GetID();
	if (selection == nullptr || selection == root) return;
	EntNode* entity = selection->getEntity();

	std::string command = "teleportposition";

	EntNode& spawnPosition = (*entity)["entityDef"]["edit"]["spawnPosition"];
	if (&spawnPosition == EntNode::SEARCH_404) {
		command.append(" 0 0 0 0");
	} else {
		EntNode& x = spawnPosition["x"];
		EntNode& y = spawnPosition["y"];
		EntNode& z = spawnPosition["z"];

		if(&x == EntNode::SEARCH_404) command.append(" 0");
		else {
			command.push_back(' ');
			command.append(x.getValue());
		}

		if (&y == EntNode::SEARCH_404) command.append(" 0");
		else {
			command.push_back(' ');
			command.append(y.getValue());
		}

		if (&z == EntNode::SEARCH_404) command.append(" 0");
		else {
			command.push_back(' ');
			command.append(z.getValue());
		}
		command.append(" 0");
	}
	wxLogMessage("%s", command);
	if(!Meathook::ExecuteCommand(command))
		wxLogMessage("Teleport command failed?");
}

void EntityTab::onFilterMenuShowHide(wxCollapsiblePaneEvent& event)
{
	Freeze(); // Freezing seems to fix visual bugs that may occur when expanding
	Layout(); // for the first time
	Thaw();
}

void EntityTab::action_PropMovers()
{
	const size_t MOVER_NAME_APPEND_LEN = 13;
	const char* MOVER_NAME_APPEND = "_offset_mover";

	const char* IDMOVER =
"entity {"
"    entityDef %s_offset_mover {" // same name as the pickup but with something added to the end like "_offset_fix"
"    inherit = \"func/mover\";"
"    class = \"idMover\";"
"    expandInheritance = false;"
"    poolCount = 0;"
"    poolGranularity = 2;"
"    networkReplicated = true;"
"    disableAIPooling = false;"
"    edit = {"
"        %s" // spawnPosition - copy from pickup
"        %s" // spawnOrientation - copy from pickup
"        renderModelInfo = {"
"            model = NULL;"
"        }"
"        clipModelInfo = {"
"            clipModelName = NULL;"
"        }"
"    }"
"}"
"}";

	if(CommitEdits() < 0)
		return;
	editor->SetActiveNode(nullptr);

	wxLogMessage("Binding idProp2s to idMovers");

	std::set<std::string_view> moverNames;
	std::vector<EntNode*> props;

	// Identify all Prop and Mover entities
	int deletedMovers = 0;
	for (int i = 0, max = root->getChildCount(); i < max; i++)
	{
		EntNode* entity = root->ChildAt(i);
		EntNode& defNode = (*entity)["entityDef"];

		std::string_view classVal = defNode["class"].getValueUQ();
		if(classVal == "idProp2")
			props.push_back(entity);
		else if (classVal == "idMover") 
		{
			moverNames.insert(defNode.getValue());
			// Remove any other "offset_fix" mover
			//std::string_view defVal = defNode.getValue();
			//size_t index = defVal.rfind(MOVER_NAME_APPEND, defVal.length() - MOVER_NAME_APPEND_LEN); 
			//if (index != std::string_view::npos)
			//{
			//	parser->EditTree("", root, i, 1, false);
			//	i--;
			//	max--;
			//}
		}
			
	}

	wxLogMessage("Counted %zu idProp2 entities", props.size());

	// Create the idMover entites
	const size_t MAX = 4096;
	char buffer[MAX];
	std::string movers;
	std::vector<EntNode*> propsUsed;
	propsUsed.reserve(props.size());

	for (EntNode* prop : props)
	{ // Must use strings instead of string_view to insure null termination for snprintf
		std::string propName = std::string((*prop)["entityDef"].getValue());
		std::string moverName = propName + MOVER_NAME_APPEND;

		if(moverNames.count(moverName) > 0)
			continue;
		propsUsed.push_back(prop);

		EntNode& editNode = (*prop)["entityDef"]["edit"];
		std::string spawnPosition;
		std::string spawnOrientation;

		editNode["spawnPosition"].generateText(spawnPosition);
		editNode["spawnOrientation"].generateText(spawnOrientation);
		
		int length = std::snprintf(buffer, MAX, IDMOVER, propName.data(), spawnPosition.data(), spawnOrientation.data());
		if (length == MAX)
		{
			wxMessageBox("Buffer size limit reached", "Binding Props to Movers Failed", wxICON_WARNING | wxOK);
			return;
		}
		movers.append(buffer);
	}

	wxLogMessage("Creating idMovers for %zu idProp2 entities. This may take some time - please be patient.", propsUsed.size());

	EntNode* version = root->ChildAt(0);
	ParseResult result = Parser->EditTree(movers, root, 0, 0, false, true);
	int moversAdded = root->getChildIndex(version);

	wxLogMessage("%i idMover entities created", moversAdded);
	
	int manualBindCount = 0;
	wxString manualBindLog = "%i idProp2 entities already had bindInfo defined. You must manually bind the following:\n";
	for (int i = 0; i < moversAdded; i++) 
	{
		EntNode* prop = propsUsed[i];
		int propIndex = root->getChildIndex(prop);
		Parser->EditPosition(root, 0, propIndex, true); // Should place below the prop

		EntNode& edit = (*prop)["entityDef"]["edit"];
		EntNode& bindInfo = edit["bindInfo"];
		if (&bindInfo != EntNode::SEARCH_404)
		{
			manualBindCount++;
			manualBindLog.append( (*prop)["entityDef"].getValueWX() + '\n');
			continue;
		}

		std::string bindString = "bindInfo = { bindParent = \""
			+ std::string((*prop)["entityDef"].getValue()) 
			+ MOVER_NAME_APPEND + "\";}";
			
		Parser->EditTree(bindString, &edit, 0, 0, false, false);
	}
	
	if(manualBindCount > 0)
		wxLogMessage(manualBindLog, manualBindCount);
	wxLogMessage("Finished Binding Props to Movers");

	Parser->PushGroupCommand();
	//fileUpToDate = false;
}

void EntityTab::action_FixTraversals()
{
	std::string inheritLine = " inherit = \"info/traversal\"; ";

	if(CommitEdits() < 0)
		return;
	editor->SetActiveNode(nullptr);

	int numNodes = 0;
	for (int i = 0, max = root->getChildCount(); i < max; i++) 
	{
		EntNode* entity = root->ChildAt(i);
		EntNode& defNode = (*entity)["entityDef"];

		// Check if it's a traversal node, and if so, if the inherit line exists
		if (defNode["class"].getValueUQ() == "idInfoTraversal"
			&& &defNode["inherit"] == EntNode::SEARCH_404) {

			Parser->EditTree(inheritLine, &defNode, 0, 0, false, false);
			numNodes++;
		}
	}

	wxLogMessage("Inherits added for %i traversal entities", numNodes);

	Parser->PushGroupCommand();
	//fileUpToDate = false;
}
