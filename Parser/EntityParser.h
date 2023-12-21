#include "EntityNode.h"
#include "GenericBlockAllocator.h"
#include "EntityLogger.h"
#include "Oodle.h"
#include <string_view>
#include <chrono>
#include <fstream>
using namespace std;



struct ParseResult {
	bool success = true;
	size_t errorLineNum = 0;
	string errorMessage;

	EntNode* parent = nullptr;
	vector<EntNode*> removedNodes;
	vector<EntNode*> addedNodes;
};

class EntityParser {
	private:
	enum class CommandType
	{
		EDITTREE // Call to EditTree function
	};

	struct ParseCommand 
	{
		CommandType type;
		string text;             // .entities text we must parse for this command
		int parentID;            // Positional id of the parent node
		int insertionIndex;      // Number of nodes this command target
		int removalCount;
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

	private:
	/*
	* Fields for longterm storage
	* of the parsed data
	*/
	EntNode root;
	BlockAllocator<char> textAlloc; // Allocator for node name/value buffers
	BlockAllocator<EntNode> nodeAlloc; // Allocator for the nodes
	BlockAllocator<EntNode*> childAlloc; // Allocator for node child buffers
	bool fileWasCompressed;

	const size_t MAX_COMMAND_MEMORY = 100;
	vector<ParseCommand> undoRedoStack;
	int undoIndex = 0;
	bool undoingOrRedoing = false; // If true, reverse the command at undoIndex instead of pushing a new one
	/*
	* Let undoIndex be the insertion index for new commands
	* Then:
	* - The first undoable command is undoIndex - 1
	* - The first redoable command is at undoIndex
	*/
	

	/*
	* Variables used during a parse.
	* Their values should be reset or cleared at the start/end of each parse
	*/
	string_view textView;					// View of the text we're currently parsing
	size_t currentLine = 1;
	size_t it = 0;							// Index of the next char to be parsed
	size_t start = 0;						// Starting index of current identifier/value token
	TokenType lastTokenType = TokenType::END;
	string_view lastUniqueToken;			// Stores most recent identifier or value token
	string_view activeID;					// Second-most-recent token (typically an identifier)

	// Every node generated during the current parse (except the root node)
	// is inside here, or childed to a node inside here, until the moment it's
	// made a child of the root node. Hence, when cancelling a parse due to an exception:
	// 1. Free all nodes stored here then clear the vector
	// 2. Free the root node's children and childArray
	vector<EntNode*> tempChildren;

	public:
	/*
	* Constructs an EntityParser with minimal data
	*/
	EntityParser() : root(NodeType::ROOT), fileWasCompressed(false),
		textAlloc(1000000), nodeAlloc(1000), childAlloc(1000)
	{
		// Cannot append a null character to a string? Hence const char* instead
		const char* rawText = "Version 7\nHierarchyVersion 1\0";
		textView = string_view(rawText, 29);

		parseContentsFile(&root);
		assertLastType(TokenType::END);
		root.populateParentRefs(nullptr);
		tempChildren.shrink_to_fit();
	}

