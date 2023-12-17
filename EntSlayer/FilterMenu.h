#include "wx/wx.h"
#include "wx/checklst.h"
#include "wx/combo.h"

class FilterList : public wxCheckListBox
{
	public:
	wxSizer* container;
	wxStaticText* label;

	FilterList(wxWindow* parent, const wxString& labelText) : wxCheckListBox(parent, wxID_ANY, wxDefaultPosition,
		wxSize(150, -1), 0, NULL, wxLB_NEEDED_SB | wxLB_SORT | wxLB_HSCROLL)
	{
		container = new wxBoxSizer(wxVERTICAL);
		label = new wxStaticText(parent, wxID_ANY, labelText);
		container->Add(label);
		container->Add(this, 1, wxEXPAND);
	}

	void leftClick(wxCommandEvent& event)
	{
		int index = event.GetSelection();
		Check(index, !IsChecked(index));
		DeselectAll();
	}

	private:
	wxDECLARE_EVENT_TABLE();
	
};
wxBEGIN_EVENT_TABLE(FilterList, wxCheckListBox)
	EVT_LISTBOX(wxID_ANY, FilterList::leftClick)
wxEND_EVENT_TABLE()


 //This works now - consider whether or not to replace existing filter boxes
/*class FilterChecklistBox : public wxCheckListBox, public wxComboPopup
{
	public:
	//FilterChecklistBox(wxWindow* parent) : wxCheckListBox(parent, wxID_ANY) {}

	virtual bool Create(wxWindow* parent) 
	{
		return wxCheckListBox::Create(parent, 1, wxPoint(0, 0), wxDefaultSize,
			0, NULL, wxLB_MULTIPLE);
	}

	virtual wxWindow* GetControl()
	{ 
		return this;
	}

	virtual void SetStringValue() const {}

	virtual wxString GetStringValue() const 
	{
		return "UNUSED?";
	}

	virtual wxSize GetAdjustedSize(int minWidth, int prefHeight, int maxHeight)
	{
		return wxSize(-1, -1);
	}

	void onClick(wxMouseEvent& event)
	{
		int index = HitTest(event.GetPosition());
		wxLogMessage("Index %i", index);
	}

	private:
	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(FilterChecklistBox, wxCheckListBox)
	EVT_LEFT_DOWN(FilterChecklistBox::onClick)
wxEND_EVENT_TABLE()*/