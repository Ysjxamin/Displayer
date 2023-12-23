#include <page_manag.h>
#include <pic_operation.h>
#include <print_manag.h>
#include <dis_manag.h>
#include <file.h>
#include <font_manag.h>
#include <music_manag.h>
#include <render.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define HALT_ICON_NAME "halt.bmp"
#define PLAY_ICON_NAME "play.bmp"

static struct DisLayout g_atMusicPageIconsLayout[] = {
	{0, 0, 0, 0, "return.bmp"},
	{0, 0, 0, 0, "play.bmp"},
	{0, 0, 0, 0, "decvol.bmp"},
	{0, 0, 0, 0, "addvol.bmp"},
	{0, 0, 0, 0, "pre_music.bmp"},
    {0, 0, 0, 0, "next_music.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tMusicPageMenuLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout       = g_atMusicPageIconsLayout,
};

static struct DisLayout g_tMusicDisLayout;

static void MusicRunPage(struct PageIdetify *ptParentPageIdentify);
static int MusicGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

static struct PageOpr g_tMusicPageOpr = {
	.name = "music",
	.RunPage = MusicRunPage,
	.GetInputEvent = MusicGetInputEvent,
//	.Prepare  =    /* ��̨׼����������ʵ�� */
};

static void CalcMusicPageMenusLayout(struct PageLayout *ptPageLayout)
{
	int iXres;
	int iYres;
	int iBpp;

	int iHeight;
	int iWidth;
	int iVerticalDis;
	int iProportion;    /* ͼ�������ͼ����ı��� */
	int iTmpTotalBytes;
	int iIconNum;
	int iIconTotal;     /* ͼ���ܵĸ��� */
	
	struct DisLayout *atDisLayout;

	/* ���ͼ������ */
	atDisLayout = ptPageLayout->atDisLayout;
	GetDisDeviceSize(&iXres, &iYres, &iBpp);
	ptPageLayout->iBpp = iBpp;

	iIconTotal = sizeof(g_atMusicPageIconsLayout) / sizeof(struct DisLayout) - 1;
	iProportion = 2000;	/* ͼ�������ı��� */

	/* ����ߣ�ͼ���࣬�� */
	iHeight = iYres * iProportion / 
		(iProportion * iIconTotal + iIconTotal + 1);
	iVerticalDis = iHeight / iProportion;
	iWidth  = iXres / 8;	/* ��Ϊ LCD ���� 1/4 */
	iIconNum = 0;

	/* ѭ����ͼ��������־������Ϊ NULL */
	while(atDisLayout->pcIconName != NULL){
		
		atDisLayout->iTopLeftX  = 5;
		atDisLayout->iTopLeftY  = iVerticalDis * (iIconNum + 1)
			+ iHeight * iIconNum;
		atDisLayout->iBotRightX = atDisLayout->iTopLeftX + iWidth - 1;
		atDisLayout->iBotRightY = atDisLayout->iTopLeftY + iHeight - 1;
		
		iTmpTotalBytes = (atDisLayout->iBotRightX -  atDisLayout->iTopLeftX + 1) * iBpp / 8
			* (atDisLayout->iBotRightY - atDisLayout->iTopLeftY + 1);

		/* �����Ϊ������ͼ���ʱ��Ϊÿ��ͼ�����ռ��� */
		if(ptPageLayout->iMaxTotalBytes < iTmpTotalBytes){
			ptPageLayout->iMaxTotalBytes = iTmpTotalBytes;
		}

		iIconNum ++;
		atDisLayout ++;
	}
}

/* �ļ���ͼ�� layout Ҫ��������з��� */
static int CalcMusicPageFilesLayout()
{
	int iXres, iYres, iBpp;
	int iTopLeftX, iTopLeftY;
	int iBotRightX, iBotRightY;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);

	/* �����Ų˵�ͼ����ұ�����,һֱ�� LCD �����ұ� */
	/* _____________________________
	 *|       _____________________ |
	 *| menu |                     ||
	 *|      |                     ||
	 *| menu |                     ||
	 *|      |         ����        ||
	 *| menu |                     ||
	 *|      |                     ||
	 *| menu |_____________________||
	 *|_____________________________|
	 */
	iTopLeftX  = g_atMusicPageIconsLayout[0].iBotRightX + 1;
	iBotRightX = iXres - 1;
	iTopLeftY  = 0;
	iBotRightY = iYres - 1;

