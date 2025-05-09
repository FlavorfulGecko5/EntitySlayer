#include "wx/wx.h"
#include "wx/checklst.h"
#include "wx/combo.h"
#include <set>
#include <string_view>

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
struct Sphere;
class FilterCtrl : public wxComboCtrl
{
	public:
	wxStaticText* label;
	EntityTab* owner;
	FilterListCombo* list;
	wxBoxSizer* container;
	wxTextCtrl* quickInput;
	bool quickInputCreatesEntries;

	FilterCtrl(EntityTab* tab, wxWindow* parent, const wxString& labelText, bool isTextKeyBox);
	void refreshAutocomplete();
	void deleteUnchecked();
	void uncheckAll();
	void onListLeftDown(wxMouseEvent& event);
	void onQuickInputEnter(wxCommandEvent& event);
	void setItems(const std::set<std::string_view>& newItems);
};

class SpawnFilter : public wxBoxSizer 
{
	public:
	EntityTab* owner;
	wxCheckBox* toggle;
	wxTextCtrl* inputs[4];
	wxStaticText* labels[4];
	
	public:
	SpawnFilter(EntityTab* tab, wxWindow* parent);
	bool getData(Sphere& sphere);
	void setData(const std::string& x, const std::string& y, const std::string &z);
	bool activated();
	void deactivate();

	void onCheckbox(wxCommandEvent &event);
	void onInputText(wxCommandEvent &event);
};

class SearchBar : public wxBoxSizer
{
	public:
	wxStaticText* label;
	EntityTab* owner;
	wxTextCtrl* input;
	wxCheckBox* caseSensitiveCheck;

	public:
	SearchBar(EntityTab* tab, wxWindow* parent);
	void initiateSearch(bool backwards);
	void onButtonNext(wxCommandEvent& event);
	void onButtonBack(wxCommandEvent& event);
};