#include <picfmt_manag.h>
#include <print_manag.h>
#include <file.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>

struct JPGErrorMgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

static int isSupportJPG(struct FileDesc *ptFileDesc);
static int JPGGetPiexlDatas(struct FileDesc *ptFileDesc, struct PiexlDatasDesc *ptPiexlDatasDesc);
static int JPGFreePiexlDatas(struct PiexlDatasDesc *ptPiexlDatasDesc);

struct PicFmtParser g_tJPGPicParser = {
	.name = "jpg",
	.isSupport = isSupportJPG,
	.GetPiexlDatas  = JPGGetPiexlDatas,
	.FreePiexlDatas = JPGFreePiexlDatas,
};

static void JPGErrorExit(j_common_ptr ptCInfo)
{
	static char errStr[JMSG_LENGTH_MAX];

	struct JPGErrorMgr *ptJPGError;

	ptJPGError = (struct JPGErrorMgr *)ptCInfo->err;

	/* ������Ϣ */
	(*ptCInfo->err->format_message)(ptCInfo, errStr);
//	DebugPrint("%s\n", errStr);

	longjmp(ptJPGError->setjmp_buffer, 1);
}

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
				iBlue  = pucSrcPiexl[3*iPixelNum + 2];
				iGreen = pucSrcPiexl[3*iPixelNum + 1];
				iRed   = pucSrcPiexl[3*iPixelNum];

				iRed   = iRed >> 3;
				iGreen = iGreen >> 2;
				iBlue  = iBlue>> 3;
				
				pwDes16Bpp[iPixelNum] = ((iRed << 11) | (iGreen << 5) | (iBlue << 0));	
			}
			break;
		}
		case 32: {
			for(iPixelNum = 0; iPixelNum < iConvLen; iPixelNum ++){
				iBlue  = pucSrcPiexl[3*iPixelNum + 2];
				iGreen = pucSrcPiexl[3*iPixelNum + 1];
				iRed   = pucSrcPiexl[3*iPixelNum];
				
				pdwDes32Bpp[iPixelNum] = (iRed << 16) | (iGreen << 8) | (iBlue << 0);
			}
			break;
		}

		default : break;
	}
}

static int isSupportJPG(struct FileDesc *ptFileDesc)
{
	struct jpeg_decompress_struct tDInfo;

    /* Ĭ�ϵĴ����������ó����˳�
     * ���ǲο�libjpeg���bmp.c��д�Լ��Ĵ�������
     */
	//struct jpeg_error_mgr tJErr;   
	struct JPGErrorMgr tJerr;
    int iRet;

    fseek(ptFileDesc->ptFp, 0, SEEK_SET);

	// ����ͳ�ʼ��һ��decompression�ṹ��
	// tDInfo.err = jpeg_std_error(&tJErr);
	tDInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = JPGErrorExit;

	if(setjmp(tJerr.setjmp_buffer))
	{
		/* ������������е�����, ��ʾJPEG������� */
        jpeg_destroy_decompress(&tDInfo);
		return 0;
	}
	jpeg_create_decompress(&tDInfo);

	// ��jpeg_read_header���jpg��Ϣ
	jpeg_stdio_src(&tDInfo, ptFileDesc->ptFp);

    iRet = jpeg_read_header(&tDInfo, TRUE);
	jpeg_abort_decompress(&tDInfo);

    return (iRet == JPEG_HEADER_OK);
}

static int JPGGetPiexlDatas(struct FileDesc *ptFileDesc, struct PiexlDatasDesc *ptPiexlDatasDesc)
{	
	struct jpeg_decompress_struct tJpegInfo;
	unsigned char *pucScanBuffer;
	unsigned char *JPGDesBuf;
	int iLineBufferLength;
	int iSacnlines;
	struct JPGErrorMgr tJerr;
//    int iRet;

	/* �����ļ�������ȡ֮�󲻻�ı䣬���� jpg ����Ҫ�ӵ�һ���ֽڿ�ʼ������Ҫ������0 */
    fseek(ptFileDesc->ptFp, 0, SEEK_SET);

	// ����ͳ�ʼ��һ��decompression�ṹ��
	tJpegInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = JPGErrorExit;

	if(setjmp(tJerr.setjmp_buffer))
	{
		/* ������������е�����, ��ʾJPEG������� */
        jpeg_destroy_decompress(&tJpegInfo);
		return 0;
	}

	/* ����һ����ѹ���ṹ�� */
	jpeg_create_decompress(&tJpegInfo);

	/* ָ�������ļ� */
	jpeg_stdio_src(&tJpegInfo, ptFileDesc->ptFp);

	/* ��ȡ�ļ�ͷ�� */
	jpeg_read_header(&tJpegInfo, TRUE);

	/* ��ʼ��ѹ�� */
	jpeg_start_decompress(&tJpegInfo);

	iLineBufferLength = tJpegInfo.output_width * tJpegInfo.output_components;
	pucScanBuffer = malloc(iLineBufferLength);
	if(NULL == pucScanBuffer){
		DebugPrint("No space for pucScanBuffer\n");
		goto error_exit;
	}

	ptPiexlDatasDesc->iHeight = tJpegInfo.output_height;
	ptPiexlDatasDesc->iWidth  = tJpegInfo.output_width;
	ptPiexlDatasDesc->iLineLength  = ptPiexlDatasDesc->iWidth * ptPiexlDatasDesc->iBpp / 8;
	ptPiexlDatasDesc->iTotalLength = ptPiexlDatasDesc->iLineLength * ptPiexlDatasDesc->iHeight;
	JPGDesBuf = malloc(ptPiexlDatasDesc->iTotalLength);
	if(NULL == JPGDesBuf){
		DebugPrint("No space for pucScanBuffer\n");
		free(pucScanBuffer);
		goto error_exit;
	}
	ptPiexlDatasDesc->pucPiexlDatasMem = JPGDesBuf;

	/* ���ϵ����ж�ȡ���� */
	for (iSacnlines = 0; iSacnlines < tJpegInfo.output_height; iSacnlines ++){
		/* ÿ�ζ�ȡһ�е����� */
		jpeg_read_scanlines(&tJpegInfo, &pucScanBuffer, 1);
		ConvertOneLine(24, ptPiexlDatasDesc->iBpp, pucScanBuffer, JPGDesBuf, ptPiexlDatasDesc->iWidth);
		JPGDesBuf += ptPiexlDatasDesc->iLineLength;
	}

	free(pucScanBuffer);
	              
	/* ������ѹ�� */
	jpeg_finish_decompress(&tJpegInfo);
	
	/* �ͷŽṹ�� */
	jpeg_destroy_decompress(&tJpegInfo);

	return 0;
error_exit:
	/* ������ѹ�� */
	jpeg_finish_decompress(&tJpegInfo);
	
	/* �ͷŽṹ�� */
	jpeg_destroy_decompress(&tJpegInfo);

	return -1;
}

static int JPGFreePiexlDatas(struct PiexlDatasDesc *ptPiexlDatasDesc)
{
	free(ptPiexlDatasDesc->pucPiexlDatasMem);
	return 0;
}

int JPGInit(void)
{
	return RegisterPicFmtParser(&g_tJPGPicParser);
}

