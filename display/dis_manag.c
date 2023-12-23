#include <dis_manag.h>
#include <print_manag.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECLARE_HEAD(g_tDisOprHead);
static struct DisOpr *g_ptDefaultDisOpr;
static struct VideoMem *g_ptVideoMemHead;

int GetDisDeviceSize(int *piXres, int *piYres, int *iBpp)
{
	if(g_ptDefaultDisOpr){ 
		*piXres = 	g_ptDefaultDisOpr->iXres;
		*piYres = 	g_ptDefaultDisOpr->iYres;
		*iBpp   = 	g_ptDefaultDisOpr->iBpp;
		
		return 0;
	}
	
	return -1;
}

/* �������Ŀռ������������˳���ʱ��Ҫȫ���ͷ� */
int AllocVideoMem(int iMemNum)
{
	int iError;

	int iXres;
	int iYres;
	int iBpp;
	int iVideoMemSize;
	int iLineBytes;

	struct VideoMem *ptNewVideoMem;

	iError = GetDisDeviceSize(&iXres, &iYres, &iBpp);
	if(iError){
		DebugPrint(DEBUG_ERR"GetDisDeviceSize error\n");
		return -1;
	}

	iVideoMemSize = iXres * iYres * iBpp / 8;
	iLineBytes    = iXres * iBpp / 8;

	ptNewVideoMem = malloc(sizeof(struct VideoMem));
	if(NULL == ptNewVideoMem){
		DebugPrint(DEBUG_ERR"malloc VideoMem error\n");
		return -1;
	}

	/* �������������� */
	ptNewVideoMem->tPiexlDatas.pucPiexlDatasMem = g_ptDefaultDisOpr->pucDisMem;
	ptNewVideoMem->iID = 0;
	ptNewVideoMem->bIsDevFrameBuffer = 1;    /* ��־��ʼ�������豸���Դ� */
	ptNewVideoMem->ePStat  = PS_BLANK;
	ptNewVideoMem->eVMStat = VMS_FREE;
	ptNewVideoMem->tPiexlDatas.iWidth  = iXres;
	ptNewVideoMem->tPiexlDatas.iHeight = iYres;
	ptNewVideoMem->tPiexlDatas.iBpp    = iBpp;
	ptNewVideoMem->tPiexlDatas.iLineLength  = iLineBytes;
	ptNewVideoMem->tPiexlDatas.iTotalLength = iVideoMemSize;

	if(iMemNum != 0){
		/* ����ж�����棬��ʶ�豸������Դ�Ϊ VMS_BUSY_FOR_MAIN��
		 * �����Ͳ��ᱻ�����ȥ
		 */
		ptNewVideoMem->eVMStat = VMS_BUSY_FOR_MAIN;
	}

	ptNewVideoMem->ptVideoMemNext = g_ptVideoMemHead;
	g_ptVideoMemHead = ptNewVideoMem;
	
	while(iMemNum --){		
		ptNewVideoMem = malloc(sizeof(struct VideoMem) + iVideoMemSize);
		if(NULL == ptNewVideoMem){
			DebugPrint(DEBUG_ERR"malloc VideoMem error\n");
			return -1;
		}
		
		/* �������������� */
		ptNewVideoMem->tPiexlDatas.pucPiexlDatasMem = (unsigned char *)(ptNewVideoMem + 1);
		ptNewVideoMem->iID = 0;
		ptNewVideoMem->bIsDevFrameBuffer = 0;	 /* ��־��ʼ�������豸���Դ� */
		ptNewVideoMem->ePStat  = PS_BLANK;
		ptNewVideoMem->eVMStat = VMS_FREE;
		ptNewVideoMem->tPiexlDatas.iWidth  = iXres;
		ptNewVideoMem->tPiexlDatas.iHeight = iYres;
		ptNewVideoMem->tPiexlDatas.iBpp    = iBpp;
		ptNewVideoMem->tPiexlDatas.iLineLength	= iLineBytes;
		ptNewVideoMem->tPiexlDatas.iTotalLength = iVideoMemSize;

		/* �������� */
		ptNewVideoMem->ptVideoMemNext = g_ptVideoMemHead;
		g_ptVideoMemHead = ptNewVideoMem;
	}

	return 0;
}

/* �ͷ����е��ڴ�� */
void FreeAllVideoMem(void)
{
	struct VideoMem *ptTempVideoMem;
	struct VideoMem *ptPreVideoMem;

	ptTempVideoMem = g_ptVideoMemHead->ptVideoMemNext;
	while(NULL != ptTempVideoMem){
		ptPreVideoMem = ptTempVideoMem;
		ptTempVideoMem = ptTempVideoMem->ptVideoMemNext;
		free(ptPreVideoMem);
	}
}

