#include <print_manag.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>  
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

static int NetPrintDeviceInit(void);
static int NetPrintDeviceExit(void);
static int NetDoPrint(char *pcPrintBuf);

/* socket */
#define LISTEN_PORT 12345

static int g_iSocketServerFd;

static struct sockaddr_in g_tServerAddr;
static struct sockaddr_in g_tClientAddr;

static int g_iHasClientConnected = 0;

static char g_cCtrlFormClient[512]; /* ���Կͻ��˵Ŀ����ַ� */

/* thread */
static pthread_t g_NetPrintToClientThreadId;
static pthread_t g_NetGetFormClientThreadId;

static pthread_mutex_t g_tMutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_tConVar = PTHREAD_COND_INITIALIZER;

static struct PrintOpr *g_ptCtrlPrintOpr;

/* ���λ����� */
#define RECVBUFF_LEN (16*1024)
static int g_iReadPos  = 0;
static int g_iWritePos = 0;
static char *g_cRecvBuff = NULL;

struct PrintOpr g_tNetPrintOpr = {
	.name = "net-print",
	.PrintDeviceInit = NetPrintDeviceInit,
	.PrintDeviceExit = NetPrintDeviceExit,
	.DoPrint = NetDoPrint,
	.isSelected = 1,
};

static int DataIsFull()
{
	return ((g_iWritePos + 1) % RECVBUFF_LEN == g_iReadPos); 
}

static int DataIsEmpty()
{
	return (g_iWritePos == g_iReadPos);
}

static int PutData(char cData)
{
	if(DataIsFull()){
		return -1;
	}
	g_cRecvBuff[g_iWritePos] = cData;
	g_iWritePos = (g_iWritePos + 1) % RECVBUFF_LEN;

	return 0;
}

static int GetData(char *cData)
{
	if(DataIsEmpty()){
		return -1;
	}
	*cData = g_cRecvBuff[g_iReadPos];
	g_iReadPos = (g_iReadPos + 1) % RECVBUFF_LEN;

	return 0;
}

/* ���ݷ����߳� */
static void *NetPrintToClientThread(void *pVoid)
{
	char cSendBufPack[512];
	int iSendBufLen;
	int iSendBufNum = 0;

	while(1){
		pthread_mutex_lock(&g_tMutex);
		pthread_cond_wait(&g_tConVar, &g_tMutex);

		/* ˵�����ܵ��� debug �ź� */
		if(g_iHasClientConnected){
			while(!DataIsEmpty()){
				while((iSendBufNum < 512) && !DataIsEmpty()){
					GetData(&cSendBufPack[iSendBufNum]);
					iSendBufNum ++;
				}
				
				iSendBufLen =  sendto(g_iSocketServerFd, cSendBufPack, iSendBufNum, 0,
							  (const struct sockaddr *)&g_tClientAddr, sizeof(struct sockaddr));
				iSendBufNum = 0;
				if(iSendBufLen <= 0){
					break;
				}	
			}
		}
		
		pthread_mutex_unlock(&g_tMutex);
	}

	return NULL;
}

