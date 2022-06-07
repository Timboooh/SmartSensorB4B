#include <drivers/XBeeProS2C.h>

#include <stdlib.h>

#include <util/delay.h>
#include <avr/pgmspace.h>
#include <tasks/Atmega324PBSerial1.h>
#include <tasks/Atmega324PBSerial0.h>

#include <boards/Board.h>

/**
 * @brief Get the Size of the input string
 *
 * @param s the input string
 * @return size_t the size of the string
 */
size_t getSize(const char *s);

uint8_t XBeeProS2C::setup()
{
    XBEEPROS2C_SLEEP_DDR = XBEEPROS2C_SLEEP_DDR | (1 << XBEEPROS2C_SLEEP_PIN); // Set the pin as output to drive the sleep pin
    this->wakeup();

    Atmega324PBSerial1 *x = Atmega324PBSerial1::getInstance();

    x->setCallback(this); // Recieve all characters on serial 1 to me.

    return 0;
}

#include <avr/wdt.h>
uint8_t XBeeProS2C::loop(uint32_t millis)
{
    // TEST
    // 7E 00 0D 90 00 7D 33 A2 00 41 8F 98 08 71 3B 01 31 6C  => correct API 2
    // 7E 00 0D 90 00 13 A2 00 41 8F 98 08 71 3B 01 31 6C => correct API 1
    //   00 0D 90 00 13 A2 00 41 8F 98 08 71 3B 01 31 6C => recieved API 1 ( first time rest not correct.)
    //         90 00 13 A2 00 41 8F 98 08 71 3B 01 31
    /*SmartSensorBoard::getBoard()->debug("TEST\n");
    char frame[100];
    uint8_t counter = 0;
    uint8_t problems = 0;
    char c;

    while ( true ) {
        wdt_reset();

        while ( !(UCSR1A & (1<<RXC)) ) { wdt_reset(); }; // Is a character available?
        c = UDR1;

        if ( c == 0x7E ) {
            if ( counter != 0 ) { // frame recieved!
                if ( counter > 10 ) {
                    SmartSensorBoard::getBoard()->debugf("Frame recieved: %d (%d)!\n", counter, problems);
                    for (uint8_t i=0; i < counter; i++ ) {
                        uint8_t h2 = (frame[i] & 0b11110000) >>4;
                        uint8_t h1 = frame[i] & 0b00001111;
                        Atmega324PBSerial0::getInstance()->transmitChar((h2 < 10 ? h2+'0' : h2-10+'A'));
                        Atmega324PBSerial0::getInstance()->transmitChar((h1 < 10 ? h1+'0' : h1-10+'A'));
                        Atmega324PBSerial0::getInstance()->transmitChar(' ');

                    }
                    Atmega324PBSerial0::getInstance()->transmitChar('\n');
                } else {
                    problems++;
                }
            }
            counter = 0;

        } else {
            frame[counter] = c;
            counter++;
        }
    }*/

    Atmega324PBSerial1 *x = Atmega324PBSerial1::getInstance();

    // TODO: Check timeout when it is not in a waiting state!

    // char m[5];
    // sprintf_P(m,PSTR("%d\n"), this->state);
    // SmartSensorBoard::getBoard()->debug(m);

    switch (this->state)
    {
    case 0: // Start to write and read configuration of the XBee
        this->atStart();
        this->state++;
        this->timestamp = millis;
        break;

    case 1:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debug_P(PSTR("XBeeProS2C: processing\n"));
            if (this->checkResultOk())
            {
                SmartSensorBoard::getBoard()->debug_P(PSTR("XBeeProS2C: Found\n"));
                this->state++;
            }
            else
            {
                this->state = 0; // Start over?!
            }
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
        }
        else
        { // Not processing yet!
            if (millis - this->timestamp > XBEEPROS2C_TIMEOUT_TIME_S)
            {
                SmartSensorBoard::getBoard()->debug_P(PSTR("XBeeProS2C: Not found\n"));
                this->state = XBEEPROS2C_STATE_NOTFOUND; // Misschien een aantal keer proberen ipv één keer?
            }
        }
        break;

    case 2:
        this->atSetPanId(XBEEPROS2C_PAN_ID);
        this->state++;
        break;

    case 4:
        this->atSetCoordinator(this->isCoordinator);
        this->state++;
        break;

    case 6:
        this->atSetNodeIdentifier(SmartSensorBoard::getBoard()->getID());
        this->state++;
        break;

    case 8:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATAP 1\r")); // 0 Transparant, 1 API, 2 API with escapes
        this->state++;
        break;

    case 10:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATAO 0\r"));
        this->state++;
        break;

    case 12:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATDH 0\r"));
        this->state++;
        break;

    case 14:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATDL 0\r"));
        this->state++;
        break;

    case 16:
        this->atWrite();
        this->state++;
        break;

    case 3:
    case 5:
    case 7:
    case 9:
    case 11:
    case 13:
    case 15:
    case 17:
    case 33:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            if (this->checkResultOk())
            {
                this->state++;
            }
            // else
            // {
            //     this->state = 0; // start over? TODO
            // }
            // this->stateReciever = XBeeProS2CStateReciever::IDLE;
        }
        break;

    case 18: // Get PAN ID
        this->atGetPanId();
        this->state++;
        break;

    case 19:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C PAN ID: %s\n"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 20:
        this->atGetSerialNumberHigh();
        this->state++;
        break;

    case 21:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C SERIAL: %s"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 22:
        this->atGetSerialNumberLow();
        this->state++;
        break;

    case 23:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("%s\n"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 24:
        this->atGetCoordinatorEnable();
        this->state++;
        break;

    case 25:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C Coordinator: %s\n"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 26:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATNI\r"));
        this->state++;
        break;

    case 27:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C Node Identifier: %s\n"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 28:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATVR\r"));
        this->state++;
        break;

    case 29:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C Firmware Version: %s\n"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 30:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATHV\r"));
        this->state++;
        break;

    case 31:
        if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
        {
            SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C Hardware Version: %s\n"), this->recieveBuffer);
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
            this->state++;
        }
        break;

    case 32:
        Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATCN\r")); // Exit command mode
        this->state++;
        this->timestamp = millis;
        break;

    case 34:
        this->state = XBEEPROS2C_STATE_RUNNING;
        break;

    case XBEEPROS2C_STATE_RUNNING: // Waiting on recieve a message from XBee or sending data to the coordinator
        if (!this->isCoordinator)
        {
            if (!Atmega324PBSerial1::getInstance()->isBusy() && millis - this->timestamp > XBEEPROS2C_SEND_DELAY)
            {

                this->timestamp = millis;

                if (this->queueCounter > 0)
                {
                    SmartSensorBoard::getBoard()->debugf_P(PSTR("XBee size: %d\n"), this->queueCounter);
                    while (this->queueCounter > 0)
                    {
                        char *msg;
                        this->popQueueMessage(&msg);
                        SmartSensorBoard::getBoard()->debugf_P(PSTR("%d\n"), this->queueCounter);
                        SmartSensorBoard::getBoard()->debugf_P(PSTR("got msg: %s\n"), msg);
                        this->sendMessageToCoordinator(msg);
                    }
                }
                this->stateReciever = XBeeProS2CStateReciever::IDLE;
            }
            else
            {
                if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING)
                {
                    // SmartSensorBoard::getBoard()->debugf_P(PSTR("XBeeProS2C Message: %s\n"), this->recieveBuffer);
                    this->stateReciever = XBeeProS2CStateReciever::IDLE;
                }

                if (this->stateReciever == XBeeProS2CStateReciever::PROCESSING_API)
                {
                    /*                char m[50] = "_GMT:                    :";
                                    if ( (uint8_t) this->recieveBuffer[0] == 0x90 ) { // Recieve packet, check the checksum!!
                                        for (uint8_t i=0; i < 10; i++ ) { // Get address 64+16 bit
                                            uint8_t h2 = (this->recieveBuffer[i+1] & 0b11110000) >>4;
                                            uint8_t h1 = this->recieveBuffer[i+1] & 0b00001111;
                                            m[5+i*2] = (h2 < 10 ? h2+'0' : h2-10+'A');
                                            m[6+i*2] = (h1 < 10 ? h1+'0' : h1-10+'A');
                                        }

                                        for (uint8_t i=12; i < this->apiLength; i++ ) { // Get the Message
                                            m[26+i-12] = this->recieveBuffer[i];
                                        }
                                        m[26+this->apiLength-12] = '\n';

                                        if ( Atmega324PBSerial0::getInstance()->isBusy() ) {
                                            Atmega324PBSerial0::getInstance()->print("HELP: You should create a buffer here!!");
                                        }
                                        Atmega324PBSerial0::getInstance()->printAsync(m);
                                        this->sendToNode(); // test

                                    } else { // Print the packet
                    */
                    for (uint8_t i = 0; i <= this->apiLength; i++)
                    {
                        uint8_t h2 = (this->recieveBuffer[i] & 0b11110000) >> 4;
                        uint8_t h1 = this->recieveBuffer[i] & 0b00001111;
                        Atmega324PBSerial0::getInstance()->transmitChar((h2 < 10 ? h2 + '0' : h2 - 10 + 'A'));
                        Atmega324PBSerial0::getInstance()->transmitChar((h1 < 10 ? h1 + '0' : h1 - 10 + 'A'));
                        Atmega324PBSerial0::getInstance()->transmitChar(' ');
                    }
                    Atmega324PBSerial0::getInstance()->transmitChar('\n');
                    //               }

                    this->stateReciever = XBeeProS2CStateReciever::IDLE;
                }
            }
            break;
            // case waiting 1->recieved message from XBee as coordinator (relay to gateway), 2->sending message to coordinator
        }
    }

    return 0;
}

