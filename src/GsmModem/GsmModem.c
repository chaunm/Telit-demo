/*
 * GsmModem.c
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include "GsmModem.h"
#include "universal.h"
#include "ATCommand.h"
#include "serialcommunication.h"

PGSMMODEM gsmModem = NULL;
WORD gsmCheckBillingCount;
BYTE gsmRestartCount;


static VOID GsmSetRestart(BYTE nCount)
{
	gsmRestartCount = nCount;
}

BYTE GsmModemGetStatus()
{
	return gsmModem->status;
}

VOID GsmModemSetStatus(char status, char* command)
{
	gsmModem->status = status;
	char* newCommand = StrDup(command);
	if (gsmModem->waitingCommand != NULL)
	{
		free(gsmModem->waitingCommand);
		gsmModem->waitingCommand = NULL;
	}
	gsmModem->waitingCommand = newCommand;
}

VOID GsmModemSetCmdStatus(char* commandStatus)
{
	//free old status
	char* newCmdStatus = StrDup(commandStatus);
	if (gsmModem->commandStatus != NULL)
	{
		free(gsmModem->commandStatus);
		gsmModem->commandStatus = NULL;
	}
	gsmModem->commandStatus = newCmdStatus;
}

BYTE GsmModemExecuteCommand(char* command)
{
	WORD timeout;
	if (gsmModem->status == GSM_MODEM_OFF)
		return COMMAND_ERROR;
	while(gsmModem->status != GSM_MODEM_ACTIVE)
	{
		sleep(1);
	}
	GsmModemSetCmdStatus(NULL);
	atSendCommand(command, gsmModem->serialPort);
	GsmModemSetStatus(GSM_MODEM_WAITING, command);
	timeout = 0;
	char* eventMessage = malloc(50);
	memset(eventMessage, 0, 50);
	while (gsmModem->status == GSM_MODEM_WAITING)
	{
		usleep(100000);
		timeout++;
		if (timeout == GSM_COMMAND_TIMEOUT)
		{
			GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
			free(eventMessage);
			return COMMAND_TIMEOUT;
		}
	}
	if (gsmModem->status == GSM_MODEM_CMD_ERROR)
	{
		printf("command %s failed\n", gsmModem->waitingCommand);
		sprintf(eventMessage, "command.%s.error", gsmModem->waitingCommand);
		GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
		free(eventMessage);
		return COMMAND_ERROR;
	}
	free(eventMessage);
	return COMMAND_SUCCESS;
}

// Day la ham duoc goi trong vong quet buffer nhan
char* GsmModemHandleCommandMessage(char* command, char* message)
{
	char* commandStatus = NULL;
	printf("command %s, status %s\n", command, message);
	// command = NULL mean event from device
	commandStatus = StrDup(message);
	if (command == NULL)
	{
		// get incoming call information
		if (strstr(message, "+CGCONTRDP:"))
		{
			atHandleCgcontrdpEvent(message);
		}
	}
	return commandStatus;
}

VOID GsmModemProcessIncoming(void* pParam, char* message)
{
	PGSMMODEM gsmModem = (PGSMMODEM)pParam;
	if ((strcmp(message, "OK") == 0))
	{
		printf("command %s execute ok\n", gsmModem->waitingCommand);
		GsmModemSetStatus(GSM_MODEM_ACTIVE, NULL);
	}
	else if (strcmp(message, "ERROR") == 0)
	{
		printf("command %s execute failed\n", gsmModem->waitingCommand);
		GsmModemSetStatus(GSM_MODEM_CMD_ERROR, gsmModem->waitingCommand);
		GsmModemSetCmdStatus(message);
	}
	else
	{
		char* commandStatus = GsmModemHandleCommandMessage(gsmModem->waitingCommand, message);
		if(commandStatus != NULL)
		{
			GsmModemSetCmdStatus(commandStatus);
			//CME ERROR Report
			if (strstr(message, "+CME ERROR:") || strstr(message, "+CMS ERROR:"))
			{
				GsmModemSetStatus(GSM_MODEM_CMD_ERROR, gsmModem->waitingCommand);
			}
			free(commandStatus);
		}
	}
}

BOOL GsmModemCheckCarrierRegister()
{
	char* currentCarrier = NULL;
	WORD index;
	BYTE carrierStart = 0;
	if(GsmModemExecuteCommand("AT+COPS?") == COMMAND_SUCCESS)
	{
		currentCarrier = malloc(50);
		memset(currentCarrier, 0, 50);
		for (index = 0; index < strlen(gsmModem->commandStatus); index++)
		{
			if ((carrierStart > 0) && (gsmModem->commandStatus[index] != '"'))
			{
				currentCarrier[index - carrierStart] = gsmModem->commandStatus[index];
			}
			if (gsmModem->commandStatus[index] == '"')
			{
				if(carrierStart == 0)
					carrierStart = index + 1;
				else
					break;
			}
		}
		if (strlen(currentCarrier) == 0)
		{
			free(currentCarrier);
			return TRUE;
		}
		if (gsmModem->carrier != NULL)
		{
			if (strcmp(gsmModem->carrier, currentCarrier) != 0)
			{
				free(gsmModem->carrier);
				gsmModem->carrier = StrDup(currentCarrier);
				// publish
				//GsmActorPublishGsmCarrier(currentCarrier);
			}
		}
		else
		{
			gsmModem->carrier = StrDup(currentCarrier);
		}
		printf("carrier: %s", currentCarrier);
		free(currentCarrier);
		return TRUE;
	}
	else
	{
		if(gsmModem->carrier != NULL)
		{
			free(gsmModem->carrier);
			gsmModem->carrier = NULL;
		}
		return FALSE;
	}
}

PGSMMODEM GsmGetInfo()
{
	return gsmModem;
}

void GsmModemSetIpAddress(char* ipAddress)
{
	gsmModem->ipAddress = StrDup(ipAddress);
}

void GsmModemSetGateway(char* gateway)
{
	gsmModem->gateway = StrDup(gateway);
}

void GsmModemSetNameServer(char* nameServer)
{
	gsmModem->nameServer = StrDup(nameServer);
}

BOOL GsmModemInit(char* SerialPort, int ttl)
{
	static pthread_t SerialProcessThread;
	static pthread_t SerialOutputThread;
	static pthread_t SerialHandleThread;
	BYTE bCommandState = COMMAND_SUCCESS;
	char* command = malloc(255);
	// open serial port
	char* PortName = malloc(strlen("/dev/") + strlen(SerialPort) + 1);
	memset(PortName, 0, strlen("/dev/") + strlen(SerialPort) + 1);
	sprintf(PortName, "%s%s", "/dev/", SerialPort);
	printf("open port %s\n", PortName);
	// open serial port for communication with gsmModem
	PSERIAL pSerialPort = SerialOpen(PortName, B57600);
	if (pSerialPort == NULL)
	{
		printf("Can not open serial port %s, try another port\n", PortName);
		GsmSetRestart(5);
		return FALSE;
	}
	free(PortName);
	printf("port opened\n");
	pthread_create(&SerialProcessThread, NULL, (void*)&SerialProcessIncomingData, (void*)pSerialPort);
	pthread_create(&SerialOutputThread, NULL, (void*)&SerialOutputDataProcess, (void*)pSerialPort);
	pthread_create(&SerialHandleThread, NULL, (void*)&SerialInputDataProcess, (void*)pSerialPort);
	sleep(1);
	// Create gsm device
	gsmModem = malloc(sizeof(GSMMODEM));
	memset(gsmModem, 0, sizeof(GSMMODEM));
	gsmModem->serialPort = pSerialPort;
	atRegisterIncommingProc(GsmModemProcessIncoming, (void*)gsmModem);
	// start up commands
	atSendCommand("AT", gsmModem->serialPort); //send this command to flush all data from buffer
	sleep(1);
	atSendCommand("ATE0", gsmModem->serialPort);
	sleep(10);

	bCommandState += GsmModemCheckCarrierRegister();
	if (bCommandState != COMMAND_SUCCESS)
	{
		printf ("No network\n");
		return FALSE;
	}
	if (strstr(gsmModem->carrier, "VINAPHONE"))
		gsmModem->apn = StrDup("m3-world");
	if (strstr(gsmModem->carrier, "MOBIFONE"))
		gsmModem->apn = StrDup("m-wap");
	if (strstr(gsmModem->carrier, "VIETTEL"))
		gsmModem->apn = StrDup("v-internet");
	memset(command, 0, 255);
	sprintf(command, "aAT+CGDCONT=4,\"IP\",\"%s\"", gsmModem->apn);
	bCommandState += GsmModemExecuteCommand(command);
	bCommandState += GsmModemExecuteCommand("AT#NCM=1,4");
	bCommandState += GsmModemExecuteCommand("AT+CGACT=1,4");
	bCommandState += GsmModemExecuteCommand("AT+CGCONTRDP=");
	GsmModemExecuteCommand("AT+CGDATA=\"M-RAW_IP\",4");
	if (bCommandState != COMMAND_SUCCESS)
	{
		printf("start gsm Modem failed\n");
		return FALSE;
	}
	printf("finish initialize modem\n");
	printf("init network interface\n");
	memset(command, 0, 255);
	sprintf(command, "ifconfig usb0 %s netmask 255.255.255.0 up", gsmModem->ipAddress);
	system(command);
	memset(command, 0, 255);
	sprintf(command, "route add default gw %s", gsmModem->gateway);
	system(command);
	memset(command, 0, 255);
	sprintf(command, "ip neigh add %s lladdr 11:22:33:44:55:66 nud permanent dev usb0", gsmModem->gateway);
	system(command);
	free(command);
	printf("init network interface done\n");
	return TRUE;
}

