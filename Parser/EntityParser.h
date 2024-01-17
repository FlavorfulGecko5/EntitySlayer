#include <string_view>
#include <vector>
#include "EntityNode.h"
#include "GenericBlockAllocator.h"

class EntityModel;
class wxDataViewCtrl;

struct ParseResult {
	bool success = true;
	size_t errorLineNum = 0;
	std::string errorMessage;
};

class EntityParser {
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

	/*
	* Fields for longterm storage
	* of the parsed data
	*/
	public:
	wxDataViewCtrl* view = nullptr;
	EntityModel* model = nullptr; // Must set this after construction
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
	EntNode** rootChildBuffer = nullptr; 
	int maxRootChildren = 0;

	/*
	* MUST TAKE INTO CONSIDERATION THIS ROOT CHILD BUFFER ANYWHERE THE ROOT'S CHILDREN COULD GET EXPANDED:
	* - On initial parse (done)
	* - On Edit Tree
	* 
	* When filter buffer is introduced, must also ensure synchonization with true child buffer:
	* - On initial parse
	* - On Edit Tree
	* - On Edit Position
	* - On Fix List Numberings? (Add safeguards to prevent this from operating on the root)
	* - Will also need to rethink the positional ID system (or maybe not, since we clear history when filters are applied)
	*/

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
		delete[] rootChildBuffer;
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
};