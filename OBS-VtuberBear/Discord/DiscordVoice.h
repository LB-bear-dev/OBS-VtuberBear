// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include "Discord/VoiceRoster.h"

namespace CharacterControlOBS
{
class DiscordVoice
{
public:
	static constexpr bool CAN_FOLLOW_USER = false;

	struct Settings
	{
		DiscordBear::GuildId Guild;
		DiscordBear::ChannelId Channel;
		bool FollowUser = CAN_FOLLOW_USER;
	};

	static DiscordVoice* Get();
	static void Shutdown();

	DiscordVoice(const DiscordVoice&) = delete;
	DiscordVoice(DiscordVoice&&) = delete;
	DiscordVoice& operator=(const DiscordVoice&) = delete;
	DiscordVoice& operator=(DiscordVoice&&) = delete;
	~DiscordVoice();

	std::optional<bool> GetSpeaking(const DiscordBear::UserId& Id) const;
	DiscordBear::SpeakingStatus GetStatus() const;
	std::vector<VoiceRoster::MemberState> GetMembers() const;
	DiscordBear::WatchedChannel GetWatched() const;
	Settings GetSettings() const;
	std::vector<DiscordBear::NamedId> GetServers() const;
	std::optional<std::vector<DiscordBear::NamedId>> GetVoiceChannels(const DiscordBear::GuildId& Guild) const;
	uint64_t GetListsVersion() const;

	void RequestServers() const;
	void RequestVoiceChannels(const DiscordBear::GuildId& Guild) const;
	void SetFollowUser(bool Follow);
	void WatchChannel(const DiscordBear::GuildId& Guild, const DiscordBear::ChannelId& Channel);
	void ShowSetup() const;

private:
	DiscordVoice();
	void Save() const;

	static std::mutex SingletonMutex;
	static std::unique_ptr<DiscordVoice> Singleton;
	static bool ShutDown;

	FilePath SettingsFile;
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
