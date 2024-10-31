#include "TsCommon.h"
#include "player.h"
#include "TsParser.h"

SimpleQueue* g_videoQueue = 0;
SimpleQueue* g_audioQueue = 0;
FrameQueue* g_FrameQueue = 0;
d3d_context* g_d3d_ctx = NULL;
ByteQueue* g_playaudioQueue = 0;

static bool bSystemRun = true;
static int g_SessionID = -1;

static unsigned long long g_AudioPlayTime_us = 0;
static unsigned long long g_AudioPlayTime_us_Tick = 0;

static bool g_AudioMute = false;
static bool g_bAudioStarted = false;

Player::Player() {
	std::cout << "Class Player Constrator()" << std::endl;
}

Player::~Player() {
	std::cout << "소멸자!" << std::endl;
}

//SDL처리 ( 실시간 데이터 확인용도 )
bool Player::initializeSDL() {
	bool f_maximized = false;
	window = SDL_CreateWindow("ATSC 3.0 Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, f_maximized ? (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_BORDERLESS) : SDL_WINDOW_RESIZABLE);
	if (!window)
	{
		printf("Err|SDL_CreateWindow - %s\n", SDL_GetError());
		return -2;
	}
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
	{
		printf("Err|SDL_CreateRenderer - %s\n", SDL_GetError());
		return -3;
	}
	SDL_Surface* image = IMG_Load("logo.bmp");
	if (image) {
		std::cout << "Image OK!" << std::endl;
		SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, image);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);
		SDL_DestroyTexture(texture);
		SDL_FreeSurface(image);

	}
	else std::cout << "not found Image" << std::endl;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
		return false;
	}

	if (window) window = NULL;
	window = SDL_CreateWindow("TS Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer) {
		std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
		return false;
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
	if (!texture) {
		std::cerr << "Texture could not be created! SDL_Error: " << SDL_GetError() << std::endl;
		return false;
	}

	return true;
}

void Player::cleanUpSDL() {
	if (texture) SDL_DestroyTexture(texture);
	if (renderer) SDL_DestroyRenderer(renderer);
	if (window) SDL_DestroyWindow(window);
	SDL_Quit();
}


void Player::updateTexture(SDL_Texture* texture, unsigned char* data, int width, int height) {
	//printf("data = 0x%x\n", data);
	//printf("width=[%d] height=[%d]\n", width, height);
	size_t ySize = width * height;
	size_t uvSize = ySize / 4;
	if (data == nullptr) {
		std::cout << "data is nullptr" << std::endl;
		return;
	}
	if (width == 0 || height == 0) {
		fprintf(stderr, "Error: Invalid texture size (width or height is zero)\n");
		return;
	}

	unsigned char* yPlane = data;
	unsigned char* uPlane = data + ySize;
	unsigned char* vPlane = data + ySize + uvSize;
	if (yPlane == nullptr || uPlane == nullptr || vPlane == nullptr) {
		fprintf(stderr, "Error: One of the YUV planes is nullptr\n");
		return;
	}

	try {
		//SDL_UpdateYUVTexture(texture, NULL, data, width, data + width * height, width / 2, data + width * height * 5 / 4, width / 2); // Assuming pixelData is in a format compatible with SDL_PIXELFORMAT_YV12
		SDL_UpdateYUVTexture(texture, NULL, yPlane, width, uPlane, width / 2, vPlane, width / 2);
		int pitch = 640 * 3;
		//SDL_UpdateTexture(texture, NULL, data, pitch);
		renderFrame();
	}
	catch (const std::runtime_error& e) {
		std::cerr << "Caught an exception : " << e.what() << std::endl;
	}
}

void Player::renderFrame() {
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}


#pragma warning(push)
#pragma warning(disable:4996)

int Player::VideoThread(void* ptr) {
	std::cout << "start Video Thread!!!" << std::endl;
	av_register_all(); // Register all formats and codecs
	unsigned char* pReceiveBuff = 0;
	bool bNoWait = false;
	while (bSystemRun)
	{
		//std::cout << "in VideoThread!!!!" << std::endl;
		if (bNoWait == false) SDL_Delay(1);
		char Codec[6];
		int TOI = 0;
		int pos = 0;
		unsigned long long us;
		int nReceiveSize = 0;
		int SessionID = -1;
		if (pReceiveBuff) free(pReceiveBuff);
		pReceiveBuff = 0;
		int width = 0;
		int height = 0;
		pReceiveBuff = g_videoQueue->pop(Codec, &TOI, &pos, &us, &nReceiveSize, &SessionID, &width, &height);
//		std::cout << "g_videoQueue->pop: = " << pos << std::endl;
		if (nReceiveSize == 0)
		{
			bNoWait = false;
			continue;
		}
		if (SessionID != g_SessionID) // reset to empty hvcc
		{
			ProcessH264(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);
			ProcessMPEG2(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);

			bNoWait = true;
			//printf("REMOVE PREV CHANNEL VIDEO QUEUE SessionID=%d g_SessionID=%d\n", SessionID, g_SessionID);
			continue;
		}
		bNoWait = false;
		//printf("Codec=%s pos=%d us=%llu now=%llu nReceiveSize=%d\n", Codec, pos, us, getTimeUs(), nReceiveSize);
		if (strcmp(Codec, "mpeg2") == 0) // MPEG2
		{
			std::cout << "codec is mpeg2 in VideoThread" << std::endl;;
			ProcessMPEG2(1, TOI, pos, us, nReceiveSize, pReceiveBuff, SessionID, width, height, ptr);
		}
		else if (strcmp(Codec, "h264") == 0)
		{
			std::cout << "codec is h264 in VideoThread" << std::endl;
			std::cout << "g_videoQueue->pop: = " << pos << std::endl;
			//ProcessMPEG2(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);
			//임시
			//pos = 1;
			ProcessH264(1, TOI, pos, us, nReceiveSize, pReceiveBuff, SessionID, width, height, ptr);
		}
		if (strcmp(Codec, "reset") == 0) // CHANNEL CHANGE CODEC RESET
		{
			std::cout << "codec is reset in VideoThread" << std::endl;
			ProcessMPEG2(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);
			ProcessH264(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);
		}

	} // while

	if (pReceiveBuff) free(pReceiveBuff);
	printf("VideoThread end\n");
	ProcessMPEG2(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);
	ProcessH264(0, 0, 0, 0, 0, 0, -1, 0, 0, NULL);

	return 0;
}

