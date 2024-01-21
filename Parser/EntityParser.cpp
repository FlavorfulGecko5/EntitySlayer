#pragma warning(disable : 4996) // Deprecation errors
#include <fstream>
#include "Oodle.h"
#include "EntityLogger.h"
//#include "EntityParser.h"
#include "EntityModel.h"


EntityParser::EntityParser() : root(NodeType::ROOT), fileWasCompressed(false),
	textAlloc(1000000), nodeAlloc(1000), childAlloc(30000)
{
	// Cannot append a null character to a string? Hence const char* instead
	const char* rawText = "Version 7\nHierarchyVersion 1\0";
	textView = std::string_view(rawText, 29);
	ch = (char*)rawText; // Probably not a very good practice

	parseContentsFile(&root);
	assertLastType(TokenType::END);
	root.populateParentRefs(nullptr);
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
	root.populateParentRefs(nullptr);
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
	initiateParse(text, &tempRoot, parent->TYPE, outcome);
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
		n->populateParentRefs(parent);
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
		model->ItemsDeleted(parentItem, removedNodes);
		model->ItemsAdded(parentItem, addedNodes);
		if (highlightNew)
			for (wxDataViewItem& i : addedNodes) // Must use Select() instead of SetSelections()
				view->Select(i);                // because the latter deselects everything else
	}

	if (renumberLists)
	{
		for (wxDataViewItem& n : addedNodes)
			fixListNumberings((EntNode*)n.GetID(), true, false);
		if(parent != &root && parent->TYPE != NodeType::OBJECT_SIMPLE_LAYER) // Are these safeguards necessary? Are more needed?
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
		model->ItemChanged(item);
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
		model->ItemDeleted(parentItem, childItem);
		model->ItemAdded(parentItem, childItem);
		if(highlight)
			view->Select(childItem);
	}
}

void EntityParser::EditName(std::string text, EntNode* node)
{
}

void EntityParser::EditValue(std::string text, EntNode* node)
{

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
	return std::runtime_error("Entities parsing failed (line " + std::to_string(currentLine) + "): " + msg);
}


void EntityParser::initiateParse(std::string &text, EntNode* tempRoot, NodeType parentType,
	ParseResult& results)
{
	// Setup variables
	text.push_back('\0');
	textView = std::string_view(text);
	currentLine = 1;
	ch = text.data();
	first = ch;

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

		results.errorLineNum = currentLine;
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
			
			for (int i = 0, max = root.childCount; i < max; i++) {
				rootchild_buffer[i] = tempChildren[i];
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
	if (childCount == 0)
		return;
	parent->children = childAlloc.reserveBlock(childCount);
	for (size_t i = startIndex, j = 0; i < s; i++)
		parent->children[j++] = tempChildren[i];
	tempChildren.resize(startIndex);
}

bool EntityParser::isLetter()
{
	return ((unsigned int)(*ch | 32) - 97) < 26U;
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
		case '\n':
		currentLine++;
		case ' ': case '\t':
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
		if(*++ch != '/')
			throw Error("Comments require two consecutive forward slashes.");
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
				if (isLetter() || isdigit(*ch) || *ch == '_')
					goto LABEL_ID_START;
				break;
			}
			lastUniqueToken = std::string_view(first, (size_t)(ch - first));
			lastTokenType = TokenType::IDENTIFIER;
			return;
		}
	}
};