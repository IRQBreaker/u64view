#include "stream.h"
#include "64.h"
#include "socket.h"

// I found the colors here: https://gist.github.com/funkatron/758033
const uint64_t  sred[]   = {0 , 255, 0x68, 0x70, 0x6f, 0x58, 0x35, 0xb8, 0x6f, 0x43, 0x9a, 0x44, 0x6c, 0x9a, 0x6c, 0x95 };
const uint64_t  sgreen[] = {0 , 255, 0x37, 0xa4, 0x3d, 0x8d, 0x28, 0xc7, 0x4f, 0x39, 0x67, 0x44, 0x6c, 0xd2, 0x5e, 0x95 };
const uint64_t  sblue[]  = {0 , 255, 0x2b, 0xb2, 0x86, 0x43, 0x79, 0x6f, 0x25, 0x00, 0x59, 0x44, 0x6c, 0x84, 0xb5, 0x95 };

// I found these colors by showing them on my CRT monitor and taking a picture with my dslr, doing white correction on the raw and averaging the pixels
// They're not mean to be faithful, just thought it'd be kinda fun to see
const uint64_t dred[]   = { 0x06, 0xf2, 0xb6, 0xa2, 0xaf, 0x86, 0x00, 0xf8, 0xd0, 0x79, 0xfb, 0x5e, 0xa3, 0xd1, 0x6e, 0xdc };
const uint64_t dgreen[] = { 0x0a, 0xf1, 0x3c, 0xf7, 0x45, 0xf9, 0x3a, 0xfe, 0x6e, 0x4e, 0x91, 0x6e, 0xb6, 0xfc, 0xb3, 0xe2 };
const uint64_t dblue[]  = { 0x0b, 0xf1, 0x47, 0xed, 0xd7, 0x64, 0xf2, 0x8a, 0x28, 0x00, 0x8f, 0x69, 0xad, 0xc5, 0xff, 0xdb };

uint64_t ured[] =   { 10,255,30,40,50,60,70,80,90,0xa0,0xb0,0xc0,0xd0,0xc0,0xd0,0xe0 };
uint64_t ugreen[] = { 10,255,30,40,50,60,70,80,90,0xa0,0xb0,0xc0,0xd0,0xc0,0xd0,0xe0 };
uint64_t ublue[] =  { 10,255,30,40,50,60,70,80,90,0xa0,0xb0,0xc0,0xd0,0xc0,0xd0,0xe0 };

static inline void pic(int width, int height, int pitch, uint32_t* pixels)
{
	union {
		uint8_t p[4];
		uint32_t c;
	} pcol;
	int p=0;
	pcol.p[0]=0xff;
	for(int y=0; y < height; y++) {
		for(int x=0; x < width; x++) {
			pcol.p[3]=header_data_cmap[header_data[p]][0];
			pcol.p[2]=header_data_cmap[header_data[p]][1];
			pcol.p[1]=header_data_cmap[header_data[p]][2];
			p++;
			pixels[x + (y*pitch/4)] = pcol.c;
		}
	}
}

static inline void chkSeq(programData *data, const char* msg, uint16_t *lseq, uint16_t cseq)
{
	if((uint16_t)(*lseq+1) != cseq && (data->totalAdataBytes>1024*10 && data->totalVdataBytes > 1024*1024) ) {
		printf(msg, *lseq, cseq);
	}
	*lseq=cseq;
}

static inline void setColors(programData *data)
{
	switch(data->curColors) {
		case DCOLORS:
			data->red = dred;
			data->green = dgreen;
			data->blue = dblue;
			break;
		case UCOLORS:
			data->red = ured;
			data->green = ugreen;
			data->blue = ublue;
			break;
		case SCOLORS:
		/* fall through */
		default:
			data->red = sred;
			data->green = sgreen;
			data->blue = sblue;
			break;
	}

	// Build a table with colors for two pixels packed into a byte.
	// Then if we treat the framebuffer as an uint64 array we get to write two pixels in by doing one read and one write
	for(int i=0; i<PIXMAP_SIZE; i++) {
		int ph = (i & 0xf0) >> 4;
		int pl = i & 0x0f;
		data->pixMap[i] = data->red[ph] << (64-8) | data->green[ph]<< (64-16) |
			data->blue[ph] << (64-24) | (uint64_t)0xff << (64-32) |
			data->red[pl] << (32-8) | data->green[pl] << (32-16) |
			data->blue[pl] << (32-24) | 0xff;
	}
}

static inline char* intToIp(programData *data, uint32_t ip)
{
	sprintf(data->ipStr, "%02i.%02i.%02i.%02i", (ip & 0x000000ff), (ip & 0x0000ff00)>>8, (ip & 0x00ff0000) >> 16, (ip & 0xff000000) >> 24);
	return data->ipStr;
}

