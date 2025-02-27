#include "Commands.hpp"
#include <algorithm>
#include <sstream>
#include "ElDorito.hpp"
#include "Modules/ModuleInput.hpp"

namespace
{
	// Maps key names to key code values
	extern std::map<std::string, Blam::KeyCodes> keyCodes;
}

/// <summary>
/// Adds a command to the console commands list.
/// </summary>
/// <param name="command">The command to add.</param>
/// <returns>A pointer to the command, if added successfully.</returns>
Command* Commands::Add(Command command)
{
	if (Find(command.Name) || Find(command.ShortName))
		return nullptr;

	this->List.push_back(command);

	return &this->List.back();
}

/// <summary>
/// Finalizes adding all commands: calls the update event for each command, ensuring default values are applied.
/// </summary>
void Commands::FinishAdd()
{
	for (auto command : List)
	{
		if (command.Type != CommandType::Command && (command.Flags & eCommandFlagsDontUpdateInitial) != eCommandFlagsDontUpdateInitial)
			if (command.UpdateEvent)
				command.UpdateEvent(std::vector<std::string>(), std::string());
	}
}

/// <summary>
/// Finds a command based on its name.
/// </summary>
/// <param name="name">The name of the command.</param>
/// <returns>A pointer to the command, if found.</returns>
Command* Commands::Find(const std::string& name)
{
	for (auto it = List.begin(); it < List.end(); it++)
		if ((!it->Name.empty() && !_stricmp(it->Name.c_str(), name.c_str())) || (!it->ShortName.empty() && !_stricmp(it->ShortName.c_str(), name.c_str())))
			return &(*it);

	return nullptr;
}

/// <summary>
/// Executes a command string (vector splits spaces?)
/// </summary>
/// <param name="command">The command string.</param>
/// <param name="isUserInput">Whether the command came from the user or internally.</param>
/// <returns>The output of the executed command.</returns>
std::string Commands::Execute(std::vector<std::string> command, bool isUserInput)
{
	std::string commandStr = "";
	for (auto cmd : command)
		commandStr += "\"" + cmd + "\" ";

	return Execute(commandStr, isUserInput);
}

/// <summary>
/// Executes a command string
/// </summary>
/// <param name="command">The command string.</param>
/// <param name="isUserInput">Whether the command came from the user or internally.</param>
/// <returns>The output of the executed command.</returns>
std::string Commands::Execute(std::string command, bool isUserInput)
{
	int numArgs = 0;
	auto args = CommandLineToArgvA((char*)command.c_str(), &numArgs);

	if (numArgs <= 0)
		return "Invalid input";

	auto cmd = Find(args[0]);
	if (!cmd || (isUserInput && cmd->Flags & eCommandFlagsInternal))
		return "Command/Variable not found";

	if ((cmd->Flags & eCommandFlagsRunOnMainMenu) && !ElDorito::Instance().Engine.HasMainMenuShown())
	{
		queuedCommands.push_back(command);
		return "Command queued until mainmenu shows";
	}

	if ((cmd->Flags & eCommandFlagsMustBeHosting))// TODO1: && !ElDorito::Instance().IsHostPlayer())
		return "Only a player hosting a game can use this command";

	if ((cmd->Flags & eCommandFlagsReplicated))// TODO1: check if player is host or on mainmenu
		return "You must be at the lobby or hosting a game to use this command";

	std::vector<std::string> argsVect;
	if (numArgs > 1)
		for (int i = 1; i < numArgs; i++)
			argsVect.push_back(args[i]);

	if (cmd->Type == CommandType::Command)
	{
		std::string retInfo;
		cmd->UpdateEvent(argsVect, retInfo); // if it's a command call it and return
		return retInfo;
	}

	std::string previousValue;
	auto updateRet = SetVariable(cmd, (numArgs > 1 ? argsVect[0] : ""), previousValue);

	switch (updateRet)
	{
	case VariableSetReturnValue::Error:
		return "Command/Variable not found";
	case VariableSetReturnValue::InvalidArgument:
		return "Invalid value";
	case VariableSetReturnValue::OutOfRange:
		if (cmd->Type == CommandType::VariableInt)
			return "Value " + argsVect[0] + " out of range [" + std::to_string(cmd->ValueIntMin) + ".." + std::to_string(cmd->ValueIntMax) + "]";
		if (cmd->Type == CommandType::VariableInt64)
			return "Value " + argsVect[0] + " out of range [" + std::to_string(cmd->ValueInt64Min) + ".." + std::to_string(cmd->ValueInt64Max) + "]";
		if (cmd->Type == CommandType::VariableFloat)
			return "Value " + argsVect[0] + " out of range [" + std::to_string(cmd->ValueFloatMin) + ".." + std::to_string(cmd->ValueFloatMax) + "]";

		return "Value " + argsVect[0] + " out of range [this shouldn't be happening!]";
	}

	// special case for blanking strings
	if (cmd->Type == CommandType::VariableString && numArgs > 1 && argsVect[0].empty())
		cmd->ValueString = "";

	if (numArgs <= 1)
		return previousValue;

	if (!cmd->UpdateEvent)
		return previousValue + " -> " + cmd->ValueString; // no update event, so we'll just return with what we set the value to

	std::string retVal;
	auto ret = cmd->UpdateEvent(argsVect, retVal);

	if (!ret) // error, revert the variable
		this->SetVariable(cmd, previousValue, std::string());

	if (retVal.empty())
		return previousValue + " -> " + cmd->ValueString;

	return retVal;
}

