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
	Coord Offset; // position of the image's top-left corner in the file's canvas; (0,0) for plain images
};

// Filename/layer name is the key
using FileImageLibrary = std::unordered_map<StrId, FileLayer>;

class WatchedFileImageLibrary final : public CharacterControlCore::WatchedFile<FileImageLibrary>
{
public:
	explicit WatchedFileImageLibrary(FilePathRef Filename) : WatchedFile(Filename), MaxResolution{.X = 0, .Y = 0}
	{
		Update();
	}

	WatchedFileImageLibrary(FilePathRef Filename, StrId OverrideName) : WatchedFile(Filename), OverrideName(std::move(OverrideName)), MaxResolution{.X = 0, .Y = 0}
	{
		Update();
	}

	// WatchedFile's destructor can't reach our DisposeInternal override, so free the textures here.
	~WatchedFileImageLibrary() override
	{
		WatchedFileImageLibrary::DisposeInternal();
	}

	WatchedFileImageLibrary(const WatchedFileImageLibrary&) = delete;
	WatchedFileImageLibrary(WatchedFileImageLibrary&&) = delete;
	WatchedFileImageLibrary& operator=(const WatchedFileImageLibrary&) = delete;
	WatchedFileImageLibrary& operator=(WatchedFileImageLibrary&&) = delete;

	[[nodiscard]] const std::vector<StrId>& GetImageNames() const;

	[[nodiscard]] Coord GetMaxResolution() const;

protected:
	bool UpdateInternal() override;
	void DisposeInternal() override;

private:
	std::vector<StrId> ImageNames;
	StrId OverrideName;

	Coord MaxResolution;
};

class CharacterImageLibrary
{
public:
	void AddImageLibrary(FilePathRef Path);
	void AddImageLibrary(FilePathRef Path, StrIdRef OverrideName);
	[[nodiscard]] std::vector<std::string> GetValidImageNames() const;
	[[nodiscard]] gs_image_file_ex* GetImageFromName(StrIdRef Name) const;
	[[nodiscard]] const FileImageLibrary* GetLayers(FilePathRef Path) const;

	[[nodiscard]] Coord GetMaxResolution() const;

private:
	std::vector<std::shared_ptr<WatchedFileImageLibrary>> ImageLibraries;
	static std::unordered_map<FilePath, std::weak_ptr<WatchedFileImageLibrary>> LoadedImageLibraries;
};
}
