#define _DEFAULT_SOURCE

#include <inttypes.h>
#include <endian.h>
#include "socket.h"

static int openU64Socket(programData *data, int port)
{
	IPaddress ip;

	data->tcpSet = SDLNet_AllocSocketSet(1);

	if(SDLNet_ResolveHost(&ip, data->hostName, port)) {
		fprintf(stderr, "Error resolving '%s' : %s\n", data->hostName, SDLNet_GetError());
		SDLNet_FreeSocketSet(data->tcpSet);
		return EXIT_FAILURE;
	}

	data->tcpSock = SDLNet_TCP_Open(&ip);
	if(!data->tcpSock) {
		fprintf(stderr, "Error connecting to '%s' : %s\n", data->hostName, SDLNet_GetError());
		SDLNet_FreeSocketSet(data->tcpSet);
		return EXIT_FAILURE;
	}

	SDLNet_TCP_AddSocket(data->tcpSet, data->tcpSock);
	SDL_Delay(1);

	return EXIT_SUCCESS;
}

static void closeU64Socket(programData *data)
{
	if (data->tcpSock) {
		SDLNet_TCP_Close(data->tcpSock);
	}
	if (data->tcpSet) {
		SDLNet_FreeSocketSet(data->tcpSet);
	}
}

static int sendSequence(programData *prgData, const uint8_t *data, int len, int prepare)
{
	uint8_t buf[TCP_BUFFER_SIZE];
	int result = 0;

	if (prepare) {
		if (openU64Socket(prgData, TELNET_PORT) != EXIT_SUCCESS) {
			return EXIT_FAILURE;
		}
	}

	SDL_Delay(COMMAND_DELAY);

	for(int i=0; i < len; i++) {
		SDL_Delay(1);

		if (unlikely(prgData->verbose)) {
			printf("%02x ", data[i]);
		}

		result = SDLNet_TCP_Send(prgData->tcpSock, &data[i], sizeof(uint8_t));
		if(result < (int)sizeof(uint8_t)) {
			fprintf(stderr, "Error sending command data: %s\n", SDLNet_GetError());
			SDLNet_TCP_Close(prgData->tcpSock);
			SDLNet_FreeSocketSet(prgData->tcpSet);
			return EXIT_FAILURE;
		}
		// Empty u64 send buffer
		while( SDLNet_CheckSockets(prgData->tcpSet, SDLNET_TIMEOUT) == 1 ) {
			result = SDLNet_TCP_Recv(prgData->tcpSock, &buf, TCP_BUFFER_SIZE - 1);
			buf[result]=0;
		}
	}

	if (prepare) {
		closeU64Socket(prgData);
	}

	return EXIT_SUCCESS;
}

static int sendPacket(programData *prgData)
{
	int result = 0;

	if (prgData->verbose) {
		for (int i = 0; i < prgData->dmaFileSize; i++) {
			printf("%02x ", prgData->dmaAddress[i]);
		}
	}

	result = SDLNet_TCP_Send(prgData->tcpSock, prgData->dmaAddress, prgData->dmaFileSize);
	if(result < prgData->dmaFileSize) {
		fprintf(stderr, "Error sending command data: %s\n", SDLNet_GetError());
		SDLNet_TCP_Close(prgData->tcpSock);
		SDLNet_FreeSocketSet(prgData->tcpSet);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int sendCommand(programData *prgData, const uint16_t *data, int len, int prepare)
{
	int result = 0;

	if (prepare) {
		if (openU64Socket(prgData, COMMAND_PORT) != EXIT_SUCCESS) {
			return EXIT_FAILURE;
		}
	}

	SDL_Delay(COMMAND_DELAY);

	for (int i = 0; i < len; i++) {
		if (unlikely(prgData->verbose)) {
			printf("sending: %04x\n", data[i]);
		}

		result = SDLNet_TCP_Send(prgData->tcpSock, &data[i], len);
		if(result < len) {
			fprintf(stderr, "Error sending command data: %s\n", SDLNet_GetError());
			closeU64Socket(prgData);
			return EXIT_FAILURE;
		}
		SDL_Delay(1);
	}

	if (prepare) {
		closeU64Socket(prgData);
	}

	return EXIT_SUCCESS;
}

int runCommand(programData *prgData, command cmd, int prepare)
{
	int result = 0;
	const uint16_t *cmdData = NULL;
	char *infoString = NULL;
	int size = 0;

	// data in little endian
	const uint16_t startData[] = {
		htole16(SOCKET_CMD_VICSTREAM_ON),
		0x0000,
		htole16(SOCKET_CMD_AUDIOSTREAM_ON),
		0x0000
	};

	const uint16_t stopData[] = {
		htole16(SOCKET_CMD_VICSTREAM_OFF),
		0x0000,
		htole16(SOCKET_CMD_AUDIOSTREAM_OFF),
		0x0000
	};

	const uint16_t resetData[] = {
		htole16(SOCKET_CMD_RESET),
		0x0000
	};

	switch(cmd) {
		case CMD_START_STREAM:
			infoString = "start stream";
			cmdData = startData;
			size = sizeof(startData) / sizeof(startData[0]);
			prgData->isStreaming=1;
			break;
		case CMD_STOP_STREAM:
			infoString = "stop stream";
			cmdData = stopData;
			size = sizeof(stopData) / sizeof(stopData[0]);
			prgData->isStreaming=0;
			break;
		case CMD_RESET:
			infoString = "reset";
			cmdData = resetData;
			size = sizeof(resetData) / sizeof(resetData[0]);
			break;
		default:
			return EXIT_FAILURE;
	}

	printf("Sending %s command to Ultimate64...\n", infoString);

	result = sendCommand(prgData, cmdData, size, prepare);
	if (result != EXIT_SUCCESS) {
		return result;
	}

	printf("  * done.\n");
	return EXIT_SUCCESS;
}

int sendFile(programData *data)
{
	// s.mysend(pack("<H", 0xFF02))
	// s.mysend(pack("<H", len(bytes)))
	// s.mysend(bytes)

	uint16_t sendCmd[] = {
		htole16(SOCKET_CMD_DMARUN),
		htole16(data->dmaFileSize)
	};

	if (openU64Socket(data, COMMAND_PORT) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	// Send command and filesize
	if (sendCommand(data, sendCmd, sizeof(sendCmd) / sizeof(sendCmd[0]), 0) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	// Send actual file
	if (sendPacket(data) != EXIT_SUCCESS) {
		return EXIT_FAILURE;
	}

	closeU64Socket(data);

	return EXIT_SUCCESS;
}

int powerOff(programData *prgData)
{
	int result;
	const uint8_t data[] = {
		0x1b, 0x5b, 0x31, 0x35, 0x7e, // f5
		0x1b, 0x5b, 0x42, // Arrow down
		0xd, 0x00, //enter
		0x1b, 0x5b, 0x42,
		0x1b, 0x5b, 0x42,
		0xd, 0x00, //enter
	};

	printf("Sending power-off sequence to Ultimate64...\n");
	result = sendSequence(prgData, data, sizeof(data), 1);
	if (result != EXIT_SUCCESS) {
		return result;
	}
	printf("  * done.\n");
	prgData->isStreaming=0;

	return EXIT_SUCCESS;
}

