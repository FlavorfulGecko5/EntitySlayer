#include "wx/wx.h"
#include "wx/stc/stc.h"  // styled text control
#include <string>

class EntNode;
struct LanguageInfo;
class EntityEditor: public wxStyledTextCtrl 
{
    private:
    EntNode* activeNode = nullptr;
    wxString originalText = "";

    bool recursiveGuard = false; // Prevent recursive loops when adding characters programatically

    public:
    /* Construction and Preference Setting */

    EntityEditor(wxWindow* parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style =
        #ifndef __WXMAC__
        wxSUNKEN_BORDER |
        #endif
        wxVSCROLL
    );

    void ReloadPreferences();

    /* Events */

    void OnMarginClick(wxStyledTextEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnCharAdded(wxStyledTextEvent& event);

    /* Editing-Related Items */

    void SetActiveNode(EntNode* node);
    EntNode* Node();
    bool Modified();
    void MarkUnmodified();
    void RevertEdits();
    void setAnnotationError(const size_t lineNumber, const std::string& errorMessage);

    private:
    wxDECLARE_EVENT_TABLE();
};
