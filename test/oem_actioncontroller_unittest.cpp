#include "pid/actioncontroller.hpp"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(OemActionControllerTest, Rotarfailure)
{
    // Verifies that when fan rotor getting failed .
    // pwm value will be set to 80%
    double reading = 0;
    std::string sensorName = "SPD_FAN_SYS0_F";
    int oneRotorFailed = 1;
    int twoRotorFailed = 2;
    int threeRotorFailed = 3;
    EXPECT_EQ(80.0, processFanAction(reading, sensorName, &oneRotorFailed));
    EXPECT_EQ(80.0, processFanAction(reading, sensorName, &twoRotorFailed));
    EXPECT_EQ(80.0, processFanAction(reading, sensorName, &threeRotorFailed));
}

TEST(OemActionControllerTest, Rotarsuccess)
{
    // Verifies that when all the fan rotor is in running state
    // no oem action will be taken.
    double reading = 5300; // RPM
    std::string sensorName = "SPD_FAN_SYS0_F";
    int readFailureCnt = 0;
    EXPECT_EQ(0.0, processFanAction(reading, sensorName, &readFailureCnt));
}
