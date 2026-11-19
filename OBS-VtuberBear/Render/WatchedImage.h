// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <CharacterControlCore/WatchedFile.h>
#include <graphics/image-file.h>

namespace CharacterControlRender
{
// One image coming from a file. A normal file has one layer, PSDs have one per photoshop layer.
struct FileLayer
{
	std::unique_ptr<gs_image_file_ex> Image;
	int ZOrder; // What is on top of what in the file
	b2Vec2 Offset{};
};

// Filename/layer name is the key
using FileImageLibrary = std::unordered_map<StrId, FileLayer>;

class WatchedFileImageLibrary final : public CharacterControlCore::WatchedFile<FileImageLibrary>
{
public:
	explicit WatchedFileImageLibrary(FilePathRef Filename) : WatchedFile(Filename), MaxResolution{.x = 0, .y = 0}
	{
		Update();
	}

	WatchedFileImageLibrary(FilePathRef Filename, StrId OverrideName) : WatchedFile(Filename), OverrideName(std::move(OverrideName)), MaxResolution{.x = 0, .y = 0}
	{
		Update();
	}

	~WatchedFileImageLibrary() override
	{
		WatchedFileImageLibrary::DisposeInternal();
	}

	WatchedFileImageLibrary(const WatchedFileImageLibrary&) = delete;
	WatchedFileImageLibrary(WatchedFileImageLibrary&&) = delete;
	WatchedFileImageLibrary& operator=(const WatchedFileImageLibrary&) = delete;
	WatchedFileImageLibrary& operator=(WatchedFileImageLibrary&&) = delete;

	const std::vector<StrId>& GetImageNames() const;

	b2Vec2 GetMaxResolution() const;

protected:
	bool UpdateInternal() override;
	void DisposeInternal() override;

private:
	std::vector<StrId> ImageNames;
	StrId OverrideName;

	b2Vec2 MaxResolution{};
};

class CharacterImageLibrary
{
public:
	void AddImageLibrary(FilePathRef Path);
	void AddImageLibrary(FilePathRef Path, StrIdRef OverrideName);
	std::vector<std::string> GetValidImageNames() const;
	gs_image_file_ex* GetImageFromName(StrIdRef Name) const;
	const FileImageLibrary* GetLayers(FilePathRef Path) const;

	b2Vec2 GetMaxResolution() const;

private:
	std::vector<std::shared_ptr<WatchedFileImageLibrary>> ImageLibraries;
	static std::unordered_map<FilePath, std::weak_ptr<WatchedFileImageLibrary>> LoadedImageLibraries;
};
}
