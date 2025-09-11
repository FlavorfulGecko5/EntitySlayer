#pragma warning(disable : 4996) // Deprecation errors
#include "EntityDiff.h"
#include "EntityLogger.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include "EntityParser.h"

typedef EntNode entnode;
typedef std::unordered_map<std::string, entnode*> nodemap_t;
typedef std::unordered_map<std::string, int> prefixmap_t;
typedef std::vector<std::string> prefixlist_t;
typedef std::vector<std::string_view> propstack_t;

void entdiff_buildnodemap(const entnode& root, nodemap_t& map, prefixlist_t& prefixes, bool& UseSubmapIndices)
{
	// First Loop: Gather all the entity prefixes
	UseSubmapIndices = false;
	prefixes.resize(1); // Ensure minimum size of 1, for Doom Eternal entities
	prefixes[0] = "";
	for (int i = 0; i < root.getChildCount(); i++) {
		entnode& e = root[i];

		if (e["entityDef"]["systemVars"]["entityType"].getValueUQ() == "idWorldspawn") {
			int submapindex = 0;
			e.ValueInt(submapindex, 0, 9999);

			if(prefixes.size() <= submapindex)
				prefixes.resize(submapindex + 1);

			prefixes[submapindex] = e["entityDef"]["edit"]["entityPrefix"].getValueUQ();
			UseSubmapIndices = true;
		}
	}

	// Second Loop: Build the entity map
	for (int i = 0; i < root.getChildCount(); i++) {
		entnode& e = root[i];

		std::string_view entityname = e["entityDef"].getValue();
		if (entityname.length() == 0)
			continue;

		// Don't throw errors for missing submap indices since Eternal doesn't have them
		// Instead just assume an index of 0
		int submapindex = 0;
		e.ValueInt(submapindex, 0, 9999);

		std::string lookupname = prefixes[submapindex];
		lookupname.push_back('@');
		lookupname.append(entityname);

		auto iter = map.find(lookupname);
		if (iter != map.end()) {
			EntityLogger::log("EntityDiff: Duplicate entity detected");
		}
		map.emplace(lookupname, &e);
	}
}

// Returns true if nodes are equivalent in all meaningful ways
bool entdiff_compare(const EntNode& a, const EntNode& b)
{
	#define diffcheck(OP) if(!(OP)) return false;

	diffcheck(a.getFlags() == b.getFlags());
	diffcheck(a.getChildCount() == b.getChildCount()); 
	diffcheck(a.NameLength() == b.NameLength()); 
	diffcheck(memcmp(a.NamePtr(), b.NamePtr(), a.NameLength()) == 0);
	diffcheck(a.ValueLength() == b.ValueLength());
	diffcheck(memcmp(a.ValuePtr(), b.ValuePtr(), a.ValueLength()) == 0);

	for (int i = 0; i < a.getChildCount(); i++) {
		diffcheck(entdiff_compare(*a.ChildAt(i), *b.ChildAt(i)));
	}

	return true;
}

void entdiff_writepropstack(propstack_t& stack, std::string& writeto) {
	// We'll use verbatim string format since there could be quotes in property names
	// (i.e. logicFX lists)
	writeto.append("<%");
	for(std::string_view s : stack) {
		writeto.append(s);
		writeto.push_back('@');
	}
	if(writeto.back() == '@')
		writeto.pop_back(); // Pop the trailing @
	writeto.append("%>");

}

void entdiff_builddiffs(const EntNode& vanilla, const EntNode& modded, std::string& deleted, std::string& added, std::string& edited, propstack_t& propstack)
{
	// Case 1: Identify nodes that have been deleted from the vanilla file
	for (int i = 0; i < vanilla.getChildCount(); i++) {
		std::string_view name = vanilla[i].getName();
		const entnode& mnode = modded[name];

		// A difference in node flags suggests things like going from a simple key = value node, to an object node
		// or vice versa. This is too complex to handle via Case 3, so we create both a deleted and an added node
		// to account for this edge case. May be rare, but definitely possible (i.e. for eventargs decl pointer = NULL)
		if (&mnode == EntNode::SEARCH_404 || vanilla[i].getFlags() != mnode.getFlags()) {
			propstack.push_back(name);
			entdiff_writepropstack(propstack, deleted);
			deleted.push_back('\n');
			propstack.pop_back();
		}
	}

	std::string addedThisNode;

	for (int i = 0; i < modded.getChildCount(); i++) {
		const entnode& mnode = modded[i];
		std::string_view name = mnode.getName();
		const entnode& vnode = vanilla[name];

		// Case 2: Newly added nodes
		// Do a flag check to finish handling the edge case explained above
		if (&vnode == EntNode::SEARCH_404 || vnode.getFlags() != mnode.getFlags()) {
			mnode.generateText(addedThisNode);
			addedThisNode.push_back('\n');
		}

		// Case 3: Both trees have the node, and it's flags are the same
		// Check to ensure their values are equal
		else {
			propstack.push_back(name);
			if (vnode.getValue() != mnode.getValue()) 
			{
				entdiff_writepropstack(propstack, edited);
				edited.append(" = ");
				edited.append(mnode.getValue());
				edited.push_back('\n');
			}
			entdiff_builddiffs(vnode, mnode, deleted, added, edited, propstack);
			propstack.pop_back();
		}
	}

	// Consolidate into a single node
	if (addedThisNode.length() > 0) {
		entdiff_writepropstack(propstack, added);
		added.append(" = {");
		added.append(addedThisNode);
		added.append("}\n");
	}
}

