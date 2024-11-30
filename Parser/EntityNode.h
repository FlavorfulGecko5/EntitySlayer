#include <string_view>

class wxString;
class EntNode 
{
	friend class EntityParser;

	public:
	
	/* Node Flags */
	enum : uint16_t {
		/* Individual Flags */

		NF_Equals      = 1 << 0,
		NF_Semicolon   = 1 << 1,
		NF_Braces      = 1 << 2,
		NF_NoIndent    = 1 << 3,

		/* Node Flag Combos */

		NFC_RootNode       = NF_NoIndent,
		NFC_ObjSimple      = NF_Braces,
		NFC_ObjCommon      = NF_Braces | NF_Equals,
		NFC_ObjEntitydef   = NF_Braces | NF_NoIndent,
		NFC_ValueLayer     = 0,
		NFC_ValueFile      = 0,
		NFC_ValueDarkmetal = NF_Equals,
		NFC_ValueCommon    = NF_Equals | NF_Semicolon,
		NFC_Comment        = 0
	};

	public:
	static EntNode* SEARCH_404; // Returned by all search functions if a key-node is not found.

	private:
	EntNode* parent = nullptr;
	EntNode** children = nullptr; // Unused by value nodes
	char* textPtr = nullptr; // Pointer to text buffer with data [name][value]
	int childCount = 0;
	int maxChildren = 0;
	short nameLength = 0;
	short valLength = 0;
	uint16_t nodeFlags = 0;

	//NodeType TYPE = NodeType::UNDESIGNATED;

	// If false, don't display this or it's children in a dataview tree GUI.
	// Setting this to true by default means newly added nodes are always displayed
	// regardless of what filters are being applied (Possible todo: test if they pass filters first?)
	bool filtered = true; 

	public:
	EntNode() {}

	EntNode(uint16_t p_Flags) : nodeFlags(p_Flags) {}

	/*
	* ACCESSOR METHODS
	*/

	uint16_t getFlags() {return nodeFlags;}

	std::string_view getName() {return std::string_view(textPtr, nameLength); }

	std::string_view getValue() {return std::string_view(textPtr + nameLength, valLength); }

	bool hasValue() {return valLength > 0;};

	// If the name is a string literal, return it unquoted. Otherwise return the name as normal
	std::string_view getNameUQ() {
		if (nameLength < 2 || *textPtr != '"')
			return std::string_view(textPtr, nameLength);
		return std::string_view(textPtr + 1, nameLength - 2);
	}

	// If the value is a string literal, get it with the quotes removed. Else return the value as normal
	std::string_view getValueUQ() {
		if (valLength < 2 || textPtr[nameLength] != '"')
			return std::string_view(textPtr + nameLength, valLength);
		return std::string_view(textPtr + nameLength + 1, valLength - 2);
	}

	wxString getNameWX();
	wxString getValueWX();
	wxString getNameWXUQ();
	wxString getValueWXUQ();

	bool IsComment();

	bool IsRoot();

	bool IsContainer();

	int NameLength() { return nameLength; }

	int ValueLength() { return valLength; }

	EntNode* getParent() { return parent; }

	bool HasParent() {return parent != nullptr;}

	EntNode** getChildBuffer() { return children; }

	int getChildCount() {return childCount;}

	/*
	* Gets the index of a node in this node's child buffer
	* @returns Index of the child, or -1 if the node could not be found.
	*/
	int getChildIndex(EntNode* child) {
		for(int i = 0; i < childCount; i++)
			if(children[i] == child)
				return i;
		return -1;
	}

	EntNode* ChildAt(int index) {
		return children[index];
	}

	// Checks whether node is filtered out, either by itself or one of it's ancestors
	bool isFiltered() {
		EntNode* node = this;

		while (node->HasParent()) {
			if(!node->filtered)
				return false;
			node = node->parent;
		}
		return node->filtered;
	}

	// More generally, get the second ancestor
	EntNode* getEntity() {
		if(!HasParent()) return nullptr;

		EntNode* entity = this;
		while(entity->parent->HasParent())
			entity = entity->parent;
		return entity;
	}

	bool IsRelatedTo(EntNode* b)
	{
		EntNode* current = this;
		while (current != nullptr)
			if (current == b)
				return true;
			else current = current->parent;

		current = b->parent;
		while(current != nullptr)
			if(current == this)
				return true;
			else current = current->parent;
		return false;
	}

	bool IsDescendantOf(EntNode* ancestor) {
		EntNode* current = this;
		while (current != nullptr)
			if(current == ancestor)
				return true;
			else current = current->parent;
		return false;
	}

	/*
	* Calling node is assumed to be the root
	* id should be initialized to 0
	*/
	bool findPositionalID(EntNode* n, int& id);

	EntNode* nodeFromPositionalID(int& decrementor);


	/*
	* Searches the node's children for a node whose name equals the given key
	* The name must be an exact match.
	* 
	* @param key - the name to search for
	* @return The first child with the given name, or 404 Node if not found
	*/
	EntNode& operator[](const std::string& key)
	{
		for (int i = 0; i < childCount; i++)
			if (children[i]->getName() == key)
				return *children[i];
		return *SEARCH_404;
	}

	/*
	* DEBUGGING FUNCTIONS
	*/

	size_t validateParentRefs(EntNode* expectedParent)
	{
		size_t mismatches = 0;
		if (parent != expectedParent)
			mismatches++;
		for (int i = 0; i < childCount; i++)
			mismatches += children[i]->validateParentRefs(this);
		return mismatches;
	}

	size_t countNodes() 
	{
		size_t sum = 1;
		for (int i = 0; i < childCount; i++)
			sum += children[i]->countNodes();
		return sum;
	}

	/*
	* SEARCH FUNCTIONS
	*/
	bool searchText(const std::string& key, const bool caseSensitive, const bool exactLength);

	EntNode* searchDownwards(const std::string& key, const bool caseSensitive, const bool exactLength, const EntNode* startAfter = nullptr); // Todo: Remove this default parameter

	EntNode* searchDownwardsLocal(const std::string& key, const bool caseSensitive, const bool exactLength);

	/*
	* Searches up the node tree for the given key, starting with
	* the node directly above this one.
	*/
	EntNode* searchUpwards(const std::string& key, const bool caseSensitive, const bool exactLength);

	EntNode* searchUpwardsLocal(const std::string& key, const bool caseSensitive, const bool exactLength);


	/*
	* TEXT GENERATION
	*/

	std::string toString()
	{
		std::string buffer;
		generateText(buffer);
		return buffer;
	}

	void generateText(std::string& buffer, int wsIndex = 0);


	/*
	* Converts the entirety of this node into text and saves
	* the result to a file
	* @param filepath File to write to
	* @param oodleCompress If true, compress the file
	* @param debug_logTime If true, output execution time data.
	*/
	void writeToFile(const std::string filepath, const bool oodleCompress, const bool debug_logTime = false);
};