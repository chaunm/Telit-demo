/*
 * ATCommand.c
 *
 *  Created on: Jun 30, 2016
 *      Author: ChauNM
 */
#include <string.h>
#include "universal.h"
#include "GsmModem.h"
#include "ATCommand.h"
#include "serialcommunication.h"
#include "queue.h"

BYTE g_atModemStatus;


ATHANDLEFN atHandleIncomingProc = NULL;
void* atHandleIncomingParam = NULL;

static BOOL atValidateIncomingMessage(PBYTE pData, WORD nSize)
{
	if ((pData[0] != '\r') || (pData[1] != '\n')
		|| (pData[nSize - 2] != '\r') || (pData[nSize - 1] != '\n'))
		return FALSE;
	return TRUE;
}

VOID atRegisterIncommingProc(void (*function)(void* pParam, char* message), void* param)
{
	atHandleIncomingProc = function;
	atHandleIncomingParam = param;
}

VOID atProcessInputByte(BYTE nData, PWORD index, PBYTE pReceiveBuffer, PSERIAL pSerialPort)
{
	if (((*index == 0) && (nData != '\r'))
		|| ((*index == 1) && (nData != '\n')))
	{
		*index = 0;
		return;
	}
	//get data
	pReceiveBuffer[*index] = nData;
	// check if message finish
	if (((*index) > 3) && (nData == '\n'))
	{
		if (pReceiveBuffer[*index - 1] == '\r')
		{
			// message finished
			QueuePush((void *)pReceiveBuffer, *index + 1, pSerialPort->pInputQueue);
			*index = 0;
			return;
		}
	}
	*index = *index + 1;
	if (*index  == MAX_SERIAL_PACKAGE_SIZE)
		*index = 0;
	return;
}


VOID atHandleMessage(PBYTE pInData, WORD nSize)
{
	WORD index;
	PBYTE pData;
	if ((pInData[0] != '\r') && (pInData[1] != '\n' ))
	{
		printf("process message with echoes\n");
		for(index = 1; index <  nSize; index++)
		{
			if((pInData[index] == 0x0A) && (pInData[index - 1] == 0x0D)
				&& (pInData[index + 2] == 0x0A) && (pInData[index + 1] == 0x0D))
			{
				pData = pInData + index + 1;
				nSize = nSize - index - 1;
				break;
			}
		}
	}
	else
		pData = pInData;
	if (atValidateIncomingMessage(pData, nSize) == FALSE)
		return;
	// counting number of message
	char* messageContent = malloc(nSize - 3);
	BYTE messageCount = 1;
	WORD messageIndex;
	WORD messageStart[5];
	WORD messageStop[5];
	messageStart[0] = 2;
	for (index = 2; index < nSize - 2; index++)
	{
		if((pData[index] == 0x0A) && (pData[index - 1] == 0x0D)
				&& (pData[index - 2] == 0x0A) && (pData[index - 3] == 0x0D))
		{
			messageStop[messageCount - 1] = index - 3;
			messageStart[messageCount] = index + 1;
			messageCount++;
		}
	}
	messageStop[messageCount - 1] = nSize - 2;
	for (messageIndex = 0; messageIndex < messageCount; messageIndex++)
	{
		memset(messageContent, 0, nSize - 3);
		for (index = messageStart[messageIndex]; index < messageStop[messageIndex]; index++)
		{
			messageContent[index - messageStart[messageIndex]] = pData[index];
		}
		if (atHandleIncomingProc != NULL)
			atHandleIncomingProc(atHandleIncomingParam, messageContent);
	}
	free(messageContent);

}

void atHandleCgcontrdpEvent(char* message)
{
	WORD index;
	WORD numberIndex = 0;
	BYTE colonCount = 0;
	BYTE pointCount = 0;
	BYTE firstComma = 0;
	if (message == NULL) return;
	for (index = 0; index < strlen(message); index++)
		if (message[index] == ',')
		{
			firstComma = index;
			break;
		}
	if (message[firstComma - 1] != '4')
		return;
	char* ipAddress = malloc(16);
	memset(ipAddress, 0, 16);
	char* gateway = malloc(16);
	memset(gateway, 0, 16);
	char* nameServer = malloc(16);
	memset(nameServer, 0, 16);
	for (index = 0; index < strlen(message); index++)
	{
		if (colonCount == 3)
		{
			if (message[index] != '"')
			{
				if (message[index] == '.')
					pointCount++;
				if(pointCount < 4)
				{
					ipAddress[numberIndex] = message[index];
					numberIndex++;
				}
			}
			else
			{
				printf("ip address %s:\n", ipAddress);
				GsmModemSetIpAddress(ipAddress);
				numberIndex = 0;
			}
		}
		if (colonCount == 5)
		{
			if (message[index] != '"')
			{
				gateway[numberIndex] = message[index];
				numberIndex++;
			}
			else
			{
				printf("gateway %s:\n", ipAddress);
				GsmModemSetGateway(gateway);
				numberIndex = 0;
			}
		}
		if (colonCount == 7)
		{
			if (message[index] != '"')
			{
				nameServer[numberIndex] = message[index];
				numberIndex++;
			}
			else
			{
				printf("name server %s:\n", ipAddress);
				GsmModemSetNameServer(nameServer);
				numberIndex = 0;
				break;
			}
		}
		if (message[index] == '"')
			colonCount++;
	}
	free(ipAddress);
	free(gateway);
	free(nameServer);
}

VOID atSendCommand(char* command, PSERIAL serialPort)
{
	char* outCommand = malloc(strlen(command));
	strcpy(outCommand, command);
	strcat(outCommand, "\r\n");
	SerialOutput(serialPort, (PBYTE)outCommand, strlen(outCommand));
	free(outCommand);
}