	/*
	* Constructs an EntityParser containing fully parsed data from the given file
	* @param filepath .entites file to parse
	* @param debug_logParseTime If true, outputs execution time data
	* @throw runtime_error thrown when the file cannot be parsed
	*/
	EntityParser(const string& filepath, const bool debug_logParseTime = false) 
		: root(NodeType::ROOT),
		textAlloc(1000000), nodeAlloc(1000), childAlloc(1000)
	{
		auto timeStart = chrono::high_resolution_clock::now();
		ifstream file(filepath, ios_base::binary); // Binary mode 50% faster than 'in' mode, keeps CR chars
		if (!file.is_open())
			throw runtime_error("Could not open file");

		vector<char> rawText;
		char* decompressedText = nullptr;
		while (!file.eof())
		{
			char temp[2048];
			file.read(temp, 2048);
			rawText.insert(rawText.end(), temp, temp + file.gcount());
		}
		file.close();

		if (rawText.size() > 16 && (unsigned char)rawText[16] == 0x8C) // Oodle compression signature
		{
			fileWasCompressed = true;
			char* rawData = rawText.data();
			size_t decompressedSize = ((size_t*)rawData)[0];
			size_t compressedSize = ((size_t*)rawData)[1];
			decompressedText = new char[decompressedSize + 1]; // +1 for the null char
			
			if(!Oodle::DecompressBuffer(rawData + 16, compressedSize, decompressedText, decompressedSize))
				throw runtime_error("Could not decompress .entities file");
			decompressedText[decompressedSize] = '\0';
			textView = string_view(decompressedText, decompressedSize + 1);
		}
		else 
		{
			fileWasCompressed = false;
			rawText.push_back('\0');
			rawText.shrink_to_fit();
			textView = string_view(rawText.data(), rawText.size());
		}

		if (debug_logParseTime)
		{
			EntityLogger::logTimeStamps("File Read/Decompress Duration: ", timeStart, chrono::high_resolution_clock::now());
			timeStart = chrono::high_resolution_clock::now();
		}

		// Simpler char analysis algorithm that runs ~10 MS faster compared to old doubled runtime
		size_t counts[256] = {0};
		for(char c : textView)
			counts[c]++;

		// Distinguishes between the number of chars comprising actual identifiers/values versus syntax chars
		size_t charBufferSize = textView.length()
			- counts['\t'] - counts['\n'] - counts['\r']-  counts['}'] - counts['{'] - counts[';']
			- counts['=']
			- counts[' '] // This is an overestimate - string values will uncommonly contain spaces
			+ 100000;
		textAlloc.setActiveBuffer(charBufferSize);

		// For a well-formatted .entities file, we can get an exact count of how many nodes we must
		// allocate by subtracting the number of closing braces from the number of lines
		size_t numCloseBraces = counts['}'] > counts['\n'] ? counts['\n'] : counts['}']; // Prevents disastrous overflow
		size_t initialBufferSize = counts['\n'] - numCloseBraces + 1000;
		nodeAlloc.setActiveBuffer(initialBufferSize);
		childAlloc.setActiveBuffer(initialBufferSize);
													                 
		if (debug_logParseTime)
		{
			EntityLogger::logTimeStamps("Node Buffer Init Duration: ", timeStart, chrono::high_resolution_clock::now());
			timeStart = chrono::high_resolution_clock::now();
		}
		parseContentsFile(&root); // During construction we allow exceptions to propagate and the parser to be destroyed
		assertLastType(TokenType::END);
		root.populateParentRefs(nullptr);
		tempChildren.shrink_to_fit();
		if(fileWasCompressed)
			delete[] decompressedText;
		if (debug_logParseTime)
			EntityLogger::logTimeStamps("Parsing Duration: ", timeStart, chrono::high_resolution_clock::now());
	}

	void ClearUndoStack() {
		undoRedoStack.clear();
		undoIndex = 0;
	}

	ParseResult UndoRedo(bool undo)
	{
		ParseResult outcome;
		if ((undo && undoIndex == 0) || (!undo && undoIndex == undoRedoStack.size()))
		{
			outcome.success = false;
			outcome.errorMessage = "Nothing to " + undo ? "Undo" : "Redo";
			return outcome;
		}
		undoingOrRedoing = true;
		if(undo) undoIndex--;

		ParseCommand* order = &(undoRedoStack[undoIndex]);
		switch (order->type)
		{
			case CommandType::EDITTREE:
			int id = order->parentID;
			EntNode* parent = root.nodeFromPositionalID(id);
			outcome = EditTree(order->text, parent, order->insertionIndex, order->removalCount);
			break;
		}

		if(!undo) undoIndex++;
		undoingOrRedoing = false;
		return outcome;
	}