int Player::AudioThread(void* ptr)
{
	SDL_Delay(100);
	printf("AudioThread Start\n");
	unsigned char* pReceiveBuff = 0;
	bool bNoWait = false;
	while (bSystemRun)
	{
		if (bNoWait == false) SDL_Delay(1);
		char Codec[6];
		int TOI = 0;
		int pos = 0;
		unsigned long long us = 0;
		int nReceiveSize = 0;
		if (pReceiveBuff) free(pReceiveBuff);
		pReceiveBuff = 0;
		int SessionID = -1;
		int SampleRate = 0;
		int AudioNumberOfChannels = 0;
		pReceiveBuff = g_audioQueue->pop(Codec, &TOI, &pos, &us, &nReceiveSize, &SessionID, &SampleRate, &AudioNumberOfChannels);
		if (nReceiveSize == 0)
		{
			bNoWait = false;
			continue;
		}
		if (SessionID != g_SessionID)
		{
			ProcessAAC();
			ProcessAC3();		
			SDL_CloseAudio();

			g_bAudioStarted = false;
			//printf("REMOVE PREV CHANNEL AUDIO QUEUE\n");
			bNoWait = true;
			continue;
		}
		bNoWait = false;
		//if (strcmp(Codec, "mp2") == 0) // AC3
		//{			
		//	ProcessAAC();
		//	ProcessAC3();		
		//}
		if (strcmp(Codec, "aac") == 0) // AAC
		{			
			ProcessAC3();
			ProcessAAC(1, pos, us, nReceiveSize, pReceiveBuff, SessionID, SampleRate, AudioNumberOfChannels);
		}
		if (strcmp(Codec, "ac3") == 0) // AC3
		{
			ProcessAAC();
			ProcessAC3(1, pos, us, nReceiveSize, pReceiveBuff, SessionID, SampleRate, AudioNumberOfChannels);
		}
		//if (strcmp(Codec, "mpegh") == 0) // MPEGH
		//{
		//	ProcessAAC();
		//	ProcessAC3();
		//
		//}
		if (strcmp(Codec, "reset") == 0) // CHANNEL CHANGE CODEC RESET
		{
			ProcessAAC();
			ProcessAC3();			
			SDL_CloseAudio();

			g_bAudioStarted = false;
		}
	} // while
	if (pReceiveBuff) free(pReceiveBuff);
	printf("AudioThread end\n");
	ProcessAAC();
	ProcessAC3();
#if PCM_TO_WAV_MODE
	g_wave.Close();
#endif
	SDL_CloseAudio();
	return 0;
}

// STATE 0 INACTIVE STATE 1 ACTIVE
int Player::ProcessAAC(int State, int Pos, unsigned long long us, int nReceiveSize, unsigned char* pReceiveBuff, int SessionID, int SampleRate, int AudioNumberOfChannels)
{
	static AVFrame* pFrame = 0;
	static AVCodecContext* codecCtx = 0;
	static AVCodec* codec = 0;
	if (State == 0)
	{
		if (codecCtx) avcodec_close(codecCtx);
		codecCtx = 0;
		codec = 0;
		if (pFrame) av_frame_free(&pFrame);
		pFrame = 0;
		return 0;
	}
	if (codec == 0) codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
	if (codec == 0)
	{
		printf("AV_CODEC_ID_AAC not supported\n");
		return -1;
	}
	if (codecCtx == 0)
	{
		codecCtx = avcodec_alloc_context3(codec);
		if (codecCtx == 0)
		{
			printf("codecCtx null\n");
			return -3;
		}
		//S_AT3MW_AUDIO_INFO info;
		//AT3MW_GetAudioInfo(&info);
		codecCtx->sample_rate = SampleRate; // info.ulsampleRate;
		codecCtx->channels = AudioNumberOfChannels; // info.ulch;
		int err = avcodec_open2(codecCtx, codec, NULL);
		if (err != 0)
		{
			printf("avcodec_open2 err null\n");
			return -2;
		}
	}
	if (codecCtx == 0)
	{
		printf("codecCtx null\n");
		return -3;
	}
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = pReceiveBuff;
	packet.size = nReceiveSize;
	if (pFrame == 0) pFrame = av_frame_alloc();
	int ret = avcodec_send_packet(codecCtx, &packet);
	if (ret < 0)
	{
		printf("avcodec_send_packet %d\n", ret);
		ProcessAAC();
		return -4;
	}
	ret = avcodec_receive_frame(codecCtx, pFrame);
	if (ret < 0)
	{
		printf("avcodec_receive_frame=%d\n", ret);
		ProcessAAC();
		return -5;
	}
	int data_size = av_samples_get_buffer_size(NULL, codecCtx->channels, pFrame->nb_samples, codecCtx->sample_fmt, 1);
	if (codecCtx->sample_fmt != AV_SAMPLE_FMT_FLTP)
	{
		printf("Unsupported codecCtx->sample_fmt=%d\n", codecCtx->sample_fmt);
		ProcessAAC();
		return -6;
	}
	unsigned int* p32l = (unsigned int*)pFrame->data[0];
	unsigned int* p32r = (unsigned int*)pFrame->data[1];
	for (int i = 0; i < pFrame->nb_samples; i++)
	{
		unsigned long long play_us = us + ((1000000 * i) / codecCtx->sample_rate);
		g_playaudioQueue->push(p32l[i], p32r[i], play_us, SessionID);
	}
	//printf("ProcessAAC\n");
	if (g_bAudioStarted == true) return 0;
	SDL_CloseAudio();
	SDL_AudioSpec   wanted_spec, spec;
	wanted_spec.freq = codecCtx->sample_rate;
	wanted_spec.format = AUDIO_F32SYS;
	wanted_spec.channels = codecCtx->channels;
	wanted_spec.silence = 0;
	wanted_spec.samples = pFrame->nb_samples;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = codecCtx;
	if (SDL_OpenAudio(&wanted_spec, &spec) < 0)
	{
		printf("SDL_OpenAudio: %s\n", SDL_GetError());
		return -7;
	}
	SDL_PauseAudio(0);
	g_bAudioStarted = true;
	return 0;
}

