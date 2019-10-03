#pragma once
#include <map>

class IniFile
{
	typedef std::map<char*, char*> IniList;

private:
	char* filename;

public:
	std::map<char*, IniList> sections;
	bool autoSave;

	char* Get(const char* section, const char* key, char* dft);
	void Set(const char* section, const char* key, char* val);
	void Save(const char* filename);
	void Save();
	void Load(const char* filename);
};
