#include <wx/wx.h>
#include <wx/treelist.h>

class EntityFolderDialog : public wxDialog 
{
	public:
	wxTreeListCtrl* treelist = nullptr;

	EntityFolderDialog(const wxString& directory, const wxArrayString& files);

	void onTreeItemChecked(wxTreeListEvent& event);

	void onTreeItemClicked(wxTreeListEvent& event);

	void GetCheckedPaths(wxArrayString& paths);
};