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

	string_view getNameUQ() {
		if (nameLength < 2)
			return string_view(textPtr, nameLength);
		return string_view(textPtr + 1, nameLength - 2);
	}

	// If the value is a string literal, get it with the quotes removed
	string_view getValueUQ() {
		if (valLength < 2)
			return string_view(textPtr + nameLength, valLength);
		return string_view(textPtr + nameLength + 1, valLength - 2);
	}

	wxString getNameWX() { return wxString(textPtr, nameLength); }

	wxString getValueWX() { return wxString(textPtr + nameLength, valLength); }

	int NameLength() { return nameLength; }

	int ValueLength() { return valLength; }

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

	EntNode* ChildAt(int index) {
		return children[index];
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

	/*
	* MUTATORS
	*/

	void populateParentRefs(EntNode* p)
	{
		parent = p;
		for(int i = 0; i < childCount; i++)
			children[i]->populateParentRefs(this);
	}

	//bool swapChildPositions(EntNode* a, EntNode* b)
	//{
	//	int indexA = getChildIndex(a);
	//	int indexB = getChildIndex(b);
	//	if (indexA == -1 || indexB == -1)
	//		return false;

	//	EntNode* temp = a;
	//	children[indexA] = b;
	//	children[indexB] = temp;

	//	return true;
	//}

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

		char fixedKey0 = key[0];
		if(fixedKey0 > '`' && fixedKey0 < '{') fixedKey0 -= 32;

		FAILED:
		while (i < max)
		{
			char c1 = textPtr[i++];
			if(c1 > '`' && c1 < '{') c1-= 32;

			if (c1 == fixedKey0)
			{
				for (int j = 1; j < k; j++, i++)
				{
					c1 = textPtr[i]; if (c1 > '`' && c1 < '{') c1 -= 32;
					char c2 = key[j]; if (c2 > '`' && c2 < '{') c2 -= 32;
					// Do not increment i here: If we fail, we'll want to
					// check the failing character to see if it == key[0]
					if (c1 != c2)
						goto FAILED;
				}

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

	void generateText(string& buffer, int wsIndex = 0)
	{
		buffer.append(wsIndex, '\t');
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
		for (int i = 0; i < childCount; i++)
		{
			children[i]->generateText(buffer, wsIndex + 1);
			buffer.push_back('\n');
		}
		if(wsIndex > 0) // If we generate entityDef text directly this will be -1
			buffer.append(wsIndex, '\t');
		buffer.push_back('}');
	}
};