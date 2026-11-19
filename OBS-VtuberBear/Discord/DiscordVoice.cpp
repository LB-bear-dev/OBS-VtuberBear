// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#include "PCH.h"
#include "Discord/DiscordVoice.h"
#include <fstream>
#include <obs-module.h>
#include <nlohmann/json.hpp>
#include "CharacterControlCore/Messaging.h"
#include "CharacterRig/PathRules.h"

using namespace CharacterControlOBS;

namespace
{
int ObsLogLevel(const LogSeverity Severity)
{
	switch(Severity)
	{
		case LogSeverity::Verbose :
			return LOG_DEBUG;
		case LogSeverity::Info :
			return LOG_INFO;
		case LogSeverity::NonSevere :
			return LOG_WARNING;
		case LogSeverity::Severe :
		case LogSeverity::Critical :
			break;
	}

	return LOG_ERROR;
}

FilePath ConfigPath(const char* Name)
{
	char* Path = obs_module_config_path(Name);

	if(Path == nullptr)
	{
		ERROR_("Cannot find config path for %s.", Name);
		return {};
	}

	FilePath Result(reinterpret_cast<const char8_t*>(Path));
	bfree(Path);
	return Result;
}

DiscordVoice::Settings LoadSettings(FilePathRef File)
{
	DiscordVoice::Settings Loaded;
	std::ifstream Stream(File);

	if(!Stream)
	{
		return Loaded;
	}

	const nlohmann::json Json = nlohmann::json::parse(Stream, nullptr, false);

	if(Json.is_discarded() || !Json.is_object())
	{
		WARN_("%s is not valid JSON.", LogPath(File).c_str());
		return Loaded;
	}

	if(const auto Guild = Json.find("Guild"); Guild != Json.end() && Guild->is_string())
	{
		Loaded.Guild = Guild->get<Str>();
	}

	if(const auto Channel = Json.find("Channel"); Channel != Json.end() && Channel->is_string())
	{
		Loaded.Channel = Channel->get<Str>();
	}

	if(const auto FollowUser = Json.find("FollowUser"); FollowUser != Json.end() && FollowUser->is_boolean())
	{
		Loaded.FollowUser = FollowUser->get<bool>();
	}

	return Loaded;
}
}

DiscordVoice* DiscordVoice::Get()
{
	const std::scoped_lock Lock(SingletonMutex);

	if(Singleton == nullptr && !ShutDown)
	{
		Singleton = std::unique_ptr<DiscordVoice>(new DiscordVoice());
	}

	return Singleton.get();
}

void DiscordVoice::Shutdown()
{
	std::unique_ptr<DiscordVoice> Stopping;
	{
		const std::scoped_lock Lock(SingletonMutex);
		ShutDown = true;
		Stopping = std::move(Singleton);
	}

	Stopping.reset();
}

DiscordVoice::~DiscordVoice()
{
	Source->Stop();
}

std::optional<bool> DiscordVoice::GetSpeaking(const DiscordBear::UserId& Id) const
{
	return Roster.GetSpeaking(Id);
}

DiscordBear::SpeakingStatus DiscordVoice::GetStatus() const
{
	return Roster.GetStatus();
}

std::vector<VoiceRoster::MemberState> DiscordVoice::GetMembers() const
{
	return Roster.GetMembers();
}

DiscordBear::WatchedChannel DiscordVoice::GetWatched() const
{
	const std::scoped_lock Lock(Mutex);
	return Watched;
}

DiscordVoice::Settings DiscordVoice::GetSettings() const
{
	const std::scoped_lock Lock(Mutex);
	return CurrentSettings;
}

std::vector<DiscordBear::NamedId> DiscordVoice::GetServers() const
{
	const std::scoped_lock Lock(Mutex);
	return Servers;
}

