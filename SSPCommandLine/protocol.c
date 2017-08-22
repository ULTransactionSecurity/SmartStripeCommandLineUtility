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
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "hidapi/hidapi.h"
#include "SSPCommandLineTool.h"
#include "protocol.h"
#include "util.h"

hid_device * sspHid = NULL;

// responses from the probe are always short: only tag + overhead
#define COMM_USB_MAX_PACKETDATASIZE_IN 256
// messages sent to the probe are longer, containing up to 120 data bytes. Let's just round that to 256 bytes.
#define COMM_USB_MAX_PACKETDATASIZE_OUT 256
#define USB_HID_REPORT_LENGTH 64

typedef enum { up_start, up_tag, up_length1, up_length2, up_data, up_checksum1, up_checksum2, up_end } comm_usb_bytereader_state_t;

typedef struct {
	uint8_t tag;										///< stores the packet's tag
	uint16_t length;									///< stores the packet's payload length
	uint8_t packetbuffer[COMM_USB_MAX_PACKETDATASIZE_IN];  ///< stores the packet's data content
	uint16_t fillpointer; // = 0						///< indicates the index where the next data byte needs to be stored.
	uint16_t checksum;									///< stores the packet's checksum
	comm_usb_bytereader_state_t brstate;				///< state of the packetparse statemachine
	bool dle_escape;									///< if the previous character was a DLE
} comm_usb_parse_data_t;

/// Initialize the parse state: no data yet, everything on zero and empty, bytereader starts in the up_start state and the dle-escape state is false (no escape)
comm_usb_parse_data_t parse_state = {
	.tag = 0,
	.length = 0,
	.packetbuffer = { 0, },
	.checksum = 0,
	.fillpointer = 0,
	.brstate = up_start,
	.dle_escape = false,
};

static bool addData(uint8_t * data, size_t * fillcount, size_t maxlength, uint8_t newbyte) {
	data[*fillcount] = newbyte;

	(*fillcount)++;
	if (*fillcount >= maxlength) {
		*fillcount = maxlength - 1;
		return false;
	}
	return true;
}
//  adds newbyte to data (escaped if neccesary) and increments fillcount with the number of bytes added, making sure fillcount never exceeds maxlength. Returns true when maxlength was not exceeded.
static bool addDataEscapeDle(uint8_t * data, size_t * fillcount, size_t maxlength, uint8_t newbyte) {
	// escape with DLE if the byte is a DLE
	if (newbyte == DLE) {
		addData(data, fillcount, maxlength, DLE);
	}
	return addData(data, fillcount, maxlength, newbyte);
}

// On Linux data is apparently not lost when closing the device. So to make sure there is no data in the buffer we do a
// quick flush, otherwise the simple method of parsing the responses used in this utility will get confused, because on
// reading it gets an earlier (=wrong) response.
void sspHidFlush() {
	uint8_t response[USB_HID_REPORT_LENGTH + 1];
	for (int i = 0; i < 5; i++) {
		int bytesread = hid_read_timeout(sspHid, response, ARRAY_SIZE(response), 1);
		// timeout
		if (bytesread == 0) {
			return;
		}
	}
}

