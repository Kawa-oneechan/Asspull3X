#include "asspull.h"
#include <map>
#include <vector>

char* IniFile::Get(const char* section, const char* key, char* dft)
{
	for (auto tit = sections.begin(); tit != sections.end(); tit++)
	{
		if (!_stricmp(tit->first, section))
		{
			for (auto tat = tit->second.begin(); tat != tit->second.end(); tat++)
			{
				if (!_stricmp(tat->first, key))
					return tat->second;
			}
		}
	}
	return dft;
}

void IniFile::Set(const char* section, const char* key, char* val)
{
	for (auto tit = sections.begin(); tit != sections.end(); tit++)
	{
		if (!_stricmp(tit->first, section))
		{
			for (auto tat = tit->second.begin(); tat != tit->second.end(); tat++)
			{
				if (!_stricmp(tat->first, key))
				{
					tat->second = _strdup(val);
					if (autoSave) Save(filename);
					return;
				}
			}
			//didn't exist yet.
			tit->second.insert(std::pair<char*, char*>(_strdup(key), _strdup(val)));
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
	FILE* fd;
	auto r = fopen_s(&fd, filename, "w");
	if (r)
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

void IniFile::Load(const char* filename)
{
	FILE* fd;
	auto r = fopen_s(&fd, filename, "r");
	if (r)
		return;

	this->filename = _strdup(filename);

	char buffer[255];
	char* b = buffer;
	auto thisSect = new IniList();
	char* sectName;
	while(!feof(fd))
	{
		char c = fgetc(fd);
		if (c == 'ÿ')
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
			sectName = _strdup(buffer);
		}
		else if (c == ';')
			while (fgetc(fd) != '\n');
		else if (iswspace(c))
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
				if (iswspace(c))
					continue;
				*b++ = c;
			}
			*b++ = 0;
			char* key = _strdup(buffer);
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
				//if (iswspace(c))
				//	continue;
				*b++ = c;
			}
			*b++ = 0;
			//we now have our key and value -- add them.
			thisSect->insert(std::pair<char*, char*>(key, _strdup(valStart)));
		}
	}
	//add final section if any
	if (thisSect->size()) sections[sectName] = *thisSect;
	fclose(fd);
}
