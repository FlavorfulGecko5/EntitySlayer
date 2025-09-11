class EntNode;
class EntityParser;

namespace EntityDiff {
	void Export(const EntNode& vanilla, const EntNode& modded, const char* outputpath);
	void Import(EntityParser& parser, const EntNode& diff, const char* LOGPATH, const char* FILENAME);
}