static void sspSendCommand(SspCommandTag tag, const unsigned char *data, size_t length) {
	sspHidFlush();
	uint8_t report[COMM_USB_MAX_PACKETDATASIZE_OUT];

	size_t fillcount = 0;
	// report number of the hid report
	report[fillcount++] = 0;

	uint16_t crc;
	Crc_init(&crc);
	Crc_add(&crc, (uint8_t)tag);
	Crc_add(&crc, (uint8_t)((length >> 8) & 0xff));
	Crc_add(&crc, (uint8_t)(length & 0xff));
	for (size_t i = 0; i < length; i++) {
		Crc_add(&crc, (uint8_t)(data[i] & 0xff));
	}
	bool lengthOk = false;
	// Header: DLE STX
	lengthOk = addData(report, &fillcount, ARRAY_SIZE(report), DLE);
	lengthOk &= addData(report, &fillcount, ARRAY_SIZE(report), STX);
	// Tag
	lengthOk &= addDataEscapeDle(report, &fillcount, ARRAY_SIZE(report), tag);
	// Length
	lengthOk &= addDataEscapeDle(report, &fillcount, ARRAY_SIZE(report), ((length >> 8) & 0xff));
	lengthOk &= addDataEscapeDle(report, &fillcount, ARRAY_SIZE(report), (length & 0xff));
	// Data
	for (size_t i = 0; i < length; i++) {
		lengthOk &= addDataEscapeDle(report, &fillcount, ARRAY_SIZE(report), data[i]);
	}
	// CRC:
	lengthOk &= addDataEscapeDle(report, &fillcount, ARRAY_SIZE(report), ((crc >> 8) & 0xff));
	lengthOk &= addDataEscapeDle(report, &fillcount, ARRAY_SIZE(report), (crc & 0xff));

	// Tail: DLE ETX
	lengthOk &= addData(report, &fillcount, ARRAY_SIZE(report), DLE);
	lengthOk &= addData(report, &fillcount, ARRAY_SIZE(report), ETX);

	if (!lengthOk) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, constructed message would be too long to transfer to the device");
	}

	// transfer the message, if needed in parts.
	unsigned int transferred = 0;
	while (transferred < fillcount) {
		uint8_t tempbuffer[USB_HID_REPORT_LENGTH + 1];
		unsigned int thisTransferLength = min(USB_HID_REPORT_LENGTH, fillcount - transferred);
		// unused bytes filled with zeroes.
		memset(tempbuffer, 0, sizeof(tempbuffer));
		
		// report id in position 0
		tempbuffer[0] = 0;
		
		// copy data to temp buffer
		memcpy(tempbuffer + 1, report + transferred, thisTransferLength);
		/*printf("Writing: ");
		for (int i = 0; i < sizeof(tempbuffer); i++) {
			printf("%02x ", tempbuffer[i]);
		}
		printf("\n"); 
		printf("Writing: ");
		for (int i = 0; i < sizeof(tempbuffer); i++) {
			printf("%c  ", tempbuffer[i]);
		}
		printf("\n"); */
		// write tempbuffer to device.
		int numbyteswritten = hid_write(sspHid, tempbuffer, sizeof(tempbuffer));
		if (numbyteswritten != sizeof(tempbuffer)) {
			cleanUpAndExit(ExitErrorCommunicationProtocol, "Incorrect number of bytes written: %d", numbyteswritten);
		}

		transferred += thisTransferLength;

	}
}

typedef enum {parse_busy, parse_error, parse_done} ParseState;

