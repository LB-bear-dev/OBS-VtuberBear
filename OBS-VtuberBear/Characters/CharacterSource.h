// ReSharper disable CppParameterMayBeConstPtrOrRef
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <obs-module.h>
#include "Characters/ActiveCharacters.h"
#include "Discord/DiscordComponent.h"
#include "Render/Coord.h"

namespace CharacterControlOBS
{
// What every character source keeps between OBS callbacks. Update runs on another thread than tick and render.
struct CharacterSourceData
{
	CharacterSourceData() = default;

	// A copy is a snapshot of the settings. It has a mutex of its own, so take the copy with SettingsMutex held.
	CharacterSourceData(const CharacterSourceData& Other) : Size(Other.Size), Name(Other.Name), Id(Other.Id), Active(Other.Active.load()) {}

	CharacterSourceData(CharacterSourceData&&) = delete;
	CharacterSourceData& operator=(const CharacterSourceData&) = delete;
	CharacterSourceData& operator=(CharacterSourceData&&) = delete;
	~CharacterSourceData() = default;

	mutable std::mutex SettingsMutex;
	CharacterControlRender::Coord Size;
	Str Name;
	Str Id;
	std::atomic<bool> Active = false;
};

// The OBS source callbacks shared by the Discord character sources. Name and ID settings are read here
template<typename Source>
class CharacterSource
{
public:
	static obs_source_info MakeInfo(const char* Id) noexcept
	{
		return {
			.id = Id,
			.type = OBS_SOURCE_TYPE_INPUT,
			.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
			.get_name = GetName,
			.create = Create,
			.destroy = Destroy,
			.get_width = GetWidth,
			.get_height = GetHeight,
			.get_defaults = GetDefaults,
			.get_properties = GetProperties,
			.update = Update,
			.activate = Activate,
			.deactivate = Deactivate,
			.show = Activate,
			.hide = Deactivate,
			.video_tick = Tick,
			.video_render = Render,
			.icon_type = OBS_ICON_TYPE_CUSTOM,
		};
	}

private:
	static void LoadSettings(Source& SourceData, obs_data_t* Settings)
	{
		SourceData.Name = obs_data_get_string(Settings, "Name");
		SourceData.Id = obs_data_get_string(Settings, "ID");
		SourceData.LoadSettings(Settings);
	}

	static void SetupCharacter(Source& SourceData)
	{
		// The images take a while to load, so work on a copy of the settings instead of holding the lock
		Source Settings = [&SourceData]() ->Source
		{
			const std::scoped_lock Lock(SourceData.SettingsMutex);
			return SourceData;
		}();

		ActiveCharacters::Get().MakeActiveCharacter(Settings.Name, Character());

		if(const std::shared_ptr<Character> ActiveCharacter = ActiveCharacters::Get().GetActiveCharacter(Settings.Name))
		{
			ActiveCharacter->AddComponent(std::make_unique<DiscordComponent>(Settings.Id));

			if(CharacterControlRender::ImageLayer* DefaultLayer = ActiveCharacter->GetImageRoot().GetLayerByName("default"))
			{
				Settings.SetupImages(*DefaultLayer);
			}

			const CharacterControlRender::Coord NewSize = ActiveCharacter->GetImageRoot().GetMaxResolution();
			bool Renamed;
			{
				const std::scoped_lock Lock(SourceData.SettingsMutex);
				SourceData.Size = NewSize;
				Renamed = SourceData.Name != Settings.Name;
			}

			if(Renamed)
			{
				// Update changed the name while this was loading and already cleared the old one, so nothing else would
				ActiveCharacters::Get().RemoveActiveCharacter(Settings.Name);
			}
		}
	}

	static const char* GetName(void* Unused)
	{
		UNUSED_PARAMETER(Unused);
		return obs_module_text(Source::DISPLAY_NAME);
	}

	static void* Create(obs_data_t* Settings, obs_source_t* OBSSource)
	{
		if(OBSSource == nullptr || Settings == nullptr)
		{
			return nullptr;
		}

		auto Data = std::make_unique<Source>();
		LoadSettings(*Data, Settings);
		Data->Active = obs_source_active(OBSSource);

		return Data.release();
	}

	static void Destroy(void* Data)
	{
		if(Data == nullptr)
		{
			return;
		}

		const std::unique_ptr<Source> SourceData(static_cast<Source*>(Data));
		ActiveCharacters::Get().RemoveActiveCharacter(SourceData->Name);
	}