void entdiff_getlookupname(const entnode& entity, const prefixlist_t& prefixes, std::string& writeto)
{
	std::string_view entityname = entity["entityDef"].getValue();
	if (entityname.length() == 0)
		return;

	int submapindex = 0;
	entity.ValueInt(submapindex, 0, 9999);

	writeto.append(prefixes[submapindex]);
	writeto.push_back('@');
	writeto.append(entityname);
}

void EntityDiff::Export(const EntNode& vanilla, const EntNode& modded, const char* outputpath)
{
	nodemap_t vanillamap, moddedmap;
	prefixlist_t vanillaprefix, moddedprefix;
	bool __dummy;

	entdiff_buildnodemap(vanilla, vanillamap, vanillaprefix, __dummy);
	entdiff_buildnodemap(modded, moddedmap, moddedprefix, __dummy);

	std::string output;
	output.reserve(1000000);

	output.append("delete = {\n");

	// First: Gather deleted entities
	// We don't need to care about order so we can iterate through the map
	for (const auto& pair : vanillamap) {
		auto iter = moddedmap.find(pair.first);
		if (iter == moddedmap.end())
		{
			output.append("\t\"");
			output.append(pair.first);
			output.append("\"\n");
		}
	}
	output.append("}\n");

	// Second: Gather modified entities
	// We do need to care about ordering due to placebefore/placeafter
	for (int i = 0; i < modded.getChildCount(); i++) {
		const entnode& current = modded[i];

		std::string_view entityname = current["entityDef"].getValue();
		if (entityname.length() == 0)
			continue;

		int submapindex = 0;
		current.ValueInt(submapindex, 0, 9999);

		std::string lookupname = moddedprefix[submapindex];
		lookupname.push_back('@');
		lookupname.append(entityname);

		// If this is a new entity, include it
		const auto& iter = vanillamap.find(lookupname);

		enum class enttype
		{
			vanilla,
			added,
			modified
		} etype;

		if (iter == vanillamap.end()) {
			etype = enttype::added;
		}
		else {
			etype = entdiff_compare(current, *iter->second) ? enttype::vanilla : enttype::modified;
		}

		switch (etype)
		{
			case enttype::vanilla:
			continue;

			case enttype::added:
			output.append("added \"");
			break;

			case enttype::modified:
			output.append("edited \"");
			break;
		}

		output.append(lookupname);
		output.append("\" {\n");

		output.append("\tname = \"");
		output.append(entityname);
		output.append("\"\n\tprefix = \"");
		output.append(moddedprefix[submapindex]);

		// todo: placeafter/placebefore
		output.append("\"\n\tplaceafter = \"");
		if (i != 0) {
			entdiff_getlookupname(modded[i - 1], moddedprefix, output);
		}

		output.append("\"\n\tplacebefore = \"");
		if (i != modded.getChildCount() - 1) {
			entdiff_getlookupname(modded[i + 1], moddedprefix, output);
		}
		output.append("\"\n");


		// For new entities, include everything
		if (etype == enttype::added)
		{
			output.append("\tnewtext = {\n");
			// Skip the entity {} wrapper to omit the subindex
			for (int e_iter = 0; e_iter < current.getChildCount(); e_iter++) {
				current[e_iter].generateText(output, 2);
				output.push_back('\n');
			}
			output.append("\t}\n");
		}

		// For Modified Entities, run the in-depth diff checker 
		else if (etype == enttype::modified)
		{
			std::string deletions = "deleted = {";
			std::string added = "added = {";
			std::string edited = "edited = {";
			propstack_t propstack;

			const entnode& vanilla = *iter->second;
			entdiff_builddiffs(vanilla, current, deletions, added, edited, propstack);

			deletions.append("}\n");
			added.append("}\n");
			edited.append("}\n");
			output.append(deletions);
			output.append(added);
			output.append(edited);
		}
		output.append("}\n");
	}

	std::ofstream writer(outputpath, std::ios_base::binary);
	writer << output;
	writer.close();
	EntityLogger::log("EntityDiff Exported Successfully");
}

