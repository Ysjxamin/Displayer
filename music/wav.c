#include <music_manag.h>
#include <alsa/asoundlib.h>
#include <print_manag.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static int WAVMusicDecoderInit(void);
static void WAVMusicDecodeExit(void);
static int isSupportWAV(struct FileDesc *ptFileDesc);
static int WAVMusicPlay(struct FileDesc *ptFileDesc);
static int WAVMusicCtrl(enum MusicCtrlCode eCtrlCode);

#pragma pack(push)
#pragma pack(1)

struct WAVFmtHeader
{
	char  strRIFF[4];    /* 'RIFF' ��Դ�ļ���־���̶��ַ� */
	unsigned long  dwTotalByte;   /* ����һ����Ա��ʼ���ļ���β�����ֽ��� */
	char  strWAVE[4];    /* 'WAVE' ��ʾ�� WAVE �ļ����̶��ַ� */
	char  strFmt[4];     /* 'fmt ' ���θ�ʽ��־���̶��ַ� */
	unsigned long  dwPcmFmt;      /* ���ݸ�ʽ 16LE,32LE�ȵ� */
	unsigned short bIsCompress;   /* �Ƿ���ѹ���� PCM ���룬���� 1 ��ʾ��ѹ�������� 1 ��ʾ��ѹ�� */
	unsigned short wChannels;     /* �������� */
	unsigned long  dwPcmSampRate; /* ����Ƶ�� */
	unsigned long  dwByteRate;    /* byte �ʣ� byterate = ����Ƶ�� * ��Ƶͨ������ * ����λ�� / 8 */
	unsigned short wFrameSize;    /* �����/֡��С framesize = ͨ���� * ����λ�� / 8 */
	unsigned short wSampBitWidth;  /* ��������λ�� */
	char  strData[4];    /* 'data' ���ݱ�־���̶��ַ� */
	unsigned long  dwPcmDataSize; /* ʵ����Ƶ���ݵĴ�С */
};

#pragma pack(pop)

/* �߳�������õ��Ĳ��� */
struct MusicThreadParams
{
	unsigned char *pucFilePosStart;
	unsigned char *pucFileCurPos;
	int iFilesize;
	int iWAVFrameSize;
	unsigned char *pucPeriodBuf;
};

#define VOL_CHANGE_NUM 2
#define DEFAULT_VOL_VAL "10"
static char g_cVolValue = 10;    /* Ĭ�ϵ�����ֵ */

#define PCM_PERIOD_SIZE 128
static struct MusicThreadParams g_tWAVThreadParams;
static int g_iRuntimeRatio = 0;

static snd_pcm_t *g_ptWAVPlaybackHandle;
static pthread_t g_WAVPlayThreadId;
static pthread_once_t g_PthreadOnce = PTHREAD_ONCE_INIT;
static char g_bMusicPalyFinish = 1;

struct MusicParser g_tWAVMusicParser = {
	.name = "wav",
	.MusicDecoderInit = WAVMusicDecoderInit,
	.MusicDecodeExit  = WAVMusicDecodeExit,
	.isSupport = isSupportWAV,
	.MusicPlay = WAVMusicPlay,
	.MusicCtrl = WAVMusicCtrl,
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
	snd_pcm_format_t ePcmFmt = SND_PCM_FORMAT_S16_LE; /* Ĭ��ֵ */
	unsigned int dwSampRate = 44100; /* Ĭ��ֵ */

	/* ��ʽʶ�� */
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

	dwSampRate = ptPcmHWParams->dwSampRate;    /* ������ */

	/* ����ʵ���ϵ��� calloc �������пռ����
	 * calloc �� malloc �������������ڣ�calloc ���������뵽�Ŀռ�ȫ������
	 * calloc(n, size); n Ҫ����ı��������� size �����Ĵ�С
	 */
	iError = snd_pcm_hw_params_malloc(&HardWareParams);
	if (iError < 0){
		DebugPrint("Can't alloc snd_pcm_hw_params_t \n");
		return -1;
	}
	
