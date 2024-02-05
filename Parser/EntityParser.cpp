#pragma warning(disable : 4996) // Deprecation errors
#include <fstream>
#include "Oodle.h"
#include "EntityLogger.h"
#include "EntityParser.h"

const std::string EntityParser::FILTER_NOLAYERS = "\"No Layers\"";

EntityParser::EntityParser() : root(NodeType::ROOT), fileWasCompressed(false),
	textAlloc(1000000), nodeAlloc(1000), childAlloc(30000)
{
	// Cannot append a null character to a string? Hence const char* instead
	const char* rawText = "Version 7\nHierarchyVersion 1\0";
	textView = std::string_view(rawText, 29);
	ch = (char*)rawText; // Probably not a very good practice

	parseContentsFile(&root);
	assertLastType(TokenType::END);
	tempChildren.shrink_to_fit();
}

EntityParser::EntityParser(const std::string& filepath, const bool debug_logParseTime)
	: root(NodeType::ROOT), textAlloc(1000000), nodeAlloc(1000), childAlloc(30000)
{
	auto timeStart = std::chrono::high_resolution_clock::now();
	std::ifstream file(filepath, std::ios_base::binary); // Binary mode 50% faster than 'in' mode, keeps CR chars
	if (!file.is_open())
		throw std::runtime_error("Could not open file");

	std::vector<char> rawText;
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

		if (!Oodle::DecompressBuffer(rawData + 16, compressedSize, decompressedText, decompressedSize))
			throw std::runtime_error("Could not decompress .entities file");
		decompressedText[decompressedSize] = '\0';
		textView = std::string_view(decompressedText, decompressedSize + 1);
		ch = decompressedText;
	}
	else
	{
		fileWasCompressed = false;
		rawText.push_back('\0');
		rawText.shrink_to_fit();
		textView = std::string_view(rawText.data(), rawText.size());
		ch = rawText.data();
	}

	if (debug_logParseTime)
	{
		EntityLogger::logTimeStamps("File Read/Decompress Duration: ", timeStart);
		timeStart = std::chrono::high_resolution_clock::now();
	}

	// Simpler char analysis algorithm that runs ~10 MS faster compared to old doubled runtime
	size_t counts[256] = { 0 };
	for (char c : textView)
		counts[c]++;

	// Distinguishes between the number of chars comprising actual identifiers/values versus syntax chars
	size_t charBufferSize = textView.length()
		- counts['\t'] - counts['\n'] - counts['\r'] - counts['}'] - counts['{'] - counts[';']
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
		EntityLogger::logTimeStamps("Node Buffer Init Duration: ", timeStart);
		timeStart = std::chrono::high_resolution_clock::now();
	}
	parseContentsFile(&root); // During construction we allow exceptions to propagate and the parser to be destroyed
	assertLastType(TokenType::END);
	tempChildren.shrink_to_fit();
	if (fileWasCompressed)
		delete[] decompressedText;
	if (debug_logParseTime)
		EntityLogger::logTimeStamps("Parsing Duration: ", timeStart);
}