void entdiff_logwarning(std::ofstream& logfile, std::string_view lookupname, std::string_view propname, std::string_view msg)
{
	std::string logmsg = "WARNING: ";
	logmsg.append(lookupname);
	logmsg.append(":");
	logmsg.append(propname);
	logmsg.append(" ");
	logmsg.append(msg);
	EntityLogger::log(logmsg);

	logfile << logmsg << "\n";
}

entnode* entdiff_getproperty(entnode& entity, std::string_view propstring)
{
	if(propstring.empty())
		return &entity;

	const char* data = propstring.data();
	const char* max = data + propstring.size();

	entnode* currentnode = &entity;
	for (const char* c = data; c < max; c++) {
		if (*c == '@')
		{
			std::string_view thisprop(data, c - data);
			data = c + 1;

			currentnode = &(*currentnode)[thisprop]; // Make a new method to avoid this jank syntax?
		}
	}

	std::string_view thisprop(data, max - data);
	currentnode = &(*currentnode)[thisprop];

	return currentnode;
}

void EntityDiff::Import(EntityParser& parser, const EntNode& diff, const char* LOGPATH, const char* FILEPATH)
{
	// Allow for writing to same log with append flag
	std::ofstream logfile(LOGPATH, std::ios_base::binary | std::ios_base::app);
	logfile << "\n\nImport Log For " << FILEPATH << "\n======\n";

	EntityLogger::log("Importing Entity Diff. This may take some time");

	entnode& root = *parser.getRoot();
	nodemap_t nodemap;
	prefixlist_t prefixlist;
	prefixmap_t prefixmap;
	ParseResult parseresult;
	bool UseSubmapIndices;

	entdiff_buildnodemap(root, nodemap, prefixlist, UseSubmapIndices);

	std::string textbuffer;
	textbuffer.reserve(5000);

	// For fast lookup of the index
	for (size_t i = 0; i < prefixlist.size(); i++) {
		prefixmap.emplace(prefixlist[i], i);
	}

	// Step 1: Import Deleted Entities
	{
		const entnode& deleted = diff["delete"];

		for (int i = 0; i < deleted.getChildCount(); i++) {
			std::string_view name = deleted[i].getNameUQ();
			auto iter = nodemap.find(std::string(name));

			if (iter == nodemap.end()) {
				entdiff_logwarning(logfile, name, "", "Deleted entity already doesn't exist in file.");
			}
			else {
				int nodeindex = root.getChildIndex(iter->second);
				parseresult = parser.EditTree("", &root, nodeindex, 1, false, true);
				if (parseresult.success) {
					nodemap.erase(iter);
				}
				else {
					entdiff_logwarning(logfile, name, "", "Parser failed to delete entity");
				}
				
			}
		}
	}

	// Step 2+3: Import new entities and modified entities
	for (int diffiter = 0; diffiter < diff.getChildCount(); diffiter++)
	{
		const entnode& current = diff[diffiter];
		if (current.getName() == "added")
		{
			std::string_view lookupname = current.getValueUQ();
			// Edge Case: Check if the entity we want to add already exists
			{
				auto iter = nodemap.find(std::string(lookupname));
				if (iter != nodemap.end()) {
					entdiff_logwarning(logfile, lookupname, "", "Added entity already exists! Skipping");
					continue;
				}
			}

			// Determine the submap index from the prefix
			int submapindex = 0;
			{
				std::string_view prefixstring = current["prefix"].getValueUQ();
				const auto& iter = prefixmap.find(std::string(prefixstring));
				if (iter == prefixmap.end()) {
					entdiff_logwarning(logfile, lookupname, "", "Submap for this entity does not exist! Cannot add!");
					continue;
				}
				submapindex = iter->second;
			}

			// Determine insertion index by anchoring entity to it's adjacent entities
			int insertionindex = root.getChildCount();
			{
				std::string_view adjacentname = current["placeafter"].getValueUQ();
				auto iter = nodemap.find(std::string(adjacentname));

				if (iter == nodemap.end()) {
					adjacentname = current["placebefore"].getValueUQ();
					iter = nodemap.find(std::string(adjacentname));
				}

				if (iter == nodemap.end()) {
					entdiff_logwarning(logfile, lookupname, "", "Failed to find adjacent entities. Placing at end of file.");
				}
				else {
					insertionindex = root.getChildIndex(iter->second);
				}
			}

			// Generate the text we'll be adding to the node tree
			{
				textbuffer.clear();
				textbuffer.append("entity ");

				// Must ensure we do not insert submap indices for Eternal entities
				if (UseSubmapIndices) {
					textbuffer.append(std::to_string(submapindex));
				}

				textbuffer.append(" {\n");
				const entnode& newtext = current["newtext"];
				for (int i = 0; i < newtext.getChildCount(); i++) {
					newtext[i].generateText(textbuffer);
					textbuffer.push_back('\n');
				}
				textbuffer.push_back('}');
			}

			// Finalize
			parseresult = parser.EditTree(textbuffer, &root, insertionindex, 0, false, true);
			if (parseresult.success) {
				nodemap[std::string(lookupname)] = &root[insertionindex];
			}
			else {
				entdiff_logwarning(logfile, lookupname, "", "Parser failed to add entity");
			}

		}



		else if (current.getName() == "edited")
		{
			std::string_view lookupname = current.getValueUQ();

			// Verify the entity exists in the new file
			const auto& nodemap_iter = nodemap.find(std::string(lookupname));
			if (nodemap_iter == nodemap.end()) {
				entdiff_logwarning(logfile, lookupname, "", "Edited entity does not exist in file");
				continue;
			}
			entnode& entity = *nodemap_iter->second;


			// Remove deleted properties
			const entnode& deletions = current["deleted"];
			for (int deliter = 0; deliter < deletions.getChildCount(); deliter++)
			{
				std::string_view propstring = deletions[deliter].getNameUQ();

				entnode* propnode = entdiff_getproperty(entity, propstring);
				if (propnode == EntNode::SEARCH_404) {
					entdiff_logwarning(logfile, lookupname, propstring, "Property has already been deleted");
				}
				else {
					entnode* parent = propnode->getParent();
					parseresult = parser.EditTree("", parent, parent->getChildIndex(propnode), 1, false, true);
					if (!parseresult.success) {
						entdiff_logwarning(logfile, lookupname, propstring, "Parser failed to delete property");
					}
				}
			}

			// Add New Properties
			const entnode& additions = current["added"];
			for (int additer = 0; additer < additions.getChildCount(); additer++)
			{
				const entnode& currentadd = additions[additer];
				std::string_view propstring = currentadd.getNameUQ();

				entnode* propnode = entdiff_getproperty(entity, propstring);
				if (propnode == EntNode::SEARCH_404) {
					entdiff_logwarning(logfile, lookupname, propstring, "Cannot add data to property that no longer exists! Skipping");
				}
				else {
					textbuffer.clear();

					// Iterate through all the properties we're adding
					for (int addpropiter = 0; addpropiter < currentadd.getChildCount(); addpropiter++) {
						std::string_view newpropname = currentadd[addpropiter].getName();
						
						// Check that each property we're adding doesn't already exist in the new file
						if (&(*propnode)[newpropname] != EntNode::SEARCH_404) {
							std::string debugstring(propstring);
							debugstring.push_back('@');
							debugstring.append(newpropname);
							entdiff_logwarning(logfile, lookupname, debugstring, "Cannot add property that already exists! Skipping");
						}
						else {
							currentadd[addpropiter].generateText(textbuffer);
							textbuffer.push_back('\n');
						}
					}

					parseresult = parser.EditTree(textbuffer, propnode, propnode->getChildCount(), 0, false, true);
					if (!parseresult.success) {
						entdiff_logwarning(logfile, lookupname, propstring, "Parser failed to add data to subproperties");
					}
				}
			}

			// Modify Edited Properties
			const entnode& edits = current["edited"];
			for (int edititer = 0; edititer < edits.getChildCount(); edititer++)
			{
				std::string_view propstring = edits[edititer].getNameUQ();
				
				entnode* propnode = entdiff_getproperty(entity, propstring);
				if (propnode == EntNode::SEARCH_404) {
					entdiff_logwarning(logfile, lookupname, propstring, "Cannot edit property that no longer exists! Skipping");
				}
				else {
					textbuffer.clear();
					textbuffer.append(propnode->getName());
					textbuffer.append(edits[edititer].getValue());
					parser.EditText(textbuffer, propnode, propnode->NameLength(), true);
				}
			}

			// Incase of edge cases when merging
			parser.fixListNumberings(&entity, true, true);
		}
	}

	parser.PushGroupCommand();
	EntityLogger::log("EntityDiff Imported Successfully");
}
