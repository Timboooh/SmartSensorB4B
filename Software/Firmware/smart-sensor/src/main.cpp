/**
 * Entry point of the application
 * @date 12 May 2020
 * @author Patrick de Jong, Paul Hobbel, Sergen Peker, Carlos Cadel, Floris Bob van Elzelingen, Maurice Snoeren
 */

#ifndef UNIT_TEST
#include <boardsupport.h>
#include <board-support/drivers/TWIDriver.h>
#include <board-support/util/PinManager.h>
#include <board-support/util/SerialLogger.h>

#include <board-support/drivers/UsartDriver.h>
#include <board-support/drivers/TWIDriver2.h>
#include "drivers/THSDriver2.h" // HELP: Why driver 2?
#include "drivers/ALSDriver.h"
#include "drivers/WemosDriver.h"
#include "drivers/RTCDriver.h"
#include "drivers/CO2Driver.h"
#include "drivers/TPMDriver.h"
#include <stdio.h>

#define FIRMWARE_VERSION "v0.9"

// Hard coded coordinator variable to select the required functionality
// TODO: The software needs to detect this automatically
volatile bool is_coordinator = false;

// The unique sensor ID that should be taken from the TPM chip
// TODO: The software needs to get a unique ID based on the hardware automatically
optional<int16_t> sensor_id{1};

// HELP: I do not know what this is?!
volatile char bitbuffer = 0;

volatile unsigned char usart1_buffer[256];
volatile uint8_t usart1_buffer_read_index = 0;
volatile uint8_t usart1_buffer_write_index = 0;

// TODO: This is a cyclic buffer that should be implemented by a class?!
void usart1_buffer_write(unsigned char c) {
    usart1_buffer[usart1_buffer_write_index] = c;
    usart1_buffer_write_index = usart1_buffer_write_index == 0xFF ? 0 : usart1_buffer_write_index + 1;
}

// TODO: This is a cyclic buffer that should be implemented by a class?!
bool usart1_buffer_has_data() {
    return usart1_buffer_read_index != usart1_buffer_write_index;
}

// TODO: This is a cyclic buffer that should be implemented by a class?!
unsigned usart1_buffer_read() {
    const auto res = usart1_buffer[usart1_buffer_read_index];
    usart1_buffer_read_index = usart1_buffer_read_index == 0xFF ? 0 : usart1_buffer_read_index + 1;
    return res;
}

// INTERRUPT HANDLING: Recieved byte via the UART1
ISR(USART1_RX_vect) {
    // HELP: What is happening here? Does it belong to the coordinator?
    //if (is_coordinator) UDR2 = UDR1;
    //else usart1_buffer_write(UDR1);
    usart1_buffer_write(UDR1);
}

// INTERRUPT HANDLING: Recieved byte via the UART2
ISR(USART2_RX_vect) {
    // HELP: What is happening here?
    UDR1 = UDR2; // HELP: Is this correct??
}

// HELP: What is happening here?
// HELP: this belongs to the XBee driver? And in fact to the communication class!
char xbee_buffer[512];
uint16_t xbee_buffer_index = 0;

void xbee_buffer_reset() {
    xbee_buffer_index = 0;
}


// HELP: Explain?!
void send_to_xbee(const char* msg, uint16_t msg_length) {

    auto length = msg_length + 14;

    uint64_t checksum = 0;
    auto send_byte = [&](uint8_t byte) { checksum += byte; Usart1.transmitChar(byte); };

    Usart1.transmitChar(0x7E);
    Usart1.transmitChar(length >> 8);
    Usart1.transmitChar(length & 0xFF);
    send_byte(0x10);
    send_byte(1);
    send_byte(0);
    send_byte(0);
    send_byte(0);
    send_byte(0);
    send_byte(0);
    send_byte(0);
    send_byte(0);
    send_byte(0);
    send_byte(0xFF);
    send_byte(0xFE);
    send_byte(0);
    send_byte(0);
    
    for (uint16_t i = 0; i < msg_length; ++i) {
        checksum += *msg;
        Usart1.transmitChar(*msg++);
    }
    Usart1.transmitChar(0xFF - (checksum & 0xFF));
}

// Method to print a 1 or 0 depending on the boolean value to reduce the memory footprint!
void serial_print_boolean(bool value) {
    if (value) SerialLogger0.print("1\n"); else SerialLogger0.print("0\n");
}

