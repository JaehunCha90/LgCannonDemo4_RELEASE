
//------------------------------------------------------------------------------------------------
// File: Cannon.cpp
// Project: LG Exec Ed Program
// Versions:
// 1.0 April 2024 - initial version
//------------------------------------------------------------------------------------------------
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <sys/select.h>
#include "NetworkTCP.h"
#include "TcpSendRecvJpeg.h"
#include "Message.h"
#include "KeyboardSetup.h"
#include "IsRPI.h"
#include <lccv.hpp>
#include "ServoPi.h"
#include "ObjectDetector.h"
#include "lgpio.h"
#include "CvImageMatch.h"
#include "ssd1306.h"

#include "../Database/class_database.h"
#include <string>
#include <iostream>
#include <sstream>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <vector>
#include <ctime>
#include "AuditLog.h"

//#define USE_TFLITE      1 
#define USE_IMAGE_MATCH 1

#define PORT            5000
#define PAN_SERVO       1
#define TILT_SERVO      2
#define MIN_TILT         (-15.0f)
#define MAX_TILT         ( 15.0f)
#define MIN_PAN          (-15.0f)
#define MAX_PAN          ( 15.0f)


#define WIDTH           1920
#define HEIGHT          1080

#define INC             0.5f
#define SKIP_DELAY_MS	5000.0

#define USE_USB_WEB_CAM 0

#define DATABASE_USER   "../Database/user.db"

using namespace cv;
using namespace std;

LoginState_t VerifyLogin(const string username, const string password);
LoginState_t UpdatePassword(const string username, const string password, const string new_password);

static const string password_salt[] = {
	"Apple",
	"Banana",
	"ShineMuscat",
	"Mango",
	"Watermelon",
	"Dragonfuit",
	"Grape",
	"Peach",
	"Orange",
	"Lemon"
};
typedef enum
{
	NOT_ACTIVE,
	ACTIVATE,
	NEW_TARGET,
	LOOKING_FOR_TARGET,
	TRACKING,
	TRACKING_STABLE,
	ENGAGEMENT_IN_PROGRESS,
	ENGAGEMENT_COMPLETE
} TEngagementState;

typedef struct
{
	int                       NumberOfTartgets;
	int                       FiringOrder[10];
	int                       CurrentIndex;
	bool                      HaveFiringOrder;
	volatile TEngagementState State;
	int                       StableCount;
	float                     LastPan;
	float                     LastTilt;
	int                       Target;
} TAutoEngage;


static TAutoEngage            AutoEngage;
static float                  Pan = 0.0f;
static float                  Tilt = 0.0f;
static unsigned char          RunCmds = 0;
static int                    gpioid;
static uint8_t                i2c_node_address = 1;
static bool                   HaveOLED = false;
static int                    OLED_Font = 0;
static pthread_t              NetworkThreadID = -1;
static pthread_t              EngagementThreadID = -1;
static pthread_t              HeartBeatThreadID = -1;
static volatile SystemState_t SystemState = SAFE;
pthread_mutex_t        TCP_Mutex;
static pthread_mutex_t        GPIO_Mutex;
static pthread_mutex_t        I2C_Mutex;
static pthread_mutex_t        Engmnt_Mutex;
static pthread_mutexattr_t    TCP_MutexAttr;
static pthread_mutexattr_t    GPIO_MutexAttr;
static pthread_mutexattr_t    I2C_MutexAttr;
static pthread_mutexattr_t    Engmnt_MutexAttr;
static pthread_cond_t         Engagement_cv;
static float                  xCorrect = 60.0, yCorrect = -90.0;
static volatile bool          isConnected = false;
static volatile bool          isCameraOn = true;
static volatile bool          isLoggedIn = false;
static Servo* Servos = NULL;


#if USE_USB_WEB_CAM
cv::VideoCapture* capture = NULL;
#else
static lccv::PiCamera* capture = NULL;
#endif

static Mat NoDataAvalable;

static TTcpListenPort* TcpListenPort = NULL;
static TTcpConnectedPort* TcpConnectedPort = NULL;

static void   Setup_Control_C_Signal_Handler_And_Keyboard_No_Enter(void);
static void   CleanUp(void);
static void   Control_C_Handler(int s);
static void   HandleInputChar(Mat& image);
static void* NetworkInputThread(void* data);
static void* EngagementThread(void* data);
static void* HeartBeatThread(void* data);
static int    PrintfSend(const char* fmt, ...);
static bool   GetFrame(Mat& frame);
static void   CreateNoDataAvalable(void);
static int    SendSystemState(SystemState_t State);
static int    SendLoginState(LoginState_t State);
static bool   compare_float(float x, float y, float epsilon = 0.5f);
static void   ServoAngle(int Num, float& Angle);
static int    read_exact_bytes(SSL* ssl, void* buffer, int bytes_to_read);

#if 1 //TLS_USED
extern SSL* connected_ssl;
#endif

/*************************************** TF LITE START ********************************************************/
#if USE_TFLITE && !USE_IMAGE_MATCH
static ObjectDetector* detector;
/*************************************** TF LITE END   ********************************************************/
#elif USE_IMAGE_MATCH && !USE_TFLITE
/*************************************** IMAGE_MATCH START *****************************************************/

/*************************************** IMAGE_MATCH END *****************************************************/
#endif

