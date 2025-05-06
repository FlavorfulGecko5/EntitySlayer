#include "prefs.h"
#include "EntityNode.h"
#include "EntityEditor.h"

wxBEGIN_EVENT_TABLE(EntityEditor, wxStyledTextCtrl)
    // stc
    EVT_STC_MARGINCLICK(wxID_ANY, EntityEditor::OnMarginClick)
    EVT_STC_CHARADDED(wxID_ANY, EntityEditor::OnCharAdded)

    EVT_KEY_DOWN(EntityEditor::OnKeyDown)
wxEND_EVENT_TABLE()

struct EditorConfig_t {
    // editor functionality prefs
    bool syntaxEnable = true;
    bool foldEnable = true;
    bool indentEnable = true;
    // display defaults prefs
    bool readOnlyInitial = false;
    bool overTypeInitial = false;
    bool wrapModeInitial = false;
    bool displayEOLEnable = false;
    bool indentGuideEnable = true;
    bool lineNumberEnable = true;
    bool longLineOnEnable = false;
    bool whiteSpaceEnable = false;
} editorConfig;

bool EntityEditor::InitializePrefs(const wxString& name)
{
    // initialize styles
    StyleClearAll();
    LanguageInfo const* curInfo = NULL;

    // determine language
    bool found = false;
    int languageNr;
    for (languageNr = 0; languageNr < g_LanguagePrefsSize; languageNr++) {
        curInfo = &g_LanguagePrefs[languageNr];
        if (curInfo->name == name) {
            found = true;
            break;
        }
    }
    if (!found) return false;

    // set lexer and language
    SetLexer(curInfo->lexer);
    m_language = curInfo;


    // annotations style
    // Annotations don't word wrap so we should keep it small
    StyleSetBackground(ANNOTATION_STYLE, wxColour(244, 220, 220));
    StyleSetForeground(ANNOTATION_STYLE, *wxBLACK);
    StyleSetFont(ANNOTATION_STYLE, wxFont(wxFontInfo(8).Family(wxFONTFAMILY_MODERN)));

    // With this new setup, the annotation font needs to be manually set. 
    // Previously it was set implicitly by StyleClearAll making the default font set in the
    // constructor the new default for every font
    // default fonts for all styles!
    int Nr;
    for (Nr = 0; Nr < wxSTC_STYLE_LASTPREDEFINED; Nr++) {
        wxFont font(wxFontInfo(10).Family(wxFONTFAMILY_MODERN));
        StyleSetFont(Nr, font);
    }

    // initialize settings
    if (editorConfig.syntaxEnable) {
        int keywordnr = 0;
        for (Nr = 0; Nr < STYLE_TYPES_COUNT; Nr++) {
            if (curInfo->styles[Nr].type == -1) continue;
            const StyleInfo& curType = g_StylePrefs[curInfo->styles[Nr].type];
            wxFont font(wxFontInfo(curType.fontsize)
                .Family(wxFONTFAMILY_MODERN)
                .FaceName(curType.fontname));
            StyleSetFont(Nr, font);
            if (curType.foreground.length()) {
                StyleSetForeground(Nr, wxColour(curType.foreground));
            }
            if (curType.background.length()) {
                StyleSetBackground(Nr, wxColour(curType.background));
            }
            StyleSetBold(Nr, (curType.fontstyle & mySTC_STYLE_BOLD) > 0);
            StyleSetItalic(Nr, (curType.fontstyle & mySTC_STYLE_ITALIC) > 0);
            StyleSetUnderline(Nr, (curType.fontstyle & mySTC_STYLE_UNDERL) > 0);
            StyleSetVisible(Nr, (curType.fontstyle & mySTC_STYLE_HIDDEN) == 0);
            StyleSetCase(Nr, curType.lettercase);
            const char* pwords = curInfo->styles[Nr].words;
            if (pwords) {
                SetKeyWords(keywordnr, pwords);
                keywordnr += 1;
            }
        }
    }

    // folding
    SetMarginType(m_FoldingID, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(m_FoldingID, wxSTC_MASK_FOLDERS);
    SetMarginWidth(m_FoldingID, 0);
    SetMarginSensitive(m_FoldingID, false);
    if (editorConfig.foldEnable) {
        // Margin width was not set before this executed - inlined FromDIP to fix it
        SetMarginWidth(m_FoldingID, curInfo->folds != 0 ? FromDIP(16) : 0);
        SetMarginSensitive(m_FoldingID, curInfo->folds != 0);
        SetProperty("fold", curInfo->folds != 0 ? "1" : "0");
        SetProperty("fold.comment",
            (curInfo->folds & mySTC_FOLD_COMMENT) > 0 ? "1" : "0");
        SetProperty("fold.compact",
            (curInfo->folds & mySTC_FOLD_COMPACT) > 0 ? "1" : "0");
        SetProperty("fold.preprocessor",
            (curInfo->folds & mySTC_FOLD_PREPROC) > 0 ? "1" : "0");
        SetProperty("fold.html",
            (curInfo->folds & mySTC_FOLD_HTML) > 0 ? "1" : "0");
        SetProperty("fold.html.preprocessor",
            (curInfo->folds & mySTC_FOLD_HTMLPREP) > 0 ? "1" : "0");
        SetProperty("fold.comment.python",
            (curInfo->folds & mySTC_FOLD_COMMENTPY) > 0 ? "1" : "0");
        SetProperty("fold.quotes.python",
            (curInfo->folds & mySTC_FOLD_QUOTESPY) > 0 ? "1" : "0");
    }
    SetFoldFlags(wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED |
        wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);

    return true;
}



void InitConstantPrefs(wxStyledTextCtrl* e, int lineNumId) {
    // Line Numbers
    e->SetMarginType(lineNumId, wxSTC_MARGIN_NUMBER); // enables line numbers
    e->SetMarginMask(lineNumId, 0);
    e->SetMarginWidth(lineNumId, 40); // Wide enough for 4 characters


    // Indentation Rules
    e->SetIndent(4);
    e->SetTabWidth(4);
    e->SetUseTabs(true); // If false convert tabs to spaces
    e->SetTabIndents(true);
    e->SetBackSpaceUnIndents(true);


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
    e->SetEdgeMode(editorConfig.longLineOnEnable ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
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

void InitColorPrefs(wxStyledTextCtrl* e) {
    wxColor test = *wxBLACK;
    wxLogMessage("%u %u %u %u", test.Red(), test.Blue(), test.Green(), test.Alpha());

    const wxColour Black = wxColour(0, 0, 0);
    const wxColour DarkGrey = wxColour(47, 47, 47);
    const wxColour Red = wxColour(255, 0, 0);
    const wxColour White = wxColour(255, 255, 255);

    // Line Number Margin
    e->StyleSetForeground(wxSTC_STYLE_LINENUMBER, DarkGrey);
    e->StyleSetBackground(wxSTC_STYLE_LINENUMBER, White);

    // Vertical indentation dots
    e->StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, DarkGrey);
    e->StyleSetBackground(wxSTC_STYLE_INDENTGUIDE, White);

    // Default Style - Probably not used anywhere?
    e->StyleSetForeground(wxSTC_STYLE_DEFAULT, Black);
    e->StyleSetBackground(wxSTC_STYLE_DEFAULT, White);
}

EntityEditor::EntityEditor(wxWindow* parent, wxWindowID id, const wxPoint& pos,
    const wxSize& size, long style)
    : wxStyledTextCtrl(parent, id, pos, size, style)
{
    m_LineNrID = 0;
    m_FoldingID = 1;
    m_language = NULL;
    
    InitConstantPrefs(this, m_LineNrID);
    InitializePrefs("C++");
    InitColorPrefs(this); // Must call this here until clear all styles in above function is moved

    // markers
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_DOTDOTDOT, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_ARROWDOWN, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_EMPTY, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_DOTDOTDOT, "BLACK", "WHITE");
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_ARROWDOWN, "BLACK", "WHITE");
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_EMPTY, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_EMPTY, "BLACK", "BLACK");

    // Set Initial Node
    SetActiveNode(nullptr);
}

void EntityEditor::OnMarginClick(wxStyledTextEvent& event)
{
    if (event.GetMargin() == m_FoldingID) {
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
    AnnotationSetStyle(lineNumber - 1, ANNOTATION_STYLE);
    AnnotationSetText(lineNumber - 1, errorMessage);
    GotoLine(lineNumber - 1);
}