/// Parses packets. Packets consist of DLE STX [tag] [lenght] [lenght] [value] ... [CRC] [CRC] DLE ETX. If a DLE is found in the data, it is escaped with a DLE.
ParseState packetParser(uint8_t c) {
	ParseState result = parse_busy;
	// First de DLE filtering, to avoid an exessive amount of cases in the statemachine:
	if (parse_state.dle_escape) {
		// We received a DLE previously, so now we get the escaped character. The only allowed characters are: STX ETX or DLE, otherwise its a parse error.
		parse_state.dle_escape = false;
		if (c == STX) {
			// DLE STX
			// If we're not in start state we are getting a DLE STX in the middle of parsing a message
			if (parse_state.brstate != up_start) {
				result = parse_error;
			}
			parse_state.brstate = up_tag;
			return result;
		}
		else if (c == ETX) {
			//DLE ETX, we're done.
			result = parse_done;
			parse_state.brstate = up_start;
			return result;
		}
		else if (c == DLE) {
			// Ok, escaped DLE, please continue.
		}
		else {
			result = parse_error;
			return result;
		}
	}
	else if (c == DLE) {
		// First DLE
		parse_state.dle_escape = true;
		return result;
	}

	// So if and when you end up here, c contains a databyte and not a control byte (like start/end transmission) 
	switch (parse_state.brstate) {
	case up_start:
		// If you end up here, then it's a character that was not sent within a message. Because if it was a DLE STX 
		// it would have been filtered and the state machine would have been advanced to the up_tag state. We stay in 
		// this state until a DLE STX is detected.
		break;
	case up_tag:
		parse_state.tag = c;
		parse_state.brstate = up_length1;
		break;
	case up_length1:
		// MSB of length
		parse_state.length = ((uint16_t)c) << 8;
		parse_state.brstate = up_length2;
		break;
	case up_length2:
		// MSB of length
		parse_state.length = parse_state.length | c;
		if (parse_state.length > 0) {
			parse_state.brstate = up_data;
			parse_state.fillpointer = 0;
		}
		else {
			parse_state.brstate = up_checksum1;
		}
		break;
	case up_data:
		// Store the byte if it fits. 
		// If there are more bytes than will fit, we will just ignore them silently. Maybe I'm going to regret this decision...
		if (parse_state.fillpointer < COMM_USB_MAX_PACKETDATASIZE_IN) {
			parse_state.packetbuffer[parse_state.fillpointer] = c;
		}
		parse_state.fillpointer++;
		// Got all bytes? Move to checksum.
		if (parse_state.fillpointer >= parse_state.length) {
			parse_state.brstate = up_checksum1;
		}
		break;
	case up_checksum1:
		parse_state.checksum = ((uint16_t)c) << 8;
		parse_state.brstate = up_checksum2;
		break;
	case up_checksum2:
		parse_state.checksum = parse_state.checksum | c;
		parse_state.brstate = up_end;
		break;
	case up_end:
		result = parse_error;
		break;
	default:
		// Just in case the state machine breaks...
		parse_state.brstate = up_start;
		result = parse_error;
		break;
	}
	return result;
}
// lets the packetParser parse the packet until it's done or encountered an error. When parse_ok is returned, parse_state contains the packet contents
ParseState parseResponsePacket(uint8_t * response, size_t length) {
	for (size_t i = 0; i < length; i++) {
		ParseState p = packetParser(response[i]);
		if (p != parse_busy) {
			return p;
		}
	}
	return parse_busy;
}

bool receivedCrcIsOk() {
	uint16_t calculatedCrc;

	Crc_init(&calculatedCrc);
	Crc_add(&calculatedCrc, parse_state.tag);
	Crc_add(&calculatedCrc, (parse_state.length >> 8) & 0xff);
	Crc_add(&calculatedCrc, parse_state.length & 0xff);

	for (uint16_t i = 0; i < parse_state.length; i++) {
		Crc_add(&calculatedCrc, parse_state.packetbuffer[i]);
	}
	return calculatedCrc == parse_state.checksum;
}

// Sends a comand as method call to the device. A valid method call should always result in a OperationOk response.
void sspMethodCall(SspCommandTag tag, const void *argument_data, size_t argument_length) {
	sspSendCommand(tag, argument_data, argument_length);
	
	uint8_t response[USB_HID_REPORT_LENGTH + 1];
	int bytesread = hid_read_timeout(sspHid, response, ARRAY_SIZE(response), 1000);
	// timeout
	if (bytesread == -1) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, no response received");
	}
	if (parseResponsePacket(response, ARRAY_SIZE(response)) != parse_done) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, error parsing response");
	}
	if (!receivedCrcIsOk()) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, CRC is wrong");
	}
	if (parse_state.tag != SspStatusOperationOk) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, device did not report OK on methodcall");
	}
}

