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

void FilterCtrl::setItems(const std::set<std::string_view>& newItems)
{
	wxArrayString newStrings;
	newStrings.reserve(newItems.size() + 5);
	for (std::string_view view : newItems)
		newStrings.push_back(wxString(view.data(), view.length()));

	// Need to ensure filters are carried over properly
	// When we use Set all items will be unchecked
	wxArrayInt oldChecked;
	wxArrayString oldCheckedStrings;
	list->GetCheckedItems(oldChecked);
	oldCheckedStrings.reserve(oldChecked.size());
	for (int i = 0, max = oldChecked.size(); i < max; i++) {
		oldCheckedStrings.push_back(list->GetString(oldChecked[i]));
	}

	// Setting the list comes with a huge runtime cost
	// Freezing it during the set further reduces this function's runtime by ~75%
	list->Freeze();
	list->Set(newStrings);

	for (wxString& old : oldCheckedStrings) {
		int location = list->FindString(old, true);

		// We don't want to change the filters when refreshing, so we ensure any checked items
		// remain in the array
		if (location < 0) {
			location = list->Append(old);
		}
		list->Check(location, true);
	}
	list->Thaw();

	// Seems to have it's own performance penalty on initial file opening 
	// - as high as 20 milliseconds across all filters
	refreshAutocomplete();

	// Original implementation - new implementation has 90% reduction in runtime in release builds, 
	// several hundred % reduction in debug
	//for (std::string_view view : newItems)
	//{
	//	wxString s(view.data(), view.length());
	//	if (list->FindString(s, true) < 0)
	//		list->Append(s);
	//}
	//refreshAutocomplete();
}

void FilterCtrl::deleteUnchecked()
{
	size_t i = 0;
	while(i < list->GetCount())
		if(!list->IsChecked(i))
			list->Delete(i);
		else i++;
	refreshAutocomplete();
}

void FilterCtrl::uncheckAll()
{
	for(size_t i = 0, max = list->GetCount(); i < max; i++)
		if(list->IsChecked(i))
			list->Check(i, false);
	SetText(list->GetStringValue());
}

void FilterCtrl::onListLeftDown(wxMouseEvent& event)
{
	int index = list->HitTest(event.GetPosition());
	if (index > -1)
	{
		list->Check(index, !list->IsChecked(index));
		owner->applyFilters(false);
		SetText(list->GetStringValue());
	}
}

void FilterCtrl::onQuickInputEnter(wxCommandEvent& event)
{
	wxString query = quickInput->GetValue();
	int index = list->FindString(query, quickInputCreatesEntries); // Only do case-sensitive matching for the text filter

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
	quickInput->Clear();

	list->Check(index, !list->IsChecked(index));
	owner->applyFilters(false);

	SetText(list->GetStringValue());
}

SpawnFilter::SpawnFilter(EntityTab* tab, wxWindow* parent)
	: wxBoxSizer(wxVERTICAL), owner(tab)
{
	toggle = new wxCheckBox(parent, wxID_ANY, "Spawn Position Distance");
	toggle->Bind(wxEVT_CHECKBOX, &SpawnFilter::onCheckbox, this);
	Add(toggle, 0, wxBOTTOM, 2);

	const wxString names[4] = {"x", "y", "z", "r"};
	for (int i = 0; i < 4; i++)
	{
		wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
		wxStaticText* label = new wxStaticText(parent, wxID_ANY, names[i], wxDefaultPosition, wxSize(8, -1));
		inputs[i] = new wxTextCtrl(parent, wxID_ANY);
		inputs[i]->Bind(wxEVT_TEXT, &SpawnFilter::onInputText, this);

		row->Add(label, 0, wxLEFT | wxRIGHT, 5);
		row->Add(inputs[i], 0, wxLEFT | wxRIGHT, 5);
		Add(row);
	}
}

bool SpawnFilter::activated() 
{
	return toggle->IsChecked();
}

void SpawnFilter::deactivate()
{
	toggle->SetValue(false);
}

bool SpawnFilter::getData(Sphere& sphere) 
{
	wxString stringX = inputs[0]->GetValue(),
		stringY = inputs[1]->GetValue(),
		stringZ = inputs[2]->GetValue(),
		stringR = inputs[3]->GetValue();
	try { // Todo: Ensure this function is as thorough as needed
		sphere.x = stof(std::string(stringX));
		sphere.y = stof(std::string(stringY));
		sphere.z = stof(std::string(stringZ));
		sphere.r = stof(std::string(stringR));
	}
	catch (std::exception) {
		toggle->SetValue(false);
		wxMessageBox("Could not convert one or more fields to numbers", 
			"Spawn Position Filtering Failed", wxICON_WARNING | wxOK);
		return false;
	}
	return true;
}

void SpawnFilter::setData(const std::string& x, const std::string& y, const std::string& z)
{
	inputs[0]->SetValue(x);
	inputs[1]->SetValue(y);
	inputs[2]->SetValue(z);
	
	if(toggle->IsChecked())
		owner->applyFilters(false);
}

void SpawnFilter::onCheckbox(wxCommandEvent& event) {
	owner->applyFilters(false);
}

void SpawnFilter::onInputText(wxCommandEvent& event) {
	if (toggle->IsChecked())
		owner->applyFilters(false);
	event.Skip();
}

SearchBar::SearchBar(EntityTab* tab, wxWindow* parent) 
	: wxBoxSizer(wxVERTICAL), owner(tab)
{
	wxStaticText* label = new wxStaticText(parent, wxID_ANY, "Search");
	input = new wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
		wxDefaultSize, wxTE_PROCESS_ENTER);
	wxButton* btnNext = new wxButton(parent, wxID_ANY, ">", wxDefaultPosition, wxSize(23, 23));
	wxButton* btnBack = new wxButton(parent, wxID_ANY, "<", wxDefaultPosition, wxSize(23, 23));
	caseSensitiveCheck = new wxCheckBox(parent, wxID_ANY, "Case Sensitive");

	input->Bind(wxEVT_TEXT_ENTER, &SearchBar::onButtonNext, this);
	btnNext->Bind(wxEVT_BUTTON, &SearchBar::onButtonNext, this);
	btnBack->Bind(wxEVT_BUTTON, &SearchBar::onButtonBack, this);

	wxSizer* topRow = new wxBoxSizer(wxHORIZONTAL);
	topRow->Add(label, 0);
	topRow->Add(input, 1, wxEXPAND | wxLEFT, 5);
	topRow->Add(btnBack);
	topRow->Add(btnNext);
	Add(topRow, 1, wxEXPAND);
	Add(caseSensitiveCheck);
}

void SearchBar::initiateSearch(bool backwards) 
{
	const std::string key(input->GetValue());
	
	if(!key.empty())
		owner->Parser->FilteredSearch(key, backwards, caseSensitiveCheck->IsChecked(), false);
}

void SearchBar::onButtonNext(wxCommandEvent& event)
{
	initiateSearch(false);
}

void SearchBar::onButtonBack(wxCommandEvent& event)
{
	initiateSearch(true);
}