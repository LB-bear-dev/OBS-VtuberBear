#include <string>
#include <gtest/gtest.h>
#include "ObsStubs.h"
#include "PCH.h"
#include "CharacterRig/Condition.h"

using namespace CharacterControlOBS;

namespace
{
class ConditionTest : public testing::Test
{
protected:
	void SetUp() override
	{
		ObsStubs::Reset();
	}

	static std::optional<Condition> Parse(const Str& Text)
	{
		return ParseCondition(nlohmann::json::parse(Text));
	}

	static void ExpectRefused(const Str& Text, const Str& Reason)
	{
		SCOPED_TRACE(Text);
		ObsStubs::Reset();

		EXPECT_EQ(Parse(Text), std::nullopt);
		EXPECT_TRUE(ObsStubs::LogContains(Reason)) << "expected the log to say: " << Reason;
	}

	static Condition Compare(const Str& Attribute, const Condition::Operator Operator, Attribute::Value Value)
	{
		Condition Result;
		Result.Attribute = Attribute;
		Result.Op = Operator;
		Result.Value = std::move(Value);
		return Result;
	}
};
}

TEST(AttributeValue, ReadsEachType)
{
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("true")), Attribute::Value(true));
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("5")), Attribute::Value(int64_t{5}));
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("-5")), Attribute::Value(int64_t{-5}));
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("\"happy\"")), Attribute::Value(Str("happy")));
}

TEST(AttributeValue, KeepsFloatsAsFloats)
{
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("1.5")), Attribute::Value(1.5));
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("2.0")), Attribute::Value(2.0));
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("1e3")), Attribute::Value(1000.0));
}

TEST(AttributeValue, RefusesWhatIsNotAValue)
{
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("null")), std::nullopt);
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("[1]")), std::nullopt);
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("{}")), std::nullopt);
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("18446744073709551615")), std::nullopt);
	EXPECT_EQ(ReadAttributeValue(nlohmann::json::parse("9223372036854775807")), Attribute::Value(int64_t{9223372036854775807}));
}

TEST_F(ConditionTest, ADefaultConditionAlwaysHolds)
{
	const Condition Always;

	EXPECT_EQ(Always.Op, Condition::Operator::All);
	EXPECT_TRUE(Always.ChildConditions.empty());
}

TEST_F(ConditionTest, ReadsAComparison)
{
	EXPECT_EQ(Parse(R"({"attr": "Talking", "eq": true})"), Compare("Talking", Condition::Operator::Equal, true));
	EXPECT_EQ(Parse(R"({"attr": "mood", "ne": "sleepy"})"), Compare("mood", Condition::Operator::NotEqual, Str("sleepy")));
	EXPECT_EQ(Parse(R"({"attr": "Volume", "lt": 3})"), Compare("Volume", Condition::Operator::Less, int64_t{3}));
	EXPECT_EQ(Parse(R"({"attr": "Volume", "gt": 3})"), Compare("Volume", Condition::Operator::Greater, int64_t{3}));
	EXPECT_EQ(Parse(R"({"attr": "Volume", "le": 3})"), Compare("Volume", Condition::Operator::LessEqual, int64_t{3}));
	EXPECT_EQ(Parse(R"({"attr": "Volume", "ge": 3})"), Compare("Volume", Condition::Operator::GreaterEqual, int64_t{3}));
	EXPECT_EQ(Parse(R"({"attr": "mood", "contains": "py"})"), Compare("mood", Condition::Operator::Contains, Str("py")));
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(ConditionTest, ComparesIntsAndFloatsAsNumbers)
{
	EXPECT_EQ(Parse(R"({"attr": "Volume", "gt": 1.5})"), Compare("Volume", Condition::Operator::Greater, 1.5));
	EXPECT_EQ(Parse(R"({"attr": "Scale", "le": 2})"), Compare("Scale", Condition::Operator::LessEqual, int64_t{2}));
}

TEST_F(ConditionTest, AcceptsAnyAttributeName)
{
	EXPECT_EQ(Parse(R"({"attr": "Shouting", "eq": true})"), Compare("Shouting", Condition::Operator::Equal, true));
	EXPECT_EQ(Parse(R"({"attr": "Talking", "eq": "loud"})"), Compare("Talking", Condition::Operator::Equal, Str("loud")));
	EXPECT_TRUE(ObsStubs::LogMessages().empty());
}

TEST_F(ConditionTest, ReadsNestedConditions)
{
	const std::optional<Condition> Result = Parse(R"({"all": [
		{"attr": "Talking", "eq": true},
		{"any": [{"attr": "mood", "eq": "angry"}, {"attr": "Volume", "gt": 5}]},
		{"not": {"attr": "mood", "eq": "sleepy"}}
	]})");

	ASSERT_TRUE(Result);
	EXPECT_EQ(Result->Op, Condition::Operator::All);
	ASSERT_EQ(Result->ChildConditions.size(), 3u);
	EXPECT_EQ(Result->ChildConditions[0], Compare("Talking", Condition::Operator::Equal, true));

	const Condition& Any = Result->ChildConditions[1];
	EXPECT_EQ(Any.Op, Condition::Operator::Any);
	ASSERT_EQ(Any.ChildConditions.size(), 2u);
	EXPECT_EQ(Any.ChildConditions[0], Compare("mood", Condition::Operator::Equal, Str("angry")));
	EXPECT_EQ(Any.ChildConditions[1], Compare("Volume", Condition::Operator::Greater, int64_t{5}));

	const Condition& Not = Result->ChildConditions[2];
	EXPECT_EQ(Not.Op, Condition::Operator::Not);
	ASSERT_EQ(Not.ChildConditions.size(), 1u);
	EXPECT_EQ(Not.ChildConditions[0], Compare("mood", Condition::Operator::Equal, Str("sleepy")));
}

