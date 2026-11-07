#include <string>
#include <gtest/gtest.h>
#include "ObsStubs.h"
#include "PCH.h"
#include "TestSupport.h"
#include "CharacterRig/PathRules.h"
#include "CharacterRig/Rig.h"

using namespace CharacterControlOBS;
using namespace CharacterControlTests;

namespace
{
const FilePath& RigPath()
{
	static const FilePath PATH(L"C:\\Characters\\123456789012345678\\rig.json");
	return PATH;
}

class RigTest : public testing::Test
{
protected:
	void SetUp() override
	{
		ObsStubs::Reset();
	}

	static std::optional<Rig> Parse(const Str& Text)
	{
		return ParseRig(Text, RigPath());
	}

	static void ExpectRefused(const Str& Text, const Str& Reason)
	{
		SCOPED_TRACE(Text);
		ObsStubs::Reset();

		EXPECT_EQ(Parse(Text), std::nullopt);
		EXPECT_TRUE(ObsStubs::LogContains("rig.json")) << "the log didn't name the rig file";
		EXPECT_TRUE(ObsStubs::LogContains(Reason)) << "expected the log to say: " << Reason;
	}

	static Condition TalkingIsTrue()
	{
		return Condition{.Op = Condition::Operator::Equal, .ChildConditions = {}, .Attribute = "Talking", .Value = true};
	}
};
}

TEST_F(RigTest, ReadsTheSimplestRig)
{
	const std::optional<Rig> Result = Parse(R"({
		"layers": [{"name": "body", "slots": ["talking"]}],
		"slots": [{
			"name": "talking",
			"values": ["not_talking", "talking"],
			"rules": [{"value": "talking", "when": {"attr": "Talking", "eq": true}}]
		}]
	})");

	ASSERT_TRUE(Result);
	ASSERT_EQ(Result->Layers.size(), 1u);
	EXPECT_EQ(Result->Layers[0], (RigLayer{.Name = "body", .Parent = {}, .Slots = {"talking"}}));
	ASSERT_EQ(Result->Slots.size(), 1u);

	const auto& [Name, Values, Rules] = Result->Slots[0];
	EXPECT_EQ(Name, "talking");
	EXPECT_EQ(Values, (std::vector<StrId>{"not_talking", "talking"}));
	ASSERT_EQ(Rules.size(), 1u);
	EXPECT_EQ(Rules[0], (SlotRule{.Value = "talking", .When = TalkingIsTrue()}));
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(RigTest, KeepsLayersAndSlotsInOrder)
{
	const std::optional<Rig> Result = Parse(R"({
		"layers": [
			{"name": "body", "slots": ["talking"]},
			{"name": "head", "parent": "body", "slots": ["talking"]},
			{"name": "eyes", "parent": "head", "slots": ["talking", "blink"]},
			{"name": "mouth", "parent": "head", "slots": ["talking"]},
			{"name": "hat", "parent": "head"}
		],
		"slots": [
			{"name": "talking", "values": ["not_talking", "talking"],
			 "rules": [{"value": "talking", "when": {"attr": "Talking", "eq": true}}]},
			{"name": "blink", "values": ["open", "blink"],
			 "rules": [{"value": "blink", "when": {"any": [{"attr": "blink", "eq": true}, {"attr": "mood", "eq": "sleepy"}]}}]}
		]
	})");

	ASSERT_TRUE(Result);
	ASSERT_EQ(Result->Layers.size(), 5u);
	EXPECT_EQ(Result->Layers[2], (RigLayer{.Name = "eyes", .Parent = "head", .Slots = {"talking", "blink"}}));
	EXPECT_EQ(Result->Layers[4].Name, "hat");
	EXPECT_TRUE(Result->Layers[4].Slots.empty());
	ASSERT_EQ(Result->Slots.size(), 2u);
	EXPECT_EQ(Result->Slots[0].Name, "talking");
	EXPECT_EQ(Result->Slots[1].Name, "blink");
	ASSERT_EQ(Result->Slots[1].Rules.size(), 1u);
	EXPECT_EQ(Result->Slots[1].Rules[0].When.Op, Condition::Operator::Any);
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(RigTest, LetsAParentComeAfterItsChild)
{
	const std::optional<Rig> Result = Parse(R"({"layers": [{"name": "hair_back", "parent": "head"}, {"name": "body"}, {"name": "head", "parent": "body"}]})");

	ASSERT_TRUE(Result);
	EXPECT_EQ(Result->Layers[0], (RigLayer{.Name = "hair_back", .Parent = "head", .Slots = {}}));
}

