#include "wx/wx.h"
#include "wx/stc/stc.h"  // styled text control
#include "prefs.h"
#include "defsext.h"
#include <EntityParser.h>

class EntityEditor: public wxStyledTextCtrl 
{
    private:
    EntNode* activeNode = nullptr;
    wxString originalText = "";

    // language properties
    const int ANNOTATION_STYLE = wxSTC_STYLE_LASTPREDEFINED + 1;
    LanguageInfo const* m_language;

    // margin variables
    int m_LineNrID;
    int m_LineNrMargin;
    int m_FoldingID;
    int m_FoldingMargin;
    int m_DividerID;

    public:
    /*
    * Critical Editing-Related Items
    */
    bool Modified ()
    {
        // return modified state
        return (GetModify() && !GetReadOnly());
    }

    void MarkUnmodified()
    {
        originalText = GetValue();
        SetModified(false);
    }

    void RevertEdits()
    {
        AnnotationClearAll();
        SetText(originalText);
        SetModified(false);
    }

    void setAnnotationError(const size_t lineNumber, const string& errorMessage)
    {
        AnnotationClearAll();
        AnnotationSetStyle(lineNumber - 1, ANNOTATION_STYLE);
        AnnotationSetText(lineNumber - 1, errorMessage);
        GotoLine(lineNumber - 1);
    }

    void SetActiveNode(EntNode* node)
    {
        wxLogMessage("Changing text editor's node");
        if (node == nullptr || node->getType() == NodeType::ROOT)
        {
            SetText("Select a node to begin editing.");
            SetReadOnly(true);
        }
        else {
            SetReadOnly(false);
            SetText(node->toString());
        }
        activeNode = node;
        AnnotationClearAll();
        EmptyUndoBuffer();
        MarkUnmodified();
    }

    EntNode* Node() 
    { 
        return activeNode; 
    }

    /*
    * Events
    */
    void OnSize( wxSizeEvent &event )
    {
        int x = GetClientSize().x +
            (g_CommonPrefs.lineNumberEnable ? m_LineNrMargin : 0) +
            (g_CommonPrefs.foldEnable ? m_FoldingMargin : 0);
        if (x > 0) SetScrollWidth(x);
        event.Skip();
    }