/// <summary>
/// Executes a list of commands, seperated by new lines
/// </summary>
/// <param name="commands">The command string.</param>
/// <param name="isUserInput">Whether the command came from the user or internally.</param>
/// <returns>Whether the command executed successfully.</returns>
std::string Commands::ExecuteList(std::string& commands, bool isUserInput)
{
	std::istringstream stream(commands);
	std::stringstream ss;
	std::string line;
	int lineIdx = 0;
	while (std::getline(stream, line))
	{
		if (!this->ExecuteWithStatus(line, isUserInput))
		{
			ss << "Error at line " << lineIdx << std::endl;
		}
		lineIdx++;
	}
	return ss.str();
}

/// <summary>
/// Executes a command string, returning a bool indicating success.
/// </summary>
/// <param name="command">The command string.</param>
/// <param name="isUserInput">Whether the command came from the user or internally.</param>
/// <returns>Whether the command executed successfully.</returns>
bool Commands::ExecuteWithStatus(std::string command, bool isUserInput)
{
	int numArgs = 0;
	auto args = CommandLineToArgvA((char*)command.c_str(), &numArgs);

	if (numArgs <= 0)
		return false;

	auto cmd = Find(args[0]);
	if (!cmd || (isUserInput && cmd->Flags & eCommandFlagsInternal))
		return false;

	std::vector<std::string> argsVect;
	if (numArgs > 1)
		for (int i = 1; i < numArgs; i++)
			argsVect.push_back(args[i]);

	if (cmd->Type == CommandType::Command)
	{
		cmd->UpdateEvent(argsVect, std::string()); // if it's a command call it and return
		return true;
	}

	std::string previousValue;
	auto updateRet = SetVariable(cmd, (numArgs > 1 ? argsVect[0] : ""), previousValue);

	if (updateRet != VariableSetReturnValue::Success)
		return false;

	if (numArgs <= 1)
		return true;

	if (!cmd->UpdateEvent)
		return true; // no update event, so we'll just return with what we set the value to

	auto ret = cmd->UpdateEvent(argsVect, std::string());

	if (ret) // error, revert the variable
		return true;

	// error, revert the variable
	this->SetVariable(cmd, previousValue, std::string());
	return false;
}

/// <summary>
/// Executes the command queue.
/// </summary>
/// <returns>Results of the executed commands.</returns>
std::string Commands::ExecuteQueue()
{
	std::stringstream ss;
	for (auto cmd : queuedCommands)
	{
		ss << Execute(cmd, true) << std::endl;
	}
	queuedCommands.clear();
	return ss.str();
}

/// <summary>
/// Gets the value of an int variable.
/// </summary>
/// <param name="name">The name of the variable.</param>
/// <param name="value">Returns the value of the variable.</param>
/// <returns>true if the value argument was updated, false if the variable isn't found or is the wrong type.</returns>
bool Commands::GetVariableInt(const std::string& name, unsigned long& value)
{
	auto command = Find(name);
	if (!command || command->Type != CommandType::VariableInt)
		return false;

	value = command->ValueInt;
	return true;
}

/// <summary>
/// Gets the value of an int64 variable.
/// </summary>
/// <param name="name">The name of the variable.</param>
/// <param name="value">Returns the value of the variable.</param>
/// <returns>true if the value argument was updated, false if the variable isn't found or is the wrong type.</returns>
bool Commands::GetVariableInt64(const std::string& name, unsigned long long& value)
{
	auto command = Find(name);
	if (!command || command->Type != CommandType::VariableInt64)
		return false;

	value = command->ValueInt64;
	return true;
}

