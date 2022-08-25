BLRevive module for saving and loading user settings

## features

- save and load integer settings and keybinds

## install and usage

first make sure proxy.dll is ready and is capable of loading modules: https://gitlab.com/blrevive/tools/proxy

install `settings-manager.dll` to `<blr>/Binaries/Win32/Modules/`

change `<blr>/FoxGame/Config/BLRevive/default.json` to load settings-manager on client

for example:
```
{
  "Proxy": {
    "Server": {
      "Host": "127.0.0.1",
      "Port": "+1"
    },
    "Modules": {
      "Server": [  ],
      "Client": [ "settings-manager" ]
    }
  }
}
```

settings are saved under `<blr>/FoxGame/Config/BLRevive/settings_manager_<player_name>`, invalid modifications to json dumps might crash the game during loading

## building prerequisites

In order to succesfully compile and run the module you need the following applications:

- Visual Studio 2019/2022 (with C++/MSVC)
- patched Blacklight: Retribution installation

## building
> Because Blacklight: Retribution itself is compiled for Win32, the modules target that platform too and only that one. It may be possible to compile the modules for another platform but there is no point in doing so.

1. start VS developer shell inside project
2. run `cmake -A Win32 -B build/` to generate build files for VS 2019 / Win32
3. configure debugging feature with cmake options (see below)

### cmake options

| option | description | default |
|---|---|---|
| `BLR_INSTALL_DIR` | absolute path to Blacklight install directory | `C:\\Program Files (x86)\\Steam\\steamapps\\common\\blacklightretribution` |
| `BLR_EXECUTABLE` | filename of BLR applicaiton | `BLR.exe` |
| `BLR_SERVER_CLI` | command line params passed to server when debugging | `server HeloDeck` |
| `BLR_CLIENT_CLI` | command line params passed to client when debugging | `127.0.0.1?Name=superewald` |


## compiling

Use `build/settings-manager.sln` to compile the module with VS 2019/2022.
The solution offers the following targets:

| target | description |
|---|---|
| Client | compile debug version and run BLR client |
| Server | compile debug version and run BLR server |
| Release | compile release version |
