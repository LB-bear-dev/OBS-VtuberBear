#pragma once
#include <chrono>
#include <filesystem>
#include <memory>

using namespace std::chrono_literals;
namespace CharacterControlCore
{
template<typename T>
class WatchedFile
{
public:
	WatchedFile(std::filesystem::path Filename) : Filename(std::move(Filename)), LastChangedTime(), LastUpdate(std::chrono::system_clock::duration::zero()), UpdateInterval(10s) {}

	WatchedFile(const WatchedFile&) = delete;
	WatchedFile& operator=(const WatchedFile&) = delete;

	WatchedFile(WatchedFile&& Rhs) noexcept : Filename(std::move(Rhs.Filename)),
		LastChangedTime(Rhs.LastChangedTime),
		LastUpdate(Rhs.LastUpdate),
		UpdateInterval(Rhs.UpdateInterval),
		File(std::move(Rhs.File)) {}

	WatchedFile& operator=(WatchedFile&&) = delete;

	// A derived class has already been destroyed by the time this runs, so it can't reach the derived DisposeInternal.
	// A class that needs to release what File holds has to call DisposeInternal from its own destructor.
	virtual ~WatchedFile() = default;

	bool Update()
	{
		if(std::chrono::system_clock::now() - LastUpdate <= UpdateInterval)
		{
			return true;
		}

		LastUpdate = std::chrono::system_clock::now();

		if(std::filesystem::exists(Filename))
		{
			auto Duration = std::filesystem::last_write_time(Filename) - LastChangedTime;
			if(Duration > std::chrono::system_clock::duration::zero())
			{
				DisposeInternal();
				File = std::make_unique<T>();
				if(!UpdateInternal())
				{
					File = nullptr;
					return false;
				}
				LastChangedTime = std::filesystem::last_write_time(Filename);
				return true;
			}
		}

		return false;
	}

	const std::filesystem::path& GetPath() const
	{
		return Filename;
	}

	const T* Get() const
	{
		return File.get();
	}

protected:
	virtual bool UpdateInternal() = 0;

	virtual void DisposeInternal() {}

	T* GetMutable() const
	{
		return File.get();
	}

private:
	std::filesystem::path Filename;
	std::filesystem::file_time_type LastChangedTime;
	std::chrono::system_clock::time_point LastUpdate;
	std::chrono::system_clock::duration UpdateInterval;
	std::unique_ptr<T> File;
};
}