	/* ��Ӳ����������ΪĬ��ֵ */
	iError = snd_pcm_hw_params_any (ptPcmHandle, HardWareParams);
	if (iError < 0) {
		DebugPrint("Initialize HardWareParams error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;
	}

	/* ����ģʽ
	 * ���ú�������ͨ�� snd_pcm_hw_param_set ����ɣ���ͬ����ʹ�ò�ͬ�Ĳ�����ָ��
	 */
	iError = snd_pcm_hw_params_set_access(ptPcmHandle, HardWareParams, ptPcmHWParams->eAccessMod);
	if (iError < 0) {
		DebugPrint("Set access mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* ���ø�ʽ, Ĭ���� signed 16 λС�� */
	iError = snd_pcm_hw_params_set_format(ptPcmHandle, HardWareParams, ePcmFmt);
	if (iError < 0) {
		DebugPrint("Set format mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* ���ò����ʣ�Ĭ���� 44100 */
	iError = snd_pcm_hw_params_set_rate_near(ptPcmHandle, HardWareParams, &dwSampRate, NULL);
	if (iError < 0) {
		DebugPrint("Set rate mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* ����ͨ������������������˫���� */
	iError = snd_pcm_hw_params_set_channels(ptPcmHandle, HardWareParams, ptPcmHWParams->dwChannels);
	if (iError < 0) {
		DebugPrint("Set channel mod error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	/* д��Ӳ������ */
	iError = snd_pcm_hw_params(ptPcmHandle, HardWareParams);
	if (iError < 0) {
		DebugPrint("Write hardware parameters error\n");
		free(HardWareParams);
		HardWareParams = NULL;
		return -1;		
	}

	snd_pcm_hw_params_free(HardWareParams);

	/* �˺����ᱻ�Զ����ã��� snd_pcm_hw_params ֮�� */
	iError = snd_pcm_prepare(ptPcmHandle);
	if (iError < 0) {
		DebugPrint("cannot prepare audio interface for use\n");		
		return -1;		
	}

	return 0;
}

/* �߳������� */
void WAVThread1Clean(void *data)
{
	struct MusicThreadParams *ptWAVThreadParams = (struct MusicThreadParams *)data;

	free(ptWAVThreadParams->pucPeriodBuf);
	snd_pcm_close(g_ptWAVPlaybackHandle);    /* ������Ҫ�ر��豸 */
	g_bMusicPalyFinish = 1;
}

void *WAVPlayThread(void *data)
{
	struct MusicThreadParams *ptWAVThreadParams = (struct MusicThreadParams *)data;
	unsigned char *pucWAVFileCurPos;
	unsigned char *pucWAVFilePos;
	unsigned char *pucPeriodBuf;
	int iFileSize;
	int iHasPlaySize = 0;
	int iFrameSize;
	int iChunkNum = 0;

	pucWAVFileCurPos = ptWAVThreadParams->pucFileCurPos;
	pucWAVFilePos    = ptWAVThreadParams->pucFilePosStart;
	pucPeriodBuf     = ptWAVThreadParams->pucPeriodBuf;
	iFileSize        = ptWAVThreadParams->iFilesize;
	iFrameSize       = ptWAVThreadParams->iWAVFrameSize;

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	/* ���������߳� */
	pthread_cleanup_push(WAVThread1Clean, ptWAVThreadParams);

	g_bMusicPalyFinish = 0;

	while(1){
		pthread_testcancel();    /* �߳�ȡ���� */

		if(pucWAVFileCurPos >= pucWAVFilePos + iFileSize){
			pthread_exit(NULL);
		}
		for(iChunkNum = 0; iChunkNum < PCM_PERIOD_SIZE; iChunkNum ++){
			if(pucWAVFileCurPos < pucWAVFilePos + iFileSize){
				memcpy(pucPeriodBuf, pucWAVFileCurPos, iFrameSize);				
				pucWAVFileCurPos += iFrameSize;
				pucPeriodBuf     += iFrameSize;

				/* ����ʱ�����, 1 - 100 */
				iHasPlaySize     += iFrameSize;
				g_iRuntimeRatio = (float)iHasPlaySize / iFileSize * 1000;
			}else{
				break;
			}
		}

		pucPeriodBuf = pucPeriodBuf - iChunkNum * iFrameSize;

		/* Ĭ��һ�� period ������ 128, ��λһ֡ */
		snd_pcm_writei(g_ptWAVPlaybackHandle, pucPeriodBuf, iChunkNum);
	}

	pthread_cleanup_pop(0);
	pthread_exit(NULL);
}

static int WAVMusicDecoderInit(void)
{
	return 0;
}

static void WAVMusicDecodeExit(void)
{	
	
}

static int isSupportWAV(struct FileDesc *ptFileDesc)
{
	struct WAVFmtHeader *ptWAVFmtHeader = (struct WAVFmtHeader *)ptFileDesc->pucFileMem;
	int iError = 0;

	iError = strncmp("RIFF", ptWAVFmtHeader->strRIFF, 4);
	iError |= strncmp("WAVE", ptWAVFmtHeader->strWAVE, 4);

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

static int WAVMusicPlay(struct FileDesc *ptFileDesc)
{
	struct WAVFmtHeader *ptWAVFmtHeader = (struct WAVFmtHeader *)ptFileDesc->pucFileMem;
	int iError = 0;
	struct PcmHardwareParams tPcmHWParams;
	unsigned char *pucPeriodBuf;

	/* ��ʼ����һ����Ƶ�豸
	 */
	g_ptWAVPlaybackHandle = OpenPcmDev("default", SND_PCM_STREAM_PLAYBACK);
	if(NULL == g_ptWAVPlaybackHandle){
		DebugPrint("Can't open pcm device %s\n");
		return -1;
	}

	tPcmHWParams.eAccessMod = SND_PCM_ACCESS_RW_INTERLEAVED;
	tPcmHWParams.eFmtMod    = ptWAVFmtHeader->dwPcmFmt;
	tPcmHWParams.dwChannels = ptWAVFmtHeader->wChannels;
	tPcmHWParams.dwSampRate = ptWAVFmtHeader->dwPcmSampRate;
	
	iError = SetHarwareParams(g_ptWAVPlaybackHandle, &tPcmHWParams);
	if(iError){
		DebugPrint("Can't set dev params \n");
		return -1;
	}

	pucPeriodBuf = malloc(ptWAVFmtHeader->wFrameSize * PCM_PERIOD_SIZE);
	if(NULL == pucPeriodBuf){
		DebugPrint("Malloc pucPeriodBuf error \n");
		return -1;
	}

	/* ˽�нṹ�壬����һЩ�߳���Ҫ�õ������ݣ�һ��Ҫ��ȫ�ֱ���������ú����˳�֮��
	 * ĳЩ�����ͱ��ͷ���,�ٴη��ʻ���ɶδ���
	 */
	g_tWAVThreadParams.pucFilePosStart = ptFileDesc->pucFileMem;
	g_tWAVThreadParams.pucFileCurPos = g_tWAVThreadParams.pucFilePosStart
								+ sizeof(struct WAVFmtHeader);
	g_tWAVThreadParams.iWAVFrameSize = ptWAVFmtHeader->wFrameSize;
	g_tWAVThreadParams.iFilesize     = ptFileDesc->iFileSize;
	g_tWAVThreadParams.pucPeriodBuf  = pucPeriodBuf;

	pthread_once(&g_PthreadOnce, SetVolThreadOnce);
	pthread_create(&g_WAVPlayThreadId, NULL, WAVPlayThread, &g_tWAVThreadParams);

	return 0;
}

static int WAVMusicCtrl(enum MusicCtrlCode eCtrlCode)
{
	char acVolStr[3];

	switch(eCtrlCode){
		case MUSIC_CTRL_CODE_HALT : {
			snd_pcm_pause(g_ptWAVPlaybackHandle, 1);

			break;
		}
		case MUSIC_CTRL_CODE_PLAY : {
			snd_pcm_pause(g_ptWAVPlaybackHandle, 0);

			break;
		}
		case MUSIC_CTRL_CODE_EXIT : {
			/* �п����߳�ȡ����ʱ������������ͣ
			 * ������û����ͣ�������������и�����Ȼ����ȡ���߳�
			 * ��ֹ�����������³�����Ӧ
			 */
			if(!g_bMusicPalyFinish){
				snd_pcm_pause(g_ptWAVPlaybackHandle, 0);  
				pthread_cancel(g_WAVPlayThreadId); /* �߳�ȡ�� */
				pthread_join(g_WAVPlayThreadId, NULL);
				snd_pcm_drop(g_ptWAVPlaybackHandle);
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

int WAVParserInit(void)
{
	int iError = 0;

	iError = RegisterMusicParser(&g_tWAVMusicParser);
	iError |= WAVMusicDecoderInit();

	return iError;
}

void WAVParserExit(void)
{
	UnregisterMusicParser(&g_tWAVMusicParser);
	WAVMusicDecodeExit();
}