int Player::ProcessAC3(int State, int Pos, unsigned long long us, int nReceiveSize, unsigned char* pReceiveBuff, int SessionID, int SampleRate, int AudioNumberOfChannels)
{
	static unsigned int prev_SampleRate = 0, prev_AudioNumberOfChannels = 0;
	static uint8_t** pNewFrame = 0;
	static struct SwrContext* swrCtx = 0;
	static AVFrame* pFrame = 0;
	static AVCodecContext* codecCtx = 0;
	static AVCodec* codec = 0;
	if (State == 0)
	{
		prev_SampleRate = 0;
		prev_AudioNumberOfChannels = 0;
		if (swrCtx) swr_free(&swrCtx);
		swrCtx = 0;
		if (pNewFrame) av_freep(&pNewFrame[0]);
		av_freep(&pNewFrame);
		pNewFrame = 0;
		if (codecCtx) avcodec_close(codecCtx);
		codecCtx = 0;
		codec = 0;
		if (pFrame) av_frame_free(&pFrame);
		pFrame = 0;
		return 0;
	}
	if (prev_SampleRate != SampleRate || prev_AudioNumberOfChannels != AudioNumberOfChannels)
	{
		//printf("Samplerate or number of channels changed(%d->%d, %d->%d)\n", prev_SampleRate, SampleRate, prev_AudioNumberOfChannels, AudioNumberOfChannels);
		ProcessAC3();

		prev_SampleRate = SampleRate;
		prev_AudioNumberOfChannels = AudioNumberOfChannels;
	}
	if (codec == 0) codec = avcodec_find_decoder(AV_CODEC_ID_AC3);
	if (codec == 0)
	{
		printf("AV_CODEC_ID_AC3 not supported\n");
		return -1;
	}
	if (codecCtx == 0)
	{
		codecCtx = avcodec_alloc_context3(codec);
		if (codecCtx == 0)
		{
			printf("codecCtx null\n");
			return -3;
		}
		//S_AT3MW_AUDIO_INFO info;
		//AT3MW_GetAudioInfo(&info);
		codecCtx->sample_rate = SampleRate; // info.ulsampleRate;
		codecCtx->channels = AudioNumberOfChannels; // info.ulch;
		int err = avcodec_open2(codecCtx, codec, NULL);
		if (err != 0)
		{
			printf("avcodec_open2 err null\n");
			return -2;
		}
	}
	if (codecCtx == 0)
	{
		printf("codecCtx null\n");
		return -3;
	}
	if (swrCtx == 0)
	{
		swrCtx = swr_alloc_set_opts(swrCtx, av_get_default_channel_layout(2), codecCtx->sample_fmt, codecCtx->sample_rate,
			av_get_default_channel_layout(codecCtx->channels), codecCtx->sample_fmt, codecCtx->sample_rate, 0, NULL);
		if (swrCtx == 0)
		{
			printf("swrCtx null\n");
			return -3;
		}
		swr_init(swrCtx);
	}
	if (swrCtx == 0)
	{
		printf("swrCtx null\n");
		return -3;
	}
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = pReceiveBuff;
	packet.size = nReceiveSize;
	if (pFrame == 0) pFrame = av_frame_alloc();
	int ret = avcodec_send_packet(codecCtx, &packet);
	if (ret < 0)
	{
		printf("avcodec_send_packet %d\n", ret);
		ProcessAC3();
		return -4;
	}
	ret = avcodec_receive_frame(codecCtx, pFrame);
	if (ret < 0)
	{
		printf("avcodec_receive_frame=%d\n", ret);
		ProcessAC3();
		return -5;
	}
	int data_size = av_samples_get_buffer_size(NULL, codecCtx->channels, pFrame->nb_samples, codecCtx->sample_fmt, 1);
	if (codecCtx->sample_fmt != AV_SAMPLE_FMT_FLTP)
	{
		printf("Unsupported codecCtx->sample_fmt=%d\n", codecCtx->sample_fmt);
		ProcessAC3();
		return -6;
	}
	if (pNewFrame == 0)
	{
		ret = av_samples_alloc_array_and_samples(&pNewFrame, NULL, 2, pFrame->nb_samples, codecCtx->sample_fmt, 0);
		if (ret < 0)
		{
			printf("av_samples_alloc_array_and_samples=%d\n", ret);
			ProcessAC3();
			return -6;
		}
	}
	int converted = swr_convert(swrCtx, pNewFrame, pFrame->nb_samples, (const uint8_t**)&pFrame->data, pFrame->nb_samples);
	if (converted == 0) return 0;
	unsigned int* p32l = (unsigned int*)pNewFrame[0];
	unsigned int* p32r = (unsigned int*)pNewFrame[1];
	static unsigned long long prev_us = 0, new_us = 0;
	if (us == prev_us)
	{
		new_us += 1000000 * pFrame->nb_samples / codecCtx->sample_rate;
	}
	else
	{
		prev_us = new_us = us;
	}
	for (int i = 0; i < pFrame->nb_samples; i++)
	{

		unsigned long long play_us = new_us + ((1000000 * i) / codecCtx->sample_rate);
		g_playaudioQueue->push(p32l[i], p32r[i], play_us, SessionID);
	}
	// printf("ProcessAC3\n");
	if (g_bAudioStarted == true) return 0;
	SDL_CloseAudio();
	SDL_AudioSpec   wanted_spec, spec;
	wanted_spec.freq = codecCtx->sample_rate;
	wanted_spec.format = AUDIO_F32SYS;
	wanted_spec.channels = 2;
	wanted_spec.silence = 0;
	wanted_spec.samples = pFrame->nb_samples;
	wanted_spec.callback = audio_callback;
	wanted_spec.userdata = codecCtx;
	if (SDL_OpenAudio(&wanted_spec, &spec) < 0)
	{
		printf("SDL_OpenAudio: %s\n", SDL_GetError());
		return -7;
	}
	SDL_PauseAudio(0);
	g_bAudioStarted = true;
	return 0;
}