	/*
	* Parses a given string of text and replaces a pre-existing block of EntNodes
	* belonging to the same parent.
	* @param text Text to parse
	* @param parent Node whose children we're editing
	* @param insertionIndex Index to insert new nodes at
	* @param removeCount Number of nodes to delete, starting at insertionIndex
	*/
	ParseResult EditTree(string text, EntNode* parent, int insertionIndex, int removeCount)
	{
		ParseResult outcome;

		// We must ensure the parse is successful before modifying the existing tree
		EntNode tempRoot(NodeType::ROOT);
		initiateParse(text, &tempRoot, parent->TYPE, outcome);
		if(!outcome.success) return outcome;
		outcome.parent = parent;

		// -count because we are replacing existing nodes
		int newNumChildren = parent->childCount + tempRoot.childCount - removeCount;
		EntNode** newChildBuffer = childAlloc.reserveBlock(newNumChildren);

		// Copy everything into the new child buffer
		int inc = 0;
		for(inc = 0; inc < insertionIndex; inc++)
			newChildBuffer[inc] = parent->children[inc];
		for (int i = 0; i < tempRoot.childCount; i++)
		{
			newChildBuffer[inc++] = tempRoot.children[i];
			tempRoot.children[i]->populateParentRefs(parent);  // Set the parents of incoming nodes.
			outcome.addedNodes.push_back(tempRoot.children[i]);
		}	
		for(int i = insertionIndex + removeCount; i < parent->childCount; i++)
			newChildBuffer[inc++] = parent->children[i];

		// Build the reverse command
		ParseCommand* reverse;
		if (undoingOrRedoing) {
			reverse = &undoRedoStack[undoIndex]; // Will be index of command we must reverse
			*reverse = ParseCommand(); // Set to new to prevent building on old data
		}
		else {
			undoRedoStack.resize(undoIndex);
			if (undoRedoStack.size() == MAX_COMMAND_MEMORY)
			{
				undoRedoStack.erase(undoRedoStack.begin());
				undoIndex--;
			}
			undoRedoStack.emplace_back();
			reverse = &undoRedoStack[undoIndex++];
		}
		reverse->type = CommandType::EDITTREE;
		root.findPositionalID(parent, reverse->parentID);
		reverse->insertionIndex = insertionIndex;
		reverse->removalCount = tempRoot.childCount;

		// Now that we've built the new child buffer, deallocate all old data
		for (int i = 0; i < removeCount; i++)
		{
			EntNode* n = parent->children[insertionIndex + i];
			outcome.removedNodes.push_back(n);
			n->generateText(reverse->text);
			reverse->text.push_back('\n');
			freeNode(n);
		}
			
		childAlloc.freeBlock(parent->children, parent->childCount);
		childAlloc.freeBlock(tempRoot.children, tempRoot.childCount);

		// Finally, attach new data to the parent
		parent->childCount = newNumChildren;
		parent->children = newChildBuffer;
		return outcome;
	}

	/*
	* Converts the entirety of this parser's EntNode tree into text and saves
	* the result to a file
	* @param filepath File to write to
	* @param oodleCompress If true, compress the file
	* @param debug_logTime If true, output execution time data.
	*/
	void writeToFile(const string filepath, const bool oodleCompress, const bool debug_logTime = false) 
	{
		auto timeStart = chrono::high_resolution_clock::now();
		// 25% of time spent writing to output buffer, 75% on generateText
		
		string raw;
		for (int i = 0; i < root.childCount; i++)
		{
			root.children[i]->generateText(raw);
			raw.push_back('\n');
		}
		
		ofstream output(filepath, ios_base::binary);

		if (oodleCompress)
		{
			char* compressedData = new char[raw.length() + 65536];
			size_t compressedSize;
			if (Oodle::CompressBuffer(raw.data(), raw.length(), compressedData + 16, compressedSize)) {
				((size_t*)compressedData)[0] = raw.length();
				((size_t*)compressedData)[1] = compressedSize;
				output.write(compressedData, compressedSize + 16);
			}
			else {
				EntityLogger::logWarning("Failed to compress .entities file. Saving uncompressed version instead.");
				output << raw;
			}
			delete[] compressedData;
		}
		else output << raw;

		output.close();
		if (debug_logTime)
			EntityLogger::logTimeStamps("Writing Duration: ", timeStart, chrono::high_resolution_clock::now());
	}