// Sends a comand as method call to the device. A valid method call should always result in a OperationOk response.
void sspFunctionCall(SspCommandTag tag, const unsigned char *argument_data, size_t argument_length, void *result_data, size_t result_length) {
	sspSendCommand(tag, argument_data, argument_length);

	uint8_t response[USB_HID_REPORT_LENGTH + 1];
	int bytesread = hid_read_timeout(sspHid, response, ARRAY_SIZE(response), 1000);
	// timeout
	if (bytesread == -1) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, no response received");
	}
	if (parseResponsePacket(response, ARRAY_SIZE(response)) != parse_done) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, error parsing response");
	}
	if (!receivedCrcIsOk()) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, CRC is wrong");
	}

	// function calls return responses using the same tag.
	if (parse_state.tag != tag) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, device did not report same tag");
	}
	
	// Responses can be longer in the future, but never shorter (for forwards compatibility). So if we get a response that's shorter than the variable we're requested to fill, that's an error.
	if (parse_state.length < result_length) {
		cleanUpAndExit(ExitErrorCommunicationProtocol, "Communication protocol error, too short response");
	}
	// copy up to result_length number of bytes to the result_data
	memcpy(result_data, parse_state.packetbuffer, result_length);
}

// Get firmware version
SspFirmwareVersion sspGetFirmwareVersion() {
	SspFirmwareVersion version;
	sspFunctionCall(SspCommandSoftwareVersion, NULL, 0, &version, sizeof(version));
	return version;
}

/* Set track data using supplied string. If the string is "" then nothing will be sent on this track, so no zeros before and after as well.
 * Extra info:
 *  Magstripe cards do not use 8 bits for characters. 
 *  To encode a useful amount of data in the limited available space, a limited character set is used; there are less than the usual 8 bits per symbol.
 * 
 *  *Track 1* 
 *  Track 1 uses 7 bits per symbol: 6 databits and a parity bit. Bits are encoded on a physical card with 210 bits per inch. Valid characters are characters
 *  with ascii value between 0x20 and 0x5f.
 *  To get the actual symbol value from an ascii character subtract 0x20. So a capital A (0x41) is sent as 0x21.
 *  The SSP allows up to 85 bytes for track 1
 *
 *  *Track 2 and 3*
 *  Track 2 and 3 uses 5 bits per symbol: 4 databits and a parity bit. Track 2 is encoded with 75 bits per inch, track 3 with 210. Valid characters are characters
 *  with ascii value between 0x30 and 0x3f.
 *  To convert an ascii value to a symbol value subtract 0x30.
 *  The SSP allows up to 120 bytes for track 2 and 3
 */
void sspSetTrackDataString(int tracknum, char * trackdata, size_t length) {
	uint8_t min;
	uint8_t max;
	uint8_t subtract;
	if (tracknum == 1) {
		min = 0x20;
		max = 0x5f;
		subtract = 0x20;
	} else {
		min = 0x30;
		max = 0x3f;
		subtract = 0x30;
	}
	char * converteddata = alloca(length);
	memcpy(converteddata, trackdata, length);
	
	for (size_t i = 0; i < length; i++) {
		if ((trackdata[i] >= min) && (trackdata[i] <= max)) {
			converteddata[i] = (trackdata[i] - subtract);
		}
		else {
			cleanUpAndExit(ExitErrorCommandLineParameter, "Invalid character supplied, 0x%02x (%c) at position %d is not between 0x%02x and 0x%02x", trackdata[i], trackdata[i], i + 1, min, max);
		}
	}
	sspMethodCall(SspCommandDataBase + tracknum, converteddata, length);
}

// use this function to directly set the binary characters of the track data. If you want the parity of the byte to be wrong, make the most significant bit high. 
// Because the SSP internally calculates the party over the whole byte, and then ignores the most significant 2 or 4 bits, the parity bit will be inverted.
void sspSetTrackDataBinary(int tracknum, uint8_t * trackdata, size_t length) {
	sspMethodCall(SspCommandDataBase + tracknum, trackdata, length);
}