uint8_t XBeeProS2C::reset()
{
    return 0;
}

uint8_t XBeeProS2C::sleep()
{
    XBEEPROS2C_SLEEP_PORT = XBEEPROS2C_SLEEP_PORT | (1 << XBEEPROS2C_SLEEP_PIN); // Sleep
    return 0;
}

uint8_t XBeeProS2C::wakeup()
{
    XBEEPROS2C_SLEEP_PORT = XBEEPROS2C_SLEEP_PORT & ~(1 << XBEEPROS2C_SLEEP_PIN); // Wake
    return 0;
}

void XBeeProS2C::recievedCharacter(char c)
{
    // Atmega324PBSerial0::getInstance()->transmitChar(c);
    //  When we are not in state running, we are in command mode!
    if (this->state == XBEEPROS2C_STATE_RUNNING && c == 0x7E)
    { // API start byte detected, start immediatly with recieving!
        this->stateReciever = XBeeProS2CStateReciever::IDLE;
    }

    switch (this->stateReciever)
    {
    case XBeeProS2CStateReciever::IDLE:
        if (c != '\r')
        {
            if (this->state == XBEEPROS2C_STATE_RUNNING && c == 0x7E)
            {
                this->recieveBufferPointer = 0;
                this->stateReciever = XBeeProS2CStateReciever::BUSY_API_LENGTH_H;
            }
            else
            {
                this->recieveBuffer[0] = c;
                this->recieveBufferPointer = 1;
                this->stateReciever = XBeeProS2CStateReciever::BUSY;
            }
        }
        break;

    case XBeeProS2CStateReciever::BUSY:
        this->recieveBuffer[this->recieveBufferPointer] = c;

        if (this->recieveBufferPointer > XBEEPROS2C_RECIEVE_BUFFER_AMOUNT - 1)
        { // Error, beyond the buffer!
            SmartSensorBoard::getBoard()->debug_P(PSTR("XBeeProS2C: Buffer overflow!\n"));
            this->stateReciever = XBeeProS2CStateReciever::IDLE;
        }

        if (c == '\r')
        {                                                           // Ready!
            this->recieveBuffer[this->recieveBufferPointer] = '\0'; // replace \r with \0 end character
            this->stateReciever = XBeeProS2CStateReciever::PROCESSING;
            // SmartSensorBoard::getBoard()->debugf("MESSAGE: %s\n", this->recieveBuffer);
        }

        this->recieveBufferPointer++;
        break;

    case XBeeProS2CStateReciever::PROCESSING:     // Help, the previous data has not been processed yet!
    case XBeeProS2CStateReciever::PROCESSING_API: // Help, the previous data has not been processed yet!
    {
        Atmega324PBSerial0 *s = Atmega324PBSerial0::getInstance();
        SmartSensorBoard::getBoard()->debugf_P(PSTR("XBEE Problem: %d\n"), c);
        // TODO: Startover?
        this->stateReciever = XBeeProS2CStateReciever::IDLE;
    }
    break;

    case XBeeProS2CStateReciever::BUSY_API_LENGTH_H:
        this->stateReciever = XBeeProS2CStateReciever::BUSY_API_LENGTH_L;
        // SmartSensorBoard::getBoard()->debug("GOT LENGTH H\n");
        this->apiLength = c << 8;
        break;

    case XBeeProS2CStateReciever::BUSY_API_LENGTH_L:
        this->stateReciever = XBeeProS2CStateReciever::BUSY_API_DATA;
        this->apiLength = this->apiLength | c;
        this->recieveBufferPointer = 0;
        // SmartSensorBoard::getBoard()->debugf("GOT LENGTH: %d\n", this->apiLength);
        break;

    case XBeeProS2CStateReciever::BUSY_API_DATA:
        if (this->recieveBufferPointer == this->apiLength)
        { // ready
            this->recieveBuffer[this->recieveBufferPointer] = c;
            this->stateReciever = XBeeProS2CStateReciever::PROCESSING_API;
            for (uint8_t i = 0; i <= this->apiLength; i++)
            { // Krijg het hier goed binnen, maar niet hierboven bij de loop?!
                uint8_t h2 = (this->recieveBuffer[i] & 0b11110000) >> 4;
                uint8_t h1 = this->recieveBuffer[i] & 0b00001111;
                Atmega324PBSerial0::getInstance()->transmitChar((h2 < 10 ? h2 + '0' : h2 - 10 + 'A'));
                Atmega324PBSerial0::getInstance()->transmitChar((h1 < 10 ? h1 + '0' : h1 - 10 + 'A'));
                Atmega324PBSerial0::getInstance()->transmitChar(' ');
            }
            Atmega324PBSerial0::getInstance()->transmitChar('\n');
        }
        else
        { // read the frame
            this->recieveBuffer[this->recieveBufferPointer] = c;
            this->recieveBufferPointer++;
        }

        break;
    }
}