	/*
	* ACCESSOR METHODS
	*/

	EntNode* getRoot() { return &root;}

	bool wasFileCompressed() {return fileWasCompressed;}

	/*
	* DEBUGGING METHODS
	*/
	void logAllocatorInfo(bool includeBlockList)
	{
		string msg = "EntNode Allocator\n=====\n";
		msg.append(nodeAlloc.toString(includeBlockList));
		msg.append("\nText Buffer Allocator\n=====\n");
		msg.append(textAlloc.toString(includeBlockList));
		msg.append("\nChild Buffer Allocator\n=====\n");
		msg.append(childAlloc.toString(includeBlockList));
		msg.push_back('\n');
		EntityLogger::log(msg);
	}

	private:
	/*
	* Creates an exception for a supplied parsing error
	* The returned exception should be thrown immediately
	*/
	runtime_error Error(string msg)
	{
		return runtime_error("Entities parsing failed (line " + to_string(currentLine) + "): " + msg);
	}


	/*
	* RECURSIVE PARSING FUNCTIONS
	*/

	void initiateParse(string &text, EntNode* tempRoot, NodeType parentType,
		ParseResult& results)
	{
		// Setup variables
		text.push_back('\0');
		textView = string_view(text);
		currentLine = 1;
		it = 0;
		start = 0;

		try 
		{
			switch (parentType)
			{
				case NodeType::ROOT:
				parseContentsFile(tempRoot);
				break;

				case NodeType::OBJECT_SIMPLE_ENTITY:
				parseContentsEntity(tempRoot);
				break;

				case NodeType::OBJECT_SIMPLE_LAYER:
				parseContentsLayer(tempRoot);
				break;

				case NodeType::OBJECT_ENTITYDEF: case NodeType::OBJECT_COMMON:
				parseContentsDefinition(tempRoot);
				break;
			}
			assertLastType(TokenType::END);
		}
		catch (runtime_error err)
		{
			// Deallocate and clear everything in the temporary vector
			for (EntNode* e : tempChildren)
				freeNode(e);
			tempChildren.clear();

			// Deallocate everything in the temporary root node,
			// then deallocate it's child buffer
			for (int i = 0; i < tempRoot->childCount; i++)
				freeNode(tempRoot->children[i]);
			childAlloc.freeBlock(tempRoot->children, tempRoot->childCount);

			results.errorLineNum = currentLine;
			results.errorMessage = err.what();
			results.success = false;
		}
		tempChildren.shrink_to_fit();
	}

	void parseContentsFile(EntNode* node) {
		EntNode* n = nullptr;
		size_t childrenStart = tempChildren.size();
		LABEL_LOOP:
		Tokenize();
		if (lastTokenType == TokenType::COMMENT)
		{
			tempChildren.push_back(setNodeNameOnly(NodeType::COMMENT));
			goto LABEL_LOOP;
		}
		if (lastTokenType != TokenType::IDENTIFIER)
		{
			setNodeChildren(node, childrenStart);
			return;
		}

		activeID = lastUniqueToken;
		TokenizeAdjustValue();
		switch (lastTokenType)
		{
			case TokenType::VALUE_NUMBER:
			case TokenType::VALUE_STRING: case TokenType::VALUE_KEYWORD:
			tempChildren.push_back(setNodeValue(NodeType::VALUE_FILE));
			break;

			case TokenType::BRACEOPEN:
			n = setNodeObj(NodeType::OBJECT_SIMPLE_ENTITY);
			tempChildren.push_back(n);
			parseContentsEntity(n);
			assertLastType(TokenType::BRACECLOSE);
			break;

			default:
			throw Error("Invalid token (File function)");
		}
		goto LABEL_LOOP;
	}

