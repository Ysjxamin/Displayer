#include <page_manag.h>
#include <font_manag.h>
#include <pic_operation.h>
#include <render.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

static struct DisLayout g_atIntervalPageIconsLayout[] = {
	{0, 0, 0, 0, "inc.bmp"},
	{0, 0, 0, 0, "time.bmp"},
	{0, 0, 0, 0, "dec.bmp"},
	{0, 0, 0, 0, "ok.bmp"},
	{0, 0, 0, 0, "cancel.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tIntervalPageLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout = g_atIntervalPageIconsLayout,
};

#define FAST_MOD_FLAG    1000
#define FAST_CHANGE_FLAG 100

static struct DisLayout g_tSecNumDisLayout;     /* ��ʾ���� */
static int g_iIntervalSec = 2;                  /* Ĭ��ֵΪ2 */

static void IntervalRunPage(struct PageIdetify *ptParentPageIdentify);
static int IntervalGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

static struct PageOpr g_tIntervalPageOpr = {
	.name = "interval",
	.RunPage = IntervalRunPage,
	.GetInputEvent = IntervalGetInputEvent,
//	.Prepare  =    /* ��̨׼����������ʵ�� */
};

/* �������õ�ʱ�䡣���Ҫ������ҳ��ʹ�� */
void GetIntervalTime(int *piIntervalTime)
{
	*piIntervalTime = g_iIntervalSec;
}

static void CalcIntervalPageLayout(struct PageLayout *ptPageLayout)
{
	int iStartY;
	int iWidth;
	int iHeight;
	int iXres, iYres, iBpp;
	int iTmpTotalBytes;
	struct DisLayout *atDisLayout;

	atDisLayout = ptPageLayout->atDisLayout;
	GetDisDeviceSize(&iXres, &iYres, &iBpp);
	ptPageLayout->iBpp = iBpp;

	/*   
	 *    ----------------------
	 *                          1/2 * iHeight
	 *          inc.bmp         iHeight * 28 / 128     
	 *         time.bmp         iHeight * 72 / 128
	 *          dec.bmp         iHeight * 28 / 128     
	 *                          1/2 * iHeight
	 *    ok.bmp     cancel.bmp 1/2 * iHeight
	 *                          1/2 * iHeight
	 *    ----------------------
	 */
	iHeight = iYres / 3;
	iWidth  = iHeight;
	iStartY = iHeight / 2;

	/* incͼ�� */
	atDisLayout[0].iTopLeftY  = iStartY;
	atDisLayout[0].iBotRightY = atDisLayout[0].iTopLeftY + iHeight * 28 / 128 - 1;
	atDisLayout[0].iTopLeftX  = (iXres - iWidth * 52 / 128) / 2;
	atDisLayout[0].iBotRightX = atDisLayout[0].iTopLeftX + iWidth * 52 / 128 - 1;

	iTmpTotalBytes = (atDisLayout[0].iBotRightX - atDisLayout[0].iTopLeftX + 1) * (atDisLayout[0].iBotRightY - atDisLayout[0].iTopLeftY + 1) * iBpp / 8;
	if (ptPageLayout->iMaxTotalBytes < iTmpTotalBytes)
	{
		ptPageLayout->iMaxTotalBytes = iTmpTotalBytes;
	}

	/* timeͼ�� */
	atDisLayout[1].iTopLeftY  = atDisLayout[0].iBotRightY + 1;
	atDisLayout[1].iBotRightY = atDisLayout[1].iTopLeftY + iHeight * 72 / 128 - 1;
	atDisLayout[1].iTopLeftX  = (iXres - iWidth) / 2;
	atDisLayout[1].iBotRightX = atDisLayout[1].iTopLeftX + iWidth - 1;
	iTmpTotalBytes = (atDisLayout[1].iBotRightX - atDisLayout[1].iTopLeftX + 1) * (atDisLayout[1].iBotRightY - atDisLayout[1].iTopLeftY + 1) * iBpp / 8;
	if (ptPageLayout->iMaxTotalBytes < iTmpTotalBytes)
	{
		ptPageLayout->iMaxTotalBytes = iTmpTotalBytes;
	}

	/* decͼ�� */
	atDisLayout[2].iTopLeftY  = atDisLayout[1].iBotRightY + 1;
	atDisLayout[2].iBotRightY = atDisLayout[2].iTopLeftY + iHeight * 28 / 128 - 1;
	atDisLayout[2].iTopLeftX  = (iXres - iWidth * 52 / 128) / 2;
	atDisLayout[2].iBotRightX = atDisLayout[2].iTopLeftX + iWidth * 52 / 128 - 1;
	iTmpTotalBytes = (atDisLayout[2].iBotRightX - atDisLayout[2].iTopLeftX + 1) * (atDisLayout[2].iBotRightY - atDisLayout[2].iTopLeftY + 1) * iBpp / 8;
	if (ptPageLayout->iMaxTotalBytes < iTmpTotalBytes)
	{
		ptPageLayout->iMaxTotalBytes = iTmpTotalBytes;
	}

	/* okͼ�� */
	atDisLayout[3].iTopLeftY  = atDisLayout[2].iBotRightY + iHeight / 2 + 1;
	atDisLayout[3].iBotRightY = atDisLayout[3].iTopLeftY + iHeight / 2 - 1;
	atDisLayout[3].iTopLeftX  = (iXres - iWidth) / 3;
	atDisLayout[3].iBotRightX = atDisLayout[3].iTopLeftX + iWidth / 2 - 1;
	iTmpTotalBytes = (atDisLayout[3].iBotRightX - atDisLayout[3].iTopLeftX + 1) * (atDisLayout[3].iBotRightY - atDisLayout[3].iTopLeftY + 1) * iBpp / 8;
	if (ptPageLayout->iMaxTotalBytes < iTmpTotalBytes)
	{
		ptPageLayout->iMaxTotalBytes = iTmpTotalBytes;
	}

	/* okͼ�� */
	atDisLayout[4].iTopLeftY  = atDisLayout[3].iTopLeftY;
	atDisLayout[4].iBotRightY = atDisLayout[3].iBotRightY;
	atDisLayout[4].iTopLeftX  = atDisLayout[3].iTopLeftX * 2 + iWidth/2;
	atDisLayout[4].iBotRightX = atDisLayout[4].iTopLeftX + iWidth/2 - 1;
	iTmpTotalBytes = (atDisLayout[4].iBotRightX - atDisLayout[4].iTopLeftX + 1) * (atDisLayout[4].iBotRightY - atDisLayout[4].iTopLeftY + 1) * iBpp / 8;
	if (ptPageLayout->iMaxTotalBytes < iTmpTotalBytes)
	{
		ptPageLayout->iMaxTotalBytes = iTmpTotalBytes;
	}

	/* ������ʾ���ֵ�����Ƚ�����, ��������
	 * time.bmpԭͼ��СΪ128x72, ������������ִ�СΪ52x40
	 * ����CalcIntervalPageLayout����������
	 */
	iWidth  = atDisLayout[1].iBotRightX - atDisLayout[1].iTopLeftX + 1;
	iHeight = atDisLayout[1].iBotRightY - atDisLayout[1].iTopLeftY + 1;

	g_tSecNumDisLayout.iTopLeftX  = atDisLayout[1].iTopLeftX + (128 - 52) / 2 * iWidth / 128;
	g_tSecNumDisLayout.iBotRightX = atDisLayout[1].iBotRightX - (128 - 52) / 2 * iWidth / 128 + 1;

	g_tSecNumDisLayout.iTopLeftY  = atDisLayout[1].iTopLeftY + (72 - 40) / 2 * iHeight / 72;
	g_tSecNumDisLayout.iBotRightY = atDisLayout[1].iBotRightY - (72 - 40) / 2 * iHeight / 72 + 1;
}

static int GenerateIntervalTimeIcon(int iSecTime, struct VideoMem *ptVideoMem)
{
	unsigned int dwFontSize;
	char strTextString[3];

	dwFontSize = g_tSecNumDisLayout.iBotRightY - g_tSecNumDisLayout.iTopLeftY;
	SetFontSize(dwFontSize);

	if(iSecTime > 59){
		return -1;
	}

	snprintf(strTextString, 3, "%02d", iSecTime);

	return MergeString(g_tSecNumDisLayout.iTopLeftX, g_tSecNumDisLayout.iTopLeftY, g_tSecNumDisLayout.iBotRightX, g_tSecNumDisLayout.iBotRightY, (unsigned char*)strTextString, ptVideoMem, CONFIG_BACKGROUND_COLOR);
}

static void ShowIntervalPage(struct PageLayout *ptPageLayout)
{
	int iError = 0;

	struct VideoMem *ptVideoMem;
	struct DisLayout *atDisLayout;

	atDisLayout = ptPageLayout->atDisLayout;

	/* ���һ���ڴ�����ʾ Interval ҳ�� */
	ptVideoMem = GetVideoMem(GetID("interval"), 1);
	if(NULL == ptVideoMem){
		DebugPrint(DEBUG_ERR"Get interval-page video memory error\n");
		return;
	}

	/* ������ͼ�껭��ȥ */
	if(atDisLayout[0].iTopLeftX == 0){
		CalcIntervalPageLayout(ptPageLayout);
	}

	iError = GeneratePage(ptPageLayout, ptVideoMem);
	GenerateIntervalTimeIcon(g_iIntervalSec, ptVideoMem);
	
	FlushVideoMemToDev(ptVideoMem);

	/* �ͷ�������ڴ棬�Թ���ĳ���ʹ�� */
	ReleaseVideoMem(ptVideoMem);
}

static int TimeDisOfTwoEvent(struct InputEvent *ptPreEvent, struct InputEvent *ptCurEvent)
{
	int iSecTime;
	int iUsecTime;

	iSecTime  = ptCurEvent->tTimeVal.tv_sec - ptPreEvent->tTimeVal.tv_sec;
	iUsecTime = ptCurEvent->tTimeVal.tv_usec - ptPreEvent->tTimeVal.tv_usec;

	return (iSecTime * 1000 + iUsecTime / 1000);
}

static void IntervalRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	int iError;
	int iIndexPressed = -1;	/* �ж��Ƿ�����ͬһ��ͼ���ϰ������ɿ� */
	int bFastFlag = 0;
	int bPressedFlag = 0;
	struct InputEvent tInputEvent;
	struct InputEvent tPreInputEvent;
	struct VideoMem *ptVideoMem;
	int iInterValSec;