	g_tMusicDisLayout.iTopLeftX	= iTopLeftX;
	g_tMusicDisLayout.iBotRightX  = iBotRightX;
	g_tMusicDisLayout.iTopLeftY	= iTopLeftY;
	g_tMusicDisLayout.iBotRightY  = iBotRightY;
	g_tMusicDisLayout.pcIconName  = NULL;

	return 0;
}

static void GenerateMusicTextPage(char *strName, struct DisLayout *ptDisLayout, struct VideoMem *ptVideoMem)
{
	struct DisLayout tCleanDisLayout;
	char strTmp[256];
	char *pcTmp;

	/* �ҵ����һ����б�� */
	pcTmp = strrchr(strName, '/');
	strcpy(strTmp, pcTmp + 1);

	/* ���Ҫ���õ����� */
	tCleanDisLayout.iTopLeftX  = ptDisLayout->iTopLeftX;
	tCleanDisLayout.iTopLeftY  = ptDisLayout->iTopLeftY;
	tCleanDisLayout.iBotRightX = ptDisLayout->iBotRightX;
	tCleanDisLayout.iBotRightY = ptDisLayout->iBotRightY * 2 / 3;
	tCleanDisLayout.pcIconName = NULL;

	SetFontSize(30);

	/* ��ʾ���� */
	MergeString(ptDisLayout->iTopLeftX, ptDisLayout->iTopLeftY,
					ptDisLayout->iBotRightX, ptDisLayout->iBotRightY * 2 / 3,
					(unsigned char *)strTmp, ptVideoMem, CONFIG_MUSIC_BG_COLOR);
}

static void GenerateMusicProgressBarBG(struct DisLayout *ptDisLayout, struct VideoMem *ptVideoMem)
{
	struct DisLayout tCleanDisLayout;
	
	/* ���Ҫ���õ����� */
	tCleanDisLayout.iTopLeftX  = ptDisLayout->iTopLeftX;
	tCleanDisLayout.iTopLeftY  = ptDisLayout->iBotRightY * 2 / 3;
	tCleanDisLayout.iBotRightX = ptDisLayout->iBotRightX - 100;
	tCleanDisLayout.iBotRightY = ptDisLayout->iBotRightY;
	tCleanDisLayout.pcIconName = NULL;
	ClearVideoMemRegion(ptVideoMem, &tCleanDisLayout, CONFIG_PROGRESS_BG_COLOR);
}

/* iRatio �������е�ʱ�������0 - 100 */
static void GenerateMusicProgressBar(int iRatio, struct DisLayout *ptDisLayout, struct VideoMem *ptVideoMem)
{
	struct DisLayout tCleanDisLayout;
	
	/* ���Ҫ���õ����� */
	tCleanDisLayout.iTopLeftX  = ptDisLayout->iTopLeftX;
	tCleanDisLayout.iTopLeftY  = ptDisLayout->iBotRightY * 2 / 3;
	tCleanDisLayout.iBotRightX = ptDisLayout->iBotRightX - 100;
	tCleanDisLayout.iBotRightY = ptDisLayout->iBotRightY;
	tCleanDisLayout.pcIconName = NULL;

	tCleanDisLayout.iBotRightX = tCleanDisLayout.iTopLeftX + iRatio * (tCleanDisLayout.iBotRightX - tCleanDisLayout.iTopLeftX + 1) / 1000;

	ClearVideoMemRegion(ptVideoMem, &tCleanDisLayout, CONFIG_PROGRESS_COLOR);
}

