#include "EntityTab.h"
#include "FilterMenus.h"

wxBEGIN_EVENT_TABLE(EntityTab, wxPanel)
	EVT_COLLAPSIBLEPANE_CHANGED(wxID_ANY, onFilterMenuShowHide)

	EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, onNodeSelection)
	EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, onNodeActivation)
	EVT_DATAVIEW_ITEM_CONTEXT_MENU(wxID_ANY, onNodeRightClick)
wxEND_EVENT_TABLE()

EntityTab::EntityTab(wxWindow* parent, const wxString name, const wxString& path)
	: wxPanel(parent, wxID_ANY), tabName(name), filePath(path)
{
	/* Initialize parser */
	if (path == "")
		parser = new EntityParser();
	else
		parser = new EntityParser(std::string(path));
	compressOnSave = parser->wasFileCompressed();
	root = parser->getRoot();
	model = new EntityModel(root);

	/* Filter Menu (should be initialized before model is associated with view) */
	wxGenericCollapsiblePane* topWrapper = new wxGenericCollapsiblePane(this, wxID_ANY, "Filter Menu",
		wxDefaultPosition, wxDefaultSize, wxCP_NO_TLW_RESIZE);
	{
		wxWindow* topWindow = topWrapper->GetPane();

		/* Layer, Class and Inherit filter lists */
		wxBoxSizer* checklistSizer = new wxBoxSizer(wxHORIZONTAL);
		layerMenu = new FilterCtrl(this, topWindow, "Layers", false);
		classMenu = new FilterCtrl(this, topWindow, "Classes", false);
		inheritMenu = new FilterCtrl(this, topWindow, "Inherits", false);
		checklistSizer->Add(layerMenu->container, 1, wxLEFT | wxRIGHT, 10);
		checklistSizer->Add(classMenu->container, 1, wxLEFT | wxRIGHT, 10);
		checklistSizer->Add(inheritMenu->container, 1, wxLEFT | wxRIGHT, 10);
		refreshFilters();

		/* Spawn Position Filter */
		spawnMenu = new SpawnFilter(this, topWindow);

		/* Text Filter */
		wxBoxSizer* textFilterSizer = new wxBoxSizer(wxVERTICAL);
		{
			keyMenu = new FilterCtrl(this, topWindow, "Text Key", true);

			caseSensCheck = new wxCheckBox(topWindow, wxID_ANY, "Case Sensitive");
			caseSensCheck->SetValue(true);
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

		wxBoxSizer* secondRowSizer = new wxBoxSizer(wxHORIZONTAL);
		secondRowSizer->Add(textFilterSizer, 1, wxALL, 10);
		secondRowSizer->Add(compact, 2, wxALL, 10);

		/* Put everything together */
		wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
		topSizer->Add(checklistSizer, 0, wxEXPAND);
		topSizer->Add(secondRowSizer, 0, wxEXPAND);
		topWindow->SetSizerAndFit(topSizer);
	}

	/* Initialize controls */
	wxSplitterWindow* splitter = new wxSplitterWindow(this);
	editor = new EntityEditor(splitter, wxID_ANY, wxDefaultPosition, wxSize(300, 300));
	view = new wxDataViewCtrl(splitter, wxID_ANY, wxDefaultPosition, wxSize(300, 300));

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

		//view->Bind(wxEVT_CHAR, &MyFrame::OnDataViewChar, this);
		//#if wxUSE_DRAG_AND_DROP && wxUSE_UNICODE
		//e_ctrl->EnableDragSource(wxDF_UNICODETEXT);
		//e_ctrl->EnableDropTarget(wxDF_UNICODETEXT);
		//#endif
		view->AssociateModel(model.get());
		view->Expand(wxDataViewItem(root));
	}

	splitter->SetMinimumPaneSize(20);
	splitter->SetSashGravity(0.5);
	splitter->SplitVertically(view, editor);

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(topWrapper, 0, wxEXPAND | wxALL, 5);
	sizer->Add(splitter, 1, wxEXPAND | wxALL, 5);
	SetSizerAndFit(sizer);
}

EntityTab::~EntityTab()
{
	delete parser;
}

bool EntityTab::IsNewAndUntouched()
{
	return filePath == "" && fileUpToDate && !editor->Modified();
}

bool EntityTab::UnsavedChanges()
{
	return !fileUpToDate || editor->Modified();
}

void EntityTab::onFilterRefresh(wxCommandEvent& event)
{
	// Edge Case: should we force a commit before refreshing?
	refreshFilters();
}

void EntityTab::refreshFilters() {
	model->refreshFilterMenus(layerMenu->list, classMenu->list, inheritMenu->list);
	layerMenu->refreshAutocomplete();
	classMenu->refreshAutocomplete();
	inheritMenu->refreshAutocomplete();
}

void EntityTab::onFilterCaseCheck(wxCommandEvent& event)
{
	if (!applyFilters(false))
		caseSensCheck->SetValue(!event.IsChecked());
}

void EntityTab::onFilterDelKeys(wxCommandEvent& event)
{
	keyMenu->deleteUnchecked();
}

void EntityTab::onFilterClearAll(wxCommandEvent& event)
{
	applyFilters(true);
}

