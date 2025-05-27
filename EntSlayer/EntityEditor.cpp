#pragma warning(disable : 4996) // Deprecation errors
#include "EntityNode.h"
#include "EntityEditor.h"
#include "Config.h"
#include "EntityProfiler.h"

wxBEGIN_EVENT_TABLE(EntityEditor, wxStyledTextCtrl)
    // stc
    EVT_STC_MARGINCLICK(wxID_ANY, EntityEditor::OnMarginClick)
    EVT_STC_CHARADDED(wxID_ANY, EntityEditor::OnCharAdded)

    EVT_KEY_DOWN(EntityEditor::OnKeyDown)
wxEND_EVENT_TABLE()

#define MarginLines 0
#define MarginFolds 1
#define AnnotationStyle wxSTC_STYLE_LASTPREDEFINED + 1

void InitColorPrefs(wxStyledTextCtrl* e) {
    //wxColor test = wxColour(e->GetMarginHi);
    //wxLogMessage("%u %u %u %u", test.Red(), test.Green(), test.Blue(), test.Alpha());

    bool nightMode = ConfigInterface::NightMode();

    /*
    * Light Mode Colors
    */
    const wxColour AnnotationRed = wxColour(244, 220, 220);
    const wxColour Black = wxColour(0, 0, 0);
    const wxColour Blue = wxColour(0, 0, 255);
    const wxColour Brown = wxColour(165, 42, 42);
    const wxColour DarkGrey = wxColour(47, 47, 47);
    const wxColour ForestGreen = wxColour(35, 142, 35);
    const wxColour Grey = wxColour(128, 128, 128);
    const wxColour Khaki = wxColour(159, 159, 95);
    const wxColour Orchid = wxColour(219, 112, 219);
    const wxColour Red = wxColour(255, 0, 0);
    const wxColour Sienna = wxColour(142, 107, 35);
    const wxColour White = wxColour(255, 255, 255);
    const wxColour MarginWhite = wxColour(240, 240, 240);

    /*
    * Dark Mode Colors
    */
    const wxColour NightBlack = wxColour(42, 42, 42);
    const wxColour NightWhite = wxColour(219, 219, 219);
    const wxColour NightString = wxColour(172, 76, 31);
    const wxColour NightOrange = wxColour(255, 106, 0);
    const wxColour NightForest = wxColour(90, 150, 85);

    struct StyleDef_t {
        wxColour foreground;
        wxColour nightForeground;
        bool bold = false;
    };

    std::unordered_map<int, StyleDef_t> StyleMap = {
        {wxSTC_C_DEFAULT           , {Black, NightWhite}},
        {wxSTC_C_COMMENT           , {ForestGreen, NightForest}},
        {wxSTC_C_COMMENTLINE       , {ForestGreen, NightForest}},
        {wxSTC_C_COMMENTDOC        , {ForestGreen, NightForest}},
        {wxSTC_C_NUMBER            , {Sienna, NightOrange}},
        {wxSTC_C_WORD              , {Blue, NightOrange, true}},
        {wxSTC_C_STRING            , {Brown, NightOrange}},
      //{wxSTC_C_CHARACTER         , {Khaki}},
      //{wxSTC_C_UUID              , {Orchid}},
      //{wxSTC_C_PREPROCESSOR      , {Grey}},
        {wxSTC_C_OPERATOR          , {Black, NightWhite, true}},
        {wxSTC_C_IDENTIFIER        , {Black, NightWhite}},
        {wxSTC_C_STRINGEOL         , {Brown, NightOrange}}, // String with no end-quote
        {wxSTC_C_VERBATIM          , {Black, NightWhite}},
      //{wxSTC_C_REGEX             , {Orchid}},
        {wxSTC_C_COMMENTLINEDOC    , {ForestGreen, NightForest}},
      //{wxSTC_C_WORD2             , {}},
        {wxSTC_C_COMMENTDOCKEYWORD , {ForestGreen, NightForest}},
        {wxSTC_C_COMMENTDOCKEYWORDERROR, {ForestGreen, NightForest}}
    };


    for (int i = 0; i < wxSTC_STYLE_DEFAULT; i++) {
        auto iterator = StyleMap.find(i);

        if (iterator != StyleMap.end()) {
            const StyleDef_t& style = iterator->second;
            e->StyleSetForeground(i, nightMode ? style.nightForeground : style.foreground);
            e->StyleSetBold(i, style.bold);
        }
        else {
            e->StyleSetForeground(i, nightMode ? NightWhite : Black);
        }
        e->StyleSetBackground(i, nightMode ? NightBlack : White);
    }


    // Symbols used in the folding margin
    wxColour markerBlack = nightMode ? NightWhite : Black;
    wxColour markerWhite = nightMode ? NightWhite : White;
    e->MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_DOTDOTDOT, markerBlack, markerBlack);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_ARROWDOWN, markerBlack, markerWhite); // Top-level Folders
    e->MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_EMPTY, markerBlack, markerBlack);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_DOTDOTDOT, markerBlack, markerWhite);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_ARROWDOWN, markerBlack, markerWhite);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_EMPTY, markerBlack, markerBlack);
    e->MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_EMPTY, markerBlack, markerBlack);

    // Line Number Margin
    e->StyleSetForeground(wxSTC_STYLE_LINENUMBER, nightMode ? NightWhite : DarkGrey);
    e->StyleSetBackground(wxSTC_STYLE_LINENUMBER, nightMode ? NightBlack : White);

    // Margin Color
    e->SetFoldMarginColour(true, nightMode ? NightBlack : White);
    e->SetFoldMarginHiColour(true, nightMode ? NightBlack : MarginWhite);

    // Vertical indentation dots
    e->StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, nightMode ? NightWhite : DarkGrey);
    e->StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, nightMode ? NightBlack : White);

    // Annotations
    e->StyleSetBackground(AnnotationStyle, AnnotationRed);
    e->StyleSetForeground(AnnotationStyle, Black);

    // Default Style - Probably not used anywhere?
    e->StyleSetForeground(wxSTC_STYLE_DEFAULT, nightMode ? NightWhite : Black);
    e->StyleSetBackground(wxSTC_STYLE_DEFAULT, nightMode ? NightBlack : White);
}

