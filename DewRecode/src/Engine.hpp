#pragma once
#include <ElDorito/ElDorito.hpp>
#include <map>
#include "Utils/Utils.hpp"

// handles game events and callbacks for different modules/plugins
// if you make any changes to this class make sure to update the exported interface (create a new interface + inherit from it if the interface already shipped)
class Engine : public IEngine
{
public:
	bool OnTick(TickCallback callback);
	bool OnWndProc(WNDPROC callback);
	bool OnEvent(std::string eventNamespace, std::string eventName, EventCallback callback);

	bool RemoveOnTick(TickCallback callback);
	bool RemoveOnWndProc(WNDPROC callback);
	bool RemoveOnEvent(std::string eventNamespace, std::string eventName, EventCallback callback);

	void Event(std::string eventNamespace, std::string eventName, void* param = 0);

	bool RegisterInterface(std::string interfaceName, void* ptrToInterface);
	void* CreateInterface(std::string interfaceName, int* returnCode);

	void PrintToConsole(std::string str);
	ConsoleBuffer* AddConsoleBuffer(ConsoleBuffer buffer);
	bool SetActiveConsoleBuffer(ConsoleBuffer* buffer);

	void ShowMessageBox(std::string title, std::string text, std::string tag, const StringArrayInitializerType& choices, UserInputBoxCallback callback);
	void ShowMessageBox(std::string title, std::string text, std::string tag, std::vector<std::string>& choices, UserInputBoxCallback callback);
	void ShowInputBox(std::string title, std::string text, std::string tag, std::string defaultText, UserInputBoxCallback callback);

	bool HasMainMenuShown() { return mainMenuHasShown; }

	HWND GetGameHWND() { return Pointer(0x199C014).Read<HWND>(); }
	Pointer GetMainTls(size_t offset = 0);

	std::string GetDoritoVersionString() { return Utils::Version::GetVersionString(); }
	DWORD GetDoritoVersionInt() { return Utils::Version::GetVersionInt(); }

	std::pair<int, int> GetGameResolution()
	{
		return std::pair<int, int>(Pointer(0x2301D08).Read<int>(), Pointer(0x2301D0C).Read<int>());
	}

	uint32_t GetServerIP();
	std::string GetPlayerName();

	// functions that aren't exposed over IEngine interface
	void Tick(const std::chrono::duration<double>& deltaTime);
	LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	Engine();
	~Engine();
private:
	bool mainMenuHasShown = false;
	bool hasFirstTickTocked = false;
	std::vector<TickCallback> tickCallbacks;
	std::vector<WNDPROC> wndProcCallbacks;
	std::map<std::string, std::vector<EventCallback>> eventCallbacks;
	std::map<std::string, void*> interfaces;

	PatchSet* enginePatchSet;
};
