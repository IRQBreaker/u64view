#ifndef SOCKET_H_
#define SOCKET_H_

#include "common.h"

int runCommand(programData *prgData, command cmd, int prepare);
int sendFile(programData *data);
int powerOff(programData *prgData);

#endif // SOCKET_H_
