// include sdk headers to communicate with UE3
// WARNING: this header file can currently only be included once!
//   the SDK currently throws alot of warnings which can be ignored
#pragma warning(disable:4244)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <Proxy/Offsets.h>
#include <SdkHeaders.h>


// for access to event manager
#include <Proxy/Events.h>
// for access to client/server
#include <Proxy/Network.h>
// for module api 
#include <Proxy/Modules.h>
// for logging in main file
#include <Proxy/Logger.h>

// file operations
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>

using namespace BLRevive;

// this assumes single thread execution from the lazy event runner, set up locks if this changes
static void saveSettings(UObject*);

// hooking of this has been tricky, it's temporarily set to fire as soon as AFoxPC is ready
static void loadSettings(UObject*);

static void dumpUEKeybinds(AFoxPC* object);

static void logError(std::string message) {
    LError("settings-manager: " + message);
    LFlush;
}

static void logDebug(std::string message) {
    LDebug("settings-manager: " + message);
    LFlush;
}

static AFoxPC* getPlayerController() {
    static AFoxPC* playerController = nullptr;
    while (playerController == nullptr) {
        // copied from loadout manager, I don't understand why the player name "Player" is treated differently
        auto apc = UObject::GetInstanceOf<AFoxPC>();
        const char* playerName = nullptr;
        if (apc != nullptr && apc->PlayerReplicationInfo != nullptr) {
            playerName = apc->PlayerReplicationInfo->PlayerName.ToChar();
        }
        if (apc != NULL && apc->PlayerReplicationInfo && std::string(playerName) != "Player") {
            playerController = apc;
        }
        if (playerName != nullptr) {
            free((void*)playerName);
        }
        if (playerController == nullptr) {
            Sleep(100);
        }
    }
    return playerController;
}


static const std::string keybinding_file = "\\keybinding.json";

/// <summary>
/// Thread thats specific to the module (function must exist and export demangled!)
/// </summary>
extern "C" __declspec(dllexport) void ModuleThread()
{
    // put your code here
    if (Utils::IsServer()) {
        return;
    }
}

