/* LCD ��ʾ���ƹ���ͷ�ļ�
 * ������ʾģ��Ľṹ���Լ���Ӧ�Ĳ�������
 *
 */

#ifndef __DIS_MANAG_H__
#define __DIS_MANAG_H__

#include <common_st.h>
#include <common_config.h>
#include <picfmt_manag.h>

struct DisLayout
{
	int iTopLeftX;	/* ���Ͻ� */
	int iTopLeftY;
	int iBotRightX;	/* ���½� */
	int iBotRightY;
	char *pcIconName;	/* ͼ������� */
};

enum VideoMemStat
{
	VMS_FREE = 0,
	VMS_BUSY_FOR_PREPARE,	/* ��׼�����߳�ʹ�� */
	VMS_BUSY_FOR_MAIN,	/* ����ǰ�����߳�ʹ�� */
};

enum PicStat
{
	PS_BLANK = 0,
	PS_GENERATING,	/* �������� */
	PS_GENERATED,	/* �Ѿ����� */
};

struct VideoMem
{
	int iID;
	int bIsDevFrameBuffer;               /* ��ʶ�Ƿ����豸���Դ棬�����������ڴ�ռ����� */
	enum VideoMemStat eVMStat;
	enum PicStat ePStat;
	struct PiexlDatasDesc tPiexlDatas; /* ͼ������ */
	struct VideoMem *ptVideoMemNext;     /* ���� */
};

struct DisOpr{
	char *name;
	int iXres;
	int iYres;
	int iBpp;
	unsigned char *pucDisMem;    /* ��ʾ�豸���Դ��ַ */
	int (*DisDeviceInit)(void);
	int (*ShowPiexl)(int iPenX, int iPeny, unsigned int dwColor);
	int (*CleanScreen)(unsigned int dwBackColor);
	void (*DrawPage)(struct VideoMem *ptPageVideoMem);
	struct list_head tDisOpr;
};

int RegisterDispOpr(struct DisOpr *pt_DisOpr);
void ShowDisOpr(void);
struct DisOpr *GetDisOpr(char *pcName);
int DisplayInit(void);
int FBInit(void);
int GetDisDeviceSize(int *piXres, int *piYres, int *iBpp);
struct VideoMem *GetDevVideoMem();
int AllocVideoMem(int iMemNum);
void FreeAllVideoMem(void);
struct VideoMem *GetVideoMem(int iMemID, int bCur);
void ReleaseVideoMem(struct VideoMem *ptVideoMem);
int SelectAndInitDefaultDisOpr(char *pcName);
struct DisOpr *GetDefaultDisOpr(void);
void ClearVideoMemRegion(struct VideoMem *ptVideoMem, struct DisLayout *ptDisLayout, unsigned int dwColor);

#endif