void Player::audio_callback(void* userdata, Uint8* stream, int len)
{
	unsigned char* p8 = (unsigned char*)malloc(len);
	memset(p8, 0, len);
	if (p8 == 0)
	{
		return;
	}
	unsigned int* p32 = (unsigned int*)p8;
	int nSamples = len / 8;
	unsigned long long us = 0;
	//printf("len = %d\n", len);
#if 1
	sAUDIO_LR lr;
	lr.l = 0;
	lr.r = 0;

	while (1)
	{
		int SessionID = g_playaudioQueue->getSessionID();
		if (SessionID == -1) break;
		if (g_playaudioQueue->getSessionID() == g_SessionID) break;
		g_playaudioQueue->pop(&lr, &us, &SessionID);
		//printf("REMOVE PREV CHANNEL AUDIO\n");
	}

	us = g_playaudioQueue->getTime_us();

	unsigned long long currPacketTime = us;
	unsigned long long currSystemTime = getTimeUs();

	if (0)
	{
		if (currSystemTime <= currPacketTime)
		{
			printf("|adec| s:%llu, p:%llu d:<%llu\n", currSystemTime, currPacketTime, currPacketTime - currSystemTime);
		}
		else
		{
			printf("|adec| s:%llu, p:%llu d:>%llu\n", currSystemTime, currPacketTime, currSystemTime - currPacketTime);
		}
	}

	static bool isPlay = false;
	if (isPlay == false)
	{
		if (currSystemTime <= currPacketTime)
		{
			unsigned long long delayTime = currPacketTime - currSystemTime;
			if (delayTime < (5 * 1000 * 1000))
			{
				memset(stream, 0, len);
				free(p8);
				//printf("AUDIO WAIT 1\n");
				return;
			}
			else
			{
				//printf("AUDIO DROP 1\n");
				while (1)
				{
					int SessionID = -1;
					bool ret = g_playaudioQueue->pop(&lr, &us, &SessionID);
					if (ret == false)
					{
						memset(stream, 0, len);
						free(p8);
						return;
					}
					if (us < currSystemTime)
					{
						break;
					}
				}
				isPlay = true;
			}
		}
		else
		{
			isPlay = true;
		}
	}
	else
	{
		if (currSystemTime <= currPacketTime)
		{
			unsigned long long delayTime = currPacketTime - currSystemTime;
			if ((100 * 1000) < delayTime)
			{
				isPlay = false;
				memset(stream, 0, len);
				free(p8);
				//printf("AUDIO WAIT 2\n");
				return;
			}
		}
		else
		{
			unsigned long long delayTime = currSystemTime - currPacketTime;
			if ((100 * 1000) < delayTime)
			{
				//printf("AUDIO DROP 2\n");
				while (1)
				{
					int SessionID = -1;
					bool ret = g_playaudioQueue->pop(&lr, &us, &SessionID);
					if (ret == false)
					{
						memset(stream, 0, len);
						free(p8);
						return;
					}
					if (currSystemTime <= us)
					{
						break;
					}
				}
			}
		}
	}

	for (int i = 0; i < nSamples; i++)
	{
		int SessionID = -1;
		if (g_playaudioQueue->pop(&lr, &us, &SessionID) == false)
		{
			memset(stream, 0, len);
			free(p8);
			return;
		}

		p32[i * 2] = lr.l;
		p32[i * 2 + 1] = lr.r;
		g_AudioPlayTime_us = us;
		g_AudioPlayTime_us_Tick = getTimeUs();
	}
#endif

	if (g_AudioMute) memset(p8, 0, len);
	memcpy(stream, p8, len);
	free(p8);
}

int Player::ProcessH264(int State = 0, int TOI = 0, int pos = 0, unsigned long long us = 0, int nReceiveSize = 0,
	unsigned char* pReceiveBuff = 0, int SessionID = -1, int Width = 0, int Height = 0, void* ptr = NULL)
{
#if 1
	static AVFrame* pFrame = 0;
	static AVCodecContext* codecCtx = 0;
	static AVCodec* codec = 0;

	if (State == 0)
	{
		if (codecCtx)
		{
			avcodec_flush_buffers(codecCtx);
			avcodec_close(codecCtx);
			avcodec_free_context(&codecCtx);

			AVPicture pict;
			memset(&pict, 0, sizeof(pict));
			pict.data[0] = (Uint8*)-1;
			pict.data[1] = (Uint8*)-1;
			pict.data[2] = (Uint8*)-1;
			g_FrameQueue->push(pict, 0, 0, 0, 0);
		}
		codecCtx = 0;
		codec = 0;
		if (pFrame) av_frame_free(&pFrame);
		pFrame = 0;
		return 0;
	}

	AVPacket packet;
	av_init_packet(&packet);
	if (pos > 0)
	{
		packet.data = pReceiveBuff;
		packet.size = nReceiveSize;
	}

	if (pos == 0) // HEVC HvcC
	{
#if 1
		printf("-->");
		for (int i = 0; i < nReceiveSize; i++) {
			printf("0x%02X ", pReceiveBuff[i]);
		}
		printf("\n");
#endif
		unsigned int tier_flag, profile_idc;
		bool bNeedResetCodec = true;
		//if (codecCtx/*/ && codecCtx->extradata_size == nReceiveSize*/)
		//{
		//	if (memcmp(codecCtx->extradata, pReceiveBuff, nReceiveSize) == 0)
		//	{
		//		printf("bNeedResetCodec is false \n");
		//		bNeedResetCodec = false;
		//	}
		//}
		if (bNeedResetCodec)
		{
			ProcessH264();
			unsigned char* HvcC = NULL;
			int nLenHvcC = 0;
			HvcC = (unsigned char*)av_mallocz(nReceiveSize);
			if (HvcC == 0) {
				printf("HvcC null\n");
				return -1;
			}
			memcpy(HvcC, pReceiveBuff, nReceiveSize);
			nLenHvcC = nReceiveSize;
			codec = avcodec_find_decoder(AV_CODEC_ID_H264);
			std::cout << "codec = " << codec << std::endl;
			if (codec == 0)
			{
				printf("AV_CODEC_ID_H264 not supported\n");
				return -2;
			}
			codecCtx = avcodec_alloc_context3(codec);
			if (codecCtx == 0)
			{
				printf("codecCtx null\n");
				return -3;
			}
			codecCtx->profile = FF_PROFILE_H264_BASELINE;	//위키에서는 DMB는 baseline이라고는 하는데....

			codecCtx->extradata = HvcC;
			codecCtx->extradata_size = nLenHvcC;
			int err = avcodec_open2(codecCtx, codec, NULL);
			if (err != 0)
			{
				printf(" avcodec_open2 err null\n");
				return -4;
			}
		}
		printf("리턴!!\n");
		return 0;
	}
	printf("111111111111111111111\n");
	if (codecCtx == 0) {
		printf("codecCtx is 0 !!!!\n");
		return -3; //임시 주석
	}

	if (pFrame == 0) pFrame = av_frame_alloc();
	int frameFinished = 0;
	int ret = avcodec_decode_video2(codecCtx, pFrame, &frameFinished, &packet);

	if (ret != nReceiveSize)
	{
		printf("fail to video decode!!! ret=%d toi=%d, pos=%d\n", ret, TOI, pos);

		avcodec_flush_buffers(codecCtx);	

		return -6;
	}

	// Did we get a video frame?
	if (frameFinished == 0) return 0;

	//printf("nScreenWidth=%d nScreenHeight=%d\n", nScreenWidth, nScreenHeight);
	int nTargetBufferSize = pFrame->width * pFrame->height;
	AVPicture pict;
	pict.data[0] = (Uint8*)malloc(nTargetBufferSize); // Y
	pict.data[1] = (Uint8*)malloc(nTargetBufferSize / 4); // U
	pict.data[2] = (Uint8*)malloc(nTargetBufferSize / 4); // V
	pict.linesize[0] = pFrame->width;
	pict.linesize[1] = pFrame->width / 2;
	pict.linesize[2] = pFrame->width / 2;
	if (pict.data[0] == 0 || pict.data[1] == 0 || pict.data[2] == 0)
	{
		printf("insufficient memory YUV\n");
		return -7;
	}
	struct SwsContext* sws_ctx = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
	sws_scale(sws_ctx, (uint8_t const* const*)pFrame->data, pFrame->linesize, 0, pFrame->height, pict.data, pict.linesize);
	sws_freeContext(sws_ctx);
	printf("pFrame->width=%d, pFrame->height=%d\n", pFrame->width, pFrame->height);
#if 1
	if (g_FrameQueue->push(pict, us, pFrame->width, pFrame->height, SessionID) == false)
	{
		free(pict.data[0]);
		free(pict.data[1]);
		free(pict.data[2]);
	} else printf("push pFrame->width=%d, pFrame->height=%d\n", pFrame->width, pFrame->height);
#else
	while (g_FrameQueue->push(pict, us, pFrame->width, pFrame->height, SessionID) == false)
	{
		SDL_Delay(1);
		if (bSystemRun == false)
		{
			free(pict.data[0]);
			free(pict.data[1]);
			free(pict.data[2]);
			break;
		}
	}
#endif
	return 0;
#endif
}