/// <summary>
/// Gets the value of a float variable.
/// </summary>
/// <param name="name">The name of the variable.</param>
/// <param name="value">Returns the value of the variable.</param>
/// <returns>true if the value argument was updated, false if the variable isn't found or is the wrong type.</returns>
bool Commands::GetVariableFloat(const std::string& name, float& value)
{
	auto command = Find(name);
	if (!command || command->Type != CommandType::VariableFloat)
		return false;

	value = command->ValueFloat;
	return true;
}

/// <summary>
/// Gets the value of a string variable.
/// </summary>
/// <param name="name">The name of the variable.</param>
/// <param name="value">Returns the value of the variable.</param>
/// <returns>true if the value argument was updated, false if the variable isn't found or is the wrong type.</returns>
bool Commands::GetVariableString(const std::string& name, std::string& value)
{
	auto command = Find(name);
	if (!command || command->Type != CommandType::VariableString)
		return false;

	value = command->ValueString;
	return true;
}

/// <summary>
/// Sets a variable from a string, string is converted to the proper variable type.
/// </summary>
/// <param name="name">The name of the variable.</param>
/// <param name="value">The value to set.</param>
/// <param name="previousValue">The previous value of the variable.</param>
/// <returns>VariableSetReturnValue</returns>
VariableSetReturnValue Commands::SetVariable(const std::string& name, std::string& value, std::string& previousValue)
{
	auto command = Find(name);
	if (!command)
		return VariableSetReturnValue::Error;

	return SetVariable(command, value, previousValue);
}

/// <summary>
/// Sets a variable from a string, string is converted to the proper variable type.
/// </summary>
/// <param name="command">The variable to update.</param>
/// <param name="value">The value to set.</param>
/// <param name="previousValue">The previous value of the variable.</param>
/// <returns>VariableSetReturnValue</returns>
VariableSetReturnValue Commands::SetVariable(Command* command, std::string& value, std::string& previousValue)
{
	try {
		switch (command->Type)
		{
		case CommandType::VariableString:
			previousValue = command->ValueString;
			if (!value.empty())
				command->ValueString = value;
			break;
		case CommandType::VariableInt:
			previousValue = std::to_string(command->ValueInt);
			if (!value.empty())
			{
				auto newValue = std::stoul(value, 0, 0);
				if ((command->ValueIntMin || command->ValueIntMax) && (newValue < command->ValueIntMin || newValue > command->ValueIntMax))
					return VariableSetReturnValue::OutOfRange;

				command->ValueInt = newValue;
				command->ValueString = std::to_string(command->ValueInt); // set the ValueString too so we can print the value out easier
			}
			break;
		case CommandType::VariableInt64:
			previousValue = std::to_string(command->ValueInt64);
			if (!value.empty())
			{
				auto newValue = std::stoull(value, 0, 0);
				if ((command->ValueInt64Min || command->ValueInt64Max) && (newValue < command->ValueInt64Min || newValue > command->ValueInt64Max))
					return VariableSetReturnValue::OutOfRange;

				command->ValueInt64 = newValue;
				command->ValueString = std::to_string(command->ValueInt64); // set the ValueString too so we can print the value out easier
			}
			break;
		case CommandType::VariableFloat:
			previousValue = std::to_string(command->ValueFloat);
			if (!value.empty())
			{
				auto newValue = std::stof(value, 0);
				if ((command->ValueFloatMin || command->ValueFloatMax) && (newValue < command->ValueFloatMin || newValue > command->ValueFloatMax))
					return VariableSetReturnValue::OutOfRange;

				command->ValueFloat = newValue;
				command->ValueString = std::to_string(command->ValueFloat); // set the ValueString too so we can print the value out easier
			}
			break;
		}
	}
	catch (std::invalid_argument)
	{
		return VariableSetReturnValue::InvalidArgument;
	}
	catch (std::out_of_range)
	{
		return VariableSetReturnValue::InvalidArgument;
	}

	return VariableSetReturnValue::Success;
}

bool compare_commands(const Command& lhs, const Command& rhs)
{
	return lhs.Name < rhs.Name;
}

