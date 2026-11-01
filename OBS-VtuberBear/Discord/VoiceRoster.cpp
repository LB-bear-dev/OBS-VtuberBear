#include "PCH.h"
#include "Discord/VoiceRoster.h"
#include <algorithm>
#include <ranges>
#include <tuple>

using namespace CharacterControlOBS;
using DiscordBear::SpeakingStatus;

DiscordBear::SpeakingEvents VoiceRoster::MakeEvents()
{
	return {
		.RosterReset = [this](std::vector<DiscordBear::VoiceMember>&& NewMembers)
		{
			Reset(std::move(NewMembers));
		},
		.MemberUpdated = [this](DiscordBear::VoiceMember&& Member)
		{
			Update(std::move(Member));
		},
		.MemberLeft = [this](const DiscordBear::UserId& Id)
		{
			Remove(Id);
		},
		.SpeakingChanged = [this](const DiscordBear::UserId& Id, const bool Speaking)
		{
			SetSpeaking(Id, Speaking);
		},
		.StatusChanged = [this](const SpeakingStatus NewStatus)
		{
			SetStatus(NewStatus);
		},
		.ServersListed = {},
		.VoiceChannelsListed = {},
		.ChannelWatched = {},
	};
}

std::optional<bool> VoiceRoster::GetSpeaking(const DiscordBear::UserId& Id) const
{
	const std::scoped_lock Lock(Mutex);

	if(const auto Found = Members.find(Id); Found != Members.end())
	{
		return Found->second.Speaking;
	}

	return std::nullopt;
}

SpeakingStatus VoiceRoster::GetStatus() const
{
	const std::scoped_lock Lock(Mutex);
	return Status;
}

std::vector<VoiceRoster::MemberState> VoiceRoster::GetMembers() const
{
	std::vector<MemberState> Result;
	{
		const std::scoped_lock Lock(Mutex);
		Result.reserve(Members.size());

		for(const auto& Member : Members | std::views::values)
		{
			Result.push_back(Member);
		}
	}

	std::ranges::sort(Result,
		[](const MemberState& Left, const MemberState& Right)
		{
			return std::tie(Left.Member.Name, Left.Member.User) < std::tie(Right.Member.Name, Right.Member.User);
		});

	return Result;
}

void VoiceRoster::Reset(std::vector<DiscordBear::VoiceMember>&& NewMembers)
{
	const std::scoped_lock Lock(Mutex);
	Members.clear();

	for(DiscordBear::VoiceMember& Member : std::move(NewMembers))
	{
		DiscordBear::UserId Id = Member.User;
		Members.insert_or_assign(std::move(Id), MemberState{.Member = std::move(Member)});
	}
}

void VoiceRoster::Update(DiscordBear::VoiceMember&& Member)
{
	DiscordBear::UserId Id = Member.User;
	const std::scoped_lock Lock(Mutex);
	Members[std::move(Id)].Member = std::move(Member);
}

void VoiceRoster::Remove(const DiscordBear::UserId& Id)
{
	const std::scoped_lock Lock(Mutex);
	Members.erase(Id);
}

void VoiceRoster::SetSpeaking(const DiscordBear::UserId& Id, const bool Speaking)
{
	const std::scoped_lock Lock(Mutex);

	if(const auto Found = Members.find(Id); Found != Members.end())
	{
		Found->second.Speaking = Speaking;
	}
}

void VoiceRoster::SetStatus(const SpeakingStatus NewStatus)
{
	const std::scoped_lock Lock(Mutex);
	Status = NewStatus;

	if(NewStatus != SpeakingStatus::Connected)
	{
		for(auto& [Member, Speaking] : Members | std::views::values)
		{
			Speaking = false;
		}
	}
}