void XBeeProS2C::sendToNode()
{
    // 0013A200418F9808713B => 7E 00 0E 00 01 00 13 A2 00 41 8F 98 08 00 68 6F 69 99 => hoi (64 bit address) - now this!
    // 0013A200418F9808713B => 7E 00 08 01 01 71 3B 00 68 6F 69 11 => hoi (16 bit address - network unique)
    // 2020-10-29T16:20:00,mt:25.6,mh:45.8 => to coordinator => 7E 00 2E 00 01 00 00 00 00 00 00 00 00 00 32 30 32 30 2D 31 30 2D 32 39 54 31 36 3A 32 30 3A 30 30 2C 6D 74 3A 32 35 2E 36 2C 6D 68 3A 34 35 2E 38 07
    Atmega324PBSerial1::getInstance()->transmitChar(0x7E);
    Atmega324PBSerial1::getInstance()->transmitChar(0x00);
    Atmega324PBSerial1::getInstance()->transmitChar(0x0E);
    Atmega324PBSerial1::getInstance()->transmitChar(0x00);
    Atmega324PBSerial1::getInstance()->transmitChar(0x01);
    Atmega324PBSerial1::getInstance()->transmitChar(0x00);
    Atmega324PBSerial1::getInstance()->transmitChar(0x13);
    Atmega324PBSerial1::getInstance()->transmitChar(0xA2);
    Atmega324PBSerial1::getInstance()->transmitChar(0x00);
    Atmega324PBSerial1::getInstance()->transmitChar(0x41);
    Atmega324PBSerial1::getInstance()->transmitChar(0x8F);
    Atmega324PBSerial1::getInstance()->transmitChar(0x98);
    Atmega324PBSerial1::getInstance()->transmitChar(0x08);
    Atmega324PBSerial1::getInstance()->transmitChar(0x00);
    Atmega324PBSerial1::getInstance()->transmitChar(0x68);
    Atmega324PBSerial1::getInstance()->transmitChar(0x6F);
    Atmega324PBSerial1::getInstance()->transmitChar(0x69);
    Atmega324PBSerial1::getInstance()->transmitChar(0x99);
}

