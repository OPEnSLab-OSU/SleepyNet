#include "pch.h"

using namespace LoomNet;
using Unit = TimeInterval::Unit;
TEST(TimeInterval, CreateAndAssign) {
	TimeInterval test(Unit::MICROSECOND, 10);
	EXPECT_EQ(test.get_time(), 10);
	EXPECT_EQ(test.get_unit(), Unit::MICROSECOND);
	EXPECT_FALSE(test.is_none());

	test = TimeInterval(Unit::DAY, 5);
	EXPECT_EQ(test.get_time(), 5);
	EXPECT_EQ(test.get_unit(), Unit::DAY);
	EXPECT_FALSE(test.is_none());

	test = TimeInterval(Unit::NONE, 0);
	EXPECT_EQ(test.get_time(), 0);
	EXPECT_EQ(test.get_unit(), Unit::NONE);
	EXPECT_TRUE(test.is_none());
}

TEST(TimeInterval, SingleUnitArithmetic) {
	TimeInterval result(TIME_NONE);
	// simple addition
	result = TimeInterval(Unit::DAY, 2) + TimeInterval(Unit::DAY, 1);
	EXPECT_EQ(result.get_time(), 3);
	EXPECT_EQ(result.get_unit(), Unit::DAY);

	// integer overflow
	result = TimeInterval(Unit::DAY, UINT32_MAX) + TimeInterval(Unit::DAY, 1);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);

	// simple subtraction
	result = TimeInterval(Unit::DAY, 3) - TimeInterval(Unit::DAY, 2);
	EXPECT_EQ(result.get_time(), 1);
	EXPECT_EQ(result.get_unit(), Unit::DAY);

	// negative numbers
	result = TimeInterval(Unit::DAY, 2) - TimeInterval(Unit::DAY, 3);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);

	// simple multiplication
	result = TimeInterval(Unit::DAY, 2) * 5;
	EXPECT_EQ(result.get_time(), 10);
	EXPECT_EQ(result.get_unit(), Unit::DAY);

	// integer overflow
	result = TimeInterval(Unit::DAY, UINT32_MAX) * 2;
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
}

TEST(TimeInterval, MultiUnitAddition) {
	TimeInterval result(TIME_NONE);
	// simple addition, day -> hour
	result = TimeInterval(Unit::DAY, 2) + TimeInterval(Unit::HOUR, 1);
	EXPECT_EQ(result.get_time(), 49);
	EXPECT_EQ(result.get_unit(), Unit::HOUR);

	// simple addition, day -> millisecond
	result = TimeInterval(Unit::DAY, 1) + TimeInterval(Unit::MILLISECOND, 1);
	EXPECT_EQ(result.get_time(), 86400001);
	EXPECT_EQ(result.get_unit(), Unit::MILLISECOND);

	// simple addition, hour -> microsecond
	result = TimeInterval(Unit::HOUR, 1) + TimeInterval(Unit::MICROSECOND, 1);
	EXPECT_EQ(result.get_time(), 3600000001);
	EXPECT_EQ(result.get_unit(), Unit::MICROSECOND);

	// simple addition, day -> hour
	result = TimeInterval(Unit::HOUR, 1) + TimeInterval(Unit::DAY, 2);
	EXPECT_EQ(result.get_time(), 49);
	EXPECT_EQ(result.get_unit(), Unit::HOUR);

	// simple addition, day -> millisecond
	result = TimeInterval(Unit::MILLISECOND, 1) + TimeInterval(Unit::DAY, 1);
	EXPECT_EQ(result.get_time(), 86400001);
	EXPECT_EQ(result.get_unit(), Unit::MILLISECOND);

	// simple addition, hour -> microsecond
	result = TimeInterval(Unit::MICROSECOND, 1) + TimeInterval(Unit::HOUR, 1);
	EXPECT_EQ(result.get_time(), 3600000001);
	EXPECT_EQ(result.get_unit(), Unit::MICROSECOND);

	// integer overflow
	result = TimeInterval(Unit::DAY, 1) + TimeInterval(Unit::MICROSECOND, 1);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);

	// integer overflow
	result = TimeInterval(Unit::MICROSECOND, 1) + TimeInterval(Unit::DAY, 1);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
}

