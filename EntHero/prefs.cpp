//////////////////////////////////////////////////////////////////////////////
// File:        contrib/samples/stc/prefs.cpp
// Purpose:     STC test Preferences initialization
// Maintainer:  Wyo
// Created:     2003-09-01
// Copyright:   (c) wxGuide
// Licence:     wxWindows licence
//////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------------------------------
// headers
//----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all 'standard' wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

//! wxWidgets headers

//! wxWidgets/contrib headers

//! application headers
#include "prefs.h"       // Preferences


//============================================================================
// declarations
//============================================================================


//----------------------------------------------------------------------------
// keywordlists
// C++
const char* CppWordlist1 = "bool char class double enum false float int long new short signed struct true unsigned ";


//----------------------------------------------------------------------------
//! languages
const LanguageInfo g_LanguagePrefs =
    {
     {{mySTC_TYPE_DEFAULT},
      {mySTC_TYPE_COMMENT},
      {mySTC_TYPE_COMMENT_LINE},
      {mySTC_TYPE_COMMENT_DOC},
      {mySTC_TYPE_NUMBER},
      {mySTC_TYPE_WORD1}, // KEYWORDS - Was word list 1
      {mySTC_TYPE_STRING},
      {mySTC_TYPE_CHARACTER},
      {mySTC_TYPE_UUID},
      {mySTC_TYPE_PREPROCESSOR},
      {mySTC_TYPE_OPERATOR},
      {mySTC_TYPE_IDENTIFIER}, // Regular Words
      {mySTC_TYPE_STRING_EOL},
      {mySTC_TYPE_DEFAULT}, // VERBATIM
      {mySTC_TYPE_REGEX},
      {mySTC_TYPE_COMMENT_SPECIAL}, // DOXY
      {mySTC_TYPE_WORD2}, // EXTRA WORDS - Was word list 2
      {mySTC_TYPE_WORD3}, // DOXY KEYWORDS - Was word list 3
      {mySTC_TYPE_ERROR}, // KEYWORDS ERROR
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1},
      {-1}}
    };


//----------------------------------------------------------------------------
//! style types
const StyleInfo g_StylePrefs [] = {
    // mySTC_TYPE_DEFAULT
    {"Default",
     "BLACK", "WHITE",
     },

    // mySTC_TYPE_WORD1
    {"Keyword1",
     "BLUE", "WHITE",
     true},

    // mySTC_TYPE_WORD2
    {"Keyword2",
     "MIDNIGHT BLUE", "WHITE",
     },

    // mySTC_TYPE_WORD3
    {"Keyword3",
     "CORNFLOWER BLUE", "WHITE",
     },

    // mySTC_TYPE_WORD4
    {"Keyword4",
     "CYAN", "WHITE",
     },

    // mySTC_TYPE_WORD5
    {"Keyword5",
     "DARK GREY", "WHITE",
     },

    // mySTC_TYPE_WORD6
    {"Keyword6",
     "GREY", "WHITE",
     },

    // mySTC_TYPE_COMMENT
    {"Comment",
     "FOREST GREEN", "WHITE",
     },

    // mySTC_TYPE_COMMENT_DOC
    {"Comment (Doc)",
     "FOREST GREEN", "WHITE",
     },

    // mySTC_TYPE_COMMENT_LINE
    {"Comment line",
     "FOREST GREEN", "WHITE",
     },

    // mySTC_TYPE_COMMENT_SPECIAL
    {"Special comment",
     "FOREST GREEN", "WHITE",
     false, true},

    // mySTC_TYPE_CHARACTER
    {"Character",
     "KHAKI", "WHITE",
     },

    // mySTC_TYPE_CHARACTER_EOL
    {"Character (EOL)",
     "KHAKI", "WHITE",
     },

    // mySTC_TYPE_STRING
    {"String",
     "BROWN", "WHITE",
     },

    // mySTC_TYPE_STRING_EOL
    {"String (EOL)",
     "BROWN", "WHITE",
     },

    // mySTC_TYPE_DELIMITER
    {"Delimiter",
     "ORANGE", "WHITE",
     },

    // mySTC_TYPE_PUNCTUATION
    {"Punctuation",
     "ORANGE", "WHITE",
     },

    // mySTC_TYPE_OPERATOR
    {"Operator",
     "BLACK", "WHITE",
     true},

    // mySTC_TYPE_BRACE
    {"Label",
     "VIOLET", "WHITE",
     },

    // mySTC_TYPE_COMMAND
    {"Command",
     "BLUE", "WHITE",
     },

    // mySTC_TYPE_IDENTIFIER
    {"Identifier",
     "BLACK", "WHITE",
     },

    // mySTC_TYPE_LABEL
    {"Label",
     "VIOLET", "WHITE",
     },

    // mySTC_TYPE_NUMBER
    {"Number",
     "SIENNA", "WHITE",
     },

    // mySTC_TYPE_PARAMETER
    {"Parameter",
     "VIOLET", "WHITE",
     false, true},

    // mySTC_TYPE_REGEX
    {"Regular expression",
     "ORCHID", "WHITE",
     },

    // mySTC_TYPE_UUID
    {"UUID",
     "ORCHID", "WHITE",
     },

    // mySTC_TYPE_VALUE
    {"Value",
     "ORCHID", "WHITE",
     false, true},

    // mySTC_TYPE_PREPROCESSOR
    {"Preprocessor",
     "GREY", "WHITE",
     },

    // mySTC_TYPE_SCRIPT
    {"Script",
     "DARK GREY", "WHITE",
     },

    // mySTC_TYPE_ERROR
    {"Error",
     "RED", "WHITE",
     },

    // mySTC_TYPE_UNDEFINED
    {"Undefined",
     "ORANGE", "WHITE",
     }

    };

const int g_StylePrefsSize = WXSIZEOF(g_StylePrefs);