TEST_F(ConditionTest, AcceptsEmptyGroups)
{
	const std::optional<Condition> All = Parse(R"({"all": []})");
	const std::optional<Condition> Any = Parse(R"({"any": []})");

	ASSERT_TRUE(All);
	EXPECT_EQ(All->Op, Condition::Operator::All);
	EXPECT_TRUE(All->ChildConditions.empty());
	ASSERT_TRUE(Any);
	EXPECT_EQ(Any->Op, Condition::Operator::Any);
	EXPECT_TRUE(Any->ChildConditions.empty());
}

TEST_F(ConditionTest, RefusesWhatIsNotACondition)
{
	ExpectRefused(R"([{"attr": "Talking", "eq": true}])", "A condition must be a JSON object.");
	ExpectRefused(R"({})", "A condition needs exactly one of \"all\", \"any\", \"not\" or \"attr\".");
	ExpectRefused(R"({"all": [], "attr": "Talking", "eq": true})", "A condition needs exactly one of");
	ExpectRefused(R"({"all": {"attr": "Talking", "eq": true}})", "\"all\" must be a list of conditions.");
	ExpectRefused(R"({"any": "Talking"})", "\"any\" must be a list of conditions.");
	ExpectRefused(R"({"not": [{"attr": "Talking", "eq": true}]})", "A condition must be a JSON object.");
	ExpectRefused(R"({"all": [], "when": true})", "Unknown key \"when\" in a \"all\" condition.");
}

TEST_F(ConditionTest, RefusesABadComparison)
{
	ExpectRefused(R"({"attr": 5, "eq": true})", "\"attr\" must be a string.");
	ExpectRefused(R"({"attr": "Talking"})", "The condition on \"Talking\" has no operator.");
	ExpectRefused(R"({"attr": "Volume", "gt": 1, "lt": 5})", "The condition on \"Volume\" has more than one operator.");
	ExpectRefused(R"({"attr": "Talking", "equals": true})", "Attribute \"Talking\" uses unknown operator equals.");
	ExpectRefused(R"({"attr": "Talking", "eq": null})", "\"Talking\" must be compared with a bool, number or string, not null.");
	ExpectRefused(R"({"attr": "mood", "eq": ["happy"]})", "\"mood\" must be compared with a bool, number or string, not [\"happy\"].");
}

TEST_F(ConditionTest, RefusesAnOperatorItsValueDoesNotSupport)
{
	ExpectRefused(R"({"attr": "Talking", "gt": false})", "The condition on \"Talking\" can't use \"gt\" with a value of type bool.");
	ExpectRefused(R"({"attr": "mood", "lt": "b"})", "The condition on \"mood\" can't use \"lt\" with a value of type string.");
	ExpectRefused(R"({"attr": "Volume", "contains": 5})", "The condition on \"Volume\" can't use \"contains\" with a value of type int.");
	ExpectRefused(R"({"attr": "Scale", "contains": 1.5})", "The condition on \"Scale\" can't use \"contains\" with a value of type float.");
}

TEST_F(ConditionTest, RefusesTheWholeTreeForOneBadLeaf)
{
	ExpectRefused(R"({"all": [{"attr": "Talking", "eq": true}, {"any": [{"not": {"attr": "mood", "lt": "b"}}]}]})", "The condition on \"mood\" can't use \"lt\"");
}

TEST_F(ConditionTest, LimitsHowDeepConditionsNest)
{
	const auto Nested = [](const int Depth)
	{
		Str Text = R"({"attr": "Talking", "eq": true})";
		for(int Level = 1; Level < Depth; ++Level)
		{
			Text.insert(0, R"({"not": )");
			Text += "}";
		}

		return Text;
	};

	EXPECT_TRUE(Parse(Nested(MAX_CONDITION_DEPTH)));
	ExpectRefused(Nested(MAX_CONDITION_DEPTH + 1), "Conditions can nest at most 32 deep.");
}