    void OnMarginClick (wxStyledTextEvent &event)
    {
        if (event.GetMargin() == 2) {
            int lineClick = LineFromPosition(event.GetPosition());
            int levelClick = GetFoldLevel(lineClick);
            if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
                ToggleFold(lineClick);
            }
        }
    }

    void OnCharAdded  (wxStyledTextEvent &event)
    {
        char chr = (char)event.GetKey();
        int currentLine = GetCurrentLine();
        // Change this if support for mac files with \r is needed
        if (chr == '\n') {
            int lineInd = 0;
            if (currentLine > 0) {
                lineInd = GetLineIndentation(currentLine - 1);
            }
            if (lineInd == 0) return;
            SetLineIndentation(currentLine, lineInd);
            GotoPos(GetLineIndentPosition(currentLine));
        }
        /*else if (chr == '#') {
            wxString s = "define?0 elif?0 else?0 endif?0 error?0 if?0 ifdef?0 "
                         "ifndef?0 include?0 line?0 pragma?0 undef?0";
            AutoCompShow(0,s);
        }*/
    }

    void OnKeyDown(wxKeyEvent &event)
    {
        if (event.GetKeyCode() == WXK_TAB) {
            if (event.ShiftDown()) BackTab();
            else Tab();
            return;
        }

        event.Skip();
    }


    /*
    * Error info
    */
    int FreqWithLimit(wxString& Input, size_t maxOffset, wxUniChar ch, size_t& CharNr)
    {
        int count = 0;
        size_t Index = 0;
        for (auto i = Input.begin(); (i != Input.end()) && (maxOffset > Index); ++i) {
            if (*i == ch) {
                count++;
                CharNr = 0;
            }

            Index += 1;
            CharNr += 1;
        }

        return count;
    }

    void SetParseErrorHelper(wxString String, size_t Offset) 
    {
        size_t CharNr = 0;
        size_t LineNumber = FreqWithLimit(String, Offset, '\n', CharNr);

        // Position some text under the parsing error.
        wxString ErrorString;
        ErrorString.resize(CharNr + 6, L' ');
        ErrorString += "^ Syntax Error";
        AnnotationSetStyle(LineNumber, ANNOTATION_STYLE);
        AnnotationSetText(LineNumber, ErrorString);
        
        // line numbers start at 0
        // GotoLine()
    }


    /*
    * Construction and preference setting
    */
    bool InitializePrefs(const wxString& name)
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

    EntityEditor(wxWindow* parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style =
        #ifndef __WXMAC__
        wxSUNKEN_BORDER |
        #endif
        wxVSCROLL
    ) 
    : wxStyledTextCtrl (parent, id, pos, size, style) 
    {
        m_LineNrID = 0;
        m_DividerID = 1;
        m_FoldingID = 2;

        // initialize language
        m_language = NULL;

        // default font for all styles
        SetViewEOL (g_CommonPrefs.displayEOLEnable);
        SetIndentationGuides (g_CommonPrefs.indentGuideEnable);
        SetEdgeMode (g_CommonPrefs.longLineOnEnable?
                     wxSTC_EDGE_LINE: wxSTC_EDGE_NONE);
        SetViewWhiteSpace (g_CommonPrefs.whiteSpaceEnable?
                           wxSTC_WS_VISIBLEALWAYS: wxSTC_WS_INVISIBLE);
        SetOvertype (g_CommonPrefs.overTypeInitial);
        SetReadOnly (g_CommonPrefs.readOnlyInitial);
        SetWrapMode (g_CommonPrefs.wrapModeInitial?
                     wxSTC_WRAP_WORD: wxSTC_WRAP_NONE);
        wxFont font(wxFontInfo(10).Family(wxFONTFAMILY_MODERN));
        StyleSetFont (wxSTC_STYLE_DEFAULT, font);
        StyleSetForeground (wxSTC_STYLE_DEFAULT, *wxBLACK);
        StyleSetBackground (wxSTC_STYLE_DEFAULT, *wxWHITE);
        StyleSetForeground (wxSTC_STYLE_LINENUMBER, wxColour ("DARK GREY"));
        StyleSetBackground (wxSTC_STYLE_LINENUMBER, *wxWHITE);
        StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, wxColour ("DARK GREY"));
        InitializePrefs (DEFAULT_LANGUAGE);

        // set visibility
        SetVisiblePolicy (wxSTC_VISIBLE_STRICT|wxSTC_VISIBLE_SLOP, 1);
        SetXCaretPolicy (wxSTC_CARET_EVEN|wxSTC_VISIBLE_STRICT|wxSTC_CARET_SLOP, 1);
        SetYCaretPolicy (wxSTC_CARET_EVEN|wxSTC_VISIBLE_STRICT|wxSTC_CARET_SLOP, 1);

        // markers
        MarkerDefine (wxSTC_MARKNUM_FOLDER,        wxSTC_MARK_DOTDOTDOT, "BLACK", "BLACK");
        MarkerDefine (wxSTC_MARKNUM_FOLDEROPEN,    wxSTC_MARK_ARROWDOWN, "BLACK", "BLACK");
        MarkerDefine (wxSTC_MARKNUM_FOLDERSUB,     wxSTC_MARK_EMPTY,     "BLACK", "BLACK");
        MarkerDefine (wxSTC_MARKNUM_FOLDEREND,     wxSTC_MARK_DOTDOTDOT, "BLACK", "WHITE");
        MarkerDefine (wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_ARROWDOWN, "BLACK", "WHITE");
        MarkerDefine (wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_EMPTY,     "BLACK", "BLACK");
        MarkerDefine (wxSTC_MARKNUM_FOLDERTAIL,    wxSTC_MARK_EMPTY,     "BLACK", "BLACK");

        // annotations
        AnnotationSetVisible(wxSTC_ANNOTATION_BOXED);

        // miscellaneous
        m_LineNrMargin = TextWidth (wxSTC_STYLE_LINENUMBER, "_999999");
        m_FoldingMargin = FromDIP(16);
        CmdKeyClear (wxSTC_KEY_TAB, 0); // this is done by the menu accelerator key
        SetLayoutCache (wxSTC_CACHE_PAGE);
        UsePopUp(wxSTC_POPUP_ALL);

        InitializePrefs("C++"); // Keep default Initialize or collapsible braces won't work
        SetViewWhiteSpace(wxSTC_WS_VISIBLEALWAYS);
        AnnotationSetVisible(wxSTC_ANNOTATION_STANDARD);
       // SetMarginType(m_LineNrID, wxSTC_MARGIN_NUMBER); // enables line numbers
       // SetMarginMask(m_LineNrID, 0);
        //SetMarginWidth(m_LineNrID, 40);
        SetActiveNode(nullptr);
    }
    
    private:
    wxDECLARE_EVENT_TABLE();
};
