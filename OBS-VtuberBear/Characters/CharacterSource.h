// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
// ReSharper disable CppParameterMayBeConstPtrOrRef
#pragma once
#include <atomic>
#include <memory>
#include <mutex>
#include <obs-module.h>
#include "Characters/ActiveCharacters.h"
#include "box2d/math_functions.h"

namespace CharacterControlOBS
{
struct CharacterSettings
{
	Str Name;
	Str Id;
};

struct CharacterSourceData
{
	mutable std::mutex SettingsMutex;
	b2Vec2 Size{};
	std::atomic<bool> Active = false;
};

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
		SourceData.Current.Name = obs_data_get_string(Settings, "Name");
		SourceData.Current.Id = obs_data_get_string(Settings, "ID");
		SourceData.Current.Load(Settings);
	}

	static void SetupCharacter(Source& SourceData)
	{
		typename Source::Settings Settings;
		{
			const std::scoped_lock Lock(SourceData.SettingsMutex);
			Settings = SourceData.Current;
		}

		ActiveCharacters::Get().MakeActiveCharacter(Settings.Name, Character());

		if(const std::shared_ptr<Character> ActiveCharacter = ActiveCharacters::Get().GetActiveCharacter(Settings.Name))
		{
			if(CharacterControlRender::ImageLayer* DefaultLayer = ActiveCharacter->GetImageRoot().GetLayerByName("default"))
			{
				Settings.SetupImages(*DefaultLayer);
			}

			const b2Vec2 NewSize = ActiveCharacter->GetImageRoot().GetMaxResolution();
			bool Renamed;
			{
				const std::scoped_lock Lock(SourceData.SettingsMutex);
				SourceData.Size = NewSize;
				Renamed = SourceData.Current.Name != Settings.Name;
			}

			if(Renamed)
			{
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
		auto Data = std::make_unique<Source>();
		LoadSettings(*Data, Settings);
		Data->Active = obs_source_active(OBSSource);

		return Data.release();
	}

	static void Destroy(void* Data)
	{
		const std::unique_ptr<Source> SourceData(static_cast<Source*>(Data));
		ActiveCharacters::Get().RemoveActiveCharacter(SourceData->Current.Name);
	}

	static uint32_t GetWidth(void* Data)
	{
		const auto* SourceData = static_cast<const Source*>(Data);
		const std::scoped_lock Lock(SourceData->SettingsMutex);
		return static_cast<uint32_t>(SourceData->Size.x);
	}

	static uint32_t GetHeight(void* Data)
	{
		const auto* SourceData = static_cast<const Source*>(Data);
		const std::scoped_lock Lock(SourceData->SettingsMutex);
		return static_cast<uint32_t>(SourceData->Size.y);
	}

	static void GetDefaults(obs_data_t* Settings)
	{
		obs_data_set_default_string(Settings, "Name", "");
	}

	static obs_properties_t* GetProperties(void* Data)
	{
		UNUSED_PARAMETER(Data);
		obs_properties_t* Properties = obs_properties_create();
		Source::AddProperties(Properties);
		return Properties;
	}

	static void Update(void* Data, obs_data_t* Settings)
	{
		auto* SourceData = static_cast<Source*>(Data);
		Str OldName;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			OldName = SourceData->Current.Name;
			LoadSettings(*SourceData, Settings);
		}

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
		Str Name;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			Name = SourceData->Current.Name;
		}

		ActiveCharacters::Get().RemoveActiveCharacter(Name);
		SourceData->Active = false;
	}

	static void Tick(void* Data, const float Seconds)
	{
		UNUSED_PARAMETER(Seconds);

		auto* SourceData = static_cast<Source*>(Data);
		std::shared_ptr<Character> ActiveCharacter;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			if(!SourceData->IsLive())
			{
				ActiveCharacters::Get().RemoveActiveCharacter(SourceData->Current.Name);
				return;
			}

			if(!SourceData->Active)
			{
				return;
			}

			ActiveCharacter = ActiveCharacters::Get().GetActiveCharacter(SourceData->Current.Name);
		}

		if(ActiveCharacter == nullptr)
		{
			// If anything causes the character to not exist, set it up this frame.
			SetupCharacter(*SourceData);
			return;
		}

		ActiveCharacter->UpdateAttributes();

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
		const auto* SourceData = static_cast<const Source*>(Data);
		std::shared_ptr<Character> ActiveCharacter;
		{
			const std::scoped_lock Lock(SourceData->SettingsMutex);
			if(!SourceData->Active)
			{
				return;
			}

			ActiveCharacter = ActiveCharacters::Get().GetActiveCharacter(SourceData->Current.Name);
		}

		if(ActiveCharacter != nullptr)
		{
			ActiveCharacter->Render(Effect);
		}
	}
};
}
