This compile to two tools: waveSendUDP and wavePlayUDP.
 - waveSendUDP record audio and send it via UDP as interleaved 16 bits raw PCM
 - wavePlayUDP receive and play the PCM stream

Achieved latency using a laptop client (recording) and raspberry pi server: 21ms

Example of usage:

Client: ./waveSendUDP --chunksize 128 --chunknum 4 --device hw:0,0 --rate 48000 --channel 2 --port 2305 192.168.1.10
Server: ./wavePlayUDP --chunksize 128 --chunknum 10 --device default --rate 48000 --channel 2 --port 2305

chunksize is in samples. There is <rate> samples / seconds (so here 128 samples with 48 samples/ms is 2.667ms per chunk)

Options:
 - chunksize: minimal chunk of audio recorded (on the client) or played (on the server). This value is directly related to the latency.
 - chunknum: size of the record / play buffer in chunks (can be small on the recorder, but can be larger on the player in case of network jitter).
 - device: ALSA device to record / play on
 - rate: sampling rate
 - channel: number of channels (1 = mono, 2 = stereo)
 - port: UDP port

Additionally, waveSendUDP need the server's IP.
