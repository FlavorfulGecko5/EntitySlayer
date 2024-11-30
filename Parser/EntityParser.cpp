#pragma warning(disable : 4996) // Deprecation errors
#include <fstream>
#include "Oodle.h"
#include "EntityLogger.h"
#include "EntityParser.h"
#include "EntityEditor.h"

const std::string EntityParser::FILTER_NOLAYERS = "\"No Layers\"";

EntityParser::EntityParser() : fileWasCompressed(false), PARSEMODE(ParsingMode::ENTITIES)
{
	// Cannot append a null character to a string? Hence const char* instead
	const char* rawText = "Version 7\nHierarchyVersion 1\0";
	textView = std::string_view(rawText, 29);
	ch = (char*)rawText; // Probably not a very good practice

	parseContentsFile(&root);
	assertLastType(TokenType::END);
	tempChildren.shrink_to_fit();
}

EntityParser::EntityParser(const std::string& filepath, const ParsingMode mode, const bool debug_logParseTime)
	: PARSEMODE(mode)
{
	auto timeStart = std::chrono::high_resolution_clock::now();
	std::ifstream file(filepath, std::ios_base::binary); // Binary mode 50% faster than 'in' mode, keeps CR chars
	if (!file.is_open())
		throw std::runtime_error("Could not open file");

	size_t rawLength = 0;
	char* rawBytes = nullptr;
	char* decompressedText = nullptr;

	// Technically tellg() is not guaranteed by the standard to give us the length
	// of the file...but in practice it does for binary streaming
	file.seekg(0, std::ios_base::end);
	rawLength = static_cast<size_t>(file.tellg());
	rawBytes = new char[rawLength + 1]; // Leave room for the null char
	file.seekg(0, std::ios_base::beg);
	file.read(rawBytes, rawLength);
	file.close();

	if (rawLength > 16 && (unsigned char)rawBytes[16] == 0x8C) // Oodle compression signature
	{
		fileWasCompressed = true;
		size_t decompressedSize = ((size_t*)rawBytes)[0];
		size_t compressedSize = ((size_t*)rawBytes)[1];
		decompressedText = new char[decompressedSize + 1]; // +1 for the null char

		if (!Oodle::DecompressBuffer(rawBytes + 16, compressedSize, decompressedText, decompressedSize))
			throw std::runtime_error("Could not decompress .entities file");
		decompressedText[decompressedSize] = '\0';
		textView = std::string_view(decompressedText, decompressedSize + 1);
		ch = decompressedText;
	}
	else
	{
		fileWasCompressed = false;
		rawBytes[rawLength++] = '\0';
		textView = std::string_view(rawBytes, rawLength);
		ch = rawBytes;
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

	if(mode == ParsingMode::ENTITIES)
		parseContentsFile(&root); // During construction we allow exceptions to propagate and the parser to be destroyed
	else if(mode == ParsingMode::PERMISSIVE)
		parseContentsPermissive(&root);
	assertLastType(TokenType::END);

	tempChildren.shrink_to_fit();
	if (fileWasCompressed)
		delete[] decompressedText;
	delete[] rawBytes;
	if (debug_logParseTime)
		EntityLogger::logTimeStamps("Parsing Duration: ", timeStart);
}

/*
New strategy to prevent nodes with irregularly large numbers of children
from creating runaway allocations. This implementation is more generalized
and maintainable compared to the original solution meant to exclusively
handle the large root child counts of .entities files
*/
int OptimalMaxChildCount(int childCount) {
	if (childCount > 100) {
		// More non-root nodes can have more than 100 children than you might initially believe
		// Hence we should do multiplication instead of flat adding 1000 to every oversized childCount
		int addition = childCount * 0.1;
		if (addition > 1000)
			addition = 1000;
		return childCount + addition;
	}
	else return childCount;
}

ParseResult EntityParser::EditTree(std::string text, EntNode* parent, int insertionIndex, int removeCount, bool renumberLists, bool highlightNew)
{
	ParseResult outcome;

	// We must ensure the parse is successful before modifying the existing tree
	EntNode tempRoot(EntNode::NFC_RootNode);
	initiateParse(text, &tempRoot, parent, outcome);
	if(!outcome.success) return outcome;

	// Populate these with nodes we might need to remove/add to the dataview
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
		if(n->filtered) // Not all nodes we're removing may be filtered in
			removedNodes.Add(wxDataViewItem(n));
		freeNode(n);
	}

	// Prep. the new nodes to be fully integrated into the tree
	for (int i = 0; i < tempRoot.childCount; i++)
	{
		EntNode* n = tempRoot.children[i];
		n->parent = parent;
		if(n->filtered) // Future-proofing
			addedNodes.Add(wxDataViewItem(n));
	}
	
	/* 
	* Common Steps for Each Branch:
	* 1. Move everything into the new child buffer
	* 2. Deallocate the old child buffers
	* 3. Assign the new child buffer/child count to the parent
	*/
	int newNumChildren = parent->childCount + tempRoot.childCount - removeCount;

	if (newNumChildren > parent->maxChildren) {
		int newMaxChildren = OptimalMaxChildCount(newNumChildren);
		EntNode** newChildBuffer = childAlloc.reserveBlock(newMaxChildren);

		// Copy everything into the new child buffer
		int inc = 0;
		for (inc = 0; inc < insertionIndex; inc++)
			newChildBuffer[inc] = parent->children[inc];
		for (int i = 0; i < tempRoot.childCount; i++)
			newChildBuffer[inc++] = tempRoot.children[i];
		for (int i = insertionIndex + removeCount; i < parent->childCount; i++)
			newChildBuffer[inc++] = parent->children[i];

		// Deallocate old child buffer
		childAlloc.freeBlock(parent->children, parent->maxChildren);
		
		// Finally, attach new data to the parent
		parent->maxChildren = newMaxChildren;
		parent->children = newChildBuffer;
		
	}
	else {
		int difference = newNumChildren - parent->childCount;
		int min = insertionIndex + removeCount;
		if (difference > 0) // Must shift to right to expand room
			for (int i = parent->childCount - 1; i >= min; i--)
				parent->children[i + difference] = parent->children[i];
					
		if (difference < 0) // Must shift left to contract space
			for (int i = min, max = parent->childCount; i < max; i++)
				parent->children[i + difference] = parent->children[i];

		for (int inc = insertionIndex, i = 0, max = tempRoot.childCount; i < max; inc++, i++)
			parent->children[inc] = tempRoot.children[i];
	}

	// Common to both branches
	childAlloc.freeBlock(tempRoot.children, tempRoot.maxChildren);
	parent->childCount = newNumChildren;

	// Must update model AFTER node is given it's new child data
	if (parent->isFiltered()) // Filtering checks are optimized so we only check parent + ancestor filter status once, right here
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
			fixListNumberings(parent, false, false);
	}
	fileUpToDate = false;
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

	// Alert model
	if (node->isFiltered()) // Todo: add safeguards so node can't be the root
	{
		wxDataViewItem item(node);
		ItemChanged(item);
		if (highlight)
			view->Select(item);
	}

	fileUpToDate = false;
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

	// Shift nodes around
	if (childIndex < insertionIndex)  // Shift middle elements up
		for (int i = childIndex; i < insertionIndex; i++)
			buffer[i] = buffer[i + 1];

	else  // Shift middle elements down
		for (int i = childIndex; i > insertionIndex; i--)
			buffer[i] = buffer[i - 1];
	buffer[insertionIndex] = child;
	
	// Only alert model if node is filtered in 
	// We assume Root will never be the node we're moving (todo: add safeguards to ensure this)
	if (child->isFiltered())
	{
		wxDataViewItem parentItem(parent);
		wxDataViewItem childItem(child);
		ItemDeleted(parentItem, childItem);
		ItemAdded(parentItem, childItem);
		if(highlight)
			view->Select(childItem);
	}
	fileUpToDate = false;
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
	// TODO: ADD SAFEGUARDS FOR UNDOING / REDOING WHILE UNPUSHED GROUP COMMAND EXISTS
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
	msg.append(std::to_string(root.maxChildren));
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
	for (char* inc = ch - 1; inc >= min; inc--) //ch will equal next char to be parsed - if == \n an extra newline would be added
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

	try 
	{
		if(PARSEMODE == ParsingMode::PERMISSIVE)
			parseContentsPermissive(tempRoot);
		else switch (parent->nodeFlags)
		{
			case EntNode::NFC_RootNode:
			parseContentsFile(tempRoot);
			break;

			case EntNode::NFC_ObjSimple:
			if (parent->parent == &root)
				parseContentsEntity(tempRoot);
			else parseContentsLayer(tempRoot);
			break;

			case EntNode::NFC_ObjEntitydef: case EntNode::NFC_ObjCommon:
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
		childAlloc.freeBlock(tempRoot->children, tempRoot->maxChildren);

		results.errorLineNum = errorLine;
		results.errorMessage = err.what();
		results.success = false;
	}
	tempChildren.shrink_to_fit();
}

