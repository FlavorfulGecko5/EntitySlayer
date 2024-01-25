#include <string_view>
#include <vector>
#include <set>
#include "wx/wx.h"
#include "wx/dataview.h"
#include "EntityNode.h"
#include "GenericBlockAllocator.h"

struct Sphere {
	float x = 0;
	float y = 0;
	float z = 0;
	float r = 0; // Radius
};

struct ParseResult {
	bool success = true;
	size_t errorLineNum = 0;
	std::string errorMessage;
};

class EntityParser : public wxDataViewModel {
	private:
	enum class CommandType : unsigned char
	{
		UNDECLARED,
		EDIT_TREE, // Call to EditTree function
		EDIT_TEXT,
		EDIT_POSITION
	};

	struct ParseCommand 
	{
		std::string text = "";       // .entities text we must parse for this command
		int parentID = 0;            // Positional id of the parent node
		int insertionIndex = 0;      // Number of nodes this command target
		int removalCount = 0;
		bool lastInGroup = false;
		CommandType type = CommandType::UNDECLARED;
	};

	enum class TokenType : unsigned char 
	{
		END = 0x00,
		BRACEOPEN = 0x01,
		BRACECLOSE = 0x02,
		ASSIGNMENT = 0x03,
		TERMINAL = 0x04,
		COMMENT = 0x05,
		IDENTIFIER = 0x06,
		VALUE_ANY = 0xF0,
		VALUE_NUMBER = 0xF1,
		VALUE_STRING = 0xF2,
		VALUE_KEYWORD = 0xF3
	};

	static const std::string FILTER_NOLAYERS;

	/*
	* Fields for longterm storage
	* of the parsed data
	*/
	public:
	wxDataViewCtrl* view = nullptr; // Must set this after construction
	private:
	bool fileWasCompressed;
	EntNode root;
	BlockAllocator<char> textAlloc; // Allocator for node name/value buffers
	BlockAllocator<EntNode> nodeAlloc; // Allocator for the nodes
	BlockAllocator<EntNode*> childAlloc; // Allocator for node child buffers

	// Root node's child count is orders of magnitude larger than any other object node
	// Repeated single-entity additions will create extremely large free blocks that cannot
	// come close to being filled faster than they're created. Hence, it's smarter to dedicate
	// a separate buffer, which we expand when necessary, to root children
	//
	// To represent filtering we have a buffer of booleans, the same length as the root child buffer
	// If a node passes all the filters, it's boolean value is true. If a node is filtered out, it's value is false
	// We refactored the filter system to this, replacing it's integration into the wxDataViewModel's GetChildren function
	// for a multitude of reasons:
	// 1. Performance - GetChildren is called a lot, resulting in many wasteful filter evaluations on all root children
	//	These become very problematic for large operations that add lots of entities at once
	// 2. Prevents debug errors and (possibly) memory leaks. If a new entity was created that didn't pass the filters, it would
	// be filtered out on-committ and a debug error throne from trying to add it to the tree.
	// 3. It's a relatively simple solution that will stay localized to the parser's core command-execution functions.
	// 
	// Using this new system we have full control over when we notify the control of changes - allowing us to cancel alerts
	// for filtered-out nodes. With debug errors gone, we also no longer need to clear the parser's undo/redo history
	// whenever we change filters
	EntNode** rootchild_buffer = nullptr;
	bool* rootchild_filter = nullptr; // Should be same size as child buffer. true = include, false = exclude
	int rootchild_capacity = 0;

	/* Variables for tracking command history */
	const size_t MAX_COMMAND_MEMORY = 100;
	int commandCount = 0;
	int redoIndex = 0;
	std::vector<ParseCommand> reverseGroup; // Reverse of the current group command incase we must cancel
	std::vector<ParseCommand> history;

	/* Functions related to command history */
	public:
	void PushGroupCommand();
	void CancelGroupCommand();
	void ClearHistory();
	void ExecuteCommand(ParseCommand& cmd);
	bool Undo();
	bool Redo();