	iInterValSec = g_iIntervalSec;

	ptVideoMem = GetDevVideoMem();

	ShowIntervalPage(&g_tIntervalPageLayout);

	tPreInputEvent.tTimeVal.tv_sec  = 0;
	tPreInputEvent.tTimeVal.tv_usec = 0;

	while(1){
		/* �ú��������� */
		iIndex = IntervalGetInputEvent(&g_tIntervalPageLayout, &tInputEvent);

		DebugPrint(DEBUG_DEBUG"Interval page index = %d****************\n", iIndex);
		if(tInputEvent.bPressure == 0){
			/* ˵�������а��£��������ɿ� */
			if(bPressedFlag){
				bPressedFlag = 0;
				bFastFlag = 0;
				ReleaseButton(&g_atIntervalPageIconsLayout[iIndexPressed]);
				DebugPrint(DEBUG_DEBUG"Release button****************\n");

				/* ��ͬһ����ť�������ɿ� */
//				if(iIndexPressed == iIndex){
//					goto nextwhilecircle;
//				}
				switch(iIndexPressed){
					case 0: {  /* ʱ������ */
						if(iInterValSec < 59){
							iInterValSec ++;
						}

						GenerateIntervalTimeIcon(iInterValSec, ptVideoMem);
						break;
					}
					case 1: {
						DebugPrint(DEBUG_DEBUG"do 1****************\n");
						break;
					}
					case 2: {  /* ʱ����� */
						if(iInterValSec > 1){
							iInterValSec --;
						}

						GenerateIntervalTimeIcon(iInterValSec, ptVideoMem);
						break; 
					}
					case 3: {  /* ȷ�� */
						g_iIntervalSec = iInterValSec;
						return; /* �˳��������� */
						break;
					}
					case 4: {  /* ȡ���������ϴε����� */
						return; /* �˳��������� */
						break;
					}
					default: {
						DebugPrint(DEBUG_EMERG"Somthing wrong\n");
						break;
					}
				}
			}
		}else{
			if(iIndex != -1){
				if(0 == bPressedFlag){
					bPressedFlag = 1;
					iIndexPressed = iIndex;
					tPreInputEvent = tInputEvent;
					PressButton(&g_atIntervalPageIconsLayout[iIndexPressed]);

					goto nextwhilecircle;
				}

				if(bFastFlag){
					if(TimeDisOfTwoEvent(&tPreInputEvent, &tInputEvent) > FAST_CHANGE_FLAG){
						if(0 == iIndexPressed){
							if(iInterValSec < 59){
								iInterValSec ++;
							}

							GenerateIntervalTimeIcon(iInterValSec, ptVideoMem);
						}else if(2 == iIndexPressed){
							if(iInterValSec > 1){
								iInterValSec --;
							}

							GenerateIntervalTimeIcon(iInterValSec, ptVideoMem);
						}

						tPreInputEvent = tInputEvent;   /* ����ʱ�� */
					}

					goto nextwhilecircle;
				}

				if(0 == bFastFlag){
					if(TimeDisOfTwoEvent(&tPreInputEvent, &tInputEvent) > FAST_MOD_FLAG){
						bFastFlag = 1;
						tPreInputEvent = tInputEvent;
					}			
				}		
			}
		}	
nextwhilecircle:
		iError = 0;
	}
}

static int IntervalGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int IntervalPageInit(void)
{
	return RegisterPageOpr(&g_tIntervalPageOpr);
}




