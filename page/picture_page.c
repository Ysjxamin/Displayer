#include <page_manag.h>
#include <pic_operation.h>
#include <print_manag.h>
#include <dis_manag.h>
#include <file.h>
#include <font_manag.h>
#include <render.h>
#include <stdlib.h>
#include <string.h>
#include <render.h>

static struct DisLayout g_atPicturePageIconsLayout[] = {
	{0, 0, 0, 0, "return.bmp"},
	{0, 0, 0, 0, "zoomout.bmp"},
	{0, 0, 0, 0, "zoomin.bmp"},
	{0, 0, 0, 0, "pre_pic.bmp"},
    {0, 0, 0, 0, "next_pic.bmp"},
    {0, 0, 0, 0, "continue_mod_small.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tPicturePageMenuLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout       = g_atPicturePageIconsLayout,
};

static struct DisLayout g_tPictureDisLayout;

/* ͼƬ */
#define ZOOM_RATIO 0.9
#define SLIP_MIN_DISTANCE 4*4

#define DEFAULT_AUTOPLAY_DIR "/etc/digitalpic/icons"

static char g_strSelDirPath[256] = DEFAULT_AUTOPLAY_DIR;

static struct PiexlDatasDesc g_tOriginPicDatas;
static struct PiexlDatasDesc g_tZoomPicDatas;

static int g_iPicCenterXPos;
static int g_iPicCenterYPos;

static void PictureRunPage(struct PageIdetify *ptParentPageIdentify);
static int PictureGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

static struct PageOpr g_tPicturePageOpr = {
	.name = "picture",
	.RunPage = PictureRunPage,
	.GetInputEvent = PictureGetInputEvent,
//	.Prepare  =    /* ��̨׼����������ʵ�� */
};

void GetSelectedAutoPageDir(char *strDirName)
{
	strncpy(strDirName, g_strSelDirPath, 256);
	strDirName[255] = '\0';
}

static void CalcPicturePageMenusLayout(struct PageLayout *ptPageLayout)
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

	iIconTotal = sizeof(g_atPicturePageIconsLayout) / sizeof(struct DisLayout) - 1;
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
static int CalcPicturePageFilesLayout()
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
	 *|      |         ͼƬ        ||
	 *| menu |                     ||
	 *|      |                     ||
	 *| menu |_____________________||
	 *|_____________________________|
	 */
	iTopLeftX  = g_atPicturePageIconsLayout[0].iBotRightX + 1;
	iBotRightX = iXres - 1;
	iTopLeftY  = 0;
	iBotRightY = iYres - 1;

	g_tPictureDisLayout.iTopLeftX	= iTopLeftX;
	g_tPictureDisLayout.iBotRightX  = iBotRightX;
	g_tPictureDisLayout.iTopLeftY	= iTopLeftY;
	g_tPictureDisLayout.iBotRightY  = iBotRightY;
	g_tPictureDisLayout.pcIconName  = NULL;

	return 0;
}

static struct PiexlDatasDesc *GetPicPiexlDataFromFile(char *pcName)
{
	int iError = 0;

	if(g_tOriginPicDatas.pucPiexlDatasMem){
		free(g_tOriginPicDatas.pucPiexlDatasMem);
		g_tOriginPicDatas.pucPiexlDatasMem = NULL;
	}

	iError = GetPiexlDatasForPic(pcName, &g_tOriginPicDatas);
	if(iError){
		return NULL;
	}

	return &g_tOriginPicDatas;
}

static struct PiexlDatasDesc *GetZoomPicPiexlDatas(struct PiexlDatasDesc *ptOriginPiexlDatas, int iWidth, int iHeight)
{
	int iXres, iYres, iBpp;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);

	if(g_tZoomPicDatas.pucPiexlDatasMem){
		free(g_tZoomPicDatas.pucPiexlDatasMem);
		g_tZoomPicDatas.pucPiexlDatasMem = NULL;
	}

	g_tZoomPicDatas.iWidth  = iWidth;
	g_tZoomPicDatas.iHeight = iHeight;
	g_tZoomPicDatas.iLineLength  = iWidth * iBpp / 8;
	g_tZoomPicDatas.iTotalLength = iHeight * g_tZoomPicDatas.iLineLength;
	g_tZoomPicDatas.iBpp = iBpp;
	g_tZoomPicDatas.pucPiexlDatasMem = malloc(g_tZoomPicDatas.iTotalLength);
	if(NULL == g_tZoomPicDatas.pucPiexlDatasMem){
		return NULL;
	}

	PicZoomOpr(ptOriginPiexlDatas, &g_tZoomPicDatas);

	return &g_tZoomPicDatas;
}

