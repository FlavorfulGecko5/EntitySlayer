#include "wx/wx.h"
#include "wx/dataview.h"
#include "EntityEditor.h"
#include "FilterMenu.h"
#include <set>

class EntityModel : public wxDataViewModel {
	public:
	// EntityModels should not have ownership of the root or tree data
	static inline const std::string FILTER_NOLAYERS = "No Layers";
	EntNode* root;

	set<std::string> classFilters = {};
	set<std::string> inheritFilters = {};
	set<std::string> layerFilters = {};

	bool filterSpawnPosition = false;
	float spawnPoint[3] = {0.0, 0.0, 0.0};
	float radius = 0;

	void refreshFilterMenus(FilterList* layerMenu, FilterList* classMenu, FilterList* inheritMenu)
	{
		layerMenu->Clear();
		classMenu->Clear();
		inheritMenu->Clear();

		set<string_view> newLayers;
		set<string_view> newClasses;
		set<string_view> newInherits;

		EntNode** children = root->getChildBuffer();
		int childCount = root->getChildCount();

		newLayers.insert(string_view(FILTER_NOLAYERS));
		for (int i = 0; i < childCount; i++)
		{
			EntNode* current = children[i];
			{
				EntNode& classNode = (*current)["entityDef"]["class"];
				if (classNode.ValueLength() > 0)
					newClasses.insert(classNode.getValue());
			}
			{
				EntNode& inheritNode = (*current)["entityDef"]["inherit"];
				if (inheritNode.ValueLength() > 0)
					newInherits.insert(inheritNode.getValue());
			}
			{
				EntNode& layerNode = (*current)["layers"];
				if (layerNode.getChildCount() > 0)
				{
					EntNode** layerBuffer = layerNode.getChildBuffer();
					int layerCount = layerNode.getChildCount();
					for (int currentLayer = 0; currentLayer < layerCount; currentLayer++)
						newLayers.insert(layerBuffer[currentLayer]->getName());
				}
			}

		}

		wxArrayString layerArray;
		for(string_view s : newLayers)
			layerArray.push_back(wxString(s.data(), s.length()));
		layerMenu->Append(layerArray);

		wxArrayString classArray;
		for(string_view s : newClasses)
			classArray.push_back(wxString(s.data(), s.length()));
		classMenu->Append(classArray);

		wxArrayString inheritArray;
		for(string_view s : newInherits)
			inheritArray.push_back(wxString(s.data(), s.length()));
		inheritMenu->Append(inheritArray);
	}

	void SetFilters(FilterList* layerMenu, FilterList* classMenu, FilterList* inheritMenu,
		bool newSpawnFilterSetting, float newX, float newY, float newZ, float newR)
	{
		layerFilters.clear();
		classFilters.clear();
		inheritFilters.clear();

		for (int i = 0, max = layerMenu->GetCount(); i < max; i++)
			if(layerMenu->IsChecked(i))
				layerFilters.insert(string(layerMenu->GetString(i)));

		for (int i = 0, max = classMenu->GetCount(); i < max; i++)
			if (classMenu->IsChecked(i))
				classFilters.insert(string(classMenu->GetString(i)));

		for (int i = 0, max = inheritMenu->GetCount(); i < max; i++)
			if (inheritMenu->IsChecked(i))
				inheritFilters.insert(string(inheritMenu->GetString(i)));

		filterSpawnPosition = newSpawnFilterSetting;
		spawnPoint[0] = newX;
		spawnPoint[1] = newY;
		spawnPoint[2] = newZ;
		radius = newR;
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
			bool filterByClass = classFilters.size() > 0;
			bool filterByInherit = inheritFilters.size() > 0;
			bool filterByLayer = layerFilters.size() > 0;
			bool noLayerFilter = layerFilters.count(FILTER_NOLAYERS) > 0;

			for (int i = 0; i < childCount; i++)
			{
				if (filterByClass)
				{
					string_view classVal = (*childBuffer[i])["entityDef"]["class"].getValue();
					if (classFilters.count(string(classVal)) < 1) // TODO: OPTIMIZE ACCESS CASTING
						continue;
				}

				if (filterByInherit)
				{
					string_view inheritVal = (*childBuffer[i])["entityDef"]["inherit"].getValue();
					if (inheritFilters.count(string(inheritVal)) < 1) // TODO: OPTIMIZE CASTING
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
						string_view l = layers[currentLayer]->getName();
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
					// If spawnPosition is undefined, we exclude (instead of assuming (0, 0, 0))
					// If a variable is undefined, we assume default value of 0
					EntNode& positionNode = (*childBuffer[i])["entityDef"]["edit"]["spawnPosition"];
					if(&positionNode == EntNode::SEARCH_404)
						continue;
					EntNode& xNode = positionNode["x"];
					EntNode& yNode = positionNode["y"];
					EntNode& zNode = positionNode["z"];

					float x = 0, y = 0, z = 0;
					try {
						// Need comparisons to distinguish between actual formatting exception
						// and exception from trying to convert "" to a float
						if(&xNode != EntNode::SEARCH_404)
							x = std::stof(string(xNode.getValue()));

						if (&yNode != EntNode::SEARCH_404)
							y = std::stof(string(yNode.getValue()));

						if (&zNode != EntNode::SEARCH_404)
							z = std::stof(string(zNode.getValue()));
					}
					catch (exception) {
						continue;
					}

					float deltaX = x - spawnPoint[0],
						  deltaY = y - spawnPoint[1],
						  deltaZ = z - spawnPoint[2];
					float distance = std::sqrtf(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

					if(distance > radius)
						continue;
				}

				// Node has passed all filters and should be included in the filtered tree
				array.Add(wxDataViewItem((void*)childBuffer[i]));
			}

			wxLogMessage("%zu", array.size());
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
