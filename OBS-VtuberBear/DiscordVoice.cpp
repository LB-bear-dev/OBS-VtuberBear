#include "PCH.h"
#include "DiscordVoice.h"
#include <fstream>
#include <obs-module.h>
#include <nlohmann/json.hpp>
#include "Messaging.h"

using namespace CharacterControlOBS;

namespace
{
// Verbose lines come with every message from Discord, so they go to debug, which OBS only writes when it runs with --verbose
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

Str LogPath(FilePathRef Path)
{
	const std::u8string Utf8 = Path.u8string();
	return {Utf8.begin(), Utf8.end()};
}

// A file or folder in the plugin's folder of OBS's config
FilePath ConfigPath(const char* Name)
{
	char* Path = obs_module_config_path(Name);

	if(Path == nullptr)
	{
		ERROR_("Could not find the plugin's config folder for %s.", Name);
		return {};
	}

	FilePath Result(reinterpret_cast<const char8_t*>(Path));
	bfree(Path);
	return Result;
}

// Missing settings are left at their defaults, so a file from an older version still loads
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
		WARN_("%s: expected a JSON object, using the default Discord voice settings.", LogPath(File).c_str());
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

std::mutex DiscordVoice::SingletonMutex;
std::unique_ptr<DiscordVoice> DiscordVoice::Singleton = nullptr;
bool DiscordVoice::ShutDown = false;

DiscordVoice* DiscordVoice::Get()
{
	// The source callbacks run on OBS's graphics thread and the dock on its UI thread, so either can get here first
	std::scoped_lock Lock(SingletonMutex);

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
		std::scoped_lock Lock(SingletonMutex);
		ShutDown = true;
		Stopping = std::move(Singleton);
	}

	// Stopped with the lock released, so nothing waiting on Get holds up the source's thread
	Stopping.reset();
}

DiscordVoice::DiscordVoice() : SettingsFile(ConfigPath("StreamKit.json"))
{
	DiscordBear::SetLogFn([](LogSeverity Severity, const char* Msg)
	{
		blog(ObsLogLevel(Severity), "%s", Msg);
	});

	CurrentSettings = LoadSettings(SettingsFile);
	CurrentSettings.FollowUser = CurrentSettings.FollowUser && CAN_FOLLOW_USER;

	DiscordBear::SpeakingEvents Events = Roster.MakeEvents();
	Events.ServersListed = [this](std::vector<DiscordBear::NamedId>&& NewServers)
	{
		std::scoped_lock Lock(Mutex);
		Servers = std::move(NewServers);
		++ListsVersion;
	};
	Events.VoiceChannelsListed = [this](const DiscordBear::GuildId& Guild, std::vector<DiscordBear::NamedId>&& Channels)
	{
		std::scoped_lock Lock(Mutex);
		VoiceChannels.insert_or_assign(Guild, std::move(Channels));
		++ListsVersion;
	};
	Events.ChannelWatched = [this](const DiscordBear::WatchedChannel& NewWatched)
	{
		std::scoped_lock Lock(Mutex);
		Watched = NewWatched;
	};

	// Discord only lets the application's testers use RPC (DiscordBear::MakeRpcSpeakingSource) until it is approved,
	// so everyone goes through StreamKit for now.
	Source = DiscordBear::MakeStreamKitSpeakingSource({
		.UserDataFolder = ConfigPath("WebView2"),
		.Guild = CurrentSettings.Guild,
		.Channel = CurrentSettings.Channel,
		.FollowUser = CurrentSettings.FollowUser,
	});

	// Start now, a character is only set up once its user shows up in the call, and nothing finds out who is in the
	// call until the source is running, so this can't wait for a character's DiscordComponent.
	Source->Start(std::move(Events));
}

DiscordVoice::~DiscordVoice()
{
	// Stop the source before the roster and lists go away, its events write to them.
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
	std::scoped_lock Lock(Mutex);
	return Watched;
}

DiscordVoice::Settings DiscordVoice::GetSettings() const
{
	std::scoped_lock Lock(Mutex);
	return CurrentSettings;
}

std::vector<DiscordBear::NamedId> DiscordVoice::GetServers() const
{
	std::scoped_lock Lock(Mutex);
	return Servers;
}

std::optional<std::vector<DiscordBear::NamedId>> DiscordVoice::GetVoiceChannels(const DiscordBear::GuildId& Guild) const
{
	std::scoped_lock Lock(Mutex);

	if(const auto Found = VoiceChannels.find(Guild); Found != VoiceChannels.end())
	{
		return Found->second;
	}

	return std::nullopt;
}

uint64_t DiscordVoice::GetListsVersion() const
{
	std::scoped_lock Lock(Mutex);
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
	if(Follow && !CAN_FOLLOW_USER)
	{
		return;
	}

	{
		std::scoped_lock Lock(Mutex);

		if(CurrentSettings.FollowUser == Follow)
		{
			return;
		}

		CurrentSettings.FollowUser = Follow;
		Save();
	}

	INFO_("Discord voice: %s", Follow ? "following you between voice channels" : "watching the voice channel picked in the dock");
	Source->SetFollowUser(Follow);
}

void DiscordVoice::WatchChannel(const DiscordBear::GuildId& Guild, const DiscordBear::ChannelId& Channel)
{
	{
		std::scoped_lock Lock(Mutex);
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

void DiscordVoice::Save() const
{
	std::error_code Error;
	std::filesystem::create_directories(SettingsFile.parent_path(), Error);
	std::ofstream Stream(SettingsFile, std::ios::trunc);
	Stream << nlohmann::json{{"Guild", CurrentSettings.Guild}, {"Channel", CurrentSettings.Channel}, {"FollowUser", CurrentSettings.FollowUser}}.dump(1, '\t');

	if(!Stream)
	{
		ERROR_("%s: could not save the Discord voice settings.", LogPath(SettingsFile).c_str());
	}
}