/* 
Permissive parse function with significantly less error checking for proper token types and arrangements.
It is intended to be as generous as possible, allowing grammars that wouldn't normally work with id's Parsers
to be given a free pass if they're not completely wrong.

This will eventually be used to parse the append menu file, and may see further use if/when EntitySlayer's is
expanded to work with .decl files
*/
void EntityParser::parseContentsPermissive(EntNode* node)
{
	EntNode* n = nullptr;
	size_t childrenStart = tempChildren.size();
	
	LABEL_LOOP:
	Tokenize();

	LABEL_LOOP_SKIP_TOKENIZE:
	switch (lastTokenType)
	{
		default: // End, BraceClose, Assignment, Value_Number
		setNodeChildren(node, childrenStart);
		return;

		case TokenType::COMMENT:
		pushNode<false, true>(EntNode::NFC_Comment);
		goto LABEL_LOOP;

		case TokenType::NEWLINE:
		case TokenType::SEMICOLON: // Stray semicolons are valid entities/decl syntax
		goto LABEL_LOOP;

		case TokenType::BRACEOPEN: // Most decl files begin with a nameless brace pair
		activeID = std::string_view("");
		n = pushNode<true, false>(EntNode::NFC_ObjSimple);
		parseContentsPermissive(n);
		assertLastType(TokenType::BRACECLOSE);
		goto LABEL_LOOP;

		/*
		* Many Possibilities:
		* 1. [Identifier/String] [newline | Closing Brace | EOF | Comment]
		* [DONE] 2. [Identifier/String] { }    
		* [DONE] 3. [Identifier/String] = { }
		* [DONE] 4. [Identifier/String] = [Identifier/Value]
		* [DONE] 5. [Identifier/String] = [Idenitifer/Value];
		* [DONE] 6. [Identifier/String] [Identifier/Value]
		* [DONE] 7. [Identifier/String] [Identifier/Value] { }
		*/
		case TokenType::IDENTIFIER: case TokenType::VALUE_STRING:
		activeID = lastUniqueToken;
		Tokenize();

		if (lastTokenType == TokenType::BRACEOPEN) {  // Simple objects
			n = pushNode<true, false>(EntNode::NFC_ObjSimple);
			parseContentsPermissive(n);
			assertLastType(TokenType::BRACECLOSE);
			goto LABEL_LOOP;
		}

		else if (lastTokenType == TokenType::EQUALSIGN) {
			Tokenize();

			if (lastTokenType == TokenType::BRACEOPEN) { // Common Objects
				n = pushNode<true, false>(EntNode::NFC_ObjCommon);
				parseContentsPermissive(n);
				assertLastType(TokenType::BRACECLOSE);
				goto LABEL_LOOP;
			}

			else if (lastTokenType >= TokenType::IDENTIFIER) { // Value assignments
				n = pushNode<true, true>(EntNode::NFC_ValueDarkmetal);
				Tokenize();
				if (lastTokenType == TokenType::SEMICOLON) {
					n->nodeFlags = EntNode::NFC_ValueCommon;
					goto LABEL_LOOP;
				}
				else goto LABEL_LOOP_SKIP_TOKENIZE;
			}

			else throw Error("Unexpected token after = sign");
		}

		else if (lastTokenType >= TokenType::IDENTIFIER) { // Consecutive Identifiers or higher
			n = pushNode<true, true>(EntNode::NFC_ValueFile);
			Tokenize();
			if (lastTokenType == TokenType::BRACEOPEN) {
				n->nodeFlags = EntNode::NFC_ObjEntitydef;
				parseContentsPermissive(n);
				assertLastType(TokenType::BRACECLOSE);
				goto LABEL_LOOP;
			}
			else goto LABEL_LOOP_SKIP_TOKENIZE;
		}
		else if (lastTokenType != TokenType::SEMICOLON) { // EOF, Braceclose, newline, comment
			pushNode<true, false>(EntNode::NFC_ValueLayer);
			goto LABEL_LOOP_SKIP_TOKENIZE;
		}
		else throw Error("Unexpected token after identifier or string literal");
	}
}