int Player::ProcessMPEG2(int State = 0, int TOI = 0, int pos = 0, unsigned long long us = 0, int nReceiveSize = 0, unsigned char* pReceiveBuff = 0, int SessionID = -1, int Width = 0, int Height = 0, void* ptr = NULL)
{
	static AVFrame* pFrame = 0;
	static AVCodecContext* codecCtx = 0;
	static AVCodec* codec = 0;
	static unsigned long long prev_us = 0, psudo_us = 0;
	if (State == 0)
	{
		if (codecCtx)
		{
			avcodec_flush_buffers(codecCtx);
			avcodec_close(codecCtx);

			avcodec_free_context(&codecCtx);

			AVPicture pict;
			memset(&pict, 0, sizeof(pict));
			pict.data[0] = (Uint8*)-1;
			pict.data[1] = (Uint8*)-1;
			pict.data[2] = (Uint8*)-1;
			g_FrameQueue->push(pict, 0, 0, 0, 0);
		}
		codecCtx = 0;
		codec = 0;
		if (pFrame) av_frame_free(&pFrame);
		pFrame = 0;
		prev_us = 0;
		psudo_us = 0;
		return 0;
	}
	AVPacket packet;
	av_init_packet(&packet);
	if (pos > 0)
	{
		packet.data = pReceiveBuff;
		packet.size = nReceiveSize;
	}
	if (pos == 0) // MPEG2 Sequence Header
	{
		//printf("MPEG2 Sequence Header\n");
		bool bNeedResetCodec = true; // mpeg2는 bNeedResetCodec를 최소화한다. 테스트 필요 2019.12.18 cglee
		if (codecCtx)
		{
			if (memcmp(codecCtx->extradata, pReceiveBuff, 11) == 0 &&
				(codecCtx->extradata[11] & 0xf8) == (pReceiveBuff[11] & 0xf8))
			{
				bNeedResetCodec = false;
			}
		}
		if (bNeedResetCodec)
		{
			ProcessMPEG2();
			unsigned char* Seq = NULL;
			int nLenSeq = 0;
			Seq = (unsigned char*)av_mallocz(nReceiveSize);
			if (Seq == 0) {
				printf("Seq null\n");
				return -1;
			}
			memcpy(Seq, pReceiveBuff, nReceiveSize);
			nLenSeq = nReceiveSize;
			codec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
			if (codec == 0)
			{
				printf("AV_CODEC_ID_MPEG2VIDEO not supported\n");
				return -2;
			}
			codecCtx = avcodec_alloc_context3(codec);
			if (codecCtx == 0)
			{
				printf("codecCtx null\n");
				return -3;
			}
			codecCtx->profile = FF_PROFILE_MPEG2_MAIN;

			codecCtx->extradata = Seq;
			codecCtx->extradata_size = nLenSeq;
			int err = avcodec_open2(codecCtx, codec, NULL);
			if (err != 0)
			{
				printf(" avcodec_open2 err null\n");
				return -4;
			}
		}
		return 0;
	}
	if (codecCtx == 0) return -3;
	// Decode video frame
#if 0   // FFMPEG NEW API
	int ret = avcodec_send_packet(codecCtx, &packet);
	if (ret < 0)
	{
		printf("avcodec_send_packet ret =%d\n", ret);
		return -5;
	}
	ret = avcodec_receive_frame(codecCtx, pFrame);
	if (ret < 0)
	{
		printf("avcodec_receive_frame ret =%d\n", ret);
		return -6;
	}
#else // FFMPEG_OLD_API

	if (pFrame == 0) pFrame = av_frame_alloc();
	int frameFinished = 0;
	int ret = avcodec_decode_video2(codecCtx, pFrame, &frameFinished, &packet);
	if (ret != nReceiveSize)
	{
		printf("avcodec_decode_video2 %d\n", ret);

		avcodec_flush_buffers(codecCtx);

		return -6;
	}
	// Did we get a video frame?
	if (frameFinished == 0) return 0;
#endif
	if (prev_us == us)
	{
		psudo_us += 1000000 * codecCtx->framerate.den / codecCtx->framerate.num;
		us = psudo_us;
	}
	else
	{
		prev_us = psudo_us = us;
	}
	 
	//printf("nScreenWidth=%d nScreenHeight=%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
	int nTargetBufferSize = pFrame->width * pFrame->height;
	AVPicture pict;
	pict.data[0] = (Uint8*)malloc(nTargetBufferSize); // Y
	pict.data[1] = (Uint8*)malloc(nTargetBufferSize / 4); // U
	pict.data[2] = (Uint8*)malloc(nTargetBufferSize / 4); // V
	pict.linesize[0] = pFrame->width;
	pict.linesize[1] = pFrame->width / 2;
	pict.linesize[2] = pFrame->width / 2;
	if (pict.data[0] == 0 || pict.data[1] == 0 || pict.data[2] == 0)
	{
		printf("insufficient memory YUV\n");
		return -7;
	}
	struct SwsContext* sws_ctx = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
	sws_scale(sws_ctx, (uint8_t const* const*)pFrame->data, pFrame->linesize, 0, pFrame->height, pict.data, pict.linesize);
	sws_freeContext(sws_ctx);

	if (g_FrameQueue->push(pict, us, pFrame->width, pFrame->height, SessionID) == false)
	{
		printf("pict is free!!\n");
		free(pict.data[0]);
		free(pict.data[1]);
		free(pict.data[2]);
	}

	return 0;
}

void Player::play() {
	int i = 1000;
	while (1) {
		printf("play Thread = %d\n", i++);
		Sleep(1000);
	}
}

