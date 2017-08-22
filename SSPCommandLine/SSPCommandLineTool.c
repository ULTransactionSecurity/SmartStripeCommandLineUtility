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
#include <stdarg.h>

#include "hidapi/hidapi.h"
#include "SSPCommandLineTool.h"
#include "protocol.h"
#include "util.h"

#define IFNOTQUIET(x)  \
	do {							\
		if (!quietOperation) {		\
			x;						\
		}							\
	} while(0)


// Hack to pull in version number from version.bat
#define set
#define rem
#define UTILITY_VERSION const char ssp_utility_version[]
#include "version.bat"
;
#undef set
#undef rem

commandLineParameter * commandLineParameterList = NULL;
bool quietOperation = false;

// frees the allocated memory for the commandLineParameterList;
static void freeCommandLineParameterList() {
	commandLineParameter * item = commandLineParameterList;
	while (item != NULL) {
		commandLineParameter * nextitem = item->next;
		free(item->parameter);
		free(item->value);
		free(item);
		item = nextitem;
	}

}

// Cleanup allocated memory and exit. In case of error, the errormessage is printed. Allows for printf-style message.
void cleanUpAndExit(int code, char * errorMessage, ...) {
	va_list argptr;
	va_start(argptr, errorMessage);
	if (code != ExitNoError) {
		vfprintf(stderr, errorMessage, argptr);
		fprintf(stderr, "\n");
	}
	va_end(argptr);

	freeCommandLineParameterList();

	exit(code);
}


void parseCommandline(int argc, char *argv[]) {
	commandLineParameter * clplEnd = commandLineParameterList;

	// parse command line arguments, skip #0, because that's the name of the executable, we don't need that.
	for (int i = 1; i < argc; i++) {
		char * workstring = strdup(argv[i]);	// freed at the end of this for loop
		char * token;
		
		commandLineParameter * newParam = checkMalloc(malloc(sizeof(commandLineParameter)));	/// freed in freeCommandLineParameterList	
		// find command
		token = strtok(workstring, "=");
		
		// store command part
		newParam->parameter = strdup(token);	/// freed in freeCommandLineParameterList	
		// find the remainder		
		token = strtok(NULL, "");
		if (token == NULL) {
			newParam->value = strdup("");		/// freed in freeCommandLineParameterList	
		} else {
			// store value part
			newParam->value = strdup(token);	/// freed in freeCommandLineParameterList	
		}
		// set next to NULL, to indicate that this is currently the last item in the list.
		newParam->next = NULL;

		// if clplEnd is NULL, this is the first one, otherwise add it to the end of the list.
		if (clplEnd == NULL) {
			commandLineParameterList = newParam;
		}
		else {			
			clplEnd->next = newParam;
		}
		// so now the newly created item is the tail of the list
		clplEnd = newParam;

		//printf("Parameter %s  value %s\n", newParam->parameter, newParam->value);

		free(workstring);
	}
}

// returns the value of the given command line parameter, returns [_default] when not found. 
// If the parameter is present in the commandline but has no value, an empty string will be returned.
char * getCommandLineParameterValue(char * parameter, char * _default) {
	commandLineParameter * item = commandLineParameterList;
	while (item) {
		if (strcmp(item->parameter, parameter) == 0) {
			return item->value;
		}
		item = item->next;
	}
	return _default;
}
// returns true if a parameter with name [parameter] was present in the commandline
bool getCommandLineParameterPresent(char * parameter) {
	return getCommandLineParameterValue(parameter, NULL) != NULL;
}


void listProbes() {
	struct hid_device_info *devs, *cur_dev;

	// SmartStripe Probes have VID 0x2B2F and PID 0x0001
	devs = hid_enumerate(SSP_VID, SSP_PID);
	cur_dev = devs;

	unsigned int devcount = 0;


	while (cur_dev) {
		cur_dev = cur_dev->next;
		devcount++;
	}
	IFNOTQUIET(printf("%d SmartStripe probe(s) found.\n\n", devcount));	
	cur_dev = devs;
	while (cur_dev) {
		printf("Serial_number: %ls Path: %s\n", cur_dev->serial_number, cur_dev->path);
		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);
}

void printUsage(char * utilityName ) {
	const char optionformat[] = "  %-20s %s";	
	printf("Usage:\n");
	printf("  %s --help\n", utilityName);
	printf("  %s /?\n", utilityName);
	printf("  %s list [-q]\n", utilityName);
	printf("  %s swipe [-q] [--serial=(auto | <SSP serial>)] [--track1=<data>] [--track2=<data>] [--track3=<data>]\n", utilityName);
	printf("\n");
	printf("Commands:\n");
	printf(optionformat, "list",				"List the connected probes\n");
	printf(optionformat, "swipe",				"Lets the probe swipe a card\n");
	printf("\n");
	printf("Options:\n");
	printf(optionformat, "--help, /?",			"Print the usage\n");
	printf(optionformat, "--serial=auto",		"Select the probe using autodetection. When multiple probes are connected, the first one is selected\n");
	printf(optionformat, "--serial=<serial>",	"Select the probe using the given serial number. A list of connected probes can be retrieved using the 'list' command\n");
	printf(optionformat, "--track1=<data>",		"Data for track 1\n");
	printf(optionformat, "--track2=<data>",		"Data for track 2\n");
	printf(optionformat, "--track3=<data>",		"Data for track 3\n");
	printf(optionformat, "-q",					"quiet operation: only output that what is absolutely neccesary\n");

}