void EntityParser::parseContentsFile(EntNode* node) {
	EntNode* n = nullptr;
	size_t childrenStart = tempChildren.size();
	LABEL_LOOP:
	Tokenize();
	if (lastTokenType == TokenType::COMMENT)
	{
		pushNode<false, true>(EntNode::NFC_Comment);
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
		pushNode<true, true>(EntNode::NFC_ValueFile);
		break;

		case TokenType::BRACEOPEN:
		n = pushNode<true, false>(EntNode::NFC_ObjSimple);
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
		pushNode<false, true>(EntNode::NFC_Comment);
		goto LABEL_LOOP;
	}
	if (lastTokenType == TokenType::VALUE_STRING) // Need this specifically for pvp_darkmetal
	{
		activeID = lastUniqueToken;
		assertIgnore(TokenType::EQUALSIGN);
		assertIgnore(TokenType::VALUE_STRING);
		pushNode<true, true>(EntNode::NFC_ValueDarkmetal);
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
		n = pushNode<true, true>(EntNode::NFC_ObjEntitydef);
		parseContentsDefinition(n);
		assertLastType(TokenType::BRACECLOSE);
		break;

		case TokenType::BRACEOPEN:
		n = pushNode<true, false>(EntNode::NFC_ObjSimple);
		parseContentsLayer(n);
		assertLastType(TokenType::BRACECLOSE);
		break;

		case TokenType::EQUALSIGN:
		TokenizeAdjustValue();
		if (lastTokenType <= TokenType::IDENTIFIER)
			throw Error("Value expected (entity function)");
		pushNode<true, true>(EntNode::NFC_ValueCommon);
		assertIgnore(TokenType::SEMICOLON);
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
		pushNode<false, true>(EntNode::NFC_Comment);
		goto LABEL_LOOP;
	}
	if (lastTokenType != TokenType::VALUE_STRING)
	{
		setNodeChildren(node, childrenStart);
		return;
	}
	pushNode<false, true>(EntNode::NFC_ValueLayer);
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
		pushNode<false, true>(EntNode::NFC_Comment);
		goto LABEL_LOOP;
	}
	if(lastTokenType != TokenType::IDENTIFIER)
	{
		setNodeChildren(node, childrenStart);
		return;
	}
	assertIgnore(TokenType::EQUALSIGN);
	activeID = lastUniqueToken;
	TokenizeAdjustValue();
	switch (lastTokenType)
	{
		case TokenType::BRACEOPEN:
		n = pushNode<true, false>(EntNode::NFC_ObjCommon);
		parseContentsDefinition(n);
		assertLastType(TokenType::BRACECLOSE);
		break;

		case TokenType::VALUE_NUMBER:
		case TokenType::VALUE_STRING: case TokenType::VALUE_KEYWORD:
		pushNode<true, true>(EntNode::NFC_ValueCommon);
		assertIgnore(TokenType::SEMICOLON);
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
		childAlloc.freeBlock(node->children, node->maxChildren);
	}

	/*
	* We should ensure node values are reset to default when freeing them.
	* Otherwise we must operate under the assumption that some data is inaccurate
	* when working with EntityNode instance methods.
	*/
	new (node) EntNode;
	nodeAlloc.freeBlock(node, 1);
}

