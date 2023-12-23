#include <music_manag.h>
#include <mad.h>
#include <alsa/asoundlib.h>
#include <print_manag.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct DataFrameHeader
{
	unsigned int bzFrameSyncFlag1:8;   /* È«Îª 1 */
	unsigned int bzProtectBit:1;       /* CRC */
	unsigned int bzVersionInfo:4;      /* °üÀ¨ mpeg °æ±¾£¬layer °æ±¾ */
	unsigned int bzFrameSyncFlag2:3;   /* È«Îª 1 */
	unsigned int bzPrivateBit:1;       /* Ë½ÓÐ */
	unsigned int bzPaddingBit:1;      /* ÊÇ·ñÌî³ä£¬1 Ìî³ä£¬0 ²»Ìî³ä
	layer1 ÊÇ 4 ×Ö½Ú£¬ÆäÓàµÄ¶¼ÊÇ 1 ×Ö½Ú */
	unsigned int bzSampleIndex:2;     /* ²ÉÑùÂÊË÷Òý */
	unsigned int bzBitRateIndex:4;    /* bit ÂÊË÷Òý */
	unsigned int bzExternBits:6;      /* °æÈ¨µÈ£¬²»¹ØÐÄ */	
	unsigned int bzCahnnelMod:2;      /* Í¨µÀ
	* 00 - Stereo 01 - Joint Stereo
	* 10 - Dual   11 - Single
	*/
};

/* IDxVx µÄÍ·²¿½á¹¹ */
struct IDxVxHeader
{
    unsigned char aucIDx[3];     /* ±£´æµÄÖµ±ÈÈçÎª"ID3"±íÊ¾ÊÇID3V2 */
    unsigned char ucVersion;     /* Èç¹ûÊÇID3V2.3Ôò±£´æ3,Èç¹ûÊÇID3V2.4Ôò±£´æ4 */
    unsigned char ucRevision;    /* ¸±°æ±¾ºÅ */
    unsigned char ucFlag;        /* ´æ·Å±êÖ¾µÄ×Ö½Ú */
    unsigned char aucIDxSize[4]; /* Õû¸ö IDxVx µÄ´óÐ¡£¬³ýÈ¥±¾½á¹¹ÌåµÄ 10 ¸ö×Ö½Ú */
    /* Ö»ÓÐºóÃæ 7 Î»ÓÐÓÃ */
};

struct PcmFmtParams
{
    unsigned char *pucDataStart;
    unsigned long dwDataLength;
	/* ÒªÍ³¼Æ¸èÇú²¥·Å³¤¶È£¬µ«ÊÇ dwDataLength
	 * ±»½âÂëº¯ÊýµÄ input ÖÃ 0£¬Ö»ÄÜÔÚÏÂÃæ±¸·ÝÒ»ÏÂ
	 */
	unsigned long dwDataLengthBak;
	unsigned char *pucDataEnd;    
};

static int MP3MusicDecoderInit(void);
static void MP3MusicDecodeExit(void);
static int isSupportMP3(struct FileDesc *ptFileDesc);
static int MP3MusicPlay(struct FileDesc *ptFileDesc);
static int MP3MusicCtrl(enum MusicCtrlCode eCtrlCode);

static int SetMp3OutPcmFmt(void);
static int SkipIDxVxContents(struct IDxVxHeader *ptIDxVxHeader);
static int GetPlayTimeForFile(unsigned char *ptDataFrameIndex, int iFileSize);

static int g_aiMP3BitRateIndex[] = {
	0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};
static int g_aiMP3SampleRateIndex[] = {
	44100, 48000, 32000, 0,
};

#define VOL_CHANGE_NUM 2
#define DEFAULT_VOL_VAL "10"
static char g_cVolValue = 10;    /* Ä¬ÈÏµÄÉùÒôÖµ */
static int g_iRuntimeRatio = 0;

static unsigned char g_bMp3DecoderExit = 0;
static snd_pcm_t *g_ptMP3PlaybackHandle;

