#include "wx/wx.h"
#include "wx/checklst.h"
#include "wx/combo.h"

class FilterListCombo : public wxCheckListBox, public wxComboPopup
{
	public:
	virtual bool Create(wxWindow* parent) 
	{
		return wxCheckListBox::Create(parent, 1, wxDefaultPosition, wxDefaultSize,
			0, NULL, wxLB_HSCROLL | wxLB_NEEDED_SB | wxLB_SORT);
	}

	virtual wxWindow* GetControl()
	{ 
		return this;
	}

	virtual wxString GetStringValue() const 
	{
		wxString value;
		for(int i = 0, max = GetCount(); i < max; i++)
			if (IsChecked(i))
			{
				value.Append(GetString(i));
				value.Append(',');
			}
				
		return value;
	}

	virtual wxSize GetAdjustedSize(int minWidth, int prefHeight, int maxHeight)
	{
		return wxSize(minWidth, prefHeight);
	}
};

class FilterCtrl : public wxComboCtrl
{
	public:
	FilterListCombo* list;
	wxBoxSizer* container;
	wxTextCtrl* quickInput;
	bool quickInputCreatesEntries;

	FilterCtrl(wxWindow* parent, const wxString& labelText, bool isTextKeyBox) : wxComboCtrl(parent, wxID_ANY, "", wxDefaultPosition, 
		wxSize(150, -1), wxCB_READONLY), quickInputCreatesEntries(isTextKeyBox)
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

	void refreshAutocomplete()
	{
		quickInput->AutoComplete(list->GetStrings());
	}

	void onListLeftDown(wxMouseEvent& event)
	{
		int index = list->HitTest(event.GetPosition());
		if (index > -1)
		{
			list->Check(index, !list->IsChecked(index));
			SetText(list->GetStringValue());
		}
	}

	void onQuickInputEnter(wxCommandEvent& event)
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
};