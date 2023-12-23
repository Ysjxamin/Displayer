#include <page_manag.h>
#include <pic_operation.h>
#include <render.h>

static struct DisLayout g_atTextPageIconsLayout[] = {
	{0, 0, 0, 0, "select_fold.bmp"},
	{0, 0, 0, 0, "interval.bmp"},
	{0, 0, 0, 0, "return.bmp"},
	{0, 0, 0, 0, NULL},
};

static struct PageLayout g_tTextPageLayout = {
	.iMaxTotalBytes = 0,
	.atDisLayout = g_atTextPageIconsLayout,
};

static void TextRunPage(struct PageIdetify *ptParentPageIdentify);
static int TextGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);

static struct PageOpr g_tTextPageOpr = {
	.name = "text",
	.RunPage = TextRunPage,
	.GetInputEvent = TextGetInputEvent,
//	.Prepare  =    /* ��̨׼����������ʵ�� */
};

static void CalcTextPageLayout(struct PageLayout *ptPageLayout)
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

	iIconTotal = sizeof(g_atTextPageIconsLayout) / sizeof(struct DisLayout) - 1;
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

static void ShowTextPage(struct PageLayout *ptPageLayout)
{
	int iError = 0;

	struct VideoMem *ptTextPageVM;
	struct DisLayout *atDisLayout;

	atDisLayout = ptPageLayout->atDisLayout;

	/* ���һ���ڴ�����ʾ Text ҳ�� */
	ptTextPageVM = GetVideoMem(GetID("text"), 1);
	if(NULL == ptTextPageVM){
		DebugPrint(DEBUG_ERR"Get Text-page video memory error\n");
		return;
	}

	/* ������ͼ�껭��ȥ */
	if(atDisLayout[0].iTopLeftX == 0){
		CalcTextPageLayout(ptPageLayout);
	}

	iError = GeneratePage(ptPageLayout, ptTextPageVM);
	
	FlushVideoMemToDev(ptTextPageVM);

	/* �ͷ�������ڴ棬�Թ���ĳ���ʹ�� */
	ReleaseVideoMem(ptTextPageVM);
}

static void TextRunPage(struct PageIdetify *ptParentPageIdentify)
{
	int iIndex;
	int iError = 0;
	int iIndexPressed = -1;	/* �ж��Ƿ�����ͬһ��ͼ���ϰ������ɿ� */
	int bPressedFlag = 0;
	struct InputEvent tInputEvent;
	struct PageIdetify tPageIdentify;

	tPageIdentify.iPageID = GetID("text");
	ShowTextPage(&g_tTextPageLayout);

	while(1){
		/* �ú��������� */
		iIndex = TextGetInputEvent(&g_tTextPageLayout, &tInputEvent);

		DebugPrint(DEBUG_DEBUG"text page index = %d****************\n", iIndex);
		if(tInputEvent.bPressure == 0){
			/* ˵�������а��£��������ɿ� */
			if(bPressedFlag){
				bPressedFlag = 0;
				ReleaseButton(&g_atTextPageIconsLayout[iIndexPressed]);
				DebugPrint(DEBUG_DEBUG"Release button****************\n");

				/* ��ͬһ����ť�������ɿ� */
//				if(iIndexPressed == iIndex){
//					goto nextwhilecircle;
//				}
					switch(iIndexPressed){
						case 0: {   /* ѡ��Ŀ¼ */
							GetPageOpr("browse")->RunPage(&tPageIdentify);
							ShowTextPage(&g_tTextPageLayout);
							break;
						}
						case 1: {   /* ���ü�� */
							GetPageOpr("interval")->RunPage(&tPageIdentify);
							ShowTextPage(&g_tTextPageLayout);
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
					PressButton(&g_atTextPageIconsLayout[iIndexPressed]);
				}			
			}
		}	
nextwhilecircle:
	iError = 0;		
	}
}

static int TextGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent)
{
	return GenericGetInputEvent(ptPageLayout, ptInputEvent);
}

int TextPageInit(void)
{
	return RegisterPageOpr(&g_tTextPageOpr);
}