	/*
	* Variables used during a parse.
	* Their values should be reset or cleared at the start/end of each parse
	*/
	private:
	// NEW PARSE VARIABLES
	char* ch = nullptr;    // Ptr to next char to be parsed
	char* first = nullptr; // Ptr to current identifier/value token

	std::string_view textView;					// View of the text we're currently parsing
	size_t currentLine = 1;
					
	TokenType lastTokenType = TokenType::END;
	std::string_view lastUniqueToken;			// Stores most recent identifier or value token
	std::string_view activeID;					// Second-most-recent token (typically an identifier)

	// Every node generated during the current parse (except the root node)
	// is inside here, or childed to a node inside here, until the moment it's
	// made a child of the root node. Hence, when cancelling a parse due to an exception:
	// 1. Free all nodes stored here then clear the vector
	// 2. Free the root node's children and childArray
	std::vector<EntNode*> tempChildren;

	public:

	~EntityParser()
	{
		delete[] rootchild_buffer;
		delete[] rootchild_filter;
	}

	/*
	* Constructs an EntityParser with minimal data
	*/
	EntityParser();

	/*
	* Constructs an EntityParser containing fully parsed data from the given file
	* @param filepath .entites file to parse
	* @param debug_logParseTime If true, outputs execution time data
	* @throw runtime_error thrown when the file cannot be parsed
	*/
	EntityParser(const std::string& filepath, const bool debug_logParseTime = false);

	/*
	* Parses a given string of text and replaces a pre-existing block of EntNodes
	* belonging to the same parent.
	* @param text Text to parse
	* @param parent Node whose children we're editing
	* @param insertionIndex Index to insert new nodes at
	* @param removeCount Number of nodes to delete, starting at insertionIndex
	* @param renumberLists If true, perform automatic list renumbering
	* @param highlightNew If true, highlight newly created nodes
	*/
	ParseResult EditTree(std::string text, EntNode* parent, int insertionIndex, int removeCount, bool renumberLists, bool highlightNew);


	/* Should be called only by undo/redo and other scenarios where the command is safe */
	void EditText(const std::string& text, EntNode* node, int nameLength, bool highlight);

	void EditPosition(EntNode* parent, int childIndex, int insertionIndex, bool highlight);


	void EditName(std::string text, EntNode* node);


	void EditValue(std::string text, EntNode* node);


	/* Move this outside of this class */
	void fixListNumberings(EntNode* parent, bool recursive, bool highlight);


	/*
	* ACCESSOR METHODS
	*/

	EntNode* getRoot();
	

	bool wasFileCompressed();
	

	/*
	* DEBUGGING METHODS
	*/
	void logAllocatorInfo(bool includeBlockList, bool logToLogger, bool logToFile, const std::string filepath = "");


	private:
	/*
	* Creates an exception for a supplied parsing error
	* The returned exception should be thrown immediately
	*/
	std::runtime_error Error(std::string msg);

	/*
	* RECURSIVE PARSING FUNCTIONS
	*/
	void initiateParse(std::string& text, EntNode* tempRoot, NodeType parentType,
		ParseResult& results);
	void parseContentsFile(EntNode* node);
	void parseContentsEntity(EntNode* node);
	void parseContentsLayer(EntNode* node);
	void parseContentsDefinition(EntNode* node);


	/*
	* ALLOCATION / DEALLOCATION FUNCTIONS
	*/

	void freeNode(EntNode* node);
	inline EntNode* setNodeObj(const NodeType p_type);
	inline EntNode* setNodeValue(const NodeType p_type);
	inline EntNode* setNodeNameOnly(const NodeType p_type);
	void setNodeChildren(EntNode* parent, const size_t startIndex);


	/*
	* TOKENIZATION FUNCTIONS
	*/

