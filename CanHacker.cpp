/*
 * CanHacker.cpp
 *
 *  Created on: 17 ���. 2015 �.
 *      Author: Dmitry
 */
#include <Arduino.h>
#include <stdio.h>
#include <ctype.h>

#include "CanHacker.h"
#include "lib.h"

const char hex_asc_upper[] = "0123456789ABCDEF";

#define hex_asc_upper_lo(x)    hex_asc_upper[((x) & 0x0F)]
#define hex_asc_upper_hi(x)    hex_asc_upper[((x) & 0xF0) >> 4]

static inline void put_hex_byte(char *buf, __u8 byte)
{
    buf[0] = hex_asc_upper_hi(byte);
    buf[1] = hex_asc_upper_lo(byte);
}

static inline void _put_id(char *buf, int end_offset, canid_t id)
{
    /* build 3 (SFF) or 8 (EFF) digit CAN identifier */
    while (end_offset >= 0) {
        buf[end_offset--] = hex_asc_upper[id & 0xF];
        id >>= 4;
    }
}

#define put_sff_id(buf, id) _put_id(buf, 2, id)
#define put_eff_id(buf, id) _put_id(buf, 7, id)

CanHacker::CanHacker(Stream *stream, Stream *debugStream, uint8_t cs) {
    _stream = stream;
    _debugStream = debugStream;

    writeDebugStream("Initialization\n");

    _cs = cs;
    mcp2515 = new MCP_CAN(_cs);
    mcp2515->reset();
    mcp2515->setConfigMode();
}

CanHacker::~CanHacker() {
    delete mcp2515;
}

Stream *CanHacker::getInterfaceStream() {
    return _stream;
}

