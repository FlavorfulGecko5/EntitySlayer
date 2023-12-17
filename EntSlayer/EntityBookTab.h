#include "wx/wx.h"
#include "wx/collpane.h"
#include "wx/splitter.h"
#include "EntityModel.h"

enum TabType {
	NEW_FILE,
	OPENED_FILE,
};

enum TabID {
	TAB_MINIMUM = wxID_HIGHEST + 1,
	BTN_APPLYFILTERS,
	INPUT_KEYFILTER
};

class EntityBookTab : public wxPanel
{
	public:
	TabType type;
	wxString filePath;
	bool compressOnSave;
	bool fileUpToDate;

	FilterList* layerMenu;
	FilterList* classMenu;
	FilterList* inheritMenu;
	wxCheckBox* spawnCheck;
	wxTextCtrl* xInput;
	wxTextCtrl* yInput;
	wxTextCtrl* zInput;
	wxTextCtrl* rInput;

	FilterList* keyMenu;
	wxTextCtrl* keyInput;
	wxCheckBox* caseSensCheck;

	EntityParser* parser;
	EntNode* root;
	wxObjectDataPtr<EntityModel> model; // Need this or model leaks when tab destroyed
	wxDataViewCtrl* view;
	EntityEditor* editor;

	EntityBookTab(wxWindow* parent, const wxString& path = "")
		: wxPanel(parent, wxID_ANY), filePath(path), fileUpToDate(true)
	{
		/* Initialize parser */
		if (path == "") {
			type = TabType::NEW_FILE;
			parser = new EntityParser();
		}
		else {
			type = TabType::OPENED_FILE;
			parser = new EntityParser(std::string(path));
		}
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
			layerMenu = new FilterList(topWindow, "Layers");
			classMenu = new FilterList(topWindow, "Classes");
			inheritMenu = new FilterList(topWindow, "Inherits");
			checklistSizer->Add(layerMenu->container, 1, wxLEFT | wxRIGHT, 10);
			checklistSizer->Add(classMenu->container, 1, wxLEFT | wxRIGHT, 10);
			checklistSizer->Add(inheritMenu->container, 1, wxLEFT | wxRIGHT, 10);
			model->refreshFilterMenus(layerMenu, classMenu, inheritMenu);

			/* Spawn Position Filter */
			wxBoxSizer* spawnFilterSizer = new wxBoxSizer(wxVERTICAL);
			{
				spawnCheck = new wxCheckBox(topWindow, wxID_ANY, "Spawn Position Distance");
				wxStaticText* xLabel = new wxStaticText(topWindow, wxID_ANY, "     x");
				wxStaticText* yLabel = new wxStaticText(topWindow, wxID_ANY, "     y");
				wxStaticText* zLabel = new wxStaticText(topWindow, wxID_ANY, "     z");
				wxStaticText* rLabel = new wxStaticText(topWindow, wxID_ANY, "Radius");
				xInput = new wxTextCtrl(topWindow, wxID_ANY);
				yInput = new wxTextCtrl(topWindow, wxID_ANY);
				zInput = new wxTextCtrl(topWindow, wxID_ANY);
				rInput = new wxTextCtrl(topWindow, wxID_ANY);
				wxBoxSizer* xSizer = new wxBoxSizer(wxHORIZONTAL);
				wxBoxSizer* ySizer = new wxBoxSizer(wxHORIZONTAL);
				wxBoxSizer* zSizer = new wxBoxSizer(wxHORIZONTAL);
				wxBoxSizer* rSizer = new wxBoxSizer(wxHORIZONTAL);

				xSizer->Add(xLabel, wxLEFT | wxRIGHT, 5);
				xSizer->Add(xInput, wxLEFT | wxRIGHT, 5);
				ySizer->Add(yLabel, wxLEFT | wxRIGHT, 5);
				ySizer->Add(yInput, wxLEFT | wxRIGHT, 5);
				zSizer->Add(zLabel, wxLEFT | wxRIGHT, 5);
				zSizer->Add(zInput, wxLEFT | wxRIGHT, 5);
				rSizer->Add(rLabel, wxLEFT | wxRIGHT, 5);
				rSizer->Add(rInput, wxLEFT | wxRIGHT, 5);

				spawnFilterSizer->Add(spawnCheck);
				spawnFilterSizer->Add(xSizer);
				spawnFilterSizer->Add(ySizer);
				spawnFilterSizer->Add(zSizer);
				spawnFilterSizer->Add(rSizer);
			}

			/* Text Filter */
			wxBoxSizer* textFilterSizer = new wxBoxSizer(wxVERTICAL);
			{
				keyMenu = new FilterList(topWindow, "Text Key");
				wxStaticText* keyInputLabel = new wxStaticText(topWindow, wxID_ANY, "Enter Key: ");
				keyInput = new wxTextCtrl(topWindow, TabID::INPUT_KEYFILTER, wxEmptyString,
					wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
				keyInput->SetHint("networkReplicated = true;");
				caseSensCheck = new wxCheckBox(topWindow, wxID_ANY, "Case Sensitive");
				caseSensCheck->SetValue(true);

				wxBoxSizer* inputSizer = new wxBoxSizer(wxHORIZONTAL);
				inputSizer->Add(keyInputLabel, 0);
				inputSizer->Add(keyInput, 1, wxEXPAND);

				textFilterSizer->Add(keyMenu->container, 1, wxEXPAND);
				textFilterSizer->Add(inputSizer, 0, wxEXPAND);
				textFilterSizer->Add(caseSensCheck);
			}

			wxBoxSizer* secondRowSizer = new wxBoxSizer(wxHORIZONTAL);
			secondRowSizer->Add(textFilterSizer, 1, wxALL, 10); // Ensures 1/3rd horizontal space
			secondRowSizer->Add(spawnFilterSizer, 2, wxALL, 15); // (alignment with checklists)

			/* Put everything together */
			wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
			topSizer->Add(checklistSizer, 0, wxEXPAND);
			topSizer->Add(secondRowSizer, 0, wxEXPAND);
			topSizer->Add(new wxButton(topWindow, TabID::BTN_APPLYFILTERS, "Apply"), 0, wxALL, 10);
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

	virtual ~EntityBookTab()
	{
		delete parser;
	}

	bool IsNewAndUntouched()
	{
		return type == TabType::NEW_FILE && fileUpToDate && !editor->Modified();
	}

	bool UnsavedChanges()
	{
		return !fileUpToDate || editor->Modified();
	}

	void onAddKeyFilter(wxCommandEvent& event)
	{
		wxString key = keyInput->GetValue();
		if(key.length() == 0) return;

		keyMenu->AppendAndEnsureVisible(key); // Todo: auto-check newly added items?
		keyInput->Clear();
	}

	void onApplyFilters(wxCommandEvent& event) 
	{
		/*
		* It should be safe to apply filters without a commit check.
		* However, in debug builds, committing a root child that was filtered out
		* of the view creates a debug error popup accompanied by small memory leaks.
		* 
		* Unclear if these leaks are present in release builds. However, better to be
		* safe than sorry for preventing unclear behavior.
		*/
		if(CommitEdits() < 0)
		{
			wxMessageBox("Please fix syntax errors before applying new filters.", "Cannot Change Filters",
				wxICON_WARNING | wxOK);
			return;
		}
		else editor->SetActiveNode(nullptr);

		bool newSpawnFilterSetting = spawnCheck->IsChecked();
		Sphere newSphere;

		if (newSpawnFilterSetting)
		{
			wxString stringX = xInput->GetValue(),
				stringY = yInput->GetValue(),
				stringZ = zInput->GetValue(),
				stringR = rInput->GetValue();
			try {
				newSphere.x = stof(string(stringX));
				newSphere.y = stof(string(stringY));
				newSphere.z = stof(string(stringZ));
				newSphere.r = stof(string(stringR));
			}
			catch (exception) {
				newSpawnFilterSetting = false;
				spawnCheck->SetValue(false);
				wxMessageBox("Could not convert one or more fields to numbers", "Spawn Position Filtering Failed",
					wxICON_WARNING | wxOK);
			}
		}

		model->SetFilters(layerMenu, classMenu, inheritMenu, newSpawnFilterSetting, newSphere, keyMenu, caseSensCheck->IsChecked());
		wxDataViewItem p(nullptr); // Todo: should try to improve this so we don't destroy entire root
		wxDataViewItem r(root);
		model->ItemDeleted(p, r);
		model->ItemAdded(p, r);
		view->Expand(r);
	}

	void saveFile()
	{
		if(type != TabType::OPENED_FILE) return;

		int commitResult = CommitEdits();
		if(commitResult > 0)
			editor->SetActiveNode(nullptr);

		if(fileUpToDate) return; // Need to check this when commitResult <= 0

		parser->writeToFile(std::string(filePath), compressOnSave);

		if(commitResult < 0)
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
	int CommitEdits()
	{
		if (!editor->Modified()) return 0;

		EntNode* replacing = editor->Node();
		EntNode* parent = replacing->getParent();
		EntNode* firstNewChild;
		int newChildCount;

		if (!parser->parseAndReplace(
			string(editor->GetText()),
			replacing, 1, firstNewChild, newChildCount))
		{
			editor->setAnnotationError(
				parser->getParsedLineCount(),
				parser->getParseFailError().what()
			);
			return -1;
		}

		// Refresh the model
		model->ItemDeleted(wxDataViewItem(parent), wxDataViewItem(replacing));
		if (newChildCount > 0)
		{
			int firstNewIndex = parent->getChildIndex(firstNewChild);
			wxDataViewItemArray newItems;
			for (int i = 0; i < newChildCount; i++)
				newItems.Add(wxDataViewItem((*parent)[firstNewIndex + i]));
			model->ItemsAdded(wxDataViewItem(parent), newItems);
		}

		fileUpToDate = false;
		return 1;
	}

	void onNodeSelection(wxDataViewEvent& event)
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

	void onNodeActivation(wxDataViewEvent& event)
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

	void onFilterMenuShowHide(wxCollapsiblePaneEvent& event)
	{
		Freeze(); // Freezing seems to fix visual bugs that may occur when expanding
		Layout(); // for the first time
		Thaw();
	}

	private:
	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(EntityBookTab, wxPanel)
	EVT_COLLAPSIBLEPANE_CHANGED(wxID_ANY, onFilterMenuShowHide)

	EVT_TEXT_ENTER(INPUT_KEYFILTER, onAddKeyFilter)
	EVT_BUTTON(BTN_APPLYFILTERS, onApplyFilters)

	EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, onNodeSelection)
	EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, onNodeActivation)
wxEND_EVENT_TABLE()