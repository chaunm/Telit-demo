/*
 * serialcommunication.c

 *
 *  Created on: Jan 14, 2016
 *      Author: ChauNM
 */
#include "serialcommunication.h"
#include "queue.h"
#include "ATCommand.h"

//static BYTE 	g_pReceivePackage[MAX_SERIAL_PACKAGE_SIZE];
static WORD 	g_nPackageIndex;

/*
 * Function: SerialOpen(unsigned int uiBaudrate)
 * Description:
 * 	- Open the serial port with the port name is given by typing
 * 	- Giving input and output queue for data read and write
 * Input:
 * 	- uiBaudrate: Baudrate of the port.
 * Return:
 * 	- pointer to serial management data struct.
 */
PSERIAL SerialOpen(char* PortName, unsigned int uiBaudrate)
{
	struct termios stSerialIo;
	PSERIAL pSerialPort = (PSERIAL)malloc(sizeof(SERIAL));
	pSerialPort->tty_fd = -1;
	/* Configure the port */
	memset(&stSerialIo, 0, sizeof(stSerialIo));
	stSerialIo.c_iflag = 0; // raw input
	stSerialIo.c_oflag = 0; // raw output
	stSerialIo.c_cflag = 0;
	stSerialIo.c_lflag = 0;
	stSerialIo.c_cc[VMIN] = 1;
	stSerialIo.c_cc[VTIME] = 0;
	/* configure control flag
	 * CS8: 8bit, 1 bit stop, no parity
	 */
	stSerialIo.c_cflag = uiBaudrate | CS8 | CREAD | CLOCAL;
	/* open port */
	pSerialPort->tty_fd = open(PortName, O_RDWR | O_NOCTTY | O_NDELAY);
	//pSerialPort->tty_fd = open(PortName, O_RDWR | O_NOCTTY);
	if (pSerialPort->tty_fd < 0)
	{
		return NULL;
	}
	/* set configuration to be activated */
	cfsetospeed(&stSerialIo, uiBaudrate);
	cfsetispeed(&stSerialIo, uiBaudrate);
	tcsetattr(pSerialPort->tty_fd, TCSANOW, &stSerialIo);
	char* pReadBuffer = malloc(255);
	while(read(pSerialPort->tty_fd, pReadBuffer, 255) > 0);
	free(pReadBuffer);
	g_nPackageIndex = 0;
	/* initialize queue for communication */
	pSerialPort->pInputQueue = QueueCreate(SERIAL_QUEUE_SIZE, MAX_SERIAL_PACKAGE_SIZE);
	pSerialPort->pOutputQueue = QueueCreate(SERIAL_QUEUE_SIZE, MAX_SERIAL_PACKAGE_SIZE);
	pSerialPort->uiBaudrate = uiBaudrate;
	return pSerialPort;
}

/*
 * Function SerialClose
 * Description:
 * 	- Close the serial port and free related data.
 * Input:
 * 	- Serial management data struct.
 * Return:
 * 	- NULL
 */
VOID SerialClose(PSERIAL pSerialPort)
{
	printf("close serial port \n");
	QueueFreeMem(pSerialPort->pInputQueue);
	QueueFreeMem(pSerialPort->pOutputQueue);
	close(pSerialPort->tty_fd);
	free((void*)pSerialPort);
}

/* Function SerialHandleIncomingByte(PSERIAL pSerialPort, BYTE byData)
 * Description:
 * 	- Handle read data byte from serial input using ZNP protocol
 * Input:
 * 	- pSerialPort: Serial management data struct.
 * 	- byData: Data read from input buffer.
 */
/*
static VOID SerialHandleIncomingByte(PSERIAL pSerialPort, BYTE byData)
{
	BYTE nIndex;
	if ((g_nPackageIndex == 0) && (byData == 0xFE))
	{
		g_pReceivePackage[g_nPackageIndex] = 0xFE;
		g_nPackageIndex++;
	}
	else if (g_nPackageIndex == (g_pReceivePackage[1] + 4))
	{
		BYTE byCRC = 0;
		unsigned int nContentLength = g_pReceivePackage[1] + 3;
		for (nIndex = 1; nIndex <= nContentLength; nIndex++)
		{
			byCRC ^= g_pReceivePackage[nIndex];
		}
		if (byCRC == byData)
		{
			g_pReceivePackage[g_nPackageIndex] = byData;
			g_nPackageIndex++;
			// push validated data to queue
			QueuePush((void *)g_pReceivePackage, g_nPackageIndex, pSerialPort->pInputQueue);
			g_nPackageIndex = 0;
		}
		else
		{
			g_nPackageIndex = 0;
			printf("Invalid package received");
		}
	}
	else
	{
		g_pReceivePackage[g_nPackageIndex] = byData;
		g_nPackageIndex++;
	}

}
*/
/* Function SerialHandleIncomingBuffer(PSERIAL pSerialPort, PBYTE pByffer, BYTE nSize)
 * Description:
 * 	- Handle read data stream from serial port
 * Input:
 * 	- pSerialPort: Serial management data struct.
 * 	- pBuffer: pointer to the data buffer.
 * 	- nSize: Size of data in buffer.
 */
