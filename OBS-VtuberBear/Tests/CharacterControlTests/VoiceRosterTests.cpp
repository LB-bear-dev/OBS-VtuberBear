// Who is in the voice channel and who is speaking, as the speaking source's events leave it
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "PCH.h"
#include "Discord/VoiceRoster.h"

using namespace CharacterControlOBS;
using DiscordBear::SpeakingStatus;
using DiscordBear::VoiceMember;

namespace
{
class VoiceRosterTest : public testing::Test
{
protected:
	void SetUp() override
	{
		Events = Roster.MakeEvents();
		Events.StatusChanged(SpeakingStatus::Connected);
		Events.RosterReset({VoiceMember{.User = "1", .Name = "alice"}, VoiceMember{.User = "2", .Name = "bob"}});
	}

	VoiceRoster Roster;
	DiscordBear::SpeakingEvents Events;
};
}

TEST_F(VoiceRosterTest, MembersAreInTheCallAndNotSpeaking)
{
	EXPECT_EQ(Roster.GetSpeaking("1"), false);
	EXPECT_EQ(Roster.GetSpeaking("2"), false);
	EXPECT_EQ(Roster.GetSpeaking("3"), std::nullopt);
	EXPECT_EQ(Roster.GetStatus(), SpeakingStatus::Connected);
}

TEST_F(VoiceRosterTest, SpeakingIsTracked)
{
	Events.SpeakingChanged("1", true);
	EXPECT_EQ(Roster.GetSpeaking("1"), true);
	EXPECT_EQ(Roster.GetSpeaking("2"), false);

	Events.SpeakingChanged("1", false);
	EXPECT_EQ(Roster.GetSpeaking("1"), false);
}

TEST_F(VoiceRosterTest, SpeakingForSomeoneNotInTheCallIsIgnored)
{
	Events.SpeakingChanged("3", true);

	EXPECT_EQ(Roster.GetSpeaking("3"), std::nullopt);
}

TEST_F(VoiceRosterTest, ResetReplacesEveryone)
{
	Events.SpeakingChanged("1", true);
	Events.RosterReset({VoiceMember{.User = "3"}});

	EXPECT_EQ(Roster.GetSpeaking("1"), std::nullopt);
	EXPECT_EQ(Roster.GetSpeaking("2"), std::nullopt);
	EXPECT_EQ(Roster.GetSpeaking("3"), false);
}

TEST_F(VoiceRosterTest, UpdateAddsOrKeepsSpeaking)
{
	Events.SpeakingChanged("1", true);
	Events.MemberUpdated(VoiceMember{.User = "1", .Muted = true});
	Events.MemberUpdated(VoiceMember{.User = "3"});

	EXPECT_EQ(Roster.GetSpeaking("1"), true);
	EXPECT_EQ(Roster.GetSpeaking("3"), false);
}

TEST_F(VoiceRosterTest, LeavingRemovesTheMember)
{
	Events.MemberLeft("2");

	EXPECT_EQ(Roster.GetSpeaking("2"), std::nullopt);
	EXPECT_EQ(Roster.GetSpeaking("1"), false);
}

TEST_F(VoiceRosterTest, MembersAreListedByName)
{
	Events.MemberUpdated(VoiceMember{.User = "3", .Name = "aaron"});
	Events.SpeakingChanged("2", true);

	const std::vector<VoiceRoster::MemberState> Members = Roster.GetMembers();
	ASSERT_EQ(Members.size(), 3u);
	EXPECT_EQ(Members[0].Member.Name, "aaron");
	EXPECT_EQ(Members[1].Member.Name, "alice");
	EXPECT_EQ(Members[2].Member.Name, "bob");
	EXPECT_TRUE(Members[2].Speaking);
	EXPECT_FALSE(Members[1].Speaking);
}

TEST_F(VoiceRosterTest, LosingTheConnectionStopsEveryoneSpeakingButKeepsThemInTheCall)
{
	Events.SpeakingChanged("1", true);
	Events.SpeakingChanged("2", true);
	Events.StatusChanged(SpeakingStatus::Starting);

	EXPECT_EQ(Roster.GetSpeaking("1"), false);
	EXPECT_EQ(Roster.GetSpeaking("2"), false);
	EXPECT_EQ(Roster.GetStatus(), SpeakingStatus::Starting);
}
