#pragma once
#include <mutex>
#include <optional>
#include <unordered_map>
#include <DiscordBear/SpeakingSource.h>

namespace CharacterControlOBS
{
// Who is in the voice channel and whether each of them is speaking. A speaking source's events update it on the source's
// thread while OBS's graphics thread reads it every tick.
class VoiceRoster
{
public:
	struct MemberState
	{
		DiscordBear::VoiceMember Member;
		bool Speaking = false;

		bool operator==(const MemberState&) const = default;
	};

	// Events that keep this roster up to date, the roster and status ones only. The roster must outlive the source they are given to.
	DiscordBear::SpeakingEvents MakeEvents();

	// Whether the user is speaking, empty while they aren't in the call
	[[nodiscard]] std::optional<bool> GetSpeaking(const DiscordBear::UserId& Id) const;
	[[nodiscard]] DiscordBear::SpeakingStatus GetStatus() const;
	// Everyone in the call, by name
	[[nodiscard]] std::vector<MemberState> GetMembers() const;

	void Reset(std::vector<DiscordBear::VoiceMember>&& NewMembers);
	// Adds the member, or updates them and keeps whether they are speaking
	void Update(DiscordBear::VoiceMember&& Member);
	void Remove(const DiscordBear::UserId& Id);
	// Ignored for anyone not in the call
	void SetSpeaking(const DiscordBear::UserId& Id, bool Speaking);
	// Anything but Connected stops everyone speaking, as no event will come to say they stopped. The members stay, so their
	// characters aren't torn down over a connection that is only gone for a moment.
	void SetStatus(DiscordBear::SpeakingStatus NewStatus);

private:
	mutable std::mutex Mutex;
	std::unordered_map<DiscordBear::UserId, MemberState> Members;
	DiscordBear::SpeakingStatus Status = DiscordBear::SpeakingStatus::Starting;
};
}
