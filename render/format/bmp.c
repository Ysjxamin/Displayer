#include <picfmt_manag.h>
#include <print_manag.h>
#include <stdio.h>
#include <stdlib.h>

/* ����ָʾ����ע */
#pragma pack(push)	/* ѹջ */
#pragma pack(1)		/* �ṹ�尴��һ�ֽڶ��� */

/* λͼ�ļ����ļ�ͷ */
struct BitMapPicHeader{
	unsigned short bfType; 	/* ���ͣ������� 0x4d42 */
	unsigned long  bfSize;	/* ���ļ��Ĵ�С */
	unsigned short bfReserved1;	/* ��������ʹ�� */
	unsigned short bfReserved2;
	unsigned long  bfOffBytes;	/* �������ݵ�ƫ�Ƶ�ַ */
};

/* λͼ�ļ�����Ϣͷ */
struct BitMapPicInfoHeader{
	unsigned long  biSize;			/* ��Ϣͷ�Ĵ�С�����Ǹýṹ��Ĵ�С */
	unsigned long  biWidth;			/* ͼƬ�Ŀ�� */
	unsigned long  biHeight;
	unsigned short biPlanes;		/*  */
	unsigned short biBitCount;		/* ����λ�� */
	unsigned long  biCompression;
	unsigned long  biSizeImage;		/* �����ܴ�С��Ҳ����ͼƬ��С */
	unsigned long  biXPelsPerMeter;	/* X �ֱ��� */
	unsigned long  biYPelsPerMeter; /* Y �ֱ��� */
	unsigned long  biClrUsed;
	unsigned long  biClrImportant;
};

#pragma pack(pop)	/* �ָ������������ */

static int isSupportBMP(struct FileDesc *ptFileDesc);
static int BMPGetPiexlDatas(struct FileDesc *ptFileDesc, struct PiexlDatasDesc *ptPiexlDatasDesc);
static int BMPFreePiexlDatas(struct PiexlDatasDesc *ptPiexlDatasDesc);


struct PicFmtParser g_tBmpPicParser = {
	.name = "bmp",
	.isSupport      = isSupportBMP,
	.GetPiexlDatas  = BMPGetPiexlDatas,
	.FreePiexlDatas = BMPFreePiexlDatas,
};

static void ConvertOneLine(int iSrcBpp, int iDesBpp, unsigned char *pucSrcPiexl, unsigned char *pucDesPiexl, int iConvLen)
{
	int iPixelNum = 0;
	int iRed;
	int iGreen;
	int iBlue;

	unsigned short *pwDes16Bpp = (unsigned short *)pucDesPiexl;
	unsigned int *pdwDes32Bpp  = (unsigned int *)pucDesPiexl;

	switch(iDesBpp){
		case 16: {
			for(iPixelNum = 0; iPixelNum < iConvLen; iPixelNum ++){
				iBlue  = pucSrcPiexl[3*iPixelNum];
				iGreen = pucSrcPiexl[3*iPixelNum + 1];
				iRed   = pucSrcPiexl[3*iPixelNum + 2];

				iRed   = iRed >> 3;
				iGreen = iGreen >> 2;
				iBlue  = iBlue>> 3;
				
				pwDes16Bpp[iPixelNum] = ((iRed << 11) | (iGreen << 5) | (iBlue << 0));	
			}
			break;
		}
		case 32: {
			for(iPixelNum = 0; iPixelNum < iConvLen; iPixelNum ++){
				iBlue  = pucSrcPiexl[3*iPixelNum];
				iGreen = pucSrcPiexl[3*iPixelNum + 1];
				iRed   = pucSrcPiexl[3*iPixelNum + 2];
				
				pdwDes32Bpp[iPixelNum] = (iRed << 16) | (iGreen << 8) | (iBlue << 0);
			}
			break;
		}

		default : break;
	}
}

static int isSupportBMP(struct FileDesc *ptFileDesc)
{
	unsigned char BMPFileHead[] = {0x42, 0x4d};

	if(ptFileDesc->pucFileMem[0] == BMPFileHead[0] && 
		ptFileDesc->pucFileMem[1] == BMPFileHead[1])
	{
		return 1;	/* ֧�� */
	}

	return 0;	/* ��֧�� */
}

/* ptPiexlDatasDesc->iBpp        = iBpp;
 * Ҫ���ⲿ����
 */
static int BMPGetPiexlDatas(struct FileDesc *ptFileDesc, struct PiexlDatasDesc *ptPiexlDatasDesc)
{
	unsigned char *BMPFileMem;
	struct BitMapPicHeader     *ptBMPHeader;
	struct BitMapPicInfoHeader *ptBMPInfoHeader;

	int iWidth;
	int iHeight;
	int iBMPLineWidth;
	int iBpp;

	int iLineNum = 0;
	unsigned char *pucPiexlSrc;
	unsigned char *pucPiexlDes;

	ptBMPHeader     = (struct BitMapPicHeader*)ptFileDesc->pucFileMem;
	ptBMPInfoHeader = (struct BitMapPicInfoHeader*)(ptFileDesc->pucFileMem + sizeof(struct BitMapPicHeader));
	BMPFileMem = ptFileDesc->pucFileMem;

	iWidth     = ptBMPInfoHeader->biWidth;
	iHeight    = ptBMPInfoHeader->biHeight;
	iBpp       = ptBMPInfoHeader->biBitCount;

	if(iBpp != 24){
		DebugPrint(DEBUG_ERR"BMP bpp = %d\n", iBpp);
		return -1;
	}

	ptPiexlDatasDesc->iWidth      = iWidth;
	ptPiexlDatasDesc->iHeight     = iHeight;
	ptPiexlDatasDesc->iLineLength = iWidth * ptPiexlDatasDesc->iBpp / 8;
	ptPiexlDatasDesc->iTotalLength= iHeight * ptPiexlDatasDesc->iLineLength;

//	DebugPrint("iLineLength = %d\n", ptPiexlDatasDesc->iLineLength);

	ptPiexlDatasDesc->pucPiexlDatasMem = malloc(ptPiexlDatasDesc->iTotalLength);
	if(NULL == ptPiexlDatasDesc->pucPiexlDatasMem){
		DebugPrint(DEBUG_ERR"Has no space for PiexlDatasMem\n");
		return -1;
	}

	/* BMP λͼ���������� 4 �ֽڶ��룬���� LCD �����������ݲ���Ҫ���� */
	iBMPLineWidth  = (iWidth * iBpp / 8 + 3) & ~0x03;
	
	pucPiexlSrc = (unsigned char *)(BMPFileMem + ptBMPHeader->bfOffBytes);
	pucPiexlSrc += (iHeight - 1) * iBMPLineWidth;
	pucPiexlDes = ptPiexlDatasDesc->pucPiexlDatasMem;

	for(iLineNum = 0; iLineNum < iHeight; iLineNum ++){
		ConvertOneLine(iBpp, ptPiexlDatasDesc->iBpp, pucPiexlSrc, pucPiexlDes, iWidth);
		pucPiexlSrc -= iBMPLineWidth;
		pucPiexlDes += ptPiexlDatasDesc->iLineLength;
	}

	return 0;		
}

static int BMPFreePiexlDatas(struct PiexlDatasDesc *ptPiexlDatasDesc)
{
	free(ptPiexlDatasDesc->pucPiexlDatasMem);
	return 0;
}

int BMPInit(void)
{
	return RegisterPicFmtParser(&g_tBmpPicParser);
}

