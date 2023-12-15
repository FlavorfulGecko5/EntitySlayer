#include <string>
#include "wx/string.h"
using namespace std;

enum class NodeType : unsigned char {
	UNDESIGNATED,
	COMMENT,
	VALUE_LAYER,
	VALUE_FILE,
	VALUE_COMMON,
	OBJECT_COMMON,
	OBJECT_SIMPLE_ENTITY, // Need two simple obj enums for
	OBJECT_SIMPLE_LAYER,  // parse and replace algorithm
	OBJECT_ENTITYDEF,
	ROOT
};

class EntNode 
{
	friend class EntityParser;

	public:
	static EntNode* SEARCH_404; // Returned by all search functions if a key-node is not found.

	private:
	// A lookup table for case-insensitive string comparisons. Is this a cleverly efficient solution? Not currently clear
	static inline const char CASE_TABLE[128] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
		0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, // Lowercase indices
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F  // with uppercase values
	};
	static inline size_t TAB_TABLE_LENGTH = 16;
	static inline string* TAB_TABLE = new string[TAB_TABLE_LENGTH] {
		"",
		"",		// We need 2 empty strings because entitydef closing brace is moved back one whitespace.
		"\t",   // This means text generation should begin with index 1
		"\t\t",
		"\t\t\t",
		"\t\t\t\t",
		"\t\t\t\t\t",
		"\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t\t\t\t\t",
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t",
	};

	private:
	EntNode* parent = nullptr;
	EntNode** children = nullptr; // Unused by value nodes
	char* textPtr = nullptr; // Pointer to text buffer with data [name][value]
	int nameLength = 0;
	int valLength = 0;
	int childCount = 0;
	NodeType TYPE = NodeType::UNDESIGNATED;

	public:
	EntNode() {}

	EntNode(NodeType p_TYPE) : TYPE(p_TYPE) {}

	/*
	* ACCESSOR METHODS
	*/

	NodeType getType() {return TYPE;}

	string_view getName() {return string_view(textPtr, nameLength); }

	string_view getValue() {return string_view(textPtr + nameLength, valLength); }

	wxString getNameWX() {return wxString(textPtr, nameLength); }
	
	wxString getValueWX() {return wxString(textPtr + nameLength, valLength); }

	int NameLength() {return nameLength;}

	int ValueLength() {return valLength;}

	EntNode* getParent() { return parent; }

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

	/*
	* Use this to quickly check whether a node is inside the tree
	* (Parser must be properly resetting variables of freed nodes for this to work)
	*/
	bool isValid()
	{
		return TYPE != NodeType::UNDESIGNATED;
	}

	/*
	* Calling node is assumed to be the root
	* id should be initialized to 0
	*/
	bool findPositionalID(EntNode* n, int& id)
	{
		if(this == n) return true;
		id++;

		for(int i = 0; i < childCount; i++)
			if(children[i]->findPositionalID(n, id)) return true;
		return false;
	}

	EntNode* nodeFromPositionalID(int& decrementor)
	{
		if(decrementor == 0) return this;
		for (int i = 0; i < childCount; i++)
		{
			EntNode* result = children[i]->nodeFromPositionalID(--decrementor);
			if(result != nullptr) return result;
		}
		return nullptr;
	}

	/*
	* Searches the node's children for a node whose name equals the given key
	* The name must be an exact match.
	* 
	* @param key - the name to search for
	* @return The first child with the given name, or 404 Node if not found
	*/
	EntNode& operator[](const string& key)
	{
		for (int i = 0; i < childCount; i++)
			if (children[i]->getName() == key)
				return *children[i];
		return *SEARCH_404;
	}

	EntNode* operator[](const int key)
	{
		return children[key];
	}

	/*
	* MUTATORS
	*/

	void populateParentRefs(EntNode* p)
	{
		parent = p;
		for(int i = 0; i < childCount; i++)
			children[i]->populateParentRefs(this);
	}

	bool swapChildPositions(EntNode* a, EntNode* b)
	{
		int indexA = getChildIndex(a);
		int indexB = getChildIndex(b);
		if (indexA == -1 || indexB == -1)
			return false;

		EntNode* temp = a;
		children[indexA] = b;
		children[indexB] = temp;

		return true;
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

	bool searchText(const string& key, const bool caseSensitive)
	{
		if (caseSensitive)
		{
			string_view s(textPtr, nameLength + valLength);
			return s.find(key) != string_view::npos;
		}

		int k = (int)key.length(), n = nameLength + valLength;
		int max = n - k + 1; // Don't search where key can't fit.
		int i = 0;

		FAILED:
		while (i < max)
		{
			if (CASE_TABLE[textPtr[i++]] == CASE_TABLE[key[0]])
			{
				for (int j = 1; j < k; j++, i++)
					// Do not increment i here: If we fail, we'll want to
					// check the failing character to see if it == key[0]
					if (CASE_TABLE[textPtr[i]] != CASE_TABLE[key[j]])
						goto FAILED;
				return true;
			}
		}
		return false;
	}

	/*
	* For searching downwards, we must:
	* - Check all children of the starting node
	* - recursively check all nodes listed after the starting node
	*	- parents in parent chain do not need their values checked
	*/
	EntNode* searchDownwards(const string& key, const bool caseSensitive, const EntNode* startAfter=nullptr)
	{
		int startIndex = 0;
		if(startAfter != nullptr) // Ensures all children in the starting node are checked
			while(startIndex < childCount)
				if(children[startIndex++] == startAfter) break;

		for (int i = startIndex; i < childCount; i++)
		{
			EntNode* result = children[i]->searchDownwardsLocal(key, caseSensitive);
			if(result != SEARCH_404) return result;
		}

		if(parent != nullptr)
			return parent->searchDownwards(key, caseSensitive, this);

		// Wrap around by performing local search on root
		return searchDownwardsLocal(key, caseSensitive);
	}

	EntNode* searchDownwardsLocal(const string& key, const bool caseSensitive)
	{
		if(searchText(key, caseSensitive)) return this;
		for (int i = 0; i < childCount; i++)
		{
			EntNode* result = children[i]->searchDownwardsLocal(key, caseSensitive);
			if(result != SEARCH_404) return result;
		}
		return SEARCH_404;
	}

	/*
	* Upward Searches require we:
	* - Do not check the children of the starting node
	* - Check children above the node
	* - Parents have their text checked AFTER children are checked
	*/

	/*
	* Searches up the node tree for the given key, starting with
	* the node directly above this one.
	* 
	*/
	EntNode* searchUpwards(const string &key, const bool caseSensitive) 
	{
		// Wrap around by performing local search on root
		if(parent == nullptr)
			return searchUpwardsLocal(key, caseSensitive);

		// Check the parent's child nodes placed above this one
		int startIndex = parent->childCount - 1;
		while(startIndex > -1)
			if(parent->children[startIndex--] == this) break;

		for (int i = startIndex; startIndex > -1; startIndex--)
		{
			EntNode* result = parent->children[i]->searchUpwardsLocal(key, caseSensitive);
			if(result != SEARCH_404) return result;
		}

		if(parent->searchText(key, caseSensitive)) return parent; 
		return parent->searchUpwards(key, caseSensitive);
	}

	EntNode* searchUpwardsLocal(const string& key, const bool caseSensitive)
	{
		for(int i = childCount - 1; i > -1; i--)
		{
			EntNode* result = children[i]->searchUpwardsLocal(key, caseSensitive);
			if(result != SEARCH_404) return result;
		} // Search children in reverse order, then the node's own text
		if(searchText(key, caseSensitive)) return this;
		return SEARCH_404;
	}

	/*
	* TEXT GENERATION
	*/

	string toString()
	{
		string buffer;
		generateText(buffer);
		return buffer;
	}

	void generateText(string& buffer, size_t wsIndex = 1)
	{
		buffer.append(TAB_TABLE[wsIndex]);
		buffer.append(textPtr, nameLength);
		switch (TYPE)
		{
			case NodeType::UNDESIGNATED:
			case NodeType::VALUE_LAYER:
			case NodeType::COMMENT:
			return;

			case NodeType::ROOT: // As a fail-safe, Root should only generate it's full text when writing to a file
			buffer.append("root");
			return;

			case NodeType::VALUE_FILE:
			buffer.push_back(' ');
			buffer.append(textPtr + nameLength, valLength);
			return;

			case NodeType::VALUE_COMMON:
			buffer.append(" = ");
			buffer.append(textPtr + nameLength, valLength);
			buffer.push_back(';');
			return;

			case NodeType::OBJECT_COMMON:
			buffer.append(" = {\n");
			break;

			case NodeType::OBJECT_SIMPLE_ENTITY:
			case NodeType::OBJECT_SIMPLE_LAYER:
			buffer.append(" {\n");
			break;

			case NodeType::OBJECT_ENTITYDEF:
			buffer.push_back(' ');
			buffer.append(textPtr + nameLength, valLength);
			buffer.append(" {\n");
			wsIndex--; 	// Entitydef objects have no inner indentation
			break;      // and their closing brace is one whitespace char backwards
		}
		size_t nextSize = wsIndex + 1;
		if (nextSize == TAB_TABLE_LENGTH)
		{
			string* oldTable = TAB_TABLE;
			size_t oldLength = TAB_TABLE_LENGTH;
			TAB_TABLE_LENGTH *= 2;
			TAB_TABLE = new string[TAB_TABLE_LENGTH];
			for(size_t i = 0; i < oldLength; i++)
				TAB_TABLE[i] = oldTable[i];
			for(size_t i = oldLength; i < TAB_TABLE_LENGTH; i++)
				TAB_TABLE[i] = TAB_TABLE[i-1] + '\t';
			delete[] oldTable;
		}
		for (int i = 0; i < childCount; i++)
		{
			children[i]->generateText(buffer, nextSize);
			buffer.push_back('\n');
		}
		buffer.append(TAB_TABLE[wsIndex]);
		buffer.push_back('}');
	}
};