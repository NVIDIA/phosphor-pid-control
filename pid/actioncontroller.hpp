#pragma once
#include "dbus/dbushelper.hpp"
#include "util.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>

constexpr auto PROP_INTF = "org.freedesktop.DBus.Properties";
constexpr auto METHOD_GET = "Get";
constexpr auto METHOD_GET_ALL = "GetAll";
constexpr auto METHOD_SET = "Set";

/*power*/
static constexpr const char* pwrService = "xyz.openbmc_project.State.Chassis";
static constexpr const char* pwrStateObjPath =
    "/xyz/openbmc_project/state/chassis0";
static constexpr const char* pwrStateIface =
    "xyz.openbmc_project.State.Chassis";
static constexpr const char* pwrCtlOff =
    "xyz.openbmc_project.State.Chassis.Transition.Off";

/*sensor*/
constexpr const char* warningInterface =
    "xyz.openbmc_project.Sensor.Threshold.Warning";
constexpr const char* criticalInterface =
    "xyz.openbmc_project.Sensor.Threshold.Critical";
constexpr auto sensorObjectPath = "/xyz/openbmc_project/sensors/temperature/";

constexpr auto pwmSetpoint = 80;
constexpr auto maxI2cFanFailure = 8;
constexpr auto maxPsuFanFailure = 6;
static const std::string psuFan = "SPD_FAN_PSU";
static const std::string i2cFan = "SPD_FAN_SYS";

using property = std::string;
using sensorValue = std::variant<int64_t, double, std::string, bool>;
using propertyMap = std::map<property, sensorValue>;

double processFanAction(double, std::string, int*);
double processThermalAction(std::string);


using value = std::variant<uint8_t, uint16_t, std::string>;
using namespace phosphor::logging;
using namespace sdbusplus;


inline bool getPowerStatus(std::shared_ptr<sdbusplus::asio::connection> conn)
{
    bool pwrGood = false;
    std::string pwrStatus;
    value variant;
    try
    {
        auto method = conn->new_method_call(pwrService, pwrStateObjPath,
                                            PROP_INTF, METHOD_GET);
        method.append(pwrStateIface, "CurrentPowerState");
        auto reply = conn->call(method);
        reply.read(variant);
        pwrStatus = std::get<std::string>(variant);
    }
    catch (sdbusplus::exception_t& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Failed to get getPowerStatus Value",
            phosphor::logging::entry("EXCEPTION=%s", e.what()));
        return pwrGood;
    }
    if (pwrStatus == "xyz.openbmc_project.State.Chassis.PowerState.On")
    {
        pwrGood = true;
    }
    return pwrGood;
}

inline void poweroff(std::shared_ptr<sdbusplus::asio::connection> conn)
{
    std::cerr << " poweroff \n";
    auto method = conn->new_method_call(pwrService, pwrStateObjPath, PROP_INTF,
                                        METHOD_SET);
    method.append(pwrStateIface, "RequestedPowerTransition");
    method.append(std::variant<std::string>(pwrCtlOff));

    auto reply = conn->call(method);

    if (reply.is_method_error())
    {
        std::cerr << "Failed to set RequestedPowerTransition poweroff \n";
    }
}

inline std::string getService(const std::string& intf, const std::string& path)
{
    auto bus = bus::new_default_system();
    auto mapper =
        bus.new_method_call("xyz.openbmc_project.ObjectMapper",
                            "/xyz/openbmc_project/object_mapper",
                            "xyz.openbmc_project.ObjectMapper", "GetObject");

    mapper.append(path);
    mapper.append(std::vector<std::string>({intf}));

    std::map<std::string, std::vector<std::string>> response;

    try
    {
        auto responseMsg = bus.call(mapper);

        responseMsg.read(response);
    }
    catch (const sdbusplus::exception::exception& ex)
    {
        log<level::ERR>("ObjectMapper call failure",
                        entry("WHAT=%s", ex.what()));
        throw;
    }

    if (response.begin() == response.end())
    {
        throw std::runtime_error("Unable to find Object: " + path);
    }

    return response.begin()->first;
}

inline double monitorThreshold(std::string sensorName)
{
    std::string objectPath = sensorObjectPath;
    double setpoint = 0;
    boost::asio::io_context io;
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    objectPath = objectPath + sensorName;
    std::string service;

    // warning interface
    try
    {
        service = getService(warningInterface, objectPath.c_str());
        propertyMap warningMap;
        auto method = conn->new_method_call(service.c_str(), objectPath.c_str(),
                                            PROP_INTF, METHOD_GET_ALL);
        method.append(warningInterface);
        auto reply = conn->call(method);
        if (reply.is_method_error())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to get all properties");
        }
        reply.read(warningMap);
        auto findWarningHigh = warningMap.find("WarningAlarmHigh");
        if (findWarningHigh != warningMap.end())
        {
            if (std::get<bool>(warningMap.at("WarningAlarmHigh")))
            {
                setpoint = pwmSetpoint;
            }
        }
    }
    catch (sdbusplus::exception_t& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Failed to fetch",
            phosphor::logging::entry("EXCEPTION=%s", e.what()));
    }

    // critical interface
    try
    {
        service = getService(criticalInterface, objectPath.c_str());
        propertyMap criticalMap;
        auto method = conn->new_method_call(service.c_str(), objectPath.c_str(),
                                            PROP_INTF, METHOD_GET_ALL);
        method.append(criticalInterface);
        auto reply = conn->call(method);
        if (reply.is_method_error())
        {
            phosphor::logging::log<phosphor::logging::level::ERR>(
                "Failed to get all properties");
        }
        reply.read(criticalMap);
        auto findCriticalHigh = criticalMap.find("CriticalAlarmHigh");

        if (findCriticalHigh != criticalMap.end())
        {
            if (std::get<bool>(criticalMap.at("CriticalAlarmHigh")))
            {
                if (getPowerStatus(conn))
                {
                    poweroff(conn);
                }
            }
        }
    }
    catch (sdbusplus::exception_t& e)
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Failed to fetch",
            phosphor::logging::entry("EXCEPTION=%s", e.what()));
    }
    return setpoint;
}
