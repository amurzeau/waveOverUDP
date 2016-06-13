#ifndef LOGGER_H_
#define LOGGER_H_

#define logMessage printf

#ifdef _WIN32

#include <windows.h>
#include <audioclient.h>

//void logMessage(const char *message);
void printError(HRESULT hres, int line);
void printAudioConfig(IAudioClient *pAudioClient);

#endif

#endif