bool EntityTab::applyFilters(bool clearAll)
{
	/*
	* It should be safe to apply filters without a commit check.
	* However, in debug builds, committing a root child that was filtered out
	* of the view creates a debug error popup accompanied by small memory leaks.
	*
	* Unclear if these leaks are present in release builds. However, better to be
	* safe than sorry for preventing unclear behavior.
	* 
	* TODO: if there is an error, the new responsive filter menus will become desynced
	* with the actual applied filters. Adjust for this later
	*/
	if (CommitEdits() < 0)
	{
		wxMessageBox("Please fix syntax errors before applying new filters.", "Cannot Change Filters",
			wxICON_WARNING | wxOK);
		return false;
	}
	else editor->SetActiveNode(nullptr);
	parser->ClearUndoStack(); // Must clear undo stack for the same reason as above

	if (clearAll)
	{
		inheritMenu->uncheckAll();
		classMenu->uncheckAll();
		layerMenu->uncheckAll();
		keyMenu->uncheckAll();
		spawnMenu->deactivate();
	}

	Sphere newSphere;
	bool filterSpawns = spawnMenu->activated() && spawnMenu->getData(newSphere);

	model->SetFilters(layerMenu->list, classMenu->list, inheritMenu->list,
		filterSpawns, newSphere, keyMenu->list, caseSensCheck->IsChecked());
	wxDataViewItem p(nullptr); // Todo: should try to improve this so we don't destroy entire root
	wxDataViewItem r(root);
	model->ItemDeleted(p, r);
	model->ItemAdded(p, r);
	view->Expand(r);
	return true;
}

void EntityTab::saveFile()
{
	if (filePath == "") return;

	int commitResult = CommitEdits();
	if (commitResult > 0)
		editor->SetActiveNode(nullptr);

	if (fileUpToDate) return; // Need to check this when commitResult <= 0

	parser->writeToFile(std::string(filePath), compressOnSave);

	if (commitResult < 0)
		wxMessageBox("File was saved. But you must fix syntax errors before saving contents of text box.",
			"File Saved", wxICON_WARNING | wxOK);
	fileUpToDate = true;
	wxLogMessage("Saving Finished");
}

/*
* Returns 0 if there was nothing to commit
* Returns Positive value if commit was successful
* Returns Negative value if commit failed due to parse error
*/
int EntityTab::CommitEdits()
{
	if (!editor->Modified()) return 0;

	EntNode* replacing = editor->Node();
	EntNode* parent = replacing->getParent();
	int replacingIndex = parent->getChildIndex(replacing);

	ParseResult outcome = parser->EditTree(string(editor->GetText()), parent, replacingIndex, 1);
	if (!outcome.success)
	{
		editor->setAnnotationError(outcome.errorLineNum, outcome.errorMessage);
		return -1;
	}

	// Refresh the model
	wxDataViewItem parentItem(parent);
	model->ItemDeleted(parentItem, wxDataViewItem(replacing));

	if (outcome.addedNodes.size() > 0)
	{
		wxDataViewItemArray newItems;
		for (EntNode* e : outcome.addedNodes)
			newItems.Add(wxDataViewItem(e));
		model->ItemsAdded(parentItem, newItems);
	}

	fileUpToDate = false;
	return 1;
}

void EntityTab::UndoRedo(bool undo)
{
	wxWindow* focused = FindFocus();
	if (focused == editor)
	{
		if (undo) editor->Undo();
		else editor->Redo();
		return;
	}

	wxString msg = undo ? "Undo" : "Redo";

	if (editor->Modified() && wxMessageBox("This action will discard your current text edits. Proceed?",
		msg, wxICON_WARNING | wxYES_NO | wxNO_DEFAULT, this) != wxYES)
		return;

	ParseResult outcome = parser->UndoRedo(undo);
	if (!outcome.success)
	{
		wxMessageBox("Nothing to " + msg, msg, wxOK | wxICON_INFORMATION);
		return;
	}

	wxLogMessage("Undo/Redo on node tree executed successfully");
	wxDataViewItem parentItem(outcome.parent);
	wxDataViewItemArray removedItems;
	wxDataViewItemArray addedItems;

	for (EntNode* e : outcome.removedNodes)
		removedItems.Add(wxDataViewItem(e));
	for (EntNode* e : outcome.addedNodes)
		addedItems.Add(wxDataViewItem(e));

	model->ItemsDeleted(parentItem, removedItems);
	model->ItemsAdded(parentItem, addedItems);
	fileUpToDate = false;

	if (addedItems.size() > 0)
	{
		view->Select(addedItems[0]);
		editor->SetActiveNode(outcome.addedNodes[0]);
	}
	else {
		view->Select(parentItem);
		editor->SetActiveNode(outcome.parent);
	}
}

void EntityTab::onNodeSelection(wxDataViewEvent& event)
{
	wxLogMessage("Selection change event fired");
	if (CommitEdits() < 0)
		return;

	// Prevents freed node from being loaded into editor
	EntNode* selection = ((EntNode*)view->GetSelection().GetID());
	if (selection == nullptr || !selection->isValid())
		editor->SetActiveNode(nullptr);
	else editor->SetActiveNode(selection);
	//m_LastNavigation.push_back(event.GetItem());
}

void EntityTab::onNodeActivation(wxDataViewEvent& event)
{
	wxLogMessage("Activation event fired");
	if (CommitEdits() < 0)
		return;

	// Prevents freed node from being loaded into editor
	EntNode* selection = ((EntNode*)event.GetItem().GetID());
	if (selection == nullptr || !selection->isValid())
		editor->SetActiveNode(nullptr);
	else editor->SetActiveNode(selection);
}

void EntityTab::onNodeRightClick(wxDataViewEvent& event)
{
	EntNode* node = (EntNode*)event.GetItem().GetID();
	wxMenu menu;

	if (node == nullptr || node->getType() == NodeType::ROOT) {
		wxLogMessage("Null or root right clicked");
		return;
	}

	menu.Append(70, "Test");
	view->PopupMenu(&menu);
}

void EntityTab::onNodeContextAction(wxCommandEvent& event)
{

}

void EntityTab::onFilterMenuShowHide(wxCollapsiblePaneEvent& event)
{
	Freeze(); // Freezing seems to fix visual bugs that may occur when expanding
	Layout(); // for the first time
	Thaw();
}