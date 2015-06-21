#include <stdio.h>

#include "Logger.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <alsa/asoundlib.h>
#include <sched.h>

static void setScheduler();
static int configureServer(int port);
static void dumpParams(snd_pcm_t *hWave);
static int writeAudio(snd_pcm_t *hWaveOut, const char* data, int size, int chunkSize);
static int audioRecovery(snd_pcm_t *hWaveOut, int err);
static snd_pcm_t * setupAudio(const char *audioTarget, int samplePerSec, int numChannels, snd_pcm_uframes_t *bufferChunk, int numBuffers);
static void stopAudio(snd_pcm_t *hWaveOut);
static void setHwParams(snd_pcm_t *hWaveOut, int samplePerSec, int numChannels, snd_pcm_uframes_t *bufferChunk, int numBuffers);
static void setSwParams(snd_pcm_t *hWaveOut, snd_pcm_uframes_t bufferChunk);

int main(int argc, char *argv[])
{

	const char *audioTarget = "default";
	int port = 2305;
	int audioSamplePerSec = 48000;
	int audioChannels = 2;
	int audioBytesPerSample = 2;
	snd_pcm_uframes_t audioBufferChunk = 128;
	int audioBufferNum = 10;

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
			audioTarget = argv[i+1];
			i++;
		}
	}

	int server = configureServer(port);
	if(server == -1)
		return 0;
	int chunkSize = audioChannels * audioBytesPerSample;

	setScheduler();

	logMessage("Ready\n");

	struct timeval packetRecvTimeout;

	while(1) {
		fd_set rfds;
		struct timeval timeout;
		FD_ZERO(&rfds);
		FD_SET(server, &rfds);
		select((int)server+1, &rfds, 0, 0, NULL);
		logMessage("Incoming data received, starting player\n");

		snd_pcm_t *hWaveOut = setupAudio(audioTarget, audioSamplePerSec, audioChannels, &audioBufferChunk, audioBufferNum);
		if(hWaveOut == NULL)
			break;

		int dataRecv;
		int dataRecvBufferSize = 32768;
		char* dataRecvBuffer = new char[dataRecvBufferSize];

		logMessage("Playing...\n");

		//drain recv buffer
		packetRecvTimeout.tv_usec = 0;
		packetRecvTimeout.tv_sec = 0;
		timeout = packetRecvTimeout;
		while(select((int)server+1, &rfds, 0, 0, &timeout)) {	//recupere les données de l'autre programme (waveSend) par le reseau
			if((dataRecv = recvfrom(server, dataRecvBuffer, dataRecvBufferSize, 0, NULL, NULL)) <= 0)
				break;
			FD_ZERO(&rfds);
			FD_SET(server, &rfds);
			timeout = packetRecvTimeout;
		}

		FD_ZERO(&rfds);
		FD_SET(server, &rfds);

		packetRecvTimeout.tv_usec = 0;
		packetRecvTimeout.tv_sec = 1;	//1sec de timeout avant de stopper le system de son (quand on recoit plus rien)
		timeout = packetRecvTimeout;
		while(select((int)server+1, &rfds, 0, 0, &timeout)) {	//recupere les données de l'autre programme (waveSend) par le reseau
			if((dataRecv = recvfrom(server, dataRecvBuffer, dataRecvBufferSize, 0, NULL, NULL)) <= 0)
				break;

			writeAudio(hWaveOut, dataRecvBuffer, dataRecv/chunkSize, chunkSize);

			if(dataRecv % chunkSize != 0)
				logMessage("Warning: received datagram of size %d but chunk size is %d\n", dataRecv, chunkSize);

			FD_ZERO(&rfds);
			FD_SET(server, &rfds);
			timeout = packetRecvTimeout;
		}

		delete[] dataRecvBuffer;

		logMessage("End of connection\n");

		stopAudio(hWaveOut);
	}

	//ferme la connection
	close(server);

	return 0;
}

static void setScheduler() {
	struct sched_param sched_param;

	if (sched_getparam(0, &sched_param) < 0) {
		logMessage("Scheduler getparam failed...\n");
		return;
	}
	sched_param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if (!sched_setscheduler(0, SCHED_FIFO, &sched_param)) {
		logMessage("Scheduler set to Round Robin with priority %i...\n", sched_param.sched_priority);
		fflush(stdout);
		return;
	}
	logMessage("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n", sched_param.sched_priority);
}

