/*

Copyright 2017 UL TS B.V. The Netherlands

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef SSPPROTOCOL_H
#define SSPPROTOCOL_H


typedef enum {
	SspTriggerModeImmediately = 0x01,	///< Immediately on each go-command when data is loaded. The probe enters stop-mode immediately after executing swipe.
	SspTriggerModeSingle = 0x05,		///< -- NOT SUPPORTED IN CURRENT HARDWARE -- Once, after entering go-mode on swipe detect. The probe enters stop-mode immediately after executing swipe.
	SspTriggerModeauto = 0x0A,			///< -- NOT SUPPORTED IN CURRENT HARDWARE -- Every time, after entering go-mode on swipe detect, until the probe switches to stop-mode.
} SspTriggerMode;

typedef enum {
	SspCommandDefaultConfiguration = 0xDC,	///< Reset to default configuration
	SspCommandDataBase = 0xd0,				///< Base command, add base-1 tracknumber to obtain real command	
	SspCommandData1 = 0xd1,					///< Data for track 1
	SspCommandData2 = 0xd2,					///< Data for track 2
	SspCommandData3 = 0xd3,					///< Data for track 3
	SspCommandConfigBase = 0xc0,			///< Base command, add base-1 tracknumber to obtain real command	
	SspCommandConfig1 = 0xc1,				///< Configuration for track 1
	SspCommandConfig2 = 0xc2,				///< Configuration for track 2
	SspCommandConfig3 = 0xc3,				///< Configuration for track 3
	SspCommandTriggerArm = 0x7A,			///< Arm the trigger: enter go-mode
	SspCommandTriggerDisarm = 0x7D,			///< Disarm the trigger: enter stop-mode
	SspCommandTriggerMode = 0x73,
	SspCommandSoftwareVersion = 0x5e,		///< Retrieve the firmware version
	SspCommandStartBootloader = 0x5b,		///< Reboot to bootloader
} SspCommandTag;

typedef enum {
	SspStatusOperationOk = 0x00,
	SspStatusErrorBase = 0xe0,				///< Base status for error codes
	SspStatusErrorSize = 0xe5,
	SspStatusErrorParsing = 0xe9,
	SspStatusErrorChecksum = 0xec,
	SspStatusErrorIllegalCommand = 0xe1,
	SspEventBase = 0x30,					///< Base code for events
	SspEventSwiped = 0x35,
} SspResponseTag;

typedef struct {
	uint8_t bootloaderMajor;
	uint8_t bootloaderMinor;
	uint8_t firmwareMajor;
	uint8_t firmwareMinor;
} SspFirmwareVersion;

typedef enum {
	LrcAuto = 0x0A,
	LrcManual = 0x03,
} LrcGenerationType;

typedef struct {
	LrcGenerationType lrcGeneration;	///< LRC: auto or manual
	uint16_t halfbittime;				///< Deprecated
	uint16_t prerunZeros;				///< Deprecated
	uint16_t postrunZeros;				///< Deprecated
	uint8_t manualLrc;					///< Value of LRC when lrcGeneration is set to manual.
} SspTrackConfiguration;

void sspConnect(char * serial);
void sspResetToDefaultConfiguration();
SspFirmwareVersion sspGetFirmwareVersion();
void sspSetTrackDataString(int tracknum, char * trackdata, size_t length);
void sspSetTriggerMode(SspTriggerMode triggerMode);
void sspSendGo();
void sspSendStop();

#endif /* not defined SSPPROTOCOL_H*/