CanHacker::ERROR CanHacker::connectCan() {
    MCP_CAN::ERROR error = mcp2515->setBitrate(bitrate);
    if (error != MCP_CAN::ERROR_OK) {
        writeDebugStream("setBitrate error:\n");
        writeDebugStream((int)error);
        writeDebugStream("\n");
        return ERROR_MCP2515_INIT_BITRATE;
    }
    
    if (_loopback) {
        error = mcp2515->setLoopbackMode();
    } else if (_listenOnly) {
        error = mcp2515->setListenOnlyMode();
    } else {
        error = mcp2515->setNormalMode();
    }
    
    if (error != MCP_CAN::ERROR_OK) {
        return ERROR_MCP2515_INIT_SET_MODE;
    }
    
    _isConnected = true;
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::disconnectCan() {
    _isConnected = false;
    mcp2515->setConfigMode();
    return ERROR_OK;
}

bool CanHacker::isConnected() {
    return _isConnected;
}

CanHacker::ERROR CanHacker::writeCan(const struct can_frame *frame) {
    if (mcp2515->sendMessage(frame) != MCP_CAN::ERROR_OK) {
        return ERROR_MCP2515_SEND;
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::pollReceiveCan() {
    if (!isConnected()) {
        return ERROR_OK;
    }
    
    while (mcp2515->checkReceive()) {
        struct can_frame frame;
        if (mcp2515->readMessage(&frame) != MCP_CAN::ERROR_OK) {
            return ERROR_MCP2515_READ;
        }
    
        ERROR error = receiveCanFrame(&frame);
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::receiveCan(const MCP_CAN::RXBn rxBuffer) {
    if (!isConnected()) {
        return ERROR_OK;
    }
    
    struct can_frame frame;
    MCP_CAN::ERROR result = mcp2515->readMessage(rxBuffer, &frame);
    if (result == MCP_CAN::ERROR_NOMSG) {
        return ERROR_OK;
    }
    
    if (result != MCP_CAN::ERROR_OK) {
        return ERROR_MCP2515_READ;
    }

    return receiveCanFrame(&frame);
}

MCP_CAN *CanHacker::getMcp2515() {
    return mcp2515;
}

uint16_t CanHacker::getTimestamp() {
    return millis() % TIMESTAMP_LIMIT;
}

CanHacker::ERROR CanHacker::receiveSetBitrateCommand(const char *buffer, const int length) {
    if (isConnected()) {
        writeDebugStream("Bitrate command cannot be called while connected\n");
        writeStream(BEL);
        return ERROR_CONNECTED;
    }
    
    if (length < 2) {
        writeStream(BEL);
        writeDebugStream("Bitrate command must by 2 bytes long\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }
    switch(buffer[1]) {
        case '0':
            writeDebugStream("Set bitrate 10KBPS\n");
            bitrate = CAN_10KBPS;
            break;
        case '1':
            writeDebugStream("Set bitrate 20KBPS\n");
            bitrate = CAN_20KBPS;
            break;
        case '2':
            writeDebugStream("Set bitrate 50KBPS\n");
            bitrate = CAN_50KBPS;
            break;
        case '3':
            writeDebugStream("Set bitrate 100KBPS\n");
            bitrate = CAN_100KBPS;
            break;
        case '4':
            writeDebugStream("Set bitrate 125KBPS\n");
            bitrate = CAN_125KBPS;
            break;
        case '5':
            writeDebugStream("Set bitrate 250KBPS\n");
            bitrate = CAN_250KBPS;
            break;
        case '6':
            writeDebugStream("Set bitrate 500KBPS\n");
            bitrate = CAN_500KBPS;
            break;
        case '7':
            writeDebugStream("Bitrate 7 is not supported\n");
            writeDebugStream((const uint8_t*)buffer, length);
            writeDebugStream('\n');
            writeStream(BEL);
            return ERROR_INVALID_COMMAND;
            break;
        case '8':
            writeDebugStream("Set bitrate 1000KBPS\n");
            bitrate = CAN_1000KBPS;
            break;
        default:
            writeDebugStream("Unexpected bitrate\n");
            writeDebugStream((const uint8_t*)buffer, length);
            writeDebugStream('\n');
            writeStream(BEL);
            return ERROR_INVALID_COMMAND;
            break;
    }
    
    return writeStream(CR);
}

CanHacker::ERROR CanHacker::processInterrupt() {
    if (!isConnected()) {
        writeDebugStream("Process interrupt while not connected\n");
        return ERROR_NOT_CONNECTED;
    }

    uint8_t irq = mcp2515->getInterrupts();

    if (irq & MCP_CAN::CANINTF_ERRIF) {
        // reset RXnOVR errors
        mcp2515->clearRXnOVR();
    }
    
    if (irq & MCP_CAN::CANINTF_RX0IF) {
        ERROR error = receiveCan(MCP_CAN::RXB0);
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    if (irq & MCP_CAN::CANINTF_RX1IF) {
        ERROR error = receiveCan(MCP_CAN::RXB1);
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    /*if (irq & (MCP_CAN::CANINTF_TX0IF | MCP_CAN::CANINTF_TX1IF | MCP_CAN::CANINTF_TX2IF)) {
        _debugStream->print("MCP_TXxIF\r\n");
        stopAndBlink(1);
    }*/
     
    
     
    /*if (irq & MCP_CAN::CANINTF_WAKIF) {
        _debugStream->print("MCP_WAKIF\r\n");
        stopAndBlink(3);
    }*/
     
    if (irq & MCP_CAN::CANINTF_MERRF) {
        _debugStream->print("MCP_MERRF\r\n");
        return ERROR_MCP2515_MERRF;
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::setFilter(const uint32_t filter) {
    if (isConnected()) {
        writeDebugStream("Filter cannot be set while connected\n");
        return ERROR_CONNECTED;
    }
    
    MCP_CAN::RXF filters[] = {MCP_CAN::RXF0, MCP_CAN::RXF1, MCP_CAN::RXF2, MCP_CAN::RXF3, MCP_CAN::RXF4, MCP_CAN::RXF5};
    for (int i=0; i<6; i++) {
        MCP_CAN::ERROR result = mcp2515->setFilter(filters[i], false, filter);
        if (result != MCP_CAN::ERROR_OK) {
            return ERROR_MCP2515_FILTER;
        }
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::setFilterMask(const uint32_t mask) {
    if (isConnected()) {
        writeDebugStream("Filter mask cannot be set while connected\n");
        return ERROR_CONNECTED;
    }
    
    MCP_CAN::MASK masks[] = {MCP_CAN::MASK0, MCP_CAN::MASK1};
    for (int i=0; i<2; i++) {
        MCP_CAN::ERROR result = mcp2515->setFilterMask(masks[i], false, mask);
        if (result != MCP_CAN::ERROR_OK) {
            return ERROR_MCP2515_FILTER;
        }
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::writeStream(const char character) {
    char str[2];
    str[0] = character;
    str[1] = '\0';
    return writeStream(str);
}

CanHacker::ERROR CanHacker::writeStream(const char *buffer) {
    /*if (_stream->availableForWrite() < strlen(buffer)) {
        return ERROR_SERIAL_TX_OVERRUN;
    }*/
    size_t printed = _stream->print(buffer);
    if (printed != strlen(buffer)) {
        return ERROR_SERIAL_TX_OVERRUN;
    }
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::writeDebugStream(const char character) {
    if (_debugStream != NULL) {
        _debugStream->write(character);
    }
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::writeDebugStream(const char *buffer) {
    if (_debugStream != NULL) {
        _debugStream->print(buffer);
    }
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::writeDebugStream(const uint8_t *buffer, size_t size) {
    if (_debugStream != NULL) {
        _debugStream->write(buffer, size);
    }
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::writeDebugStream(const int buffer) {
    if (_debugStream != NULL) {
        _debugStream->print(buffer);
    }
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::receiveCommand(const char *buffer, const int length) {
    switch (buffer[0]) {
        case COMMAND_GET_SERIAL: {
            return writeStream(CANHACKER_SERIAL_RESPONSE);
        }

        case COMMAND_GET_SW_VERSION: {
            return writeStream(CANHACKER_SW_VERSION_RESPONSE);
        }

        case COMMAND_GET_VERSION: {
            return writeStream(CANHACKER_VERSION_RESPONSE);
        }

        case COMMAND_SEND_11BIT_ID:
        case COMMAND_SEND_29BIT_ID:
        case COMMAND_SEND_R11BIT_ID:
        case COMMAND_SEND_R29BIT_ID:
            return receiveTransmitCommand(buffer, length);

        case COMMAND_CLOSE_CAN_CHAN:
            return receiveCloseCommand(buffer, length);
            
        case COMMAND_OPEN_CAN_CHAN:
            return receiveOpenCommand(buffer, length);
            
        case COMMAND_SET_BITRATE:
            return receiveSetBitrateCommand(buffer, length);
            
        case COMMAND_SET_ACR:
            return receiveSetAcrCommand(buffer, length);
            
        case COMMAND_SET_AMR:
            return receiveSetAmrCommand(buffer, length);
        
        case COMMAND_SET_BTR: {
            if (isConnected()) {
                writeStream(BEL);
                writeDebugStream("SET_BTR command cannot be called while connected\n");
                return ERROR_CONNECTED;
            }
            writeDebugStream("SET_BTR not supported\n");
            return writeStream(CR);
        }
        
        case COMMAND_LISTEN_ONLY:
            return receiveListenOnlyCommand(buffer, length);
        
        case COMMAND_TIME_STAMP:
            return receiveTimestampCommand(buffer, length);
            
        case COMMAND_WRITE_REG:
        case COMMAND_READ_REG: {
            return writeStream(CR);
        }
        
        case COMMAND_READ_STATUS:
        case COMMAND_READ_ECR:
        case COMMAND_READ_ALCR: {
            if (!isConnected()) {
                writeDebugStream("Read status, ecr, alcr while not connected\n");
                writeStream(BEL);
                return ERROR_NOT_CONNECTED;
            }
            return writeStream(CR);
        }

        default: {
            writeStream(BEL);
            writeDebugStream("Unknown command received\n");
            writeDebugStream((const uint8_t*)buffer, length);
            writeDebugStream('\n');
            return ERROR_UNKNOWN_COMMAND;
        }
    }
}

CanHacker::ERROR CanHacker::receiveCanFrame(const struct can_frame *frame) {
    char out[35];
    ERROR error = createTransmit(frame, out, 35);
    if (error != ERROR_OK) {
        return error;
    }
    return writeStream(out);
}

CanHacker::ERROR CanHacker::parseTransmit(const char *buffer, int length, struct can_frame *frame) {
    if (length < MIN_MESSAGE_LENGTH) {
        writeDebugStream("Transmit message lenght < minimum\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }

    int isExended = 0;
    int isRTR = 0;

    switch (buffer[0]) {
        case 't':
            break;
        case 'T':
            isExended = 1;
            break;
        case 'r':
            isRTR = 1;
            break;
        case 'R':
            isExended = 1;
            isRTR = 1;
            break;
        default:
            writeDebugStream("Unexpected type of transmit command\n");
            writeDebugStream((const uint8_t*)buffer, length);
            writeDebugStream('\n');
            return ERROR_INVALID_COMMAND;

    }
    
    int offset = 1;

    canid_t id = 0;
    int idChars = isExended ? 8 : 3;
    for (int i=0; i<idChars; i++) {
        id <<= 4;
        id += hexCharToByte(buffer[offset++]);
    }
    if (isRTR) {
        id |= CAN_RTR_FLAG;
    }
    if (isExended) {
        id |= CAN_EFF_FLAG;
    }
    frame->can_id = id;
    
    __u8 dlc = hexCharToByte(buffer[offset++]);
    if (dlc > 8) {
        writeDebugStream("DLC > 8\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }
    if (dlc == 0) {
        writeDebugStream("DLC = 0\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }
    frame->can_dlc = dlc;

    if (!isRTR) {
        for (int i=0; i<dlc; i++) {
            char hiHex = buffer[offset++];
            char loHex = buffer[offset++];
            frame->data[i] = hexCharToByte(loHex) + (hexCharToByte(hiHex) << 4);
        }
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::createTransmit(const struct can_frame *frame, char *buffer, const int length) {
    int offset;
    int len = frame->can_dlc;

    int isRTR = (frame->can_id & CAN_RTR_FLAG) ? 1 : 0;
    
    if (frame->can_id & CAN_ERR_FLAG) {
        return ERROR_ERROR_FRAME_NOT_SUPPORTED;
    } else if (frame->can_id & CAN_EFF_FLAG) {
        buffer[0] = isRTR ? 'R' : 'T';
        put_eff_id(buffer+1, frame->can_id & CAN_EFF_MASK);
        offset = 9;
    } else {
        buffer[0] = isRTR ? 'r' : 't';
        put_sff_id(buffer+1, frame->can_id & CAN_SFF_MASK);
        offset = 4;
    }

    buffer[offset++] = hex_asc_upper_lo(frame->can_dlc);

    if (!isRTR) {
        int i;
        for (i = 0; i < len; i++) {
            put_hex_byte(buffer + offset, frame->data[i]);
            offset += 2;
        }
    }
    
    if (_timestampEnabled) {
        uint16_t ts = getTimestamp();
        put_hex_byte(buffer + offset, ts >> 8);
        offset += 2;
        put_hex_byte(buffer + offset, ts);
        offset += 2;
    }
    
    buffer[offset++] = CR;
    buffer[offset] = '\0';
    
    if (offset >= length) {
        return ERROR_BUFFER_OVERFLOW;
    }
    
    return ERROR_OK;
}

CanHacker::ERROR CanHacker::sendFrame(const struct can_frame *frame) {
    return writeCan(frame);
}

CanHacker::ERROR CanHacker::receiveTransmitCommand(const char *buffer, const int length) {
    if (!isConnected()) {
        writeDebugStream("Transmit command while not connected\n");
        return ERROR_NOT_CONNECTED;
    }
    
    if (_listenOnly) {
        return ERROR_LISTEN_ONLY;
    }
    
    struct can_frame frame;
    ERROR error = parseTransmit(buffer, length, &frame);
    if (error != ERROR_OK) {
        return error;
    }
    error = writeCan(&frame);
    if (error != ERROR_OK) {
        return error;
    }

    return writeStream(CR);
}

CanHacker::ERROR CanHacker::receiveTimestampCommand(const char *buffer, const int length) {
    if (length != 2) {
        writeStream(BEL);
        writeDebugStream("Timestamp command must be 2 bytes long\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }
    switch (buffer[1]) {
        case '0':
            _timestampEnabled = false;
            return writeStream(CR);
        case '1':
            _timestampEnabled = true;
            return writeStream(CR);
        default:
            writeStream(BEL);
            writeDebugStream("Timestamp cammand must have value 0 or 1\n");
            writeDebugStream((const uint8_t*)buffer, length);
            writeDebugStream('\n');
            return ERROR_INVALID_COMMAND;
    }

    return ERROR_OK;
}

CanHacker::ERROR CanHacker::receiveCloseCommand(const char *buffer, const int length) {
    writeDebugStream("receiveCloseCommand\n");
    if (!isConnected()) {
        return writeStream(BEL);
    }
    ERROR error = disconnectCan();
    if (error != ERROR_OK) {
        return error;
    }
    return writeStream(CR);
}

CanHacker::ERROR CanHacker::receiveOpenCommand(const char *buffer, const int length) {
    writeDebugStream("receiveOpenCommand\n");
    ERROR error = connectCan();
    if (error != ERROR_OK) {
        return error;
    }
    return writeStream(CR);
}

CanHacker::ERROR CanHacker::receiveListenOnlyCommand(const char *buffer, const int length) {
    writeDebugStream("receiveListenOnlyCommand\n");
    if (isConnected()) {
        writeStream(BEL);
        writeDebugStream("ListenOnly command cannot be called while connected\n");
        return ERROR_CONNECTED;
    }
    _listenOnly = true;
    return writeStream(CR);
}

CanHacker::ERROR CanHacker::receiveSetAcrCommand(const char *buffer, const int length) {
    if (length != 9) {
        writeStream(BEL);
        writeDebugStream("ACR command must by 9 bytes long\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }
    uint32_t id = 0;
    for (int i=1; i<=8; i++) {
        id <<= 4;
        id += hexCharToByte(buffer[i]);
    }
    
    bool beenConnected = isConnected();
    ERROR error;
    
    if (beenConnected) {
        error = disconnectCan();
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    error = setFilter(id);
    if (error != ERROR_OK) {
        return error;
    }
    
    if (beenConnected) {
        error = connectCan();
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    return writeStream(CR);
}

CanHacker::ERROR CanHacker::receiveSetAmrCommand(const char *buffer, const int length) {
    if (length != 9) {
        writeStream(BEL);
        writeDebugStream("AMR command must by 9 bytes long\n");
        writeDebugStream((const uint8_t*)buffer, length);
        writeDebugStream('\n');
        return ERROR_INVALID_COMMAND;
    }
    uint32_t id = 0;
    for (int i=1; i<=8; i++) {
        id <<= 4;
        id += hexCharToByte(buffer[i]);
    }
    
    bool beenConnected = isConnected();
    ERROR error;
    
    if (beenConnected) {
        error = disconnectCan();
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    error = setFilterMask(id);
    if (error != ERROR_OK) {
        return error;
    }
    
    if (beenConnected) {
        error = connectCan();
        if (error != ERROR_OK) {
            return error;
        }
    }
    
    return writeStream(CR);
}

CanHacker::ERROR CanHacker::setLoopbackEnabled(const bool value) {
    if (isConnected()) {
        writeDebugStream("Loopback cannot be changed while connected\n");
        return ERROR_CONNECTED;
    }
    
    _loopback = value;
    
    return ERROR_OK;
}