TEST_F(RigTest, NeedsNoSlots)
{
	const std::optional<Rig> Result = Parse(R"({"layers": [{"name": "background"}]})");

	ASSERT_TRUE(Result);
	EXPECT_TRUE(Result->Slots.empty());
}

TEST_F(RigTest, ARuleWithoutWhenAlwaysHolds)
{
	const std::optional<Rig> Result = Parse(R"({"layers": [{"name": "body", "slots": ["mood"]}], "slots": [{"name": "mood", "values": ["calm", "happy"], "rules": [{"value": "happy"}]}]})");

	ASSERT_TRUE(Result);
	ASSERT_EQ(Result->Slots[0].Rules.size(), 1u);
	EXPECT_EQ(Result->Slots[0].Rules[0], (SlotRule{.Value = "happy", .When = Condition{}}));
}

TEST_F(RigTest, IgnoresKeysItDoesNotKnow)
{
	const std::optional<Rig> Result = Parse(R"({
		"version": 2,
		"layers": [{"name": "body", "pivot": [0, 0], "slots": ["talking"]}],
		"slots": [{"name": "talking", "values": ["not_talking", "talking"], "layers": [], "rules": [{"value": "talking", "if": true}]}]
	})");

	ASSERT_TRUE(Result);
	EXPECT_EQ(Result->Layers[0], (RigLayer{.Name = "body", .Parent = {}, .Slots = {"talking"}}));
	ASSERT_EQ(Result->Slots[0].Rules.size(), 1u);
	EXPECT_EQ(Result->Slots[0].Rules[0], (SlotRule{.Value = "talking", .When = Condition{}}));
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(RigTest, RefusesWhatIsNotARig)
{
	ExpectRefused(R"({"layers": [)", "Json is malformed.");
	ExpectRefused(R"([{"name": "body"}])", "must be a JSON object.");
	ExpectRefused(R"({})", "the rig needs \"layers\", a list of layers.");
	ExpectRefused(R"({"layers": {"name": "body"}})", "the rig needs \"layers\", a list of layers.");
}

TEST_F(RigTest, RefusesABadLayer)
{
	ExpectRefused(R"({"layers": ["body"]})", "must be a JSON object.");
	ExpectRefused(R"({"layers": [{"parent": "body"}]})", "needs a \"name\".");
	ExpectRefused(R"({"layers": [{"name": ""}]})", "\"name\" must be a non-empty string.");
	ExpectRefused(R"({"layers": [{"name": 5}]})", "\"name\" must be a non-empty string.");
	ExpectRefused(R"({"layers": [{"name": "body"}, {"name": "body"}]})", "there are two layers named \"body\".");
	ExpectRefused(R"({"layers": [{"name": "head", "parent": ""}]})", "\"parent\" must be a non-empty string.");
}

TEST_F(RigTest, RefusesBadLayerSlots)
{
	ExpectRefused(R"({"layers": [{"name": "body", "slots": "talking"}]})", "\"slots\" must be a list of names.");
	ExpectRefused(R"({"layers": [{"name": "body", "slots": [""]}]})", "\"slots\" must only hold non-empty strings.");
	ExpectRefused(R"({"layers": [{"name": "body", "slots": ["talking", "talking"]}], "slots": [{"name": "talking", "values": ["a"]}]})", "\"slots\" lists \"talking\" twice.");
	ExpectRefused(R"({"layers": [{"name": "body", "slots": ["talking"]}]})", "layer \"body\" lists slot \"talking\", which isn't a slot.");
}

TEST_F(RigTest, RefusesABadParent)
{
	ExpectRefused(R"({"layers": [{"name": "head", "parent": "body"}]})", "layer \"head\" has parent \"body\", which isn't a layer.");
	ExpectRefused(R"({"layers": [{"name": "head", "parent": "head"}]})", "layer \"head\" is its own ancestor.");
	ExpectRefused(R"({"layers": [{"name": "a", "parent": "b"}, {"name": "b", "parent": "c"}, {"name": "c", "parent": "a"}]})", "layer \"a\" is its own ancestor.");
}

