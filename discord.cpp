#include "asspull.h"
#include <discord_rpc.h>
#include <time.h>

namespace Discord
{
	bool enabled = false;
	const char* ApplicationId = "709359522732441653";
	
	typedef void (WINAPI* INITIALIZEPROC) (const char* applicationId, DiscordEventHandlers* handlers, int autoRegister, const char* optionalSteamId);
	typedef void (WINAPI* UPDATEPRESENCEPROC) (const DiscordRichPresence* presence);
	typedef void (WINAPI* CLEARPRESENCEPROC) (void);
	typedef void (WINAPI* SHUTDOWNPROC) (void);
	INITIALIZEPROC __Discord_Initialize;
	UPDATEPRESENCEPROC __Discord_UpdatePresence;
	CLEARPRESENCEPROC __Discord_ClearPresence;
	SHUTDOWNPROC __Discord_Shutdown;

	HMODULE discordDLL = NULL;

	void SetPresence(char* gameName);

	void Initialize()
	{
		if (!enabled)
			return;

		discordDLL = LoadLibraryExA("discord-rpc.dll", NULL, 0);
		if (discordDLL == NULL)
		{
			Log(logWarning, UI::GetString(IDS_DISCORDDLL)); //"Discord is enabled but the DLL isn't here."
			enabled = false;
			return;
		}
		__Discord_Initialize = (INITIALIZEPROC)GetProcAddress(discordDLL, "Discord_Initialize");
		__Discord_UpdatePresence = (UPDATEPRESENCEPROC)GetProcAddress(discordDLL, "Discord_UpdatePresence");
		__Discord_ClearPresence = (CLEARPRESENCEPROC)GetProcAddress(discordDLL, "Discord_ClearPresence");
		__Discord_Shutdown = (SHUTDOWNPROC)GetProcAddress(discordDLL, "Discord_Shutdown");

		DiscordEventHandlers handlers = {};
		__Discord_Initialize(ApplicationId, &handlers, 1, nullptr);
		SetPresence(NULL);
	}

	void SetPresence(char* gameName)
	{
		if (!enabled)
			return;
		WCHAR wName[128] = { 0 };
		if (gameName == NULL)
		{
			wcscpy(wName, UI::GetString(IDS_NOTPLAYING));
			gameName = "";
		}
		else
			mbstowcs(wName, gameName, ARRAYSIZE(wName));
		Log(L"Discord: \"%s\"", wName);
		DiscordRichPresence discord_presence = {};
		discord_presence.details = gameName;
		discord_presence.startTimestamp = time(NULL);
		discord_presence.smallImageKey = "a3x";
		discord_presence.largeImageText = gameName;
		char key[32] = {};
		for (auto i = 0; i < 32 && gameName[i] != 0; i++)
			key[i] = (char)tolower(gameName[i]);
		discord_presence.largeImageKey = key;
		__Discord_UpdatePresence(&discord_presence);
	}

	void Shutdown()
	{
		if (!enabled)
			return;
		__Discord_ClearPresence();
		__Discord_Shutdown();

		FreeModule(discordDLL);
	}
}