/// <summary>
/// Module initializer (function must exist and export demangled!)
/// </summary>
/// <param name="data"></param>
extern "C" __declspec(dllexport) void InitializeModule(std::shared_ptr<Module::InitData> data)
{
    if (Utils::IsServer()) {
        return;
    }
    // check param validity
    if (!data || !data->EventManager || !data->Logger) {
        LError("module initializer param was null!"); LFlush;
        return;
    }

    // initialize logger (to enable logging to the same file)
    Logger::Link(data->Logger);

    // initialize event manager
    // an instance of the manager can be retrieved with Events::Manager::Instance() afterwards
    Events::Manager::Link(data->EventManager);

    // initialize your module
    std::shared_ptr<Events::Manager> eventManager = Events::Manager::GetInstance();

    eventManager->RegisterHandler({
        Events::ID("*", "ei_ApplyChanges"),
        [=](Events::Info info) {
            logDebug(std::format("processing {0} ei_ApplyChanges ", info.Object->GetName()));
            saveSettings(info.Object);
        },
        false // cannot blocking process ei_ApplyChanges, it will freeze for some reason
        });
    logDebug("Registered handler for * ei_ApplyChanges");

    eventManager->RegisterHandler({
        // inject settings during set to defaults
        Events::ID("FoxPC", "OnReadProfileSettingsComplete"),
        [=](Events::Info info) {
            logDebug("processing FoxPC OnReadProfileSettingsComplete");
            loadSettings(info.Object);
        },
        true
        });
    logDebug("Registered handler for FoxDataStore_OnlinePlayerData OnRegister");

    // dumping events
    /*
    eventManager->RegisterHandler({
        Events::ID("*", "*"),
        [=](Events::Info info) {
            //logDebug(std::format("usable event: ({0}) ({1}) ", info.Object->GetName(), info.Function->GetName()));
        },
        true
        });
    logDebug("Registered handler for event listing");
    */

    // dumping keybinds
    /*
    eventManager->RegisterHandler({
    Events::ID("FoxPC", "OnReadProfileSettingsComplete"),
    [=](Events::Info info) {
        dumpUEKeybinds((AFoxPC *)info.Object);
    },
    true
        });
    logDebug("Registered handler for event FoxPC OnReadProfileSettingsComplete");
    */
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

static std::string getOutputPath(){
    std::string outputPath = Utils::FS::BlreviveConfigPath() + "settings_manager_" + std::string(Utils::URL::Param::String("Name", "default"));

    struct stat info;
    if (stat(outputPath.c_str(), &info) == 0) {
        // path exists
        if (info.st_mode & S_IFDIR) {
            // path exists and is a dir
            return outputPath;
        }
        else {
            logError(std::format("{0} exists but it is not a directory", outputPath));
            return "";
        }
    }
    else {
        // path do not exist
        if (CreateDirectory(outputPath.c_str(), nullptr)) {
            return outputPath;
        }
        else {
            logError(std::format("cannot create directory {0}", outputPath));
            return "";
        }
    }
}

static void saveVideo(UFoxSettingsUIVideo *object) {
    logDebug("saving video settings...");
}

static void saveGameplay(UFoxSettingsUIGameplay *object) {
    logDebug("saving gameplay settings...");
}

static void saveAudio(UFoxSettingsUIAudio *object) {
    logDebug("saving audio settings...");
}

static void saveCrosshair(UFoxSettingsUICrosshair *object) {
    logDebug("saving corsshair settings...");
}

static void saveControls(UFoxSettingsUIControls *object) {
    logDebug("saving control settings...");
}

static void saveKeyBindings(UFoxSettingsUIKeyBindings *object) {
    logDebug("saving keybinds...");

    json jsonOut;
    TArray<struct FKeyBindInfo> keybinds = object->CurrentKeyBindings;
    for (int i = 0;i < keybinds.Num();i++) {
        struct FKeyBindInfo keybind = keybinds(i);
        const char* command = keybind.CommandName.ToChar();
        jsonOut[command]["primary"] = keybind.PrimaryKey.GetName();
        jsonOut[command]["alternate"] = keybind.AlternateKey.GetName();
        free((void*)command);
    }

    std::string outputPath = getOutputPath();
    if (outputPath.length() == 0) {
        logError("failed saving keybinds");
        return;
    }

    std::ofstream output(outputPath + keybinding_file);
    if (!output.is_open()) {
        logError(std::format("failed opening {0} for writing", outputPath));
        logError("failed saving keybinds");
        return;
    }
    output << jsonOut.dump(4) << std::endl;
    output.close();
}

static void saveSettings(UObject *object) {
    logDebug("saving settings...");
    const char* objectName = object->GetName();
    if (strcmp(objectName, "FoxSettingsUIVideo") == 0) {
        saveVideo((UFoxSettingsUIVideo *)object);
    }
    else if (strcmp(objectName, "FoxSettingsUIGameplay") == 0) {
        saveGameplay((UFoxSettingsUIGameplay*)object);
    }
    else if (strcmp(objectName, "FoxSettingsUIAudio") == 0) {
        saveAudio((UFoxSettingsUIAudio*)object);
    }
    else if (strcmp(objectName, "FoxSettingsUICrosshair") == 0) {
        saveCrosshair((UFoxSettingsUICrosshair *)object);
    }
    else if (strcmp(objectName, "FoxSettingsUIControls") == 0) {
        saveControls((UFoxSettingsUIControls *)object);
    }
    else if (strcmp(objectName, "FoxSettingsUIKeyBindings") == 0) {
        saveKeyBindings((UFoxSettingsUIKeyBindings *)object);
    }
    else {
        logDebug(std::format("{0} saving is not implemented", object->GetName()));
    }
}

static const char* const command_menu_map[] = {
        "GBA_Fire",
        "GBA_ToggleZoom",
        "GBA_MoveForward",
        "GBA_MoveBackward",
        "GBA_StrafeLeft",
        "GBA_StrafeRight",
        "GBA_Sprint",
        "GBA_ToggleVisor",
        "GBA_Melee",
        "GBA_PickupWeapon",
        "GBA_Reload",
        "GBA_HoldCrouch",
        "GBA_Jump",
        "GBA_UseObject",
        "GBA_SelectPrimaryWeapon",
        "GBA_SelectSecondaryWeapon",
        "GBA_SelectTactical",
        "GBA_LastWeapon",
        "GBA_NextWeapon",
        "GBA_PrevWeapon",
        "GBA_SwitchGear1",
        "GBA_SwitchGear2",
        "GBA_SwitchGear3",
        "GBA_SwitchGear4",
        "GBA_Taunt",
        "GBA_Chat",
        "GBA_QuickGear"
};

static void loadKeyBindings(AFoxPC* object) {
    logDebug("loading keybinds...");
    std::string outputPath = getOutputPath();
    if (outputPath.length() == 0) {
        logError("cannot load keybinds");
    }

    std::ifstream input(outputPath + keybinding_file);
    if (!input.is_open()) {
        logError("cannot open " + outputPath + keybinding_file + " for reading");
        logError("failed loading keybinds");
        return;
    }

    json inputJson;
    try {
        inputJson = json::parse(input);
    }
    catch (json::exception e) {
        logError("cannot parse" + outputPath + keybinding_file);
        logError(e.what());
        return;
    }

    TArray<FKeyBind> &keybinds = object->MyFoxInput->Bindings;
    for (int i = 0;i < keybinds.Num(); i++) {
        FKeyBind &entry = keybinds(i);
        const char* command = entry.Command.ToChar();
        for (int j = 0; j < sizeof(command_menu_map) / sizeof(char*); j++) {
            if (strcmp(command, command_menu_map[j]) == 0) {
                try {
                    std::string primary = inputJson[command]["primary"];
                    std::string secondary = inputJson[command]["alternate"];
                    logDebug(std::format("binding {0} to {1} and {2}", command, primary, secondary));
                    FName primaryKey(primary.c_str());
                    FName secondaryKey(secondary.c_str());
                    entry.Name = primaryKey;
                    entry.SecondaryName = secondaryKey;
                }
                catch (json::exception e) {
                    logError("unexpected format in " + outputPath + keybinding_file);
                    logError(e.what());
                    return;
                }
                break;
            }
        }
        free((void*)command);
    }

    logDebug("finished loading keybinds");
}

static void loadSettings(UObject* object) {
    const char* objectName = object->GetName();
    if (strcmp("FoxPC", objectName) == 0) {
        logDebug("loading settings...");
        loadKeyBindings((AFoxPC*)object);
    }
    else {
        logDebug(std::format("ignoring unexpected event from {0}", objectName));
    }
}

static void dumpUEKeybinds(AFoxPC* object) {
    UFoxPlayerInput* inputObj = object->MyFoxInput;

    for (int i = 0;i < inputObj->CommandMappings.Num();i++) {
        FKeyBind keyBind = inputObj->CommandMappings(i);
        const char* command = keyBind.Command.ToChar();
        LDebug("settings-manager: keybind from CommandMappings ({0}) ({1}) ({2}) ({3})", command, keyBind.Name.GetName(), keyBind.SecondaryName.GetName(), keyBind.GamePadName.GetName());
        free((void*)command);
    }

    for (int i = 0;i < inputObj->PreDefinedPCBindings.Num();i++) {
        FKeyBind keyBind = inputObj->PreDefinedPCBindings(i);
        const char* command = keyBind.Command.ToChar();
        LDebug("settings-manager: keybind from PreDefinedPCBindings ({0}) ({1}) ({2}) ({3})", command, keyBind.Name.GetName(), keyBind.SecondaryName.GetName(), keyBind.GamePadName.GetName());
        free((void*)command);
    }

    for (int i = 0;i < inputObj->PreDefinedControllerBindings.Num();i++) {
        FKeyBind keyBind = inputObj->PreDefinedControllerBindings(i);
        const char* command = keyBind.Command.ToChar();
        LDebug("settings-manager: keybind from PreDefinedControllerBindings ({0}) ({1}) ({2}) ({3})", command, keyBind.Name.GetName(), keyBind.SecondaryName.GetName(), keyBind.GamePadName.GetName());
        free((void*)command);
    }

    for (int i = 0;i < inputObj->ControllerBindings.Num();i++) {
        FKeyBind keyBind = inputObj->ControllerBindings(i);
        const char* command = keyBind.Command.ToChar();
        LDebug("settings-manager: keybind from ControllerBindings ({0}) ({1}) ({2}) ({3})", command, keyBind.Name.GetName(), keyBind.SecondaryName.GetName(), keyBind.GamePadName.GetName());
        free((void*)command);
    }

    for (int i = 0;i < inputObj->Bindings.Num();i++) {
        FKeyBind keyBind = inputObj->Bindings(i);
        const char* command = keyBind.Command.ToChar();
        LDebug("settings-manager: keybind from Bindings ({0}) ({1}) ({2}) ({3})", command, keyBind.Name.GetName(), keyBind.SecondaryName.GetName(), keyBind.GamePadName.GetName());
        free((void*)command);
    }
}