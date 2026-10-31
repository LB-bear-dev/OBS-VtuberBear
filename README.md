# OBS-VtuberBear
A plugin for OBS that directly voice chat speaking state, so you can control a vtuber setup from inside OBS.

This is a direct OBS plugin, so it does not require browser sources to render the characters. The characters are drawn directly in OBS.
This means characters do not require many system resources, resulting in a less laggy and less CPU intensive streaming experience.

## Installing a release
- Run `OBS-VtuberBear-<version>-windows-x64-setup.exe` from the release. For a portable OBS, change the folder on the installer's folder page to `<your OBS folder>\plugins\OBS-VtuberBear`.
- To install by hand, extract the release zip into your root OBS directory and delete any older copy.

###Requirements:
- OBS 33.0.0 and later
- If you want to use the scripted characters, you need Python 3.8 or newer installed

## Building Setup

Prerequisite installs:
- [CMake](https://cmake.org/download/) and git on your PATH
- [Python 3.12](https://www.python.org/downloads/) from python.org, which also installs the `py` launcher
- Clone the repo

### Using your own Python or OBS build

`BearApps.user.props` in the repo root holds paths to installed outside dependencies. Point it at builds you already have if they are not in the paths listed below:

```xml
<Project>
  <PropertyGroup>
    <OBSPATH>C:\src\obs-studio\build_x64</OBSPATH> <!-- Where your OBS SDK install is located -->
    <QTPATH>C:\src\obs-studio\.deps\obs-deps-qt6-...-x64</QTPATH> <!-- Where your OBS' Qt install is located -->
    <PYTHONPATH>C:\Python312</PYTHONPATH> <!-- Where your Python install is located -->	
    <OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS> <!-- Where your RETAIL OBS plugin is located, if you want to auto deploy the dlls to the plugins directory -->	
  </PropertyGroup>
</Project>
```
### Setting the rest up automatically

Run `setup-deps.ps1`. It gets everything the solution needs from outside the repo:
- Grabs git submodules (GoogleTest and psd_sdk), and applies `ThirdParty/patches/psd_sdk.patch` to psd_sdk (fixes for PSD files crafted to crash it)
- Runs NuGet
- Grabs OBS Studio at the release we're currently supporting
- Finds the python install

Open `OBS-VtuberBear.sln` in your IDE of choice, build and run.

### Copying builds into OBS

Builds copy the plugin into OBS's plugin folder, in the layout OBS now requires (`<plugins>\OBS-VtuberBear\OBS-VtuberBear.dll`, see [Legacy plugin locations](https://obsproject.com/kb/legacy-plugin-locations)). By default that is the `plugins` folder in the `rundir` of the OBS build in `OBSPATH`, used only if it exists. To use an installed OBS instead, add this to `BearApps.user.props`; the folder is created if it is missing:

```xml
<OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS>
```

Pass `/p:DeployToOBS=false` to skip the copy.

### Updating OBS

The supported OBS version is `ObsVersion` at the top of `scripts\setup-deps.ps1`. To move to a different release, run:

```powershell
.\scripts\setup-deps.ps1 -SetObsVersion 33.1.0
```

This builds that release, points `OBSPATH` and `QTPATH` in `BearApps.user.props` at it (replacing whatever was there) and updates the `ObsVersion` default, which CI reads. Commit the script change, then install the matching OBS Studio so the plugin loads.