	void parseContentsEntity(EntNode* node) {
		EntNode* n = nullptr;
		size_t childrenStart = tempChildren.size();
		LABEL_LOOP:
		Tokenize();
		if (lastTokenType == TokenType::COMMENT)
		{
			tempChildren.push_back(setNodeNameOnly(NodeType::COMMENT));
			goto LABEL_LOOP;
		}
		if(lastTokenType != TokenType::IDENTIFIER)
		{
			setNodeChildren(node, childrenStart);
			return;
		}
		activeID = lastUniqueToken;
		TokenizeAdjustValue();
		switch (lastTokenType)
		{
			case TokenType::IDENTIFIER:
			assertIgnore(TokenType::BRACEOPEN);
			n = setNodeValue(NodeType::OBJECT_ENTITYDEF);
			tempChildren.push_back(n);
			parseContentsDefinition(n);
			assertLastType(TokenType::BRACECLOSE);
			break;

			case TokenType::BRACEOPEN:
			n = setNodeObj(NodeType::OBJECT_SIMPLE_LAYER);
			tempChildren.push_back(n);
			parseContentsLayer(n);
			assertLastType(TokenType::BRACECLOSE);
			break;

			case TokenType::ASSIGNMENT:
			TokenizeAdjustValue();
			if (lastTokenType < TokenType::VALUE_ANY)
				throw Error("Value expected (entity function)");
			tempChildren.push_back(setNodeValue(NodeType::VALUE_COMMON));
			assertIgnore(TokenType::TERMINAL);
			break;

			default:
			throw Error("Bad token type (Entity function");
		}
		goto LABEL_LOOP;
	}

	// same basic principle except we return for non-value
	void parseContentsLayer(EntNode* node) {
		size_t childrenStart = tempChildren.size();
		LABEL_LOOP:
		Tokenize();
		if (lastTokenType == TokenType::COMMENT)
		{
			tempChildren.push_back(setNodeNameOnly(NodeType::COMMENT));
			goto LABEL_LOOP;
		}
		if (lastTokenType != TokenType::VALUE_STRING)
		{
			setNodeChildren(node, childrenStart);
			return;
		}
		tempChildren.push_back(setNodeNameOnly(NodeType::VALUE_LAYER));
		goto LABEL_LOOP;
	}

	void parseContentsDefinition(EntNode* node) {
		EntNode *n = nullptr;
		size_t childrenStart = tempChildren.size();
		LABEL_LOOP:
		Tokenize();
		if (lastTokenType == TokenType::COMMENT)
		{
			tempChildren.push_back(setNodeNameOnly(NodeType::COMMENT));
			goto LABEL_LOOP;
		}
		if(lastTokenType != TokenType::IDENTIFIER)
		{
			setNodeChildren(node, childrenStart);
			return;
		}
		assertIgnore(TokenType::ASSIGNMENT);
		activeID = lastUniqueToken;
		TokenizeAdjustValue();
		switch (lastTokenType)
		{
			case TokenType::BRACEOPEN:
			n = setNodeObj(NodeType::OBJECT_COMMON);
			tempChildren.push_back(n);
			parseContentsDefinition(n);
			assertLastType(TokenType::BRACECLOSE);
			break;

			case TokenType::VALUE_NUMBER:
			case TokenType::VALUE_STRING: case TokenType::VALUE_KEYWORD:
			tempChildren.push_back(setNodeValue(NodeType::VALUE_COMMON));
			assertIgnore(TokenType::TERMINAL);
			break;

			default:
			throw Error("Bad token type (definition function)");
		}
		goto LABEL_LOOP;
	}


	/*
	* ALLOCATION / DEALLOCATION FUNCTIONS
	*/

