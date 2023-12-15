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
	BTN_APPLYFILTERS
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
	wxTextCtrl* xInput;
	wxTextCtrl* yInput;
	wxTextCtrl* zInput;
	wxTextCtrl* rInput;


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
			wxFont font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false);

			wxBoxSizer* checklistSizer = new wxBoxSizer(wxHORIZONTAL);
			layerMenu = new FilterList(topWindow, "Layers");
			classMenu = new FilterList(topWindow, "Classes");
			inheritMenu = new FilterList(topWindow, "Inherits");
			checklistSizer->Add(layerMenu->container, 1, wxLEFT | wxRIGHT, 10);
			checklistSizer->Add(classMenu->container, 1, wxLEFT | wxRIGHT, 10);
			checklistSizer->Add(inheritMenu->container, 1, wxLEFT | wxRIGHT, 10);
			model->refreshFilterMenus(layerMenu, classMenu, inheritMenu);

			wxBoxSizer* spawnFilterSizer = new wxBoxSizer(wxVERTICAL);
			{
				wxStaticText* label = new wxStaticText(topWindow, wxID_ANY, "Spawn Position");
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

				spawnFilterSizer->Add(label);
				spawnFilterSizer->Add(xSizer);
				spawnFilterSizer->Add(ySizer);
				spawnFilterSizer->Add(zSizer);
				spawnFilterSizer->Add(rSizer);
			}


			
			wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
			topSizer->Add(checklistSizer, 0, wxEXPAND);
			topSizer->Add(spawnFilterSizer, 0, wxLEFT | wxRIGHT, 10);
			topSizer->Add(new wxButton(topWindow, TabID::BTN_APPLYFILTERS, "Apply"), 0, wxALL, 10);
			topWindow->SetSizerAndFit(topSizer);
		}
		
		/* Initialize controls */
		wxSplitterWindow* splitter = new wxSplitterWindow(this);
		editor = new EntityEditor(splitter, wxID_ANY);
		view = new wxDataViewCtrl(splitter, wxID_ANY);

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

	void onApplyFilters(wxCommandEvent& event) 
	{
		/*
		* Should be 100% safe to apply filters without committing
		* and making nullptr the node we're editing - in spite of a warning message
		* that appears in debug builds when you commit a root child that was
		* filtered out of the view.
		*/
		float newX = 0, newY = 0, newZ = 0, newR = 0;
		bool newSpawnFilterSetting = true;

		wxString stringX = xInput->GetValue(),
			stringY = yInput->GetValue(),
			stringZ = zInput->GetValue(),
			stringR = rInput->GetValue();
		try {
			newX = stof(string(stringX));
			newY = stof(string(stringY));
			newZ = stof(string(stringZ));
			newR = stof(string(stringR));
		}
		catch (exception) {
			newSpawnFilterSetting = false;
		}

		model->SetFilters(layerMenu, classMenu, inheritMenu,
			newSpawnFilterSetting, newX, newY, newZ, newR);
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

	EVT_BUTTON(BTN_APPLYFILTERS, onApplyFilters)

	EVT_DATAVIEW_SELECTION_CHANGED(wxID_ANY, onNodeSelection)
	EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, onNodeActivation)
wxEND_EVENT_TABLE()