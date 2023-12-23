#include <input_manag.h>
#include <dis_manag.h>
#include <print_manag.h>

#include <stdlib.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <tslib.h>

static int TSInputDeviceExit(void);
static int TSInputDeviceInit(void);
static int TSGetInputEvent(struct InputEvent *ptInputEvent);

static struct tsdev *g_ptTouchScreenFd = NULL;
static int g_iTSXres;
static int g_iTSYres;
static int g_iBpp;
static struct timeval g_tPreTimeVal;
//static struct timeval g_tCurTimeVal;

struct InputOpr g_tTSInputOpr = {
	.name = "touchsrceen-input",
	.InputDeviceExit = TSInputDeviceExit,
	.InputDeviceInit = TSInputDeviceInit,
	.GetInputEvent   = TSGetInputEvent,
};

static int TSInputDeviceExit(void)
{
	if(NULL != g_ptTouchScreenFd){
		ts_close(g_ptTouchScreenFd);
	}

	return 0;
}

static int TSInputDeviceInit(void)
{
	char *pcTSName = NULL;
	int iError = 0;

	if((pcTSName = getenv("TSLIB_TSDEVICE"))){
		g_ptTouchScreenFd = ts_open(pcTSName, 0);	
	}else{
		g_ptTouchScreenFd = ts_open("/dev/event0", 0);
	}

	if(NULL == g_ptTouchScreenFd){
		DebugPrint(DEBUG_NOTICE"Open ts device error\n");
		return -1;
	}
	
	iError = ts_config(g_ptTouchScreenFd);
	if(iError){
		DebugPrint(DEBUG_NOTICE"Config ts device error\n");
		return -1;
	}
	
	g_tTSInputOpr.iInputDeviceFd = ts_fd(g_ptTouchScreenFd);

	/* g_iBpp �����ﲢû���õ� */
	return GetDisDeviceSize(&g_iTSXres, &g_iTSYres, &g_iBpp);
}

#if 0
static int OutOfTimeToSet(struct timeval *ptPreTime, struct timeval *ptCurTime)
{
	int iPreTime, iCurTime;

	iPreTime = ptPreTime->tv_sec * 1000 + ptPreTime->tv_usec / 1000;
	iCurTime = ptCurTime->tv_sec * 1000 + ptCurTime->tv_usec / 1000;

	if((iCurTime - iPreTime > 200)){
		return -1;
	}

	return 0;
}
#endif
static int TSGetInputEvent(struct InputEvent *ptInputEvent)
{	
	struct ts_sample tTSSample;
	int iError = 0;

	iError = ts_read(g_ptTouchScreenFd, &tTSSample, 1);
	if(iError != 1){
		DebugPrint(DEBUG_WARNING"TS read error\n");
		return -1;
	}

	/* ������������������⣬���ﲻ��ʹ��ts_read������ʱ�����ݣ�����Ļ������ȥ�����
	 * if �жϣ����Ҷ�ȡ������bug����ʱ�����ְ��˺ü��ε��Ƿ�ҳû����ô���
	 * Ȼ����������һ�ε�ʱ��ͻȻ�෭����ҳ
	 * ���϶��߳�֮���û�����ˣ�Ӧ���Ƕ�ȡ������
	 * ����ȷ��������Ϊ������ read ����δ�����������̲߳������У����߳�û��ʱ���ȡ����
	 */
//	gettimeofday(&g_tCurTimeVal, NULL);
#if 0
	printf("tTSSample.sec = %d, tTSSample.usec = %d\n", tTSSample.tv.tv_sec, tTSSample.tv.tv_usec);
	printf("g_tPreTimeVal.sec = %d, g_tPreTimeVal.usec = %d\n", g_tPreTimeVal.tv_sec, g_tPreTimeVal.tv_usec);
	printf("xres = %d yres = %d\n", tTSSample.x, tTSSample.y);

	printf("touchscreen get event\n");
	printf("xres = %d yres = %d\n", tTSSample.x, tTSSample.y);
#endif			
	g_tPreTimeVal = tTSSample.tv;
	ptInputEvent->iInputType = INPUT_TYPE_TS;
	ptInputEvent->iInputCode = INPUT_CODE_UNKNOW;
	ptInputEvent->iXPos      = tTSSample.x;
	ptInputEvent->iYPos      = tTSSample.y;
	ptInputEvent->bPressure  = tTSSample.pressure;
	ptInputEvent->tTimeVal   = tTSSample.tv;

	if((tTSSample.x > g_iTSXres / 3) && (tTSSample.x < g_iTSXres * 2 /3) && 
		(tTSSample.y > g_iTSYres * 2 / 3))
	{
		ptInputEvent->iInputCode = INPUT_CODE_DOWN;
	}

	if((tTSSample.x > g_iTSXres / 3) && (tTSSample.x < g_iTSXres * 2 /3) && 
		(tTSSample.y < g_iTSYres / 3))
	{
		ptInputEvent->iInputCode = INPUT_CODE_UP;
	}

	if((tTSSample.x > g_iTSXres * 2 /3) && 
		(tTSSample.y > g_iTSYres / 3) && (tTSSample.y < g_iTSYres * 2 / 3))
	{
		ptInputEvent->iInputCode = INPUT_CODE_EXIT;
	}

	return 0;
}

int TouchScreenInit(void)
{
	int iError = 0;
	
	iError = TSInputDeviceInit();
	iError |= RegisterInputOpr(&g_tTSInputOpr);
	
	return iError;
}

void TouchScreenExit(void)
{
	TSInputDeviceExit();
	UnregisterInputOpr(&g_tTSInputOpr);
}