	/*
	* To prevent bad stuff from happening, we should ensure node values are reset to
	* default when freeing them.
	* It would be pretty terrible to have to operate under the assumption that some data
	* is outdated when working with EntityNode instance methods
	*/
	void freeNode(EntNode* node)
	{
		// Free the allocated text block
		textAlloc.freeBlock(node->textPtr, node->nameLength + node->valLength);
		node->textPtr = nullptr;
		node->nameLength = 0;
		node->valLength = 0;

		// Free the node's children and the pointer block listing them
		if (node->childCount > 0)
		{
			for (int i = 0; i < node->childCount; i++)
				freeNode(node->children[i]);
			childAlloc.freeBlock(node->children, node->childCount);
			node->childCount = 0;
		}
		node->children = nullptr;
		node->parent = nullptr;

		// Free the node memory itself
		node->TYPE = NodeType::UNDESIGNATED;
		nodeAlloc.freeBlock(node, 1);
	}

	inline EntNode* setNodeObj(const NodeType p_type) 
	{
		char* buffer = textAlloc.reserveBlock(activeID.length());
		size_t i = 0;
		for (char c : activeID)
			buffer[i++] = c;

		EntNode* n = nodeAlloc.reserveBlock(1);
		n->TYPE = p_type;
		n->textPtr = buffer;
		n->nameLength = (int)activeID.length();
		return n;
	}

	inline EntNode* setNodeValue(const NodeType p_type)
	{
		size_t length = activeID.length() + lastUniqueToken.length();
		char* buffer = textAlloc.reserveBlock(length);
		size_t i = 0;
		for (char c : activeID)
			buffer[i++] = c;
		for (char c : lastUniqueToken)
			buffer[i++] = c;
		
		EntNode* n = nodeAlloc.reserveBlock(1);
		n->TYPE = p_type;
		n->textPtr = buffer;
		n->nameLength = (int)activeID.length();
		n->valLength = (int)lastUniqueToken.length();
		return n;
	}

	inline EntNode* setNodeNameOnly(const NodeType p_type)
	{
		char* buffer = textAlloc.reserveBlock(lastUniqueToken.length());
		size_t i = 0;
		for (char c : lastUniqueToken)
			buffer[i++] = c;

		EntNode* n = nodeAlloc.reserveBlock(1);
		n->TYPE = p_type;
		n->textPtr = buffer;
		n->nameLength = (int)lastUniqueToken.length();
		return n;
	}

	void setNodeChildren(EntNode* parent, const size_t startIndex)
	{
		size_t s = tempChildren.size();
		size_t childCount = s - startIndex;
		parent->childCount = (int)childCount;
		if (childCount == 0)
			return;
		parent->children = childAlloc.reserveBlock(childCount);
		for (size_t i = startIndex, j = 0; i < s; i++)
			parent->children[j++] = tempChildren[i];
		tempChildren.resize(startIndex);
	}

	/*
	* TOKENIZATION FUNCTIONS
	*/

	/*
	* Used when tokenizing
	* Faster than standard library isalpha(char) function
	* Appears to save anywhere between 100-200 MS wall clock time on ~1.2 million line file
	*/
	inline bool isLetter()
	{
		return ((unsigned int)(textView[it] | 32) - 97) < 26U;
	}

	/*
	* Throws an error if the last-parsed token is not of the required type
	*/
	inline void assertLastType(TokenType requiredType)
	{
		if (lastTokenType != requiredType)
			throw Error("Bad Token Type assertLast");
	}

	/*
	* Parses raw text for the next token
	* Throws an error if the token is not of the required type
	*/
	inline void assertIgnore(TokenType requiredType)
	{
		Tokenize();
		if(lastTokenType != requiredType)
			throw Error("Bad token type assertIgnore");
	}

	/*
	* Parses raw text for the next token
	* If it's an identifier, distinguish whether it's a true ID or special keyword value
	*/
	inline void TokenizeAdjustValue()
	{
		Tokenize();
		if (lastTokenType == TokenType::IDENTIFIER && lastUniqueToken == "true" || lastUniqueToken == "false" || lastUniqueToken == "NULL")
			lastTokenType = TokenType::VALUE_KEYWORD;
	}

