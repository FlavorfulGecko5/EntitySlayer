#include "EntityFolderDialog.h"
#include "wx/log.h"
#include <map>

EntityFolderDialog::EntityFolderDialog(const wxString& directory, const wxArrayString& files) : wxDialog(nullptr, wxID_ANY, directory)
{
	/* Confirm and Cancel buttons */
	wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->Add(new wxButton(this, wxID_OK, "Confirm"));
	buttons->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxLEFT, 5);

	/* Initialize Tree List */
	// Something is really screwed up with tree list control sizing
	treelist = new wxTreeListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(900, 400), wxTL_SINGLE | wxTL_CHECKBOX | wxTL_NO_HEADER);
	treelist->AppendColumn("File Path", 900, wxALIGN_LEFT, wxCOL_RESIZABLE);
	treelist->Bind(wxEVT_TREELIST_ITEM_CHECKED, &EntityFolderDialog::onTreeItemChecked, this);
	treelist->Bind(wxEVT_TREELIST_SELECTION_CHANGED, &EntityFolderDialog::onTreeItemClicked, this);


	/* Populate Tree List */
	std::unordered_map<wxString, wxTreeListItem> extensionMap;
	const wxString NO_EXT("No Extension");

	for (const wxString& path : files) {
		
		wxTreeListItem extItem;

		size_t extIndex = path.find_last_of('.');
		wxString ext = extIndex == wxString::npos ? NO_EXT : path.substr(extIndex);

		auto iter = extensionMap.find(ext);
		if (iter == extensionMap.end()) {
			extItem = treelist->AppendItem(treelist->GetRootItem(), ext);
			extensionMap.emplace(ext, extItem);
		}
		else {
			extItem = iter->second;
		}

		treelist->AppendItem(extItem, path);
	}


	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	mainSizer->Add(treelist, 0, wxEXPAND);
	mainSizer->Add(buttons, 0, wxCENTER | wxTOP | wxBOTTOM, 5);
	SetSizerAndFit(mainSizer);
	CenterOnScreen();

	ShowModal();
	Destroy();
}

void EntityFolderDialog::GetCheckedPaths(wxArrayString& paths) {
	wxTreeListItem item = treelist->GetRootItem();

	while (1) {
		item = treelist->GetNextItem(item);
		if(!item.IsOk())
			break;

		if(treelist->GetCheckedState(item) != wxCHK_CHECKED)
			continue;

		/* Path String items will not have children */
		if(treelist->GetFirstChild(item).IsOk())
			continue;

		paths.Add(treelist->GetItemText(item));
	}

}

void EntityFolderDialog::onTreeItemClicked(wxTreeListEvent& event) {
	wxTreeListItem node = event.GetItem();
	wxCheckBoxState state = static_cast<wxCheckBoxState>(treelist->GetCheckedState(node) == 1 ? 0 : 1);
	treelist->CheckItemRecursively(node, state);
	treelist->UnselectAll();
}

void EntityFolderDialog::onTreeItemChecked(wxTreeListEvent& event) {
	/*
	* When the checkbox is clicked, a selection change event is fired, then an item checked change is fired.
	* This results in the item being checked, then immediately unchecked or vice versa. This line corrects this issue.
	*/
	treelist->CheckItem(event.GetItem(), event.GetOldCheckedState());
}