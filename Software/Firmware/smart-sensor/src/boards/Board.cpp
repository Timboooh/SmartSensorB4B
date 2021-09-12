#include <boards/Board.h>

#include <stdlib.h>

/* Define in the main.c which board you would like to compile to. In the future
   it is possible that the software detects the version itself. */
#if defined(SMARTSENSOR_BOARD_1_2)
    #pragma message "SmartSensor v1.2 has been selected to be compiled."
    #include "boards/BoardV1_2.h"
    SmartSensorBoardV1_2 board;
#else
    #pragma message "SMARTSENSOR_BOARD_X_X has not been defined, using the newest SmartBoard version v1.2."
    #include "boards/BoardV1_2.h"
    SmartSensorBoardV1_2 board;
#endif

SmartSensorBoard* SmartSensorBoard::getBoard() {
    return &board;
}

void SmartSensorBoard::addDriver(IDriver *driver, const char* driverName) {
    if ( this->totalDrivers < SMARTSENSOR_MAX_DRIVERS) {
        uint8_t result = driver->setup();

        if ( result == 0 ) {
            this->drivers[this->totalDrivers] = driver;
            this->totalDrivers++;
        
        } else {
            this->debugf(PSTR("The driver '%s' could not be loaded: %d"), driverName, result);
        }
    
    } else {
        this->debugf(PSTR("Maximum drivers reached, cannot add '%s' to the system."), driverName);
    }
}

void SmartSensorBoard::loop() {
    uint32_t start = this->millis();
    for ( uint8_t i=0; i < this->totalDrivers; ++i ) { // Loop through all the drivers
        this->drivers[i]->loop(this->millis());
    }

    this->debugf(PSTR("Loop time %d ms"), (this->millis() - start)); // Also a measurement?
}

// http://www.cplusplus.com/reference/cstdio/vsprintf/
void SmartSensorBoard::debugf( const char* message, ...) {
    char buffer[50];
    va_list args;
    va_start(args, message);
    vsprintf (buffer, message, args);
    va_end (args);

    this->debug(buffer);
}
