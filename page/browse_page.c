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

struct FileAndDirLayout
{
	char *strName;
	struct PiexlDatasDesc *ptPiexlDatas;
};

static struct DisLayout g_atBrowsePageIconsLayout[] = {
	{0, 0, 0, 0, "up.bmp"},
	{0, 0, 0, 0, "select.bmp"},
	{0, 0, 0, 0, "pre_page.bmp"},
	{0, 0, 0, 0, "next_page.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tBrowsePageLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout = g_atBrowsePageIconsLayout,
};

static void BrowseRunPage(struct PageIdetify *ptParentPageIdentify);
static int BrowseGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

#define DEFAULT_DIR "/"        /* Ĭ��Ŀ¼�Ǹ�Ŀ¼ */

/* ͼ����һ�������Σ������ļ�����������Ҳ�������ε� */
#define DIR_FILE_ICON_WIDTH  50
#define DIR_FILE_ICON_HEIGHT DIR_FILE_ICON_WIDTH
#define DIR_FILE_NAME_HEIGHT 20
#define DIR_FILE_NAME_WIDTH  (DIR_FILE_ICON_HEIGHT + DIR_FILE_NAME_HEIGHT)
/* all ָ���ǰ����ļ������ļ�ͼ��������ο� */
#define DIR_FILE_ALL_WIDTH   DIR_FILE_NAME_WIDTH
#define DIR_FILE_ALL_HEIGHT  DIR_FILE_ALL_WIDTH

static char g_strCurDirPath[256] = DEFAULT_DIR;

/* �ļ���Ŀ¼���¼���� */
#define DIRFILE_ICON_BASE 256

/* Ŀ¼���ļ� */
static struct DirContent *g_ptDirContents;
static int g_iDirContentsNumber;
static int g_iFileStartIndex = 0;

/* ͼ�� */
static int g_iDirFileNumPerRow;
static int g_iDirFileNumPerCol;

static struct PiexlDatasDesc g_tDirClosedIconPiexlDatas;
static struct PiexlDatasDesc g_tDirOpenedIconPiexlDatas;
static struct PiexlDatasDesc g_tFileIconPiexlDatas;
static struct PiexlDatasDesc g_tUnknowIconPiexlDatas;
static struct PiexlDatasDesc g_tTxtIconPiexlDatas;
static struct PiexlDatasDesc g_tMusicIconPiexlDatas;

struct FileAndDirLayout g_atBrowsePageFileAndDirIcons[] = {
	{"fold_opened.bmp", &g_tDirOpenedIconPiexlDatas},
	{"fold_closed.bmp", &g_tDirClosedIconPiexlDatas},
	{"file.bmp", &g_tFileIconPiexlDatas},
	{"unknow.bmp", &g_tUnknowIconPiexlDatas},
	{"txt.bmp", &g_tTxtIconPiexlDatas},
	{"music.bmp", &g_tMusicIconPiexlDatas},
	{NULL, NULL},
};

static struct DisLayout  *g_atFileDirIconLayout;
static struct PageLayout g_tFileDirPageLayout = {
	.iMaxTotalBytes = 0,
};

static struct PageOpr g_tBrowsePageOpr = {
	.name = "browse",
	.RunPage = BrowseRunPage,
	.GetInputEvent = BrowseGetInputEvent,
};

static void CalcBrowsePageMenusLayout(struct PageLayout *ptPageLayout)
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

	iIconTotal = sizeof(g_atBrowsePageIconsLayout) / sizeof(struct DisLayout) - 1;
	iProportion = 2000;	/* ͼ�������ı��� */

