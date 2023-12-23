#include <page_manag.h>
#include <pic_operation.h>
#include <render.h>

static struct DisLayout g_atSettingPageIconsLayout[] = {
	{0, 0, 0, 0, "select_fold.bmp"},
	{0, 0, 0, 0, "interval.bmp"},
	{0, 0, 0, 0, "return.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tSettingPageLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout = g_atSettingPageIconsLayout,
};

static void SettingRunPage(struct PageIdetify *ptParentPageIdentify);
static int SettingGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

static struct PageOpr g_tSettingPageOpr = {
	.name = "setting",
	.RunPage = SettingRunPage,
	.GetInputEvent = SettingGetInputEvent,
//	.Prepare  =    /* ��̨׼����������ʵ�� */
};

static void CalcSettingPageLayout(struct PageLayout *ptPageLayout)
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

	iIconTotal = sizeof(g_atSettingPageIconsLayout) / sizeof(struct DisLayout) - 1;
	iProportion = 4;	/* ͼ�������ı��� */

	/* ����ߣ�ͼ���࣬�� */
	iHeight = iYres * iProportion / 
		(iProportion * iIconTotal + iIconTotal + 1);
	iVerticalDis = iHeight / iProportion;
	iWidth  = iHeight * 2;	/* �̶���߱�Ϊ 2:1 */
	iIconNum = 0;

	

	/* ѭ����ͼ��������־������Ϊ NULL */
	while(atDisLayout->pcIconName != NULL){
		
		atDisLayout->iTopLeftX  = (iXres - iWidth) / 2;
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

static void ShowSettingPage(struct PageLayout *ptPageLayout)
{
	int iError = 0;

	struct VideoMem *ptSettingPageVM;
	struct DisLayout *atDisLayout;

	atDisLayout = ptPageLayout->atDisLayout;

	/* ���һ���ڴ�����ʾ Setting ҳ�� */
	ptSettingPageVM = GetVideoMem(GetID("setting"), 1);
	if(NULL == ptSettingPageVM){
		DebugPrint(DEBUG_ERR"Get setting-page video memory error\n");
		return;
	}

	/* ������ͼ�껭��ȥ */
	if(atDisLayout[0].iTopLeftX == 0){
		CalcSettingPageLayout(ptPageLayout);
	}

	iError = GeneratePage(ptPageLayout, ptSettingPageVM);
	
	FlushVideoMemToDev(ptSettingPageVM);

	/* �ͷ�������ڴ棬�Թ���ĳ���ʹ�� */
	ReleaseVideoMem(ptSettingPageVM);
}

static void SettingRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	int iError = 0;
	int iIndexPressed = -1;	/* �ж��Ƿ�����ͬһ��ͼ���ϰ������ɿ� */
	int bPressedFlag = 0;
	struct InputEvent tInputEvent;
	struct PageIdetify tPageIdentify;

	tPageIdentify.iPageID = GetID("setting");
	ShowSettingPage(&g_tSettingPageLayout);

	while(1){
		/* �ú��������� */
		iIndex = SettingGetInputEvent(&g_tSettingPageLayout, &tInputEvent);

		DebugPrint(DEBUG_DEBUG"Setting page index = %d****************\n", iIndex);
		if(tInputEvent.bPressure == 0){
			/* ˵�������а��£��������ɿ� */
			if(bPressedFlag){
				bPressedFlag = 0;
				ReleaseButton(&g_atSettingPageIconsLayout[iIndexPressed]);
				DebugPrint(DEBUG_DEBUG"Release button****************\n");

				/* ��ͬһ����ť�������ɿ� */
//				if(iIndexPressed == iIndex){
//					goto nextwhilecircle;
//				}
					switch(iIndexPressed){
						case 0: {   /* ѡ��Ŀ¼ */
							GetPageOpr("browse")->RunPage(&tPageIdentify);
							ShowSettingPage(&g_tSettingPageLayout);
							break;
						}
						case 1: {   /* ���ü�� */
							GetPageOpr("interval")->RunPage(&tPageIdentify);
							ShowSettingPage(&g_tSettingPageLayout);
							break;
						}
						case 2: {
							return; /* �˳��������� */
						}
						default: {
							DebugPrint(DEBUG_EMERG"Somthing wrong\n");
							break;
						}
					}
				iIndexPressed = -1;
			}
		}else{
			if(iIndex != -1){
				if(0 == bPressedFlag){
					bPressedFlag = 1;
					iIndexPressed = iIndex;
					PressButton(&g_atSettingPageIconsLayout[iIndexPressed]);
				}			
			}
		}	
//nextwhilecircle:
	iError = 0;		
	}
}

static int SettingGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int SettingPageInit(void)
{
	return RegisterPageOpr(&g_tSettingPageOpr);
}