template <bool useID, bool useLast>
EntNode* EntityParser::pushNode(const uint16_t p_flags)
{
	EntNode* n = nodeAlloc.reserveBlock(1);
	n->nodeFlags = p_flags;
	n->textPtr = textAlloc.reserveBlock(
		(useID ? activeID.length() : 0)
		+ (useLast ? lastUniqueToken.length() : 0)
	);
	char* inc = n->textPtr;

	if (useID) {
		for (char c : activeID)
			*inc++ = c;
		n->nameLength = (int)activeID.length();
	}
	if (useLast) {
		for(char c : lastUniqueToken)
			*inc++ = c;
		if(useID) n->valLength = (int)lastUniqueToken.length();
		else n->nameLength = (int)lastUniqueToken.length();
	}

	tempChildren.push_back(n);
	return n;
}

void EntityParser::setNodeChildren(EntNode* parent, const size_t startIndex)
{
	size_t s = tempChildren.size();
	size_t childCount = s - startIndex;
	parent->childCount = (int)childCount;
	parent->maxChildren = OptimalMaxChildCount(parent->childCount);
	parent->children = childAlloc.reserveBlock(parent->maxChildren);

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
	/*
	* A fun little micro-optimization that actually seems to make few-milliseconds difference
	* It's a simple logic: ensure the CPU is performing as many comparisons simultaneously as possible
	* Check for false first, since statistically speaking entities files have it more frequently than true
	*/
	if (lastTokenType == TokenType::IDENTIFIER) {
		const char* raw = lastUniqueToken.data();
		size_t len = lastUniqueToken.length();

		if (len == 5 && memcmp(raw, "false", 5) == 0)
			lastTokenType = TokenType::VALUE_KEYWORD;
		else if (len == 4 && memcmp(raw, "true", 4) == 0 || memcmp(raw, "NULL", 4) == 0)
			lastTokenType = TokenType::VALUE_KEYWORD;
	}
}