ParseResult EntityParser::EditTree(std::string text, EntNode* parent, int insertionIndex, int removeCount, bool renumberLists, bool highlightNew)
{
	ParseResult outcome;

	// We must ensure the parse is successful before modifying the existing tree
	EntNode tempRoot(NodeType::ROOT);
	initiateParse(text, &tempRoot, parent, outcome);
	if(!outcome.success) return outcome;

	wxDataViewItemArray removedNodes;
	wxDataViewItemArray addedNodes;

	// Build the reverse command
	reverseGroup.emplace_back();
	ParseCommand& reverse = reverseGroup.back();
	reverse.type = CommandType::EDIT_TREE;
	root.findPositionalID(parent, reverse.parentID);
	reverse.insertionIndex = insertionIndex;
	reverse.removalCount = tempRoot.childCount;
	for (int i = 0; i < removeCount; i++) {
		EntNode* n = parent->children[insertionIndex + i];
		n->generateText(reverse.text);
		reverse.text.push_back('\n');

		// Deallocate deleted nodes
		removedNodes.Add(wxDataViewItem(n));
		freeNode(n);
	}

	// Prep. the new nodes to be fully integrated into the tree
	for (int i = 0; i < tempRoot.childCount; i++)
	{
		EntNode* n = tempRoot.children[i];
		n->parent = parent;
		addedNodes.Add(wxDataViewItem(n));
	}
	
	/* 
	* Common Steps for Each Branch:
	* 1. Determine if we should alert the model
	* 2. Move everything into the new child buffer
	* 3. Deallocate the old child buffers
	* 4. Assign the new child buffer/child count to the parent
	*/
	bool alertModel = true;
	int newNumChildren = parent->childCount + tempRoot.childCount - removeCount;
	if (parent == &root)
	{ 
		// If it's the root node, we always alert the model.
		// However, not every entity we're removing may be filtered in, so we must check them
		removedNodes.clear();
		for (int i = 0; i < removeCount; i++)
		{
			int index = insertionIndex + i;
			if(rootchild_filter[index])
				removedNodes.Add(wxDataViewItem(rootchild_buffer[index]));
		}

		if (newNumChildren > rootchild_capacity) 
		{ // Rare enough that we don't care about optimizing recopy versus bloating this branch with more code
			rootchild_capacity = newNumChildren + 1000;

			EntNode** newBuffer = new EntNode*[rootchild_capacity];
			bool* newFilter = new bool[rootchild_capacity];
			for (int i = 0, max = root.childCount; i < max; i++) {
				newBuffer[i] = rootchild_buffer[i];
				newFilter[i] = rootchild_filter[i];
			}
				
			delete[] rootchild_buffer;
			delete[] rootchild_filter;
			rootchild_buffer = newBuffer;
			rootchild_filter = newFilter;
			root.children = rootchild_buffer;
		}
		int difference = newNumChildren - root.childCount;
		int min = insertionIndex + removeCount;
		if (difference > 0) // Must shift to right to expand room
			for (int i = root.childCount - 1; i >= min; i--) {
				rootchild_buffer[i + difference] = rootchild_buffer[i];
				rootchild_filter[i + difference] = rootchild_filter[i];
			}
				
		if (difference < 0) // Must shift left to contract space
			for (int i = min, max = root.childCount; i < max; i++) {
				rootchild_buffer[i + difference] = rootchild_buffer[i];
				rootchild_filter[i + difference] = rootchild_filter[i];
			}

		for (int inc = insertionIndex, i = 0, max = tempRoot.childCount; i < max; inc++, i++) {
			rootchild_buffer[inc] = tempRoot.children[i];
			rootchild_filter[inc] = true; // New nodes will be filtered-in and shown to the user
			// Possible future development: first test whether they pass the filters?
		}

		// Deallocate old data
		childAlloc.freeBlock(tempRoot.children, tempRoot.childCount);

		// Attach new data
		root.childCount = newNumChildren;
	}
	else 
	{
		// For a non-root parent, we should only alert the model if it's
		// entity ancestor is filtered in
		EntNode* entity = parent->getEntity();
		int entityIndex = root.getChildIndex(entity);
		if(!rootchild_filter[entityIndex]) alertModel = false;

		EntNode** newChildBuffer = childAlloc.reserveBlock(newNumChildren);

		// Copy everything into the new child buffer
		int inc = 0;
		for(inc = 0; inc < insertionIndex; inc++)
			newChildBuffer[inc] = parent->children[inc];
		for (int i = 0; i < tempRoot.childCount; i++)
			newChildBuffer[inc++] = tempRoot.children[i];
		for(int i = insertionIndex + removeCount; i < parent->childCount; i++)
			newChildBuffer[inc++] = parent->children[i];

		// Deallocate child buffers
		childAlloc.freeBlock(parent->children, parent->childCount);
		childAlloc.freeBlock(tempRoot.children, tempRoot.childCount);

		// Finally, attach new data to the parent
		parent->childCount = newNumChildren;
		parent->children = newChildBuffer;
	}

	// Must update model AFTER node is given it's new child data
	if (alertModel)
	{
		wxDataViewItem parentItem(parent);
		ItemsDeleted(parentItem, removedNodes);
		ItemsAdded(parentItem, addedNodes);
		if (highlightNew)
			for (wxDataViewItem& i : addedNodes) // Must use Select() instead of SetSelections()
				view->Select(i);                // because the latter deselects everything else
	}

	if (renumberLists)
	{
		for (wxDataViewItem& n : addedNodes)
			fixListNumberings((EntNode*)n.GetID(), true, false);
		if(parent != &root) // Don't waste time reordering the root children, there shouldn't be a list there
			fixListNumberings(parent, false, true);
	}
	return outcome;
}