/// <summary>
/// Generates help text.
/// </summary>
/// <param name="moduleFilter">If set, only commands/variables belonging to this module will be printed.</param>
/// <returns>Help text.</returns>
std::string Commands::GenerateHelpText(std::string moduleFilter)
{
	std::deque<Command> tempCommands(List);

	std::sort(tempCommands.begin(), tempCommands.end(), compare_commands);
	std::stringstream ss;
	std::stringstream hasParent; // store commands with a parent module seperately, so they can be added to the main stringstream after the non-parent commands
	for (auto cmd : tempCommands)
	{
		if (cmd.Flags & eCommandFlagsHidden || cmd.Flags & eCommandFlagsInternal)
			continue;

		if (!moduleFilter.empty() && (cmd.ModuleName.empty() || _stricmp(cmd.ModuleName.c_str(), moduleFilter.c_str())))
			continue;

		std::string helpText = cmd.Name;
		if (cmd.Type != CommandType::Command && !(cmd.Flags & eCommandFlagsOmitValueInList))
			helpText += " " + cmd.ValueString;

		helpText += " - " + cmd.Description;

		if (!cmd.ModuleName.empty())
			hasParent << helpText << std::endl;
		else
			ss << helpText << std::endl;
	}

	ss << hasParent.str();

	return ss.str();
}

std::string Commands::GenerateHelpText(const Command& command)
{
	std::stringstream ss;
	ss << command.Name << " - " << command.Description << "." << std::endl;
	ss << "Usage: " << command.Name << " ";
	if (command.Type != CommandType::Command)
	{
		ss << "<value(";
		switch (command.Type)
		{
		case CommandType::VariableInt:
			ss << "int";
			break;
		case CommandType::VariableInt64:
			ss << "int64";
			break;
		case CommandType::VariableFloat:
			ss << "float";
			break;
		case CommandType::VariableString:
			ss << "string";
			break;
		}
		ss << ")>" << std::endl << "Current value: " << command.ValueString << std::endl << std::endl;
	}
	else
	{
		std::stringstream paramStream;
		for (auto arg : command.CommandArgs)
		{
			auto argname = arg;
			std::string argdesc = "";
			auto nameEnd = arg.find(" ");
			if (nameEnd != std::string::npos)
			{
				argname = arg.substr(0, nameEnd);
				if (arg.length() > (nameEnd + 1))
					argdesc = arg.substr(nameEnd + 1);
			}
			ss << "<" << argname << "> ";
			if (argdesc.length() > 0)
				paramStream << "  " << argname << ": " << argdesc << std::endl;
		}
		ss << std::endl;
		auto paramDescs = paramStream.str();
		if (paramDescs.length() > 0)
			ss << paramDescs << std::endl;
		else
			ss << std::endl;
	}

	return ss.str();
}

/// <summary>
/// Writes each variables name and value to a string.
/// </summary>
/// <returns>Each variables name and value.</returns>
std::string Commands::SaveVariables()
{
	std::stringstream ss;
	for (auto cmd : List)
	{
		if (cmd.Type == CommandType::Command || !(cmd.Flags & eCommandFlagsArchived) || (cmd.Flags & eCommandFlagsInternal))
			continue;

		ss << cmd.Name << " \"" << cmd.ValueString << "\"" << std::endl;
	}
	ss << std::endl;

	for (int i = 0; i < Blam::eKeyCodes_Count; i++)
	{
		auto& bind = bindings[i];
		if (bind.command.empty() || bind.key.empty())
			continue;

		ss << "Input.Bind " << bind.key << " " << bind.command;
		ss << std::endl;
	}
	return ss.str();
}

/// <summary>
/// Adds or clears a keyboard binding.
/// </summary>
/// <param name="key">The key to bind.</param>
/// <param name="command">The command to run (empty if clearing).</param>
/// <returns>BindingReturnValue</returns>
BindingReturnValue Commands::AddBinding(std::string key, std::string command)
{
	// Get the key, convert it to lowercase, and translate it to a key code
	auto actualKey = ElDorito::Instance().Utils.ToLower(key);
	auto it = keyCodes.find(key);
	if (it == keyCodes.end())
		return BindingReturnValue::UnknownKey;

	auto keyCode = it->second;
	auto binding = &bindings[keyCode];

	// If no command was specified, unset the binding
	if (command.empty())
	{
		binding->command.clear();
		return BindingReturnValue::ClearedBinding;
	}

	// Set the binding
	binding->key = actualKey;
	binding->command = command;
	return BindingReturnValue::Success;
}