struct VideoMem *GetVideoMem(int iMemID, int bCur)
{
	struct VideoMem *ptTempVideoMem;

	/* ȡ�����еģ�ID ��ͬ���ڴ�� */
	ptTempVideoMem = g_ptVideoMemHead;
	
	while(NULL != ptTempVideoMem){
		if((ptTempVideoMem->eVMStat == VMS_FREE) && (ptTempVideoMem->iID == iMemID)){
			ptTempVideoMem->eVMStat = bCur ? VMS_BUSY_FOR_MAIN : VMS_BUSY_FOR_PREPARE;
			return ptTempVideoMem;
		}

		ptTempVideoMem = ptTempVideoMem->ptVideoMemNext;
	}

	/* ������ɹ�����ȡ��һ�����еģ�����û�����ݵ��ڴ�� */
	ptTempVideoMem = g_ptVideoMemHead;

	while(NULL != ptTempVideoMem){
		if((ptTempVideoMem->eVMStat == VMS_FREE) && (ptTempVideoMem->ePStat == PS_BLANK)){
			ptTempVideoMem->eVMStat = bCur ? VMS_BUSY_FOR_MAIN : VMS_BUSY_FOR_PREPARE;
			ptTempVideoMem->iID     = iMemID;
			return ptTempVideoMem;
		}

		ptTempVideoMem = ptTempVideoMem->ptVideoMemNext;
	}

	
	/* ������ɹ�����ȡ��һ�����еĵ��ڴ�� */
	ptTempVideoMem = g_ptVideoMemHead;

	while(NULL != ptTempVideoMem){
		if(ptTempVideoMem->eVMStat == VMS_FREE){
			ptTempVideoMem->eVMStat = bCur ? VMS_BUSY_FOR_MAIN : VMS_BUSY_FOR_PREPARE;
			ptTempVideoMem->iID     = iMemID;
			ptTempVideoMem->ePStat  = PS_BLANK;
			return ptTempVideoMem;
		}

		ptTempVideoMem = ptTempVideoMem->ptVideoMemNext;
	}

	/* ��������ɹ������� bCur == 1 ��ȡ��һ������һ���ڴ�� */
	if(1 == bCur){
		ptTempVideoMem = g_ptVideoMemHead;
		
		ptTempVideoMem->eVMStat = bCur ? VMS_BUSY_FOR_MAIN : VMS_BUSY_FOR_PREPARE;
		ptTempVideoMem->iID 	= iMemID;
		ptTempVideoMem->ePStat	= PS_BLANK;
		return ptTempVideoMem;
	}

	return NULL;	
}

/* �ͷ�һ���ڴ�� */
void ReleaseVideoMem(struct VideoMem *ptVideoMem)
{
	ptVideoMem->eVMStat = VMS_FREE;
	if(-1 == ptVideoMem->iID){         /* ���� -1 ˵������û���� */
		ptVideoMem->ePStat = PS_BLANK;
	}
}

int SelectAndInitDefaultDisOpr(char *pcName)
{
	g_ptDefaultDisOpr = GetDisOpr(pcName);
	if(NULL == g_ptDefaultDisOpr){
		DebugPrint(DEBUG_ERR"Can't get default DisOpr\n");
		return -1;
	}

	/* ��ʼ������ */
	g_ptDefaultDisOpr->DisDeviceInit();
	g_ptDefaultDisOpr->CleanScreen(0);

	return 0;
}

struct DisOpr *GetDefaultDisOpr(void)
{
	return g_ptDefaultDisOpr;
}

struct VideoMem *GetDevVideoMem()
{
	struct VideoMem *ptVideoMemTmp;

	ptVideoMemTmp = g_ptVideoMemHead;

	while(ptVideoMemTmp){
		if(ptVideoMemTmp->bIsDevFrameBuffer){
			/* �ҵ��豸���Դ� */
			return ptVideoMemTmp;
		}

		ptVideoMemTmp = ptVideoMemTmp->ptVideoMemNext;
	}

	return NULL; /* ���е�����϶��ǳ����� */
}

/* 
 * ����ڶ�������Ϊ NULL��˵��Ҫ�������ڴ����Ϊָ����ɫ
 */
