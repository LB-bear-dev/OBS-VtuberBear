# OBS-VtuberBear
A plugin for OBS that directly voice chat speaking state, so you can control a vtuber setup from inside OBS.

This is a direct OBS plugin, so it does not require browser sources to render the characters.
This means characters do not require many system resources, resulting in a less laggy and less CPU intensive streaming experience.

## Installing a release
- Run `OBS-VtuberBear-<version>-windows-x64-setup.exe` from the release.
- If you have a custom OBS install directory, install to `<your OBS folder>\plugins\OBS-VtuberBear`.
- To install by hand, extract the release zip into your root OBS directory and delete any older copy.

### Requirements
- OBS 33.0.0 and later

## Building Setup

Prerequisite installs:
- git on your PATH
- A build of [OBS Studio](https://github.com/obsproject/obs-studio) at the release in `ObsVersion` in `Directory.Build.props` (see [Building OBS Studio](https://github.com/obsproject/obs-studio/wiki/Build-Instructions-For-Windows)). Only `libobs` is needed.
- Clone the repo

### Pointing the build at OBS

Create `BearApps.user.props` in the repo root (it is not checked in) with the path to your OBS build:

```xml
<Project>
  <PropertyGroup>
    <OBSPATH>C:\src\obs-studio\build_x64</OBSPATH> <!-- Your OBS build folder -->
    <QTPATH>C:\src\obs-studio\.deps\obs-deps-qt6-...-x64</QTPATH> <!-- Optional: OBS's Qt, found in .deps next to OBSPATH if left out -->
    <OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS> <!-- Optional: where your RETAIL OBS plugins are, to auto deploy the dlls there -->
  </PropertyGroup>
</Project>
```
### Setting the rest up

Run `msbuild Setup.proj` from the repo root (a Developer PowerShell for VS has msbuild on its PATH). It:
- Grabs git submodules (Box2D, googletest)
- Runs NuGet
- Checks that `OBSPATH` points at an OBS build

Open `OBS-VtuberBear.sln` in your IDE of choice, build and run.

### Copying builds into OBS

Builds copy the plugin into OBS's plugin folder, in the layout OBS now requires (`<plugins>\OBS-VtuberBear\OBS-VtuberBear.dll`, see [Legacy plugin locations](https://obsproject.com/kb/legacy-plugin-locations)). By default that is the `plugins` folder in the `rundir` of the OBS build in `OBSPATH`, used only if it exists. To use an installed OBS instead, add this to `BearApps.user.props`; the folder is created if it is missing:

```xml
<OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS>
```

Pass `/p:DeployToOBS=false` to skip the copy.

### Updating OBS

The supported OBS version is `ObsVersion` in `Directory.Build.props`, which CI reads too. To move to a different release, change it, build that release of OBS and point `OBSPATH` at it. CI builds libobs at `ObsVersion` itself. Commit the change, then install the matching OBS Studio so the plugin loads.