/* iVol ������С */
static void GenerateMusicVolText(int iVol, struct DisLayout *ptDisLayout, struct VideoMem *ptVideoMem)
{
	struct DisLayout tCleanDisLayout;
	char strTmp[3];

	/* ���Ҫ���õ����� */
	tCleanDisLayout.iTopLeftX  = ptDisLayout->iBotRightX - 100;
	tCleanDisLayout.iTopLeftY  = ptDisLayout->iBotRightY * 2 / 3;
	tCleanDisLayout.iBotRightX = ptDisLayout->iBotRightX;
	tCleanDisLayout.iBotRightY = ptDisLayout->iBotRightY;
	tCleanDisLayout.pcIconName = NULL;

	SetFontSize(30);

	sprintf(strTmp, "%d", iVol);

	/* ��ʾ���� */
	MergeString(ptDisLayout->iBotRightX - 100, ptDisLayout->iBotRightY * 2 / 3,
					ptDisLayout->iBotRightX, ptDisLayout->iBotRightY,
					(unsigned char *)strTmp, ptVideoMem, CONFIG_MUSIC_BG_COLOR);
}

static void ShowMusicPage(struct PageLayout *ptPageLayout, char *strPath)
{
	struct DisLayout *atDisLayout;
	struct VideoMem *ptVideoMem;

	atDisLayout = ptPageLayout->atDisLayout;

	ptVideoMem = GetVideoMem(GetID("music"), 1);
	if(NULL == ptVideoMem){
		DebugPrint(DEBUG_ERR"malloc music VideoMem error\n");
		return;
	}

	if(atDisLayout[0].iTopLeftX == 0){
		CalcMusicPageMenusLayout(ptPageLayout);
		CalcMusicPageFilesLayout();
	}
	
	GeneratePage(ptPageLayout, ptVideoMem);
	
	GenerateMusicTextPage(strPath, &g_tMusicDisLayout, ptVideoMem);
	GenerateMusicProgressBarBG(&g_tMusicDisLayout, ptVideoMem);
	GenerateMusicVolText(0, &g_tMusicDisLayout, ptVideoMem);

	FlushVideoMemToDev(ptVideoMem);

	ReleaseVideoMem(ptVideoMem);
}

static int SetPlayHaltIcon(struct DisLayout *ptIconDisLayout, struct VideoMem *ptVideoMem, char *pcIconName)
{
	int iError = 0;

	struct PiexlDatasDesc tOriginIconDatas;
	struct PiexlDatasDesc tIcondatas;

	tIcondatas.iBpp  = g_tMusicPageMenuLayout.iBpp;
	
	tIcondatas.pucPiexlDatasMem = malloc(g_tMusicPageMenuLayout.iMaxTotalBytes);
	if(NULL == tIcondatas.pucPiexlDatasMem){
		DebugPrint(DEBUG_ERR"tIcondatas.pucPiexlDatasMem malloc error\n");
		return -1;
	}

	iError = GetPiexlDatasForIcons(pcIconName, &tOriginIconDatas);
	if(iError){
		DebugPrint(DEBUG_ERR"GetPiexlDatasForIcons error\n");
		free(tIcondatas.pucPiexlDatasMem);
		return -1;
	}
	
	tIcondatas.iHeight = ptIconDisLayout->iBotRightY - ptIconDisLayout->iTopLeftY;
	tIcondatas.iWidth  = ptIconDisLayout->iBotRightX - ptIconDisLayout->iTopLeftX;
	tIcondatas.iLineLength	= tIcondatas.iWidth * tIcondatas.iBpp / 8;
	tIcondatas.iTotalLength = tIcondatas.iLineLength * tIcondatas.iHeight;
	
	PicZoomOpr(&tOriginIconDatas, &tIcondatas);
	PicMergeOpr(ptIconDisLayout->iTopLeftX, ptIconDisLayout->iTopLeftY, 
		&tIcondatas, &ptVideoMem->tPiexlDatas);

	free(tIcondatas.pucPiexlDatasMem);

	return 0;
}

void *ProgressBarThread(void *data)
{
	struct MusicParser *ptMusicParser = (struct MusicParser *)data;
	int iRunTimeRatio = 0;
	struct VideoMem *ptVideoMem = GetDevVideoMem();

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

	while(1){
		usleep(100*1000);   /* 100ms ����һ�� */
		iRunTimeRatio = CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_GET_RUNTIME);
		GenerateMusicProgressBar(iRunTimeRatio, &g_tMusicDisLayout, ptVideoMem);	
		pthread_testcancel();    /* �߳�ȡ���� */

	}

	pthread_exit(NULL);
}

