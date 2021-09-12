#pragma once
/**
 * @file       : boards/BoardV1_2.h
 * @description: This file contains the specific board v1.2.
 * @date       : 11-09-2021
 * @author     : Maurice Snoeren (MS)
 * @version    : 0.1
 * @todo       : -
 * @updates
 * 
 */
#define BOARDV1_2_ADAPTER_IN_USE_PIN PA6
#define BOARDV1_2_ADAPTER_IN_USE_DDR DDRA
#define BOARDV1_2_ADAPTER_IN_USE_PORT PORTA

#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/pgmspace.h>

#include <boards/Board.h>
#include <boards/SmartSensorMeasurement.h>
#include <util/MeasurementBuffer.h>
#include <drivers/Timer1Driver.h> // Needs to be moved and renamed?

#include <drivers/LedDriver.h>
#include <drivers/SHTC3Driver.h>
#include <drivers/MCP7940NDriver.h>

class SmartSensorBoardV1_2: public SmartSensorBoard {
private:
    MeasurementBuffer buffer;
    Timer1Driver timer1;

    LedDriver ledDriver;
    SHTC3Driver shtc3Driver;
    MCP7940NDriver mcp7940nDriver;
        
public:
    SmartSensorBoardV1_2() {}
    SmartSensorBoardV1_2(SmartSensorMeasurement* cbMeasurement): shtc3Driver(cbMeasurement) {}

    void setup();

    uint32_t millis();

    bool adapterInUse();

    void debug( const char* message);

    void addMeasurement(const char* measurement, ...);
};