//void Player::playerThread() {
//	t = std::thread(&Player::mainLoop, this);
//}

void Player::start(std::string source, std::string output) {
	std::cout << "start!!!" << std::endl;

	TsParser* ts = new TsParser(source, output);
	ts->startThread(AT3APP_AvCallbackSub);	//callback등록

	//Player::mainLoop();

	/*playerThread();
	if (t.joinable()) {
		t.join();
	}
	*/

	ts->joinThread();
}

void Player::mainLoop() {
	std::cout << "mainLoop()" << std::endl;
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("Err|SDL_Init - %s\n", SDL_GetError());
		return;
	}

	if (!initializeSDL()) {
		std::cerr << "Failed to initialized SDL!!" << std::endl;
		abort;
	}
	else {
		pixelData = new unsigned char[SCREEN_WIDTH * SCREEN_HEIGHT * 4];
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
	}

	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(window, &wmInfo);
	int backbuf_width, backbuf_height;
	backbuf_width = GetSystemMetrics(SM_CXSCREEN);
	backbuf_height = GetSystemMetrics(SM_CYSCREEN);
	g_d3d_ctx = d3d_init(wmInfo.info.win.window, backbuf_width, backbuf_height);
	if (!g_d3d_ctx)
	{
		printf("Err|d3d_init\n");

		g_bForceSwDecoding = true;
	}

	IDirect3DSurface9* backbufSurface = NULL;
	if (g_d3d_ctx)
	{
		backbufSurface = d3d_create_surface_from_backbuffer(g_d3d_ctx->d3ddevice);
		if (!backbufSurface)
		{
			printf("Err|d3d_create_surface_from_backbuffer\n");

			g_bForceSwDecoding = true;
		}
	}

	IDirect3DSurface9* videoSurface = NULL;
	if (g_d3d_ctx)
	{
		videoSurface = d3d_create_render_target_surface(g_d3d_ctx->d3ddevice, backbuf_width, backbuf_height, D3DFMT_X8R8G8B8);
		if (!videoSurface)
		{
			printf("Err|d3d_create_render_target_surface\n");

			g_bForceSwDecoding = true;
		}
	}

	//초기화
	g_videoQueue = new SimpleQueue;
	g_FrameQueue = new FrameQueue;
	g_audioQueue = new SimpleQueue;
	g_playaudioQueue = new ByteQueue;

	SDL_Thread* pVideoThread = SDL_CreateThread(VideoThread, "VideoThread", (void*)videoSurface);
	SDL_Thread* pAudioThread = SDL_CreateThread(AudioThread, "AudioThread", 0);

	//MW 코드 실행 ( CB등록 및 실행 )
	//std::string sourcePath = "C:\\Work\\STREAM\\ts\\test2.ts";	//mpeg2

#if 0
	std::string sourcePath = "C:\\Work\\STREAM\\ts\\av_s1.ts";	//h264
#else	
	std::string sourcePath = "C:\\Work\\STREAM\\dmb\\myMBC.ts";	//DMB	