static void MusicRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	int iIndexPressed = -1;	/* �ж��Ƿ�����ͬһ��ͼ���ϰ������ɿ� */
	int bPressedFlag = 0;
	struct PageIdetify tPageIdentify;
	struct InputEvent tInputEvent;
	char strFullPathName[256];

	int iError = 0;
	char strCurDirPath[256];
	char strCurFileName[256];
	char *pcTmp;
	struct DirContent *ptDirContents;
	struct MusicParser *ptMusicParser;
	struct FileDesc tMusicFileDesc;
	int iDirContentsNumber;
	int iMusicFileIndex;
	int iCurMusicIndex = 0;
	unsigned char bHaltMusic = 0;

	pthread_t g_ProgressBarThreadId;

	struct VideoMem *ptVideoMem;
	int iXres, iYres, iBpp;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);
	ptVideoMem = GetDevVideoMem();

	tPageIdentify.iPageID = GetID("music");
	snprintf(strFullPathName, 256, "%s", ptParentPageIdentify->strCurPicFile);
	strFullPathName[255] = '\0';

	ShowMusicPage(&g_tMusicPageMenuLayout, strFullPathName);

	strcpy(strCurDirPath, ptParentPageIdentify->strCurPicFile);
	pcTmp = strrchr(strCurDirPath, '/');
	*pcTmp = '\0';
	strcpy(strCurFileName, pcTmp + 1);
	printf("%s",strCurDirPath);
	iError = GetDirContents(strCurDirPath, &ptDirContents, &iDirContentsNumber);

	/* ȷ����ǰ�ļ����ڵ�λ�� */
	for(iMusicFileIndex = 0; iMusicFileIndex < iDirContentsNumber; iMusicFileIndex ++){
		if(0 == strcmp(strCurFileName, ptDirContents[iMusicFileIndex].strDirName)){
			iCurMusicIndex = iMusicFileIndex;
			break;
		}
	}

	ptMusicParser = PlayMusic(&tMusicFileDesc, strFullPathName);
	if(NULL == ptMusicParser){
		DebugPrint("Play %s error\n", strFullPathName);
		StopMusic(&tMusicFileDesc, ptMusicParser);
	}
	
	iError = CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_GET_VOL);
	GenerateMusicVolText(iError, &g_tMusicDisLayout, ptVideoMem);

	pthread_create(&g_ProgressBarThreadId, NULL, ProgressBarThread, ptMusicParser);

	while(1){
		/*该函数会休眠*/
		iIndex = MusicGetInputEvent(&g_tMusicPageMenuLayout, &tInputEvent);

		if(tInputEvent.bPressure == 0){
			/* ˵������û�а��£��������ɿ� */
			if(0 == bPressedFlag){
				goto nextwhilecircle;
			}

			bPressedFlag = 0;
			ReleaseButton(&g_atMusicPageIconsLayout[iIndexPressed]);
			
			switch(iIndexPressed){
				case 0: {
					StopMusic(&tMusicFileDesc, ptMusicParser);
					free(ptDirContents);
					ptDirContents = NULL;
					
					pthread_cancel(g_ProgressBarThreadId); /* �߳�ȡ�� */
					pthread_join(g_ProgressBarThreadId, NULL);
					return; /* �˳��������� */
					break;
				}
				case 1: {  /*暂停播放*/
					if(!bHaltMusic){
						CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_HALT);
						SetPlayHaltIcon(&g_atMusicPageIconsLayout[iIndexPressed], ptVideoMem, HALT_ICON_NAME);
						bHaltMusic = 1;
					}else{
						CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_PLAY);
						SetPlayHaltIcon(&g_atMusicPageIconsLayout[iIndexPressed], ptVideoMem, PLAY_ICON_NAME);
						bHaltMusic = 0;
					}
					
					break;
				}
				case 2: {  /*音量加*/
					CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_ADD_VOL);
					iError = CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_GET_VOL);
					GenerateMusicVolText(iError, &g_tMusicDisLayout, ptVideoMem);
					break;
				}
				case 3: {  /*音量减*/
					CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_DEC_VOL);
					iError = CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_GET_VOL);
					GenerateMusicVolText(iError, &g_tMusicDisLayout, ptVideoMem);
					break;
				}
				case 4: {  /*上一首*/
					iMusicFileIndex = iCurMusicIndex;
					while(iMusicFileIndex > 0){
						iMusicFileIndex --;
						
						if(ptDirContents[iMusicFileIndex].eFileType == NORMAL_FILE){
							snprintf(strFullPathName, 256, "%s/%s", strCurDirPath, ptDirContents[iMusicFileIndex].strDirName);
							strFullPathName[255] = '\0';
							
							if(isMusicSupported(strFullPathName)){
								ShowMusicPage(&g_tMusicPageMenuLayout, strFullPathName);
								iCurMusicIndex = iMusicFileIndex;

								pthread_cancel(g_ProgressBarThreadId); /*线程取消*/
								pthread_join(g_ProgressBarThreadId, NULL);

								bHaltMusic = 0;
								StopMusic(&tMusicFileDesc, ptMusicParser);
								ptMusicParser = PlayMusic(&tMusicFileDesc, strFullPathName);
								if(NULL == ptMusicParser){
									DebugPrint("Play %s error\n", strFullPathName);
									StopMusic(&tMusicFileDesc, ptMusicParser);
									break;
								}
								iError = CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_GET_VOL);
								GenerateMusicVolText(iError, &g_tMusicDisLayout, ptVideoMem);

								/* ���´����߳� */
								pthread_create(&g_ProgressBarThreadId, NULL, ProgressBarThread, ptMusicParser);

								break;
							}
						}
					}
					break;
				}
				case 5: {  /*下一首*/
					iMusicFileIndex = iCurMusicIndex;
					while(iMusicFileIndex < iDirContentsNumber - 1){
						iMusicFileIndex ++;
						
						if(ptDirContents[iMusicFileIndex].eFileType == NORMAL_FILE){
							snprintf(strFullPathName, 256, "%s/%s", strCurDirPath, ptDirContents[iMusicFileIndex].strDirName);
							strFullPathName[255] = '\0';
							
							if(isMusicSupported(strFullPathName)){
								ShowMusicPage(&g_tMusicPageMenuLayout, strFullPathName);
								iCurMusicIndex = iMusicFileIndex;

								pthread_cancel(g_ProgressBarThreadId); /* �߳�ȡ�� */
								pthread_join(g_ProgressBarThreadId, NULL);

								bHaltMusic = 0;
								StopMusic(&tMusicFileDesc, ptMusicParser);
								ptMusicParser = PlayMusic(&tMusicFileDesc, strFullPathName);
								if(NULL == ptMusicParser){
									DebugPrint("Play %s error\n", strFullPathName);
									StopMusic(&tMusicFileDesc, ptMusicParser);
									break;
								}
								iError = CtrlMusic(ptMusicParser, MUSIC_CTRL_CODE_GET_VOL);
								GenerateMusicVolText(iError, &g_tMusicDisLayout, ptVideoMem);

								/* ���´����߳� */
								pthread_create(&g_ProgressBarThreadId, NULL, ProgressBarThread, ptMusicParser);

								break;
							}
						}
					}
					break;
				}
				default: {
					DebugPrint(DEBUG_EMERG"Somthing wrong\n");
					break;
				}
			}
		}else{
			if(iIndex == -1){    /* ˵��û�а��ڲ˵����� */
				goto nextwhilecircle;
			}
			
			if(0 == bPressedFlag){
				bPressedFlag = 1;
				iIndexPressed = iIndex;
				PressButton(&g_atMusicPageIconsLayout[iIndexPressed]);
				goto nextwhilecircle;
			}

		}
nextwhilecircle:
		iError = 0;
	}
}

static int MusicGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int MusicPageInit(void)
{
	return RegisterPageOpr(&g_tMusicPageOpr);
}