void ClearVideoMemRegion(struct VideoMem *ptVideoMem, struct DisLayout *ptDisLayout, unsigned int dwColor)
{
	int iXStartRes;
	int iXEndRes;
	int iYStartRes;
	int iYEndRes;
	int iLineBytesToClear;
	int iX;
	int iY;

	int iRed;
	int iGreen;
	int iBlue;
	unsigned short wFinalColor;

	unsigned char *puc8Bpp;
	unsigned short *pw16Bpp;
	unsigned int *pdw32Bpp;

	puc8Bpp  = ptVideoMem->tPiexlDatas.pucPiexlDatasMem;

	/* ������������� */
	if(NULL == ptDisLayout){
		iXStartRes = 0;
		iYStartRes = 0;
		iXEndRes   = ptVideoMem->tPiexlDatas.iWidth;
		iYEndRes   = ptVideoMem->tPiexlDatas.iHeight;
		iLineBytesToClear = ptVideoMem->tPiexlDatas.iLineLength;
	}else{
		iXStartRes = ptDisLayout->iTopLeftX;
		iYStartRes = ptDisLayout->iTopLeftY;
		iXEndRes   = ptDisLayout->iBotRightX;
		iYEndRes   = ptDisLayout->iBotRightY;
		iLineBytesToClear = (iXEndRes - iXStartRes + 1) * ptVideoMem->tPiexlDatas.iBpp / 8;
	}

	switch(ptVideoMem->tPiexlDatas.iBpp){
		case 8: {
			wFinalColor = dwColor;

			puc8Bpp = (unsigned char *)(puc8Bpp 
				+ iYStartRes * ptVideoMem->tPiexlDatas.iLineLength 
				+ iXStartRes * ptVideoMem->tPiexlDatas.iBpp / 8);

			for(iY = iYStartRes; iY < iYEndRes; iY ++){
				memset(puc8Bpp, wFinalColor, iLineBytesToClear);
				puc8Bpp += ptVideoMem->tPiexlDatas.iLineLength;
			}
			
			break;
		}

		case 16: {
			iRed   = (dwColor >> 16 >> 3) & 0x1f;
			iGreen = (dwColor >> 8 >> 2)  & 0x3f;
			iBlue  = (dwColor >> 0 >> 3)  & 0x1f;

			wFinalColor = (iRed << 11) | (iGreen << 5) | (iBlue << 0);

			pw16Bpp = (unsigned short *)(puc8Bpp 
				+ iYStartRes * ptVideoMem->tPiexlDatas.iLineLength 
				+ iXStartRes * ptVideoMem->tPiexlDatas.iBpp / 8);
			
			for(iY = iYStartRes; iY < iYEndRes; iY ++){
				for(iX = iXStartRes; iX < iXEndRes; iX ++){
					pw16Bpp[iX-iXStartRes] = wFinalColor;
				}
				
				pw16Bpp += (ptVideoMem->tPiexlDatas.iLineLength) / 2;
			}

			break;
		}

		case 32: {
	
			pdw32Bpp = (unsigned int *)(puc8Bpp 
				+ iYStartRes * ptVideoMem->tPiexlDatas.iLineLength 
				+ iXStartRes * ptVideoMem->tPiexlDatas.iBpp / 8);

			for(iY = iYStartRes; iY < iYEndRes; iY ++){
				for(iX = iXStartRes; iX < iXEndRes; iX ++){
					pdw32Bpp[iX-iXStartRes] = dwColor;
				}
				pdw32Bpp += (ptVideoMem->tPiexlDatas.iLineLength) / 4;
			}

			break;
		}

		default : {
			DebugPrint(DEBUG_ERR"Can't support this bpp %d\n", ptVideoMem->tPiexlDatas.iBpp);
			break;
		}		
	}
}

int RegisterDispOpr(struct DisOpr *pt_DisOpr)
{
	ListAddTail(&pt_DisOpr->tDisOpr, &g_tDisOprHead);

	return 0;
}

void ShowDisOpr(void)
{
	int iPTNum = 0;
	struct list_head *ptLHTmpPos;	//LH = lis_head
	struct DisOpr *ptDOTmpPos;
	
	LIST_FOR_EACH_ENTRY_H(ptLHTmpPos, &g_tDisOprHead){
		ptDOTmpPos = LIST_ENTRY(ptLHTmpPos, struct DisOpr, tDisOpr);

		printf("%d <---> %s\n", iPTNum++, ptDOTmpPos->name);
	}
}

struct DisOpr *GetDisOpr(char *pcName)
{
	struct list_head *ptLHTmpPos;	//LH = lis_head
	struct DisOpr *ptDOTmpPos;
	
	LIST_FOR_EACH_ENTRY_H(ptLHTmpPos, &g_tDisOprHead){
		ptDOTmpPos = LIST_ENTRY(ptLHTmpPos, struct DisOpr, tDisOpr);

		if(0 == strcmp(pcName, ptDOTmpPos->name)){
			return ptDOTmpPos;
		}
	}

	return NULL;
}

int DisplayInit(void)
{
	int iError;

	iError = FBInit();

	return iError;
}