#endif
	std::string outputPath = "ouputs/parser.txt";
	TsParser *ts = new TsParser(sourcePath, outputPath);
	ts->startThread(AT3APP_AvCallbackSub);

	std::cout << " mainLoop!!!!! Before while:" << bSystemRun << std::endl;
	while (bSystemRun) {
		static int nCount = 0;
		nCount++;
		if ((nCount % 1000) == 0)
		{
			/*printf("g_videoQueue.GetCountPercent() = %d\n", g_videoQueue->GetCountPercent());
			printf("g_audioQueue.GetCountPercent() = %d\n", g_audioQueue->GetCountPercent());
			printf("g_FrameQueue.GetCountPercent() = %d\n", g_FrameQueue->GetCountPercent());*/
		}

		SDL_Delay(1);
		SDL_Event event;
		SDL_PollEvent(&event);
		switch (event.type)
		{
		case SDL_QUIT:
			printf("SDL_QUIT\n");
			bSystemRun = false;
			break;
		default:
			break;
		}

		static unsigned long long ulLastPresentTime = 0; // 마지막으로 화면에 출력했던 시간
		AVPicture pict;
		memset(&pict, 0, sizeof(pict));
		unsigned short width = 0;
		unsigned short height = 0;
		unsigned long long us = 0;
		int SessionID = -1;

		// 비디오 데이터가 없어도 화면 갱신했던 시간이 50ms가 지났다면 화면 갱신
		if (g_FrameQueue->pop(&pict, &us, &width, &height, &SessionID) == false && getTimeUs() - ulLastPresentTime < 50 * 1000) {
			//std::cout << "continue!!" << std::endl;
			continue;
		}

		if (pict.data[0] == (Uint8*)-1 && pict.data[1] == (Uint8*)-1 && pict.data[2] == (Uint8*)-1)
		{
			std::cout << "1111" << std::endl;
			// video clear
			if (!g_bForceSwDecoding)
			{
				d3d_fill_color(g_d3d_ctx->d3ddevice, videoSurface, NULL, D3DCOLOR_ARGB(0, 0, 0, 0));
				d3d_fill_color(g_d3d_ctx->d3ddevice, backbufSurface, NULL, D3DCOLOR_ARGB(0, 0, 0, 0));
				d3d_present(g_d3d_ctx->d3ddevice);
			}
			else
			{
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
				SDL_RenderClear(renderer);
				SDL_RenderPresent(renderer);
			}
			ulLastPresentTime = getTimeUs(); // 마지막 화면 표시 시간 갱신
			std::cout << "22222" << std::endl;
			continue;
		}

		//해당 부분을 주석처리하면 화면은 깜빡임.
		if (SessionID != -1 && SessionID != g_SessionID)
		{
			printf("SessionID = %d, gSessionID = %d\n");
		//	//printf("REMOVE PREV CHANNEL VIDEO FRAME\n");
			if (pict.data[0]) free(pict.data[0]);
			if (pict.data[1]) free(pict.data[1]);
			if (pict.data[2]) free(pict.data[2]);
			continue;
		}

		//printf("g_bForceSwDecoding = [%d]\n", g_bForceSwDecoding); 
		g_bForceSwDecoding = true;
		if (!g_bForceSwDecoding)
		{
			if (!pict.data[0] && !pict.data[1] && !pict.data[2])
			{
				d3d_copy_surface(g_d3d_ctx->d3ddevice, videoSurface, NULL, backbufSurface, NULL);
			}

			d3d_present(g_d3d_ctx->d3ddevice);
			ulLastPresentTime = getTimeUs(); // 마지막 화면 표시 시간 갱신
			//continue;	//해당 부분을 오픈하면 화면이 표시 안됨. 
		}

		if (!pict.data[0] && !pict.data[1] && !pict.data[2])
		{
			continue;
		}
		//std::cout << "#######################################" << std::endl;
		SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
		SDL_UpdateYUVTexture(texture, NULL, pict.data[0]/*yPlane*/, width, pict.data[1]/*uPlane*/, width / 2, pict.data[2]/*vPlane*/, width / 2);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_DestroyTexture(texture);
		
		unsigned long long currPacketTime = us;
		unsigned long long currSystemTime = getTimeUs();
		//해당 기능을 활성화 해야 화면이 보임. 
		if (currSystemTime <= currPacketTime)
		{
			unsigned long long delayTime = currPacketTime - currSystemTime;
			if (delayTime < (5 * 1000 * 1000))
			{
				SDL_Delay(Uint32(delayTime / 1000));

				SDL_RenderPresent(renderer);
				//	g_VideoPlayTime_us = us;
				//	g_VideoPlayTime_us_Tick = getTimeUs();
			}
			else
			{
				//printf("EARLY VIDEO %lluus\n", delayTime);
			}
		}
		else
		{
			SDL_RenderPresent(renderer);	//화면을 그리는 역할.
			//g_VideoPlayTime_us = us;
			//g_VideoPlayTime_us_Tick = getTimeUs();

			//printf("DELAYED VIDEO %uus\n", (unsigned int)(currSystemTime - currPacketTime));
		}

		if (pict.data[0]) free(pict.data[0]);
		if (pict.data[1]) free(pict.data[1]);
		if (pict.data[2]) free(pict.data[2]);
	}

	/*int ThreadReturnValue = 0;
	if (pVideoThread) SDL_WaitThread(pVideoThread, &ThreadReturnValue);
	printf("Video Thread returned value: %d\n", ThreadReturnValue);

	if (g_videoQueue)
	{
		delete g_videoQueue;
		printf("delete g_videoQueue\n");
	}
	if (videoSurface)
	{
		d3d_free_surface(videoSurface);
	}

	cleanUpSDL();	*/

	std::cout << "endLoop()" << std::endl;
	if (g_videoQueue)
	{
		delete g_videoQueue;
		printf("delete g_videoQueue\n");
	}
	if (videoSurface)
	{
		d3d_free_surface(videoSurface);
	}
	if (backbufSurface)
	{
		d3d_free_surface(backbufSurface);
	}
	if (g_d3d_ctx)
	{
		//d3d_uninit(g_d3d_ctx);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Player::d3d_free_surface(IDirect3DSurface9* surface)
{
	if (!surface) {
		printf("NULL D3D Surface\n");
		return;
	}

	surface->Release();
}

void Player::d3d_present(IDirect3DDevice9* device)
{
	HRESULT hr;

	if (!device) {
		printf("NULL D3D Device\n");
		return;
	}

	hr = device->Present(NULL, NULL, NULL, NULL);
	if (FAILED(hr)) {
		printf("Failed to present Direct3D device\n");
	}
}

void Player::d3d_fill_color(IDirect3DDevice9* device, IDirect3DSurface9* surface, RECT* rect, D3DCOLOR color)
{
	HRESULT hr;

	if (!device) {
		printf("NULL D3D Device\n");
		return;
	}

	if (!surface) {
		printf("NULL D3D Surface\n");
		return;
	}

	device->BeginScene();

	hr = device->ColorFill(surface, rect, color);
	if (FAILED(hr)) {
		printf("Failed to fill Direct3D surface\n");
	}

	device->EndScene();
}

void Player::d3d_copy_surface(IDirect3DDevice9* device, IDirect3DSurface9* src, RECT* src_rect, IDirect3DSurface9* dst, RECT* dst_rect)
{
	HRESULT hr;

	if (!device) {
		printf("NULL D3D Device\n");
		return;
	}

	if (!src) {
		printf("NULL D3D Src Surface\n");
		return;
	}

	if (!dst) {
		printf("NULL D3D Dst Surface\n");
		return;
	}

	device->BeginScene();

	hr = device->StretchRect(src, src_rect, dst, dst_rect, D3DTEXF_LINEAR);
	if (FAILED(hr)) {
		printf("Failed to stretch Direct3D surface\n");
	}

	device->EndScene();
}


struct d3d_context;

d3d_context* Player::d3d_init(HWND hwnd, int backbuf_width, int backbuf_height)
{
	HRESULT hr;

	if (!hwnd) {
		printf("NULL HWND\n");
		return NULL;
	}

	HMODULE d3dlib;
	d3dlib = LoadLibrary(L"d3d9.dll");
	if (!d3dlib) {
		printf("Failed to load D3D9 library\n");
		return NULL;
	}

	IDirect3D9* (WINAPI * Direct3DCreate9)(UINT);
	Direct3DCreate9 = (IDirect3D9 * (WINAPI*)(UINT))GetProcAddress(d3dlib, "Direct3DCreate9");
	if (!Direct3DCreate9) {
		printf("Failed to locate Direct3DCreate9\n");
		return NULL;
	}

	IDirect3D9* d3d;
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d) {
		printf("Failed to create Direct3D object\n");
		return NULL;
	}

	D3DPRESENT_PARAMETERS d3dpp;
	memset(&d3dpp, 0, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.BackBufferWidth = backbuf_width;
	d3dpp.BackBufferHeight = backbuf_height;
	d3dpp.BackBufferCount = 0;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	d3dpp.FullScreen_RefreshRateInHz = 0;

	IDirect3DDevice9* d3ddevice;
	hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
		&d3dpp, &d3ddevice);
	if (FAILED(hr)) {
		printf("Failed to create Direct3D device\n");
		return NULL;
	}

	d3ddevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3ddevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3ddevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
	d3ddevice->SetRenderState(D3DRS_ALPHAREF, 0x0);
	d3ddevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	d3ddevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	d3d_context* ctx;
	ctx = (d3d_context*)malloc(sizeof(d3d_context));
	if (!ctx) {
		printf("Failed to allocate context\n");
		return NULL;
	}
	ctx->d3dlib = d3dlib;
	ctx->d3d = d3d;
	ctx->d3ddevice = d3ddevice;

	return ctx;
}

IDirect3DSurface9* Player::d3d_create_surface_from_backbuffer(IDirect3DDevice9* device)
{
	HRESULT hr;

	if (!device) {
		printf("NULL D3D Device\n");
		return NULL;
	}

	IDirect3DSurface9* surface;
	hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);
	if (FAILED(hr)) {
		printf("Failed to get Direct3D back buffer\n");
		return NULL;
	}

	return surface;
}