void XBeeProS2C::transmitAndChecksum(char transmitChar, int *checksum)
{
    *checksum -= transmitChar;
    Atmega324PBSerial1::getInstance()->transmitChar(transmitChar);
}

void XBeeProS2C::sendMessageToCoordinator(const char *message)
{
    size_t size = getSize(message);
    int checksum = 0xFF;
    uint16_t i;
    size_t sizeID = getSize(SmartSensorBoard::getBoard()->getID());

    SmartSensorBoard::getBoard()->debugf_P(PSTR("sending msg: %s\n"), message);

#if XBEEPROS2C_USE_API_MODE_MSG == 1

    char buf[50];

    sprintf_P(buf, PSTR("got msg: %s of length %d with id: %d\n"), message, size, size + sizeID);
    SmartSensorBoard::getBoard()->debug(buf);

    Atmega324PBSerial1::getInstance()->transmitChar(0x7E);                    /* start delimiter */
    uint16_t length = 11 + size + sizeID;                                     /* length: length of message + 8 bytes address + 1 byte Frame ID + 1 byte Frame type + 1 byte options*/
    Atmega324PBSerial1::getInstance()->transmitChar((char)(length & 0xFF00)); /* length first byte */
    Atmega324PBSerial1::getInstance()->transmitChar((char)(length & 0xFF));   /* length second byte */
    this->transmitAndChecksum(0x00, &checksum);                               /* frame type */
    this->transmitAndChecksum(0x01, &checksum);                               /* frame ID */

    for (i = 0; i < 8; i++) /* transmit 8 bytes of 0 to make the 64 bit address of the coordinator. We don't need to substract this from the checksum as it's all 0 */
    {
        Atmega324PBSerial1::getInstance()->transmitChar(0x00);
    }
    this->transmitAndChecksum(0x00, &checksum); /* options */

    for (i = 0; i < sizeID; i++)
    {
        this->transmitAndChecksum(SmartSensorBoard::getBoard()->getID()[i], &checksum);
    }
    this->transmitAndChecksum(':', &checksum);

    for (i = 0; i < size; i++) /* transmit all bytes of the message */
    {
        this->transmitAndChecksum(message[i], &checksum);
    }

    this->transmitAndChecksum('\r', &checksum);
    this->transmitAndChecksum('\n', &checksum);

    /* calculate the checksum

    from digi's website:

        Add all bytes of the packet, except the start delimiter 0x7E and the length (the second and third bytes).
        Keep only the lowest 8 bits from the result.
        Subtract this quantity from 0xFF.
     */
    Atmega324PBSerial1::getInstance()->transmitChar((char)(checksum & 0xFF));
#else

    for (i = 0; i < sizeID; i++)
    {
        this->transmitAndChecksum(SmartSensorBoard::getBoard()->getID()[i], &checksum);
    }
    this->transmitAndChecksum(':', &checksum);
    for (i = 0; i < size; i++) /* transmit all bytes of the message */
    {
        this->transmitAndChecksum(message[i], &checksum);
    }

    this->transmitAndChecksum('\r', &checksum);
    this->transmitAndChecksum('\n', &checksum);
#endif
}