void EntityParser::EditText(const std::string& text, EntNode* node, int nameLength, bool highlight)
{
	// Construct reverse command
	reverseGroup.emplace_back();
	ParseCommand& reverse = reverseGroup.back();
	reverse.type = CommandType::EDIT_TEXT;
	reverse.text = std::string(node->textPtr, node->nameLength + node->valLength);
	reverse.insertionIndex = node->nameLength;
	root.findPositionalID(node, reverse.parentID);

	// Create new buffer
	char* newBuffer = textAlloc.reserveBlock(text.length());
	int i = 0;
	for (char c : text)
		newBuffer[i++] = c;

	// Free old data
	textAlloc.freeBlock(node->textPtr, node->nameLength + node->valLength);

	// Assign new data to node
	node->textPtr = newBuffer;
	node->nameLength = nameLength;
	node->valLength = (int)text.length() - nameLength;

	// Determine whether to alert model
	bool alertModel = true;
	EntNode* entity = node->getEntity(); // Todo: add safeguards so node can't be the root
	int entityIndex = root.getChildIndex(entity);
	if(!rootchild_filter[entityIndex]) alertModel = false;

	// Alert model
	if (alertModel)
	{
		wxDataViewItem item(node);
		ItemChanged(item);
		if (highlight)
			view->Select(item);
	}
}

/* 
* Moves a node's child to a different index in it's buffer 
* Nodes inbetween the two indices are shifted up/down to fill the node's old slot
*/
void EntityParser::EditPosition(EntNode* parent, int childIndex, int insertionIndex, bool highlight)
{
	// Construct reverse command
	reverseGroup.emplace_back();
	ParseCommand& reverse = reverseGroup.back();
	reverse.type = CommandType::EDIT_POSITION;
	reverse.insertionIndex = childIndex; // Removal count is interpreted as the child index, and 
	reverse.removalCount = insertionIndex;
	root.findPositionalID(parent, reverse.parentID);

	// Assemble data
	EntNode** buffer = parent->children;
	EntNode* child = buffer[childIndex];
	bool parentIsRoot = parent == &root;
	bool alertModel = true;
	{
		/*
		* We should alert the model when:
		* Parent is Root: Only if the node is filtered in
		* Parent isn't Root: Only if the entity ancestor is filtered in
		* Root will never be the node we're moving, so that simplifies our boolean logic
		*/
		EntNode* entity = child->getEntity();
		int entityIndex = root.getChildIndex(entity);
		if(!rootchild_filter[entityIndex]) alertModel = false;
	}

	// Shift nodes around
	if (childIndex < insertionIndex)  { // Shift middle elements up
		for (int i = childIndex; i < insertionIndex; i++)
			buffer[i] = buffer[i + 1];
		if(parentIsRoot) 
			for (int i = childIndex; i < insertionIndex; i++)
				rootchild_filter[i] = rootchild_filter[i+1];
	}

	else  { // Shift middle elements down
		for (int i = childIndex; i > insertionIndex; i--)
			buffer[i] = buffer[i - 1];
		if(parentIsRoot)
			for(int i = childIndex; i > insertionIndex; i--)
				rootchild_filter[i] = rootchild_filter[i-1];
	}
	buffer[insertionIndex] = child;
	if(parentIsRoot)
		rootchild_filter[insertionIndex] = alertModel;
	
	// Notify model
	if (alertModel)
	{
		wxDataViewItem parentItem(parent);
		wxDataViewItem childItem(child);
		ItemDeleted(parentItem, childItem);
		ItemAdded(parentItem, childItem);
		if(highlight)
			view->Select(childItem);
	}
}

void EntityParser::fixListNumberings(EntNode* parent, bool recursive, bool highlight)
{
	int listItems = 0;
	for (int i = 0; i < parent->childCount; i++) 
	{
		EntNode* current = parent->children[i];
		if(current->childCount > 0 && recursive)
			fixListNumberings(current, true, highlight);

		std::string_view name = current->getName();
		if(!name._Starts_with("item["))
			continue;
		listItems++;

		std::string newName = "item[" + std::to_string(listItems - 1) + ']';
		if(name == newName) continue;

		int length = (int)newName.length();
		newName.append(current->getValue());

		EditText(newName, current, length, highlight);
	}

	EntNode& numNode = (*parent)["num"];
	if (&numNode == EntNode::SEARCH_404)
	{
		if (listItems == 0) return;
		EditTree("num = " + std::to_string(listItems) + ';', parent, 0, 0, false, highlight);
	}
	else {
		std::string newVal = std::to_string(listItems);
		if(numNode.getValue() == newVal) return;

		EditText("num" + newVal, &numNode, 3, highlight);
	}
}