/*
static VOID SerialHandleIncomingBuffer(PSERIAL pSerialPort, PBYTE pBuffer, BYTE nSize)
{
	BYTE nReceiveIndex;
	for (nReceiveIndex = 0; nReceiveIndex < nSize; nReceiveIndex++)
	{
		//SerialHandleIncomingByte(pSerialPort, pBuffer[nReceiveIndex]);
		atProcessInputByte(pBuffer[nReceiveIndex], &g_nPackageIndex, g_pReceivePackage, pSerialPort);
	}

}
*/
/* Function SerialProcessIncomingData(PSERIAL pSerialPort)
 * Description:
 * 	- Handle read data from serial port
 * Input:
 * 	- pSerialPort: Serial management data struct.
 */
VOID SerialProcessIncomingData(PSERIAL pSerialPort)
{
	PBYTE pReceiveBuffer = (PBYTE)malloc(MAX_SERIAL_PACKAGE_SIZE);
	ssize_t byReceiveByte;
	PBYTE pReadBuffer = pReceiveBuffer;
	ssize_t total = 0;
	while(1)
	{
		byReceiveByte = read(pSerialPort->tty_fd, pReadBuffer, MAX_SERIAL_PACKAGE_SIZE);
		if (byReceiveByte > 0)
		{
			total = byReceiveByte;
			pReadBuffer += byReceiveByte;
		}
		while (byReceiveByte > 0)
		{
			//SerialHandleIncomingBuffer(pSerialPort, pReceiveBuffer, byReceiveByte);
			usleep(3000);
			byReceiveByte = read(pSerialPort->tty_fd, pReadBuffer, MAX_SERIAL_PACKAGE_SIZE);
			if (byReceiveByte > 0)
			{
				total += byReceiveByte;
				pReadBuffer += byReceiveByte;
			}
		}
		if (total > 0)
		{
			QueuePush(pReceiveBuffer, total, pSerialPort->pInputQueue);
			memset(pReceiveBuffer, 0, MAX_SERIAL_PACKAGE_SIZE);
			pReadBuffer = pReceiveBuffer;
			total = 0;
		}
		//if (pReceiveBuffer != NULL)
			//free((void*)pReceiveBuffer);
		usleep(10000);
	}
	if (pReceiveBuffer != NULL)
		free(pReceiveBuffer);
}


/* Function SerialOutputDataProcess(PSERIAL pSerialPort)
 * Description:
 * 	- Write data to serial port.
 * Input:
 * 	- pSerialPort: Serial management data struct.
 */
void SerialOutputDataProcess(PSERIAL pSerialPort)
{
	QUEUECONTENT stOutputContent;
	WORD nIndex;
	while(1)
	{
		if (QueueGetState(pSerialPort->pOutputQueue) == QUEUE_ACTIVE)
		{
			stOutputContent = QueueGetContent(pSerialPort->pOutputQueue);
			if (stOutputContent.nSize > 0)
			{
				//print data for debugging purpose
				printf("<< ");
				for (nIndex = 0; nIndex < stOutputContent.nSize; nIndex++)
					printf("0x%02X ", stOutputContent.pData[nIndex]);
				printf("\n");
				write(pSerialPort->tty_fd, (void*)(stOutputContent.pData), stOutputContent.nSize);
				QueueFinishProcBuffer(pSerialPort->pOutputQueue);
			}
		}
		usleep(1000);
	}
}

/* Function SerialInputDataProcess(PSERIAL pSerialPort)
 * Description:
 * 	- Process data in the input queue.
 * Input:
 * 	- pSerialPort: Serial management data struct.
 */
void SerialInputDataProcess(PSERIAL pSerialPort)
{
	QUEUECONTENT stInputContent;
	WORD nIndex;
	while(1)
	{
		if (QueueGetState(pSerialPort->pInputQueue) == QUEUE_ACTIVE)
		{
			stInputContent = QueueGetContent(pSerialPort->pInputQueue);
			if (stInputContent.nSize > 0)
			{
				// print data for debugging purpose
				printf(">> ");
				for (nIndex = 0; nIndex < stInputContent.nSize; nIndex++)
				{
					printf("0x%02X ", stInputContent.pData[nIndex]);
				}
				printf("\n");
				// put handler function here
				//ZnpHandleCommand(stInputContent.pData, stInputContent.nSize);
				atHandleMessage(stInputContent.pData, stInputContent.nSize);
				QueueFinishProcBuffer(pSerialPort->pInputQueue);
			}
		}
		usleep(1000);
	}
}

/* Function SerialOutput(PSERIAL pSerialPort, PBYTE pData, BYTE nSize)
 * Description:
 * 	- Write data to serial port's output queue.
 * Input:
 * 	- pSerialPort: Serial management data struct.
 * 	- pData: Pointer to data buffer.
 * 	- nSize: Size of data.
 */
BYTE SerialOutput(PSERIAL pSerialPort, PBYTE pData, WORD nSize)
{
	return QueuePush(pData, nSize, pSerialPort->pOutputQueue);
}