// Checking the track data to provide some feedback and suggestions to the user when the trackdata is probably invalid.
void checkTrackData(int tracknum, char * trackcontents) {
	bool warning = false;
	size_t length = strlen(trackcontents);
	
	// First byte should be a start sentinel. Track 1, in alpha format should have a percent sign as start sentinel, where the other tracks have a semicolon as start sentinel.
	char expected_start_byte;
	// The end sentinel is always a '?'
	char expected_end_byte = '?';
	char expected_separator;
	unsigned int maxcharactercount;
	uint8_t min;
	uint8_t max;
	
	switch (tracknum) {
	case 1:
		expected_start_byte = '%';
		expected_separator = '^';
		min = 0x20;
		max = 0x5f;
		maxcharactercount = 79;
		break;
	case 2:
		expected_start_byte = ';';
		expected_separator = '=';
		min = 0x30;
		max = 0x3f;
		maxcharactercount = 40;
		break;
	case 3:
		expected_start_byte = ';';
		expected_separator = '=';
		min = 0x30;
		max = 0x3f;
		maxcharactercount = 107;
		break;
	}

	// Empty tracks are permitted, the SSP just sends nothing.
	IFNOTQUIET(printf("Checking track %d data for validity:\n", tracknum));
	if (length == 0) {
		IFNOTQUIET(printf("-- OK: Empty track.\n"));
		return;
	}

	for (size_t i = 0; i < length; i++) {
		if (!((trackcontents[i] >= min) && (trackcontents[i] <= max))) {
			printf("-- Error: Invalid character at position %zu: '%c' should be within range [%d,%d].\n",
				i + 1, trackcontents[i], min, max);
			return;
		}
	}
	
	if (length > maxcharactercount) {
		IFNOTQUIET(printf("-- Warning: your track is probably too long (%zu, where %d is permitted).\n", length, maxcharactercount));
	}

	if (trackcontents[0] != expected_start_byte) {
		IFNOTQUIET(printf("-- Warning: track starts with a '%c' instead of '%c'. A cardreader will probably refuse to read this track.\n", trackcontents[0], expected_start_byte));
		warning = true;
	}

	if (trackcontents[length-1] != expected_end_byte) {
		IFNOTQUIET(printf("-- Warning: track ends with a '%c' instead of '%c'. A cardreader will probably refuse to read this track.\n", trackcontents[length - 1], expected_end_byte));
		warning = true;
	}

	if (strchr(trackcontents, expected_separator) == NULL) {
		IFNOTQUIET(printf("-- Note: track does not contain any separators ('%c').\n", expected_separator));
	}

	if (!warning) {
		IFNOTQUIET(printf("-- OK: Looks like a readable track.\n"));
	}

}

int swipeCard() {
	char * serial = getCommandLineParameterValue("--serial", "");
	if (strcmp(serial, "auto") == 0) {
		IFNOTQUIET(printf("Connecting to probe using autodetection\n"));
	}
	else {
		IFNOTQUIET(printf("Connecting to probe with serialnumber %s\n", serial));
	}
	sspConnect(serial);
	// wipe any configuration traces from a previous run
	sspResetToDefaultConfiguration();
	// Retrieve firmware version:
	SspFirmwareVersion version = sspGetFirmwareVersion();
	IFNOTQUIET(printf("Firmware version: %d.%d Bootloader %d.%d\n\n", version.firmwareMajor, version.firmwareMinor, version.bootloaderMajor, version.bootloaderMinor));
	
	char *track1 = getCommandLineParameterValue("--track1", "");
	char *track2 = getCommandLineParameterValue("--track2", "");
	char *track3 = getCommandLineParameterValue("--track3", "");

	IFNOTQUIET(printf("Card data:\n"));
	IFNOTQUIET(printf("\tTrack 1: %s\n", track1));
	IFNOTQUIET(printf("\tTrack 2: %s\n", track2));
	IFNOTQUIET(printf("\tTrack 3: %s\n", track3));

	checkTrackData(1, track1);
	checkTrackData(2, track2);
	checkTrackData(3, track3);

	IFNOTQUIET(printf("\nSwiping card...\n"));
	// Send the contents of the tracks
	sspSetTrackDataString(1, track1, strlen(track1));
	sspSetTrackDataString(2, track2, strlen(track2));
	sspSetTrackDataString(3, track3, strlen(track3));

	// Set trigger mode immediately
	sspSetTriggerMode(SspTriggerModeImmediately);
	// Arm the trigger (because we just set the trigger mode to immediately, the swipe will be fired and we return to stop mode);
	sspSendGo();	

	return 0;
}

int main(int argc, char *argv[]) {

	parseCommandline(argc, argv);
	if (getCommandLineParameterPresent("-q")) {
		quietOperation = true;
	}

	IFNOTQUIET(printf("SmartStripeProbe Command line utility (C) 2017 UL TS B.V. Version %s\n\n", ssp_utility_version));

	if (getCommandLineParameterPresent("list")) {
		listProbes();
	} else if (getCommandLineParameterPresent("swipe") && getCommandLineParameterPresent("--serial")) {
		swipeCard();
	} else { 
		printUsage(argv[0]);
	}
	cleanUpAndExit(0, "Finished");


}