	/* ����ߣ�ͼ���࣬�� */
	iHeight = iYres * iProportion / 
		(iProportion * iIconTotal + iIconTotal + 1);
	iVerticalDis = iHeight / iProportion;
	iWidth  = iXres / 6;	/* ��Ϊ LCD ���� 1/4 */
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
static int CalcBrowsePageFilesLayout()
{
	int iXres, iYres, iBpp;
	int iTopLeftX, iTopLeftY;
	int iTopLeftXBak;
	int iBotRightX, iBotRightY;
	int iIconWidth, iIconHeight;
	int iNumPerCol, iNumPerRow;
	int iDeltaX, iDeltaY;
	int i, j, k = 0;

	GetDisDeviceSize(&iXres, &iYres, &iBpp);

	/* �����Ų˵�ͼ����ұ�����,һֱ�� LCD �����ұ� */
	/* _____________________________
	 *|       _____________________ |
	 *| menu |   _     _     _     ||
	 *|      |  |_|   |_|   |_|    ||
	 *| menu |                     ||
	 *|      |     �ļ���Ŀ¼      ||
	 *| menu |   _     _     _     ||
	 *|      |  |_|   |_|   |_|    ||
	 *| menu |_____________________||
	 *|_____________________________|
	 */
	iTopLeftX  = g_atBrowsePageIconsLayout[0].iBotRightX + 1;
	iBotRightX = iXres - 1;
	iTopLeftY  = 0;
	iBotRightY = iYres - 1;

	/* ȷ����ʾ���ļ�����������һ�м�����һ�м��� 
	 * ��Ӧ�궨���е� DIR_FILE_ALL
	 */
	iIconWidth  = DIR_FILE_NAME_WIDTH;
	iIconHeight = iIconWidth;

	/* �������ͼ����� */
	iNumPerRow = (iBotRightX - iTopLeftX + 1) / iIconWidth;
	while(1){
		/* ������϶�Ŀ�� */
		iDeltaX = (iBotRightX - iTopLeftX + 1) - iIconWidth * iNumPerRow;
		if(iDeltaX / (iNumPerRow + 1) < 10){
			iNumPerRow --;
		}else{
			break;
		}
	}

	/* ��������ͼ����� */
	iNumPerCol = (iBotRightY - iTopLeftY + 1) / iIconHeight;
	while(1){
		/* ������϶�Ŀ�� */
		iDeltaY = (iBotRightY - iTopLeftY + 1) - iIconHeight * iNumPerCol;
		if(iDeltaY / (iNumPerCol + 1) < 10){
			iNumPerCol --;
		}else{
			break;
		}
	}

	iDeltaX = iDeltaX / (iNumPerRow + 1);
	iDeltaY = iDeltaY / (iNumPerCol + 1);

	g_iDirFileNumPerRow = iNumPerRow;
	g_iDirFileNumPerCol = iNumPerCol;

	/* ���� DisLayout �ṹ�壬����ͼ��������һ�� 
	 * 2 * g_iDirFileNumPerRow * g_iDirFileNumPerCol ��
	 * ���Ͻ�����ʶ��һ��
	 * 2 * g_iDirFileNumPerRow * g_iDirFileNumPerCol + 1 ��
	 */

	g_atFileDirIconLayout = malloc(sizeof(struct DisLayout) * (2 * g_iDirFileNumPerRow * g_iDirFileNumPerCol + 1));
	if(NULL == g_atFileDirIconLayout){
		DebugPrint(DEBUG_ERR"malloc g_atFileDirIconLayout error\n");
		return -1;
	}

	g_tFileDirPageLayout.iTopLeftX  = iTopLeftX;
	g_tFileDirPageLayout.iTopLeftY  = iTopLeftY;
	g_tFileDirPageLayout.iBotRightX = iBotRightX;
	g_tFileDirPageLayout.iBotRightY = iBotRightY;
	g_tFileDirPageLayout.iBpp       = iBpp;
	g_tFileDirPageLayout.iMaxTotalBytes = DIR_FILE_ALL_HEIGHT * DIR_FILE_ALL_WIDTH * iBpp / 8;
	g_tFileDirPageLayout.atDisLayout    = g_atFileDirIconLayout;

	/* ȷ��ÿһ��ͼ���λ�� */
	iTopLeftX = iTopLeftX + iDeltaX;
	iTopLeftY = iTopLeftY + iDeltaY;
	iTopLeftXBak = iTopLeftX;

	k = 0;
	for(i = 0; i < g_iDirFileNumPerCol; i ++){
		for(j = 0;j < g_iDirFileNumPerRow; j ++){
			/* �ļ����Ǳ�ͼ���� */
			g_atFileDirIconLayout[k].iTopLeftX  = iTopLeftX + (DIR_FILE_NAME_WIDTH - DIR_FILE_ICON_WIDTH) / 2;
			g_atFileDirIconLayout[k].iBotRightX = g_atFileDirIconLayout[k].iTopLeftX + DIR_FILE_ICON_WIDTH - 1;
			g_atFileDirIconLayout[k].iTopLeftY  = iTopLeftY;
			g_atFileDirIconLayout[k].iBotRightY = g_atFileDirIconLayout[k].iTopLeftY + DIR_FILE_ICON_HEIGHT - 1;

			/* �ļ��� */
			g_atFileDirIconLayout[k+1].iTopLeftX  = iTopLeftX;
			g_atFileDirIconLayout[k+1].iBotRightX = g_atFileDirIconLayout[k+1].iTopLeftX + DIR_FILE_NAME_WIDTH - 1;
			g_atFileDirIconLayout[k+1].iTopLeftY  = g_atFileDirIconLayout[k].iBotRightY + 1;
			g_atFileDirIconLayout[k+1].iBotRightY = g_atFileDirIconLayout[k+1].iTopLeftY + DIR_FILE_NAME_HEIGHT - 1;

			iTopLeftX = iTopLeftX + DIR_FILE_ALL_WIDTH + iDeltaX;
			k += 2;
		}

		iTopLeftX = iTopLeftXBak;
		iTopLeftY = iTopLeftY + DIR_FILE_ALL_HEIGHT + iDeltaY;
	}

	/* ͼ�������־ */
	g_atFileDirIconLayout[k].iTopLeftX  = 0;
	g_atFileDirIconLayout[k].iBotRightX = 0;
	g_atFileDirIconLayout[k].iTopLeftY  = 0;
	g_atFileDirIconLayout[k].iBotRightY = 0;
	g_atFileDirIconLayout[k].pcIconName = NULL;

	return 0;
}

static int GenerateDirAndFileIcons(struct PageLayout *ptPageLayout)
{
	struct PiexlDatasDesc tOriginIconPiexlDatas;
	int iError;
	int iXres;
	int iYres;
	int iBpp;
	struct DisLayout *atLayout;
	int iIconNum;
	int iIconNumIndex;

	/* ����ȷ��ͼ��Ĵ�С */
	atLayout = ptPageLayout->atDisLayout;

	iError = GetDisDeviceSize(&iXres, &iYres, &iBpp);
	if(iError){
		DebugPrint("GetDisDeviceSize error \n");
		return -1;
	}

	iIconNum = sizeof(g_atBrowsePageFileAndDirIcons) / sizeof(struct FileAndDirLayout) - 1;

	for(iIconNumIndex = 0; iIconNumIndex < iIconNum; iIconNumIndex ++){
		/* Ϊ��/�ر�Ŀ¼�Լ���ͨ�ļ�����ռ� */
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iBpp = iBpp;
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->pucPiexlDatasMem = malloc(ptPageLayout->iMaxTotalBytes);
			
		if(NULL == g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->pucPiexlDatasMem){
			DebugPrint(DEBUG_ERR"malloc %s's memory error\n", g_atBrowsePageFileAndDirIcons[iIconNumIndex].strName);
			return -1;
		}

		
		/* ������֮����ȡ���� */
		iError = GetPiexlDatasForIcons(g_atBrowsePageFileAndDirIcons[iIconNumIndex].strName, &tOriginIconPiexlDatas);
		if(iError){
			DebugPrint(DEBUG_ERR"get %s error\n", g_atBrowsePageFileAndDirIcons[iIconNumIndex].strName);
			return -1;
		}
		
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iHeight = atLayout[0].iBotRightY - atLayout[0].iTopLeftY + 1;
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iWidth  = atLayout[0].iBotRightX - atLayout[0].iTopLeftX + 1;
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iLineLength = g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iWidth * iBpp / 8;
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iTotalLength = g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iLineLength * g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->iLineLength;
		
		PicZoomOpr(&tOriginIconPiexlDatas, g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas);
		FreePiexlDatasForIcon(&tOriginIconPiexlDatas);
	}

	return 0;
}

void FreeDirAndFileIcons(void)
{
	int iIconNum;
	int iIconNumIndex;

	iIconNum = sizeof(g_atBrowsePageFileAndDirIcons) / sizeof(struct FileAndDirLayout) - 1;

	for(iIconNumIndex = 0; iIconNumIndex < iIconNum; iIconNumIndex ++){
		free(g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->pucPiexlDatasMem);
		g_atBrowsePageFileAndDirIcons[iIconNumIndex].ptPiexlDatas->pucPiexlDatasMem = NULL;
	}

	/* ͼ��� dislayout��������������ֻ����һ���ڴ� */
	free(g_atFileDirIconLayout);
}

/* iNum ���ܵ�ͼ������� */
static int GenerateBrowsePageDirAndFile(int iStart, int iNum, struct DirContent *atDirContent, struct VideoMem *ptVideoMem)
{
	struct PageLayout *ptPageLayout;
	struct DisLayout *atDisLayout;
	struct DisLayout tCleanDisLayout;
	int i, j, k;
	int iIndex;
	char strTmp[256];

	ptPageLayout = &g_tFileDirPageLayout;
	atDisLayout = ptPageLayout->atDisLayout;

	/* ���Ҫ���õ����� */
	tCleanDisLayout.iTopLeftX  = g_tFileDirPageLayout.iTopLeftX;
	tCleanDisLayout.iTopLeftY  = g_tFileDirPageLayout.iTopLeftY;
	tCleanDisLayout.iBotRightX = g_tFileDirPageLayout.iBotRightX;
	tCleanDisLayout.iBotRightY = g_tFileDirPageLayout.iBotRightY;
	tCleanDisLayout.pcIconName = NULL;
	ClearVideoMemRegion(ptVideoMem, &tCleanDisLayout, CONFIG_BACKGROUND_COLOR);

	SetFontSize(atDisLayout[1].iBotRightY- atDisLayout[1].iTopLeftY - 5);

	k = 0;
	iIndex = iStart;
	for(i = 0; i < g_iDirFileNumPerCol; i ++){
		for(j = 0; j < g_iDirFileNumPerRow; j ++){
			if(iIndex < iNum){
				if(atDirContent[iIndex].eFileType == DIR_FILE){
					PicMergeOpr(atDisLayout[k].iTopLeftX, atDisLayout[k].iTopLeftY, &g_tDirClosedIconPiexlDatas, &ptVideoMem->tPiexlDatas);
				}else{
					snprintf(strTmp, 256, "%s/%s", g_strCurDirPath, atDirContent[iIndex].strDirName);
					strTmp[255] = '\0';
					if(isPictureSupported(strTmp)){
						PicMergeOpr(atDisLayout[k].iTopLeftX, atDisLayout[k].iTopLeftY, &g_tFileIconPiexlDatas, &ptVideoMem->tPiexlDatas);					
					}else if(isMusicSupported(strTmp)){
						PicMergeOpr(atDisLayout[k].iTopLeftX, atDisLayout[k].iTopLeftY, &g_tMusicIconPiexlDatas, &ptVideoMem->tPiexlDatas);
					}else{
						PicMergeOpr(atDisLayout[k].iTopLeftX, atDisLayout[k].iTopLeftY, &g_tUnknowIconPiexlDatas, &ptVideoMem->tPiexlDatas);					
					}
				}

				k ++;
				
				/* ��ʾ���� */
				MergeString(atDisLayout[k].iTopLeftX, atDisLayout[k].iTopLeftY,
								atDisLayout[k].iBotRightX, atDisLayout[k].iBotRightY,
								(unsigned char *)atDirContent[iIndex].strDirName, ptVideoMem, CONFIG_BACKGROUND_COLOR);

				k ++;
				iIndex ++;
			}else{
				return 0;
			}
		}
	}

	return 0;
}

static void ShowBrowsePage(struct PageLayout *ptPageLayout)
{
	struct DisLayout *atDisLayout;
	struct VideoMem *ptVideoMem;

	atDisLayout = ptPageLayout->atDisLayout;

	ptVideoMem = GetVideoMem(GetID("browse"), 1);
	if(NULL == ptVideoMem){
		DebugPrint(DEBUG_ERR"malloc browse VideoMem error\n");
		return;
	}

	if(atDisLayout[0].iTopLeftX == 0){
		CalcBrowsePageMenusLayout(ptPageLayout);
		CalcBrowsePageFilesLayout();
	}
	
	/* ��ȡ���������ļ�ͼ��ĵ�����Ϣ����ŵ�ȫ�ֱ������� */
	if(!g_atBrowsePageFileAndDirIcons[0].ptPiexlDatas->pucPiexlDatasMem){
		GenerateDirAndFileIcons(&g_tFileDirPageLayout);
	}

	GeneratePage(ptPageLayout, ptVideoMem);
	GenerateBrowsePageDirAndFile(g_iFileStartIndex, g_iDirContentsNumber, g_ptDirContents, ptVideoMem);

	FlushVideoMemToDev(ptVideoMem);

	ReleaseVideoMem(ptVideoMem);
}

static int GetInputPosition(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	int iFileNum = 0;
	struct DisLayout *atLayout = ptPageLayout->atDisLayout;

	while(iFileNum < (g_iDirContentsNumber * 2)){
		if(ptInputEvent->iXPos <= atLayout[iFileNum].iBotRightX
			&& ptInputEvent->iXPos >= atLayout[iFileNum].iTopLeftX
			&& ptInputEvent->iYPos <= atLayout[iFileNum].iBotRightY
			&& ptInputEvent->iYPos >= atLayout[iFileNum].iTopLeftY)
		{
			return iFileNum;
		}

		iFileNum ++;  /* ��������Ҳ����Ϊ�ǵ����˸��ļ� */
	}

	return -1;
}
#if 0
static void SelectDirForAutoPage(struct DisLayout *ptDisLayout, struct VideoMem *ptVideoMem)
{
	PicMergeOpr(ptDisLayout->iTopLeftX, ptDisLayout->iTopLeftY, &g_tDirOpenedIconPiexlDatas, &ptVideoMem->tPiexlDatas);
}

static void DeselectDirForAutoPage(struct DisLayout *ptDisLayout, struct VideoMem *ptVideoMem)
{
	PicMergeOpr(ptDisLayout->iTopLeftX, ptDisLayout->iTopLeftY, &g_tDirClosedIconPiexlDatas, &ptVideoMem->tPiexlDatas);
}
#endif
static void BrowseRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	struct InputEvent tInputEvent;
	struct InputEvent tPrePressInputEvent;

//	int bUsedToSelectDir = 0;    /* �Ƿ�������ѡ������ͼƬ�� */
//	int bIconPressed = 0;        /* ��ǰҳ���Ƿ���ͼ�갴�� */

//	int bHaveClickSelectIcon = 0;
	