	/*
	* Used when tokenizing
	* Faster than standard library isalpha(char) function
	* Appears to save anywhere between 100-200 MS wall clock time on ~1.2 million line file
	*/
	inline bool isLetter();

	/*
	* Throws an error if the last-parsed token is not of the required type
	*/
	inline void assertLastType(TokenType requiredType);


	/*
	* Parses raw text for the next token
	* Throws an error if the token is not of the required type
	*/
	inline void assertIgnore(TokenType requiredType);

	/*
	* Parses raw text for the next token
	* If it's an identifier, distinguish whether it's a true ID or special keyword value
	*/
	inline void TokenizeAdjustValue();

	/*
	* Parses raw text for the next token
	*/
	void Tokenize();

	/*
	* FILTER FUNCTIONS
	*/
	public:

	/* 
	* Checks for newly created layers/classes/inherits and adds them to their respective filter checklists
	* Previously identified values are NOT removed from the checklists
	*/
	void refreshFilterMenus(wxCheckListBox* layerMenu, wxCheckListBox* classMenu, wxCheckListBox* inheritMenu);

	void SetFilters(wxCheckListBox* layerMenu, wxCheckListBox* classMenu, wxCheckListBox* inheritMenu,
		bool filterSpawnPosition, Sphere spawnSphere, wxCheckListBox* textMenu, bool caseSensitiveText);

	void FilteredSearch(const std::string& key, bool backwards, bool caseSensitive);

	/*
	* wxDataViewModel Functions
	*/

	// wxWidgets calls this function very frequently, on every visible node, if you so much as breathe on the dataview
	void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override
	{
		wxASSERT(item.IsOk());
		EntNode* node = (EntNode*)item.GetID();

		if (col == 0) {
			// Use value in entityDef node instead of "entity"
			if (node->parent == &root)
			{
				EntNode& entityDef = (*node)["entityDef"];
				if (&entityDef != EntNode::SEARCH_404)
				{
					variant = entityDef.getValueWX();
					return;
				}
			}
			variant = node->getNameWX();
		}
		else {
			variant = node->getValueWX();
		}
	}

	unsigned int GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const override
	{
		EntNode* node = (EntNode*)parent.GetID();

		if (!node) // There is our root node (which we shouldn't delete), and wx's invisible root node
		{
			array.Add(wxDataViewItem((void*)&root)); // !! This is how wxDataViewCtrl finds the root variable
			return 1;
		}

		if (node == &root)
		{
			bool* boolInc = rootchild_filter;
			EntNode** childInc = rootchild_buffer;
			for (int i = 0, max = root.childCount; i < max; i++, childInc++, boolInc++)
				if (*boolInc)
					array.Add(wxDataViewItem(*childInc));
			return array.size();
		}

		EntNode** childBuffer = node->getChildBuffer();
		int childCount = node->getChildCount();
		for (int i = 0; i < childCount; i++)
			array.Add(wxDataViewItem((void*)childBuffer[i]));
		return childCount;
	}

	wxDataViewItem GetParent(const wxDataViewItem& item) const override
	{
		if (!item.IsOk()) // Invisible root node
			return wxDataViewItem(0);

		EntNode* node = (EntNode*)item.GetID();
		return wxDataViewItem((void*)node->parent);
	}

	bool IsContainer(const wxDataViewItem& item) const override
	{
		if (!item.IsOk()) // Need this for invisible root node
			return true;

		EntNode* node = (EntNode*)item.GetID();

		return node->TYPE > NodeType::VALUE_COMMON; // Todo: ensure this is safe
	}

	/* These can probably stay defined in the header */

	wxString GetColumnType(unsigned int col) const override
	{
		return "string";
	}

	unsigned int GetColumnCount() const override
	{
		return 2;
	}

	bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) override
	{
		// Changes to fields will not be saved when double clicked/edited
		return false;
	}

	bool HasContainerColumns(const wxDataViewItem& item) const override
	{
		return true;
	}
};