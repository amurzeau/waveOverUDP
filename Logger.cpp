#include <stdio.h>
#include "Logger.h"

//void logMessage(const char *message) {
//	fprintf(stderr, "%s\n", message);
//	fflush(stderr);
//}

#ifdef _WIN32

void printError(HRESULT hres, int line) {
	const char* hresStr = "Unknown";

	switch(hres) {
		case AUDCLNT_E_NOT_INITIALIZED: hresStr = "AUDCLNT_E_NOT_INITIALIZED"; break;
		case AUDCLNT_E_ALREADY_INITIALIZED: hresStr = "AUDCLNT_E_ALREADY_INITIALIZED"; break;
		case AUDCLNT_E_WRONG_ENDPOINT_TYPE: hresStr = "AUDCLNT_E_WRONG_ENDPOINT_TYPE"; break;
		case AUDCLNT_E_DEVICE_INVALIDATED: hresStr = "AUDCLNT_E_DEVICE_INVALIDATED"; break;
		case AUDCLNT_E_NOT_STOPPED: hresStr = "AUDCLNT_E_NOT_STOPPED"; break;
		case AUDCLNT_E_BUFFER_TOO_LARGE: hresStr = "AUDCLNT_E_BUFFER_TOO_LARGE"; break;
		case AUDCLNT_E_OUT_OF_ORDER: hresStr = "AUDCLNT_E_OUT_OF_ORDER"; break;
		case AUDCLNT_E_UNSUPPORTED_FORMAT: hresStr = "AUDCLNT_E_UNSUPPORTED_FORMAT"; break;
		case AUDCLNT_E_INVALID_SIZE: hresStr = "AUDCLNT_E_INVALID_SIZE"; break;
		case AUDCLNT_E_DEVICE_IN_USE: hresStr = "AUDCLNT_E_DEVICE_IN_USE"; break;
		case AUDCLNT_E_BUFFER_OPERATION_PENDING: hresStr = "AUDCLNT_E_BUFFER_OPERATION_PENDING"; break;
		case AUDCLNT_E_THREAD_NOT_REGISTERED: hresStr = "AUDCLNT_E_THREAD_NOT_REGISTERED"; break;
		case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED: hresStr = "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED"; break;
		case AUDCLNT_E_ENDPOINT_CREATE_FAILED: hresStr = "AUDCLNT_E_ENDPOINT_CREATE_FAILED"; break;
		case AUDCLNT_E_SERVICE_NOT_RUNNING: hresStr = "AUDCLNT_E_SERVICE_NOT_RUNNING"; break;
		case AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED: hresStr = "AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED"; break;
		case AUDCLNT_E_EXCLUSIVE_MODE_ONLY: hresStr = "AUDCLNT_E_EXCLUSIVE_MODE_ONLY"; break;
		case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL: hresStr = "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL"; break;
		case AUDCLNT_E_EVENTHANDLE_NOT_SET: hresStr = "AUDCLNT_E_EVENTHANDLE_NOT_SET"; break;
		case AUDCLNT_E_INCORRECT_BUFFER_SIZE: hresStr = "AUDCLNT_E_INCORRECT_BUFFER_SIZE"; break;
		case AUDCLNT_E_BUFFER_SIZE_ERROR: hresStr = "AUDCLNT_E_BUFFER_SIZE_ERROR"; break;
		case AUDCLNT_E_CPUUSAGE_EXCEEDED: hresStr = "AUDCLNT_E_CPUUSAGE_EXCEEDED"; break;
		case AUDCLNT_E_BUFFER_ERROR: hresStr = "AUDCLNT_E_BUFFER_ERROR"; break;
		case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED: hresStr = "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED"; break;
		case AUDCLNT_E_INVALID_DEVICE_PERIOD: hresStr = "AUDCLNT_E_INVALID_DEVICE_PERIOD"; break;
		case AUDCLNT_S_BUFFER_EMPTY: hresStr = "AUDCLNT_S_BUFFER_EMPTY"; break;
		case AUDCLNT_S_THREAD_ALREADY_REGISTERED: hresStr = "AUDCLNT_S_THREAD_ALREADY_REGISTERED"; break;
		case AUDCLNT_S_POSITION_STALLED: hresStr = "AUDCLNT_S_POSITION_STALLED"; break;
	}

	logMessage("Error at line %d: %s(0x%08X)\n", line, hresStr, hres);
}

#define REFTIMES_PER_MILLISEC  10000L
void printAudioConfig(IAudioClient *pAudioClient) {
	WAVEFORMATEX *waveFormat;
	UINT32 bufferFrameCount;
	REFERENCE_TIME hnsDefaultDevicePeriod, hnsMinimumDevicePeriod, hnsLatency;

	pAudioClient->GetMixFormat(&waveFormat);

	if(waveFormat) {
		// Get the size of the allocated buffer.
		HRESULT hr = pAudioClient->GetBufferSize(&bufferFrameCount);
		if(FAILED(hr))
			bufferFrameCount = 0;

		// Get the size of the allocated buffer.
		hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsMinimumDevicePeriod);
		if(FAILED(hr))
			hnsDefaultDevicePeriod = hnsMinimumDevicePeriod = 0;

		// Get the size of the allocated buffer.
		hr = pAudioClient->GetStreamLatency(&hnsLatency);
		if(FAILED(hr))
			hnsLatency = 0;

		logMessage("Audio format:\n"
				   "- Channels: %d\n"
				   "- Sample rate: %d\n"
				   "- Frame size: %d\n"
				   "- Bits per sample: %d\n"
				   "- Buffer size: %d (%f ms)\n"
				   "- Latency: %f ms\n"
				   "- Default device period: %f ms\n"
				   "- Minimal device period: %f ms\n",
				   waveFormat->nChannels,
				   waveFormat->nSamplesPerSec,
				   waveFormat->nBlockAlign,
				   waveFormat->wBitsPerSample / waveFormat->nChannels,
				   bufferFrameCount,
				   (double)1000.0 * bufferFrameCount / waveFormat->nSamplesPerSec,
				   (double)hnsLatency / REFTIMES_PER_MILLISEC,
				   (double)hnsDefaultDevicePeriod / REFTIMES_PER_MILLISEC,
				   (double)hnsMinimumDevicePeriod / REFTIMES_PER_MILLISEC);
		CoTaskMemFree(waveFormat);
	}
}

#endif

