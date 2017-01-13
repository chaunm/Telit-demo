/*
 ============================================================================
 Name        : ZigbeeHost.c
 Author      : ChauNM
 Version     :
 Copyright   :
 Description : C Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "serialcommunication.h"
#include "universal.h"
#include "GsmModem.h"

void PrintHelpMenu() {
	printf("program: ZigbeeHostAMA\n"
			"using ./ZigbeeHostAMA --serial []");
}

int main(int argc, char* argv[])
{
	/* get option */
	int opt = 0;
	char *SerialPort = NULL;
	WORD ttl = 0;

	// specific the expected option
	static struct option long_options[] = {
			{"serial",  required_argument, 0, 's' },
	};
	int long_index;
	/* Process option */
	while ((opt = getopt_long(argc, argv,":hs:",
			long_options, &long_index )) != -1) {
		switch (opt) {
		case 'h' :
			PrintHelpMenu();
			return EXIT_SUCCESS;
			break;
		case 's' :
			SerialPort = StrDup(optarg);
			break;
		default:
			break;
		}
	}
	if (SerialPort == NULL)
	{
		printf("invalid options, using -h for help\n");
		return EXIT_FAILURE;
	}
	/* All option valid, start program */

	puts("Program start");
	// GsmModem
	if (GsmModemInit(SerialPort, ttl) == FALSE)
		exit(0);
	return EXIT_SUCCESS;
}
