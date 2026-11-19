// Copyright (C) 2026 LB-bear-dev <lb_burr@outlook.com>
#pragma once
#include <mutex>
#include <optional>
#include <unordered_map>
#include <DiscordBear/SpeakingSource.h>

namespace CharacterControlOBS
{
class VoiceRoster
{
public:
	struct MemberState
	{
		DiscordBear::VoiceMember Member;
		bool Speaking = false;

		bool operator==(const MemberState&) const = default;
	};

	DiscordBear::SpeakingEvents MakeEvents();

	std::optional<bool> GetSpeaking(const DiscordBear::UserId& Id) const;
	DiscordBear::SpeakingStatus GetStatus() const;
	std::vector<MemberState> GetMembers() const;

	void Reset(std::vector<DiscordBear::VoiceMember>&& NewMembers);
	void Update(DiscordBear::VoiceMember&& Member);
	void Remove(const DiscordBear::UserId& Id);
	void SetSpeaking(const DiscordBear::UserId& Id, bool Speaking);
	void SetStatus(DiscordBear::SpeakingStatus NewStatus);

private:
	mutable std::mutex Mutex;
	std::unordered_map<DiscordBear::UserId, MemberState> Members;
	DiscordBear::SpeakingStatus Status = DiscordBear::SpeakingStatus::Starting;
};
}
