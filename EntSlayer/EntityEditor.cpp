#include "EntityEditor.h"

wxBEGIN_EVENT_TABLE(EntityEditor, wxStyledTextCtrl)
    // common
    EVT_SIZE(EntityEditor::OnSize)
    // stc
    EVT_STC_MARGINCLICK(wxID_ANY, EntityEditor::OnMarginClick)
    EVT_STC_CHARADDED(wxID_ANY, EntityEditor::OnCharAdded)

    EVT_KEY_DOWN(EntityEditor::OnKeyDown)
wxEND_EVENT_TABLE()