static int GeneratePicture(char *strPath, struct VideoMem *ptVideoMem)
{
	int iXres, iYres, iBpp;
	int iPicWidth, iPicHeight;
	int iCenterTopLeftX, iCenterTopLeftY;
	float iRatio;
	struct PiexlDatasDesc *ptPicDatas;
	struct PiexlDatasDesc *ptZoomPicDatas;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);

	ptPicDatas = GetPicPiexlDataFromFile(strPath);
	if(NULL == ptPicDatas){
		return -1;
	}
	
	iPicWidth  = g_tPictureDisLayout.iBotRightX - g_tPictureDisLayout.iTopLeftX;
	iPicHeight = g_tPictureDisLayout.iBotRightY - g_tPictureDisLayout.iTopLeftY;

	ptPicDatas->iBpp = iBpp;

	/* 
	 * �����߱ȣ������������һ����ͼƬ��ȸߴ�һ����֮�෴
	 * ���ֱַ�Ƚ���Ļ�Ŀ���ͼƬ�Ŀ�����Ļ�ĸ���ͼƬ�ĸ�
	 */
	if(ptPicDatas->iWidth > ptPicDatas->iHeight){
		if(iPicWidth < ptPicDatas->iWidth){
			iRatio = (float)ptPicDatas->iWidth / ptPicDatas->iHeight;
			ptPicDatas->iWidth  = iPicWidth;
			ptPicDatas->iHeight = iPicWidth / iRatio;			
		}
	}else if(ptPicDatas->iWidth <= ptPicDatas->iHeight){
		if(iPicHeight < ptPicDatas->iHeight){
			iRatio = (float)ptPicDatas->iHeight / ptPicDatas->iWidth;
			ptPicDatas->iHeight = iPicHeight;
			ptPicDatas->iWidth  = iPicHeight / iRatio;
		}
	}
	
#if 1
	ptZoomPicDatas = GetZoomPicPiexlDatas(&g_tOriginPicDatas, ptPicDatas->iWidth, ptPicDatas->iHeight);
	if(NULL == ptZoomPicDatas){
		return -1;
	}
#else
	ptZoomPicDatas = malloc(sizeof(struct PiexlDatasDesc));
	ptZoomPicDatas->iBpp = iBpp;
	ptZoomPicDatas->iHeight = iZoomHeight;
	ptZoomPicDatas->iWidth   = iZoomWidth;
	ptZoomPicDatas->iLineLength  = ptZoomPicDatas->iWidth * iBpp;
	ptZoomPicDatas->iTotalLength = ptZoomPicDatas->iHeight * ptZoomPicDatas->iLineLength;
	ptZoomPicDatas->pucPiexlDatasMem = malloc(ptZoomPicDatas->iTotalLength);

	PicZoomOpr(ptPicDatas, ptZoomPicDatas);
#endif
	iCenterTopLeftX = g_tPictureDisLayout.iTopLeftX + (iPicWidth - ptZoomPicDatas->iWidth) / 2;
	iCenterTopLeftY = g_tPictureDisLayout.iTopLeftY + (iPicHeight - ptZoomPicDatas->iHeight) / 2;

	/* �����ĵ���ͼƬ�����е� */
	g_iPicCenterXPos = g_tPictureDisLayout.iTopLeftX
		+ (g_tPictureDisLayout.iBotRightX - g_tPictureDisLayout.iTopLeftX) / 2;
	g_iPicCenterYPos = g_tPictureDisLayout.iTopLeftY
		+ (g_tPictureDisLayout.iBotRightY - g_tPictureDisLayout.iTopLeftY) / 2;

	ClearVideoMemRegion(ptVideoMem, &g_tPictureDisLayout, CONFIG_BACKGROUND_COLOR);
	PicMergeOpr(iCenterTopLeftX, iCenterTopLeftY, ptZoomPicDatas, &ptVideoMem->tPiexlDatas);
