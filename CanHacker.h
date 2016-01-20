/*
 * CanHacker.h
 *
 *  Created on: 17 ���. 2015 �.
 *      Author: Dmitry
 */

#ifndef CANHACKER_H_
#define CANHACKER_H_

#include <can.h>

#define CAN_MIN_DLEN 1
#define HEX_PER_BYTE 2
#define MIN_MESSAGE_DATA_HEX_LENGTH CAN_MIN_DLEN * HEX_PER_BYTE
#define MAX_MESSAGE_DATA_HEX_LENGTH CAN_MAX_DLEN * HEX_PER_BYTE
#define MIN_MESSAGE_LENGTH 5

#define CANHACKER_CMD_MAX_LENGTH 26

#define CANHACKER_SERIAL_RESPONSE     "N0001\r"
#define CANHACKER_SW_VERSION_RESPONSE "v0107\r"
#define CANHACKER_VERSION_RESPONSE    "V1010\r"

class CanHacker {
    public:
        enum ERROR { 
            ERROR_OK, 
            ERROR_CONNECTED, 
            ERROR_NOT_CONNECTED, 
            ERROR_UNEXPECTED_COMMAND,
            ERROR_INVALID_COMMAND,
            ERROR_ERROR_FRAME_NOT_SUPPORTED,
            ERROR_BUFFER_OVERFLOW,
            ERROR_SERIAL_TX_OVERRUN,
            ERROR_LISTEN_ONLY,
            ERROR_MCP2515_INIT,
            ERROR_MCP2515_INIT_CONFIG,
            ERROR_MCP2515_INIT_BITRATE,
            ERROR_MCP2515_INIT_SET_MODE,
            ERROR_MCP2515_SEND,
            ERROR_MCP2515_READ,
            ERROR_MCP2515_FILTER,
            ERROR_MCP2515_ERRIF,
            ERROR_MCP2515_MERRF
        };
        
        ERROR receiveCommand(const char *buffer, const int length);
        ERROR receiveCanFrame(const struct can_frame *frame);
        ERROR sendFrame(const struct can_frame *);
        ERROR setLoopbackEnabled(const bool value);

    private:
    
        enum /*class*/ COMMAND : char {
            COMMAND_SET_BITRATE    = 'S', // set CAN bit rate
            COMMAND_SET_BTR        = 's', // set CAN bit rate via
            COMMAND_OPEN_CAN_CHAN  = 'O', // open CAN channel
            COMMAND_CLOSE_CAN_CHAN = 'C', // close CAN channel
            COMMAND_SEND_11BIT_ID  = 't', // send CAN message with 11bit ID
            COMMAND_SEND_29BIT_ID  = 'T', // send CAN message with 29bit ID
            COMMAND_SEND_R11BIT_ID = 'r', // send CAN remote message with 11bit ID
            COMMAND_SEND_R29BIT_ID = 'R', // send CAN remote message with 29bit ID
            COMMAND_READ_STATUS    = 'F', // read status flag byte
            COMMAND_SET_ACR        = 'M', // set Acceptance Code Register
            COMMAND_SET_AMR        = 'm', // set Acceptance Mask Register
            COMMAND_GET_VERSION    = 'V', // get hardware and software version
            COMMAND_GET_SW_VERSION = 'v', // get software version only
            COMMAND_GET_SERIAL     = 'N', // get device serial number
            COMMAND_TIME_STAMP     = 'Z', // toggle time stamp setting
            COMMAND_READ_ECR       = 'E', // read Error Capture Register
            COMMAND_READ_ALCR      = 'A', // read Arbritation Lost Capture Register
            COMMAND_READ_REG       = 'G', // read register conten from SJA1000
            COMMAND_WRITE_REG      = 'W', // write register content to SJA1000
            COMMAND_LISTEN_ONLY    = 'L'  // switch to listen only mode
        };
    
        ERROR parseTransmit(const char *buffer, int length, struct can_frame *frame);
        ERROR createTransmit(const struct can_frame *frame, char *buffer, const int length);
        
        virtual ERROR connectCan();
        virtual ERROR disconnectCan();
        virtual bool isConnected();
        virtual ERROR writeCan(const struct can_frame *);
        virtual ERROR writeSerial(const char *buffer);
        virtual uint16_t getTimestamp();
        virtual ERROR setFilter(const uint32_t filter);
        virtual ERROR setFilterMask(const uint32_t mask);
        
        virtual ERROR receiveSetBitrateCommand(const char *buffer, const int length);
        ERROR receiveTransmitCommand(const char *buffer, const int length);
        ERROR receiveTimestampCommand(const char *buffer, const int length);
        ERROR receiveCloseCommand(const char *buffer, const int length);
        ERROR receiveOpenCommand(const char *buffer, const int length);
        ERROR receiveListenOnlyCommand(const char *buffer, const int length);
        ERROR receiveSetAcrCommand(const char *buffer, const int length);
        ERROR receiveSetAmrCommand(const char *buffer, const int length);
        
    protected:
        static const uint16_t TIMESTAMP_LIMIT = 0xEA60;
        
        static const char CR  = '\r';
        static const char BEL = 7;
        
        bool _timestampEnabled = false;
        bool _listenOnly = false;
        bool _loopback = false;
        
        ERROR writeSerial(const char character);
};

class CanHackerLineReader {
    private:
        static const int COMMAND_MAX_LENGTH = 30; // not including \r\0
        
        CanHacker *canHacker;
        char buffer[COMMAND_MAX_LENGTH + 2];
        int index;
    public:
        CanHackerLineReader(CanHacker *vCanHacker);
        CanHacker::ERROR processChar(char rxChar);
};

#endif /* CANHACKER_H_ */