void XBeeProS2C::enableCoordinator()
{
    this->isCoordinator = true;
}

void XBeeProS2C::atStart()
{
    Atmega324PBSerial1 *x = Atmega324PBSerial1::getInstance();
    x->printAsync_P(PSTR("+++"));
}

void XBeeProS2C::atGetPanId()
{
    Atmega324PBSerial1 *x = Atmega324PBSerial1::getInstance();
    x->printAsync_P(PSTR("ATID\r"));
}

void XBeeProS2C::atGetCoordinatorEnable()
{
    Atmega324PBSerial1 *x = Atmega324PBSerial1::getInstance();
    x->printAsync_P(PSTR("ATCE\r"));
}

void XBeeProS2C::atGetSerialNumberHigh()
{
    Atmega324PBSerial1 *x = Atmega324PBSerial1::getInstance();
    x->printAsync_P(PSTR("ATSH\r"));
}

void XBeeProS2C::atGetSerialNumberLow()
{
    Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATSL\r"));
}

bool XBeeProS2C::checkResultOk()
{
    return (this->recieveBuffer[0] == 'O' && this->recieveBuffer[1] == 'K');
}

void XBeeProS2C::atSetPanId(const char *id)
{
    char m[15];
    sprintf_P(m, PSTR("ATID %s\r"), id);
    Atmega324PBSerial1::getInstance()->printAsync(m);
}

