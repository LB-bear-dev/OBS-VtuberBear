#include "PCH.h"
#include "VoiceRoster.h"
#include <algorithm>
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
	};
}

std::optional<bool> VoiceRoster::GetSpeaking(const DiscordBear::UserId& Id) const
{
	std::scoped_lock Lock(Mutex);

	if(const auto Found = Members.find(Id); Found != Members.end())
	{
		return Found->second.Speaking;
	}

	return std::nullopt;
}

SpeakingStatus VoiceRoster::GetStatus() const
{
	std::scoped_lock Lock(Mutex);
	return Status;
}

std::vector<VoiceRoster::MemberState> VoiceRoster::GetMembers() const
{
	std::vector<MemberState> Result;
	{
		std::scoped_lock Lock(Mutex);
		Result.reserve(Members.size());

		for(const auto& [Id, Member] : Members)
		{
			Result.push_back(Member);
		}
	}

	std::ranges::sort(Result, [](const MemberState& Left, const MemberState& Right)
	{
		return std::tie(Left.Member.Name, Left.Member.User) < std::tie(Right.Member.Name, Right.Member.User);
	});

	return Result;
}

void VoiceRoster::Reset(std::vector<DiscordBear::VoiceMember>&& NewMembers)
{
	std::scoped_lock Lock(Mutex);
	Members.clear();

	for(DiscordBear::VoiceMember& Member : NewMembers)
	{
		DiscordBear::UserId Id = Member.User;
		Members.insert_or_assign(std::move(Id), MemberState{.Member = std::move(Member)});
	}
}

void VoiceRoster::Update(DiscordBear::VoiceMember&& Member)
{
	DiscordBear::UserId Id = Member.User;
	std::scoped_lock Lock(Mutex);
	Members[std::move(Id)].Member = std::move(Member);
}

void VoiceRoster::Remove(const DiscordBear::UserId& Id)
{
	std::scoped_lock Lock(Mutex);
	Members.erase(Id);
}

void VoiceRoster::SetSpeaking(const DiscordBear::UserId& Id, const bool Speaking)
{
	std::scoped_lock Lock(Mutex);

	if(const auto Found = Members.find(Id); Found != Members.end())
	{
		Found->second.Speaking = Speaking;
	}
}

void VoiceRoster::SetStatus(const SpeakingStatus NewStatus)
{
	std::scoped_lock Lock(Mutex);
	Status = NewStatus;

	if(NewStatus != SpeakingStatus::Connected)
	{
		for(auto& [Id, Member] : Members)
		{
			Member.Speaking = false;
		}
	}
}
