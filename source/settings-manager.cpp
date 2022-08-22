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
using namespace BLRevive;

// functions
static void saveSettings(UObject *);

/// <summary>
/// Thread thats specific to the module (function must exist and export demangled!)
/// </summary>
extern "C" __declspec(dllexport) void ModuleThread()
{
    // put your code here
}

/// <summary>
/// Module initializer (function must exist and export demangled!)
/// </summary>
/// <param name="data"></param>
extern "C" __declspec(dllexport) void InitializeModule(std::shared_ptr<Module::InitData> data)
{
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
        // Letterbox 152
        // ADS blur 154
        // ?? 153
        
        Events::ID("*", "SetSettingEnabled"),
        [=](Events::Info info) {
            UFoxProfileSettings_eventSetSettingEnabled_Parms *params = (UFoxProfileSettings_eventSetSettingEnabled_Parms*)info.Params;
            LDebug("FoxProfileSettings SetSettingsEnabled {0} {1} {2}", std::to_string(params->SettingId), std::to_string(params->bEnabled), info.Object->GetName());
            LFlush;
        },
        true
    });

    
    eventManager->RegisterHandler({
        Events::ID("*", "ei_ApplyChanges"),
        [=](Events::Info info) {
            LDebug("{0} ei_ApplyChanges ", info.Object->GetName());
            saveSettings(info.Object);
        },
        false /* cannot blocking process ei_ApplyChanges, it will freeze for some reason */
        });
    
    /*
    eventManager->RegisterHandler({
        Events::ID("*", "OnSettingsClose"),
            [=](Events::Info info) {
            UFoxPlayerInput_execSetBind_Parms* params = (UFoxPlayerInput_execSetBind_Parms*)info.Params;
            const char* command = params->Command.ToChar();
            LDebug("? OnSettingsClose {0}", info.Object->GetName());
            LFlush;
            free((void*)command);
        },
        true
        });
    */
    eventManager->RegisterHandler({
        Events::ID("*", "SetSettingValueInt"),
        [=](Events::Info info) {
            UFoxProfileSettings_eventSetSettingValueInt_Parms* params = (UFoxProfileSettings_eventSetSettingValueInt_Parms*)info.Params;
            LDebug("FoxProfileSettings SetSettingValueInt {0} {1} {2}", std::to_string(params->SettingId), std::to_string(params->Value), info.Object->GetName());
            LFlush;
        },
        true
    });

    eventManager->RegisterHandler({
        Events::ID("*", "SetAutoReload"),
        [=](Events::Info info) {
            UFoxProfileSettings_eventSetAutoReload_Parms* params = (UFoxProfileSettings_eventSetAutoReload_Parms*)info.Params;
            LDebug("FoxProfileSettings SetAutoReload {0}", std::to_string(params->bEnabled));
            LFlush;
        },
        true
    });
    

    eventManager->RegisterHandler({
    Events::ID("*", "ConnectByIP"),
    [=](Events::Info info) {
        AFoxPC_execConnectByIP_Parms* params = (AFoxPC_execConnectByIP_Parms*)info.Params;
        LDebug("FoxPC ConnectByIP fired");
        LFlush;
    },
    true
        });

    LDebug("settings-manager: handlers registered");
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

static void saveVideo(UFoxSettingsUIVideo *object) {
    LDebug("settings-manager: saving video settings...");
    LFlush;
}

static void saveGameplay(UFoxSettingsUIGameplay *object) {
    LDebug("settings-manager: saving gameplay settings...");
    LFlush;
}

static void saveAudio(UFoxSettingsUIAudio *object) {
    LDebug("settings-manager: saving audio settings...");
    LFlush;
}

static void saveCrosshair(UFoxSettingsUICrosshair *object) {
    LDebug("settings-manager: saving corsshair settings...");
    LFlush;
}

static void saveControls(UFoxSettingsUIControls *object) {
    LDebug("settings-manager: saving control settings...");
    LFlush;
}

static void saveKeyBindings(UFoxSettingsUIKeyBindings *object) {
    LDebug("settings-manager: saving keybinds...");
    LFlush;
    for (int i = 0;i < object->CurrentKeyBindings.Num();i++) {
        FKeyBindInfo keyBind = object->CurrentKeyBindings(i);
        const char* command = keyBind.CommandName.ToChar();
        LDebug("settings-manager: saving keybind {0} {1} {2}", command, keyBind.PrimaryKey.GetName(), keyBind.AlternateKey.GetName());
        free((void*)command);
    }
}

static void saveSettings(UObject *object) {
    LDebug("settings-manager: saving settings...");
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
        LDebug("settings-manager: {0} saving is not implemented", object->GetName());
        LFlush;
    }
}