static pthread_t g_MP3PlayThreadId;
static pthread_once_t g_PthreadOnce = PTHREAD_ONCE_INIT;
static struct PcmFmtParams g_tMP3PcmFmtParams;

static pthread_mutex_t g_tMutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_tConVar = PTHREAD_COND_INITIALIZER;
static pthread_t g_MP3TimeThreadId;
static int g_iMP3PlayTime;
static char g_bMusicHalt = 0;
static char g_bMP3TimeThreadExit = 1;
static char g_bMP3PlayThreadExit = 1;

struct MusicParser g_tMP3MusicParser = {
	.name = "mp3",
	.MusicDecoderInit = MP3MusicDecoderInit,
	.MusicDecodeExit  = MP3MusicDecodeExit,
	.isSupport = isSupportMP3,
	.MusicPlay = MP3MusicPlay,
	.MusicCtrl = MP3MusicCtrl,
};

static snd_pcm_t *OpenPcmDev(char *strDevName, snd_pcm_stream_t eSndPcmStream)
{
	snd_pcm_t *SndPcmHandle;
	int iError = 0;

	iError = snd_pcm_open (&SndPcmHandle, strDevName, eSndPcmStream, 0);
	if (iError < 0) {
		return NULL;
	}

	return SndPcmHandle;
}

