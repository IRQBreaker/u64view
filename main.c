/**
 * License: WTFPL
 * Copyleft 2019 DusteD
 */
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include "common.h"
#include "stream.h"

extern const uint64_t  *sred;
extern const uint64_t  *sgreen;
extern const uint64_t  *sblue;
extern const uint64_t *dred;
extern const uint64_t *dgreen;
extern const uint64_t *dblue;
extern uint64_t *ured;
extern uint64_t *ugreen;
extern uint64_t *ublue;

void setDefaults(programData *data)
{
	memset(data, 0, sizeof(programData));
	data->scale = 1;
	data->renderFlag = SDL_RENDERER_ACCELERATED;
	data->fast = 1;
	data->audioFlag = SDL_INIT_AUDIO;
	data->curColors = SCOLORS;
	data->stopStreamOnExit = 1;
	data->startStreamOnStart = 1;
	data->listen = DEFAULT_LISTEN_PORT;
	data->listenaudio = DEFAULT_LISTENAUDIO_PORT;
	data->width = DEFAULT_WIDTH;
	data->height = DEFAULT_HEIGHT;
	data->red = sred;
	data->green = sgreen;
	data->blue = sblue;
}

void printColors(const uint64_t *red, const uint64_t *green, const uint64_t *blue)
{
	for(int i=0; i < 16; i++) {
		printf("%02x%02x%02x%c", (int)red[i], (int)green[i],(int)blue[i], ((i==15)?' ':',') );
	}

}

void setUserColors(char *ucol)
{
	printf("Using user-provided colors: ");
	int pos=0;
	char colbyte[3] = {0,0,0 };
	for(int i=0; i < 16; i++) {
		colbyte[0] = ucol[pos];
		pos++;
		colbyte[1] = ucol[pos];
		pos++;
		ured[i] = strtol(colbyte, NULL, 16);

		colbyte[0] = ucol[pos];
		pos++;
		colbyte[1] = ucol[pos];
		pos++;
		ugreen[i] = strtol(colbyte, NULL, 16);

		colbyte[0] = ucol[pos];
		pos++;
		colbyte[1] = ucol[pos];
		pos++;
		ublue[i] = strtol(colbyte, NULL, 16);

		pos++; // Skip character after 6 bytes
	}

	printColors(ured, ugreen, ublue);
	printf("\n");
}

void cleanUp(programData *data)
{
	if (data->dev) {
		SDL_CloseAudioDevice(data->dev);
	}
	if (data->vfp) {
		fclose(data->vfp);
	}
	if (data->afp) {
		fclose(data->afp);
	}
	if (data->dmaAddress) {
		munmap(data->dmaAddress, data->dmaFileSize);
	}
	if (data->dfp) {
		fclose(data->dfp);
	}
	if (data->pkg) {
		SDLNet_FreePacket(data->pkg);
	}
	if (data->audpkg) {
		SDLNet_FreePacket(data->audpkg);
	}
	if (data->set) {
		SDLNet_FreeSocketSet(data->set);
	}
	if (data->udpsock) {
		SDLNet_UDP_Close(data->udpsock);
	}
	if (data->audiosock) {
		SDLNet_UDP_Close(data->audiosock);
	}
	if (data->tex) {
		SDL_DestroyTexture(data->tex);
	}
	if (data->ren) {
		SDL_DestroyRenderer(data->ren);
	}
	if (data->win) {
		SDL_DestroyWindow(data->win);
	}
	if (!data->sdl_net_init) {
		SDLNet_Quit();
	}
	if (!data->sdl_init) {
		SDL_Quit();
	}
}

