#include "asspull.h"
#if _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#define strcmpi strcmp
#define _getcwd getcwd
#define _chdir chdir
#endif
#include <map>
#include <vector>

char const* IniFile::Get(const char* section, const char* key, char const* dft)
{
	for (auto tit = sections.begin(); tit != sections.end(); tit++)
	{
		if (!strcmpi(tit->first, section))
		{
			for (auto tat = tit->second.begin(); tat != tit->second.end(); tat++)
			{
				if (!strcmpi(tat->first, key))
					return tat->second;
			}
		}
	}
	return dft;
}

void IniFile::Set(const char* section, const char* key, char const* val)
{
	for (auto tit = sections.begin(); tit != sections.end(); tit++)
	{
		if (!strcmpi(tit->first, section))
		{
			for (auto tat = tit->second.begin(); tat != tit->second.end(); tat++)
			{
				if (!strcmpi(tat->first, key))
				{
					tat->second = strdup(val);
					if (autoSave) Save(filename);
					return;
				}
			}
			//didn't exist yet.
			tit->second.insert(std::pair<char*, char*>(strdup(key), strdup(val)));
			if (autoSave) Save(filename);
			return;
		}
	}
	//clearly, the section didn't exist. try adding one and recurse.
	auto newSect = new IniList();
	sections[(char*)section] = *newSect;
	Set(section, key, val);
}

void IniFile::Save(const char* filename)
{
	_chdir(this->cwd);

	FILE* fd = fopen(filename, "w");
	if (!fd)
		return;

	for (auto tit = sections.begin(); tit != sections.end(); tit++)
	{
		fprintf(fd, "[%s]\n", tit->first);
		for (auto tat = tit->second.begin(); tat != tit->second.end(); tat++)
		{
			fprintf(fd, "%s=%s\n", tat->first, tat->second);
		}
		fprintf(fd, "\n");
	}
	fclose(fd);
}

void IniFile::Save()
{
	Save(this->filename);
}

bool isspace(char isIt)
{
	return (isIt == ' ' || isIt == '\n' || isIt == '\r' || isIt == '\t');
}

void IniFile::Load(const char* filename)
{
	FILE* fd = fopen(filename, "r");
	if (!fd)
		return;

	this->filename = strdup(filename);
	this->cwd = (char*)malloc(FILENAME_MAX);
	_getcwd(this->cwd, FILENAME_MAX);

	char buffer[255];
	char* b = buffer;
	auto thisSect = new IniList();
	char* sectName;
	while(!feof(fd))
	{
		char c = fgetc(fd);
		if (c == -1)
			break;
		if (c == '[')
		{
			if (thisSect->size()) sections[sectName] = *thisSect;
			thisSect->clear();
			b = buffer;
			while (!feof(fd))
			{
				c = fgetc(fd);
				if (c == ']') break;
				*b++ = c;
			}
			*b++ = 0;
			sectName = strdup(buffer);
		}
		else if (c == ';')
			while (fgetc(fd) != '\n');
		else if (isspace(c))
			continue;
		else
		{
			//finally, some good fucking data.
			b = buffer;
			*b++ = c;
			while (!feof(fd))
			{
				c = fgetc(fd);
				if (c == '=')
					break;
				if (isspace(c))
					continue;
				*b++ = c;
			}
			*b++ = 0;
			char* key = strdup(buffer);
			char* valStart = b;
			while (!feof(fd))
			{
				c = fgetc(fd);
				if (c == '\n' || c == '\r')
					break;
				if (c == ';')
				{
					while (!feof(fd) && fgetc(fd) != '\n');
					break;
				}
				//if (isspace(c))
				//	continue;
				*b++ = c;
			}
			*b++ = 0;
			//we now have our key and value -- add them.
			thisSect->insert(std::pair<char*, char*>(key, strdup(valStart)));
		}
	}
	//add final section if any
	if (thisSect->size()) sections[sectName] = *thisSect;
	fclose(fd);
}