std::optional<std::vector<DiscordBear::NamedId>> DiscordVoice::GetVoiceChannels(const DiscordBear::GuildId& Guild) const
{
	const std::scoped_lock Lock(Mutex);

	if(const auto Found = VoiceChannels.find(Guild); Found != VoiceChannels.end())
	{
		return Found->second;
	}

	return std::nullopt;
}

uint64_t DiscordVoice::GetListsVersion() const
{
	const std::scoped_lock Lock(Mutex);
	return ListsVersion;
}

void DiscordVoice::RequestServers() const
{
	Source->RequestServers();
}

void DiscordVoice::RequestVoiceChannels(const DiscordBear::GuildId& Guild) const
{
	Source->RequestVoiceChannels(Guild);
}

void DiscordVoice::SetFollowUser(const bool Follow)
{
	if constexpr(!CAN_FOLLOW_USER)
	{
		if(Follow)
		{
			return;
		}
	}

	{
		const std::scoped_lock Lock(Mutex);

		if(CurrentSettings.FollowUser == Follow)
		{
			return;
		}

		CurrentSettings.FollowUser = Follow;
		Save();
	}

	INFO_("Discord follow user: %s", Follow ? "on" : "off");
	Source->SetFollowUser(Follow);
}

void DiscordVoice::WatchChannel(const DiscordBear::GuildId& Guild, const DiscordBear::ChannelId& Channel)
{
	{
		const std::scoped_lock Lock(Mutex);
		CurrentSettings.Guild = Guild;
		CurrentSettings.Channel = Channel;
		Save();
	}

	Source->WatchChannel(Guild, Channel);
}

void DiscordVoice::ShowSetup() const
{
	Source->ShowSetup();
}

DiscordVoice::DiscordVoice() : SettingsFile(ConfigPath("StreamKit.json"))
{
	DiscordBear::SetLogFn([](const LogSeverity Severity, const char* Msg)
	{
		blog(ObsLogLevel(Severity), "%s", Msg);
	});

	CurrentSettings = LoadSettings(SettingsFile);
	CurrentSettings.FollowUser = CurrentSettings.FollowUser && CAN_FOLLOW_USER;

	DiscordBear::SpeakingEvents Events = Roster.MakeEvents();
	Events.ServersListed = [this](std::vector<DiscordBear::NamedId>&& NewServers)
	{
		const std::scoped_lock Lock(Mutex);
		Servers = std::move(NewServers);
		++ListsVersion;
	};
	Events.VoiceChannelsListed = [this](const DiscordBear::GuildId& Guild, std::vector<DiscordBear::NamedId>&& Channels)
	{
		const std::scoped_lock Lock(Mutex);
		VoiceChannels.insert_or_assign(Guild, std::move(Channels));
		++ListsVersion;
	};
	Events.ChannelWatched = [this](const DiscordBear::WatchedChannel& NewWatched)
	{
		const std::scoped_lock Lock(Mutex);
		Watched = NewWatched;
	};

	Source = DiscordBear::MakeStreamKitSpeakingSource({
		.UserDataFolder = ConfigPath("WebView2"),
		.Guild = CurrentSettings.Guild,
		.Channel = CurrentSettings.Channel,
		.FollowUser = CurrentSettings.FollowUser,
	});

	Source->Start(std::move(Events));
}

void DiscordVoice::Save() const
{
	std::error_code Error;
	create_directories(SettingsFile.parent_path(), Error);
	std::ofstream Stream(SettingsFile, std::ios::trunc);
	Stream << nlohmann::json{{"Guild", CurrentSettings.Guild}, {"Channel", CurrentSettings.Channel}, {"FollowUser", CurrentSettings.FollowUser}}.dump(1, '\t');

	if(!Stream)
	{
		ERROR_("Cannot save %s.", LogPath(SettingsFile).c_str());
	}
}

std::mutex DiscordVoice::SingletonMutex;
std::unique_ptr<DiscordVoice> DiscordVoice::Singleton = nullptr;
bool DiscordVoice::ShutDown = false;
