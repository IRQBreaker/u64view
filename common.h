#ifndef COMMON_H_
#define COMMON_H_

#include <stdio.h>
#include <inttypes.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

#define MAX_STRING_SIZE 4096
#define UDP_PAYLOAD_SIZE 768
#define SAMPLE_SIZE 192*4
#define IP_ADDR_SIZE 64
#define DEFAULT_LISTEN_PORT 11000
#define DEFAULT_LISTENAUDIO_PORT 11001
#define DEFAULT_WIDTH 384
#define DEFAULT_HEIGHT 272
#define TCP_BUFFER_SIZE 1024
#define TELNET_PORT 23
#define COMMAND_PORT 64
#define SDLNET_TIMEOUT 30
#define SDLNET_STREAM_TIMEOUT 200
#define USER_COLORS 16*6 + 15 // 16 6 byte values + the 15 commas between them
#define PIXMAP_SIZE 0x100
#define COMMAND_DELAY 10
#define AUDIO_FREQUENCY 48000
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES 192

// "Ok ok, use them then..."
#define SOCKET_CMD_DMA         0xFF01
#define SOCKET_CMD_DMARUN      0xFF02
#define SOCKET_CMD_KEYB        0xFF03
#define SOCKET_CMD_RESET       0xFF04
#define SOCKET_CMD_WAIT        0xFF05
#define SOCKET_CMD_DMAWRITE    0xFF06
#define SOCKET_CMD_REUWRITE    0xFF07
#define SOCKET_CMD_KERNALWRITE 0xFF08
#define SOCKET_CMD_DMAJUMP     0xFF09
#define SOCKET_CMD_MOUNT_IMG   0xFF0A
#define SOCKET_CMD_RUN_IMG     0xFF0B

// Only available on U64
#define SOCKET_CMD_VICSTREAM_ON    0xFF20
#define SOCKET_CMD_AUDIOSTREAM_ON  0xFF21
#define SOCKET_CMD_DEBUGSTREAM_ON  0xFF22
#define SOCKET_CMD_VICSTREAM_OFF   0xFF30
#define SOCKET_CMD_AUDIOSTREAM_OFF 0xFF31
#define SOCKET_CMD_DEBUGSTREAM_OFF 0xFF32

#if !defined(likely)
#define likely(x)    __builtin_expect (!!(x), 1)
#endif
#if !defined(unlikely)
#define unlikely(x)  __builtin_expect (!!(x), 0)
#endif

typedef struct __attribute__((__packed__)) {
	uint16_t seq;
	uint16_t frame;
	uint16_t line;
	uint16_t pixelsInLine;
	uint8_t linexInPacket;
	uint8_t bpp;
	uint16_t encoding;
	char payload[UDP_PAYLOAD_SIZE];
} u64msg_t;

typedef struct __attribute__((__packed__)) {
	uint16_t seq;
	int16_t sample[SAMPLE_SIZE];
} a64msg_t;

typedef enum {
	SCOLORS,
	DCOLORS,
	UCOLORS,
	NUM_OF_COLORSCHEMES
} colorScheme;

typedef enum {
	CMD_START_STREAM,
	CMD_STOP_STREAM,
	CMD_RESET,
	NUM_OF_COMMANDS
} command;

typedef struct {
	int scale;
	int fullscreenFlag;
	int renderFlag;
	int vsyncFlag;
	int verbose;
	int fast;
	int audioFlag;
	colorScheme curColors;
	FILE *vfp;
	FILE *afp;
	char fnbuf[MAX_STRING_SIZE];
	char hostName[MAX_STRING_SIZE];
	int stopStreamOnExit;
	int startStreamOnStart;
	int showHelp;
	UDPpacket *pkg;
	UDPpacket *audpkg;
	SDLNet_SocketSet set;
	UDPsocket udpsock;
	UDPsocket audiosock;
	int listen;
	int listenaudio;
	SDL_AudioSpec want;
	SDL_AudioSpec have;
	SDL_AudioDeviceID dev;
	SDL_Window *win;
	int width;
	int height;
	SDL_Renderer *ren;
	SDL_Texture *tex;
	uint32_t *pixels;
	int pitch;
	int isStreaming;
	uint64_t totalVdataBytes;
	uint64_t totalAdataBytes;
	uint64_t pixMap[PIXMAP_SIZE];
	const uint64_t *red;
	const uint64_t *green;
	const uint64_t *blue;
	char ipStr[IP_ADDR_SIZE];
	int sdl_init;
	int sdl_net_init;
	FILE *dfp;
	char dmaFile[MAX_STRING_SIZE];
	int dmaFileSize;
	uint8_t *dmaAddress;
	TCPsocket tcpSock;
	SDLNet_SocketSet tcpSet;
} programData;

void cleanUp(programData *data);

#endif // COMMON_H_