// HELP: Explain?!
uint8_t read_from_xbee(char *buffer) {
    //while (true) UDR0 = Usart1.receiveChar();

    auto debug_byte = []{auto val = Usart1.receiveChar(); /*UDR0 = val;*/ return val; };

    while (debug_byte() != 0x7E); // HELP: POTENTIAL SECURITY ISSUE!!
    //SerialLogger0.print("in frame\n");    

    uint8_t h = debug_byte();
    uint8_t l = debug_byte();

    uint16_t length = (((uint16_t)h) << 8) | ((uint16_t)l);

    //_delay_ms(2);

    //SerialLogger0.printf("length := %d (0x%x 0x%x)\n", (int)length, (uint16_t)h, (uint16_t)l);

    uint16_t msg_bytes_read = 0;
    auto read_byte = [&]{++msg_bytes_read; return debug_byte(); };
    
    uint8_t frame_type = 0;
    if (length) frame_type = read_byte();
    if (frame_type == 0xFF) {
        for (uint16_t i = 0; i < 17; ++i) read_byte();
        for (uint16_t i = 0; msg_bytes_read < length; ++i)
            buffer[i] = read_byte();
        buffer[length - 17] = '\0';
    } else {
        for (uint16_t i = 1; i < length; ++i) read_byte();
    }
    uint8_t checksum = debug_byte();
    return frame_type;
}