static int configureServer(int port) {
	struct sockaddr_in sin;
	int server = -1;

	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if((server = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		logMessage("Erreur Winsock\n");
		return -1;
	}

	if(bind(server, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
		logMessage("Erreur: port deja utilisé, choisissez un autre\n");

		return -1;
	}

	return server;
}

static snd_pcm_t * setupAudio(const char *audioTarget, int samplePerSec, int numChannels, snd_pcm_uframes_t *bufferChunk, int numBuffers) {
	int result;
	snd_pcm_t * hWaveOut;

	result = snd_pcm_open(&hWaveOut, audioTarget, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	if(result < 0) {
		logMessage("Error no waveout: %s\n",  snd_strerror(result));
		return NULL;
	}

	setHwParams(hWaveOut, samplePerSec, numChannels, bufferChunk, numBuffers);
	setSwParams(hWaveOut, *bufferChunk);
	dumpParams(hWaveOut);

	result = snd_pcm_prepare(hWaveOut);
	if(result < 0) {
		logMessage("Error preparing: %s\n",  snd_strerror(result));
		return NULL;
	}

	return hWaveOut;
}

static void stopAudio(snd_pcm_t *hWaveOut) {
	snd_pcm_drop(hWaveOut);
	snd_pcm_close(hWaveOut);
}

static void setHwParams(snd_pcm_t *hWaveOut, int samplePerSec, int numChannels, snd_pcm_uframes_t *bufferChunk, int numBuffers) {
	snd_pcm_hw_params_t *hwparams;
	int result;

	snd_pcm_hw_params_alloca(&hwparams);

	result = snd_pcm_hw_params_any(hWaveOut, hwparams);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_any failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_format(hWaveOut, hwparams, SND_PCM_FORMAT_S16_LE);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_set_format failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_rate(hWaveOut, hwparams, samplePerSec, 0);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_set_rate failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_channels(hWaveOut, hwparams, numChannels);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_set_channels failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_access(hWaveOut, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_set_access failed: %s\n", snd_strerror(result));
		return;
	}

	int dir = 0;
	result = snd_pcm_hw_params_set_period_size_near(hWaveOut, hwparams, bufferChunk, &dir);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_set_period_size_near failed: %s\n", snd_strerror(result));
		return;
	}

	snd_pcm_uframes_t totalBufferSize = (*bufferChunk) * numBuffers;
	result = snd_pcm_hw_params_set_buffer_size_near(hWaveOut, hwparams, &totalBufferSize);
	if(result < 0) {
		logMessage("snd_pcm_hw_params_set_buffer_size_near failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params(hWaveOut, hwparams);
	if(result < 0) {
		logMessage("snd_pcm_hw_params failed: %s\n", snd_strerror(result));
		return;
	}
}

static void setSwParams(snd_pcm_t *hWaveOut, snd_pcm_uframes_t bufferChunk) {
	snd_pcm_sw_params_t *swparams;
	int result;

	snd_pcm_sw_params_alloca(&swparams);

	result = snd_pcm_sw_params_current(hWaveOut, swparams);
	if(result < 0) {
		logMessage("snd_pcm_sw_params_current failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_sw_params_set_start_threshold(hWaveOut, swparams, bufferChunk);
	if(result < 0) {
		logMessage("snd_pcm_sw_params_set_start_threshold failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_sw_params(hWaveOut, swparams);
	if(result < 0) {
		logMessage("snd_pcm_sw_params failed: %s\n", snd_strerror(result));
		return;
	}
}

static int writeAudio(snd_pcm_t *hWaveOut, const char* data, int size, int chunkSize) {
	while (size > 0) {
		int err = snd_pcm_writei(hWaveOut, data, size);
		if (err == -EAGAIN)
			return 1;

		if (err < 0) {
			if (audioRecovery(hWaveOut, err) < 0) {
				logMessage("Write error: %s\n", snd_strerror(err));
				return 3;
			}
			return 2; /* skip one period */
		}
		data += err * chunkSize;
		size -= err;
	}

	return 0;
}
static int audioRecovery(snd_pcm_t *hWaveOut, int err) {
	// Under-run?
	if (err == -EPIPE) {
		// NOTE: If you see these during playback, you'll have to increase
		// BUFFERSIZE/PERIODSIZE
		//logMessage("underrun\n");
		if ((err = snd_pcm_prepare(hWaveOut)) >= 0)
			return 0;
		logMessage("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
	}

	return(err);
}

static void dumpParams(snd_pcm_t *hWave) {
	snd_output_t *out;

	snd_output_stdio_attach(&out, stderr, 0);
	snd_output_printf(out, "dump :\n");
	snd_pcm_dump_setup(hWave, out);
	snd_output_close(out);
}
