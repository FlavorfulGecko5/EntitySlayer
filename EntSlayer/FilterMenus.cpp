#include "FilterMenus.h"
#include "EntityTab.h"

FilterCtrl::FilterCtrl(EntityTab* tab, wxWindow* parent, const wxString& labelText, bool isTextKeyBox) : wxComboCtrl(parent, wxID_ANY, "", wxDefaultPosition,
	wxSize(150, -1), wxCB_READONLY), owner(tab), quickInputCreatesEntries(isTextKeyBox)
{
	list = new FilterListCombo;
	SetPopupControl(list);
	list->Bind(wxEVT_LEFT_DOWN, &FilterCtrl::onListLeftDown, this);

	wxStaticText* label = new wxStaticText(parent, wxID_ANY, labelText);
	quickInput = new wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
		wxDefaultSize, wxTE_PROCESS_ENTER);
	quickInput->Bind(wxEVT_TEXT_ENTER, &FilterCtrl::onQuickInputEnter, this);

	wxBoxSizer* topRow = new wxBoxSizer(wxHORIZONTAL);
	topRow->Add(label);
	topRow->Add(quickInput, 1, wxEXPAND | wxLEFT | wxDOWN, 5);

	container = new wxBoxSizer(wxVERTICAL);
	container->Add(topRow, 0, wxEXPAND);
	container->Add(this, 1, wxEXPAND);
}

void FilterCtrl::refreshAutocomplete()
{
	quickInput->AutoComplete(list->GetStrings());
}

void FilterCtrl::onListLeftDown(wxMouseEvent& event)
{
	int index = list->HitTest(event.GetPosition());
	if (index > -1)
	{
		list->Check(index, !list->IsChecked(index));
		SetText(list->GetStringValue());
	}
}

void FilterCtrl::onQuickInputEnter(wxCommandEvent& event)
{
	wxString query = quickInput->GetValue();
	wxArrayString matches = list->GetStrings();
	int index = matches.Index(query);

	if (index == -1) {
		if (!quickInputCreatesEntries)
		{
			quickInput->SetBackgroundColour(wxColour(244, 220, 220));
			quickInput->Refresh(); // Need refresh or it won't update until focus lost
			return;
		}

		index = list->Append(query);
		refreshAutocomplete();
	}

	quickInput->SetBackgroundColour(wxColour(255, 255, 255, 255));
	list->Check(index, !list->IsChecked(index));
	SetText(list->GetStringValue());
	quickInput->Clear();
}