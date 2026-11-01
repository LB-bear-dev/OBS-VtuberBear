#pragma once
// The short names for strings and paths used across the plugin and CharacterRig
#include <filesystem>
#include <string>

using Str = std::string;
using StrId = std::string;
using StrIdRef = const StrId&;
using FilePath = std::filesystem::path;
using FilePathRef = const FilePath&;