#if 0
	free(ptZoomPicDatas->pucPiexlDatasMem);
	free(ptZoomPicDatas);	
#endif
	return 0;
}

static void ShowZoomPic(struct PiexlDatasDesc *ptZoomPicDatas, struct VideoMem *ptDevVideoMem)
{
	int iTopLeftX, iTopLeftY;
	int iXres, iYres, iBpp;
	float iRatio;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);

	/* 
	 * �����߱ȣ������������һ����ͼƬ��ȸߴ�һ����֮�෴
	 * ���ֱַ�Ƚ���Ļ�Ŀ���ͼƬ�Ŀ�����Ļ�ĸ���ͼƬ�ĸ�
	 */
	if(ptZoomPicDatas->iWidth > ptZoomPicDatas->iHeight){
		if(g_iPicCenterXPos + ptZoomPicDatas->iWidth / 2 > iXres){
			iRatio = (float)ptZoomPicDatas->iWidth / ptZoomPicDatas->iHeight;
			ptZoomPicDatas->iWidth  = g_tPictureDisLayout.iBotRightX - g_tPictureDisLayout.iTopLeftX;
			ptZoomPicDatas->iHeight = ptZoomPicDatas->iWidth / iRatio;
		}
	}else if(ptZoomPicDatas->iWidth <= ptZoomPicDatas->iHeight){
		if(g_iPicCenterYPos + ptZoomPicDatas->iHeight / 2 > iYres){
			iRatio = (float)ptZoomPicDatas->iHeight/ ptZoomPicDatas->iWidth;
			ptZoomPicDatas->iHeight = g_tPictureDisLayout.iBotRightY - g_tPictureDisLayout.iTopLeftY;
			ptZoomPicDatas->iWidth  = ptZoomPicDatas->iHeight / iRatio;
		}
	}
	
	iTopLeftX = g_iPicCenterXPos - ptZoomPicDatas->iWidth / 2;
	iTopLeftY = g_iPicCenterYPos - ptZoomPicDatas->iHeight / 2;

	ptZoomPicDatas->iLineLength  = ptZoomPicDatas->iWidth * ptZoomPicDatas->iBpp / 8;
	ptZoomPicDatas->iTotalLength = ptZoomPicDatas->iLineLength * ptZoomPicDatas->iHeight;

	if(g_tZoomPicDatas.pucPiexlDatasMem){
		free(g_tZoomPicDatas.pucPiexlDatasMem);
		g_tZoomPicDatas.pucPiexlDatasMem = NULL;
	}
	
	g_tZoomPicDatas.pucPiexlDatasMem = malloc(g_tZoomPicDatas.iTotalLength);
	if(NULL == g_tZoomPicDatas.pucPiexlDatasMem){
		return;
	}

	PicZoomOpr(&g_tOriginPicDatas, ptZoomPicDatas);
	ClearVideoMemRegion(ptDevVideoMem, &g_tPictureDisLayout, CONFIG_BACKGROUND_COLOR);
	PicMergeOpr(iTopLeftX, iTopLeftY, ptZoomPicDatas, &ptDevVideoMem->tPiexlDatas);
}

static void ShowSlidePic(struct PiexlDatasDesc *ptZoomPicDatas, struct VideoMem *ptDevVideoMem)
{
	int iTopLeftX, iTopLeftY;

	/* ���ȼ������Ͻǵ�����ֵ */
	iTopLeftX = g_iPicCenterXPos - g_tZoomPicDatas.iWidth / 2;
	iTopLeftY = g_iPicCenterYPos - g_tZoomPicDatas.iHeight / 2;

	/* �綨�߽�ֵ����������߽�Ҫ�������� */
	if(iTopLeftX < g_tPictureDisLayout.iTopLeftX){
		iTopLeftX = g_tPictureDisLayout.iTopLeftX;
		g_iPicCenterXPos = iTopLeftX + g_tZoomPicDatas.iWidth / 2;
	}
	
	if(iTopLeftY < g_tPictureDisLayout.iTopLeftY){
		iTopLeftY = g_tPictureDisLayout.iTopLeftY;
		g_iPicCenterYPos = iTopLeftY + g_tZoomPicDatas.iHeight / 2;
	}

	if(iTopLeftX > g_tPictureDisLayout.iBotRightX){
		iTopLeftX = g_tPictureDisLayout.iBotRightX;
		g_iPicCenterXPos = iTopLeftX + g_tZoomPicDatas.iWidth / 2;
	}

	if(iTopLeftY> g_tPictureDisLayout.iBotRightY){
		iTopLeftY = g_tPictureDisLayout.iBotRightY;
		g_iPicCenterYPos = iTopLeftY + g_tZoomPicDatas.iHeight / 2;
	}
	
	ClearVideoMemRegion(ptDevVideoMem, &g_tPictureDisLayout, CONFIG_BACKGROUND_COLOR);
	PicMergeOpr(iTopLeftX, iTopLeftY, &g_tZoomPicDatas, &ptDevVideoMem->tPiexlDatas);
}