void XBeeProS2C::atSetCoordinator(bool enable)
{
    char m[15];
    sprintf_P(m, PSTR("ATCE %d\r"), enable);
    Atmega324PBSerial1::getInstance()->printAsync(m);
}

void XBeeProS2C::atSetNodeIdentifier(const char *id)
{
    char m[30];
    sprintf_P(m, PSTR("ATNI %s\r"), id);
    Atmega324PBSerial1::getInstance()->printAsync(m);
}

void XBeeProS2C::atWrite()
{
    Atmega324PBSerial1::getInstance()->printAsync_P(PSTR("ATWR\r"));
}

void XBeeProS2C::addMessageForTransfer(Message message)
{
    SmartSensorBoard::getBoard()->debugf_P(PSTR("Adding msg: %s\n"), message.getMessage());

    if (message.getType() == MessageType::MEASUREMENT)
    {
        if (this->queueCounter < XBEEPROS2C_MAX_MESSAGES) // if the queue is not yet completely full
        {
            // SmartSensorBoard::getBoard()->debugf_P(PSTR("not yet full: %d\n"), this->getQueueCounter());
            if (this->getQueueCounter() > 0)
            {
                // SmartSensorBoard::getBoard()->debug("queue not empty\n");
                // shift everything one place to the right
                for (int i = this->queueCounter - 1; i >= 0; i--) // start at the last element, go towards the front
                {
                    // SmartSensorBoard::getBoard()->debugf_P(PSTR("shifting msg from %d to %d\n"), i,i+1);
                    strcpy(this->transmitQueueArray[i + 1], this->transmitQueueArray[i]);
                }
                // SmartSensorBoard::getBoard()->debug("shifted\n");
            }
            // SmartSensorBoard::getBoard()->debugf_P(PSTR("about to add msg, size is %d\n"), this->getQueueCounter());
            // this->transmitQueueArray[0] = (char *)malloc(getSize(message.getMessage()));
            // strcpy(this->transmitQueueArray[0], message.getMessage());
            strcpy(this->transmitQueueArray[0], message.getMessage());
            SmartSensorBoard::getBoard()->debug("added new element\n");
            SmartSensorBoard::getBoard()->debugf_P(PSTR("first element is now %s\n"), this->transmitQueueArray[0]);
            this->queueCounter++;
            SmartSensorBoard::getBoard()->debugf_P(PSTR("done, size is now %d\n"), this->queueCounter);
        }
        else // the queue is already full
        {
            for (int i = XBEEPROS2C_MAX_MESSAGES - 1; i > 0; i--) // start from the back and shift all elements one to the right
            {
                // this->transmitQueueArray[i] = this->transmitQueueArray[i - 1];
                strcpy(this->transmitQueueArray[i], this->transmitQueueArray[i - 1]);
            }
            strcpy(this->transmitQueueArray[0], message.getMessage());
        }
    }
}

void XBeeProS2C::popQueueMessage(char** temp)
{
    SmartSensorBoard::getBoard()->debug("Popping!\n");
    if (this->queueCounter != 0)
    {
        *temp = this->transmitQueueArray[0];
        this->queueCounter--;
        for (int i = 1; i < this->queueCounter; i++)
        {
            // realloc(this->transmitQueueArray[i - 1], getSize(this->transmitQueueArray[i]));
            strcpy(this->transmitQueueArray[i - 1], this->transmitQueueArray[i]);
        }
        for (int i = this->queueCounter; i < XBEEPROS2C_MAX_MESSAGES - 1; i++)
        {
            free(this->transmitQueueArray[this->queueCounter]);
        }
    }
}

size_t getSize(const char *s)
{
    size_t size = 0;
    while (*s++)
        ++size;
    return size;
}
