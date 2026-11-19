#pragma once
#include <functional>
#include <vector>
#include "LogDefinitions.h"

#ifdef DISCORDBEAR
#define DLLINTERFACE   __declspec( dllexport )
#else
#define DLLINTERFACE   __declspec( dllimport )
#endif

namespace DiscordBear
{
using UserId = std::string;
using ChannelId = std::string;

struct DiscordInfo
{
	bool Valid = false;

	DiscordInfo() = default;
};

struct GenericChannelInfo : DiscordInfo
{
	ChannelId Channel;
};

struct SpeakingEventInfo : DiscordInfo
{
	UserId User;
};

struct VoiceUserInfo : DiscordInfo
{
	UserId User;
};

struct VoiceChannelInfo : GenericChannelInfo
{
	std::vector<VoiceUserInfo> VoiceUserList;
};

class DiscordClient
{
public:
	DiscordClient() = default;

	DiscordClient(DiscordClient&) = delete;

	virtual ~DiscordClient() = default;

	virtual bool Connect() = 0;
	virtual void Disconnect() = 0;

	virtual void GetSelectedVoiceChannel(std::function<void(VoiceChannelInfo&&)>) = 0;

	virtual void SubscribeOnVoiceChannelJoin(std::function<void(GenericChannelInfo&&)>) = 0;
	virtual void UnsubscribeOnVoiceChannelJoin() = 0;

	virtual void SubscribeOnVoiceChannelSpeak(const std::vector<ChannelId>& Channels,
		std::function<void(SpeakingEventInfo&&)> SpeakingStartEvent,
		std::function<void(SpeakingEventInfo&&)> SpeakingEndEvent) = 0;
	virtual void UnsubscribeOnVoiceChannelSpeak() = 0;

	virtual void SubscribeOnVoiceChannelEnter(const std::vector<ChannelId>& Channels, std::function<void(VoiceUserInfo&&)> VoiceEnterEvent) = 0;
	virtual void UnsubscribeOnVoiceChannelEnter() = 0;

	virtual void SubscribeOnVoiceChannelExit(const std::vector<ChannelId>& Channels, std::function<void(VoiceUserInfo&&)> VoiceExitEvent) = 0;
	virtual void UnsubscribeOnVoiceChannelExit() = 0;

	virtual void Update() = 0;
	virtual void UpdateAsync() = 0;
};

DLLINTERFACE void SetLogFn(LogFnDefinition LogFnDefinition);

using ClientPtr = std::unique_ptr<DiscordClient>;
DLLINTERFACE ClientPtr GetDiscordClient(const std::string& AppId);
}
