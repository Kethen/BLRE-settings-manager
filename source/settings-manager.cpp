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

#include <settings-manager/base64.h>

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
static void saveKeyBindingsFromCache();

static void loadProfileSettings(UFoxProfileSettings*, bool);
static void saveProfileSettings(UFoxProfileSettings*);

static void loadSettings(UObject*);

static void backupCrosshairDefaults();
static void backupProfileDefaults(UFoxProfileSettings*);
static void backupKeybindDefaults(AFoxPC*);
static void restoreDefaults(AFoxPC*);


static std::string getOutputPath();

static const char* const command_list[] = {
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

static const std::string keybinding_file = "\\keybinding.json";
static const std::string profile_file = "\\UE3_online_profile.json";

struct keybindCacheEntry {
    const char *command;
    std::string primary;
    std::string alternate;
};

struct profileSettingBackupEntry {
    char setting[sizeof(FOnlineProfileSetting)];
};

struct keybindBackupEntry {
    char primary[sizeof(FName)];
    char alternate[sizeof(FName)];
};


char crosshairNeutralDefault[sizeof(FString)];
char crosshairFriendlyDefault[sizeof(FString)];
char crosshairEnemyInRangeDefault[sizeof(FString)];
char crosshairEnemyOutRangeDefault[sizeof(FString)];
static keybindCacheEntry keybindCache[sizeof(command_list) / sizeof(char*)];
static bool keybindCachePopulated = false;
static bool settingsModified = false;
static bool keybindingsLoaded = false;
static bool profileSettingsLoaded = false;
static AFoxPC* playerController = nullptr;
static profileSettingBackupEntry* profileDefaults = nullptr;
static int numProfileDefaults = 0;
static keybindBackupEntry defaultKeybindings[sizeof(command_list) / sizeof(char*)];
static int periodicSaves = 0;
static bool defaultRestored = false;

static void logError(std::string message) {
    LError("settings-manager: " + message);
    LFlush;
}

static void logDebug(std::string message) {
    LDebug("settings-manager: " + message);
    LFlush;
}

static void logWarn(std::string message) {
    LWarn("settings-manager: " + message);
    LFlush;
}

static AFoxPC* getPlayerController() {
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


/// <summary>
/// Thread thats specific to the module (function must exist and export demangled!)
/// </summary>
extern "C" __declspec(dllexport) void ModuleThread()
{
    // put your code here
    if (Utils::IsServer()) {
        return;
    }

    std::shared_ptr<Events::Manager> eventManager = Events::Manager::GetInstance();

#if 1
    eventManager->RegisterHandler({
    Events::ID("FoxProfileSettingsPC", "SetToDefaults"),
        [=](Events::Info info) {
            backupCrosshairDefaults();
            backupProfileDefaults((UFoxProfileSettings*)info.Object);
            loadProfileSettings((UFoxProfileSettings*)info.Object, true);
            // items that gets overlayed during SetToDefaults
            backupKeybindDefaults(getPlayerController());
            loadSettings(getPlayerController());
        },
        true
        });
    logDebug("registered handler for event FoxProfileSettingsPC SetToDefaults");
#endif
#if 1
    eventManager->RegisterHandler({
        Events::ID("FoxPC", "ReceivedPlayer"),
        [=](Events::Info info) {
            playerController = (AFoxPC*)info.Object;
        },
        true
        });
    logDebug("registered handler for event FoxPC ReceivedPlayer");
#endif
#if 1
    eventManager->RegisterHandler({
    Events::ID("FoxMenuUI", "ui_ShowPreGameLobby"),
        [=](Events::Info info) {
            restoreDefaults(getPlayerController());
        },
        false
        });
    logDebug("registered handler for event FoxMenuUI ui_ShowPreGameLobby");
#endif
#if 1
    eventManager->RegisterHandler({
    Events::ID("FoxMenuUI", "ui_ShowIntermissionLobby"),
        [=](Events::Info info) {
            restoreDefaults(getPlayerController());
        },
        false
        });
    logDebug("registered handler for event FoxMenuUI ui_ShowIntermissionLobby");
#endif
#if 1
    eventManager->RegisterHandler({
        Events::ID("*", "ei_ApplyChanges"),
        [=](Events::Info info) {
            saveProfileSettings(getPlayerController()->ProfileSettings);
            // items that reuqires special care
            saveSettings(info.Object);
            settingsModified = true;
            periodicSaves = 0;
        },
        false /*block processing ei_ApplyChanges could freeze*/
        });
    logDebug("registered handler for event * ei_ApplyChanges");
#endif
#if 1
    eventManager->RegisterHandler({
        Events::ID("*", "PromptResetToDefaults"),
        [=](Events::Info info) {
            saveProfileSettings(getPlayerController()->ProfileSettings);
            // items that reuqires special care
            saveSettings(info.Object);
            settingsModified = true;
            periodicSaves = 0;
        },
        false /*block processing ei_ResetToDefaults could freeze*/
        });
    logDebug("registered handler for event * PromptResetToDefaults");
#endif
#if 1
    eventManager->RegisterHandler({
        Events::ID("FoxUI", "ei_menuTransitionComplete"),
        [=](Events::Info info) {
            if (settingsModified && periodicSaves < 3) {
                logDebug("performing periodic settings saving");
                saveProfileSettings(getPlayerController()->ProfileSettings);
                saveKeyBindingsFromCache();
                periodicSaves++;
            }
        },
        false
        });
    logDebug("registered handler for event FoxUI ei_menuTransitionComplete");
#endif
    /* hack, early kill the game so that it does not crash dump later at
    /*
    Address=0229FE5C
    To=0049EF51
    From=00F97729
    Size=28
    Comment=foxgame-win32-shipping-patched.00F97729
    Party=User
    */
#if 1
    eventManager->RegisterHandler({
        Events::ID("OnlineSubsystemPW", "Exit"),
        [=](Events::Info info) {
            logWarn("hack: exiting the game on event OnlineSubsystemPW Exit to avoid a crash");
            exit(0);
        },
        true
        });
    logDebug("registered handler for OnlineSubsystemPW Exit");
#endif
	// hack, exit on quit match button press so that the player joined earlier does not get DCed
#if 1
    eventManager->RegisterHandler({
        Events::ID("FoxMenuUI", "ei_QuitMatch"),
        [=](Events::Info info) {
            logWarn("hack: exiting the game on event FoxMenuUI ei_QuitMatch to not disconnect the player joined before you");
            exit(0);
        },
        true
        });
    logDebug("registered handler for FoxMenuUI ei_QuitMatch");
#endif
	// hack, exit on idle kick so that the player joined earlier does not get DCed
#if 1
    eventManager->RegisterHandler({
        Events::ID("FoxPC", "ClientKickedForIdle"),
        [=](Events::Info info) {
            logWarn("hack: exiting the game on event FoxPC ClientKickedForIdle to not disconnect the player joined before you");
            exit(0);
        },
        true
        });
    logDebug("registered handler for FoxPC ClientKickedForIdle");
#endif
    // dumping events
#if 0
    eventManager->RegisterHandler({
        Events::ID("*", "*"),
        [=](Events::Info info) {
        },
        true
        });
    logDebug("registered handler for event listing");
#endif
}



/// <summary>
/// Module initializer (function must exist and export demangled!)
/// </summary>
/// <param name="data"></param>
extern "C" __declspec(dllexport) void InitializeModule(Module::InitData *data)
{
    if (Utils::IsServer()) {
        return;
    }
    // check param validity
    if (!data->EventManager || !data->Logger) {
        LError("module initializer param was null!"); LFlush;
        return;
    }

    // initialize logger (to enable logging to the same file)
    Logger::Link(data->Logger);

    // initialize event manager
    // an instance of the manager can be retrieved with Events::Manager::Instance() afterwards
    Events::Manager::Link(data->EventManager);

    // initialize your module
    getOutputPath();

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

static std::string getPlayerName() {
    static char *nameStr = nullptr;
    if (nameStr == nullptr) {
        std::string name = Utils::URL::Param::String("Name", "default");
        nameStr = new char[name.length() + 1];
        strcpy(nameStr, name.c_str());
        return name;
    }
    else {
        return std::string(nameStr);
    }
}

static std::string getConfigPath() {
    static char *pathStr = nullptr;
    if (pathStr == nullptr) {
        std::string path = Utils::FS::BlreviveConfigPath();
        pathStr = new char[path.length() + 1];
        strcpy(pathStr, path.c_str());
        return path;
    }
    else {
        return std::string(pathStr);
    }
}

static std::string getOutputPath(){
    std::string outputPath = getConfigPath() + "settings_manager_" + getPlayerName();

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

static void saveKeyBindingsToCache(UFoxSettingsUIKeyBindings* object) {
    logDebug("populating keybind cache...");
    TArray<struct FKeyBindInfo> keybinds = object->CurrentKeyBindings;
    for (int i = 0;i < keybinds.Num();i++) {
        FKeyBindInfo &keybind = keybinds(i);
        const char* command = keybind.CommandName.ToChar();
        for (int j = 0;j < sizeof(command_list) / sizeof(char*);j++) {
            if (strcmp(command, command_list[j]) == 0) {
                keybindCache[j].command = command_list[j];
                keybindCache[j].primary = std::string(keybind.PrimaryKey.GetName());
                keybindCache[j].alternate = std::string(keybind.AlternateKey.GetName());
                break;
            }
        }
        free((void*)command);
    }
    keybindCachePopulated = true;
}

static void saveKeyBindingsFromCache() {
    logDebug("saving keybind cache to disk...");
    if (!keybindCachePopulated) {
        return;
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

    json jsonOut;
    for (int i = 0;i < sizeof(command_list) / sizeof(char*);i++) {
        jsonOut[keybindCache[i].command]["primary"] = keybindCache[i].primary;
        jsonOut[keybindCache[i].command]["alternate"] = keybindCache[i].alternate;
    }

    output << jsonOut.dump(4) << std::endl;
    output.close();
}

static void saveKeyBindings(UFoxSettingsUIKeyBindings *object) {
    saveKeyBindingsToCache(object);
    saveKeyBindingsFromCache();
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

    json jsonIn;
    try {
        jsonIn = json::parse(input);
    }
    catch (json::exception e) {
        logError("cannot parse" + outputPath + keybinding_file);
        logError(e.what());
        return;
    }

    TArray<UUIResourceDataProvider*> bindings = object->GetMenuItemsDataStore()->KeyBindingProviders;
    for (int i = 0;i < bindings.Num();i++) {
        UFoxDataProvider_KeyBindings *entry = (UFoxDataProvider_KeyBindings *)bindings(i);
        const char* command = entry->CmdName.ToChar();
        for (int j = 0;j < sizeof(command_list) / sizeof(char*);j++) {
            if (strcmp(command, command_list[j]) == 0) {
                try {
                    std::string primary = jsonIn[command]["primary"];
                    std::string secondary = jsonIn[command]["alternate"];
                    FName primaryFName(primary.c_str());
                    FName secondaryFName(secondary.c_str());
                    logDebug(std::format("inserting keybinds for {0} as {1} and {2}", command, primary, secondary));
                    entry->DefaultPrimaryKey = primaryFName;
                    entry->DefaultAlternateKey = secondaryFName;
                }
                catch (json::exception e) {
                    logError(std::format("failed inserting keybinds for {0}",command));
                    logError("unexpected format in " + outputPath + keybinding_file);
                    logError(e.what());
                }
                break;
            }
        }
        free((void*)command);
    }

    keybindingsLoaded = true;
    logDebug("finished loading keybinds");
}

static void substituteCrosshairColor(UFoxUI *uiDefaultClass, FString &defaultCrosshair, int crosshairIndex){
    const char *defaultCrosshairName;
    const char *targetCrosshairName;
    if(crosshairIndex != -1 && crosshairIndex < uiDefaultClass->CrosshairColors.Num()){
        defaultCrosshairName = defaultCrosshair.ToChar();
        FString &targetCrosshair = uiDefaultClass->CrosshairColors(crosshairIndex).Name;
        targetCrosshairName = targetCrosshair.ToChar();
        if(strcmp(defaultCrosshairName, targetCrosshairName) != 0){
            logDebug(std::format("changing crosshair color from {0} to {1}", defaultCrosshairName, targetCrosshairName));
            memcpy(&defaultCrosshair, &targetCrosshair, sizeof(FString));
        }
        free((void *)defaultCrosshairName);
        free((void *)targetCrosshairName);
    }else{
        logError(std::format("crosshair index {0} is out of bound, max index is {1}", crosshairIndex, uiDefaultClass->CrosshairColors.Num() - 1));
    }
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


static void loadProfileSettings(UFoxProfileSettings* object, bool asDefault = false) {
    int crosshairNeutral = -1;
    int crosshairFriendly = -1;
    int crosshairEnemyInRange = -1;
    int crosshairEnemyOutRange = -1;
    if (asDefault) {
        logDebug("loading UE3 online profile as default");
    }
    else {
        logDebug("loading UE3 online profile");
    }

    std::string inputPath = getOutputPath();
    if (inputPath.length() == 0) {
        logError("failed loading UE3 online profile");
        return;
    }

    std::ifstream input(inputPath + profile_file);
    if (!input.is_open()) {
        logError(std::format("failed opening {0} for reading", inputPath + profile_file));
        logError("failed loading UE3 online profile");
        return;
    }

    json jsonIn;
    try {
        jsonIn = json::parse(input);
    }
    catch (json::exception e) {
        logError("cannot parse " + inputPath + profile_file);
        logError(e.what());
        return;
    }

    int i = 0;
    for (json::iterator it = jsonIn.begin(); it != jsonIn.end(); ++it) {
        try {
            FOnlineProfileSetting entry;
            entry.Owner = it.value()["Owner"];
            entry.ProfileSetting.PropertyId = it.value()["ProfileSetting"]["PropertyId"];
            entry.ProfileSetting.AdvertisementType = it.value()["ProfileSetting"]["AdvertisementType"];
            entry.ProfileSetting.Data.Type = it.value()["ProfileSetting"]["Data"]["Type"];
            if (entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Int32 || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Float) {
                entry.ProfileSetting.Data.Value1 = it.value()["ProfileSetting"]["Data"]["Value1"];
            }
            else if (entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Int64 || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Double || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_DateTime) {
                *(int64_t*)&entry.ProfileSetting.Data.Value1 = it.value()["ProfileSetting"]["Data"]["Value1"];
            }
            else if (entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_String || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Blob) {
                logError(std::format("SDT_String and SDT_Blob are currently not supported, skipping property id {0} restore", entry.ProfileSetting.PropertyId));
                continue;
            }
            else {
                logError(std::format("unexpected type ESettingsDataType::{0}", entry.ProfileSetting.Data.Type));
                continue;
            }

            logDebug(std::format("seeking existing data with id {0}", entry.ProfileSetting.PropertyId));
            TArray<FOnlineProfileSetting>* target;
            if (asDefault) {
                target = &object->DefaultSettings;
            }
            else {
                target = &object->ProfileSettings;
            }
            bool found = false;
            for (int j = 0;j < target->Num();j++) {
                FOnlineProfileSetting& existingEntry = (*target)(j);
                if (existingEntry.ProfileSetting.PropertyId == entry.ProfileSetting.PropertyId) {
                    logDebug(std::format("inserting data with type ESettingsDataType::{0} and id {1}", entry.ProfileSetting.Data.Type, entry.ProfileSetting.PropertyId));
                    memcpy(&existingEntry, &entry, sizeof(FOnlineProfileSetting));
                    found = true;
                    break;
                }
            }
            if (!found) {
                logDebug(std::format("existing entry with ESettingsDataType::{0} and id {1} not found", entry.ProfileSetting.Data.Type, entry.ProfileSetting.PropertyId));
            }

            // save crosshair colors for later
            switch(entry.ProfileSetting.PropertyId){
            case 107:
                crosshairNeutral = entry.ProfileSetting.Data.Value1;
                break;
            case 108:
                crosshairFriendly = entry.ProfileSetting.Data.Value1;
                break;
            case 109:
                crosshairEnemyInRange = entry.ProfileSetting.Data.Value1;
                break;
            case 110:
                crosshairEnemyOutRange = entry.ProfileSetting.Data.Value1;
                break;
            }
        }
        catch (json::exception e) {
            logError(std::format("failed to restore entry {0} with a json parsing exception", i));
            logError("unexpected format in " + inputPath + profile_file);
            logError(e.what());
        }
        i++;
    }

    // crosshair color
    UFoxUI *uiDefaultClass = UObject::GetInstanceOf<UFoxUI>(true);
    substituteCrosshairColor(uiDefaultClass, uiDefaultClass->CrosshairDefaultNeutralColor, crosshairNeutral);
    substituteCrosshairColor(uiDefaultClass, uiDefaultClass->CrosshairDefaultFriendlyColor, crosshairFriendly);
    substituteCrosshairColor(uiDefaultClass, uiDefaultClass->CrosshairDefaultEnemyInRangeColor, crosshairEnemyInRange);
    substituteCrosshairColor(uiDefaultClass, uiDefaultClass->CrosshairDefaultEnemyOutRangeColor, crosshairEnemyOutRange);

    profileSettingsLoaded = true;
    logDebug("profile settings loaded");
}

static void saveProfileSettings(UFoxProfileSettings* object) {
    logDebug("saving UE3 online profile");
    if (object == nullptr) {
        logError("UE3 online profile is not available for saving");
        return;
    }

    std::string outputPath = getOutputPath();
    if (outputPath.length() == 0) {
        logError("failed saving UE3 online profile");
        return;
    }

    std::ofstream output(outputPath + profile_file);
    if (!output.is_open()) {
        logError(std::format("failed opening {0} for writing", outputPath + profile_file));
        logError("failed saving UE3 online profile");
        return;
    }

    json jsonOut;
    int j = 0;
    for (int i = 0;i < object->ProfileSettings.Num();i++) {
        // https://wiki.beyondunreal.com/UE3:Settings_(UT3)#ESettingsDataType
        FOnlineProfileSetting &entry = object->ProfileSettings(i);
        if (entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Int32 || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Float) {
            jsonOut[j]["ProfileSetting"]["Data"]["Value1"] = entry.ProfileSetting.Data.Value1;
        }
        else if(entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Int64 || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Double || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_DateTime) {
            jsonOut[j]["ProfileSetting"]["Data"]["Value1"] = *(int64_t *)(&entry.ProfileSetting.Data.Value1);
        }
        else if (entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_String || entry.ProfileSetting.Data.Type == ESettingsDataType::SDT_Blob) {
            logError(std::format("SDT_String and SDT_Blob are currently not supported, skipping property id {0} saving", entry.ProfileSetting.PropertyId));
            continue;
#if 0
            // no idea how to deal with FPointer from here, wish the setters and getters would just work
            jsonOut[j]["ProfileSetting"]["Data"]["Value1"] = base64_encode((const char*)entry.ProfileSetting.Data.Value2.Dummy, false);
#endif
        }
        else {
            logError(std::format("unexpected type ESettingsDataType::{0}", entry.ProfileSetting.Data.Type));
            continue;
        }
        jsonOut[j]["Owner"] = entry.Owner;
        jsonOut[j]["ProfileSetting"]["PropertyId"] = entry.ProfileSetting.PropertyId;
        jsonOut[j]["ProfileSetting"]["AdvertisementType"] = entry.ProfileSetting.AdvertisementType;
        jsonOut[j]["ProfileSetting"]["Data"]["Type"] = entry.ProfileSetting.Data.Type;
        j++;
    }
    output << jsonOut.dump(4) << std::endl;
    output.close();
}

static void backupCrosshairDefaults(){
    UFoxUI *uiDefaultClass = UObject::GetInstanceOf<UFoxUI>(true);
    memcpy(crosshairNeutralDefault, &(uiDefaultClass->CrosshairDefaultNeutralColor), sizeof(FString));
    memcpy(crosshairFriendlyDefault, &(uiDefaultClass->CrosshairDefaultFriendlyColor), sizeof(FString));
    memcpy(crosshairEnemyInRangeDefault, &(uiDefaultClass->CrosshairDefaultEnemyInRangeColor), sizeof(FString));
    memcpy(crosshairEnemyOutRangeDefault, &(uiDefaultClass->CrosshairDefaultEnemyOutRangeColor), sizeof(FString));
}

static void restoreCrosshairDefaults(){
    UFoxUI *uiDefaultClass = UObject::GetInstanceOf<UFoxUI>(true);
    memcpy(&(uiDefaultClass->CrosshairDefaultNeutralColor), crosshairNeutralDefault, sizeof(FString));
    memcpy(&(uiDefaultClass->CrosshairDefaultFriendlyColor), crosshairFriendlyDefault, sizeof(FString));
    memcpy(&(uiDefaultClass->CrosshairDefaultEnemyInRangeColor), crosshairEnemyInRangeDefault, sizeof(FString));
    memcpy(&(uiDefaultClass->CrosshairDefaultEnemyOutRangeColor), crosshairEnemyOutRangeDefault, sizeof(FString));
}

static void backupKeybindDefaults(AFoxPC* object) {
    logDebug("backing up default keybindings...");
    TArray<UUIResourceDataProvider*> bindings = object->GetMenuItemsDataStore()->KeyBindingProviders;
    for (int i = 0;i < bindings.Num();i++) {
        UFoxDataProvider_KeyBindings *entry = (UFoxDataProvider_KeyBindings *)bindings(i);
        const char* command = entry->CmdName.ToChar();
        for (int j = 0;j < sizeof(command_list) / sizeof(char*);j++) {
            if (strcmp(command, command_list[j]) == 0) {
                memcpy(defaultKeybindings[j].primary, &(entry->DefaultPrimaryKey), sizeof(FName));
                memcpy(defaultKeybindings[j].alternate, &(entry->DefaultAlternateKey), sizeof(FName));
                break;
            }
        }
        free((void*)command);
    }
}

static void restoreKeybindDefaults(AFoxPC* object) {
    // only restore if it was ever loaded, memory quirk
    if (!keybindingsLoaded) {
        return;
    }
    logDebug("restoring default keybindings...");
    TArray<UUIResourceDataProvider*> bindings = object->GetMenuItemsDataStore()->KeyBindingProviders;
    for (int i = 0;i < bindings.Num();i++) {
        UFoxDataProvider_KeyBindings* entry = (UFoxDataProvider_KeyBindings *)bindings(i);
        const char* command = entry->CmdName.ToChar();
        for (int j = 0;j < sizeof(command_list) / sizeof(char*);j++) {
            if (strcmp(command, command_list[j]) == 0) {
                memcpy(&(entry->DefaultPrimaryKey), defaultKeybindings[j].primary, sizeof(FName));
                memcpy(&(entry->DefaultAlternateKey), defaultKeybindings[j].alternate, sizeof(FName));
                break;
            }
        }
        free((void*)command);
    }
}

static void backupProfileDefaults(UFoxProfileSettings* object) {
    if (profileDefaults != nullptr) {
        return;
    }
    logDebug("backing up default profile settings...");
    numProfileDefaults = object->DefaultSettings.Num();
    profileDefaults = new profileSettingBackupEntry[numProfileDefaults];
    for (int i = 0;i < numProfileDefaults; i++) {
        FOnlineProfileSetting& entry = object->DefaultSettings(i);
        memcpy(profileDefaults[i].setting, &entry, sizeof(FOnlineProfileSetting));
    }
}

static void restoreProfileDefaults(UFoxProfileSettings* object) {
    // only restore if it was ever loaded, memory quirk
    if (profileDefaults == nullptr || object == nullptr || !profileSettingsLoaded) {
        return;
    }
    logDebug("restoring default profile settings...");
    for (int i = 0;i < numProfileDefaults; i++) {
        FOnlineProfileSetting *backup = (FOnlineProfileSetting *)profileDefaults[i].setting;
        for (int j = 0;j < object->DefaultSettings.Num();j++) {
            FOnlineProfileSetting& entry = object->DefaultSettings(j);
            if (backup->ProfileSetting.PropertyId == entry.ProfileSetting.PropertyId) {
                memcpy(&entry, backup, sizeof(FOnlineProfileSetting));
                break;
            }
        }
    }
}

static void restoreDefaults(AFoxPC* object) {
    if (!defaultRestored) {
        restoreKeybindDefaults(object);
        restoreProfileDefaults(object->ProfileSettings);
        restoreCrosshairDefaults();
    }
    defaultRestored = true;
}