TEST(TimeInterval, MultiUnitSubtraction) {
	TimeInterval result(TIME_NONE);
	// simple subtraction, day -> hour
	result = TimeInterval(Unit::DAY, 2) - TimeInterval(Unit::HOUR, 1);
	EXPECT_EQ(result.get_time(), 47);
	EXPECT_EQ(result.get_unit(), Unit::HOUR);

	// simple subtraction, day -> millisecond
	result = TimeInterval(Unit::DAY, 1) - TimeInterval(Unit::MILLISECOND, 1);
	EXPECT_EQ(result.get_time(), 86399999);
	EXPECT_EQ(result.get_unit(), Unit::MILLISECOND);

	// simple subtraction, hour -> microsecond
	result = TimeInterval(Unit::HOUR, 1) - TimeInterval(Unit::MICROSECOND, 1);
	EXPECT_EQ(result.get_time(), 3599999999);
	EXPECT_EQ(result.get_unit(), Unit::MICROSECOND);

	// negative numbers
	result = TimeInterval(Unit::HOUR, 1) - TimeInterval(Unit::DAY, 2);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
	result = TimeInterval(Unit::MILLISECOND, 1) - TimeInterval(Unit::DAY, 1);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
	result = TimeInterval(Unit::MILLISECOND, 1) - TimeInterval(Unit::HOUR, 1);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
}

TEST(TimeInterval, SingleUnitComparision) {
	const TimeInterval left(Unit::DAY, 1);
	const TimeInterval right1(Unit::DAY, 0);
	const TimeInterval right2(Unit::DAY, 2);

	EXPECT_TRUE(left == left);
	EXPECT_FALSE(left == right1);
	EXPECT_FALSE(left != left);
	EXPECT_TRUE(left != right1);

	EXPECT_FALSE(left > left);
	EXPECT_TRUE(left > right1);
	EXPECT_FALSE(left > right2);

	EXPECT_FALSE(left < left);
	EXPECT_FALSE(left < right1);
	EXPECT_TRUE(left < right2);

	EXPECT_TRUE(left >= left);
	EXPECT_TRUE(left >= right1);
	EXPECT_FALSE(left >= right2);

	EXPECT_TRUE(left <= left);
	EXPECT_FALSE(left <= right1);
	EXPECT_TRUE(left <= right2);
}

TEST(TimeInterval, MultiUnitComparision) {
	const TimeInterval left(Unit::HOUR, 1);
	const TimeInterval right1(Unit::MINUTE, 60);
	const TimeInterval right2(Unit::SECOND, 1);
	const TimeInterval right3(Unit::DAY, 1);

	EXPECT_TRUE(left == right1);
	EXPECT_FALSE(left == right2);
	EXPECT_FALSE(left != right1);
	EXPECT_TRUE(left != right2);

	EXPECT_FALSE(left > right1);
	EXPECT_TRUE(left > right2);
	EXPECT_FALSE(left > right3);

	EXPECT_FALSE(left < right1);
	EXPECT_FALSE(left < right2);
	EXPECT_TRUE(left < right3);

	EXPECT_TRUE(left >= right1);
	EXPECT_TRUE(left >= right2);
	EXPECT_FALSE(left >= right3);

	EXPECT_TRUE(left <= right1);
	EXPECT_FALSE(left <= right2);
	EXPECT_TRUE(left <= right3);
}

TEST(TimeInterval, NoneArithmetic) {
	TimeInterval result(TIME_NONE);
	// simple addition
	result = TimeInterval(Unit::DAY, 2) + TIME_NONE;
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
	result = TIME_NONE + TimeInterval(Unit::DAY, 2);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);

	// simple subtraction
	result = TimeInterval(Unit::DAY, 3) - TIME_NONE;
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
	result = TIME_NONE - TimeInterval(Unit::DAY, 3);
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);

	// simple multiplication
	result = TIME_NONE * 5;
	EXPECT_EQ(result.get_time(), 0);
	EXPECT_EQ(result.get_unit(), Unit::NONE);
}