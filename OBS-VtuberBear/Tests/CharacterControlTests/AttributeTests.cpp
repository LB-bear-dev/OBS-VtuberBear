#include <gtest/gtest.h>
#include "PCH.h"
#include "CharacterRig/Attribute.h"

using namespace CharacterControlOBS;

TEST(Attribute, NamesItsTypes)
{
	EXPECT_STREQ(Attribute::GetTypeName(Attribute::Value(true)), "bool");
	EXPECT_STREQ(Attribute::GetTypeName(Attribute::Value(int64_t{1})), "int");
	EXPECT_STREQ(Attribute::GetTypeName(Attribute::Value(1.0)), "float");
	EXPECT_STREQ(Attribute::GetTypeName(Attribute::Value(Str("a"))), "string");
}

TEST(Attribute, TakesItsTypeFromItsDefault)
{
	EXPECT_STREQ(Attribute(false).GetTypeName(), "bool");
	EXPECT_STREQ(Attribute(int64_t{0}).GetTypeName(), "int");
	EXPECT_STREQ(Attribute(1.0).GetTypeName(), "float");
	EXPECT_STREQ(Attribute(Str("happy")).GetTypeName(), "string");
}

TEST(Attribute, StartsAtItsDefault)
{
	const Attribute Mood(Str("happy"));

	EXPECT_EQ(Mood.Get(), Attribute::Value(Str("happy")));
}

TEST(Attribute, SetsAValueOfItsType)
{
	Attribute Talking(false);

	EXPECT_TRUE(Talking.Set(true));
	EXPECT_EQ(Talking.Get(), Attribute::Value(true));
}

TEST(Attribute, GetsOnlyItsOwnType)
{
	const Attribute Volume(int64_t{3});

	ASSERT_NE(Volume.Get<int64_t>(), nullptr);
	EXPECT_EQ(*Volume.Get<int64_t>(), 3);
	EXPECT_EQ(Volume.Get<double>(), nullptr);
	EXPECT_EQ(Volume.Get<bool>(), nullptr);
}

TEST(Attribute, RefusesAValueOfAnotherType)
{
	Attribute Volume(int64_t{3});

	EXPECT_FALSE(Volume.Set(1.5));
	EXPECT_FALSE(Volume.Set(Str("loud")));
	EXPECT_EQ(Volume.Get(), Attribute::Value(int64_t{3}));
}
