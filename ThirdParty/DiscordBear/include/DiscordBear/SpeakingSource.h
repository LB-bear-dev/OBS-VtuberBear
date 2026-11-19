#pragma once
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Interface.h"

namespace DiscordBear
{
using GuildId = std::string;

// Someone in the voice channel being watched
struct VoiceMember
{
	UserId User;
	// Empty when the source doesn't know it, the RPC source only gets IDs
	std::string Name;
	bool Muted = false;
	bool Deafened = false;

	bool operator==(const VoiceMember&) const = default;
};

// A server, or a voice channel in one
struct NamedId
{
	std::string Id;
	std::string Name;

	bool operator==(const NamedId&) const = default;
};

// The voice channel being watched. Channel is empty while there is none.
struct WatchedChannel
{
	GuildId Guild;
	ChannelId Channel;
	// Empty until Discord has said
	std::string Name;

	bool operator==(const WatchedChannel&) const = default;
};

enum class SpeakingStatus : char
{
	// No voice channel picked yet
	NotSetUp,
	Starting,
	// The source can't run at all, e.g. the WebView2 runtime is missing
	Unavailable,
	DiscordNotRunning,
	NotAuthorized,
	// Talking to Discord, but nothing about the channel has come through
	NoTraffic,
	Connected,
	// Following the user, who isn't in a voice channel right now
	NotInVoiceChannel,
};

DLLINTERFACE const char* GetStatusText(SpeakingStatus Status);

// Called on the source's own thread. The roster events and StatusChanged must be set, the rest may be left empty.
struct SpeakingEvents
{
	// Everyone in the channel, replacing whoever was there before
	std::function<void(std::vector<VoiceMember>&&)> RosterReset;
	// Someone joined, or their name, mute or deafen state changed
	std::function<void(VoiceMember&&)> MemberUpdated;
	std::function<void(const UserId&)> MemberLeft;
	std::function<void(const UserId&, bool Speaking)> SpeakingChanged;
	// Anything but Connected means speaking events have stopped, so nobody should be left showing as speaking
	std::function<void(SpeakingStatus)> StatusChanged;

	// The answer to RequestServers
	std::function<void(std::vector<NamedId>&&)> ServersListed;
	// The answer to RequestVoiceChannels
	std::function<void(const GuildId&, std::vector<NamedId>&&)> VoiceChannelsListed;
	// The source started or stopped watching a channel, or found out its name
	std::function<void(const WatchedChannel&)> ChannelWatched;
};

// Where voice state comes from. Callers only see who is in the channel and who is speaking, never how it was found out.
class SpeakingSource
{
public:
	SpeakingSource() = default;
	SpeakingSource(const SpeakingSource&) = delete;
	SpeakingSource& operator=(const SpeakingSource&) = delete;
	virtual ~SpeakingSource() = default;

	// Starts delivering events.
	virtual void Start(SpeakingEvents Events) = 0;
	// No events arrive once this returns.
	virtual void Stop() = 0;
	// Shows whatever the user has to do to authorize this source, if there is anything.
	virtual void ShowSetup() = 0;

	// The answers come back as ServersListed and VoiceChannelsListed. Sources that can't list them never answer.
	virtual void RequestServers() = 0;
	virtual void RequestVoiceChannels(const GuildId& Guild) = 0;
	// Watches this channel while not following the user.
	virtual void WatchChannel(const GuildId& Guild, const ChannelId& Channel) = 0;
	// Watches whichever voice channel the user is in, switching when they do.
	virtual void SetFollowUser(bool Follow) = 0;
};

using SpeakingSourcePtr = std::unique_ptr<SpeakingSource>;

// Discord's local RPC pipe. It always follows the user and can't list servers or channels. Discord only lets the
// application's testers authorize it until the application is approved for RPC.
DLLINTERFACE SpeakingSourcePtr MakeRpcSpeakingSource(const std::string& AppId);

struct StreamKitOptions
{
	// The WebView2 profile, which keeps StreamKit's authorization between runs
	std::filesystem::path UserDataFolder;
	// The channel picked last time, watched while not following the user. Empty until one has been picked.
	GuildId Guild;
	ChannelId Channel;
	bool FollowUser = true;
};

// Discord StreamKit's voice widget, loaded in a hidden WebView2. The widget is for one channel, so following the user
// loads it again for each channel they join.
DLLINTERFACE SpeakingSourcePtr MakeStreamKitSpeakingSource(StreamKitOptions Options);
}
