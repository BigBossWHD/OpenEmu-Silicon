// Copyright (c) 2026, OpenEmu Team
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef OEDualSenseBluetoothReport_h
#define OEDualSenseBluetoothReport_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t axes[6]; // X, Y, Z, Rz, Rx (L2), Ry (R2), matching report 0x01.
    uint8_t hatSwitch;
    uint16_t buttons; // The 14 buttons exposed by the simple-report descriptor.
} OEDualSenseBluetoothState;

// IOHIDValue supplies the 77-byte vendor-defined payload, without report ID
// 0x31. The CRC covers Bluetooth prefix 0xA1, report ID 0x31, and the payload
// preceding its four-byte little-endian CRC. No output/feature report is sent.
static inline bool OEDecodeDualSenseBluetoothReport(uint32_t reportID,
                                                   const uint8_t *payload,
                                                   size_t length,
                                                   OEDualSenseBluetoothState *state)
{
    if(reportID != 0x31 || payload == NULL || state == NULL || length != 77)
        return false;

    uint32_t crc = UINT32_MAX;
    for(size_t i = 0; i < 75; i++) {
        uint8_t byte = i == 0 ? 0xA1 : i == 1 ? 0x31 : payload[i - 2];
        crc ^= byte;
        for(unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xEDB88320) : 0);
    }
    crc = ~crc;
    uint32_t expected = (uint32_t)payload[73]
                      | ((uint32_t)payload[74] << 8)
                      | ((uint32_t)payload[75] << 16)
                      | ((uint32_t)payload[76] << 24);
    if(crc != expected)
        return false;

    memcpy(state->axes, payload + 1, sizeof(state->axes));
    state->hatSwitch = payload[8] & 0x0F;
    state->buttons = (payload[8] >> 4)
                   | ((uint16_t)payload[9] << 4)
                   | ((uint16_t)(payload[10] & 0x03) << 12);
    return true;
}

// Translate onto the existing report-0x01 elements so normal event creation
// can retain each control's cookie, saved bindings, and axis calibration.
static inline bool OEDualSenseBluetoothValueForUsage(const OEDualSenseBluetoothState *state,
                                                    uint32_t page, uint32_t usage,
                                                    int *value)
{
    if(page == 0x09 && usage >= 1 && usage <= 14) {
        *value = (state->buttons >> (usage - 1)) & 1;
        return true;
    }
    if(page != 0x01)
        return false;
    switch(usage) {
        case 0x30: *value = state->axes[0]; return true;
        case 0x31: *value = state->axes[1]; return true;
        case 0x32: *value = state->axes[2]; return true;
        case 0x35: *value = state->axes[3]; return true;
        case 0x33: *value = state->axes[4]; return true;
        case 0x34: *value = state->axes[5]; return true;
        case 0x39: *value = state->hatSwitch; return true;
        default: return false;
    }
}

#endif
