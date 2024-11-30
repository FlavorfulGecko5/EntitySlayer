#include "prefs.h"
#include "EntityNode.h"
#include "EntityEditor.h"

wxBEGIN_EVENT_TABLE(EntityEditor, wxStyledTextCtrl)
    // stc
    EVT_STC_MARGINCLICK(wxID_ANY, EntityEditor::OnMarginClick)
    EVT_STC_CHARADDED(wxID_ANY, EntityEditor::OnCharAdded)

    EVT_KEY_DOWN(EntityEditor::OnKeyDown)
wxEND_EVENT_TABLE()

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

    // set margin for line numbers
    SetMarginType(m_LineNrID, wxSTC_MARGIN_NUMBER);
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour("DARK GREY"));
    StyleSetBackground(wxSTC_STYLE_LINENUMBER, *wxWHITE);
    SetMarginWidth(m_LineNrID, 0); // start out not visible

    // annotations style
    StyleSetBackground(ANNOTATION_STYLE, wxColour(244, 220, 220));
    StyleSetForeground(ANNOTATION_STYLE, *wxBLACK);
    StyleSetSizeFractional(ANNOTATION_STYLE,
        (StyleGetSizeFractional(wxSTC_STYLE_DEFAULT) * 4) / 5);

    // default fonts for all styles!
    int Nr;
    for (Nr = 0; Nr < wxSTC_STYLE_LASTPREDEFINED; Nr++) {
        wxFont font(wxFontInfo(10).Family(wxFONTFAMILY_MODERN));
        StyleSetFont(Nr, font);
    }

    // set common styles
    StyleSetForeground(wxSTC_STYLE_DEFAULT, wxColour("DARK GREY"));
    StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, wxColour("DARK GREY"));

    // initialize settings
    if (g_CommonPrefs.syntaxEnable) {
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

    // set margin as unused
    SetMarginType(m_DividerID, wxSTC_MARGIN_SYMBOL);
    SetMarginWidth(m_DividerID, 0);
    SetMarginSensitive(m_DividerID, false);

    // folding
    SetMarginType(m_FoldingID, wxSTC_MARGIN_SYMBOL);
    SetMarginMask(m_FoldingID, wxSTC_MASK_FOLDERS);
    StyleSetBackground(m_FoldingID, *wxWHITE);
    SetMarginWidth(m_FoldingID, 0);
    SetMarginSensitive(m_FoldingID, false);
    if (g_CommonPrefs.foldEnable) {
        SetMarginWidth(m_FoldingID, curInfo->folds != 0 ? m_FoldingMargin : 0);
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

    // set spaces and indentation
    SetTabWidth(4);
    SetUseTabs(true);
    SetTabIndents(true);
    SetBackSpaceUnIndents(true);
    SetIndent(g_CommonPrefs.indentEnable ? 4 : 0);

    // others
    SetViewEOL(g_CommonPrefs.displayEOLEnable);
    SetIndentationGuides(g_CommonPrefs.indentGuideEnable);
    SetEdgeColumn(80);
    SetEdgeMode(g_CommonPrefs.longLineOnEnable ? wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(g_CommonPrefs.whiteSpaceEnable ?
        wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);
    SetOvertype(g_CommonPrefs.overTypeInitial);
    SetReadOnly(g_CommonPrefs.readOnlyInitial);
    SetWrapMode(g_CommonPrefs.wrapModeInitial ?
        wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);

    return true;
}

EntityEditor::EntityEditor(wxWindow* parent, wxWindowID id, const wxPoint& pos,
    const wxSize& size, long style)
    : wxStyledTextCtrl(parent, id, pos, size, style)
{
    m_LineNrID = 0;
    m_DividerID = 1;
    m_FoldingID = 2;

    // initialize language
    m_language = NULL;

    // default font for all styles
    SetViewEOL(g_CommonPrefs.displayEOLEnable);
    SetIndentationGuides(g_CommonPrefs.indentGuideEnable);
    SetEdgeMode(g_CommonPrefs.longLineOnEnable ?
        wxSTC_EDGE_LINE : wxSTC_EDGE_NONE);
    SetViewWhiteSpace(g_CommonPrefs.whiteSpaceEnable ?
        wxSTC_WS_VISIBLEALWAYS : wxSTC_WS_INVISIBLE);
    SetOvertype(g_CommonPrefs.overTypeInitial);
    SetReadOnly(g_CommonPrefs.readOnlyInitial);
    SetWrapMode(g_CommonPrefs.wrapModeInitial ?
        wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);
    wxFont font(wxFontInfo(10).Family(wxFONTFAMILY_MODERN));
    StyleSetFont(wxSTC_STYLE_DEFAULT, font);
    StyleSetForeground(wxSTC_STYLE_DEFAULT, *wxBLACK);
    StyleSetBackground(wxSTC_STYLE_DEFAULT, *wxWHITE);
    StyleSetForeground(wxSTC_STYLE_LINENUMBER, wxColour("DARK GREY"));
    StyleSetBackground(wxSTC_STYLE_LINENUMBER, *wxWHITE);
    StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, wxColour("DARK GREY"));
    InitializePrefs(DEFAULT_LANGUAGE);

    // set visibility
    SetVisiblePolicy(wxSTC_VISIBLE_STRICT | wxSTC_VISIBLE_SLOP, 1);
    SetXCaretPolicy(wxSTC_CARET_EVEN | wxSTC_VISIBLE_STRICT | wxSTC_CARET_SLOP, 1);
    SetYCaretPolicy(wxSTC_CARET_EVEN | wxSTC_VISIBLE_STRICT | wxSTC_CARET_SLOP, 1);

    // markers
    MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_DOTDOTDOT, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_ARROWDOWN, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_EMPTY, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_DOTDOTDOT, "BLACK", "WHITE");
    MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_ARROWDOWN, "BLACK", "WHITE");
    MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_EMPTY, "BLACK", "BLACK");
    MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_EMPTY, "BLACK", "BLACK");

    // annotations
    AnnotationSetVisible(wxSTC_ANNOTATION_BOXED);

    // miscellaneous
    m_LineNrMargin = TextWidth(wxSTC_STYLE_LINENUMBER, "_999999");
    m_FoldingMargin = FromDIP(16);
    CmdKeyClear(wxSTC_KEY_TAB, 0); // this is done by the menu accelerator key
    SetLayoutCache(wxSTC_CACHE_PAGE);
    UsePopUp(wxSTC_POPUP_ALL);

    /* 
    * EntitySlayer Constant Preferences 
    */
    InitializePrefs("C++"); // Keep default Initialize or collapsible braces won't work

    // Whitespace Configuration
    SetViewWhiteSpace(wxSTC_WS_VISIBLEALWAYS);
    //SetUseTabs(false);
    //SetBackSpaceUnIndents(true);

    // Annotation Config
    AnnotationSetVisible(wxSTC_ANNOTATION_STANDARD);

    // Line Wrap Settings
    SetWrapMode(wxSTC_WRAP_WORD); 
    SetWrapIndentMode(wxSTC_WRAPINDENT_SAME);
    SetUseHorizontalScrollBar(false);

    // Line Number Settings
    SetMarginType(m_LineNrID, wxSTC_MARGIN_NUMBER); // enables line numbers
    SetMarginMask(m_LineNrID, 0);
    SetMarginWidth(m_LineNrID, 40);

    // Set Initial Node
    SetActiveNode(nullptr);
}

void EntityEditor::OnMarginClick(wxStyledTextEvent& event)
{
    if (event.GetMargin() == 2) {
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
            InsertText(GetCurrentPos(), "\";");
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