int main() {
    /* Print the versions to the serial */
	SerialLogger0.print("SmartSensor ");
    SerialLogger0.print(BOARD_VERSION);
    SerialLogger0.print(", Firmware ");
    SerialLogger0.print(FIRMWARE_VERSION);
    SerialLogger0.print("\n");

    /* Setup the hardware */
    TWI2_0.setBitrate(TWI0_SCL_CLOCK);
    TWI2_1.setBitrate(TWI1_SCL_CLOCK);

    PinManager::set_mode(CO2_INT_PIN, INPUT);
    //PinManager::set_mode(CO2_ENABLE_PIN, OUTPUT); // NOT USED ANYMORE
    PinManager::set_mode(CO2_WAKE_PIN, OUTPUT);

    //PinManager::set_mode(LS_ENABLE_PIN, OUTPUT); // NOT USED ANYMORE
    //PinManager::set_mode(THS_ENABLE_PIN, OUTPUT); // NOT USED ANYMORE

    // HELP: What is this?
    PinManager::set_mode(ATM_MOSI_PIN, OUTPUT);
    PinManager::digital_write(ATM_MOSI_PIN, HIGH);
    PinManager::set_mode(ATM_MISO_PIN, OUTPUT);
    PinManager::digital_write(ATM_MISO_PIN, HIGH);
      
    //PinManager::set_mode(XBEE_ENABLE_PIN, OUTPUT); // NOT USED ANYMORE
    //PinManager::digital_write(XBEE_ENABLE_PIN, LOW); // NOT USED ANYMORE
    // HELP: What is this? It looks like communication with XBee_D18?! 
    PinManager::set_mode(PinPortA0, OUTPUT);
    PinManager::digital_write(PinPortA0, LOW);

    PinManager::set_mode(XBEE_SLEEP_PIN, INPUT); // HELP: INPUT? What is this then?
    //PinManager::set_mode(XBEE_SS_PIN, OUTPUT); // Why not?

    PinManager::set_mode(STATUS_LED_1_PIN, OUTPUT);

    //_delay_ms(1000);
    //PinManager::digital_write(CO2_ENABLE_PIN, HIGH); // NOT USED ANYMORE
    PinManager::digital_write(CO2_WAKE_PIN, HIGH);

    //PinManager::digital_write(LS_ENABLE_PIN, HIGH); // NOT USED ANYMORE
    //PinManager::digital_write(THS_ENABLE_PIN, LOW); // NOT USED ANYMORE

    /* Blink led for one second and a flash to show it has been started. */
    PinManager::digital_write(STATUS_LED_1_PIN, LOW);
    _delay_ms(1000);
    PinManager::digital_write(STATUS_LED_1_PIN, HIGH);
    _delay_ms(1000);
    PinManager::digital_write(STATUS_LED_1_PIN, LOW);
    _delay_ms(100);
    PinManager::digital_write(STATUS_LED_1_PIN, HIGH);
    _delay_ms(1000);

    // HELP: What is this?
    // TODO: Logic to detect wheter the software required to be a coordinator.
    if (is_coordinator) { // send xbee to wemos (HELP: What do you mean?)
        UCSR1B = 0b1001'1000;
        UCSR2B = 0b1001'1000;
        while (true); //HELP: ??
    } else {
        // HELP: What is this?
        //UCSR1B = 0b1001'1000;
    }

    // HELP: What is this?
    /*PRR0 = 0b1000'0000;
    PRR1 = 0;
    PRR2 = 0;*/
    
    //PinManager::digital_write(XBEE_SLEEP_PIN, LOW);
    //PinManager::digital_write(XBEE_SS_PIN, HIGH);
    /* Is not used any more!
    _delay_ms(1);
    PinManager::digital_write(XBEE_ENABLE_PIN, LOW);
    _delay_ms(1);
    */

    //PinManager::digital_write(CO2_WAKE_PIN, HIGH); // HELP: ALREADY SET HIGH!

    //PinManager::digital_write(XBEE_SLEEP_PIN, LOW);

    //_delay_ms(1000); // HELP: Already waited to blink the led!

    //SerialLogger0.print("Initialized\n");
    //_delay_ms(10);

    /* Enable the interrupts */
    sei();
    _delay_ms(10);

    /* Perform hardware check! */
    SerialLogger0.print("RTC:");
    serial_print_boolean(RTC.isConnected());
    _delay_ms(1);

    SerialLogger0.print("CLOCK:");
    serial_print_boolean(RTC.startClock());
    _delay_ms(1);

    SerialLogger0.print("THS:");
    serial_print_boolean(THS.isConnected());
    _delay_ms(1);

    SerialLogger0.print("ALS:");
    serial_print_boolean(ALS.isConnected());
    ALS.setupSensor();
    _delay_ms(1);

    // HELP: What is this? Why not created a lib for this like the rest TPMDriver?! Lazy?
    SerialLogger0.print("TPM:");
    serial_print_boolean(TPM.isConnected());
    TPM.setup();
    _delay_ms(1);

    SerialLogger0.print("CO2:");
    serial_print_boolean(CO2.isConnected());
    CO2.setupSensor();
    _delay_ms(1);

    // HELP: Below debug code?!
    //TWI2_0.start().wait().get();
    
    /*while (true) {
        //if (UCSR2A & 0b1000'0000) bitbuffer = UDR2;

        //Usart1.transmitChar(bitbuffer);
        xbee_buffer_reset();
        //xbee_buffer_append("hey!?!");
        xbee_buffer_index = sprintf(xbee_buffer, "test := %f", (double)123.456f);
        send_to_xbee(xbee_buffer, xbee_buffer_index);
        //Usart1.transmitChar(0xAB);
        //Usart1.transmitChar('a');
        _delay_ms(2000);
    }*/

    /*while (true) {

        const auto time = *RTC.getTime();
        SerialLogger0.printf("time := %s\n", generate_ISO_8601_string(time));

        const auto temp_hum = *THS.takeMeasurement();
        SerialLogger0.printf("temperature := %fc, humidity %f%%\n", temp_hum.temperature, temp_hum.humidity);

        const auto lx = *ALS.takeMeasurement();
        SerialLogger0.printf("light intensity := %flx\n", lx);

        CO2.takeMeasurement();

        _delay_ms(2000);
        SerialLogger0.print("\n");
    }*/

    // HELP: ?!
    bool flip = false;

    // HELP: ?!
    uint8_t data_to_write = 1;

    /*while (!sensor_id) {
        SerialLogger1.printf("{ \"")
    }*/

    // HELP: does it belong to the XBee driver?!
    xbee_buffer_reset();

    //_delay_ms(2000); // MS: A lot of delay!
    /*PinManager::digital_write(PinPortA0, HIGH);
    //while (true) Usart0.transmitChar(Usart1.receiveChar());
    while (true) {
        while (!usart1_buffer_has_data());
        Usart0.transmitChar(usart1_buffer_read());
    }

    while (true) {
        SerialLogger0.print("waiting for xbee frame...");
        const uint8_t frame_type = read_from_xbee(xbee_buffer);
        SerialLogger0.print("recieved\n");
        SerialLogger0.printf("xbee_frame(%x)\n", frame_type);
        if (frame_type == 0x90) SerialLogger0.printf("%s\n", xbee_buffer);
    }

    /*for (int i = 0; i < 100; ++i) {
        xbee_buffer_index = sprintf(xbee_buffer, "hey %i", i);
        send_to_xbee(xbee_buffer, xbee_buffer_index);
        _delay_ms(100);
    }*/

    // HELP: I do not understand this?!
    if (is_coordinator) while(true); 

   // SerialLogger0.print("sending init to coordinator...");
   // MS: This should have been implemented in a method!
    xbee_buffer_index = sprintf(xbee_buffer, "!SmartSensor-%d"
    , *sensor_id);
    send_to_xbee(xbee_buffer, xbee_buffer_index);
    //SerialLogger0.print("done\n");   

    // MS: No state machine has been implemented.
    // MS: No generic method of measuring has been implemented?!
    // MS: A very simple approach have been implemented!
    // MS: I do not see any initialization of the XBee and also not the status of the XBee?!
    // MS: The XBee driver does not implement anything around the configuration of the XBee?!
    // MS: The coordinator does not measure things?!
    // MS: No configuration like sampling frequency as discussed implemented. (Less far?!?!)
    // MS: System does not check nor wait for the XBee?!
    while (true) {
        _delay_ms(2000);

        // HELP: Only THS?
        if (auto res = THS.takeMeasurement(); res && sensor_id) {
            xbee_buffer_reset();
            xbee_buffer_index = sprintf(xbee_buffer,
R"!(
{
    "id": "SmartSensor-%d",
    "measurements": [{
        "timestamp": "%s",
        "temperature": %f,
        "humidity": %f
    }]
}
)!"
            , *sensor_id , generate_ISO_8601_string(*RTC.getTime()), (double)(*res).temperature, (double)(*res).humidity);
            send_to_xbee(xbee_buffer, xbee_buffer_index);
        }
        

        //SerialLogger0.print(xbee_buffer);
        //SerialLogger0.print("\n");

        //ALS.takeMeasurement();
        // MS: This should have been implemented by a state machine, so we also have an idea that everything is correctly handled.
        PinManager::digital_write(STATUS_LED_1_PIN, (flip = !flip) ? HIGH : LOW);

        //assert(flip);

        //SerialLogger0.print("past assert\n");

       // Wemos.isConnected() ? SerialLogger0.print("ack from wemos\n") : SerialLogger0.print("nack from wemos\n");

        //Usart1.transmitChar('a');
        //Usart1.transmitChar('\n');
    }

    /*while (true) {
        /*for (char c = 'a'; c <= 'z'; ++c) {
            //Usart1.transmitChar('a');
            //Usart1.transmitChar('\n');
            //Usart1.flush();
            _delay_ms(10);
        }* /
        //_delay_ms(10);
        Usart1.transmitChar('a');
        Usart0.transmitChar('a');
        Usart1.flush();
        Usart0.flush();
        _delay_ms(100);
    }*/
    
    /*while (true)
    {
        SerialLogger0.print("\nLS_ENABLE_PIN := HIGH\n");
        PinManager::digital_write(LS_ENABLE_PIN, HIGH);
        PinManager::digital_write(STATUS_LED_1_PIN, HIGH);
        _delay_ms(100);
        if (twi0_start()) if (twi0_write_adress(TWI_ALS_ADDRESS, 0)) TWI2_0.stop(), twi0_disable(), SerialLogger0.print("able to select ALS\n"); else SerialLogger0.print("unable to select ALS\n");
        _delay_ms(4900);

        SerialLogger0.print("\nLS_ENABLE_PIN := LOW\n");
        PinManager::digital_write(LS_ENABLE_PIN, LOW);
        PinManager::digital_write(STATUS_LED_1_PIN, LOW);
        _delay_ms(100);
        if (twi0_start()) if (twi0_write_adress(TWI_ALS_ADDRESS, 0)) TWI2_0.stop(), twi0_disable(), SerialLogger0.print("able to select ALS\n"); else SerialLogger0.print("unable to select ALS\n");
        _delay_ms(4900);
    }*/

    /*
    for(;;) {

        {
            SerialLogger0.print("\npre transaction\n");

            //Start TWI, and start a transaction, and check succes
            if (!TWI2_0.start(TWIMode::MasterTransmitter).wait().get()) goto delay;

            //Transmit adress & mode
            if (!TWI2_0.select(TWI_RTC_ADDRESS).wait().get()) goto delay;

            //Transmit Register adress
            if (!TWI2_0.write(0x20).wait().get()) goto stop;

            if (flip) {

                //Transmit data
                if (!TWI2_0.write(data_to_write).wait().get()) goto stop;

                if (data_to_write == 0xFF) data_to_write = 0;
                else data_to_write += 1;

            } else {

                //Start TWI, and start a transaction, and check succes
                if (!TWI2_0.repeated_start(TWIMode::MasterReciever).wait().get()) goto delay;

                //Transmit adress & mode
                if (!TWI2_0.select(TWI_RTC_ADDRESS).wait().get()) goto delay;

                auto res = TWI2_0.read_nack().wait().get();
                if (!res) goto stop;

                SerialLogger0.printf("data := %u\n", *res);
            }

            stop:
            //Stop transaction 
            TWI2_0.stop();

            //Stop TWI
            TWI2_0.disable();
            SerialLogger0.print("transaction & TWI stopped\n");

            delay:

            SerialLogger0.print("post transaction\n");
        }
        _delay_ms(500);
        flip = !flip;
    }*/
}

#endif