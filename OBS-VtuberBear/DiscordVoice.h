#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include "VoiceRoster.h"

namespace CharacterControlOBS
{
// The one Discord connection the characters and the dock share, started the first time anything asks for it. Its settings
// are the plugin's, not any one character's.
class DiscordVoice
{
public:
	// Following the user between voice channels is turned off for now: the dock has no control for it and a saved setting is
	// ignored, so only the channel picked in the dock is watched.
	static constexpr bool CAN_FOLLOW_USER = false;

	struct Settings
	{
		// The channel picked in the dock, watched while not following the user
		DiscordBear::GuildId Guild;
		DiscordBear::ChannelId Channel;
		bool FollowUser = CAN_FOLLOW_USER;
	};

	// Null once Shutdown has run, nothing starts it again after that
	static DiscordVoice* Get();
	// Must run from obs_module_unload, at process exit the WebView's thread is already gone and stopping would wait on it forever.
	static void Shutdown();

	DiscordVoice(const DiscordVoice&) = delete;
	DiscordVoice& operator=(const DiscordVoice&) = delete;
	~DiscordVoice();

	// Whether the user is speaking, empty while they aren't in the call
	[[nodiscard]] std::optional<bool> GetSpeaking(const DiscordBear::UserId& Id) const;
	[[nodiscard]] DiscordBear::SpeakingStatus GetStatus() const;
	[[nodiscard]] std::vector<VoiceRoster::MemberState> GetMembers() const;
	[[nodiscard]] DiscordBear::WatchedChannel GetWatched() const;
	[[nodiscard]] Settings GetSettings() const;
	[[nodiscard]] std::vector<DiscordBear::NamedId> GetServers() const;
	// Empty until Discord has listed them
	[[nodiscard]] std::optional<std::vector<DiscordBear::NamedId>> GetVoiceChannels(const DiscordBear::GuildId& Guild) const;
	// Goes up whenever a server or channel list arrives
	[[nodiscard]] uint64_t GetListsVersion() const;

	// The answers show up in GetServers and GetVoiceChannels
	void RequestServers() const;
	void RequestVoiceChannels(const DiscordBear::GuildId& Guild) const;
	// Both are saved for next time
	void SetFollowUser(bool Follow);
	void WatchChannel(const DiscordBear::GuildId& Guild, const DiscordBear::ChannelId& Channel);
	// Shows StreamKit so the user can authorize it
	void ShowSetup() const;

private:
	DiscordVoice();
	// With Mutex held
	void Save() const;

	static std::mutex SingletonMutex;
	static std::unique_ptr<DiscordVoice> Singleton;
	static bool ShutDown;

	FilePath SettingsFile;
	// Everything below it but the roster and the source, which the source's thread writes and the UI and graphics threads read
	mutable std::mutex Mutex;
	Settings CurrentSettings;
	DiscordBear::WatchedChannel Watched;
	std::vector<DiscordBear::NamedId> Servers;
	std::map<DiscordBear::GuildId, std::vector<DiscordBear::NamedId>> VoiceChannels;
	uint64_t ListsVersion = 0;

	VoiceRoster Roster;
	DiscordBear::SpeakingSourcePtr Source;
};
}