#if 1 //TLS_USED
void print_ssl_info(SSL *ssl) {
	BIO *bio_out = BIO_new_fp(stdout, BIO_NOCLOSE);
	if (bio_out != NULL) {
		SSL_SESSION *session = SSL_get_session(ssl);
		if (session != NULL) {
			SSL_SESSION_print(bio_out, session);
		} else {
			BIO_printf(bio_out, "No SSL session available.\n");
		}
		BIO_free(bio_out);
	}
}
#endif
//------------------------------------------------------------------------------------------------
// static void ReadOffsets
//------------------------------------------------------------------------------------------------
static void ReadOffsets(void)
{
	FILE *fp;
	float x, y;
	char xs[100], ys[100];
	int retval = 0;

	fp = fopen("Correct.ini", "r");
	if (fp == NULL) {
    printf("Error: Could not open file\n");
    return;
	}	
	int result;
	result = fscanf(fp, "%99s %f", xs, &x);
	if (result == EOF) {
		printf("Error: Could not read file\n");
		fclose(fp);
		return;
	}
	retval += result;

	result = fscanf(fp, "%99s %f", ys, &y);
	if (result == EOF) {
		printf("Error: Could not read file\n");
		fclose(fp);
		return;
	}
	retval += result;
	if (retval == 4)
	{
		if ((strcmp(xs, "xCorrect") == 0) && (strcmp(ys, "yCorrect") == 0))
		{
			// TO DO: xCorrect와 yCorrect가 정상 범위에 있는지 확인한다.
			// if (x < -100 || x > 100 || y < -100 || y > 100)
			// {
			//   printf("Error: Invalid values in Correct.ini\n");
			// }
			xCorrect = x;
			yCorrect = y;
			printf("Read Offsets:\n");
			printf("xCorrect= %f\n", xCorrect);
			printf("yCorrect= %f\n", yCorrect);
			if (fp != NULL) {
				if (fclose(fp) == 0)
				{
					return;
				}
				else
				{
					printf("Error: Could not close file\n");
				}
			} else {
				printf("Error: File pointer is NULL\n");
			}
			return;
		} else {
			printf("Error: Could not read values from file\n");
		}
	} else {
		printf("Error: Could not read values from file\n");
	}
	printf("Using Default Offsets\n");
	if (fp != NULL) {
		if (fclose(fp) != 0)
		{
			printf("Error: Could not close file\n");
		}
	} else {
		printf("Error: File pointer is NULL\n");
	}
}
//------------------------------------------------------------------------------------------------
// END  static void readOffsets
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void readOffsets
//------------------------------------------------------------------------------------------------
static void WriteOffsets(void)
{
	FILE *fp;
	float x, y;
	char xs[100], ys[100];

	fp = fopen("Correct.ini", "w+");
	if (fp == NULL)
	{
		printf("Error: Could not open file\n");
		return;
	}
	rewind(fp);
	fprintf(fp, "xCorrect %f\n", xCorrect);
	fprintf(fp, "yCorrect %f\n", yCorrect);

	printf("Wrote Offsets:\n");
	printf("xCorrect= %f\n", xCorrect);
	printf("yCorrect= %f\n", yCorrect);
	fclose(fp);
}
//------------------------------------------------------------------------------------------------
// END  static void readOffsets
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// static bool compare_float
//------------------------------------------------------------------------------------------------
static bool compare_float(float x, float y, float epsilon)
{
	if (fabs(x - y) < epsilon)
		return true; // they are same
	return false;  // they are not same
}
//------------------------------------------------------------------------------------------------
// END static bool compare_float
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ServoAngle
//------------------------------------------------------------------------------------------------
static void ServoAngle(int Num, float& Angle)
{
	pthread_mutex_lock(&I2C_Mutex);
  if (Num == TILT_SERVO)
	{
		if (Angle < MIN_TILT){
      Angle = MIN_TILT;
      PrintfSend("It's already reached to MIN_TILT value.");
    }
		else if (Angle > MAX_TILT) {
      Angle = MAX_TILT;
      PrintfSend("It's already reached to MAX_TILT value.");
    }
			
	}
	else if (Num == PAN_SERVO)
	{
		if (Angle < MIN_PAN){
			Angle = MIN_PAN;
      PrintfSend("It's already reached to MIN_PAN value.");
    }
		else if (Angle > MAX_PAN){
			Angle = MAX_PAN;
      PrintfSend("It's already reached to MAX_PAN value.");
    }
	}
	Servos->angle(Num, Angle);
	pthread_mutex_unlock(&I2C_Mutex);
}
//------------------------------------------------------------------------------------------------
// END static void ServoAngle
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void fire
//------------------------------------------------------------------------------------------------
static void fire(bool value)
{
	if ( value == true ) {
		if (((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == UNKNOWN) ||
			((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == SAFE) ||
			((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == PREARMED)
			)
		{
			//Firing can't be permiited
			printf("Firing can't be permitted in several state\n");
			PrintfSend("Firing can't be permitted in several state.");
			SystemState = PREARMED;
			SendSystemState(SystemState);
			return;
		}
	}

	pthread_mutex_lock(&GPIO_Mutex);
	if (value)
  	{
    	auditlog("FIRE ON");
		SystemState = (SystemState_t)(SystemState | FIRING);
  	}
	else
 	{
		SystemState = (SystemState_t)(SystemState & CLEAR_FIRING_MASK);
  	}
	lgGpioWrite(gpioid, 17, value);
	pthread_mutex_unlock(&GPIO_Mutex);
}
//------------------------------------------------------------------------------------------------
// END static void fire
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void armed
//------------------------------------------------------------------------------------------------
static void armed(bool value)
{
	pthread_mutex_lock(&GPIO_Mutex);
	if (value)
		SystemState = (SystemState_t)(SystemState | ARMED);
	else
		SystemState = (SystemState_t)(SystemState & CLEAR_ARMED_MASK);
	pthread_mutex_unlock(&GPIO_Mutex);
}
//------------------------------------------------------------------------------------------------
// END static void armed
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void calibrate
//------------------------------------------------------------------------------------------------
static void calibrate(bool value)
{
	pthread_mutex_lock(&GPIO_Mutex);
	if (value)
		SystemState = (SystemState_t)(SystemState | CALIB_ON);
	else
		SystemState = (SystemState_t)(SystemState & CLEAR_CALIB_MASK);
	pthread_mutex_unlock(&GPIO_Mutex);
}
//------------------------------------------------------------------------------------------------
// END static void calibrate
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void laser
//------------------------------------------------------------------------------------------------
static void laser(bool value)
{
	pthread_mutex_lock(&GPIO_Mutex);
	if (value)
  	{
    	auditlog("LASER ON");
		SystemState = (SystemState_t)(SystemState | LASER_ON);
 	}
	else
 	{
		SystemState = (SystemState_t)(SystemState & CLEAR_LASER_MASK);
  	}
	lgGpioWrite(gpioid, 18, value);
	pthread_mutex_unlock(&GPIO_Mutex);
}
//------------------------------------------------------------------------------------------------
// END static void laser
//------------------------------------------------------------------------------------------------

static TEngagementState TryToMatch(TAutoEngage *Auto, int width, int height) {
	static chrono::steady_clock::time_point Tbegin;
	static bool trying = false;
	TEngagementState state = LOOKING_FOR_TARGET;

	for (int i = 0; i < NumMatches; i++) {
		if (DetectedMatches[i].match == Auto->Target) {
			float PanError, TiltError;
			PanError = (DetectedMatches[i].center.x + xCorrect) - width / 2;
			Pan = Pan - PanError / 75;
			if (MIN_PAN < Pan && Pan < MAX_PAN)
			{
				ServoAngle(PAN_SERVO, Pan);
			}
			else
			{
				state = ENGAGEMENT_COMPLETE;
				PrintfSend("Skip because of Pan Limit");
				break;
			}

			TiltError = (DetectedMatches[i].center.y + yCorrect) - height / 2;
			Tilt = Tilt - TiltError / 75;

			if (MIN_TILT < Tilt && Tilt < MAX_TILT)
			{
				ServoAngle(TILT_SERVO, Tilt);
			}
			else
			{
				state = ENGAGEMENT_COMPLETE;
				PrintfSend("Skip because of Tilt Limit");
				break;
			}

			if ((compare_float(Auto->LastPan, Pan)) && (compare_float(Auto->LastTilt, Tilt)))
				Auto->StableCount++;
			else
				Auto->StableCount = 0;

			Auto->LastPan = Pan;
			Auto->LastTilt = Tilt;
			if (Auto->StableCount > 2)
				state = TRACKING_STABLE;
			else
				state = TRACKING;
			break;
		}
	}

	if (state == LOOKING_FOR_TARGET) {
		if (!trying) {
			trying = true;
			Tbegin = chrono::steady_clock::now();
		} else {
			chrono::steady_clock::time_point Tend = chrono::steady_clock::now();
			auto diff = chrono::duration_cast<chrono::milliseconds>(Tend - Tbegin).count();
			if (diff > SKIP_DELAY_MS) {
				state = ENGAGEMENT_COMPLETE;
			}
		}
	}

	if (state != LOOKING_FOR_TARGET) {
		trying = false;
	}

	return state;
}

//------------------------------------------------------------------------------------------------
// static void ProcessTargetEngagements
//------------------------------------------------------------------------------------------------
static void ProcessTargetEngagements(TAutoEngage* Auto, int width, int height)
{

	bool NewState = false;

	switch (Auto->State)
	{
	case NOT_ACTIVE:
		break;
	case ACTIVATE:
		Auto->CurrentIndex = 0;
		Auto->State = NEW_TARGET;

	case NEW_TARGET:
		AutoEngage.Target = Auto->FiringOrder[Auto->CurrentIndex];
		Auto->StableCount = 0;
		Auto->LastPan = -99999.99;
		Auto->LastTilt = -99999.99;
		NewState = true;

	case LOOKING_FOR_TARGET:
	case TRACKING:
		{
			int retval;
			TEngagementState state = TryToMatch(Auto, width, height);
			if (Auto->State != state)
			{
				NewState = true;
				Auto->State = state;
			}
			if (NewState)
			{
				if (state == LOOKING_FOR_TARGET)
				{
					armed(false);
					SendSystemState(SystemState);
					PrintfSend("Looking for Target %d", AutoEngage.Target);
				}
				else if (state == TRACKING)
				{
					armed(true);
					SendSystemState(SystemState);
					PrintfSend("Tracking Target Unstable %d", AutoEngage.Target);
				}
				else if (state == TRACKING_STABLE)
				{
					PrintfSend("Target Tracking Stable %d", AutoEngage.Target);
					Auto->State = ENGAGEMENT_IN_PROGRESS;
					printf("Signaling Engagement\n");
					if ((retval = pthread_cond_signal(&Engagement_cv)) != 0)
					{
						printf("pthread_cond_signal Error\n");
						exit(0);
					}
				}
				else if (state == ENGAGEMENT_COMPLETE)
				{
					printf("skipped target: %d\n", Auto->Target);
					PrintfSend("Target not found, skipped target: %d\n", Auto->Target);
					auditlog("Target not found, skipped target");
				}
			}
		}
		break;
	case ENGAGEMENT_IN_PROGRESS:
		{
		}
		break;
	case ENGAGEMENT_COMPLETE:
		{
			AutoEngage.CurrentIndex++;
			if (AutoEngage.CurrentIndex >= AutoEngage.NumberOfTartgets)
			{
				Auto->State = NOT_ACTIVE;
				SystemState = PREARMED;
				SendSystemState(SystemState);
				PrintfSend("Target List Completed");
			}
			else
				Auto->State = NEW_TARGET;
		}
		break;
	default:
		printf("Invaid State\n");
		break;
	}
	return;
}
//------------------------------------------------------------------------------------------------
// END static void ProcessTargetEngagements
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void CreateNoDataAvalable
//------------------------------------------------------------------------------------------------
static void CreateNoDataAvalable(void)
{
	while (!GetFrame(NoDataAvalable))
		printf("blank frame grabbed\n");
	cv::String Text = format("NO DATA");

	int baseline;
	float FontSize = 3.0; // 12.0;
	int Thinkness = 4;

	NoDataAvalable.setTo(cv::Scalar(128, 128, 128));
	Size TextSize = cv::getTextSize(Text, cv::FONT_HERSHEY_COMPLEX, FontSize, Thinkness, &baseline); // Get font size

	int textX = (NoDataAvalable.cols - TextSize.width) / 2;
	int textY = (NoDataAvalable.rows + TextSize.height) / 2;
	putText(NoDataAvalable, Text, Point(textX, textY), cv::FONT_HERSHEY_COMPLEX, FontSize, Scalar(255, 255, 255), Thinkness * Thinkness, cv::LINE_AA);
	putText(NoDataAvalable, Text, Point(textX, textY), cv::FONT_HERSHEY_COMPLEX, FontSize, Scalar(0, 0, 0), Thinkness, cv::LINE_AA);
	printf("frame size %d %d\n", NoDataAvalable.cols, NoDataAvalable.rows);
}
//------------------------------------------------------------------------------------------------
// END static void CreateNoDataAvalable
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static bool OpenCamera
//------------------------------------------------------------------------------------------------
static bool OpenCamera(void)
{
#if USE_USB_WEB_CAM
	capture = new cv::VideoCapture("/dev/video8", cv::CAP_V4L);
	if (!capture->isOpened())
	{
		std::cout << "Failed to open camera." << std::endl;
		delete capture;
		return false;
	}

#else
	capture = new lccv::PiCamera();
	capture->options->video_width = WIDTH;
	capture->options->video_height = HEIGHT;
	capture->options->framerate = 30;
	capture->options->verbose = true;
	capture->startVideo();
	usleep(500 * 1000);
#endif
	return (true);
}
//------------------------------------------------------------------------------------------------
// END static bool OpenCamera
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static bool GetFrame
//------------------------------------------------------------------------------------------------
static bool GetFrame(Mat& frame)
{
#if USE_USB_WEB_CAM
	// wait for a new frame from camera and store it into 'frame'
	capture->read(frame);
	// check if we succeeded
	if (image.empty())
		return (false);
#else
	if (!capture->getVideoFrame(frame, 1000))
		return (false);
#endif

	flip(frame, frame, -1); // if running on PI5 flip(-1)=180 degrees

	return (true);
}
//------------------------------------------------------------------------------------------------
// END static bool GetFrame
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void CloseCamera
//------------------------------------------------------------------------------------------------
static void CloseCamera(void)
{
	if (capture != NULL)
	{
#if USE_USB_WEB_CAM
		capture->release();
#else
		capture->stopVideo();
#endif
		delete capture;
		capture = NULL;
	}
}
//------------------------------------------------------------------------------------------------
// END static void CloseCamera
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void OpenServos
//------------------------------------------------------------------------------------------------
static void OpenServos(void)
{
	Servos = new Servo(0x40, 0.750, 2.250);
}
//------------------------------------------------------------------------------------------------
// END static void OpenServos
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static bool CloseServos
//------------------------------------------------------------------------------------------------
static void CloseServos(void)
{
	if (Servos != NULL)
	{
		delete Servos;
		Servos = NULL;
	}
}
//------------------------------------------------------------------------------------------------
// END static  CloseServos
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void OpenGPIO
//------------------------------------------------------------------------------------------------
static void OpenGPIO(void)
{
	gpioid = lgGpiochipOpen(4);          // 4 - PI 5
	lgGpioClaimOutput(gpioid, 0, 17, 0); // Fire Cannon
	lgGpioClaimOutput(gpioid, 0, 18, 0); // Laser
}
//------------------------------------------------------------------------------------------------
// END static void OpenGPIO
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void CloseGPIO
//------------------------------------------------------------------------------------------------
static void CloseGPIO(void)
{
	lgGpiochipClose(gpioid);
}
//------------------------------------------------------------------------------------------------
// END static void CloseGPIO
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static bool OLEDInit
//------------------------------------------------------------------------------------------------
static bool OLEDInit(void)
{
	uint8_t rc = 0;
	// open the I2C device node
	rc = ssd1306_init(i2c_node_address);

	if (rc != 0)
	{
		printf("no oled attached to /dev/i2c-%d\n", i2c_node_address);
		return (false);
	}
	rc = ssd1306_oled_default_config(64, 128);
	if (rc != 0)
	{
		printf("OLED DIsplay initialization failed\n");
		return (false);
	}
	rc = ssd1306_oled_clear_screen();
	if (rc != 0)
	{
		printf("OLED Clear screen Failed\n");
		return (false);
	}
	ssd1306_oled_set_rotate(0);
	ssd1306_oled_set_XY(0, 0);
	ssd1306_oled_write_line(OLED_Font, (char *)"READY");
	return (true);
}
//------------------------------------------------------------------------------------------------
// END static bool OLEDInit
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void OLED_UpdateStatus
//------------------------------------------------------------------------------------------------
static void OLED_UpdateStatus(void)
{
	char Status[128];
	static SystemState_t LastSystemState = UNKNOWN;
	static SystemState_t LastSystemStateBase = UNKNOWN;
	SystemState_t SystemStateBase;
	if (!HaveOLED)
		return;
	pthread_mutex_lock(&I2C_Mutex);
	if (LastSystemState == SystemState)
	{
		pthread_mutex_unlock(&I2C_Mutex);
		return;
	}
	SystemStateBase = (SystemState_t)(SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK);
	if (SystemStateBase != LastSystemStateBase)
	{
		LastSystemStateBase = SystemStateBase;
		ssd1306_oled_clear_line(0);
		ssd1306_oled_set_XY(0, 0);
		if (SystemStateBase == UNKNOWN)
			strcpy(Status, "Unknown");
		else if (SystemStateBase == SAFE)
			strcpy(Status, "SAFE");
		else if (SystemStateBase == PREARMED)
			strcpy(Status, "PREARMED");
		else if (SystemStateBase == ENGAGE_AUTO)
			strcpy(Status, "ENGAGE AUTO");
		else if (SystemStateBase == ARMED_MANUAL)
			strcpy(Status, "ARMED_MANUAL");
		if (SystemState & ARMED)
			strcat(Status, "-ARMED");
		ssd1306_oled_write_line(OLED_Font, Status);
	}

	if ((SystemState & LASER_ON) != (LastSystemState & LASER_ON) || (LastSystemState == UNKNOWN))
	{
		ssd1306_oled_clear_line(1);
		ssd1306_oled_set_XY(0, 1);
		if (SystemState & LASER_ON)
			strcpy(Status, "LASER-ON");
		else
			strcpy(Status, "LASER-OFF");
		ssd1306_oled_write_line(OLED_Font, Status);
	}
	if ((SystemState & FIRING) != (LastSystemState & FIRING) || (LastSystemState == UNKNOWN))
	{
		ssd1306_oled_clear_line(2);
		ssd1306_oled_set_XY(0, 2);
		if (SystemState & FIRING)
			strcpy(Status, "FIRING-TRUE");
		else
			strcpy(Status, "FIRING-FALSE");
		ssd1306_oled_write_line(OLED_Font, Status);
	}
	LastSystemState = SystemState;
	pthread_mutex_unlock(&I2C_Mutex);
	return;
}
//------------------------------------------------------------------------------------------------
// END static void OLED_UpdateStatus
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void DrawCrosshair
//------------------------------------------------------------------------------------------------
static void DrawCrosshair(Mat& img, Point correct, const Scalar& color)
{
	// Use `shift` to try to gain sub-pixel accuracy
	int shift = 10;
	int m = pow(2, shift);

	Point pt = Point((int)((img.cols / 2 - correct.x / 2) * m), (int)((img.rows / 2 - correct.y / 2) * m));

	int size = int(10 * m);
	int gap = int(4 * m);
	line(img, Point(pt.x, pt.y - size), Point(pt.x, pt.y - gap), color, 1, LINE_8, shift);
	line(img, Point(pt.x, pt.y + gap), Point(pt.x, pt.y + size), color, 1, LINE_8, shift);
	line(img, Point(pt.x - size, pt.y), Point(pt.x - gap, pt.y), color, 1, LINE_8, shift);
	line(img, Point(pt.x + gap, pt.y), Point(pt.x + size, pt.y), color, 1, LINE_8, shift);
	line(img, pt, pt, color, 1, LINE_8, shift);
}
//------------------------------------------------------------------------------------------------
// END static void DrawCrosshair
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// main - This is the main program for the Gel Cannon and contains the control loop
//------------------------------------------------------------------------------------------------
int main(int argc, const char** argv)
{
	Mat                              Frame, ResizedFrame;      // camera image in Mat format 
	float                            avfps=0.0,FPS[16]={0.0,0.0,0.0,0.0,
							    0.0,0.0,0.0,0.0,
							    0.0,0.0,0.0,0.0,
							    0.0,0.0,0.0,0.0};
	int                              retval,i,Fcnt = 0;
	struct sockaddr_in               cli_addr;
	socklen_t                        clilen;
	chrono::steady_clock::time_point Tbegin, Tend;

	ReadOffsets();

	for (i = 0; i < 16; i++)
		FPS[i] = 0.0;

	AutoEngage.State = NOT_ACTIVE;
	AutoEngage.HaveFiringOrder = false;
	AutoEngage.NumberOfTartgets = 0;

	pthread_mutexattr_init(&TCP_MutexAttr);
	pthread_mutexattr_settype(&TCP_MutexAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutexattr_init(&GPIO_MutexAttr);
	pthread_mutexattr_settype(&GPIO_MutexAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutexattr_init(&I2C_MutexAttr);
	pthread_mutexattr_settype(&I2C_MutexAttr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutexattr_init(&Engmnt_MutexAttr);
	pthread_mutexattr_settype(&Engmnt_MutexAttr, PTHREAD_MUTEX_ERRORCHECK);

	if (pthread_mutex_init(&TCP_Mutex, &TCP_MutexAttr) != 0) return -1;
	if (pthread_mutex_init(&GPIO_Mutex, &GPIO_MutexAttr) != 0) return -1;
	if (pthread_mutex_init(&I2C_Mutex, &I2C_MutexAttr) != 0) return -1;
	if (pthread_mutex_init(&Engmnt_Mutex, &Engmnt_MutexAttr) != 0) return -1;

	HaveOLED = OLEDInit();

	printf("OpenCV: Version %s\n", cv::getVersionString().c_str());

	// printf("OpenCV: %s", cv::getBuildInformation().c_str());

#if USE_TFLITE
	printf("TensorFlow Lite Mode\n");
	detector = new ObjectDetector("../TfLite-2.17/Data/detect.tflite", false);
#elif USE_IMAGE_MATCH

	printf("Image Match Mode\n");

	DetectedMatches = new TDetectedMatches[MAX_DETECTED_MATCHES];

	if (LoadRefImages(symbols) == -1)
	{
		printf("Error reading reference symbols\n");
		return -1;
	}

#endif

	if ((TcpListenPort = OpenTcpListenPort(PORT)) == NULL) // Open UDP Network port
	{
		printf("OpenTcpListenPortFailed\n");
		return (-1);
	}

	Setup_Control_C_Signal_Handler_And_Keyboard_No_Enter(); // Set Control-c handler to properly exit clean

	printf("Listening for connections\n");
	clilen = sizeof(cli_addr);
	if ((TcpConnectedPort = AcceptTcpConnection(TcpListenPort, &cli_addr, &clilen)) == NULL)
	{
		printf("AcceptTcpConnection Failed\n");
		return (-1);
	}
	isConnected = true;
	printf("Accepted connection Request\n");
#if 0 //TLS_USED
	CloseTcpListenPort(&TcpListenPort); // Close listen port
#endif
	OpenGPIO();
	laser(false);
	fire(false);
	calibrate(false);

	OpenServos();
	ServoAngle(PAN_SERVO, Pan);
	ServoAngle(TILT_SERVO, Tilt);

	if (!OpenCamera())
	{
		printf("Could not Open Camera\n");
		return (-1);
	}
	else
		printf("Opened Camera\n");

	CreateNoDataAvalable();

	if (pthread_create(&NetworkThreadID, NULL, NetworkInputThread, NULL) != 0)
	{
		printf("Failed to Create Network Input Thread\n");
		exit(0);
	}
	if (pthread_create(&EngagementThreadID, NULL, EngagementThread, NULL) != 0)
	{
		printf("Failed to Create ,Engagement Thread\n");
		exit(0);
	}

	do
	{
		//usleep(250 * 1000);
		//usleep(10 * 1000);
		if (isLoggedIn == false)
			continue;

		Tbegin = chrono::steady_clock::now();

		if (!GetFrame(Frame))
		{
			printf("ERROR! blank frame grabbed\n");
			continue;
		}

		HandleInputChar(Frame); // Handle Keyboard Input
#if USE_TFLITE

		DetectResult *res = detector->detect(Frame);
		for (i = 0; i < detector->DETECT_NUM; ++i)
		{
			int labelnum = res[i].label;
			float score = res[i].score;
			float xmin = res[i].xmin;
			float xmax = res[i].xmax;
			float ymin = res[i].ymin;
			float ymax = res[i].ymax;
			int baseline = 0;

			if (score < 0.10)
				continue;

			cv::rectangle(Frame, Point(xmin, ymin), Point(xmax, ymax), Scalar(10, 255, 0), 2);
			cv::String label = to_string(labelnum) + ": " + to_string(int(score * 100)) + "%";

			Size labelSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &baseline);                                                                            // Get font size
			int label_ymin = std::max((int)ymin, (int)(labelSize.height + 10));                                                                                              // Make sure not to draw label too close to top of window
			rectangle(Frame, Point(xmin, label_ymin - labelSize.height - 10), Point(xmin + labelSize.width, label_ymin + baseline - 10), Scalar(255, 255, 255), cv::FILLED); // Draw white box to put label text in
			putText(Frame, label, Point(xmin, label_ymin - 7), cv::FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 0), 2);                                                           // Draw label text
		}
		delete[] res;
#elif USE_IMAGE_MATCH
		TEngagementState tmpstate = AutoEngage.State;

		if (tmpstate != ENGAGEMENT_IN_PROGRESS)
			FindTargets(Frame);
		ProcessTargetEngagements(&AutoEngage, Frame.cols, Frame.rows);
		if (tmpstate != ENGAGEMENT_IN_PROGRESS)
			DrawTargets(Frame);
#endif
#define FPS_XPOS 0
#define FPS_YPOS 20
		cv::String FPS_label = format("FPS %0.2f", avfps / 16);
		int FPS_baseline = 0;

		Size FPS_labelSize = cv::getTextSize(FPS_label, cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, &FPS_baseline);                                                                                            // Get font size
		int FPS_label_ymin = std::max((int)FPS_YPOS, (int)(FPS_labelSize.height + 10));                                                                                                              // Make sure not to draw label too close to top of window
		rectangle(Frame, Point(FPS_XPOS, FPS_label_ymin - FPS_labelSize.height - 10), Point(FPS_XPOS + FPS_labelSize.width, FPS_label_ymin + FPS_baseline - 10), Scalar(255, 255, 255), cv::FILLED); // Draw white box to put label text in
		putText(Frame, FPS_label, Point(FPS_XPOS, FPS_label_ymin - 7), cv::FONT_HERSHEY_SIMPLEX, 0.7, Scalar(0, 0, 0), 2);                                                                           // Draw label text

		if (SystemState == SAFE)
		{
			Frame = NoDataAvalable.clone();
			resize(Frame, ResizedFrame, Size(Frame.cols / 2, Frame.rows / 2));
		}
		else
		{
			resize(Frame, ResizedFrame, Size(Frame.cols / 2, Frame.rows / 2));
			DrawCrosshair(ResizedFrame, Point((int)xCorrect, (int)yCorrect), Scalar(0, 0, 255)); // BGR
		}

		if ((isConnected) && (isCameraOn)) {
			int ret = 0;
			if ((ret = pthread_mutex_lock(&TCP_Mutex)) != 0) {
				printf("TCP_Mutex ERROR\n");
				continue;
			}

			if ((ret = TcpSendImageAsJpeg(TcpConnectedPort, ResizedFrame)) < 0) {
				pthread_mutex_unlock(&TCP_Mutex);
				continue;
			}
			pthread_mutex_unlock(&TCP_Mutex);
		}

		Tend = chrono::steady_clock::now();
		avfps = chrono::duration_cast<chrono::milliseconds>(Tend - Tbegin).count();
		if (avfps > 0.0)
			FPS[((Fcnt++) & 0x0F)] = 1000.0 / avfps;
		for (avfps = 0.0, i = 0; i < 16; i++)
		{
			avfps += FPS[i];
		}
	} while (isConnected);
	printf("Main Thread Exiting\n");
	CleanUp();
	return 0;
}
//------------------------------------------------------------------------------------------------
// End main
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void * EngagementThread
//------------------------------------------------------------------------------------------------
static void* EngagementThread(void* data)
{
	while (1)
	{
		int ret;
		if ((ret = pthread_mutex_lock(&Engmnt_Mutex)) != 0)
		{
			printf("Engmnt_Mutex ERROR\n");
			break;
		}
		printf("Waiting for Engagement Order\n");
		if ((ret = pthread_cond_wait(&Engagement_cv, &Engmnt_Mutex)) != 0)
		{
			printf("Engagement  pthread_cond_wait ERROR\n");
			break;
		}

		printf("Engagment in Progress\n");
		laser(true);
		SendSystemState(SystemState);
		usleep(1500 * 1000);
		fire(true);
		//SendSystemState((SystemState_t)(SystemState | FIRING));
		SendSystemState((SystemState_t)(SystemState));
		usleep(200 * 1000);
		fire(false);
		laser(false);
		armed(false);
		SendSystemState(SystemState);
		PrintfSend("Engaged Target %d", AutoEngage.Target);
		AutoEngage.State = ENGAGEMENT_COMPLETE;

		if ((ret = pthread_mutex_unlock(&Engmnt_Mutex)) != 0)
		{
			printf("Engagement pthread_cond_wait ERROR\n");
			break;
		}
	}

	return NULL;
}
//------------------------------------------------------------------------------------------------
// END static void * EngagementThread
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static int PrintfSend
//------------------------------------------------------------------------------------------------
static int PrintfSend(const char* fmt, ...)
{
	char Buffer[2048];
	int BytesWritten;
	pthread_mutex_lock(&TCP_Mutex);
	va_list args;
	va_start(args, fmt);
	BytesWritten = vsnprintf(Buffer, sizeof(Buffer), fmt, args);
	va_end(args);
	if (BytesWritten > 0)
	{
		TMesssageHeader MsgHdr;
		BytesWritten++;
		MsgHdr.Len = htonl(BytesWritten);
		MsgHdr.Type = htonl(MT_TEXT);
		if (WriteDataTcp(TcpConnectedPort, (unsigned char *)&MsgHdr, sizeof(TMesssageHeader)) != sizeof(TMesssageHeader)) {
			pthread_mutex_unlock(&TCP_Mutex);
			return (-1);
		}

		if (WriteDataTcp(TcpConnectedPort, (unsigned char *)Buffer, BytesWritten) != BytesWritten) {
			pthread_mutex_unlock(&TCP_Mutex);
			return (-1);
		}
	}

	pthread_mutex_unlock(&TCP_Mutex);
	return (BytesWritten);
}
//------------------------------------------------------------------------------------------------
// END static int PrintfSend
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static int SendSystemState
//------------------------------------------------------------------------------------------------
static int SendSystemState(SystemState_t State)
{
	TMesssageSystemState StateMsg;
	int retval;
	pthread_mutex_lock(&TCP_Mutex);
	StateMsg.State = (SystemState_t)htonl(State);
	StateMsg.Hdr.Len = htonl(sizeof(StateMsg.State));
	StateMsg.Hdr.Type = htonl(MT_STATE);
	OLED_UpdateStatus();
	retval = WriteDataTcp(TcpConnectedPort, (unsigned char *)&StateMsg, sizeof(TMesssageSystemState));
	pthread_mutex_unlock(&TCP_Mutex);
	return (retval);
}
//------------------------------------------------------------------------------------------------
// END static int SendSystemState
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static int SendLoginState
//------------------------------------------------------------------------------------------------
static int SendLoginState(LoginState_t State)
{
	TMesssageLoginState LoginMsg;
	int retval;
	pthread_mutex_lock(&TCP_Mutex);
	LoginMsg.Login = (LoginState_t)htonl(State);
	LoginMsg.Hdr.Len = htonl(sizeof(LoginMsg.Login));
	LoginMsg.Hdr.Type = htonl(MT_LOGIN_RES);
	retval = WriteDataTcp(TcpConnectedPort, (unsigned char *)&LoginMsg, sizeof(TMesssageLoginState));
	pthread_mutex_unlock(&TCP_Mutex);
	return (retval);
}
//------------------------------------------------------------------------------------------------
// END static int SendLoginState
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static int SendCamState
//------------------------------------------------------------------------------------------------
static int SendCamState(CAMState_t State)
{
	TMesssageCAMState CamMsg;
	int retval;
	pthread_mutex_lock(&TCP_Mutex);
	CamMsg.State = (CAMState_t)htonl(State);
	CamMsg.Hdr.Len = htonl(sizeof(CamMsg.State));
	CamMsg.Hdr.Type = htonl(MT_CAM_STATE);
	retval = WriteDataTcp(TcpConnectedPort, (unsigned char *)&CamMsg, sizeof(TMesssageCAMState));
	pthread_mutex_unlock(&TCP_Mutex);
	return (retval);
}
//------------------------------------------------------------------------------------------------
// END static int SendCamState
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ProcessPreArm
//------------------------------------------------------------------------------------------------
static void ProcessPreArm(char* Code)
{
	if (SystemState == SAFE)
	{
		char Decode[] = { 0x61, 0x60, 0x76, 0x75, 0x67, 0x7b, 0x72, 0x7c };
		if ((Code[sizeof(Decode)] == 0) && (strlen(Code) == sizeof(Decode)))
		{
			for (int i = 0; i < sizeof(Decode); i++)
				Code[i] ^= Decode[i];
			if (strcmp((const char *)Code, "PREARMED") == 0)
			{
				SystemState = PREARMED;
				SendSystemState(SystemState);
			}
		} else {
			PrintfSend("Pre-Arm code is not matched.");
		}
	}
}
//------------------------------------------------------------------------------------------------
// END static void ProcessPreArm
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ProcessStateChangeRequest
//------------------------------------------------------------------------------------------------
static void ProcessStateChangeRequest(SystemState_t state)
{
	static bool CalibrateWasOn = false;
	switch (state & CLEAR_LASER_FIRING_ARMED_CALIB_MASK)
	{
	case SAFE:
		{
			laser(false);
			calibrate(false);
			fire(false);
			SystemState = (SystemState_t)(state & CLEAR_LASER_FIRING_ARMED_CALIB_MASK);
			AutoEngage.State = NOT_ACTIVE;
			AutoEngage.HaveFiringOrder = false;
			AutoEngage.NumberOfTartgets = 0;
		}
		break;
	case PREARMED:
		{
			if (((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == ENGAGE_AUTO) ||
				((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == ARMED_MANUAL))
			{
				laser(false);
				fire(false);
				calibrate(false);
				if ((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == ENGAGE_AUTO)
				{
					AutoEngage.State = NOT_ACTIVE;
					AutoEngage.HaveFiringOrder = false;
					AutoEngage.NumberOfTartgets = 0;
				}
				SystemState = (SystemState_t)(state & CLEAR_LASER_FIRING_ARMED_CALIB_MASK);
			}
		}
		break;

	case ENGAGE_AUTO:
		{
			if ((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != PREARMED)
			{
				PrintfSend("Invalid State request to Auto %d\n", SystemState);
			}
			else if (!AutoEngage.HaveFiringOrder)
			{
				PrintfSend("No Firing Order List");
			}
			else
			{
				laser(false);
				calibrate(false);
				fire(false);
				SystemState = (SystemState_t)(state & CLEAR_LASER_FIRING_ARMED_CALIB_MASK);
				AutoEngage.State = ACTIVATE;
			}
		}
		break;
	case ARMED_MANUAL:
		{
			if (((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != PREARMED) &&
				((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != ARMED_MANUAL))
			{
				PrintfSend("Invalid State request to Auto %d\n", SystemState);
			}
			else if ((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) == PREARMED)
			{
				laser(false);
				calibrate(false);
				fire(false);
				SystemState = (SystemState_t)(state & CLEAR_LASER_FIRING_ARMED_CALIB_MASK);
			}
			else
				SystemState = state;
		}
		break;
	default:
		{
			printf("UNKNOWN STATE REQUEST %d\n", state);
		}
		break;
	}

	if (SystemState & LASER_ON)
		laser(true);
	else
		laser(false);

	if (SystemState & CALIB_ON)
	{
		calibrate(true);
		CalibrateWasOn = true;
	}
	else
	{
		calibrate(false);
		if (CalibrateWasOn)
		{
			CalibrateWasOn = false;
			WriteOffsets();
		}
	}

	SendSystemState(SystemState);
}
//------------------------------------------------------------------------------------------------
// END static void ProcessStateChangeRequest
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ProcessFiringOrder
//------------------------------------------------------------------------------------------------
static void ProcessFiringOrder(char* FiringOrder)
{
	int len = strlen(FiringOrder);

	AutoEngage.State = NOT_ACTIVE;
	AutoEngage.HaveFiringOrder = false;
	AutoEngage.NumberOfTartgets = 0;
	AutoEngage.Target = 0;

	if (len > 10)
	{
		printf("Firing order error\n");
		return;
	}
	for (int i = 0; i < len; i++)
	{
		if (FiringOrder[i] < '0' || FiringOrder[i] > '9') {
			printf("Error: FiringOrder contains non-digit character\n");
			return;
		}
		AutoEngage.FiringOrder[i] = FiringOrder[i] - '0';
	}
	if (len > 0)
		AutoEngage.HaveFiringOrder = true;
	else
	{
		AutoEngage.HaveFiringOrder = false;
		PrintfSend("Empty Firing List");
		return;
	}
	AutoEngage.NumberOfTartgets = len;
#if 0  
	printf("Firing order:\n");
	for (int i = 0; i < len; i++) printf("%d\n", AutoEngage.FiringOrder[i]);
	printf("\n\n");
#endif
}
//------------------------------------------------------------------------------------------------
// END static void ProcessFiringOrder
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ProcessCommands
//------------------------------------------------------------------------------------------------
static void ProcessCommands(unsigned char cmd)
{
	if (((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != PREARMED) &&
		((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != ARMED_MANUAL))
	{
		printf("received Commands outside of Pre-Arm or Armed Manual State %x \n", cmd);
		return;
	}
	if (((cmd == FIRE_START) || (cmd == FIRE_STOP)) && ((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != ARMED_MANUAL))
	{
		printf("received Fire Commands outside of Armed Manual State %x \n", cmd);
		return;
	}

	switch (cmd)
	{
	case PAN_LEFT_START:
		RunCmds |= PAN_LEFT_START;
		RunCmds &= PAN_RIGHT_STOP;
		Pan += INC;
		ServoAngle(PAN_SERVO, Pan);
		break;
	case PAN_RIGHT_START:
		RunCmds |= PAN_RIGHT_START;
		RunCmds &= PAN_LEFT_STOP;
		Pan -= INC;
		ServoAngle(PAN_SERVO, Pan);
		break;
	case PAN_UP_START:
		RunCmds |= PAN_UP_START;
		RunCmds &= PAN_DOWN_STOP;
		Tilt += INC;
		ServoAngle(TILT_SERVO, Tilt);
		break;
	case PAN_DOWN_START:
		RunCmds |= PAN_DOWN_START;
		RunCmds &= PAN_UP_STOP;
		Tilt -= INC;
		ServoAngle(TILT_SERVO, Tilt);
		break;
	case FIRE_START:
		RunCmds |= FIRE_START;
		fire(true);
		SendSystemState(SystemState);
		break;
	case PAN_LEFT_STOP:
		RunCmds &= PAN_LEFT_STOP;
		break;
	case PAN_RIGHT_STOP:
		RunCmds &= PAN_RIGHT_STOP;
		break;
	case PAN_UP_STOP:
		RunCmds &= PAN_UP_STOP;
		break;
	case PAN_DOWN_STOP:
		RunCmds &= PAN_DOWN_STOP;
		break;
	case FIRE_STOP:
		RunCmds &= FIRE_STOP;
		fire(false);
		SendSystemState(SystemState);
		break;
	default:
		printf("invalid command %x\n", cmd);
		break;
	}
}
//------------------------------------------------------------------------------------------------
// END static void ProcessCommands
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ProcessCalibCommands
//------------------------------------------------------------------------------------------------
static void ProcessCalibCommands(unsigned char cmd)
{
	if (((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != PREARMED) &&
		((SystemState & CLEAR_LASER_FIRING_ARMED_CALIB_MASK) != ARMED_MANUAL) &&
		!(SystemState & CALIB_ON))
	{
		printf("received Commands outside of Armed Manual State %x \n", cmd);
		return;
	}

	switch (cmd)
	{
	case DEC_X:
		xCorrect++;
		break;
	case INC_X:
		xCorrect--;
		break;
	case DEC_Y:
		yCorrect--;
		break;
	case INC_Y:
		yCorrect++;
		break;

	default:
		printf("invalid command %x\n", cmd);
		break;
	}
}
//------------------------------------------------------------------------------------------------
// END static void ProcessCalibCommands
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// string GetSHA512
//------------------------------------------------------------------------------------------------
std::string GetSHA512(const std::string& str)
{
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	const EVP_MD* md = EVP_sha512();

	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int hash_len;

	EVP_DigestInit_ex(ctx, md, NULL);
	EVP_DigestUpdate(ctx, str.c_str(), str.size());
	EVP_DigestFinal_ex(ctx, hash, &hash_len);

	EVP_MD_CTX_free(ctx);

	std::string hashedString = "";
	for (unsigned int i = 0; i < hash_len; i++)
	{
		char hex[3];
		sprintf(hex, "%02x", hash[i]);
		hashedString += hex;
	}

	return hashedString;
}
//------------------------------------------------------------------------------------------------
// END string GetSHA512
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// string GetSaltHashPassword
//------------------------------------------------------------------------------------------------
string GetSaltHashPassword(const string username, const string password) {
	string hashedPassword = "";
	int salt_idx = 0;

	salt_idx = (username.front() + username.back()) % 10;
	hashedPassword = GetSHA512(password) + password_salt[salt_idx];
	hashedPassword = GetSHA512(hashedPassword);

	return hashedPassword;
}
//------------------------------------------------------------------------------------------------
// END string GetSaltHashPassword
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// LoginState_t VerifyLogin
//------------------------------------------------------------------------------------------------
LoginState_t VerifyLogin(string username, string password)
{
	SQLHandler* sql_handler = new SQLHandler(DATABASE_USER);
	vector<vector<string>> v_result;
	LoginState_t l_state = LOGIN_UNKNOWN;

	string hashedPassword = GetSaltHashPassword(username, password);
	string query = "";

	query = "SELECT * FROM user WHERE username= ? AND password = ?";
	v_result = sql_handler->query(query, 2, &username.at(0), &hashedPassword.at(0));

	if ((int)v_result.size() == 0)
	{
		printf("Login Fail\n");
		l_state = LOGIN_FAIL;
	}
	else
	{
		query = "SELECT created_at FROM user WHERE username = ?";
		v_result = sql_handler->query(query, 1, &username.at(0));

		String created_at = v_result.at(0).at(0);

		tm tm = {};
		istringstream ss(created_at);

		ss >> get_time(&tm, "%Y-%m-%d %H:%M:%S");

		if (ss.fail()) {
			printf("Date parsing failed!\n");
			return LOGIN_FAIL;
		}

		time_t date = mktime(&tm);
		double elapsed_sec = difftime(time(NULL), date);

		if (elapsed_sec < 60 * 60 * 24 * 30) {
			l_state = LOGIN_OK;
			printf("Login OK\n");
		}
		else {
			l_state = DATE_EXPIRED;
			printf("Date expired\n");
		}
	}

	sql_handler->close();
	delete sql_handler;
	return l_state;
}
//------------------------------------------------------------------------------------------------
// END LoginState_t VerifyLogin
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// LoginState_t UpdatePassword
//------------------------------------------------------------------------------------------------

LoginState_t UpdatePassword(string username, string password, string new_password)
{
	SQLHandler* sql_handler = new SQLHandler(DATABASE_USER);
	vector<vector<string>> v_result;
	LoginState_t l_state = LOGIN_UNKNOWN;
	string query = "";
	string hashedPassword = "";

	hashedPassword = GetSaltHashPassword(username, password);
	query = "SELECT * FROM user WHERE username= ? AND password = ?";
	v_result = sql_handler->query(query, 2, &username.at(0), &hashedPassword.at(0));

	if ((int)v_result.size() == 0)
	{
		printf("Update PW FAIL - username doesn't exist\n");
		l_state = LOGIN_FAIL;
	}
	else
	{
		printf("username exists\n");
		hashedPassword = GetSaltHashPassword(username, new_password);

		// Check that if changed password is the same as the existing one.
		query = "SELECT * FROM user WHERE username= ? AND password = ?";
		v_result = sql_handler->query(query, 2, &username.at(0), &hashedPassword.at(0));
		

		if ((int)v_result.size() != 0)
		{
			l_state = LOGIN_FAIL;
			printf("Update PW FAIL - changed password is the same as the existing one\n");
		}
		else
		{
			query = "UPDATE user SET password = ?, created_at = CURRENT_TIMESTAMP WHERE username = ?";
			v_result = sql_handler->query(query, 2, &hashedPassword.at(0), &username.at(0));

			if (VerifyLogin(username, new_password) == LOGIN_OK)
			{
				l_state = LOGIN_OK;
				printf("Update PW OK\n");
			}
			else
			{
				l_state = LOGIN_FAIL;
				printf("Update PW FAIL - verifylogin failed\n");
			}

		}
	}

	sql_handler->close();
	delete sql_handler;
	return l_state;
}

//------------------------------------------------------------------------------------------------
// END LoginState_t UpdatePassword
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// static bool ProcessLogin
//------------------------------------------------------------------------------------------------
static void ProcessLogin(TMesssageLoginRequest* msgLoginRequest)
{
	LoginState_t l_state;
	const string& username = msgLoginRequest->UserName;
	const string& password = msgLoginRequest->Password;
	const int isChangePW = msgLoginRequest->is_change_password;
	const string& new_password = msgLoginRequest->NewPassword;

	if (isChangePW == 0)
	{
		printf("VerifyLogin");
		l_state = VerifyLogin(username, password);
	}
	else if (isChangePW == 1)
	{
		printf("UpdatePassword new : %s", new_password);
		l_state = UpdatePassword(username, password, new_password);
	}
	else
	{
		printf("isChangePW XXXX ");
		l_state = LOGIN_FAIL;
	}

	SendLoginState(l_state);
	isLoggedIn = (l_state == LOGIN_OK);
	if (isLoggedIn)
	{
		if (HeartBeatThreadID == -1)
		{
			if (pthread_create(&HeartBeatThreadID, NULL, HeartBeatThread, NULL) != 0)
			{
				printf("Failed to Create ,HeartBeat Thread\n");
				exit(0);
			}
		}
	}
}
//------------------------------------------------------------------------------------------------
// END static void ProcessLogin
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void ProcessCamStateChangeRequest
//------------------------------------------------------------------------------------------------
static void ProcessCamStateChangeRequest(CAMState_t state)
{
	switch (state)
	{
	case CAM_ON:
		printf("Camera state change: ON");
		isCameraOn = true;
		SendCamState(CAM_ON);
		break;
	case CAM_OFF:
		printf("Camera state change: OFF");
		isCameraOn = false;
		SendCamState(CAM_OFF);
		break;
	default:
		printf("Invalid Camera State Request %d\n", state);
		break;
	}
}
//------------------------------------------------------------------------------------------------
// END static void ProcessCamStateChangeRequest
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static int *read_exact_bytes
//------------------------------------------------------------------------------------------------
static int read_exact_bytes(SSL* ssl, void* buffer, int bytes_to_read)
{
	int total_bytes_read = 0;
	int bytes_read;

	while (total_bytes_read < bytes_to_read) {
		bytes_read = SSL_read(ssl, (unsigned char *)buffer + total_bytes_read, bytes_to_read - total_bytes_read);
		if (bytes_read <= 0) {
			// exception happen or connection closed
			printf("bytes_read fail\n");
			return bytes_read;
		}
		total_bytes_read += bytes_read;
		printf("bytes_read %d, total_bytes_read %d\n", bytes_read, total_bytes_read);
	}

	return total_bytes_read;
}
//------------------------------------------------------------------------------------------------
// END static int *read_exact_bytes
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void *NetworkInputThread
//------------------------------------------------------------------------------------------------
static void* NetworkInputThread(void* data)
{
	unsigned char Buffer[512];
	TMesssageHeader *MsgHdr;
	int fd = TcpConnectedPort->ConnectedFd, retval;

	//  SendSystemState(SystemState);
	createAuditObj();
	while (1)
	{
#if 1 //TLS_USED
		if (connected_ssl == NULL) {
			printf("SSL_new() failed.\n");
		} else {
			printf("SSL_new() succeeded.\n");
		}
		print_ssl_info(connected_ssl);
#endif

		memset(Buffer, 0xFF, sizeof(Buffer));

#if 1 //TLS_USED
		if ((retval = ReadDataTcp(TcpConnectedPort, (unsigned char*)&Buffer, sizeof(TMesssageHeader))) != sizeof(TMesssageHeader))
		//if ((retval = read_exact_bytes(connected_ssl, Buffer, sizeof(TMesssageHeader))) != sizeof(TMesssageHeader))
			//if ((retval = SSL_read(connected_ssl, &Buffer, sizeof(TMesssageHeader))) != sizeof(TMesssageHeader ))
#else
		if ((retval = recv(fd, &Buffer, sizeof(TMesssageHeader), 0)) != sizeof(TMesssageHeader))
#endif
		{
			cout << retval << endl;
			if (retval == 0)
				printf("Client Disconnnected in HeaderSize\n");
			else
				printf("Connecton Lost %s in HeaderSize\n", strerror(errno));
			break;
		}
		MsgHdr = (TMesssageHeader*)Buffer;
		MsgHdr->Len = ntohl(MsgHdr->Len);
		MsgHdr->Type = ntohl(MsgHdr->Type);

		if (MsgHdr->Len > sizeof(Buffer) - sizeof(TMesssageHeader))
		{
			printf("oversized message error %d\n", MsgHdr->Len);
			continue;
		}
#if 1 //TLS_USED
		if ((retval = ReadDataTcp(TcpConnectedPort, &Buffer[sizeof(TMesssageHeader)], MsgHdr->Len)) != MsgHdr->Len)
		//if ((retval = read_exact_bytes(connected_ssl,  MsgHdr->Len)) != MsgHdr->Len)
			//if ((retval = SSL_read(connected_ssl, &Buffer[sizeof(TMesssageHeader)], MsgHdr->Len)) != MsgHdr->Len)
#else
		if ((retval = recv(fd, &Buffer[sizeof(TMesssageHeader)], MsgHdr->Len, 0)) != MsgHdr->Len)
#endif
		{
			if (retval == 0)
				printf("Client Disconnnected in MsgLen\n");
			else
				printf("Connecton Lost %s in MsgLen\n", strerror(errno));
			break;
		}

		if (MsgHdr->Type == MT_LOGIN_REQ)
		{
			auditlog("MT_LOGIN_REQ");
			TMesssageLoginRequest* msgLoginRequest = (TMesssageLoginRequest*)Buffer;
			msgLoginRequest->is_change_password = ntohl(msgLoginRequest->is_change_password);
			ProcessLogin(msgLoginRequest);
			continue;
		}

		if (isLoggedIn == false) {
			usleep(100 * 1000);
			continue;
		}
		switch (MsgHdr->Type)
		{
		case MT_CAM_STATE_CHANGE_REQ:
		{
			auditlog("MT_CAM_STATE_CHANGE_REQ");
			TMesssageChangeCAMStateRequest* msgCamStateChangeRequest = (TMesssageChangeCAMStateRequest*)Buffer;
			msgCamStateChangeRequest->State = (CAMState_t)ntohl(msgCamStateChangeRequest->State);
			ProcessCamStateChangeRequest(msgCamStateChangeRequest->State);
		}
		break;
		case MT_COMMANDS:
		{
			auditlog("MT_COMMANDS");
			TMesssageCommands* msgCmds = (TMesssageCommands*)Buffer;
			ProcessCommands(msgCmds->Commands);
		}
		break;
		case MT_CALIB_COMMANDS:
		{
			auditlog("MT_CALIB_COMMANDS");
			TMesssageCalibCommands* msgCmds = (TMesssageCalibCommands*)Buffer;
			ProcessCalibCommands(msgCmds->Commands);
		}
		break;

		case MT_TARGET_SEQUENCE:
		{
			auditlog("MT_TARGET_SEQUENCE");
			TMesssageTargetOrder* msgTargetOrder = (TMesssageTargetOrder*)Buffer;
			ProcessFiringOrder(msgTargetOrder->FiringOrder);
		}
		break;
		case MT_PREARM:
		{
			auditlog("MT_PREARM");
			TMesssagePreArm* msgPreArm = (TMesssagePreArm*)Buffer;
			ProcessPreArm(msgPreArm->Code);
		}
		break;
		case MT_STATE_CHANGE_REQ:
		{
			auditlog("MT_STATE_CHANGE_REQ");
			TMesssageChangeStateRequest* msgChangeStateRequest = (TMesssageChangeStateRequest*)Buffer;
			msgChangeStateRequest->State = (SystemState_t)ntohl(msgChangeStateRequest->State);

			ProcessStateChangeRequest(msgChangeStateRequest->State);
		}
		break;
		default:
			auditlog("DEFAULT");
			printf("Invalid Message Type: %d\n", MsgHdr->Type);
			break;
		}
	}
	isConnected = false;
	isCameraOn = false;
	isLoggedIn = false;
	NetworkThreadID = -1; // Temp Fix OS probem determining if thread id are valid
	printf("Network Thread Exit\n");
	detroyAuditObj();
	return NULL;
}
//------------------------------------------------------------------------------------------------
// END static void *NetworkInputThread
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
// static void *HeartBeatThread
//------------------------------------------------------------------------------------------------
static void* HeartBeatThread(void* data)
{
	const char* hstring[] = { ".", "|","/","-","\\" };
	printf("HeartBeat Thread Started\n");

	int i = 0;
	while (1) {
		// Sending heartbeat evey 1 seconds
		TMesssageHeartBeat HBMsg;
		int retval;
		usleep(1000000);
		pthread_mutex_lock(&TCP_Mutex);
		HBMsg.HeartBeat = 100;
		HBMsg.Hdr.Len = htonl(sizeof(HBMsg.HeartBeat));
		HBMsg.Hdr.Type = htonl(MT_HEARTBEAT);
		retval = WriteDataTcp(TcpConnectedPort, (unsigned char*)&HBMsg, sizeof(TMesssageHeartBeat));
		pthread_mutex_unlock(&TCP_Mutex);
		printf("Sending HeartBeat %1s\n", hstring[i]);
		i = (i + 1) % 5;
		if (retval == -1) {
			isConnected = false;
			printf("HeartBeat writing fail\n");
		}
	}

	printf("HeartBeat Thread Exit\n");
	return NULL;
}
//------------------------------------------------------------------------------------------------
// END static void *HeartBeatThread
//------------------------------------------------------------------------------------------------
//----------------------------------------------------------------
// Setup_Control_C_Signal_Handler_And_Keyboard_No_Enter - This
// sets uo the Control-c Handler and put the keyboard in a mode
// where it will not
// 1. echo input
// 2. need enter hit to get a character
// 3. block waiting for input
//-----------------------------------------------------------------
static void Setup_Control_C_Signal_Handler_And_Keyboard_No_Enter(void)
{
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = Control_C_Handler; // Setup control-c callback
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
	ConfigKeyboardNoEnterBlockEcho(); // set keyboard configuration
}
//-----------------------------------------------------------------
// END Setup_Control_C_Signal_Handler_And_Keyboard_No_Enter
//-----------------------------------------------------------------
//----------------------------------------------------------------
// CleanUp - Performs cleanup processing before exiting the
// the program
//-----------------------------------------------------------------
static void CleanUp(void)
{
	void *res;
	int s;

	RestoreKeyboard(); // restore Keyboard
	if (NetworkThreadID != -1)
	{
		// printf("Cancel Network Thread\n");
		s = pthread_cancel(NetworkThreadID);
		if (s != 0)
			printf("Network Thread Cancel Failure\n");

		// printf("Network Thread Join\n");
		s = pthread_join(NetworkThreadID, &res);
		if (s != 0)
			printf("Network Thread Join Failure\n");

		if (res == PTHREAD_CANCELED)
			printf("Network Thread canceled\n");
		else
			printf("Network Thread was not canceled\n");
	}
	if (EngagementThreadID != -1)
	{
		// printf("Cancel Engagement Thread\n");
		s = pthread_cancel(EngagementThreadID);
		if (s != 0)
			printf("Engagement Thread Cancel Failure\n");

		// printf("Engagement Thread Join\n");
		s = pthread_join(EngagementThreadID, &res);
		if (s != 0)
			printf("Engagement  Thread Join Failure\n");

		if (res == PTHREAD_CANCELED)
			printf("Engagement Thread canceled\n");
		else
			printf("Engagement Thread was not canceled\n");
	}
	if (HeartBeatThreadID != -1)
	{
		// printf("Cancel HeartBeat Thread\n");
		s = pthread_cancel(HeartBeatThreadID);
		if (s != 0)
			printf("HeartBeat Thread Cancel Failure\n");

		// printf("HeartBeat Thread Join\n");
		s = pthread_join(HeartBeatThreadID, &res);
		if (s != 0)
			printf("HeartBeat  Thread Join Failure\n");

		if (res == PTHREAD_CANCELED)
			printf("HeartBeat Thread canceled\n");
		else
			printf("HeartBeat Thread was not canceled\n");
	}
	CloseCamera();
	CloseServos();

	laser(false);
	fire(false);
	calibrate(false);
	CloseGPIO();

	CloseTcpConnectedPort(&TcpConnectedPort); // Close network port;

	if (HaveOLED)
		ssd1306_end();
	printf("CleanUp Complete\n");
}
//-----------------------------------------------------------------
// END CleanUp
//-----------------------------------------------------------------
//----------------------------------------------------------------
// Control_C_Handler - called when control-c pressed
//-----------------------------------------------------------------
static void Control_C_Handler(int s)
{
	printf("Caught signal %d\n", s);
	CleanUp();
	printf("Exiting\n");
	exit(1);
}
//-----------------------------------------------------------------
// END Control_C_Handler
//-----------------------------------------------------------------
//----------------------------------------------------------------
// HandleInputChar - check if keys are press and proccess keys of
// interest.
//-----------------------------------------------------------------
static void HandleInputChar(Mat& frame)
{
	int ch;
	static unsigned int ImageCount = 0;

	if ((ch = getchar()) != EOF)
	{
		if (ch == 's')
		{
			char String[1024];
			ImageCount++;
			sprintf(String, "images/Capture%u.jpg", ImageCount);
			imwrite(String, frame);
			printf("saved %s\n", String);
		}
	}
}
//-----------------------------------------------------------------
// END HandleInputChar
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------