	/*
	* Parses raw text for the next token
	*/
	void Tokenize() 
	{
		LABEL_TOKENIZE_START:
		switch (textView[it]) // Not auto-incrementing should eliminate unnecessary arithmetic operations
		{                     // at the cost of needing to manually it++ in additional areas
			case '\r':
			if(textView[++it] != '\n')
				throw Error("Expected line feed after carriage return");
			case '\n':
			currentLine++;
			case ' ': case '\t':
			it++;
			goto LABEL_TOKENIZE_START;

			case '\0': // Appending null character to end of string simplifies this function's bounds-checking
			lastTokenType = TokenType::END; // Do not it++ here to prevent out of bounds error
			return;

			case ';':
			lastTokenType = TokenType::TERMINAL;
			it++;
			return;

			case '{':
			lastTokenType = TokenType::BRACEOPEN;
			it++;
			return;

			case '}':
			lastTokenType = TokenType::BRACECLOSE;
			it++;
			return;

			case '=':
			lastTokenType = TokenType::ASSIGNMENT;
			it++;
			return;

			case '/':
			start = it;
			if(textView[++it] != '/')
				throw Error("Comments require two consecutive forward slashes.");
			LABEL_COMMENT_START:
			switch (textView[++it])
			{
				case '\n': case '\r': case '\0':
				lastTokenType = TokenType::COMMENT;
				lastUniqueToken = textView.substr(start, it - start);
				return;

				default:
				goto LABEL_COMMENT_START;
			}

			case '"':
			start = it;
			LABEL_STRING_START:
			switch (textView[++it])
			{
				case '"':
				lastTokenType = TokenType::VALUE_STRING;
				lastUniqueToken = textView.substr(start, ++it - start); // Increment past quote to set to next char
				return;

				case '\r': case '\n': case '\0': // Again, relies on string ending in null character
				throw Error("No end-quote to complete string literal");

				default:
				goto LABEL_STRING_START;
			}

			// TEMPORARY COMPROMISES:
			// 1. Negative sign can't be seperated from the number by whitespace
			// 2. Number token cannot begin with a period.
			case '-': //case '.':
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			{
				bool hasDot = false;
				start = it;
				LABEL_NUMBER_START:
				switch (textView[++it])
				{
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
					goto LABEL_NUMBER_START;

					case '.':
					if(hasDot)
						throw Error("Decimal numbers can't have multiple periods.");
					hasDot = true;
					goto LABEL_NUMBER_START;

					case 'e': case 'E': case'+': case '-':
					goto LABEL_NUMBER_START;

					default:
					lastTokenType = TokenType::VALUE_NUMBER;
					lastUniqueToken = textView.substr(start, it - start);
					return;
				}
				// Unimplemented remnants of previous version
				//if (hasDot && it - 1 == start)
				//	throw Error("Decimal numbers need at least one digit");
				//if (minusToken)
				//	lastUniqueToken = new string('-' + rawText.substr(start, it - start)); // Possibly optimize?
				//else
				//	lastUniqueToken = new string(rawText, start, it - start);
			}

			/*
			case '-':
				if (minusToken)
					throw Error("Cannot have consecutive minus signs.");
				minusToken = true;
				Tokenize();
				if (lastTokenType != TokenType::VALUE_NUMBER)
					throw Error("Numerical value expected after negative sign");
				minusToken = false;
				return;
			*/

			default:
			{
				if (!isLetter() && textView[it] != '_')
					throw Error("Unrecognized character");
				start = it;

				LABEL_ID_START:
				switch (textView[++it])
				{
					case '[':
					LABEL_ID_BRACKET_START:
					switch(textView[++it])
					{
						case '0': case '1': case '2': case '3': case '4':
						case '5': case '6': case '7': case '8': case '9':
						goto LABEL_ID_BRACKET_START;
						break;

						case ']':
						if(textView[it++ - 1] != '[')
							break;
						default:
						throw Error("Improper bracket usage in identifer");
					}
					break;

					default:
					if (isLetter() || isdigit(textView[it]) || textView[it] == '_')
						goto LABEL_ID_START;
					break;
				}
				lastUniqueToken = textView.substr(start, it - start);
				lastTokenType = TokenType::IDENTIFIER;
				return;
			}
		}
	};
};