void EntityParser::PushGroupCommand() {
	if (reverseGroup.size() == 0) return;

	if (redoIndex < history.size())
	{
		size_t index = redoIndex, max = history.size();
		while (index < max)
			if (history[index++].lastInGroup)
				commandCount--;
		history.resize(redoIndex);
	}

	if (commandCount == MAX_COMMAND_MEMORY)
	{
		// Assumes a max command history > 1
		// Find start of the second-to-last undo command,
		// and erase everything before it.
		size_t index = 1;
		while (!history[index].lastInGroup)
			index++;
		auto first = history.begin();
		history.erase(first, first + index);
		commandCount--;
	}

	reverseGroup[0].lastInGroup = true;
	for (ParseCommand& p : reverseGroup)
		history.push_back(p);
	redoIndex = (int)history.size();
	reverseGroup.clear();
	commandCount++;
}

void EntityParser::CancelGroupCommand()
{
	for (int i = (int)reverseGroup.size() - 1; i > -1; i--) // Should(?) underflow to -1
		ExecuteCommand(reverseGroup[i]);
	reverseGroup.clear();
}

void EntityParser::ClearHistory()
{
	history.clear();
	redoIndex = 0;
	commandCount = 0;
}

void EntityParser::ExecuteCommand(ParseCommand& cmd)
{
	int id = cmd.parentID;
	EntNode* node = root.nodeFromPositionalID(id);
	switch (cmd.type)
	{
		case CommandType::EDIT_TREE:
		EditTree(cmd.text, node, cmd.insertionIndex, cmd.removalCount, false, true);
		break;

		case CommandType::EDIT_TEXT:
		EditText(cmd.text, node, cmd.insertionIndex, true); // TODO: GENERALIZE PARM NAMES?
		break;

		case CommandType::EDIT_POSITION:
		EditPosition(node, cmd.removalCount, cmd.insertionIndex, true);
		break;
	}
}

bool EntityParser::Undo()
{
	// Nothing to undo
	if (redoIndex == 0) return false;

	int index = redoIndex - 1;
	do ExecuteCommand(history[index]);
	while (!history[index--].lastInGroup);

	redoIndex = ++index;
	reverseGroup[0].lastInGroup = true;
	for (int i = (int)reverseGroup.size() - 1; i > -1; i--)
		history[index++] = reverseGroup[i];

	reverseGroup.clear();
	return true;
}

bool EntityParser::Redo()
{
	// Nothing to redo
	if (redoIndex == history.size()) return false;

	int index = redoIndex;
	do ExecuteCommand(history[index]);
	while (!history[index++].lastInGroup);

	reverseGroup[0].lastInGroup = true;
	for (ParseCommand& p : reverseGroup)
		history[redoIndex++] = p;

	reverseGroup.clear();
	return true;
}

EntNode* EntityParser::getRoot() { return &root; }

bool EntityParser::wasFileCompressed() { return fileWasCompressed; }

void EntityParser::logAllocatorInfo(bool includeBlockList, bool logToLogger, bool logToFile, const std::string filepath)
{
	std::string msg = "EntNode Allocator\n=====\n";
	msg.append(nodeAlloc.toString(includeBlockList));
	msg.append("\nText Buffer Allocator\n=====\n");
	msg.append(textAlloc.toString(includeBlockList));
	msg.append("\nChild Buffer Allocator\n=====\n");
	msg.append(childAlloc.toString(includeBlockList));
	msg.append("\nRoot Child Buffer: ");
	msg.append(std::to_string(root.childCount));
	msg.append(" / ");
	msg.append(std::to_string(rootchild_capacity));
	msg.append(" Slots Filled");
	msg.push_back('\n');

	if (logToLogger)
		EntityLogger::log(msg);

	if(logToFile)
	{
		std::ofstream file(filepath, std::ios_base::binary);
		file.write(msg.data(), msg.length());
		file.close();
	}
}

std::runtime_error EntityParser::Error(std::string msg)
{
	const char *min = textView.data();
	for (char* inc = ch; inc >= min; inc--)
		if (*inc == '\n') errorLine++;
	return std::runtime_error("Entities parsing failed (line " + std::to_string(errorLine) + "): " + msg);
}


