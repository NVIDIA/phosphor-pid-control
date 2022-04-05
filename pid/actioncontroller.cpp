#include "actioncontroller.hpp"

#include <string.h>
#include <iostream>

/** @brief Process Fan defined action
 *  @param[in] reading  sensor reading
 *  @param[in] sensorName sensor name
 *  @param[in] readFailureCnt  count number of fan having reading 0
 *  @return The setpoint to set pwm value
 */

double processFanAction(double reading, std::string sensorName,
                        int* readFailureCnt)
{
    double setpoint = 0;

    if (!(reading > 0))
    {
        setpoint = pwmSetpoint;
        (*readFailureCnt)++;
    }

    if (((*readFailureCnt) == maxPsuFanFailure &&
         !strncmp(sensorName.c_str(), psuFan.c_str(),
                  strlen(psuFan.c_str()))) ||
        ((*readFailureCnt) == maxI2cFanFailure &&
         !strncmp(sensorName.c_str(), i2cFan.c_str(), strlen(i2cFan.c_str()))))
    {
        boost::asio::io_context io;
        auto conn = std::make_shared<sdbusplus::asio::connection>(io);
        if (getPowerStatus(conn))
        {
            poweroff(conn);
        }
        }

    return setpoint;
}

/** @brief Process Temp sensor defined action
 *  @param[in] sensorName sensor name
 *  @return The setpoint to set pwm value
 */

double processThermalAction(std::string sensorName)
{
    double setpoint = 0;
    setpoint = monitorThreshold(sensorName);
    return setpoint;
}
