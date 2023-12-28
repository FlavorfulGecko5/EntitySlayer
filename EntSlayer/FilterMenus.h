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

class EntityTab;
class FilterCtrl : public wxComboCtrl
{
	public:
	EntityTab* owner;
	FilterListCombo* list;
	wxBoxSizer* container;
	wxTextCtrl* quickInput;
	bool quickInputCreatesEntries;

	FilterCtrl(EntityTab* tab, wxWindow* parent, const wxString& labelText, bool isTextKeyBox);
	void refreshAutocomplete();
	void onListLeftDown(wxMouseEvent& event);
	void onQuickInputEnter(wxCommandEvent& event);
};