static int SetHarwareParams(snd_pcm_t *ptPcmHandle ,struct PcmHardwareParams *ptPcmHWParams)
{
	int iError = 0;
	snd_pcm_hw_params_t *HardWareParams;
	snd_pcm_format_t ePcmFmt = SND_PCM_FORMAT_S16_LE; /* Ä¬ÈÏÖµ */
	unsigned int dwSampRate = 44100; /* Ä¬ÈÏÖµ */

	/* ¸ñÊ½Ê¶±ð */
	switch(ptPcmHWParams->eFmtMod){
		case 8: {
			ePcmFmt = SND_PCM_FORMAT_S8;			
			break;
		}
		case 16: {
			ePcmFmt = SND_PCM_FORMAT_S16_LE;
			break;
		}
		case 24: {
			ePcmFmt = SND_PCM_FORMAT_S24_LE;
			break;
		}
		case 32: {
			ePcmFmt = SND_PCM_FORMAT_S32_LE;
			break;
		}
		default : {
			DebugPrint("Unsupported format bits : %d\n", ptPcmHWParams->eFmtMod);
			return -1;
			break;
		}
	}

	dwSampRate = ptPcmHWParams->dwSampRate;    /* ²ÉÑùÂÊ */

	/* ÀïÃæÊµ¼ÊÉÏµ÷ÓÃ calloc º¯Êý½øÐÐ¿Õ¼ä·ÖÅä
	 * calloc Óë malloc º¯ÊýµÄÇø±ðÔÚÓÚ£¬calloc º¯Êý°ÑÉêÇëµ½µÄ¿Õ¼äÈ«²¿ÇåÁã
	 * calloc(n, size); n ÒªÉêÇëµÄ±äÁ¿¸öÊý£¬ size ±äÁ¿µÄ´óÐ¡
	 */
	iError = snd_pcm_hw_params_malloc(&HardWareParams);
	if (iError < 0){
		DebugPrint("Can't alloc snd_pcm_hw_params_t \n");
		return -1;
	}
	
	/* °ÑÓ²¼þ²ÎÊýÉèÖÃÎªÄ¬ÈÏÖµ */
	iError = snd_pcm_hw_params_any (ptPcmHandle, HardWareParams);
	if (iError < 0) {
		DebugPrint("Initialize HardWareParams error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;
	}

	/* ÉèÖÃÄ£Ê½
	 * ÉèÖÃº¯Êý¶¼ÊÇÍ¨¹ý snd_pcm_hw_param_set À´Íê³É£¬²»Í¬ÉèÖÃÊ¹ÓÃ²»Í¬µÄ²ÎÊýÀ´Ö¸¶¨
	 */
	iError = snd_pcm_hw_params_set_access(ptPcmHandle, HardWareParams, ptPcmHWParams->eAccessMod);
	if (iError < 0) {
		DebugPrint("Set access mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* ÉèÖÃ¸ñÊ½, Ä¬ÈÏÊÇ signed 16 Î»Ð¡¶Ë */
	iError = snd_pcm_hw_params_set_format(ptPcmHandle, HardWareParams, ePcmFmt);
	if (iError < 0) {
		DebugPrint("Set format mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* ÉèÖÃ²ÉÑùÂÊ£¬Ä¬ÈÏÊÇ 44100 */
	iError = snd_pcm_hw_params_set_rate_near(ptPcmHandle, HardWareParams, &dwSampRate, NULL);
	if (iError < 0) {
		DebugPrint("Set rate mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* ÉèÖÃÍ¨µÀÊý£¬ÕâÀïÊÇÁ½¸ö£¬Ë«ÉùµÀ */
	iError = snd_pcm_hw_params_set_channels(ptPcmHandle, HardWareParams, ptPcmHWParams->dwChannels);
	if (iError < 0) {
		DebugPrint("Set channel mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* Ð´ÈëÓ²¼þÉèÖÃ */
	iError = snd_pcm_hw_params(ptPcmHandle, HardWareParams);
	if (iError < 0) {
		DebugPrint("Write hardware parameters error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	snd_pcm_hw_params_free(HardWareParams);

	/* ´Ëº¯Êý»á±»×Ô¶¯µ÷ÓÃ£¬ÔÚ snd_pcm_hw_params Ö®ºó */
	iError = snd_pcm_prepare(ptPcmHandle);
	if (iError < 0) {
		DebugPrint("cannot prepare audio interface for use\n");		
		return -1;		
	}

	return 0;
}

static int MP3MusicDecoderInit(void)
{

	return 0;
}

static void MP3MusicDecodeExit(void)
{	
	
}

/* ÓÉÓÚ mp3 ¸ñÊ½²»ÊÇºÜ¹Ì¶¨£¬ËùÒÔÆ¾½èºó×ºÃûÀ´ÅÐ¶ÏÊÇ²»ÊÇ mp3 ¸ñÊ½ÎÄ¼þ */
static int isSupportMP3(struct FileDesc *ptFileDesc)
{
	char *pcMP3Fmt;
	int iError = 0;

	pcMP3Fmt = strrchr(ptFileDesc->cFileName, '.');

	if(NULL == pcMP3Fmt){
		return 0;
	}

	iError = strcmp(pcMP3Fmt + 1, "mp3");

	return !iError;
}

static int SetVol(char *vol, int roflag)
{
	int err;
	snd_ctl_t *handle = NULL;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&control);

	if (snd_ctl_ascii_elem_id_parse(id, "numid=45")) {
		return -1;
	}
	
	if ((err = snd_ctl_open(&handle, "default", SND_CTL_NONBLOCK)) < 0) {
		DebugPrint("Control open error\n");
		return err;
	}
	snd_ctl_elem_info_set_id(info, id);
	if ((err = snd_ctl_elem_info(handle, info)) < 0) {
		DebugPrint("Cannot find the given element from control \n");
		snd_ctl_close(handle);
		handle = NULL;
		
		return err;
	}

	snd_ctl_elem_value_set_id(control, id);

	if(roflag)
		return (int)snd_ctl_elem_value_get_integer(control, 1);
	
	if ((err = snd_ctl_elem_read(handle, control)) < 0) {
		DebugPrint("Cannot read the given element from control\n");
		snd_ctl_close(handle);
		handle = NULL;

		return err;
	}
	err = snd_ctl_ascii_value_parse(handle, control, info, vol);
	if (err < 0) {
		DebugPrint("Control parse error: %s\n");
		snd_ctl_close(handle);
		handle = NULL;
		
		return err;
	}
	if ((err = snd_ctl_elem_write(handle, control)) < 0) {
		DebugPrint("Control element write error: %s\n");
		snd_ctl_close(handle);
		handle = NULL;
		return err;
	}

	snd_ctl_close(handle);
	handle = NULL;
	
	return 0;
}

static void SetVolThreadOnce(void)
{
	SetVol(DEFAULT_VOL_VAL, 0);
	g_cVolValue = atoi(DEFAULT_VOL_VAL);		
}

static int SetMp3OutPcmFmt(void)
{
    struct PcmHardwareParams tPcmHWParams;
    int iError = 0;

    tPcmHWParams.eAccessMod = SND_PCM_ACCESS_RW_INTERLEAVED;
    tPcmHWParams.dwSampRate = 44100;
    tPcmHWParams.dwChannels = 2;
    tPcmHWParams.eFmtMod    = 16;    /* libmad Ä¬ÈÏÖµ */
  
    /* ´ò¿ªÒ»¸öÒôÆµÉè±¸
     */
    g_ptMP3PlaybackHandle = OpenPcmDev("default", SND_PCM_STREAM_PLAYBACK);
    if(NULL == g_ptMP3PlaybackHandle){
  	  DebugPrint(DEBUG_ERR"Can't open dev\n");
  	  return -1;
    }   
      
    iError = SetHarwareParams(g_ptMP3PlaybackHandle, &tPcmHWParams);
    if(iError){
  	    DebugPrint(DEBUG_ERR"Can't set dev params \n");
		snd_pcm_close(g_ptMP3PlaybackHandle);
        return -1;
    }

	pthread_once(&g_PthreadOnce, SetVolThreadOnce);

	return 0;
}

static int SkipIDxVxContents(struct IDxVxHeader *ptIDxVxHeader)
{
    int iSkipNum;

    if(!strncmp("ID3", (char *)ptIDxVxHeader->aucIDx, 3)){
        iSkipNum = (ptIDxVxHeader->aucIDxSize[0] & 0x7f) * 0x200000
			+ (ptIDxVxHeader->aucIDxSize[1] & 0x7f) * 0x4000
			+ (ptIDxVxHeader->aucIDxSize[2] & 0x7f) * 0x80
			+ (ptIDxVxHeader->aucIDxSize[3] & 0x7f); 

		return iSkipNum + 10;    /* ½á¹¹Ìå±¾Éí 10 ¸ö×Ö½Ú */
    }

	return 0;
}

static int GetPlayTimeForFile(unsigned char *ptDataFrameIndex, int iFileSize)
{
	unsigned char *pucDataFrame = ptDataFrameIndex;
	int iBitRate = 0;
	int iSampleRate = 0;
	int iPlayTime = 0;
	unsigned char ucChannels = 0;
	struct DataFrameHeader *ptDataFrameHeader;

	/* ÓÐÐ©ÎÄ¼þ¿ÉÄÜ¿ªÍ·²»ÊÇÖ¡Êý¾Ý£¬¶øÊÇÌî³äµÄ 0 */
	while(*pucDataFrame != 0xFF){
		if(pucDataFrame >= ptDataFrameIndex + iFileSize){
			/* ÎÄ¼þ¿ÉÄÜÒÑ¾­Ëð»µ */
			return -1;
		}
		pucDataFrame ++;
	}

	ptDataFrameHeader = (struct DataFrameHeader *)pucDataFrame;

	iBitRate   = g_aiMP3BitRateIndex[ptDataFrameHeader->bzBitRateIndex];
	iSampleRate = g_aiMP3SampleRateIndex[ptDataFrameHeader->bzSampleIndex];
	ucChannels  = ptDataFrameHeader->bzCahnnelMod;

	iPlayTime = iFileSize * 8 / iBitRate / 1000;
	
	return iPlayTime;
}

/*
 * Êý¾Ý×ª»»
 */
static inline
signed int scale(mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* clip */
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/*
 * ¸Ã»Øµ÷º¯ÊýÓÃÓÚÊä³ö½âÂëµÄÒôÆµÊý¾Ý. ËüÔÚÃ¿Ò»Ö¡µÄÊý¾Ý½âÂëÍê±ÏÖ®ºó±»»Øµ÷
 * Ä¿µÄÊÇÎªÁËÊä³ö»òÕß²¥·Å PCM ÒôÆµÊý¾Ý
 */

static
enum mad_flow Mp3DataOutput(void *data, struct mad_header const *header,
		     struct mad_pcm *pcm)
{
    unsigned int nchannels, nsamples;
    mad_fixed_t const *left_ch, *right_ch;
    unsigned char aucFrameData[1152*4];
	int iFrameDataNum = 0;
	int iError = 0;
	
    /* pcm->samplerate contains the sampling frequency */
    nchannels = pcm->channels;
    nsamples  = pcm->length;
    left_ch   = pcm->samples[0];
    right_ch  = pcm->samples[1];
	
    while (nsamples--) {
        signed int sample;
     
        /* output sample(s) in 16-bit signed little-endian PCM */
     
        sample = scale(*left_ch++);
        aucFrameData[iFrameDataNum ++] = ((sample >> 0) & 0xff);
        aucFrameData[iFrameDataNum ++] = ((sample >> 8) & 0xff);
   		           
   	    if(nchannels == 2){
            sample = scale(*right_ch++);
            aucFrameData[iFrameDataNum ++] = ((sample >> 0) & 0xff);
            aucFrameData[iFrameDataNum ++] = ((sample >> 8) & 0xff);
        }else if(nchannels == 1){
            /* Èç¹ûÊÇµ¥ÉùµÀ¾ÍÀ©³äÊý¾Ý */
            aucFrameData[iFrameDataNum ++] = ((sample >> 0) & 0xff);
   		    aucFrameData[iFrameDataNum ++] = ((sample >> 8) & 0xff); 
   	    }

    }

    /* ´«ËÍÊý¾Ý,ËÄ¸ö×Ö½ÚÎªµ¥Î» */
	iError = snd_pcm_writei(g_ptMP3PlaybackHandle, aucFrameData, iFrameDataNum / 4);

	if(g_bMp3DecoderExit){
		return MAD_FLOW_STOP;
	}
  
    return MAD_FLOW_CONTINUE;
}

/*
 * »Øµ÷º¯Êý. mad_stream_buffer() »ñµÃÊäÈëÊý¾ÝÁ÷
 * µ±´Ëº¯ÊýµÚ¶þ´Î±»µ÷ÓÃµÄÊ±ºò£¬ÒôÀÖ½áÊø
 */
static
enum mad_flow Mp3GetInput(void *data, struct mad_stream *stream)
{
	struct PcmFmtParams *ptPcmFmtParams = data;

	if (!ptPcmFmtParams->dwDataLength){
		return MAD_FLOW_STOP;		
	}

	/* Òª½âÂëµÄÊý¾ÝÁ÷,ÒòÎªÖ®Ç°ÒÑ¾­Ó³ÉäÊý¾Ýµ½ÄÚ´æÁË£¬ËùÒÔÖ±½ÓÊ¹ÓÃ´Ëº¯Êý¼´¿É£¬¿ÉÄÜÓÐ±ðµÄ
	 *  º¯ÊýÓÃÓÚÃ»ÓÐÌáÇ°Ó³ÉäµÄÊý¾Ý½âÂë
	 */
	mad_stream_buffer(stream, ptPcmFmtParams->pucDataStart, ptPcmFmtParams->dwDataLength);

	ptPcmFmtParams->dwDataLength = 0;

	return MAD_FLOW_CONTINUE;
}

/*
 * ´íÎó´¦Àíº¯Êý MAD_ERROR_* ´íÎóÔÚ mad.h (or stream.h) ÖÐ¶¨Òå
 */
static
enum mad_flow Mp3DecoderError(void *data, struct mad_stream *stream,
		    struct mad_frame *frame)
{
  struct PcmFmtParams *ptPcmFmtParams = data;

  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - ptPcmFmtParams->pucDataStart);

  /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

  return MAD_FLOW_CONTINUE;
}

static void MP3Thread1Clean(void *data)
{
	g_iRuntimeRatio = 0;
	g_bMusicHalt = 0;
	pthread_mutex_unlock(&g_tMutex);
	g_bMP3TimeThreadExit = 1;
}

static void *MP3TimeThread(void *data)
{
	int iTotalPlayTimeUsec = g_iMP3PlayTime * 1000;
	int iHasPlayTimeUsec = 0;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* ÉèÖÃÇåÀíÏß³Ì */
	pthread_cleanup_push(MP3Thread1Clean, NULL);

	g_bMP3TimeThreadExit = 0;

	while(iHasPlayTimeUsec < iTotalPlayTimeUsec){
		pthread_testcancel();    /* Ïß³ÌÈ¡Ïûµã */
		
		/* »ñÈ¡Ëø²¢ÐÝÃß */
		pthread_mutex_lock(&g_tMutex);
		if(g_bMusicHalt){
			pthread_cond_wait(&g_tConVar, &g_tMutex);
		}		
		pthread_mutex_unlock(&g_tMutex);

		g_iRuntimeRatio = (float)iHasPlayTimeUsec / iTotalPlayTimeUsec * 1000;
		usleep(50 * 1000);		
		iHasPlayTimeUsec = iHasPlayTimeUsec + 50;
	}

	pthread_exit(NULL);
	pthread_cleanup_pop(0);
}

static void *MP3PlayThread(void *data)
{
	struct mad_decoder decoder;
	int result;

	g_bMp3DecoderExit = 0;    /*åˆå§‹åŒ–ä¸º0*/
	g_bMP3PlayThreadExit = 0;

	/*é…ç½®è¾“å…¥ã€è¾“å‡ºå’Œé”™è¯¯å›žè°ƒå‡½æ•°*/
	mad_decoder_init(&decoder, data,
		   Mp3GetInput, 0, 0, Mp3DataOutput,
		   Mp3DecoderError, 0);

	/*å¼€å§‹è§£ç */
	result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

	/*è§£ç ç»“æŸ*/
	mad_decoder_finish(&decoder);

	snd_pcm_close(g_ptMP3PlaybackHandle);

	g_bMP3PlayThreadExit = 1;

	pthread_exit(NULL);
}

static int MP3MusicPlay(struct FileDesc *ptFileDesc)
{
	struct IDxVxHeader *ptIDxVxHeader = (struct IDxVxHeader *)ptFileDesc->pucFileMem;
	int iError = 0;
	int iSkipSize = 0;
	unsigned char *pucMemStart = ptFileDesc->pucFileMem;
	int iRealPcmDataSize = 0;

	iError = SetMp3OutPcmFmt();
	if(iError){
		DebugPrint("SetMp3OutPcmFmt error \n");
		return -1;
	}

	/*æœ‰å¾ˆé•¿ä¸€æ®µæ˜¯æ­Œè¯ç­‰ç­‰ä¿¡æ¯ï¼Œè¦è·³è¿‡*/
    ptIDxVxHeader = (struct IDxVxHeader *)pucMemStart;
	iSkipSize = SkipIDxVxContents(ptIDxVxHeader);

    /*ç»“å°¾æœ‰ID3V1,é•¿128å­—èŠ‚*/
    iRealPcmDataSize = ptFileDesc->iFileSize - iSkipSize - 128;
    pucMemStart = pucMemStart + iSkipSize;

	/*åˆå§‹åŒ–ç§æœ‰ä¿¡æ¯ç»“æž„ä½“ï¼ŒåŒ…æ‹¬è¦è§£ç çš„èµ·å§‹åœ°å€ä»¥åŠé•¿åº¦*/
	g_tMP3PcmFmtParams.pucDataStart = pucMemStart;
	g_tMP3PcmFmtParams.dwDataLength = iRealPcmDataSize;
	g_tMP3PcmFmtParams.dwDataLengthBak = iRealPcmDataSize;
	g_tMP3PcmFmtParams.pucDataEnd  = g_tMP3PcmFmtParams.pucDataStart + iRealPcmDataSize;

	g_iMP3PlayTime = GetPlayTimeForFile(g_tMP3PcmFmtParams.pucDataStart, iRealPcmDataSize);

	pthread_create(&g_MP3TimeThreadId, NULL, MP3TimeThread, NULL);	
	pthread_create(&g_MP3PlayThreadId, NULL, MP3PlayThread, &g_tMP3PcmFmtParams);

	return 0;
}

static int MP3MusicCtrl(enum MusicCtrlCode eCtrlCode)
{
	char acVolStr[3];

	switch(eCtrlCode){
		case MUSIC_CTRL_CODE_HALT : {
			snd_pcm_pause(g_ptMP3PlaybackHandle, 1);
			pthread_mutex_lock(&g_tMutex);
			g_bMusicHalt = 1;
			pthread_mutex_unlock(&g_tMutex);	/* Release the lock */

			break;
		}
		case MUSIC_CTRL_CODE_PLAY : {
			snd_pcm_pause(g_ptMP3PlaybackHandle, 0);
			/* Get lock */
			pthread_mutex_lock(&g_tMutex);
			g_bMusicHalt = 0;
			pthread_cond_signal(&g_tConVar);	/* Weak the main thread */
			pthread_mutex_unlock(&g_tMutex);	/* Release the lock */

			break;
		}
		case MUSIC_CTRL_CODE_EXIT : {
			/* ÓÐ¿ÉÄÜÏß³ÌÈ¡ÏûµÄÊ±ºòÒôÀÖÕýÔÚÔÝÍ£
			 * ²»¹ÜÓÐÃ»ÓÐÔÝÍ£¸èÇú£¬¶¼ÏÈÔËÐÐ¸èÇú£¬È»ºóÔÙÈ¡ÏûÏß³Ì
			 * ·ÀÖ¹ÓÉÓÚ×èÈûµ¼ÖÂ³ÌÐò²»ÏìÓ¦
			 */
			if(!g_bMP3PlayThreadExit){
				snd_pcm_pause(g_ptMP3PlaybackHandle, 0);
				g_bMp3DecoderExit = 1;
				pthread_join(g_MP3PlayThreadId, NULL);
				snd_pcm_drop(g_ptMP3PlaybackHandle);
			}

			if(!g_bMP3TimeThreadExit){
				pthread_cancel(g_MP3TimeThreadId);
				pthread_join(g_MP3TimeThreadId, NULL);				
			}
			break;
		}
		case MUSIC_CTRL_CODE_ADD_VOL : {
			g_cVolValue = g_cVolValue > 63 - VOL_CHANGE_NUM ? 63 : g_cVolValue + VOL_CHANGE_NUM;
			sprintf(acVolStr, "%d", g_cVolValue);
			SetVol(acVolStr, 0);
			break;
		}
		case MUSIC_CTRL_CODE_DEC_VOL : {
			g_cVolValue = g_cVolValue < VOL_CHANGE_NUM ? 0 : g_cVolValue - VOL_CHANGE_NUM;
			sprintf(acVolStr, "%d", g_cVolValue);
			SetVol(acVolStr, 0);
			break;
		}
		case MUSIC_CTRL_CODE_GET_VOL : {
			return g_cVolValue;
			break;
		}
		case MUSIC_CTRL_CODE_GET_RUNTIME : {
			return g_iRuntimeRatio;
			break;
		}
		
		default : break;
	}

	return 0;
}

int MP3ParserInit(void)
{
	int iError = 0;

	iError = RegisterMusicParser(&g_tMP3MusicParser);
	iError |= MP3MusicDecoderInit();

	return iError;
}

void MP3ParserExit(void)
{
	UnregisterMusicParser(&g_tMP3MusicParser);
	MP3MusicDecodeExit();
}