// Set the trigger mode: there is only one valid mode: immediately. The other ones are not implemented in hardware.
void sspSetTriggerMode(SspTriggerMode triggerMode) {
	uint8_t payload[] = { triggerMode };
	sspMethodCall(SspCommandTriggerMode, payload, ARRAY_SIZE(payload));
}

// Reset trackdata and track settings to their default values.
void sspResetToDefaultConfiguration() {
	sspMethodCall(SspCommandDefaultConfiguration, NULL, 0);
}

// send a go, with triggermode set to immediately, this will result in a immediate swipe of the card.
void sspSendGo() {
	sspMethodCall(SspCommandTriggerArm, NULL, 0);
}

// Stop mode: not used for triggermode==immediately
void sspSendStop() {
	sspMethodCall(SspCommandTriggerDisarm, NULL, 0);
}

// Send trackconfig to the probe
void sspSetTrackConfig(int tracknum, SspTrackConfiguration * trackconfig) {
	uint8_t trackconfig_bytes[8];
	trackconfig_bytes[0] = trackconfig->lrcGeneration;
	trackconfig_bytes[1] = 0;
	trackconfig_bytes[2] = 0;
	trackconfig_bytes[3] = 0;
	trackconfig_bytes[4] = 0;
	trackconfig_bytes[5] = 0;
	trackconfig_bytes[6] = 0;
	trackconfig_bytes[7] = trackconfig->manualLrc;
	
	sspMethodCall(SspCommandConfigBase + tracknum, trackconfig, ARRAY_SIZE(trackconfig));
}

// Manually configure the LRC. If you want to do this you will know what to do. By making the most significant bit high, you can send the lrc with a wrong parity bit.
void sspSetManualLrc(int tracknum, uint8_t lrc) {
	SspTrackConfiguration trackConfig = {
		.lrcGeneration = LrcManual,					// The lrc is supplied by the user
		.halfbittime = 0,							// Deprecated
		.prerunZeros = 0,							// Deprecated
		.postrunZeros = 0,							// Deprecated
		.manualLrc = lrc,							// Value of LRC when lrcGeneration == LrcManual
	};
	sspSetTrackConfig(tracknum, &trackConfig);
}

// Reset the track config back to the default. (Is done internally for all tracks on SspCommandDefaultConfiguration as well).
void sspSetTrackConfigDefault(int tracknum) {
	SspTrackConfiguration trackConfig = {
		.lrcGeneration = LrcAuto,					// The lrc is automatically generated (by the SSP)
		.halfbittime = 0,							// Deprecated
		.prerunZeros = 0,							// Deprecated
		.postrunZeros = 0,							// Deprecated
		.manualLrc = 0,								// Value of LRC when lrcGeneration == LrcManual
	};
	sspSetTrackConfig(tracknum, &trackConfig);
}

// Connect to the probe. If serial points to a string "auto" the HID library will select the probe automatically based on USB PID/VID.
void sspConnect(char * serial) {
	if (hid_init()) {
		cleanUpAndExit(ExitErrorHidApi, "Error initializing HID api\n");
	}

	if (strcmp(serial, "auto") == 0) {
		sspHid = hid_open(SSP_VID, SSP_PID, NULL);
		if (sspHid == NULL) {
			cleanUpAndExit(ExitErrorHidOpen, "Error opening HID device (using automatic selection)\n");
		}
	}
	else {
		size_t newsize = strlen(serial) + 1;
		wchar_t * wserial = (wchar_t *)checkMalloc(malloc(newsize * sizeof(wchar_t)));
		mbstowcs(wserial, serial, newsize);
		
		sspHid = hid_open(SSP_VID, SSP_PID, wserial);
		
		free(wserial);
	
		if (sspHid == NULL) {
			cleanUpAndExit(ExitErrorHidOpen, "Error opening HID device (using serial %s)\n", serial);
		}
	}
}
