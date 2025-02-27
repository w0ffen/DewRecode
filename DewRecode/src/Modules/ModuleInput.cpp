#include "ModuleInput.hpp"
#include <sstream>
#include <algorithm>
#include "../ElDorito.hpp"

namespace
{
	int controllerIndex = 0;

	bool VariableInputRawInputUpdate(const std::vector<std::string>& Arguments, std::string& returnInfo)
	{
		unsigned long value = ElDorito::Instance().Modules.Input.VarInputRawInput->ValueInt;

		std::string status = "disabled.";
		bool statusBool = value != 0;
		if (statusBool)
			status = "enabled.";

		returnInfo = "Raw input " + status;
		return true;
	}

	bool VariableInputControllerIndexUpdate(const std::vector<std::string>& Arguments, std::string& returnInfo)
	{
		unsigned long value = ElDorito::Instance().Modules.Input.VarInputControllerIndex->ValueInt;
		controllerIndex = value;
		return true;
	}


	__declspec(naked) void Input_ControllerIndexHook()
	{
		__asm
		{
			mov edi, controllerIndex
			mov esi, eax
			mov ebx, 0x238E714
			push 0x5128F4
			ret
		}
	}

	bool CommandBind(const std::vector<std::string>& Arguments, std::string& returnInfo)
	{
		if (Arguments.size() < 1)
		{
			returnInfo =  "Usage: Bind <key> [[+]command] [arguments]\n";
			returnInfo += "If the command starts with a +, then it will be ";
			returnInfo += "passed an argument of 1 on key down and 0 on key ";
			returnInfo += "up. Omit the command to unbind the key.";
			return false;
		}

		std::string command = "";

		for (size_t i = 1; i < Arguments.size(); i++)
		{
			command += Arguments.at(i) + " ";
		}

		auto& dorito = ElDorito::Instance();

		command = dorito.Utils.Trim(command);

		auto retVal = dorito.Commands.AddBinding(Arguments[0], command);
		if (retVal == BindingReturnValue::Success)
		{
			returnInfo = "Binding set";
			return true;
		}
		if (retVal == BindingReturnValue::ClearedBinding)
		{
			returnInfo = "Binding cleared";
			return true;
		}

		if (retVal == BindingReturnValue::UnknownKey)
			returnInfo = "Unrecognized key name: " + Arguments[0];
		else if (retVal == BindingReturnValue::InvalidArgument)
			returnInfo = "Invalid command";
		
		return false;
	}

	void KeyboardUpdated(void* param)
	{
		auto& dorito = ElDorito::Instance();
		for (auto i = 0; i < Blam::eKeyCodes_Count; i++)
		{
			const auto binding = dorito.Commands.GetBinding(i);
			if (binding->command.empty())
				continue; // Key is not bound

			// Read the key and swallow it
			auto keyCode = static_cast<Blam::KeyCodes>(i);
			auto keyTicks = dorito.Modules.InputPatches.GetKeyTicks(keyCode, Blam::eInputTypeSpecial);
			dorito.Modules.InputPatches.Swallow(keyCode);

			auto command = binding->command;
			auto isHold = command.at(0) == '+';
			if (isHold)
				command = command.substr(1);

			// We're only interested in the key if it was just pressed or if
			// this is a hold binding and it was just released
			if (keyTicks > 1 || (keyTicks == 0 && !(isHold && binding->active)))
				continue;

			if (isHold)
			{
				// The command is a hold binding - append an argument depending
				// on whether it was pressed or released
				if (keyTicks == 1)
					command += " 1";
				else if (keyTicks == 0)
					command += " 0";

				binding->active = (keyTicks > 0);
			}
			
			// Execute the command and print its result
			dorito.Modules.Console.PrintToConsole(dorito.Commands.Execute(command, true));
		}
	}
}

namespace Modules
{
	ModuleInput::ModuleInput() : ModuleBase("Input")
	{
		VarInputRawInput = AddVariableInt("RawInput", "rawinput", "Enables raw mouse input with no acceleration applied", eCommandFlagsArchived, 1, VariableInputRawInputUpdate);
		VarInputRawInput->ValueIntMin = 0;
		VarInputRawInput->ValueIntMax = 1;

		VarInputControllerIndex = AddVariableInt("ControllerIndex", "controllerindex", "Which controller index should be used for player 1", eCommandFlagsArchived, 0, VariableInputControllerIndexUpdate);
		VarInputControllerIndex->ValueIntMin = 0;
		VarInputControllerIndex->ValueIntMax = 3;

		AddModulePatches(
		{
			Patch("ControllerIndexNop", 0x5128F0, 0x90, 4)
		},
		{
			Hook("ControllerIndex", 0x5128EB, Input_ControllerIndexHook, HookType::Jmp)
		});

		AddCommand("Bind", "bind", "Binds a command to a key", eCommandFlagsNone, CommandBind, { "key", "[+]command", "arguments" });
		engine->OnEvent("Core", "Input.KeyboardUpdate", KeyboardUpdated);
	}
}

namespace
{

}