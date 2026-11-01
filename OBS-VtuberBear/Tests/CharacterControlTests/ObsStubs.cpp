#include "ObsStubs.h"
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <obs.h>
#include <graphics/image-file.h>
#include <util/base.h>
#include <util/bmem.h>

namespace
{
std::vector<std::string>& Messages()
{
	static std::vector<std::string> Log;
	return Log;
}

int Uploads = 0;
}

namespace ObsStubs
{
void Reset()
{
	Messages().clear();
	Uploads = 0;
}

const std::vector<std::string>& LogMessages()
{
	return Messages();
}

bool LogContains(const std::string& Text)
{
	for(const std::string& Message : Messages())
	{
		if(Message.find(Text) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

int TextureUploads()
{
	return Uploads;
}
}

void blog(const int LogLevel, const char* Format, ...)
{
	(void)LogLevel;

	std::array<char, 1024> Buffer = {};
	va_list Args;
	va_start(Args, Format);
	static_cast<void>(vsnprintf(Buffer.data(), Buffer.size(), Format, Args));
	va_end(Args);

	Messages().emplace_back(Buffer.data());
}

void* bmalloc(const size_t Size)
{
	return malloc(Size);
}

void bfree(void* Ptr)
{
	free(Ptr);
}

void obs_enter_graphics() {}

void obs_leave_graphics() {}

// Plain image files (not PSDs) are not under test, so none of them load
void gs_image_file_ex_init(gs_image_file_ex_t* Image, const char* File, const gs_image_alpha_mode AlphaMode)
{
	(void)File;

	*Image = {};
	Image->alpha_mode = AlphaMode;
}

void gs_image_file_ex_free(gs_image_file_ex_t* Image)
{
	bfree(Image->texture_data);
	Image->texture_data = nullptr;
	Image->loaded = false;
}

void gs_image_file_ex_init_texture(gs_image_file_ex_t* Image)
{
	(void)Image;

	++Uploads;
}