/// <summary>
/// Gets the binding for a key.
/// </summary>
/// <param name="key">The key.</param>
/// <returns>A pointer to the KeyBinding struct for this key.</returns>
KeyBinding* Commands::GetBinding(std::string key)
{
	auto actualKey = ElDorito::Instance().Utils.ToLower(key);
	auto it = keyCodes.find(key);
	if (it == keyCodes.end())
		return nullptr;

	auto keyCode = it->second;
	return &bindings[keyCode];
}

/// <summary>
/// Gets the binding for a keycode.
/// </summary>
/// <param name="keyCode">The key code.</param>
/// <returns>A pointer to the KeyBinding struct for this key code.</returns>
KeyBinding* Commands::GetBinding(int keyCode)
{
	if (keyCode < 0 || keyCode >= Blam::eKeyCodes_Count)
		return nullptr;

	return &bindings[keyCode];
}

namespace
{
	// Key codes table
	std::map<std::string, Blam::KeyCodes> keyCodes =
	{
		{ "escape", Blam::eKeyCodesEscape },
		{ "f1", Blam::eKeyCodesF1 },
		{ "f2", Blam::eKeyCodesF2 },
		{ "f3", Blam::eKeyCodesF3 },
		{ "f4", Blam::eKeyCodesF4 },
		{ "f5", Blam::eKeyCodesF5 },
		{ "f6", Blam::eKeyCodesF6 },
		{ "f7", Blam::eKeyCodesF7 },
		{ "f8", Blam::eKeyCodesF8 },
		{ "f9", Blam::eKeyCodesF9 },
		{ "f10", Blam::eKeyCodesF10 },
		{ "f11", Blam::eKeyCodesF11 },
		{ "f12", Blam::eKeyCodesF12 },
		{ "printscreen", Blam::eKeyCodesPrintScreen },
		{ "f14", Blam::eKeyCodesF14 },
		{ "f15", Blam::eKeyCodesF15 },
		{ "tilde", Blam::eKeyCodesTilde },
		{ "1", Blam::eKeyCodes1 },
		{ "2", Blam::eKeyCodes2 },
		{ "3", Blam::eKeyCodes3 },
		{ "4", Blam::eKeyCodes4 },
		{ "5", Blam::eKeyCodes5 },
		{ "6", Blam::eKeyCodes6 },
		{ "7", Blam::eKeyCodes7 },
		{ "8", Blam::eKeyCodes8 },
		{ "9", Blam::eKeyCodes9 },
		{ "0", Blam::eKeyCodes0 },
		{ "minus", Blam::eKeyCodesMinus },
		{ "plus", Blam::eKeyCodesPlus },
		{ "back", Blam::eKeyCodesBack },
		{ "tab", Blam::eKeyCodesTab },
		{ "q", Blam::eKeyCodesQ },
		{ "w", Blam::eKeyCodesW },
		{ "e", Blam::eKeyCodesE },
		{ "r", Blam::eKeyCodesR },
		{ "t", Blam::eKeyCodesT },
		{ "y", Blam::eKeyCodesY },
		{ "u", Blam::eKeyCodesU },
		{ "i", Blam::eKeyCodesI },
		{ "o", Blam::eKeyCodesO },
		{ "p", Blam::eKeyCodesP },
		{ "lbracket", Blam::eKeyCodesLBracket },
		{ "rbracket", Blam::eKeyCodesRBracket },
		{ "pipe", Blam::eKeyCodesPipe },
		{ "capital", Blam::eKeyCodesCapital },
		{ "a", Blam::eKeyCodesA },
		{ "s", Blam::eKeyCodesS },
		{ "d", Blam::eKeyCodesD },
		{ "f", Blam::eKeyCodesF },
		{ "g", Blam::eKeyCodesG },
		{ "h", Blam::eKeyCodesH },
		{ "j", Blam::eKeyCodesJ },
		{ "k", Blam::eKeyCodesK },
		{ "l", Blam::eKeyCodesL },
		{ "colon", Blam::eKeyCodesColon },
		{ "quote", Blam::eKeyCodesQuote },
		{ "enter", Blam::eKeyCodesEnter },
		{ "lshift", Blam::eKeyCodesLShift },
		{ "z", Blam::eKeyCodesZ },
		{ "x", Blam::eKeyCodesX },
		{ "c", Blam::eKeyCodesC },
		{ "v", Blam::eKeyCodesV },
		{ "b", Blam::eKeyCodesB },
		{ "n", Blam::eKeyCodesN },
		{ "m", Blam::eKeyCodesM },
		{ "comma", Blam::eKeyCodesComma },
		{ "period", Blam::eKeyCodesPeriod },
		{ "question", Blam::eKeyCodesQuestion },
		{ "rshift", Blam::eKeyCodesRShift },
		{ "lcontrol", Blam::eKeyCodesLControl },
		{ "lmenu", Blam::eKeyCodesLmenu },
		{ "space", Blam::eKeyCodesSpace },
		{ "rmenu", Blam::eKeyCodesRmenu },
		{ "apps", Blam::eKeyCodesApps },
		{ "rcontrol", Blam::eKeyCodesRcontrol },
		{ "up", Blam::eKeyCodesUp },
		{ "down", Blam::eKeyCodesDown },
		{ "left", Blam::eKeyCodesLeft },
		{ "right", Blam::eKeyCodesRight },
		{ "insert", Blam::eKeyCodesInsert },
		{ "home", Blam::eKeyCodesHome },
		{ "pageup", Blam::eKeyCodesPageUp },
		{ "delete", Blam::eKeyCodesDelete },
		{ "end", Blam::eKeyCodesEnd },
		{ "pagedown", Blam::eKeyCodesPageDown },
		{ "numlock", Blam::eKeyCodesNumLock },
		{ "divide", Blam::eKeyCodesDivide },
		{ "multiply", Blam::eKeyCodesMultiply },
		{ "numpad0", Blam::eKeyCodesNumpad0 },
		{ "numpad1", Blam::eKeyCodesNumpad1 },
		{ "numpad2", Blam::eKeyCodesNumpad2 },
		{ "numpad3", Blam::eKeyCodesNumpad3 },
		{ "numpad4", Blam::eKeyCodesNumpad4 },
		{ "numpad5", Blam::eKeyCodesNumpad5 },
		{ "numpad6", Blam::eKeyCodesNumpad6 },
		{ "numpad7", Blam::eKeyCodesNumpad7 },
		{ "numpad8", Blam::eKeyCodesNumpad8 },
		{ "numpad9", Blam::eKeyCodesNumpad9 },
		{ "subtract", Blam::eKeyCodesSubtract },
		{ "add", Blam::eKeyCodesAdd },
		{ "numpadenter", Blam::eKeyCodesNumpadEnter },
		{ "decimal", Blam::eKeyCodesDecimal },
		{ "shift", Blam::eKeyCodesShift },
		{ "ctrl", Blam::eKeyCodesCtrl },
		{ "menu", Blam::eKeyCodesMenu },
	};