void EntityParser::initiateParse(std::string &text, EntNode* tempRoot, EntNode* parent,
	ParseResult& results)
{
	// Setup variables
	text.push_back('\0');
	textView = std::string_view(text);
	errorLine = 1;
	ch = text.data();
	first = ch;

	try 
	{
		switch (parent->TYPE)
		{
			case NodeType::ROOT:
			parseContentsFile(tempRoot);
			break;

			case NodeType::OBJECT_SIMPLE:
			if (parent->parent == &root)
				parseContentsEntity(tempRoot);
			else parseContentsLayer(tempRoot);
			break;

			case NodeType::OBJECT_ENTITYDEF: case NodeType::OBJECT_COMMON:
			parseContentsDefinition(tempRoot);
			break;
		}
		assertLastType(TokenType::END);
	}
	catch (std::runtime_error err)
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

		results.errorLineNum = errorLine;
		results.errorMessage = err.what();
		results.success = false;
	}
	tempChildren.shrink_to_fit();
}

void EntityParser::parseContentsFile(EntNode* node) {
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
		if(node != &root)
			setNodeChildren(node, childrenStart);
		else {
			// At this point, only the root children should
			// be in the temporary vector
			root.childCount = (int)tempChildren.size();
			rootchild_capacity = root.childCount + 1000;
			rootchild_filter = new bool[rootchild_capacity];
			rootchild_buffer = new EntNode*[rootchild_capacity];
			root.children = rootchild_buffer;
			
			EntNode* rootAddr = &root;
			for (int i = 0, max = root.childCount; i < max; i++) {
				rootchild_buffer[i] = tempChildren[i];
				rootchild_buffer[i]->parent = rootAddr;
				rootchild_filter[i] = true;
			}
				
			tempChildren.clear();
		}
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
		n = setNodeObj(NodeType::OBJECT_SIMPLE);
		tempChildren.push_back(n);
		parseContentsEntity(n);
		assertLastType(TokenType::BRACECLOSE);
		break;

		default:
		throw Error("Invalid token (File function)");
	}
	goto LABEL_LOOP;
}