TEST_F(RigTest, RefusesABadSlot)
{
	const auto WithSlots = [](const Str& Slots)
	{
		return R"({"layers": [{"name": "body"}], "slots": )" + Slots + "}";
	};

	ExpectRefused(WithSlots(R"({"name": "talking"})"), "\"slots\" must be a list of slots.");
	ExpectRefused(WithSlots(R"(["talking"])"), "slot 1 must be a JSON object.");
	ExpectRefused(WithSlots(R"([{"values": ["a"]}])"), "needs a \"name\".");
	ExpectRefused(WithSlots(R"([{"name": "talking"}])"), "needs \"values\".");
	ExpectRefused(WithSlots(R"([{"name": "talking", "values": "a"}])"), "\"values\" must be a list of names.");
	ExpectRefused(WithSlots(R"([{"name": "talking", "values": []}])"), "needs at least one value.");
	ExpectRefused(WithSlots(R"([{"name": "talking", "values": ["a", ""]}])"), "\"values\" must only hold non-empty strings.");
	ExpectRefused(WithSlots(R"([{"name": "talking", "values": ["a", "a"]}])"), "\"values\" lists \"a\" twice.");
	ExpectRefused(WithSlots(R"([{"name": "talking", "values": ["a+b"]}])"), "but values can't contain \"+\".");
	ExpectRefused(WithSlots(R"([{"name": "talking", "values": ["a"]}, {"name": "talking", "values": ["b"]}])"), "there are two slots named \"talking\".");
}

TEST_F(RigTest, RefusesABadRule)
{
	const auto WithRules = [](const Str& Rules)
	{
		return R"({"layers": [{"name": "body", "slots": ["talking"]}], "slots": [{"name": "talking", "values": ["not_talking", "talking"], "rules": )" + Rules + "}]}";
	};

	ExpectRefused(WithRules(R"({"value": "talking"})"), "must be a list of rules.");
	ExpectRefused(WithRules(R"(["talking"])"), "slot \"talking\" rule 1 must be a JSON object.");
	ExpectRefused(WithRules(R"([{"when": {"attr": "Talking", "eq": true}}])"), "needs a \"value\".");
	ExpectRefused(WithRules(R"([{"value": "talking"}, {"value": "shouting"}])"), "slot \"talking\" rule 2 sets \"shouting\", which isn't one of the slot's values.");
}

TEST_F(RigTest, SaysWhichRuleHasABadCondition)
{
	ExpectRefused(R"({"layers": [{"name": "body", "slots": ["talking"]}], "slots": [{"name": "talking", "values": ["not_talking", "talking"], "rules": [{"value": "talking", "when": {"attr": "Talking", "gt": true}}]}]})",
		"slot \"talking\" rule 1 has an invalid condition.");
	EXPECT_TRUE(ObsStubs::LogContains("The condition on \"Talking\" can't use \"gt\" with a value of type bool."));
}

TEST_F(RigTest, LoadsTheRigFile)
{
	const TempDir Dir;
	const FilePath Path = Dir.File("rig.json");
	WriteText(Path, R"({"layers": [{"name": "body", "slots": ["talking"]}], "slots": [{"name": "talking", "values": ["not_talking", "talking"]}]})");

	const std::optional<Rig> Result = LoadRig(Path);

	ASSERT_TRUE(Result);
	ASSERT_EQ(Result->Layers.size(), 1u);
	EXPECT_EQ(Result->Layers[0], (RigLayer{.Name = "body", .Parent = {}, .Slots = {"talking"}}));
	ASSERT_EQ(Result->Slots.size(), 1u);
	EXPECT_EQ(Result->Slots[0], (RigSlot{.Name = "talking", .Values = {"not_talking", "talking"}, .Rules = {}}));
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(RigTest, SaysWhenThereIsNoRigFile)
{
	const TempDir Dir;

	EXPECT_EQ(LoadRig(Dir.File("rig.json")), std::nullopt);
	EXPECT_TRUE(ObsStubs::LogContains("Cannot open"));
	EXPECT_TRUE(ObsStubs::LogContains("rig.json"));
}

TEST_F(RigTest, NamesTheLoadedFileWhenItIsRefused)
{
	const TempDir Dir;
	const FilePath Path = Dir.File("rig.json");
	WriteText(Path, R"({"layers": [{"name": "head", "parent": "body"}]})");

	EXPECT_EQ(LoadRig(Path), std::nullopt);
	EXPECT_TRUE(ObsStubs::LogContains(LogPath(Path) + ": layer \"head\" has parent \"body\", which isn't a layer."));
}