void InitPrefs(wxStyledTextCtrl* e) {
    EditorConfig_t cfg = ConfigInterface::GetEditorConfig();

    // Set Lexer (Must set this before Folding properties are set)
    e->StyleClearAll();
    e->SetLexer(wxSTC_LEX_CPP);

    // Set Fonts (Must do this before applying bold/italic/etc. properties)
    // Annotations don't word wrap so we should keep it small
    e->StyleSetFont(AnnotationStyle, wxFont(wxFontInfo(cfg.fontSize - 2).Family(wxFONTFAMILY_MODERN)));
    for (int i = 0; i < wxSTC_STYLE_LASTPREDEFINED; i++) {
        wxFont font(wxFontInfo(cfg.fontSize).Family(wxFONTFAMILY_MODERN));
        e->StyleSetFont(i, font);
    }
    e->StyleSetFont(wxSTC_STYLE_LINENUMBER, wxFont(wxFontInfo(10).Family(wxFONTFAMILY_MODERN)));

    // Set Keywords the lexers use with specific styles
    e->SetKeyWords(0, "bool char class double enum false float int long new short signed struct true unsigned ");

    // Color Preferences
    InitColorPrefs(e);

    // Line Number Margin
    e->SetMarginType(MarginLines, wxSTC_MARGIN_NUMBER); // enables line numbers
    e->SetMarginMask(MarginLines, 0);
    e->SetMarginWidth(MarginLines, 40); // Wide enough for 4 characters


    // Indentation Rules
    e->SetIndent(4);
    e->SetTabWidth(4);
    e->SetUseTabs(true); // If false convert tabs to spaces
    e->SetTabIndents(true);
    e->SetBackSpaceUnIndents(true);

    // Folding Margin
    e->SetMarginType(MarginFolds, wxSTC_MARGIN_SYMBOL);
    e->SetMarginMask(MarginFolds, wxSTC_MASK_FOLDERS);
    e->SetMarginWidth(MarginFolds, e->FromDIP(16));
    e->SetMarginSensitive(MarginFolds, true);
    e->SetProperty("fold", "1");
    e->SetProperty("fold.comment", "1");
    e->SetProperty("fold.compact", "1");
    e->SetProperty("fold.preprocessor", "1");
    e->SetFoldFlags(wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED | wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);


    // Annotations (Used to display parsing errors)
    e->AnnotationSetVisible(wxSTC_ANNOTATION_STANDARD);
    

    // Whitespace Visibility
    e->SetIndentationGuides(wxSTC_IV_REAL);       // Vertical Dots Going down brace groups. Replacing tab arrows
    e->SetViewEOL(false);                         // View CRLF characters
    e->SetViewWhiteSpace(wxSTC_WS_INVISIBLE);     // Tab and Space Visibility
    e->SetTabDrawMode(wxSTC_TD_LONGARROW);        // How Tab is drawn


    // Line Wrap - May be toggled on/off elsewhere in the file
    e->SetWrapMode(wxSTC_WRAP_WORD);
    e->SetWrapIndentMode(wxSTC_WRAPINDENT_SAME);
    e->SetUseHorizontalScrollBar(false);


    // Unimportant Options
    e->SetOvertype(false); // Insertion Mode
    e->SetEdgeMode(wxSTC_EDGE_NONE);
    e->SetEdgeColumn(80); // This + Above toggle = vertical line at column


    // Visibility Policies - Haven't touched these, probably best not to
    e->SetVisiblePolicy(wxSTC_VISIBLE_STRICT | wxSTC_VISIBLE_SLOP, 1);
    e->SetXCaretPolicy(wxSTC_CARET_EVEN | wxSTC_VISIBLE_STRICT | wxSTC_CARET_SLOP, 1);
    e->SetYCaretPolicy(wxSTC_CARET_EVEN | wxSTC_VISIBLE_STRICT | wxSTC_CARET_SLOP, 1);

    // More policies that it's probably best not to touch
    e->CmdKeyClear(wxSTC_KEY_TAB, 0); // This is done by the menu accelerator key
    e->SetLayoutCache(wxSTC_CACHE_PAGE);
    e->UsePopUp(wxSTC_POPUP_ALL); // Disables the context menu if disabled

    // Controlled Elsewhere in the Editor
    //e->SetReadOnly(editorConfig.readOnlyInitial);
    
}