static void ShowPicturePage(struct PageLayout *ptPageLayout, char *strPath)
{
	struct DisLayout *atDisLayout;
	struct VideoMem *ptVideoMem;

	atDisLayout = ptPageLayout->atDisLayout;

	ptVideoMem = GetVideoMem(GetID("picture"), 1);
	if(NULL == ptVideoMem){
		DebugPrint(DEBUG_ERR"malloc picture VideoMem error\n");
		return;
	}

	if(atDisLayout[0].iTopLeftX == 0){
		CalcPicturePageMenusLayout(ptPageLayout);
		CalcPicturePageFilesLayout();
	}
	
	GeneratePage(ptPageLayout, ptVideoMem);
	GeneratePicture(strPath, ptVideoMem);

	FlushVideoMemToDev(ptVideoMem);

	ReleaseVideoMem(ptVideoMem);
}

static void PictureRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	int iIndexPressed = -1;	/* �ж��Ƿ�����ͬһ��ͼ���ϰ������ɿ� */
	int bPressedFlag = 0;
	int bSlipPic = 0;
	struct InputEvent tInputEvent;
	struct InputEvent tSlipInputEvent;
	struct PageIdetify tPageIdentify;
	char strFullPathName[256];

	int iError = 0;
	char strCurDirPath[256];
	char strCurFileName[256];
	char *pcTmp;
	struct DirContent *ptDirContents;
	int iDirContentsNumber;
	int iPicFileIndex;
	int iCurPicIndex = 0;

	struct VideoMem *ptVideoMem;
	int iXres, iYres, iBpp;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);
	ptVideoMem = GetDevVideoMem();

	tPageIdentify.iPageID = GetID("picture");
	snprintf(strFullPathName, 256, "%s", ptParentPageIdentify->strCurPicFile);
	strFullPathName[255] = '\0';

	ShowPicturePage(&g_tPicturePageMenuLayout, strFullPathName);

	strcpy(strCurDirPath, ptParentPageIdentify->strCurPicFile);
	pcTmp = strrchr(strCurDirPath, '/');
	*pcTmp = '\0';
	strcpy(strCurFileName, pcTmp + 1);
	iError = GetDirContents(strCurDirPath, &ptDirContents, &iDirContentsNumber);

	/* ȷ����ǰ�ļ����ڵ�λ�� */
	for(iPicFileIndex = 0; iPicFileIndex < iDirContentsNumber; iPicFileIndex ++){
		if(0 == strcmp(strCurFileName, ptDirContents[iPicFileIndex].strDirName)){
			iCurPicIndex = iPicFileIndex;
			break;
		}
	}

	tSlipInputEvent.iXPos = 0;
	tSlipInputEvent.iYPos = 0;
	
	while(1){
		/* �ú��������� */
		iIndex = PictureGetInputEvent(&g_tPicturePageMenuLayout, &tInputEvent);

		if(tInputEvent.bPressure == 0){
			bSlipPic = 0;

			/* ˵������û�а��£��������ɿ� */
			if(0 == bPressedFlag){
				goto nextwhilecircle;
			}

			bPressedFlag = 0;
			ReleaseButton(&g_atPicturePageIconsLayout[iIndexPressed]);

			/* �ڲ�ͬһ����ť�������ɿ� */
//			if(iIndexPressed != iIndex){
//				goto nextwhilecircle;
//			}
			
			switch(iIndexPressed){
				case 0: {
					free(g_tZoomPicDatas.pucPiexlDatasMem);
					g_tZoomPicDatas.pucPiexlDatasMem = NULL;
					free(g_tOriginPicDatas.pucPiexlDatasMem);
					g_tOriginPicDatas.pucPiexlDatasMem = NULL;
					free(ptDirContents);
					ptDirContents = NULL;
					return; /* �˳��������� */
					break;
				}
				case 1: {  /* ��С */
					g_tZoomPicDatas.iWidth  = (float)g_tZoomPicDatas.iWidth * ZOOM_RATIO;
					g_tZoomPicDatas.iHeight = (float)g_tZoomPicDatas.iHeight * ZOOM_RATIO;

					ShowZoomPic(&g_tZoomPicDatas, ptVideoMem);
					break;
				}
				case 2: {  /* �Ŵ� */
					g_tZoomPicDatas.iWidth	= (float)g_tZoomPicDatas.iWidth / ZOOM_RATIO;
					g_tZoomPicDatas.iHeight = (float)g_tZoomPicDatas.iHeight / ZOOM_RATIO;

					ShowZoomPic(&g_tZoomPicDatas, ptVideoMem);
					break;
				}
				case 3: {  /* ��һ�� */
					iPicFileIndex = iCurPicIndex;
					while(iPicFileIndex > 0){
						iPicFileIndex --;
						
						if(ptDirContents[iPicFileIndex].eFileType == NORMAL_FILE){
							snprintf(strFullPathName, 256, "%s/%s", strCurDirPath, ptDirContents[iPicFileIndex].strDirName);
							strFullPathName[255] = '\0';
							
							if(isPictureSupported(strFullPathName)){
								ShowPicturePage(&g_tPicturePageMenuLayout, strFullPathName);
								iCurPicIndex = iPicFileIndex;
								break;
							}
						}
					}
					break;
				}
				case 4: {  /* ��һ�� */
					iPicFileIndex = iCurPicIndex;
					while(iPicFileIndex < iDirContentsNumber - 1){
						iPicFileIndex ++;
						
						if(ptDirContents[iPicFileIndex].eFileType == NORMAL_FILE){
							snprintf(strFullPathName, 256, "%s/%s", strCurDirPath, ptDirContents[iPicFileIndex].strDirName);
							strFullPathName[255] = '\0';
							
							if(isPictureSupported(strFullPathName)){
								ShowPicturePage(&g_tPicturePageMenuLayout, strFullPathName);
								iCurPicIndex = iPicFileIndex;
								break;
							}
						}
					}
					break;
				}
				case 5: {    /* ���� */
					strcpy(g_strSelDirPath, strCurDirPath);
					g_strSelDirPath[255] = '\0';

					GetPageOpr("auto")->RunPage(&tPageIdentify);
					ShowPicturePage(&g_tPicturePageMenuLayout, strFullPathName);
					break;
				}
				default: {
					DebugPrint(DEBUG_EMERG"Somthing wrong\n");
					break;
				}
			}
		}else{
			if(iIndex == -1){    /* ˵��û�а��ڲ˵����� */
				
				if(0 == bSlipPic){
					bSlipPic = 1;
					tSlipInputEvent = tInputEvent;
					goto nextwhilecircle;
				}
				
				if(DisOfTwoPoint(&tSlipInputEvent, &tInputEvent) >= SLIP_MIN_DISTANCE){
					g_iPicCenterXPos += tInputEvent.iXPos - tSlipInputEvent.iXPos;
					g_iPicCenterYPos += tInputEvent.iYPos - tSlipInputEvent.iYPos;
					ShowSlidePic(&g_tZoomPicDatas, ptVideoMem);

					tSlipInputEvent = tInputEvent;  /* ���±궨λ����ʱ�� */
				}
				goto nextwhilecircle;
			}
			
			if(0 == bPressedFlag){
				bPressedFlag = 1;
				iIndexPressed = iIndex;
				PressButton(&g_atPicturePageIconsLayout[iIndexPressed]);
				goto nextwhilecircle;
			}

		}
nextwhilecircle:
		iError = 0;
	}
}

static int PictureGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int PicturePageInit(void)
{
	return RegisterPageOpr(&g_tPicturePageOpr);
}