	static uint32_t GetWidth(void* Data)
	{
		if(Data == nullptr)
		{
			return 100;
		}

		const auto* SourceData = static_cast<const Source*>(Data);
		const std::scoped_lock Lock(SourceData->SettingsMutex);
		return static_cast<uint32_t>(SourceData->Size.X);
	}

	static uint32_t GetHeight(void* Data)
	{
		if(Data == nullptr)
		{
			return 100;
		}

		const auto* SourceData = static_cast<const Source*>(Data);
		const std::scoped_lock Lock(SourceData->SettingsMutex);
		return static_cast<uint32_t>(SourceData->Size.Y);
	}

	static void GetDefaults(obs_data_t* Settings)
	{
		if(Settings == nullptr)
		{
			return;
		}

		obs_data_set_default_string(Settings, "Name", "");
	}

	static obs_properties_t* GetProperties(void* Data)
	{
		obs_properties_t* Properties = obs_properties_create();

		if(Data != nullptr)
		{
			Source::AddProperties(Properties);
		}

		return Properties;
	}

	static void Update(void* Data, obs_data_t* Settings)
	{
		if(Data == nullptr || Settings == nullptr)
		{
			return;
		}

		auto* SourceData = static_cast<Source*>(Data);
		Str OldName;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			OldName = SourceData->Name;
			LoadSettings(*SourceData, Settings);
		}

		// Tick builds the character again from the new settings. Removed with the lock gone, since the character's destructor takes the graphics lock.
		ActiveCharacters::Get().RemoveActiveCharacter(OldName);
	}

	// Also used for show
	static void Activate(void* Data)
	{
		static_cast<Source*>(Data)->Active = true;
	}

	// Also used for hide
	static void Deactivate(void* Data)
	{
		auto* SourceData = static_cast<Source*>(Data);
		std::shared_ptr<Character> Removed;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			Removed = ActiveCharacters::Get().RemoveActiveCharacter(SourceData->Name);
		}

		// Let go of the character with no lock held
		Removed.reset();
		SourceData->Active = false;
	}

	static void Tick(void* Data, const float Seconds)
	{
		UNUSED_PARAMETER(Seconds);

		if(Data == nullptr)
		{
			return;
		}

		auto* SourceData = static_cast<Source*>(Data);
		// Declared before the lock so a removed character is destroyed after the lock is released
		std::shared_ptr<Character> ActiveCharacter;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			if(!SourceData->IsLive())
			{
				ActiveCharacters::Get().RemoveActiveCharacter(SourceData->Name);
				return;
			}

			if(!SourceData->Active)
			{
				return;
			}

			ActiveCharacter = ActiveCharacters::Get().GetActiveCharacter(SourceData->Name);
		}

		if(ActiveCharacter == nullptr)
		{
			// If anything causes the character to not exist, set it up this frame.
			SetupCharacter(*SourceData);
			return;
		}

		ActiveCharacter->UpdateAttributes();

		if constexpr(Source::UPDATES_RENDER_STATE)
		{
			ActiveCharacter->UpdateRenderState();
		}

		if(const Attribute* Attr = ActiveCharacter->GetAttribute("Talking"))
		{
			if(const bool* Talking = Attr->Get<bool>())
			{
				ActiveCharacter->GetImageRoot().SetSlot("SpeakingState", *Talking ? "TALK" : "SILENT");
			}
		}

		if(const Attribute* Attr = ActiveCharacter->GetAttribute("InCall"))
		{
			if(const bool* InCall = Attr->Get<bool>())
			{
				if(*InCall)
				{
					ActiveCharacter->GetImageRoot().Activate();
				} else
				{
					ActiveCharacter->GetImageRoot().Deactivate();
				}
			}
		}
	}

	static void Render(void* Data, gs_effect_t* Effect)
	{
		if(Data == nullptr || Effect == nullptr)
		{
			return;
		}

		const auto* SourceData = static_cast<const Source*>(Data);
		std::shared_ptr<Character> ActiveCharacter;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			if(!SourceData->Active)
			{
				return;
			}

			ActiveCharacter = ActiveCharacters::Get().GetActiveCharacter(SourceData->Name);
		}

		if(ActiveCharacter != nullptr)
		{
			ActiveCharacter->Render(Effect);
		}
	}
};
}