EntityEditor::EntityEditor(wxWindow* parent, wxWindowID id, const wxPoint& pos,
    const wxSize& size, long style)
    : wxStyledTextCtrl(parent, id, pos, size, style)
{
    
    InitPrefs(this);

    // Set Initial Node
    SetActiveNode(nullptr);
}

void EntityEditor::ReloadPreferences() {
    InitPrefs(this);
}

void EntityEditor::OnMarginClick(wxStyledTextEvent& event)
{
    if (event.GetMargin() == MarginFolds) {
        int lineClick = LineFromPosition(event.GetPosition());
        int levelClick = GetFoldLevel(lineClick);
        if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
            ToggleFold(lineClick);
        }
    }
}

void EntityEditor::OnKeyDown(wxKeyEvent& event)
{
    if (event.GetKeyCode() == WXK_TAB) {
        if (event.ShiftDown()) BackTab();
        else Tab();
        return;
    }
    
    event.Skip();
}

void EntityEditor::OnCharAdded(wxStyledTextEvent& event)
{
    char chr = (char)event.GetKey();

    if (chr == '\n') {
        int currentLine = GetCurrentLine();

        int lastInd = GetLineIndentation(currentLine - 1);
        int nextInd = GetLineIndentation(currentLine + 1);
        int indent = nextInd > lastInd ? nextInd : lastInd;

        SetLineIndentation(currentLine, indent);
        GotoPos(GetLineIndentPosition(currentLine));

        int position = GetCurrentPos();
        if (GetCharAt(position) == '}' && !recursiveGuard) 
        {   
            // NewLine() triggers a char added event, hence we need a recursive guard
            recursiveGuard = true;
            Tab();
            position = GetCurrentPos();
            NewLine();
            GotoPos(position);
            SetLineIndentation(currentLine + 1, indent);
            recursiveGuard = false;
        }
    }
    else if (chr == '{') {
        InsertText(GetCurrentPos(), '}');
    }
    else if (chr == '[') {
        InsertText(GetCurrentPos(), ']');
    }
    else if (chr == '"') {
        wxString line = GetLineText(GetCurrentLine());

        int numQuotes = 0;
        for(char c : line)
            if(c == '"') 
                numQuotes++;

        if(numQuotes == 1)
            InsertText(GetCurrentPos(), "\"");
    }


    /*else if (chr == '#') {
        wxString s = "define?0 elif?0 else?0 endif?0 error?0 if?0 ifdef?0 "
                     "ifndef?0 include?0 line?0 pragma?0 undef?0";
        AutoCompShow(0,s);
    }*/
}

void EntityEditor::SetActiveNode(EntNode* node)
{
    //wxLogMessage("Changing text editor's node");

    /* 
    * Line Wrap causes noticeable lag when loading large texts
    * like idAI2 entities - we can mitigate this problem
    * by disabling line wrap until the node's text has been loaded in
    */
    SetWrapMode(wxSTC_WRAP_NONE);

    if (node == nullptr || node->IsRoot())
    {
        SetText("Select a node to begin editing.");
        SetReadOnly(true);
    }
    else {
        SetReadOnly(false);
        SetText(node->toString());
    }
    SetWrapMode(wxSTC_WRAP_WORD);
    activeNode = node;
    AnnotationClearAll();
    EmptyUndoBuffer();
    MarkUnmodified();
}

EntNode* EntityEditor::Node()
{
    return activeNode;
}

bool EntityEditor::Modified()
{
    // return modified state
    return (GetModify() && !GetReadOnly());
}

void EntityEditor::MarkUnmodified()
{
    originalText = GetValue();
    SetModified(false);
}

void EntityEditor::RevertEdits()
{
    AnnotationClearAll();
    SetText(originalText);
    SetModified(false);
}

void EntityEditor::setAnnotationError(const size_t lineNumber, const std::string& errorMessage)
{
    AnnotationClearAll();
    AnnotationSetStyle(lineNumber - 1, AnnotationStyle);
    AnnotationSetText(lineNumber - 1, errorMessage);
    GotoLine(lineNumber - 1);
}