void EntityParser::parseContentsEntity(EntNode* node) {
	EntNode* n = nullptr;
	size_t childrenStart = tempChildren.size();
	LABEL_LOOP:
	Tokenize();
	if (lastTokenType == TokenType::COMMENT)
	{
		tempChildren.push_back(setNodeNameOnly(NodeType::COMMENT));
		goto LABEL_LOOP;
	}
	if (lastTokenType == TokenType::VALUE_STRING) // Need this specifically for pvp_darkmetal
	{
		activeID = lastUniqueToken;
		assertIgnore(TokenType::ASSIGNMENT);
		assertIgnore(TokenType::VALUE_STRING);
		tempChildren.push_back(setNodeValue(NodeType::VALUE_DARKMETAL));
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
		n = setNodeObj(NodeType::OBJECT_SIMPLE);
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

void EntityParser::parseContentsLayer(EntNode* node) {
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

void EntityParser::parseContentsDefinition(EntNode* node)
{
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

void EntityParser::freeNode(EntNode* node)
{
	// Free the allocated text block
	textAlloc.freeBlock(node->textPtr, node->nameLength + node->valLength);

	// Free the node's children and the pointer block listing them
	if (node->childCount > 0)
	{
		for (int i = 0; i < node->childCount; i++)
			freeNode(node->children[i]);
		childAlloc.freeBlock(node->children, node->childCount);
	}

	/*
	* We should ensure node values are reset to default when freeing them.
	* Otherwise we must operate under the assumption that some data is inaccurate
	* when working with EntityNode instance methods.
	*/
	new (node) EntNode;
	nodeAlloc.freeBlock(node, 1);
}

EntNode* EntityParser::setNodeObj(const NodeType p_type)
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

EntNode* EntityParser::setNodeValue(const NodeType p_type)
{
	size_t length = activeID.length() + lastUniqueToken.length();
	char* buffer = textAlloc.reserveBlock(length);
	char* inc = buffer;

	for (char c : activeID)
		*inc++ = c;
	for (char c : lastUniqueToken)
		*inc++ = c;

	EntNode* n = nodeAlloc.reserveBlock(1);
	n->TYPE = p_type;
	n->textPtr = buffer;
	n->nameLength = (int)activeID.length();
	n->valLength = (int)lastUniqueToken.length();
	return n;
}

EntNode* EntityParser::setNodeNameOnly(const NodeType p_type)
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

void EntityParser::setNodeChildren(EntNode* parent, const size_t startIndex)
{
	size_t s = tempChildren.size();
	size_t childCount = s - startIndex;
	parent->childCount = (int)childCount;
	parent->children = childAlloc.reserveBlock(childCount);

	// Fill child buffer, assign parent values to children
	EntNode **childrenPtr = parent->children, 
			**max = childrenPtr + childCount,
			**tempPtr = tempChildren.data() + startIndex;
	while (childrenPtr < max) { 
		(*tempPtr)->parent = parent;
		*childrenPtr++ = *tempPtr++;
	}
	tempChildren.resize(startIndex);
}

bool EntityParser::isLetter()
{
	return ((unsigned int)(*ch | 32) - 97) < 26U;
}

bool EntityParser::isNum()
{
	return ((unsigned)*ch - '0') < 10u;
}

void EntityParser::assertLastType(TokenType requiredType)
{
	if (lastTokenType != requiredType)
		throw Error("Bad Token Type assertLast");
}

void EntityParser::assertIgnore(TokenType requiredType)
{
	Tokenize();
	if (lastTokenType != requiredType)
		throw Error("Bad token type assertIgnore");
}

void EntityParser::TokenizeAdjustValue()
{
	Tokenize();
	if (lastTokenType == TokenType::IDENTIFIER && lastUniqueToken == "true" || lastUniqueToken == "false" || lastUniqueToken == "NULL")
		lastTokenType = TokenType::VALUE_KEYWORD;
}

void EntityParser::Tokenize() 
{
	LABEL_TOKENIZE_START:
	switch (*ch) // Not auto-incrementing should eliminate unnecessary arithmetic operations
	{                     // at the cost of needing to manually it++ in additional areas
		case '\r':
		if(*++ch != '\n')
			throw Error("Expected line feed after carriage return");
		case '\n': case ' ': case '\t':
		ch++;
		goto LABEL_TOKENIZE_START;

		case '\0': // Appending null character to end of string simplifies this function's bounds-checking
		lastTokenType = TokenType::END; // Do not it++ here to prevent out of bounds error
		return;

		case ';':
		lastTokenType = TokenType::TERMINAL;
		ch++;
		return;

		case '{':
		lastTokenType = TokenType::BRACEOPEN;
		ch++;
		return;

		case '}':
		lastTokenType = TokenType::BRACECLOSE;
		ch++;
		return;

		case '=':
		lastTokenType = TokenType::ASSIGNMENT;
		ch++;
		return;

		case '/':
		first = ch;
		if (*++ch == '/') {
			LABEL_COMMENT_START:
			switch (*++ch)
			{
				case '\n': case '\r': case '\0':
				lastTokenType = TokenType::COMMENT;
				lastUniqueToken = std::string_view(first, (size_t)(ch - first));
				return;

				default:
				goto LABEL_COMMENT_START;
			}
		}
		else if (*ch == '*') { // Don't increment here
			LABEL_COMMENT_MULTILINE_START:
			switch (*++ch) 
			{
				case '\0':
				throw Error("No end to multiline comment");

				case '*':
				if (*(ch + 1) != '/')
					goto LABEL_COMMENT_MULTILINE_START;
				ch += 2;
				lastTokenType = TokenType::COMMENT;
				lastUniqueToken = std::string_view(first, (size_t)(ch - first));
				return;

				default:
				goto LABEL_COMMENT_MULTILINE_START;
			}
		}
		else throw Error("Invalid Comment Syntax");

		case '"':
		first = ch;
		LABEL_STRING_START:
		switch (*++ch)
		{
			case '"':
			lastTokenType = TokenType::VALUE_STRING;
			lastUniqueToken = std::string_view(first, (size_t)(++ch - first)); // Increment past quote to set to next char
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
			first = ch;
			LABEL_NUMBER_START:
			switch (*++ch)
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
				lastUniqueToken = std::string_view(first, (size_t)(ch - first));
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
			if (!isLetter() && *ch != '_')
				throw Error("Unrecognized character");
			first = ch;

			LABEL_ID_START:
			switch (*++ch)
			{
				case '[':
				LABEL_ID_BRACKET_START:
				switch(*++ch)
				{
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
					goto LABEL_ID_BRACKET_START;
					break;

					case ']':
					if(*(ch++ - 1) != '[')
						break;
					default:
					throw Error("Improper bracket usage in identifer");
				}
				break;

				default:
				if (isLetter() || isNum() || *ch == '_')
					goto LABEL_ID_START;
				break;
			}
			lastUniqueToken = std::string_view(first, (size_t)(ch - first));
			lastTokenType = TokenType::IDENTIFIER;
			return;
		}
	}
};

void EntityParser::FilteredSearch(const std::string& key, bool backwards, bool caseSensitive, bool exactLength) 
{
	EntNode* startAfter = (EntNode*)view->GetCurrentItem().GetID();
	if(startAfter == nullptr)
		startAfter = &root;
	
	EntNode* firstResult = nullptr; // Use this to prevent infinite loops from rejection of filtered results
	EntNode* result;
	while (true)
	{
		/*
		* TODO: Ideally, we shouldn't be searching through filtered out nodes in the first place. If a massive
		* amount of nodes are filtered out on large files, search can take significant time to complete
		*/
		if (backwards)
			result = startAfter->searchUpwards(key, caseSensitive, exactLength);
		else result = startAfter->searchDownwards(key, caseSensitive, exactLength, nullptr);

		if (result == EntNode::SEARCH_404 || result == firstResult) {
			EntityLogger::log("Could not find key");
			return;
		}
		//wxLogMessage("%s", result->getNameWX());

		// Root should never have text, so this should be safe
		EntNode* entity = result->getEntity(); 
		int entityIndex = root.getChildIndex(entity);

		if (rootchild_filter[entityIndex]) {
			wxDataViewItem item(result);
			view->UnselectAll();
			view->Select(item);
			if(result->childCount > 0)
				view->Expand(item);
			view->EnsureVisible(item);
			//wxLogMessage("Found");
			return;
		}

		if(firstResult == nullptr) 
			firstResult = result;
		startAfter = result;
	}
}

void EntityParser::refreshFilterMenus(wxCheckListBox* layerMenu, wxCheckListBox* classMenu, wxCheckListBox* inheritMenu)
{
	std::set<std::string_view> newLayers;
	std::set<std::string_view> newClasses;
	std::set<std::string_view> newInherits;

	EntNode** children = rootchild_buffer;
	int childCount = root.childCount;

	newLayers.insert(std::string_view(FILTER_NOLAYERS.data() + 1, FILTER_NOLAYERS.length() - 2));
	for (int i = 0; i < childCount; i++)
	{
		EntNode* current = children[i];
		EntNode& defNode = (*current)["entityDef"];
		{
			EntNode& classNode = defNode["class"];
			if (classNode.ValueLength() > 0)
				newClasses.insert(classNode.getValueUQ());
		}
		{
			EntNode& inheritNode = defNode["inherit"];
			if (inheritNode.ValueLength() > 0)
				newInherits.insert(inheritNode.getValueUQ());
		}
		{
			EntNode& layerNode = (*current)["layers"];
			if (layerNode.getChildCount() > 0)
			{
				EntNode** layerBuffer = layerNode.getChildBuffer();
				int layerCount = layerNode.getChildCount();
				for (int currentLayer = 0; currentLayer < layerCount; currentLayer++)
					newLayers.insert(layerBuffer[currentLayer]->getNameUQ());
			}
		}
	}

	for (std::string_view view : newLayers)
	{
		wxString s(view.data(), view.length());
		if(layerMenu->FindString(s, true) < 0)
			layerMenu->Append(s);
	}

	for (std::string_view view : newClasses)
	{
		wxString s(view.data(), view.length());
		if (classMenu->FindString(s, true) < 0)
			classMenu->Append(s);
	}

	for (std::string_view view : newInherits)
	{
		wxString s(view.data(), view.length());
		if (inheritMenu->FindString(s, true) < 0)
			inheritMenu->Append(s);
	}
}

void EntityParser::SetFilters(wxCheckListBox* layerMenu, wxCheckListBox* classMenu, wxCheckListBox* inheritMenu,
	bool filterSpawnPosition, Sphere spawnSphere, wxCheckListBox* textMenu, bool caseSensitiveText)
{
	std::set<std::string> layerFilters;
	std::set<std::string> classFilters;
	std::set<std::string> inheritFilters;
	std::vector<std::string> textFilters;

	for (int i = 0, max = layerMenu->GetCount(); i < max; i++)
		if(layerMenu->IsChecked(i))
			layerFilters.insert('"' + std::string(layerMenu->GetString(i)) + '"');

	for (int i = 0, max = classMenu->GetCount(); i < max; i++)
		if (classMenu->IsChecked(i))
			classFilters.insert('"' + std::string(classMenu->GetString(i)) + '"');

	for (int i = 0, max = inheritMenu->GetCount(); i < max; i++)
		if (inheritMenu->IsChecked(i))
			inheritFilters.insert('"' + std::string(inheritMenu->GetString(i)) + '"');

	for(int i = 0, max = textMenu->GetCount(); i < max; i++)
		if(textMenu->IsChecked(i))
			textFilters.push_back(std::string(textMenu->GetString(i)));

	// MOVED FROM GetChildren - Apply the filters
	auto start = std::chrono::high_resolution_clock::now();

	bool filterByClass = classFilters.size() > 0;
	bool filterByInherit = inheritFilters.size() > 0;
	bool filterByLayer = layerFilters.size() > 0;
	bool noLayerFilter = layerFilters.count(FILTER_NOLAYERS) > 0;
					
	size_t numTextFilters = textFilters.size();
	bool filterByText = numTextFilters > 0;
	//string textBuffer;

	// Eliminates need to take square root in every distance calculation
	float maxR2 = spawnSphere.r * spawnSphere.r;

	int childCount = root.childCount;
	EntNode** childBuffer = rootchild_buffer;
	for (int i = 0; i < childCount; i++)
	{
		EntNode* entity = childBuffer[i];
		EntNode& entityDef = (*entity)["entityDef"];
		if (filterByClass)
		{
			std::string_view classVal = entityDef["class"].getValue();
			if (classFilters.count(std::string(classVal)) < 1) // TODO: OPTIMIZE ACCESS CASTING
			{
				rootchild_filter[i] = false;
				continue;
			}
				
		}

		if (filterByInherit)
		{
			std::string_view inheritVal = entityDef["inherit"].getValue();
			if (inheritFilters.count(std::string(inheritVal)) < 1) // TODO: OPTIMIZE CASTING
			{
				rootchild_filter[i] = false;
				continue;
			}
		}

		if (filterByLayer)
		{
			bool hasLayer = false;
			EntNode& layerNode = (*entity)["layers"];
			if (layerNode.getChildCount() == 0 && noLayerFilter)
				hasLayer = true; // Search_404 also implies 0 layers

			EntNode** layers = layerNode.getChildBuffer();
			int layerCount = layerNode.getChildCount();
			for (int currentLayer = 0; currentLayer < layerCount; currentLayer++)
			{
				std::string_view l = layers[currentLayer]->getName();
				if (layerFilters.count(std::string(l)) > 0)
				{
					hasLayer = true;
					break;
				}
			}
			if (!hasLayer)
			{
				rootchild_filter[i] = false;
				continue;
			}
		}

		if (filterSpawnPosition)
		{
			// If a variable is undefined, we assume default value of 0
			// If spawnPosition is undefined, we assume (0, 0, 0) instead of excluding
			EntNode& positionNode = entityDef["edit"]["spawnPosition"];
			//if(&positionNode == EntNode::SEARCH_404)
			//	continue;
			EntNode& xNode = positionNode["x"];
			EntNode& yNode = positionNode["y"];
			EntNode& zNode = positionNode["z"];

			float x = 0, y = 0, z = 0;
			try {
				// Need comparisons to distinguish between actual formatting exception
				// and exception from trying to convert "" to a float
				if(&xNode != EntNode::SEARCH_404)
					x = std::stof(std::string(xNode.getValue()));

				if (&yNode != EntNode::SEARCH_404)
					y = std::stof(std::string(yNode.getValue()));

				if (&zNode != EntNode::SEARCH_404)
					z = std::stof(std::string(zNode.getValue()));
			}
			catch (std::exception) {
				rootchild_filter[i] = false;
				continue;
			}

			float deltaX = x - spawnSphere.x,
					deltaY = y - spawnSphere.y,
					deltaZ = z - spawnSphere.z;
			float distance2 = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

			if(distance2 > maxR2)
			{
				rootchild_filter[i] = false;
				continue;
			}
		}

		if (filterByText)
		{
			//textBuffer.clear();
			//childBuffer[i]->generateText(textBuffer);
					
			bool containsText = false;
			for (const std::string& key : textFilters)
				if (childBuffer[i]->searchDownwardsLocal(key, caseSensitiveText, false) != EntNode::SEARCH_404)
				{
					containsText = true;
					break;
				}
			//for (size_t filter = 0; filter < numTextFilters; filter++)
			//	if (textBuffer.find(textFilters[filter]) != string::npos)
			//	{
			//		containsText = true;
			//		break;
			//	}
			if(!containsText)
			{
				rootchild_filter[i] = false;
				continue;
			}
		}

		// Node has passed all filters and should be included in the filtered tree
		rootchild_filter[i] = true;
	}
	EntityLogger::logTimeStamps("Time to Filter: ", start);
}