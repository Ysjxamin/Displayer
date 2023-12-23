#include <picfmt_manag.h>
#include <pic_operation.h>
#include <print_manag.h>
#include <stdlib.h>
#include <string.h>

/* ���ڲ�ֵ����������
 *
 * Sx ԭͼ��x���� Sw ԭͼ�Ŀ�
 * Sy ԭͼ��y���� Sh ԭͼ�ĸ�
 * Dx ����ͼ��x���� Dw ����ͼ�Ŀ�
 * Dy ����ͼ��y���� Dh ����ͼ�ĸ�
 * 
 * ����˵�����ǳ���ȡֵ���Ŵ���С��������Ļ�Ч�����ã����ԸĽ���������Χ������
 * ȡ�����ص�ƽ��ֵ������������Ŵ�
 * 
 * Sx=Dx*Sw/Dw 
 * Sy=Dy*Sh/Dh
 */
int PicZoomOpr(struct PiexlDatasDesc *ptOrigin, struct PiexlDatasDesc *ptZoom)
{
	unsigned int dwSrcY;
	unsigned int dwSrcX;

	unsigned int dwZoomWidth;
	unsigned int dwZoomHeight;
	unsigned int dwPiexlBytes;
	unsigned int dwZoomX;
	unsigned int dwZoomY;

	unsigned char *pucDesDatas;
	unsigned char *pucSrcDatas;
	float fFw;
	float fFh;

	dwZoomWidth  = ptZoom->iWidth;
	dwZoomHeight = ptZoom->iHeight;
	dwPiexlBytes = ptOrigin->iBpp / 8;

	/* ������ߵ����ؿ�Ȳ�һ�¾Ͳ��������� */
	if(ptOrigin->iBpp != ptZoom->iBpp){
		return -1;
	}

	fFw = (float)(ptOrigin->iWidth) / (float)(dwZoomWidth);
	fFh = (float)(ptOrigin->iHeight) / (float)(dwZoomHeight);

	/* �� Y ����г����������� X ��ĳ������� */
	for(dwZoomY = 0; dwZoomY < dwZoomHeight; dwZoomY ++){
		dwSrcY = (unsigned int)(dwZoomY * fFh);

		pucDesDatas = ptZoom->pucPiexlDatasMem + dwZoomY * ptZoom->iLineLength;
		pucSrcDatas = ptOrigin->pucPiexlDatasMem + dwSrcY * ptOrigin->iLineLength;

		for(dwZoomX = 0; dwZoomX < dwZoomWidth; dwZoomX ++){
			dwSrcX = (unsigned int)(dwZoomX * fFw);
			memcpy(pucDesDatas + dwZoomX * dwPiexlBytes, 
				pucSrcDatas + dwSrcX * dwPiexlBytes, dwPiexlBytes);
		}
	}

	return 0;
}


/* ���ڲ�ֵ����������
 *
 * Sx ԭͼ��x���� Sw ԭͼ�Ŀ�
 * Sy ԭͼ��y���� Sh ԭͼ�ĸ�
 * Dx ����ͼ��x���� Dw ����ͼ�Ŀ�
 * Dy ����ͼ��y���� Dh ����ͼ�ĸ�
 * 
 * ����˵�����ǳ���ȡֵ���Ŵ���С��������Ļ�Ч�����ã����ԸĽ���������Χ������
 * ȡ�����ص�ƽ��ֵ������������Ŵ�
 * 
 * Sx=Dx*Sw/Dw 
 * Sy=Dy*Sh/Dh
 */
