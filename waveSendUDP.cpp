#include <alsa/asoundlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>

#include <stdio.h>
#include "Logger.h"

#define PACKETSIZE 16384

void setScheduler();
void setHwParams(snd_pcm_t *hWave, int numChannels, int bytesPerSample, int samplePerSec, int period, int numBuffer);
void dumpParams(snd_pcm_t *hWave);

inline int min(int a, int b) {
	return (a>b)? b:a;
}

bool inputAvailable() {
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(0, &fds));
}

int main(int argc, char *argv[])
{
	snd_pcm_t *hWaveIn;
	int result;

	int datasent;
	short *data_buffer;
	double volume = 1;

	int connection = -1;
	struct sockaddr_in sin_server = {0};

	const char* ip = NULL;
	const char* audioTarget = "default";
	int port = 2305;
	int audioSamplePerSec = 48000;
	int audioChannels = 2;
	int audioBytesPerSample = 2;
	int audioNumBuffer = 4;

	int bufferChunk = 128;
	int bufferSize;

	for(int i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "--rate") && (i+1) < argc) {
			audioSamplePerSec = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--channel") && (i+1) < argc) {
			audioChannels = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--chunksize") && (i+1) < argc) {
			bufferChunk = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--chunknum") && (i+1) < argc) {
			audioNumBuffer = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--port") && (i+1) < argc) {
			port = atoi(argv[i+1]);
			i++;
		} else if(!strcmp(argv[i], "--device") && (i+1) < argc) {
			audioTarget = argv[i+1];
			i++;
		} else {
			ip = argv[i];
		}
	}

	if(ip == NULL) {
		printf("Usage example: ./waveSendUDP --chunksize 128 --chunknum 4 --device hw:0,0 --rate 48000 --channel 2 --port 2305 192.168.1.10\n");
		return 1;
	}

	bufferSize = bufferChunk*audioChannels*audioBytesPerSample;

	printf("Sending audio to %s\n", ip);
	setScheduler();

	sin_server.sin_addr.s_addr = inet_addr(ip);
	sin_server.sin_family = AF_INET;
	sin_server.sin_port = htons(port);

	if((connection = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		logMessage("Erreur Winsock");
		return 2;
	}

	int flags;
	flags = fcntl(connection, F_GETFL, 0);
	fcntl(connection, F_SETFL, flags | O_NONBLOCK);
	flags = 1;
	setsockopt(connection, SOL_SOCKET, SO_BROADCAST, &flags, sizeof(flags));

	result = snd_pcm_open(&hWaveIn, audioTarget, SND_PCM_STREAM_CAPTURE, 0);
	if(result < 0) {
		logMessage("Failed to open waveform input device.");
		return 3;
	}

	setHwParams(hWaveIn, audioChannels, audioBytesPerSample, audioSamplePerSec, bufferChunk, audioNumBuffer);
	dumpParams(hWaveIn);

	snd_pcm_prepare(hWaveIn);
	data_buffer = (short*)calloc(bufferSize, 1);

	logMessage("Recording...");
	while(1) {
		if(inputAvailable()) {
			char line[256];
			read(STDIN_FILENO, line, 255);
			line[255] = 0;
			if(isdigit(line[0]))
				volume = atof(line);
			printf("vol = %lf\n", volume);
		}

		result = snd_pcm_readi(hWaveIn, data_buffer, bufferChunk);

		if(result < 0) {
			printf("Error %d, errno: %d\n", -result, errno);
			result = snd_pcm_recover(hWaveIn, result, 0);
			logMessage("overload");
		}
		if(result < 0) {
			printf("snd_pcm_... failed\n");
			continue;
		}
		short max = 0;
		if(volume != 1.0f) {
			for(int i = 0; i < bufferSize/2; i++) {
				data_buffer[i] = volume * data_buffer[i];
				if(max < data_buffer[i])
					max = data_buffer[i];
			}
		}
		if(max > 20000)
			printf("max = %d\n", max);
		if(result > 0 && result < bufferChunk) printf("Short write : %d\n", result);
		if(result == bufferChunk) {
			for(datasent = 0 ; datasent < bufferSize;) {
				result = sendto(connection, ((char*)data_buffer) + datasent, min(bufferSize - datasent, PACKETSIZE), MSG_NOSIGNAL, (struct sockaddr*)&sin_server, sizeof(sin_server));
				if(result == -1 && errno == EWOULDBLOCK)
					result = 0;

				if(result == -1) {
					printf("Socket error, errno: %d\n", errno);
					break;
				}
				datasent += result;
			}
		}
	}

	close(connection);
	return 0;
}

void setScheduler()
{
	struct sched_param sched_param;

	if (sched_getparam(0, &sched_param) < 0) {
		printf("Scheduler getparam failed...\n");
		return;
	}
	sched_param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if (!sched_setscheduler(0, SCHED_FIFO, &sched_param)) {
		printf("Scheduler set to Round Robin with priority %i...\n", sched_param.sched_priority);
		fflush(stdout);
		return;
	}
	printf("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n", sched_param.sched_priority);
}


void setHwParams(snd_pcm_t *hWave, int numChannels, int bytesPerSample, int samplePerSec, int period, int numBuffer) {
	snd_pcm_hw_params_t *hwparams;
	int result;

	snd_pcm_hw_params_alloca(&hwparams);

	result = snd_pcm_hw_params_any(hWave, hwparams);
	if(result < 0) {
		printf("snd_pcm_hw_params_any failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_format(hWave, hwparams, SND_PCM_FORMAT_S16_LE);
	if(result < 0) {
		printf("snd_pcm_hw_params_set_format failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_rate(hWave, hwparams, samplePerSec, 0);
	if(result < 0) {
		printf("snd_pcm_hw_params_set_rate failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_channels(hWave, hwparams, numChannels);
	if(result < 0) {
		printf("snd_pcm_hw_params_set_channels failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params_set_access(hWave, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED );
	if(result < 0) {
		printf("snd_pcm_hw_params_set_access failed: %s\n", snd_strerror(result));
		return;
	}

	snd_pcm_uframes_t period_size = period;
	int dir = 0;
	result = snd_pcm_hw_params_set_period_size_near(hWave, hwparams, &period_size, &dir);
	if(result < 0) {
		printf("snd_pcm_hw_params_set_period_size_near failed: %s\n", snd_strerror(result));
		return;
	}

	snd_pcm_uframes_t target_buffer_size = period_size*numBuffer;
	result = snd_pcm_hw_params_set_buffer_size_near(hWave, hwparams, &target_buffer_size);
	if(result < 0) {
		printf("snd_pcm_hw_params_set_buffer_size_near failed: %s\n", snd_strerror(result));
		return;
	}

	result = snd_pcm_hw_params(hWave, hwparams);
	if(result < 0) {
		printf("snd_pcm_hw_params failed: %s\n", snd_strerror(result));
		return;
	}
}

void dumpParams(snd_pcm_t *hWave) {
	snd_output_t *out;

	snd_output_stdio_attach(&out, stderr, 0);
	snd_output_printf(out, "dump :\n");
	snd_pcm_dump_setup(hWave, out);
	snd_output_close(out);
}