IDirect3DSurface9* Player::d3d_create_render_target_surface(IDirect3DDevice9* device, int width, int height, D3DFORMAT format)
{
	HRESULT hr;

	if (!device) {
		printf("NULL D3D Device\n");
		return NULL;
	}

	IDirect3DSurface9* surface;
	hr = device->CreateRenderTarget(width, height, format, D3DMULTISAMPLE_NONE, 0, TRUE, &surface, NULL);
	if (FAILED(hr)) {
		printf("Failed to create Direct3D render target surface\n");
		return NULL;
	}

	return surface;
}

void Player::avPlayerSDL(unsigned char* data, int size) {
	static AVFrame* pFrame = 0;
	static AVCodecContext* codecCtx = 0;
	static AVCodec* codec = 0;
	AVPacket packet;

	av_init_packet(&packet);
	packet.data = data;
	packet.size = size;

	if (pFrame == 0) pFrame = av_frame_alloc();
	int frameFinished = 0;
	int ret = avcodec_decode_video2(codecCtx, pFrame, &frameFinished, &packet);
	//int ret = avcodec_send_packet(codecCtx, &packet);
	if (ret != size)
	{
		printf("avcodec_decode_video2 %d\n", ret);

		avcodec_flush_buffers(codecCtx);

		return;
	}
	if (frameFinished == 0) return;

	int nTargetBufferSize = pFrame->width * pFrame->height;
	AVPicture pict;
	//AVFrame pict;
	pict.data[0] = (Uint8*)malloc(nTargetBufferSize); // Y
	pict.data[1] = (Uint8*)malloc(nTargetBufferSize / 4); // U
	pict.data[2] = (Uint8*)malloc(nTargetBufferSize / 4); // V
	pict.linesize[0] = pFrame->width;
	pict.linesize[1] = pFrame->width / 2;
	pict.linesize[2] = pFrame->width / 2;
	if (pict.data[0] == 0 || pict.data[1] == 0 || pict.data[2] == 0)
	{
		printf("insufficient memory YUV\n");
		return;
	}
	struct SwsContext* sws_ctx = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
	sws_scale(sws_ctx, (uint8_t const* const*)pFrame->data, pFrame->linesize, 0, pFrame->height, pict.data, pict.linesize);
	sws_freeContext(sws_ctx);

	SDL_UpdateYUVTexture(texture, NULL, pict.data[0], SCREEN_WIDTH, pict.data[1], SCREEN_WIDTH / 2, pict.data[2], SCREEN_WIDTH / 2);
	renderFrame();
}

void Player::AT3APP_AvCallbackSub(const char* codec, int TOI, int pos, unsigned long long decode_time_us, unsigned int data_length, unsigned char* data, int SessionID, unsigned long long minBufferTime, int param1, int param2)
{
	printf("[%s] %s %d %d %llu %u\n", __func__, codec, TOI, pos, decode_time_us, data_length);
	g_SessionID = SessionID;
	decode_time_us = 1;
	static unsigned long long first_video_decode_time_us = 0, first_decode_time_us = 0, first_system_time_us = 0, offset_time_us = 0;
	if (strcmp(codec, "aac") == 0 || strcmp(codec, "ac3") == 0 || strcmp(codec, "mpegh") == 0 || strcmp(codec, "mp2") == 0 || strcmp(codec, "hevc") == 0 || strcmp(codec, "mpeg2") == 0
		|| strcmp(codec, "h264") == 0)
	{
		if (decode_time_us)
		{
			if (first_decode_time_us)
			{
				unsigned long long new_decode_time_us = decode_time_us - first_decode_time_us + first_system_time_us;
				unsigned long long system_time_us = getTimeUs();
				if (new_decode_time_us < system_time_us)
				{
					if ((5 * 1000 * 1000) < (system_time_us - new_decode_time_us))
					{
						first_decode_time_us = 0;

						printf("WRAP AROUND REWIND sys %llu dec %llu diff %llu\n",
							system_time_us, new_decode_time_us, system_time_us - new_decode_time_us);
					}
				}
				else
				{
					if ((5 * 1000 * 1000) < (new_decode_time_us - system_time_us))
					{
						first_decode_time_us = 0;

						printf("WRAP AROUND FORWARD sys %llu dec %llu diff %llu\n",
							system_time_us, new_decode_time_us, new_decode_time_us - system_time_us);
					}
				}
			}

			if (first_decode_time_us == 0)
			{
				first_video_decode_time_us = 0;
				first_decode_time_us = decode_time_us;
				first_system_time_us = getTimeUs();
				offset_time_us = minBufferTime + (100 * 1000);

				//printf("offset_time_us = %llu\n", offset_time_us);
			}
		}
	}
	if (strcmp(codec, "reset") == 0)
	{
		g_SessionID = SessionID;
		printf("codec is reset. gVideo/gAudio queue push!!");
		g_audioQueue->push(codec, 0, 0, 0, (unsigned int)strlen(codec), (unsigned char*)codec, SessionID, 0, 0); // Simple Queue의 reset 함수에서 초기화하기 위해서는 더미 데이터가 필요하다.
		g_videoQueue->push(codec, 0, 0, 0, (unsigned int)strlen(codec), (unsigned char*)codec, SessionID, 0, 0); // Simple Queue의 reset 함수에서 초기화하기 위해서는 더미 데이터가 필요하다.
	}
	if (strcmp(codec, "aac") == 0 || strcmp(codec, "ac3") == 0 || strcmp(codec, "mpegh") == 0 || strcmp(codec, "mp2") == 0)
	{
		if (first_decode_time_us)
		{
			unsigned long long new_decode_time_us = decode_time_us - first_decode_time_us + first_system_time_us;
			g_audioQueue->push(codec, TOI, pos, new_decode_time_us + offset_time_us, data_length, data, SessionID, param1, param2);
			//printf("|mpegh| %llu, %llu\n", decode_time_us, new_decode_time_us + offset_time_us);
		}
	}
	if (strcmp(codec, "hevc") == 0 || strcmp(codec, "mpeg2") || strcmp(codec, "h264") == 0)
	{		
		if (first_decode_time_us || pos == 0)
		{
			unsigned long long video_offset_time_us;
			video_offset_time_us = offset_time_us;

			unsigned long long new_decode_time_us = decode_time_us - first_decode_time_us + first_system_time_us;
			std::cout << "gVideoQueue push " << "[" << pos << "]" << std::endl;
			g_videoQueue->push(codec, TOI, pos, new_decode_time_us + video_offset_time_us, data_length, data, SessionID, param1, param2);			
			//printf("|hevc| %llu, %llu\n", decode_time_us, new_decode_time_us + offset_time_us);
		}
	}
}

#pragma warning(pop)