void EntityParser::Tokenize() 
{
	// Faster than STL isalpha(char) and isdigit(char) functions
	#define isLetter (((unsigned int)(*ch | 32) - 97) < 26U)
	#define isNum (((unsigned)*ch - '0') < 10u)
	char* first; // Ptr to start of current identifier/value token

	LABEL_TOKENIZE_START:
	switch (*ch) // Not auto-incrementing should eliminate unnecessary arithmetic operations
	{                     // at the cost of needing to manually it++ in additional areas
		case '\r':
		if(*++ch != '\n')
			throw Error("Expected line feed after carriage return");
		case '\n':
		if (PARSEMODE == ParsingMode::PERMISSIVE) {
			ch++;
			lastTokenType = TokenType::NEWLINE;
			return;
		}
		case ' ': case '\t':
		ch++;
		goto LABEL_TOKENIZE_START;

		case '\0': // Appending null character to end of string simplifies this function's bounds-checking
		lastTokenType = TokenType::END; // Do not it++ here to prevent out of bounds error
		return;

		case ';':
		lastTokenType = TokenType::SEMICOLON;
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
		lastTokenType = TokenType::EQUALSIGN;
		ch++;
		return;

		case ',':
		lastTokenType = TokenType::COMMA;
		ch++;
		return;

		case ')':
		lastTokenType = TokenType::PARENCLOSE;
		ch++;
		return;

		case '(':
		{
			first = ch++;
			int numvalues = 0;
			
			LABEL_TUPLE_START:
			Tokenize();
			switch (lastTokenType)
			{
				case TokenType::IDENTIFIER: // declType( keyword )
				case TokenType::VALUE_NUMBER:
				case TokenType::VALUE_TUPLE:
				case TokenType::COMMA: // TODO: Improve this?
				goto LABEL_TUPLE_START;

				case TokenType::PARENCLOSE:
				lastUniqueToken = std::string_view(first, (size_t)(ch - first));
				lastTokenType = TokenType::VALUE_TUPLE;
				return;

				default:
				throw Error("Invalid format for a tuple");
				break;
			}
		}

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

		// NOTES:
		// 1. PERMANENT: Negative sign can't be seperated from the number by whitespace
		// 2. CHANGE IN THE FUTURE (?): Number token cannot begin with a period.
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

		default:
		{
			if (!isLetter && *ch != '_') {
				if(*ch != '%' || PARSEMODE != ParsingMode::PERMISSIVE)
					throw Error("Unrecognized character");
			}
				
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
				if (isLetter || isNum || *ch == '_')
					goto LABEL_ID_START;
				if(*ch == '%' && PARSEMODE == ParsingMode::PERMISSIVE)
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
		if (result->isFiltered()) {
			wxDataViewItem item(result);
			view->UnselectAll();
			view->Select(item);
			if(result->childCount > 0)
				view->Expand(item);
			view->EnsureVisible(item);
			//wxLogMessage("Found");
			return;
		}

		// Set sentinel in first loop iteration
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

	EntNode** children = root.children;
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
	EntNode** childBuffer = root.children;
	for (int i = 0; i < childCount; i++)
	{
		EntNode* entity = childBuffer[i];
		EntNode& entityDef = (*entity)["entityDef"];
		if (filterByClass)
		{
			std::string_view classVal = entityDef["class"].getValue();
			if (classFilters.count(std::string(classVal)) < 1) // TODO: OPTIMIZE ACCESS CASTING
			{
				entity->filtered = false;
				continue;
			}
				
		}

		if (filterByInherit)
		{
			std::string_view inheritVal = entityDef["inherit"].getValue();
			if (inheritFilters.count(std::string(inheritVal)) < 1) // TODO: OPTIMIZE CASTING
			{
				entity->filtered = false;
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
				entity->filtered = false;
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
				entity->filtered = false;
				continue;
			}

			float deltaX = x - spawnSphere.x,
					deltaY = y - spawnSphere.y,
					deltaZ = z - spawnSphere.z;
			float distance2 = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

			if(distance2 > maxR2)
			{
				entity->filtered = false;
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
				entity->filtered = false;
				continue;
			}
		}

		// Node has passed all filters and should be included in the filtered tree
		entity->filtered = true;
	}
	EntityLogger::logTimeStamps("Time to Filter: ", start);
}