#include <windows.h>
#include <MMSystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <comdef.h>
#include <stdio.h>
#include "../Logger.h"
#include <string>
#include <FunctionDiscoveryKeys_devpkey.h>

//#define AUDIO_SAMPLEPERSEC 48000
//#define AUDIO_CHANNELS 2
//#define AUDIO_BYTESPERSAMPLE 2

//#define AUDIO_BUFFER_MSEC 10

#define PACKETSIZE 16384

int main(int argc, char* argv[]);
void waitAudioData();
HRESULT playWasapi(int audioSamplePerSec, int audioChannels, int audioBytesPerSample, int audioBufferChunk, int audioBufferNum, std::string name);
HRESULT getDefaultDevice(std::string name, IMMDevice **ppMMDevice);

SOCKET connection = INVALID_SOCKET;
SOCKADDR_IN sin_server;
char recvBuffer[PACKETSIZE];

void atexitFunction() {
	char dummy;
	sendto(connection, &dummy, 0, 0, (SOCKADDR*)&sin_server, sizeof(sin_server));
}

BOOL WINAPI consoleHandlerRoutine(DWORD) {
	atexitFunction();
	return FALSE;
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return main(__argc, __argv);
}

int main(int argc, char* argv[]) {
	int port = 2305;
	int audioSamplePerSec = 48000;
	int audioChannels = 2;
	int audioBytesPerSample = 2;
	int audioBufferChunk = 240;
	int audioBufferNum = 6;
	std::string audioDeviceName;

	WSADATA wsadata;

	WSAStartup(MAKEWORD(2, 0), &wsadata);
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	for(int i = 1; (i+1) < argc; i++) {
		if(!strcmp(argv[i], "--rate")) {
			audioSamplePerSec = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--channel")) {
			audioChannels = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--chunksize")) {
			audioBufferChunk = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--chunknum")) {
			audioBufferNum = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--port")) {
			port = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--device")) {
			audioDeviceName = std::string(argv[i+1]);
			i++;
		}
	}

	printf("Listening on: 0.0.0.0:%d\n", port);
	sin_server.sin_addr.s_addr = htonl(INADDR_ANY);
	sin_server.sin_family = AF_INET;
	sin_server.sin_port = htons(port);

	if((connection = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
	{
		MessageBox(NULL, "Erreur Winsock", "NetSound", MB_OK);
		return -1;
	}

	if(bind(connection, (struct sockaddr*)&sin_server, sizeof(sin_server)) == SOCKET_ERROR)
	{
		MessageBox(NULL, "Erreur Winsock", "NetSound", MB_OK);
		return -1;
	}

	atexit(atexitFunction);
	SetConsoleCtrlHandler(consoleHandlerRoutine, TRUE);
	
	unsigned long flags = 1;
	ioctlsocket(connection, FIONBIO, &flags);

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

	while(1) {
		waitAudioData();
		logMessage("Received data\n");
		int errcode = playWasapi(audioSamplePerSec, audioChannels, audioBytesPerSample, audioBufferChunk, audioBufferNum, audioDeviceName);
		if(errcode) {
			_com_error err(errcode);
			LPCTSTR errMsg = err.ErrorMessage();
			logMessage("Error: %s\n", errMsg);
		}
		logMessage("Disconnected\n");
	}

	closesocket(connection);

	CoUninitialize();
	WSACleanup();
	return 0;
}

void waitAudioData() {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(connection, &rfds);
	select((int)connection+1, &rfds, 0, 0, NULL);
}

int recvAudioData(void* data, unsigned int size) {
	fd_set rfds;
	struct timeval packetRecvTimeout;
	packetRecvTimeout.tv_usec = 0;
	packetRecvTimeout.tv_sec = 1;	//1sec de timeout avant de stopper le system de son (quand on recoit plus rien)
	FD_ZERO(&rfds);
	FD_SET(connection, &rfds);
	if(select((int)connection+1, &rfds, 0, 0, &packetRecvTimeout)) {
		return recvfrom(connection, (char*)data, size, 0, NULL, NULL);
	}

	return -1;
}

// -------------------------------
// WASAPI

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
			  if (FAILED(hres)) { printError(hres, __LINE__); goto Exit; }
#define SAFE_RELEASE(punk)  \
			  if ((punk) != NULL)  \
				{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

HRESULT playWasapi(int audioSamplePerSec, int audioChannels, int audioBytesPerSample, int audioBufferChunk, int audioBufferNum, std::string name)
{
	HRESULT hr;
	UINT32 bufferFrameCount;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioRenderClient *pRenderClient = NULL;
	WAVEFORMATEX pFormat;
	int timePeriod = audioBufferChunk * 1000 / audioSamplePerSec;

	hr = getDefaultDevice(name, &pDevice);
	EXIT_ON_ERROR(hr)

	hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

	pFormat.wFormatTag = WAVE_FORMAT_PCM;
	pFormat.nChannels = audioChannels;
	pFormat.wBitsPerSample = audioBytesPerSample*8;
	pFormat.nBlockAlign = pFormat.nChannels * audioBytesPerSample;
	pFormat.nSamplesPerSec = audioSamplePerSec;
	pFormat.nAvgBytesPerSec = pFormat.nBlockAlign * pFormat.nSamplesPerSec;
	pFormat.cbSize = 0;

	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
								  AUDCLNT_SESSIONFLAGS_EXPIREWHENUNOWNED | AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_SESSIONFLAGS_DISPLAY_HIDE | AUDCLNT_SESSIONFLAGS_DISPLAY_HIDEWHENEXPIRED,
								  timePeriod * REFTIMES_PER_MILLISEC * audioBufferNum,
								  0 /* timePeriod * REFTIMES_PER_MILLISEC */,
								  &pFormat,
								  NULL);
	EXIT_ON_ERROR(hr)

	// Get the size of the allocated buffer.
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

	printAudioConfig(pAudioClient);

	timeBeginPeriod(timePeriod);

	hr = pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
	EXIT_ON_ERROR(hr)

	logMessage("Recording\n");

	hr = pAudioClient->Start();  // Start recording.
	EXIT_ON_ERROR(hr)

	// Each loop fills about half of the shared buffer.
	while(1) {
		int packetLength = recvAudioData(recvBuffer, PACKETSIZE);

		if(packetLength > 0) {
			UINT32 packetFrames = packetLength / pFormat.nBlockAlign;
			int dataRead = 0;
			//while(packetFrames > 0)
			{
				UINT32 paddingFrames;
				pAudioClient->GetCurrentPadding(&paddingFrames);
				const UINT32 availableFrames = bufferFrameCount - paddingFrames;

				if(packetFrames > availableFrames)
					logMessage("Buffer too small: %d available but received %d\n", availableFrames, packetFrames);

				UINT32 numFramesRequested = min(packetFrames, availableFrames);
				BYTE *pData;

				// Get the available data in the shared buffer.
				hr = pRenderClient->GetBuffer(numFramesRequested, &pData);
				EXIT_ON_ERROR(hr)

				memcpy(pData, recvBuffer + dataRead, numFramesRequested * pFormat.nBlockAlign);

				hr = pRenderClient->ReleaseBuffer(numFramesRequested, 0);
				EXIT_ON_ERROR(hr)

				packetFrames -= numFramesRequested;
				dataRead += numFramesRequested * pFormat.nBlockAlign;
			}
		} else {
			break;
		}
	}

	hr = pAudioClient->Stop();  // Stop playing.
	EXIT_ON_ERROR(hr)

Exit:
	timeEndPeriod(timePeriod);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pRenderClient)

	return hr;
}

HRESULT getDeviceByName(std::string name, IMMDeviceEnumerator *pMMDeviceEnumerator, IMMDevice **ppMMDevice, EDataFlow dataFlow) {
	HRESULT hr = S_OK;
	IMMDeviceCollection *pMMDeviceCollection = NULL;
	UINT count = 0;

	hr = pMMDeviceEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
	EXIT_ON_ERROR(hr)

	hr = pMMDeviceCollection->GetCount(&count);
	EXIT_ON_ERROR(hr)

	for(UINT i = 0; i < count; i++) {
		IMMDevice* pEndpoint;
		IPropertyStore *pProps = NULL;
		PROPVARIANT varName;
		char deviceName[512];

		pMMDeviceCollection->Item(i, &pEndpoint);

		hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
		if (FAILED(hr)) {
			printf("IMMDevice(OpenPropertyStore) failed: hr = 0x%08x\n", hr);
			pEndpoint->Release();
			continue;
		}

		// Initialize container for property value.
		PropVariantInit(&varName);

		// Get the endpoint's friendly-name property.
		hr = pProps->GetValue(PKEY_Device_DeviceDesc, &varName);
		if (FAILED(hr)) {
			printf("IPropertyStore(GetValue) failed: hr = 0x%08x\n", hr);
			pProps->Release();
			pEndpoint->Release();
			continue;
		}

		sprintf(deviceName, "%S", varName.pwszVal);
		PropVariantClear(&varName);
		pProps->Release();

		if(name == deviceName) {
			printf("Using device %s\n", deviceName);
			*ppMMDevice = pEndpoint;
			break;
		} else {
			pEndpoint->Release();
		}
	}

Exit:
	SAFE_RELEASE(pMMDeviceCollection)

	return hr;
}

HRESULT getDefaultDevice(std::string name, IMMDevice **ppMMDevice) {
	HRESULT hr = S_OK;
	IMMDeviceEnumerator *pMMDeviceEnumerator = NULL;
	// activate a device enumerator
	hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void**)&pMMDeviceEnumerator
	);
	EXIT_ON_ERROR(hr)

	*ppMMDevice = NULL;
	hr = getDeviceByName(name, pMMDeviceEnumerator, ppMMDevice, eRender);
	EXIT_ON_ERROR(hr)

	if(*ppMMDevice == NULL) {
		// get the default render endpoint
		printf("Using default device\n");
		hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, ppMMDevice);
		if (FAILED(hr)) {
			printf("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08x\n", hr);
			goto Exit;
		}
	}

Exit:
	SAFE_RELEASE(pMMDeviceEnumerator)

	return hr;
}
