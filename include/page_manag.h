
#ifndef __PAGE_MANAG_H__
#define __PAGE_MANAG_H__

#include <common_config.h>
#include <common_st.h>
#include <input_manag.h>
#include <dis_manag.h>

/* ȷ�����һ������ʱ�������ɿ���������� */
//#define CONFIRM_CLICK_DISTANCE 200*200

struct PageIdetify
{
	int iPageID;
	char strCurPicFile[256];	/* Ҫ����ĵ�һ��ͼƬ�ļ� */
};

struct PageConfig
{
	int iIntervalSec;
	char strDirName[256];   /* �����ĸ�Ŀ¼�µ�ͼƬ�ļ� */
};

struct PageLayout
{
	int iTopLeftX;	/* ���Ͻ� */
	int iTopLeftY;
	int iBotRightX;	/* ���½� */
	int iBotRightY;
	int iBpp;
	int iMaxTotalBytes;
	struct DisLayout *atDisLayout;	/* layout ���� */
};

struct PageOpr
{
	char *name;
	void (*RunPage)(struct PageIdetify *ptParentPageIdentify);
	int (*GetInputEvent)(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);
	int (*Prepare)(void);	/* ��̨׼����������ʵ�� */
	struct list_head tPageOpr;
};

int RegisterPageOpr(struct PageOpr *ptPageOpr);
struct PageOpr *GetPageOpr(char *pcName);
void ShowPageOpr(void);
int PagesInit(void);
int MainPageInit(void);
int SettingPageInit(void);
int BrowsePageInit(void);
int PicturePageInit(void);
int MusicPageInit(void);
int IntervalPageInit(void);
int AutoPageInit(void);
int GetID(char *pcName);
int GenericGetInputEvent(struct PageLayout *ptPageLayout, struct InputEvent *ptInputEvent);
void GetSelectedAutoPageDir(char *strDirName);
void GetIntervalTime(int *piIntervalTime);
void GetPageConfig(struct PageConfig *ptPageConfig);
int DisOfTwoPoint(struct InputEvent *ptPreEvent, struct InputEvent *ptCurEvent);
void FreeDirAndFileIcons(void);

#endif

