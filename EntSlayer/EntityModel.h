#include "wx/wx.h"
#include "wx/dataview.h"
#include <EntityParser.h>
#include <set>
#include <chrono>

struct Sphere {
	float x = 0;
	float y = 0;
	float z = 0;
	float r = 0; // Radius
};

class EntityModel : public wxDataViewModel {
	public:
	// EntityModels should not have ownership of the root or tree data
	static inline const std::string FILTER_NOLAYERS = "\"No Layers\"";
	EntNode* root;

	std::set<std::string> classFilters = {};
	std::set<std::string> inheritFilters = {};
	std::set<std::string> layerFilters = {};

	bool filterSpawnPosition = false;
	Sphere spawnSphere;

	std::vector<std::string> textFilters;
	bool caseSensitiveText = true;

	/* 
	* Checks for newly created layers/classes/inherits and adds them to their respective filter checklists
	* Previously identified values are NOT removed from the checklists
	*/
	void refreshFilterMenus(wxCheckListBox* layerMenu, wxCheckListBox* classMenu, wxCheckListBox* inheritMenu)
	{
		std::set<std::string_view> newLayers;
		std::set<std::string_view> newClasses;
		std::set<std::string_view> newInherits;

		EntNode** children = root->getChildBuffer();
		int childCount = root->getChildCount();

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

	void SetFilters(wxCheckListBox* layerMenu, wxCheckListBox* classMenu, wxCheckListBox* inheritMenu,
		bool newSpawnFilterSetting, Sphere newSphere, wxCheckListBox* textMenu, bool newCaseSensSetting)
	{
		layerFilters.clear();
		classFilters.clear();
		inheritFilters.clear();
		textFilters.clear();

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

		caseSensitiveText = newCaseSensSetting;
		filterSpawnPosition = newSpawnFilterSetting;
		spawnSphere = newSphere;
	}

	EntityModel(EntNode* p_root) : root(p_root) {}

	void GetValue(wxVariant& variant, const wxDataViewItem& item, unsigned int col) const override
	{
		wxASSERT(item.IsOk());

		EntNode* node = (EntNode*)item.GetID();

		if (col == 0) {

			// Use value in entityDef node instead of "entity"
			if (node->getParent() == root)
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

	// TODO: ENABLE VALUE SAVING
	bool SetValue(const wxVariant& variant, const wxDataViewItem& item, unsigned int col) override
	{
		// This works, but changes to fields will not be saved when double clicked/edited
		return false;
	}

	wxDataViewItem GetParent(const wxDataViewItem& item) const override
	{
		if (!item.IsOk()) // Invisible root node (whatever that is)
			return wxDataViewItem(0);

		EntNode* node = (EntNode*)item.GetID();
		return wxDataViewItem((void*)node->getParent());
	}

	/*
	* TODO: ItemsAdded() calls this ONCE PER ITEM ADDED
	* This means the filtration system can get applied multiple times consecutively, for no good reason
	* Need to refactor the filters system - move it out of GetChildren, to fix this, and the debug errors/memory leaks
	*/
	unsigned int GetChildren(const wxDataViewItem& parent, wxDataViewItemArray& array) const override
	{
		EntNode* node = (EntNode*)parent.GetID();

		if (!node) // There is our root node (which we shouldn't delete), and wx's invisible root node
		{
			array.Add(wxDataViewItem((void*)root)); // !! This is how wxDataViewCtrl finds the root variable
			return 1;
		}

		EntNode** childBuffer = node->getChildBuffer();
		int childCount = node->getChildCount();

		/*
		* Filter system for the root node
		*/
		if (node == root)
		{
			auto start = std::chrono::high_resolution_clock::now();

			bool filterByClass = classFilters.size() > 0;
			bool filterByInherit = inheritFilters.size() > 0;
			bool filterByLayer = layerFilters.size() > 0;
			bool noLayerFilter = layerFilters.count(FILTER_NOLAYERS) > 0;
			
			
			size_t numTextFilters = textFilters.size();
			bool filterByText = numTextFilters > 0;
			//string textBuffer;

			for (int i = 0; i < childCount; i++)
			{
				if (filterByClass)
				{
					std::string_view classVal = (*childBuffer[i])["entityDef"]["class"].getValue();
					if (classFilters.count(std::string(classVal)) < 1) // TODO: OPTIMIZE ACCESS CASTING
						continue;
				}

				if (filterByInherit)
				{
					std::string_view inheritVal = (*childBuffer[i])["entityDef"]["inherit"].getValue();
					if (inheritFilters.count(std::string(inheritVal)) < 1) // TODO: OPTIMIZE CASTING
						continue;
				}

				if (filterByLayer)
				{
					bool hasLayer = false;
					EntNode& layerNode = (*childBuffer[i])["layers"];
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
						continue;
				}

				if (filterSpawnPosition)
				{
					// If a variable is undefined, we assume default value of 0
					// If spawnPosition is undefined, we assume (0, 0, 0) instead of excluding
					EntNode& positionNode = (*childBuffer[i])["entityDef"]["edit"]["spawnPosition"];
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
						continue;
					}

					float deltaX = x - spawnSphere.x,
						  deltaY = y - spawnSphere.y,
						  deltaZ = z - spawnSphere.z;
					float distance = std::sqrtf(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

					if(distance > spawnSphere.r)
						continue;
				}

				if (filterByText)
				{
					//textBuffer.clear();
					//childBuffer[i]->generateText(textBuffer);
					
					bool containsText = false;
					for (const std::string& key : textFilters)
						if (childBuffer[i]->searchDownwardsLocal(key, caseSensitiveText) != EntNode::SEARCH_404)
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
						continue;
				}

				// Node has passed all filters and should be included in the filtered tree
				array.Add(wxDataViewItem((void*)childBuffer[i]));
			}
			auto stop = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
			//wxLogMessage("Time to Filter: %zu", duration.count());

			return array.size();
		}

		for (int i = 0; i < childCount; i++)
			array.Add(wxDataViewItem((void*)childBuffer[i]));
		return childCount;
	}

	bool IsContainer(const wxDataViewItem& item) const override
	{
		if (!item.IsOk()) // invisible root node?
			return true;

		EntNode* node = (EntNode*)item.GetID();

		return node->getType() > NodeType::VALUE_COMMON;
	}

	unsigned int GetColumnCount() const override
	{
		return 2;
	}

	wxString GetColumnType(unsigned int col) const override
	{
		return "string";
	}

	bool HasContainerColumns(const wxDataViewItem& item) const override
	{
		return true;
	}
};
