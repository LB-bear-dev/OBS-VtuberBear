#pragma once
// WatchedImage.cpp reaches libobs through a few calls: logging, memory, and uploading the loaded pixels as a texture. ObsStubs.cpp
// defines those, so the PSD loader compiles straight into the test exe without OBS or a graphics context.
//
// One difference from OBS: the real gs_image_file_ex_init_texture hands the pixels to the GPU and frees them. The stub
// keeps them in texture_data, which is how the tests read back what the loader produced.

#include <string>
#include <vector>

namespace ObsStubs
{
// Forget the log and the upload count; call at the start of each test
void Reset();

// Everything the loader sent to blog() since Reset, formatted
const std::vector<std::string>& LogMessages();

bool LogContains(const std::string& Text);

// Calls to gs_image_file_ex_init_texture since Reset, one per layer that made it into the library
int TextureUploads();
}