int PicZoomOprBackup(struct PiexlDatasDesc *ptOrigin, struct PiexlDatasDesc *ptZoom)
{
	unsigned int *pdwSrcXTable;
	unsigned int dwSrcY;

	unsigned int dwZoomWidth;
	unsigned int dwZoomHeight;
	unsigned int dwPiexlBytes;
	unsigned int dwZoomX;
	unsigned int dwZoomY;

	unsigned char *pucDesDatas;
	unsigned char *pucSrcDatas;

	dwZoomWidth  = ptZoom->iWidth;
	dwZoomHeight = ptZoom->iHeight;
	dwPiexlBytes = ptOrigin->iBpp / 8;

	/* ������ߵ����ؿ�Ȳ�һ�¾Ͳ��������� */
	if(ptOrigin->iBpp != ptZoom->iBpp){
		return -1;
	}

	pdwSrcXTable = malloc(sizeof(unsigned int) * (dwZoomWidth));
	if(NULL == pdwSrcXTable){
		DebugPrint(DEBUG_ERR"pdwSrcXTable Malloc error\n");
		return -1;
	}

	/* �� X ����г��� */
	for(dwZoomX = 0; dwZoomX < dwZoomWidth; dwZoomX ++){
		pdwSrcXTable[dwZoomX] = (dwZoomX * ptOrigin->iWidth / ptZoom->iWidth);
	}

	/* �� Y ����г����������� X ��ĳ������� */
	for(dwZoomY = 0; dwZoomY < dwZoomHeight; dwZoomY ++){
		dwSrcY = (dwZoomY * ptOrigin->iHeight / ptZoom->iHeight);

		pucDesDatas = ptZoom->pucPiexlDatasMem + dwZoomY * ptZoom->iLineLength;
		pucSrcDatas = ptOrigin->pucPiexlDatasMem + dwSrcY * ptOrigin->iLineLength;

		for(dwZoomX = 0; dwZoomX < dwZoomWidth; dwZoomX ++){
			memcpy(pucDesDatas + dwZoomX * dwPiexlBytes, 
				pucSrcDatas + pdwSrcXTable[dwZoomX] * dwPiexlBytes, dwPiexlBytes);
			
		}
	}

	free(pdwSrcXTable);

	return 0;
}


/* 
 * ��С��ͼƬ�ϲ������ͼƬ����
 * iXPos �� iYPos ȷ��ͼƬ���Ͻǵ�λ��(��ͼƬ����)
 * �����������궼������ڴ�ͼƬ�����Ͻǵõ���
 * 
 */
int PicMergeOpr(int iXPos, int iYPos, struct PiexlDatasDesc *ptSmallPic, struct PiexlDatasDesc *ptBigPic)
{
	unsigned char *pucDesDatas;
	unsigned char *pucSrcDatas;
	unsigned int dwPiexlLineNum;
	int dwYSize;
	int dwXSize;

	if((ptSmallPic->iHeight > ptBigPic->iHeight) || 
		(ptSmallPic->iWidth > ptBigPic->iWidth) || 
		(ptSmallPic->iBpp != ptBigPic->iBpp))
	{
		return -1;
	}

	dwXSize = ptSmallPic->iWidth;
	dwYSize = ptSmallPic->iHeight;

	if(iYPos < 0){
		iYPos = 0;
	}

	if(iXPos < 0){
		iXPos = 0;
	}

	if(ptSmallPic->iHeight + iYPos > ptBigPic->iHeight){
		dwYSize = ptBigPic->iHeight - iYPos;
	}

	if(ptSmallPic->iWidth + iXPos > ptBigPic->iWidth){
		dwXSize = ptBigPic->iWidth - iXPos;
	}

	pucSrcDatas = ptSmallPic->pucPiexlDatasMem;
	pucDesDatas = ptBigPic->pucPiexlDatasMem + iYPos * ptBigPic->iLineLength + iXPos * ptBigPic->iBpp / 8;

	for(dwPiexlLineNum = 0; dwPiexlLineNum < dwYSize; dwPiexlLineNum ++){
		memcpy(pucDesDatas, pucSrcDatas, dwXSize * ptBigPic->iBpp / 8);
		pucSrcDatas += ptSmallPic->iLineLength;
		pucDesDatas += ptBigPic->iLineLength;
	}

	return 0;
}

