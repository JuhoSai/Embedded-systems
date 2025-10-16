#include <gtest/gtest.h>
#include "../TimeParser.h"

TEST(TimeParserTest, TestCaseCorrectTime) {


    // Test with correct time string
    char time_test[] = "000006";
    ASSERT_EQ(time_parse(time_test),6);

    char time_test2[] = "000105";
    ASSERT_EQ(time_parse(time_test2),65);

}

TEST(TimeParserTest, TestCaseInvalidTime) {
    //Test with invalidTime
    char time_test[] = "126160";
    ASSERT_EQ(time_parse(time_test), TIME_VALUE_ERROR);
    
    char time_test2[] = "126160";
    ASSERT_EQ(time_parse(time_test2), TIME_VALUE_ERROR);
}

TEST(TimeParserTest, TestCaseInvalidHour) {
    //Test with invalidhour
    char time_test[] = "240000";
    ASSERT_EQ(time_parse(time_test), TIME_VALUE_ERROR);

    char time_test2[] = "010000";
    ASSERT_EQ(time_parse(time_test2), 3600);
}

TEST(TimeParserTest, TestCaseStringLength) {
    //Test with string lenght
    char time_test[] = "10230";
    ASSERT_EQ(time_parse(time_test), TIME_LEN_ERROR);

    char time_test2[] = "000101";
    ASSERT_EQ(time_parse(time_test2), 61);
}

TEST(TimeParserTest, TestCaseZeroTime) {
    //Test if time is zero
    char time_test[] = "000000";
    ASSERT_EQ(time_parse(time_test), TIME_ZERO_ERROR);

    char time_test2[] = "000012";
    ASSERT_EQ(time_parse(time_test2), 12);
}

TEST(TimeParserTest, TestCaseNonNumeric) {
    //Test if has numeric values
    char time_test[] = "0000D1";
    ASSERT_EQ(time_parse(time_test), TIME_NONNUM_ERROR);

    char time_test2[] = "000030";
    ASSERT_EQ(time_parse(time_test2), 30);
}

// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
