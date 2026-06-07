#include "utils/TimeUtils.h"
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <cstdint>
 
namespace backtester {
namespace time {
 
class TimeUtilsTest : public ::testing::Test {};
 
// 2024-01-01T09:30:00 UTC, used by the hardening cases below.
constexpr uint64_t kJan1_0930 = 1704101400000000000ULL;
 
////////////////////////////////////////////////////////////////////////////////
// MARK: GetTimeOfDay
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, GetTimeOfDay_Midnight) {
    uint64_t midnight = 1704067200ULL * 1'000'000'000ULL;
    TimeOfDay tod = GetTimeOfDay(midnight);
    EXPECT_EQ(tod.hour, 0);
    EXPECT_EQ(tod.minute, 0);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_Noon) {
    uint64_t noon = (1704067200ULL + 12 * 3600) * 1'000'000'000ULL;
    TimeOfDay tod = GetTimeOfDay(noon);
    EXPECT_EQ(tod.hour, 12);
    EXPECT_EQ(tod.minute, 0);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_MarketOpen_930) {
    uint64_t ts = (1704067200ULL + 9 * 3600 + 30 * 60) * 1'000'000'000ULL;
    TimeOfDay tod = GetTimeOfDay(ts);
    EXPECT_EQ(tod.hour, 9);
    EXPECT_EQ(tod.minute, 30);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_LastMinuteOfDay) {
    uint64_t ts = (1704067200ULL + 23 * 3600 + 59 * 60) * 1'000'000'000ULL;
    TimeOfDay tod = GetTimeOfDay(ts);
    EXPECT_EQ(tod.hour, 23);
    EXPECT_EQ(tod.minute, 59);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_IgnoresSeconds) {
    uint64_t ts = (1704067200ULL + 9 * 3600 + 30 * 60 + 45) * 1'000'000'000ULL;
    TimeOfDay tod = GetTimeOfDay(ts);
    EXPECT_EQ(tod.hour, 9);
    EXPECT_EQ(tod.minute, 30);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_IgnoresNanoseconds) {
    uint64_t ts = (1704067200ULL + 14 * 3600 + 15 * 60) * 1'000'000'000ULL + 999'999'999;
    TimeOfDay tod = GetTimeOfDay(ts);
    EXPECT_EQ(tod.hour, 14);
    EXPECT_EQ(tod.minute, 15);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_SameTimeOnDifferentDays) {
    uint64_t day1 = (1704067200ULL + 16 * 3600 + 45 * 60) * 1'000'000'000ULL;
    uint64_t day2 = (1704067200ULL + 86400 + 16 * 3600 + 45 * 60) * 1'000'000'000ULL;
    TimeOfDay tod1 = GetTimeOfDay(day1);
    TimeOfDay tod2 = GetTimeOfDay(day2);
    EXPECT_EQ(tod1.hour, 16);
    EXPECT_EQ(tod1.minute, 45);
    EXPECT_EQ(tod1.hour, tod2.hour);
    EXPECT_EQ(tod1.minute, tod2.minute);
}
 
TEST_F(TimeUtilsTest, GetTimeOfDay_OneSecondBeforeMidnight) {
    uint64_t ts = (1704067200ULL + 23 * 3600 + 59 * 60 + 59) * 1'000'000'000ULL;
    TimeOfDay tod = GetTimeOfDay(ts);
    EXPECT_EQ(tod.hour, 23);
    EXPECT_EQ(tod.minute, 59);
}
 
////////////////////////////////////////////////////////////////////////////////
// MARK: ParseIsoToUnix — happy path / precision
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, ParseIso_BasicTimestamp_FullNanos) {
    auto result = ParseIsoToUnix("2024-01-01T00:00:00.000000000Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos, 1704067200ULL * 1'000'000'000ULL);
}
 
TEST_F(TimeUtilsTest, ParseIso_WithNanoseconds) {
    auto result = ParseIsoToUnix("2022-01-03T00:24:44.606235524Z");
    EXPECT_TRUE(result.success);
    uint64_t nanos = result.unix_nanos % 1'000'000'000ULL;
    EXPECT_EQ(nanos, 606235524);
}
 
TEST_F(TimeUtilsTest, ParseIso_NoSubseconds) {
    auto result = ParseIsoToUnix("2024-01-01T12:30:00Z");
    EXPECT_TRUE(result.success);
    uint64_t expected = (1704067200ULL + 12 * 3600 + 30 * 60) * 1'000'000'000ULL;
    EXPECT_EQ(result.unix_nanos, expected);
}
 
TEST_F(TimeUtilsTest, ParseIso_PartialNanos_OneDigit) {
    auto result = ParseIsoToUnix("2024-01-01T00:00:00.1Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos % 1'000'000'000ULL, 100'000'000);
}
 
TEST_F(TimeUtilsTest, ParseIso_PartialNanos_ThreeDigits) {
    auto result = ParseIsoToUnix("2024-01-01T00:00:00.123Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos % 1'000'000'000ULL, 123'000'000);
}
 
TEST_F(TimeUtilsTest, ParseIso_PartialNanos_SixDigits) {
    auto result = ParseIsoToUnix("2024-01-01T00:00:00.123456Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos % 1'000'000'000ULL, 123'456'000);
}
 
TEST_F(TimeUtilsTest, ParseIso_FullNineDigitNanos) {
    auto result = ParseIsoToUnix("2024-01-01T00:00:00.999999999Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos % 1'000'000'000ULL, 999'999'999);
}
 
TEST_F(TimeUtilsTest, ParseIso_ZeroNanos) {
    auto result = ParseIsoToUnix("2024-01-01T00:00:00.000000000Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos % 1'000'000'000ULL, 0);
}
 
TEST_F(TimeUtilsTest, ParseIso_EndOfDay) {
    auto result = ParseIsoToUnix("2024-01-01T23:59:59.999999999Z");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.unix_nanos % 1'000'000'000ULL, 999'999'999);
    uint64_t next_day = 1704153600ULL * 1'000'000'000ULL; // 2024-01-02
    EXPECT_EQ(result.unix_nanos, next_day - 1);
}
 
TEST_F(TimeUtilsTest, ParseIso_LeapYear) {
    auto result = ParseIsoToUnix("2024-02-29T12:00:00.000000000Z");
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.unix_nanos, 0);
}
 
TEST_F(TimeUtilsTest, ParseIso_ConsistencyWithEpochToString) {
    std::string input = "2024-06-15T14:30:45.123456789Z";
    auto result = ParseIsoToUnix(input);
    EXPECT_TRUE(result.success);
    std::string output = EpochToString(result.unix_nanos, "UTC");
    EXPECT_EQ(output.substr(0, 4), "2024");
    EXPECT_EQ(output.substr(5, 2), "06");
    EXPECT_EQ(output.substr(8, 2), "15");
    EXPECT_EQ(output.substr(11, 2), "14");
    EXPECT_EQ(output.substr(14, 2), "30");
    EXPECT_EQ(output.substr(17, 2), "45");
    EXPECT_EQ(output.substr(20, 9), "123456789");
}
 
////////////////////////////////////////////////////////////////////////////////
// MARK: ParseIsoToUnix — non-numeric fields (from_chars fails outright)
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, ParseIso_InvalidYear) {
    EXPECT_FALSE(ParseIsoToUnix("XXXX-01-01T00:00:00.000000000Z").success);
}
TEST_F(TimeUtilsTest, ParseIso_InvalidMonth) {
    EXPECT_FALSE(ParseIsoToUnix("2024-XX-01T00:00:00.000000000Z").success);
}
TEST_F(TimeUtilsTest, ParseIso_InvalidDay) {
    EXPECT_FALSE(ParseIsoToUnix("2024-01-XXT00:00:00.000000000Z").success);
}
TEST_F(TimeUtilsTest, ParseIso_InvalidHour) {
    EXPECT_FALSE(ParseIsoToUnix("2024-01-01TXX:00:00.000000000Z").success);
}
TEST_F(TimeUtilsTest, ParseIso_InvalidMinute) {
    EXPECT_FALSE(ParseIsoToUnix("2024-01-01T00:XX:00.000000000Z").success);
}
TEST_F(TimeUtilsTest, ParseIso_InvalidSecond) {
    EXPECT_FALSE(ParseIsoToUnix("2024-01-01T00:00:XX.000000000Z").success);
}
 
////////////////////////////////////////////////////////////////////////////////
// MARK: ParseIsoToUnix — hardening (length, separators, full-width,
//       timezone, trailing junk, fraction edges, numeric range)
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, ParseIso_AcceptsBareUtcWithoutZ) {
    auto r = ParseIsoToUnix("2024-01-01T09:30:00");
    ASSERT_TRUE(r.success) << r.error_msg;
    EXPECT_EQ(r.unix_nanos, kJan1_0930);
}
 
TEST_F(TimeUtilsTest, ParseIso_AcceptsSpaceSeparator) {
    auto r = ParseIsoToUnix("2024-01-01 09:30:00");
    ASSERT_TRUE(r.success) << r.error_msg;
    EXPECT_EQ(r.unix_nanos, kJan1_0930);
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsTooShort) {
    for (std::string_view s : {"", "2024", "2024-01-01", "2024-01-01T09:30"}) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
TEST_F(TimeUtilsTest, ParseIso_TooShortReportsError) {
    auto r = ParseIsoToUnix("2024-01-01");
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(r.error_msg.empty());
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsBadSeparators) {
    for (std::string_view s : {
             "2024/01/01T09:30:00",   // s[4]
             "2024-01/01T09:30:00",   // s[7]
             "2024-01-01X09:30:00",   // s[10] (not 'T' or ' ')
             "2024-01-01T09-30:00",   // s[13]
             "2024-01-01T09:30-00",   // s[16]
         }) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsPartialDigitFields) {
    for (std::string_view s : {
             "20a4-01-01T09:30:00",   // year
             "2024-0a-01T09:30:00",   // month
             "2024-01-0bT09:30:00",   // day
             "2024-01-01T0c:30:00",   // hour
             "2024-01-01T09:3d:00",   // minute
             "2024-01-01T09:30:0e",   // second
         }) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsTimezoneOffsets) {
    for (std::string_view s : {
             "2024-01-01T09:30:00+05:00",
             "2024-01-01T09:30:00-08:00",
             "2024-01-01T09:30:00.500+05:00",
         }) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsTrailingJunk) {
    for (std::string_view s : {
             "2024-01-01T09:30:00Zextra",
             "2024-01-01T09:30:00XYZ",
             "2024-01-01T09:30:00 ",
         }) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsEmptyFraction) {
    for (std::string_view s : {"2024-01-01T09:30:00.", "2024-01-01T09:30:00.Z"}) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsTooManyFractionalDigits) {
    EXPECT_FALSE(ParseIsoToUnix("2024-01-01T09:30:00.1234567890Z").success);  // 10 digits
    EXPECT_FALSE(ParseIsoToUnix("2024-01-01T09:30:00.99999999999").success);  // overflow
}
 
TEST_F(TimeUtilsTest, ParseIso_RejectsOutOfRangeFields) {
    for (std::string_view s : {
             "2024-00-01T09:30:00",   // month 0
             "2024-13-01T09:30:00",   // month 13
             "2024-01-00T09:30:00",   // day 0
             "2024-01-32T09:30:00",   // day 32
             "2024-01-01T24:30:00",   // hour 24
             "2024-01-01T09:60:00",   // minute 60
             "2024-01-01T09:30:60",   // second 60
         }) {
        EXPECT_FALSE(ParseIsoToUnix(s).success) << "input: " << s;
    }
}
 
////////////////////////////////////////////////////////////////////////////////
// MARK: EpochToString
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, EpochToString_Midnight_UTC) {
    uint64_t midnight = 1704067200ULL * 1'000'000'000ULL;
    EXPECT_EQ(EpochToString(midnight, "UTC"), "2024-01-01 00:00:00.000000000");
}
 
TEST_F(TimeUtilsTest, EpochToString_WithNanos) {
    uint64_t ts = 1704067200ULL * 1'000'000'000ULL + 123456789;
    EXPECT_EQ(EpochToString(ts, "UTC"), "2024-01-01 00:00:00.123456789");
}
 
TEST_F(TimeUtilsTest, EpochToString_EST_Offset) {
    uint64_t midnight_utc = 1704067200ULL * 1'000'000'000ULL;
    std::string result = EpochToString(midnight_utc, "EST");
    EXPECT_EQ(result.substr(11, 2), "19"); // 19:00 EST
    EXPECT_EQ(result.substr(8, 2), "31");  // Dec 31
}
 
TEST_F(TimeUtilsTest, EpochToString_CST_Offset) {
    uint64_t midnight_utc = 1704067200ULL * 1'000'000'000ULL;
    std::string result = EpochToString(midnight_utc, "CST");
    EXPECT_EQ(result.substr(11, 2), "18"); // 18:00 CST
}
 
TEST_F(TimeUtilsTest, EpochToString_JST_Offset) {
    uint64_t midnight_utc = 1704067200ULL * 1'000'000'000ULL;
    std::string result = EpochToString(midnight_utc, "JST");
    EXPECT_EQ(result.substr(11, 2), "09");
    EXPECT_EQ(result.substr(8, 2), "01"); // Still Jan 1
}
 
TEST_F(TimeUtilsTest, EpochToString_PreservesNanosPrecision) {
    uint64_t ts = 1704067200ULL * 1'000'000'000ULL + 1;
    EXPECT_EQ(EpochToString(ts, "UTC").substr(20, 9), "000000001");
}
 
////////////////////////////////////////////////////////////////////////////////
// MARK: GetTimezoneOffset
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, TimezoneOffset_UTC)          { EXPECT_EQ(GetTimezoneOffset("UTC"), 0); }
TEST_F(TimeUtilsTest, TimezoneOffset_GMT)          { EXPECT_EQ(GetTimezoneOffset("GMT"), 0); }
TEST_F(TimeUtilsTest, TimezoneOffset_EST)          { EXPECT_EQ(GetTimezoneOffset("EST"), -5 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_EST_LongForm) { EXPECT_EQ(GetTimezoneOffset("America/New_York"), -5 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_CST)          { EXPECT_EQ(GetTimezoneOffset("CST"), -6 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_CST_LongForm) { EXPECT_EQ(GetTimezoneOffset("America/Chicago"), -6 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_PST)          { EXPECT_EQ(GetTimezoneOffset("PST"), -8 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_MST)          { EXPECT_EQ(GetTimezoneOffset("MST"), -7 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_CET)          { EXPECT_EQ(GetTimezoneOffset("CET"), 1 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_JST)          { EXPECT_EQ(GetTimezoneOffset("JST"), 9 * 3600); }
TEST_F(TimeUtilsTest, TimezoneOffset_HKT)          { EXPECT_EQ(GetTimezoneOffset("HKT"), 8 * 3600); }
 
TEST_F(TimeUtilsTest, TimezoneOffset_UnknownDefaultsToZero) {
    EXPECT_EQ(GetTimezoneOffset("FAKE"), 0);
    EXPECT_EQ(GetTimezoneOffset(""), 0);
    EXPECT_EQ(GetTimezoneOffset("Moon"), 0);
}
 
////////////////////////////////////////////////////////////////////////////////
// MARK: fast_atoi helpers
////////////////////////////////////////////////////////////////////////////////
 
TEST_F(TimeUtilsTest, FastAtoi2_ValidInputs) {
    EXPECT_EQ(fast_atoi_2("00"), 0);
    EXPECT_EQ(fast_atoi_2("01"), 1);
    EXPECT_EQ(fast_atoi_2("09"), 9);
    EXPECT_EQ(fast_atoi_2("10"), 10);
    EXPECT_EQ(fast_atoi_2("42"), 42);
    EXPECT_EQ(fast_atoi_2("99"), 99);
}
 
TEST_F(TimeUtilsTest, FastAtoi4_ValidInputs) {
    EXPECT_EQ(fast_atoi_4("0000"), 0);
    EXPECT_EQ(fast_atoi_4("0001"), 1);
    EXPECT_EQ(fast_atoi_4("1234"), 1234);
    EXPECT_EQ(fast_atoi_4("2024"), 2024);
    EXPECT_EQ(fast_atoi_4("9999"), 9999);
}
 
}  
}  