int setupStream(programData *data)
{
	setColors(data);

	data->pkg = SDLNet_AllocPacket(sizeof(u64msg_t));
	data->audpkg = SDLNet_AllocPacket(sizeof(a64msg_t));

	// Initialize SDL2
	data->sdl_init = SDL_Init(SDL_INIT_VIDEO|data->audioFlag);
	if (data->sdl_init != 0) {
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}

	data->sdl_net_init = SDLNet_Init();
	if(data->sdl_net_init == -1) {
		fprintf(stderr, "SDLNet_Init: %s\n", SDLNet_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}

	if(strlen(data->hostName) && data->startStreamOnStart) {
		if (runCommand(data, CMD_START_STREAM, 1) != EXIT_SUCCESS) {
			cleanUp(data);
			return EXIT_FAILURE;
		}
	}

	data->set=SDLNet_AllocSocketSet(2);
	if(!data->set) {
		fprintf(stderr, "SDLNet_AllocSocketSet: %s\n", SDLNet_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}

	printf("Opening UDP socket on port %i for video...\n", data->listen);
	data->udpsock=SDLNet_UDP_Open(data->listen);
	if(!data->udpsock) {
		fprintf(stderr, "SDLNet_UDP_Open: %s\n", SDLNet_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}

	if( SDLNet_UDP_AddSocket(data->set, data->udpsock) == -1 ) {
		fprintf(stderr, "SDLNet_UDP_AddSocket error: %s\n", SDLNet_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}

	if(data->audioFlag) {
		printf("Opening UDP socket on port %i for audio...\n", data->listenaudio);
		data->audiosock=SDLNet_UDP_Open(data->listenaudio);
		if(!data->audiosock) {
			fprintf(stderr, "SDLNet_UDP_Open: %s\n", SDLNet_GetError());
			cleanUp(data);
			return EXIT_FAILURE;
		}

		if( SDLNet_UDP_AddSocket(data->set, data->audiosock) == -1 ) {
			fprintf(stderr, "SDLNet_UDP_AddSocket error: %s\n", SDLNet_GetError());
			cleanUp(data);
			return EXIT_FAILURE;
		}

		SDL_memset(&data->want, 0, sizeof(data->want));
		data->want.freq = AUDIO_FREQUENCY;
		data->want.format = AUDIO_S16LSB;
		data->want.channels = AUDIO_CHANNELS;
		data->want.samples = AUDIO_SAMPLES;
		data->dev = SDL_OpenAudioDevice(NULL, 0, &data->want, &data->have, 0);

		if(data->dev==0) {
			fprintf(stderr, "Failed to open audio: %s", SDL_GetError());
		}

		SDL_PauseAudioDevice(data->dev, 0);
	}

	// Create a window
	data->win = SDL_CreateWindow("Ultimate 64 view!", 100, 100, data->width*data->scale,
				data->height*data->scale, SDL_WINDOW_SHOWN | data->fullscreenFlag | SDL_WINDOW_RESIZABLE);
	if (data->win == NULL) {
		fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}
	// Set icon
	SDL_Surface *iconSurface = SDL_CreateRGBSurfaceFrom(iconPixels,32,32,32,32*4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_SetWindowIcon(data->win, iconSurface);
	SDL_FreeSurface(iconSurface);

	// Create a renderer
	data->ren = SDL_CreateRenderer(data->win, -1, (data->vsyncFlag | data->renderFlag));
	if (data->ren == NULL) {
		fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
		cleanUp(data);
		return EXIT_FAILURE;
	}

	data->tex = SDL_CreateTexture(data->ren,
				SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_STREAMING,
				data->width,
				data->height);

	if( SDL_LockTexture(data->tex, NULL, (void**)&data->pixels, &data->pitch) ) {
		fprintf(stderr, "Failed to lock texture for writing");
	}

	if (strlen(data->dmaFile)) {
		if (sendFile(data) != EXIT_SUCCESS) {
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

void runStream(programData *data)
{
	SDL_Event event;
	int run = 1;
	int sync = 1;
	int staleVideo=7;
	int r = 0;
	uint16_t lastAseq=0;
	uint16_t lastVseq=0;

	pic(data->width, data->height, data->pitch, data->pixels);
	while (run) {
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case SDLK_ESCAPE:
					run = 0;
				break;
				case SDLK_c:
					data->showHelp=0;
					data->curColors++;
					if(data->curColors == NUM_OF_COLORSCHEMES) {
						data->curColors=SCOLORS;
					}
					setColors(data);
				break;
				case SDLK_s:
					data->showHelp=0;
					if(!strlen(data->hostName)) {
						printf("Can only start/stop stream when started with -u, -U or -I.\n");
					} else {
						if(data->isStreaming) {
							if (runCommand(data, CMD_STOP_STREAM, 1) != EXIT_SUCCESS) {
								run = 0;
							}
						} else {
							if (runCommand(data, CMD_START_STREAM, 1) != EXIT_SUCCESS) {
								run = 0;
							}
						}
					}
				break;
				case SDLK_h:
					data->showHelp=!data->showHelp;
				break;
				case SDLK_p:
					data->stopStreamOnExit=0;
					if (powerOff(data) != EXIT_SUCCESS) {
						run = 0;
					}
					memset(data->hostName, 0, sizeof data->hostName);
				break;
				case SDLK_r:
					if(strlen(data->hostName)) {
						if (runCommand(data, CMD_RESET, 1) != EXIT_SUCCESS) {
							run = 0;
						}
					} else {
						printf("Can only reset when start with -u, -U or -I.\n");
					}
			}
			break;
			case SDL_QUIT:
				run=0;
				break;
			}
		}

		// Check for audio
		if(likely(data->audioFlag)) {
			r = SDLNet_UDP_Recv(data->audiosock, data->audpkg);
			if(likely(r==1)) {

				if(unlikely(data->totalAdataBytes==0)) {
					printf("Got data on audio port (%i) from %s:%i\n", data->listenaudio,
						intToIp(data, data->audpkg->address.host), data->audpkg->address.port );
				}
				data->totalAdataBytes += sizeof(a64msg_t);

				a64msg_t *a = (a64msg_t*)data->audpkg->data;
				if(unlikely(data->verbose)) {
					chkSeq(data, "UDP audio packet missed or out of order, last received: %i current %i\n", &lastAseq, a->seq);
				}

				if(unlikely(data->afp && data->totalVdataBytes != 0 && data->totalAdataBytes != 0)) {
					fwrite(a->sample, SAMPLE_SIZE, 1, data->afp);
				}

				SDL_QueueAudio(data->dev, a->sample, SAMPLE_SIZE );
			} else if(unlikely(r == -1)) {
				fprintf(stderr, "SDLNet_UDP_Recv error: %s\n", SDLNet_GetError());
			}
		}

		// Check for video
		r = SDLNet_UDP_Recv(data->udpsock, data->pkg);
		if(likely(r==1 && !data->showHelp)) {
			if(unlikely(data->totalVdataBytes==0)) {
				printf("Got data on video port (%i) from %s:%i\n", data->listen,
				       intToIp(data, data->pkg->address.host), data->pkg->address.port );
			}
			data->totalVdataBytes += sizeof(u64msg_t);

			u64msg_t *p = (u64msg_t*)data->pkg->data;
			if(unlikely(data->verbose)) {
				chkSeq(data, "UDP video packet missed or out of order, last received: %i current %i\n", &lastVseq, p->seq);
			}

			int y = p->line & 0b0111111111111111;
			if(likely(data->fast)) {
				int lpp = p->linexInPacket;
				int hppl =p->pixelsInLine/2;
				for(int l=0; l < lpp; l++) {
					for(int x=0; x < hppl; x++) {
						int idx = x+(l*hppl);
						uint8_t pc = (p->payload[idx]);
						((uint64_t*)data->pixels)[x + ((y+l)*data->pitch/8)] = data->pixMap[pc];
					}
				}
			} else {
				for(int l=0; l < p->linexInPacket; l++) {
					for(int x=0; x < p->pixelsInLine/2; x++) {
						int idx = x+(l*p->pixelsInLine/2);
						int pl = (p->payload[idx] & 0x0f);
						int ph = (p->payload[idx] & 0xf0) >> 4;
						int r = data->red[pl];
						int g = data->green[pl];
						int b = data->blue[pl];

						SDL_SetRenderDrawColor(data->ren, r, g, b, 255);
						SDL_RenderDrawPoint(data->ren, x*2, y+l);
						r = data->red[ph];
						g = data->green[ph];
						b = data->blue[ph];
						SDL_SetRenderDrawColor(data->ren, r, g, b, 255);
						SDL_RenderDrawPoint(data->ren, x*2+1, y+l);
					}
				}
			}
			if(likely(p->line & 0b1000000000000000)) {
				sync=1;
				staleVideo=0;
			}
		} else if(unlikely(r == -1)) {
			fprintf(stderr, "SDLNet_UDP_Recv error: %s\n", SDLNet_GetError());
		} else {
			staleVideo++;
			if(unlikely(staleVideo > 5)) {
				if(staleVideo == 6) {
					pic(data->width, data->height, data->pitch, data->pixels);
				} else if(staleVideo%10 == 0) {
					sync=1;
				}
			}
		}

		if(likely(sync)) {
			sync=0;
			if(likely(data->fast)) {
				if(unlikely(data->vfp && data->totalVdataBytes != 0 && data->totalAdataBytes != 0)) {
					fwrite(data->pixels, sizeof(uint32_t)*data->width*data->height, 1, data->vfp);
				}
				SDL_UnlockTexture(data->tex);
				SDL_RenderCopy(data->ren, data->tex, NULL, NULL);
				SDL_RenderPresent(data->ren);
				if( SDL_LockTexture(data->tex, NULL, (void**)data->pixels, &data->pitch) ) {
					fprintf(stderr, "Error: Failed to lock texture for writing.");
				}
			} else {
				SDL_RenderPresent(data->ren);
			}
		}
		SDLNet_CheckSockets(data->set, SDLNET_STREAM_TIMEOUT);
	}

	if(strlen(data->hostName) && data->stopStreamOnExit) {
		runCommand(data, CMD_STOP_STREAM, 1);
	}

	cleanUp(data);
}