int loadFile(programData *data)
{
	struct stat st;

	data->dfp = fopen(data->dmaFile, "r");
	if (!data->dfp) {
		perror("Error");
		return EXIT_FAILURE;
	}

	if (fstat(fileno(data->dfp), &st) != 0) {
		perror("Error");
		fclose(data->dfp);
		return EXIT_FAILURE;
	}

	data->dmaFileSize = (int)st.st_size;

	data->dmaAddress = mmap(NULL, data->dmaFileSize, PROT_READ, MAP_SHARED, fileno(data->dfp), 0);
	if (data->dmaAddress == MAP_FAILED) {
		perror("Error");
		fclose(data->dfp);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void printHelp(void)
{
	printf("\nUsage: u64view [-l N] [-a N] [-z N |-f] [-s] [-v] [-V] [-c] [-m] [-t] [-T [RGB,...]] [-u IP | -U IP -I IP] [-o FN]\n"
			"       -l N  (default 11000) Video port number.\n"
			"       -a N  (default 11001) Audio port number.\n"
			"       -z N  (default 1)     Scale the window to N times size, N must be an integer.\n"
			"       -f    (default off)   Fullscreen, will stretch.\n"
			"       -s    (default off)   Prefer software rendering, more cpu intensive.\n"
			"       -v    (default off)   Use vsync.\n"
			"       -V    (default off)   Verbose output, tell when packets are dropped, how much data was transferred.\n"
			"       -c    (default off)   Use more versatile drawing method, more cpu intensive, can't scale.\n"
			"       -m    (default off)   Completely turn off audio.\n"
			"       -t    (default off)   Use colors that look more like DusteDs TV instead of the 'real' colors.\n"
			"       -T [] (default off)   No argument: Show color values and help for -T\n"
			"       -u IP (default off)   Connect to Ultimate64 at IP and command it to start streaming Video and Audio.\n"
			"       -U IP (default off)   Same as -u but don't stop the streaming when u64view exits.\n"
			"       -I IP (default off)   Just know the IP, do nothing, so keys can be used for starting/stopping stream.\n"
			"       -o FN (default off)   Output raw ARGB to FN.rgb and PCM to FN.pcm (20 MiB/s, you disk must keep up or packets are dropped).\n"
			"       -x FN                 load and run program\n\n");
}

int parseArguments(int argc, char **argv, programData *data)
{
	opterr = 0;
	int c;
	char *endptr;

	errno = 0;
	while ((c = getopt (argc, argv, "hl:a:z:fsvVcmtT:u:U:I:o:x:")) != -1) {
		switch(c) {
			case 'l':
				data->listen = (int)strtol(optarg, &endptr, 10);
				if (endptr == optarg) {
					errno = EINVAL;
					perror("Error");
					return EXIT_FAILURE;
				}
				if (data->listen <= 1024) {
					fprintf(stderr, "Video port must be an integer larger than 1024.\n");
					return EXIT_FAILURE;
				}
				break;
			case 'a':
				data->listenaudio = (int)strtol(optarg, &endptr, 10);
				if (endptr == optarg) {
					errno = EINVAL;
					perror("Error");
					return EXIT_FAILURE;
				}
				if (data->listenaudio <=1024) {
					fprintf(stderr, "Audio port must be an integer larger than 0.\n");
					return EXIT_FAILURE;
				}
				break;
			case 'z':
				data->scale = (int)strtol(optarg, &endptr, 10);
				if (endptr == optarg) {
					errno = EINVAL;
					perror("Error");
					return EXIT_FAILURE;
				}
				if (data->scale == 0) {
					fprintf(stderr, "Scale must be an integer larger than 0.\n");
					return EXIT_FAILURE;
				}
				break;
			case 'f':
				data->fullscreenFlag = SDL_WINDOW_FULLSCREEN_DESKTOP;
				printf("Fullscreen is on.\n");
				break;
			case 's':
				data->renderFlag = SDL_RENDERER_SOFTWARE;
				break;
			case 'v':
				data->vsyncFlag = SDL_RENDERER_PRESENTVSYNC;
				printf("Vsync is on.\n");
				break;
			case 'V':
				data->verbose=1;
				printf("Verbose is on.\n");
				break;
			case 'c':
				data->fast = 0;
				break;
			case 'm':
				data->audioFlag=0;
				printf("Audio is off.\n");
				break;
			case 't':
				data->curColors = DCOLORS;
				printf("Using DusteDs CRT colors.\n");
				break;
			case 'T':
				if (strlen(optarg) != USER_COLORS) {
					fprintf(stderr, "Error: Expected a string of exactly %i characters (see  -T without parameter to see examples)\n", USER_COLORS);
					return EXIT_FAILURE;
				}
				setUserColors(optarg);
				data->curColors = UCOLORS;
				break;
			case 'o':
				data->verbose=1;
				printf("Turning on verbose mode, so you can see if you miss any data!\n");
				printf("Outputting video to %s.rgb and audio to %s.pcm ...\n", optarg, optarg);
				printf( "\nTry encoding with:\n"
						"ffmpeg -vcodec rawvideo -pix_fmt abgr -s 384x272 -r 50\\\n"
						"  -i %s.rgb -f s16le -ar 47983 -ac 2 -i %s.pcm\\\n"
						"  -vf scale=w=1920:h=1080:force_original_aspect_ratio=decrease\\\n"
						"  -sws_flags neighbor -crf 15 -vcodec libx264 %s.avi\n\n", optarg, optarg, optarg);

				sprintf(data->fnbuf, "%s.rgb", optarg);
				data->vfp=fopen(data->fnbuf,"w");
				if(!data->vfp) {
					perror("Error");
					return EXIT_FAILURE;
				}
				sprintf(data->fnbuf, "%s.pcm", optarg);
				data->afp=fopen(data->fnbuf,"w");
				if(!data->afp) {
					perror("Error");
					fclose(data->vfp);
					return EXIT_FAILURE;
				}
				break;
			case 'u':
				strncpy(data->hostName, optarg, MAX_STRING_SIZE - 1);
				break;
			case 'U':
				strncpy(data->hostName, optarg, MAX_STRING_SIZE - 1);
				data->stopStreamOnExit=0;
				break;
			case 'I':
				strncpy(data->hostName, optarg, MAX_STRING_SIZE - 1);
				data->stopStreamOnExit=0;
				data->startStreamOnStart=0;
				break;
			case 'x':
				strncpy(data->dmaFile, optarg, MAX_STRING_SIZE - 1);
				if (loadFile(data) != EXIT_SUCCESS)
				{
					return EXIT_FAILURE;
				}
				break;
			case '?':
				if (optopt == 'l' || optopt == 'a' || optopt == 'z' ||
				    optopt == 'u' || optopt  == 'U' || optopt == 'I' || optopt == 'x') {
					fprintf(stderr, "Option -%c requires an argument.\n", optopt);
					return EXIT_FAILURE;
				} else if (optopt == 'T') {
					printf("User-defined color option (-T):\n\n    Default colors: ");
					printColors(sred, sgreen, sblue);
					printf("\n    DusteDs colors: ");
					printColors(dred, dgreen, dblue);
					printf("\n\n    If you want to use your own color values, just type them after -T in the format shown above (RGB24 in hex, like HTML, and comma between each color).\n"
							"    The colors are, in order: black, white, red, cyan, purple, green, blue, yellow, orange, brown, pink, dark-grey, grey, light-green, light-blue, light-grey.\n"
							"    Example: DusteDs colors, with a slightly darker blue: -T 060a0b,f2f1f1,b63c47,a2f7ed,af45d7,86f964,0030Ef,f8fe8a,d06e28,794e00,fb918f,5e6e69,a3b6ad,d1fcc5,6eb3ff,dce2db\n\n\n");
					return EXIT_FAILURE;
				} else {
					fprintf(stderr, "Invalid argument '-%c'\n", optopt);
				}
				return EXIT_FAILURE;
			case 'h':
			/* fall through */
			default:
				printHelp();
				return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
	programData data;

	setDefaults(&data);
	printf("\nUltimate 64 view!\n-----------------\n");
	printf("Please enable command interface on the Ultimate64\nTry -h for options.\n\n");

	if (parseArguments(argc, argv, &data) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	printf("Ultimate64 telnet/command interface at %s\n", data.hostName);

	if (setupStream(&data) == EXIT_FAILURE) {
		return EXIT_FAILURE;
	}

	printf("\nRunning...\nPress ESC or close window to stop.\n\n");
	runStream(&data);

	if(data.verbose) {
		printf("\nReceived video data: %"PRIu64" bytes.\nReceived audio data: %"PRIu64" bytes.\n",
		       data.totalVdataBytes, data.totalAdataBytes);
	}

	printf("\n\nThanks to Jens Blidon and Markus Schneider for making my favourite tunes!\n");
	printf("Thanks to Booze for making the best remix of Chicanes Halcyon and such beautiful visuals to go along with it!\n");
	printf("Thanks to Gideons Logic for the U64!\n\n                                    - DusteD says hi! :-)\n\n");

	return EXIT_SUCCESS;
}
