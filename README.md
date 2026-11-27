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
- A build of [OBS Studio](https://github.com/obsproject/obs-studio) at the release in `ObsVersion` in `Directory.Build.props` 

### Pointing the build at OBS

Edit `BearApps.user.props` in the repo root:

```xml
<Project>
  <PropertyGroup>
    <OBSPATH>C:\src\obs-studio\build_x64</OBSPATH> <!-- Your OBS build folder -->
    <QTPATH>C:\src\obs-studio\.deps\obs-deps-qt6-...-x64</QTPATH> <!-- Optional: OBS's Qt, found in .deps next to OBSPATH if left out -->
    <OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS> <!-- Optional: where your RETAIL OBS plugins are, to auto deploy the dlls there -->
  </PropertyGroup>
</Project>
```

Run `msbuild Setup.proj`

### Copying builds into OBS

Builds copy the plugin into OBS's plugin folder. By default that is the `plugins` folder in the `rundir` of the OBS build in `OBSPATH`, used only if it exists. You can manually override where this copies to with

```xml
<OBSPLUGINS>C:\ProgramData\obs-studio\plugins</OBSPLUGINS>
```

In your BearApps.user.props

Pass `/p:DeployToOBS=false` to skip the copy.

### Updating OBS

The supported OBS version is `ObsVersion` in `Directory.Build.props`. To move to a different release, change it, build that release of OBS and point `OBSPATH` at it. CI builds the obs from that version number as well.
