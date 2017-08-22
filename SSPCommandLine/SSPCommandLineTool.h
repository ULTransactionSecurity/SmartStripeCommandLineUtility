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
#ifndef SSPCOMMANDLINEC_H
#define SSPCOMMANDLINEC_H

#include <stdint.h>
#include <stdbool.h>

#define SSP_VID	0x2B2F
#define SSP_PID	0x0001
#define DLE		0x10
#define STX		0x02
#define ETX		0x03

typedef struct commandLineParameter_s commandLineParameter;

struct commandLineParameter_s {
	char * parameter;
	char * value;
	commandLineParameter * next;
};

// Return codes of the utility (ERRORLEVEL / $?)
typedef enum {
	ExitNoError = 0,
	ExitErrorHidApi = -1,
	ExitErrorHidOpen = -2,
	ExitErrorCommunicationProtocol = -3,
	ExitErrorResponseParsing = -4,
	ExitErrorCommandLineParameter = -5,
} ExitCode;

void cleanUpAndExit(int code, char * errorMessage, ...);

#endif /*not defined SSPCOMMANDLINEC_H */