/* ���ݽ����߳�,���߳�һֱ���У�����recvfrom���������� */
static void *NetGetFromClientThread(void *pVoid)
{	
	socklen_t iSockLen;
	int iRecvBufLen;

	while(1){
		iSockLen = sizeof(struct sockaddr);
		iRecvBufLen = recvfrom(g_iSocketServerFd, g_cCtrlFormClient, sizeof(g_cCtrlFormClient), 0,
							   (struct sockaddr *)&g_tClientAddr, &iSockLen);
		if(iRecvBufLen > 0){
			g_cCtrlFormClient[iRecvBufLen] = '\0';
			/* debug	    ���ʹ�ӡ���еĻ��λ��������������
			 * console=0  �رմ��ڴ�ӡ
			 * console=1  �������ڴ�ӡ
			 * net=0      �ر������ӡ
			 * net=1      ���������ӡ
			 */
			DebugPrint(DEBUG_DEBUG"Receive buff %s\n", g_cCtrlFormClient);
			if(!strncmp(g_cCtrlFormClient, "debuglevel=", 11)){
				DebugPrint(DEBUG_DEBUG"Get here\n");
				if(g_cCtrlFormClient[12] != '\0'){
					goto next_circle;
				}
				if(g_cCtrlFormClient[11] < '0' || g_cCtrlFormClient[11] > '7'){
					goto next_circle;
				}
				DebugPrint(DEBUG_DEBUG"Get here\n");				
				SetGlobalDebugLevel(g_cCtrlFormClient[11] - '0');
			}
			if(!strncmp(g_cCtrlFormClient, "debug", 5)){
				if(g_cCtrlFormClient[5] != '\0'){
					goto next_circle;
				}
				g_iHasClientConnected = 1;	/* ���յ� debug ��ʱ�����Ϊ�пͻ��˵����� */
				DebugPrint(DEBUG_DEBUG"Branch 1\n");
				DebugPrint("");
			}
			if(!strncmp(g_cCtrlFormClient, "console=", 8)){
				if(g_cCtrlFormClient[9] != '\0'){
					goto next_circle;
				}
				if(g_cCtrlFormClient[8] == '0'){
					g_ptCtrlPrintOpr = GetPrintOpr("console-print");
					if(NULL != g_ptCtrlPrintOpr){
						g_ptCtrlPrintOpr->isSelected = 0;
					}
				}
				if(g_cCtrlFormClient[8] == '1'){
					g_ptCtrlPrintOpr = GetPrintOpr("console-print");
					if(NULL != g_ptCtrlPrintOpr){
						g_ptCtrlPrintOpr->isSelected = 1;
					}
				}
				DebugPrint(DEBUG_DEBUG"Branch 2\n");
			}
			if(!strncmp(g_cCtrlFormClient, "net=", 4)){
				if(g_cCtrlFormClient[5] != '\0'){
					goto next_circle;
				}
				if(g_cCtrlFormClient[4] == '0'){
					g_ptCtrlPrintOpr = GetPrintOpr("net-print");
					if(NULL != g_ptCtrlPrintOpr){
						g_ptCtrlPrintOpr->isSelected = 0;
					}
				}
				if(g_cCtrlFormClient[4] == '1'){
					g_ptCtrlPrintOpr = GetPrintOpr("net-print");
					if(NULL != g_ptCtrlPrintOpr){
						g_ptCtrlPrintOpr->isSelected = 1;
					}
				}
				DebugPrint(DEBUG_DEBUG"Branch 3\n");
			}
			
		}
next_circle:
		iRecvBufLen = 0;
	}
	return NULL;
}

static int NetPrintDeviceInit(void)
{	
	int iError = 0;
	
	/*
	 * SOCK_STREAM : Provides sequenced, reliable, two-way, connection-based byte streams
	 *				 �����ģ��ɿ��ģ�˫��ģ��ֽ���
	 */
	g_iSocketServerFd = socket(AF_INET, SOCK_DGRAM, 0);
	if(-1 == g_iSocketServerFd){
		printf("build socket error\n");
		return -1;
	}

	g_tServerAddr.sin_family	  = AF_INET;	/* ����IPv4 */
	g_tServerAddr.sin_port		  = htons(LISTEN_PORT); /* host to net, short */
	g_tServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);  /* ������е�ip INADDR_ANY = 0.0.0.0*/
	/* �󶨵�ַ��socket */
	iError = bind(g_iSocketServerFd, (const struct sockaddr *)&g_tServerAddr,
					sizeof(struct sockaddr));
	if(-1 == iError){
		printf("Bind error\n");
		return -1;
	}

	/* �����������ݷ����߳� */
	pthread_create(&g_NetPrintToClientThreadId, NULL, NetPrintToClientThread, NULL);

	/* �������ݽ����߳� */
	pthread_create(&g_NetGetFormClientThreadId, NULL, NetGetFromClientThread, NULL);

	g_cRecvBuff = malloc(RECVBUFF_LEN);
	if(NULL == g_cRecvBuff){
		printf("Has no space for receive data\n");
		return -1;
	}
		
	return 0;
}

static int NetPrintDeviceExit(void)
{
	close(g_iSocketServerFd);
	free(g_cRecvBuff);
	
	return 0;
}

static int NetDoPrint(char *pcPrintBuf)
{
	int iBufNum = 0;

	while(pcPrintBuf[iBufNum] != '\0'){
		if(DataIsFull()){
			return -1;
		}
		PutData(pcPrintBuf[iBufNum]);
		iBufNum++;
	}

	/* ���ѷ����߳� */
	pthread_mutex_lock(&g_tMutex);
	pthread_cond_signal(&g_tConVar);
	pthread_mutex_unlock(&g_tMutex);
	
	return iBufNum;
}

int NetPrintInit(void)
{
	int iError = 0;
	
	iError = NetPrintDeviceInit();
	iError |= RegisterPrintOpr(&g_tNetPrintOpr);

	return iError;
}

void NetPrintExit(void)
{
	NetPrintDeviceExit();
	UnregisterPrintOpr(&g_tNetPrintOpr);
}

