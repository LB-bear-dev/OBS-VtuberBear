# DiscordBear (prebuilt)

DiscordBear, the DLL that gets Discord voice state, is built in its own repository (`D:\BearApps\DiscordBear`, `DiscordBear.sln`). Every Debug or Release build there copies its output here; nothing in this repo builds it.

- `include\DiscordBear\`: the headers the plugin includes (`#include <DiscordBear/SpeakingSource.h>`)
- `Debug\`, `Release\`: `DiscordBear.dll` and its import library. Its PDB is deliberately left out; it stays with the DiscordBear build.

OBS-VtuberBear links the import library for the configuration it is building, copies the DLL next to its own output (so it deploys to OBS and gets packaged with it), and errors out if that configuration is missing. Both configurations are kept because the DLL's interface passes standard library types, which differ between the Debug and Release runtimes.

To update it, build both configurations of `DiscordBear.sln` and commit this folder.