	char** CommandLineToArgvA(char* CmdLine, int* _argc)
	{
		char** argv;
		char*  _argv;
		unsigned long len;
		unsigned long argc;
		char   a;
		unsigned long i, j;

		bool in_QM;
		bool in_TEXT;
		bool in_SPACE;

		len = strlen(CmdLine);
		i = ((len + 2) / 2)*sizeof(void*) + sizeof(void*);

		argv = (char**)malloc(i + (len + 2)*sizeof(char));
		//argv = (char**)GlobalAlloc(GMEM_FIXED,
		//	i + (len + 2)*sizeof(char));

		if (!argv)
			return 0;

		_argv = (char*)(((unsigned char*)argv) + i);

		argc = 0;
		argv[argc] = _argv;
		in_QM = false;
		in_TEXT = false;
		in_SPACE = true;
		i = 0;
		j = 0;

		while (a = CmdLine[i]) {
			if (in_QM) {
				if (a == '\"') {
					in_QM = false;
				}
				else {
					_argv[j] = a;
					j++;
				}
			}
			else {
				switch (a) {
				case '\"':
					in_QM = true;
					in_TEXT = true;
					if (in_SPACE) {
						argv[argc] = _argv + j;
						argc++;
					}
					in_SPACE = false;
					break;
				case ' ':
				case '\t':
				case '\n':
				case '\r':
					if (in_TEXT) {
						_argv[j] = '\0';
						j++;
					}
					in_TEXT = false;
					in_SPACE = true;
					break;
				default:
					in_TEXT = true;
					if (in_SPACE) {
						argv[argc] = _argv + j;
						argc++;
					}
					_argv[j] = a;
					j++;
					in_SPACE = false;
					break;
				}
			}
			i++;
		}
		_argv[j] = '\0';
		argv[argc] = NULL;

		(*_argc) = argc;
		return argv;
	}
}