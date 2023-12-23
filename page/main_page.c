#include <page_manag.h>
#include <pic_operation.h>
#include <render.h>

static struct DisLayout g_atMainPageIconsLayout[] = {
	{0, 0, 0, 0, "browse_mod.bmp"},
	{0, 0, 0, 0, "continue_mod.bmp"},
	{0, 0, 0, 0, "setting.bmp"},
	{0, 0, 0, 0, "return.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tMainPageLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout = g_atMainPageIconsLayout,
};

static void MainRunPage(struct PageIdetify *ptParentPageIdentify);
static int MainGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

static struct PageOpr g_tMainPageOpr = {
	.name = "main",
	.RunPage = MainRunPage,
	.GetInputEvent = MainGetInputEvent,
//	.Prepare  =    /* ��̨׼����������ʵ�� */
};

static void CalcMainPageLayout(struct PageLayout *ptPageLayout)
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

	iIconTotal = sizeof(g_atMainPageIconsLayout) / sizeof(struct DisLayout) - 1;
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

static void ShowMainPage(struct PageLayout *ptPageLayout)
{
	int iError = 0;

	struct VideoMem *ptMainPageVM;
	struct DisLayout *atDisLayout;

	atDisLayout = ptPageLayout->atDisLayout;

	/* ���һ���ڴ�����ʾ main ҳ�� */
	ptMainPageVM = GetVideoMem(GetID("main"), 1);
	if(NULL == ptMainPageVM){
		DebugPrint(DEBUG_ERR"Get main-page video memory error\n");
		return;
	}

	/* ������ͼ�껭��ȥ */
	if(atDisLayout[0].iTopLeftX == 0){
		CalcMainPageLayout(ptPageLayout);
	}

	iError = GeneratePage(ptPageLayout, ptMainPageVM);
	
	FlushVideoMemToDev(ptMainPageVM);

	/* �ͷ�������ڴ棬�Թ���ĳ���ʹ�� */
	ReleaseVideoMem(ptMainPageVM);
}

static void MainRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	int iError = 0;
	int iIndexPressed = -1;	/* �ж��Ƿ�����ͬһ��ͼ���ϰ������ɿ� */
	int bPressedFlag = 0;
	struct InputEvent tInputEvent;
	struct PageIdetify tMainPageIdentify;

	tMainPageIdentify.iPageID = GetID("main");
	ShowMainPage(&g_tMainPageLayout);

	while(1){
		/* �ú��������� */
		iIndex = MainGetInputEvent(&g_tMainPageLayout, &tInputEvent);

		if(tInputEvent.bPressure == 0){
			/* ˵�������а��£��������ɿ� */
			if(bPressedFlag){
				bPressedFlag = 0;
				ReleaseButton(&g_atMainPageIconsLayout[iIndexPressed]);

				/* ��ͬһ����ť�������ɿ� */
//				if(iIndexPressed != iIndex){
//					goto nextwhilecircle;
//				}
				switch(iIndexPressed){
					case 0: {
						GetPageOpr("browse")->RunPage(&tMainPageIdentify);
						ShowMainPage(&g_tMainPageLayout);
						break;
					}
					case 1: {
						GetPageOpr("auto")->RunPage(&tMainPageIdentify);
						ShowMainPage(&g_tMainPageLayout);
						break;
					}
					case 2: {
						GetPageOpr("setting")->RunPage(&tMainPageIdentify);
						ShowMainPage(&g_tMainPageLayout);
						break;
					}
					case 3: {
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
					PressButton(&g_atMainPageIconsLayout[iIndexPressed]);

				}			
			}
		}
//nextwhilecircle:
	iError = 0;
	}
}

static int MainGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int MainPageInit(void)
{
	return RegisterPageOpr(&g_tMainPageOpr);
}