	int iIndexPressed = -1;
	int iDirFileContentIndex;

	int iError;
	struct VideoMem *ptDevVideoMem;

	struct PageIdetify tBrowsePageIdentify;
	char strPathTmp[256];  /* ·�����ݴ��� */
	char *pcPathTmp;

	int bPressedFlag = 0;

	/* ��ʼ��Ϊ0 */
	tPrePressInputEvent.tTimeVal.tv_sec  = 0;
	tPrePressInputEvent.tTimeVal.tv_usec = 0;

	/* ˵����Ϊ����������Ŀ¼ */
//	if(ptParentPageIdentify->iPageID == GetID("setting")){
//		bUsedToSelectDir = 1;
//	}

	ptDevVideoMem = GetDevVideoMem(); /* ���Ĭ����ʾ�豸���Դ� */
	printf("%s",g_strCurDirPath);
	iError = GetDirContents(g_strCurDirPath, &g_ptDirContents, &g_iDirContentsNumber);
	if(iError){
		DebugPrint(DEBUG_ERR"Can't get directory error\n");
		return;
	}

	tBrowsePageIdentify.iPageID = GetID("browse");
	ShowBrowsePage(&g_tBrowsePageLayout);

	while(1){
		/* �ú��������� */
		iIndex = BrowseGetInputEvent(&g_tBrowsePageLayout, &tInputEvent);

		if(-1 == iIndex){
			/* ˵�������ڲ˵�ͼ�����棬Ҫ�ж��ǲ������ļ������ļ������� */
			iIndex = GetInputPosition(&g_tFileDirPageLayout, &tInputEvent);
			if(-1 != iIndex){
				if(g_iFileStartIndex + iIndex / 2 < g_iDirContentsNumber){
					iIndex += DIRFILE_ICON_BASE;
				}else{
					iIndex = -1;
				}
			}
		}

		if(tInputEvent.bPressure == 0){		
			if(0 == bPressedFlag){
				goto nextwhilecircle;
			}

			/* ˵�������а��£��������ɿ� */
			if(iIndexPressed < DIRFILE_ICON_BASE){
				bPressedFlag = 0;
				ReleaseButton(&g_atBrowsePageIconsLayout[iIndexPressed]);

				/* �ڲ�ͬһ����ť�������ɿ� */
//				if(iIndexPressed != iIndex){
//					goto nextwhilecircle;
//				}
				
				/* ��ͬһ����ť�������ɿ� */
				switch(iIndexPressed){
					/* ���� */
					case 0: {
						if(0 == strcmp(g_strCurDirPath, "/")){
							FreeDirContents(&g_ptDirContents);
							return; /* �˳�����ҳ�� */
						}

						/* �ҵ����һ��"/"ɾ�� */
						pcPathTmp = strrchr(g_strCurDirPath, '/');
						*pcPathTmp = '\0';
						FreeDirContents(&g_ptDirContents);

						/* ���»�ȡĿ¼��������� */
						iError = GetDirContents(g_strCurDirPath, &g_ptDirContents, &g_iDirContentsNumber);
						if(iError){
							DebugPrint(DEBUG_ERR"Get DirContents error\n");
							return;
						}
						g_iFileStartIndex = 0;
						GenerateBrowsePageDirAndFile(g_iFileStartIndex, g_iDirContentsNumber, g_ptDirContents, ptDevVideoMem);
						break;
					}
					/* ѡ�� */
					case 1: {
						ReleaseButton(&g_atBrowsePageIconsLayout[iIndexPressed]);
						break;
					}
					/* ��һҳ */
					case 2: {
						if(0 == g_iFileStartIndex){
							break;
						}else{
							g_iFileStartIndex -= g_iDirFileNumPerCol * g_iDirFileNumPerRow;
							GenerateBrowsePageDirAndFile(g_iFileStartIndex, g_iDirContentsNumber, g_ptDirContents, ptDevVideoMem);
							break;
						}
						break;
					}
					/* ��һҳ */
					case 3: {
						g_iFileStartIndex += g_iDirFileNumPerCol * g_iDirFileNumPerRow;
						
						if(g_iFileStartIndex > g_iDirContentsNumber){
							g_iFileStartIndex -= g_iDirFileNumPerCol * g_iDirFileNumPerRow;
							break;
						}else{	
							GenerateBrowsePageDirAndFile(g_iFileStartIndex, g_iDirContentsNumber, g_ptDirContents, ptDevVideoMem);
							break;
						}
						break;
					}
					default: {
						DebugPrint(DEBUG_EMERG"Somthing wrong\n");
						break;
					}
				}
			}else{				
				
				bPressedFlag = 0;
				ReleaseButton(&g_atFileDirIconLayout[iIndexPressed - DIRFILE_ICON_BASE]);

				/* �ڲ�ͬһ����ť�������ɿ� */
//				if(iIndexPressed != iIndex){
//					goto nextwhilecircle;
//				}

				/* �����Ŀ¼�ͽ��� */
				iDirFileContentIndex = g_iFileStartIndex + (iIndexPressed - DIRFILE_ICON_BASE) / 2;
				if(g_ptDirContents[iDirFileContentIndex].eFileType == DIR_FILE){
					snprintf(strPathTmp, 256, "%s/%s", g_strCurDirPath, g_ptDirContents[iDirFileContentIndex].strDirName);
					strPathTmp[255] = '\0';
					strcpy(g_strCurDirPath, strPathTmp);  /* ���ĵ�ǰĿ¼ */
					FreeDirContents(&g_ptDirContents);
					/* ���»�ȡĿ¼��������� */
					iError = GetDirContents(g_strCurDirPath, &g_ptDirContents, &g_iDirContentsNumber);
					if(iError){
						DebugPrint(DEBUG_ERR"Get DirContents error\n");
						return;
					}
					g_iFileStartIndex = 0;
					GenerateBrowsePageDirAndFile(g_iFileStartIndex, g_iDirContentsNumber, g_ptDirContents, ptDevVideoMem);
				}else if(g_ptDirContents[iDirFileContentIndex].eFileType == NORMAL_FILE){
					snprintf(tBrowsePageIdentify.strCurPicFile, 256, "%s/%s", g_strCurDirPath, g_ptDirContents[iDirFileContentIndex].strDirName);
					tBrowsePageIdentify.strCurPicFile[255] = '\0';
					if(isPictureSupported(tBrowsePageIdentify.strCurPicFile)){
						tBrowsePageIdentify.iPageID = GetID("browse");
						GetPageOpr("picture")->RunPage(&tBrowsePageIdentify);
						ShowBrowsePage(&g_tBrowsePageLayout);
					}
					if(isMusicSupported(tBrowsePageIdentify.strCurPicFile)){
						tBrowsePageIdentify.iPageID = GetID("browse");
						GetPageOpr("music")->RunPage(&tBrowsePageIdentify);
						ShowBrowsePage(&g_tBrowsePageLayout);
					}
				}
			}	
		}else{  /* ��ť���� */
			if(iIndex == -1){  /* û�������κ�һ��ͼ������ */
				goto nextwhilecircle;
			}
			
			if(0 == bPressedFlag){
				bPressedFlag = 1;
				iIndexPressed = iIndex;

				if(iIndexPressed == 0){
					tPrePressInputEvent = tInputEvent;   /* ��¼��һ�ΰ��µ��¼� */
				}

				if(iIndexPressed < DIRFILE_ICON_BASE){					
					PressButton(&g_atBrowsePageIconsLayout[iIndexPressed]);
				}else{
					PressButton(&g_atFileDirIconLayout[iIndexPressed - DIRFILE_ICON_BASE]);
				}

				goto nextwhilecircle;
			}else{
				if(iIndex != 0){  /* ˵�������������·��ؼ� */
					tPrePressInputEvent = tInputEvent;
					goto nextwhilecircle;
				}
				/* ��������Ҫֱ�ӷ��ص���ҳ�� */
				if(tInputEvent.tTimeVal.tv_sec - tPrePressInputEvent.tTimeVal.tv_sec >= 2){
					/* �´ν����ʱ���ǴӸ�Ŀ¼��ʼ */
					g_strCurDirPath[0] = '/';
					g_strCurDirPath[1] = '\0';
					FreeDirContents(&g_ptDirContents);
					return; /* �˳�����ҳ�� */
				}
			}			
		}	

nextwhilecircle:
		iError = 0;
	}
}

static int BrowseGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int BrowsePageInit(void)
{
	return RegisterPageOpr(&g_tBrowsePageOpr);
}

