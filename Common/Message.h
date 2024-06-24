//------------------------------------------------------------------------------------------------
//Include
//------------------------------------------------------------------------------------------------
#ifndef MessageH
#define MessageH

#define MT_COMMANDS              1
#define MT_TARGET_SEQUENCE       2
#define MT_IMAGE                 3
#define MT_TEXT                  4
#define MT_PREARM                5
#define MT_STATE                 6
#define MT_STATE_CHANGE_REQ      7
#define MT_CALIB_COMMANDS        8
#define MT_LOGIN_REQ             9
#define MT_LOGIN_RES             10
#define MT_CAM_STATE_CHANGE_REQ  11
#define MT_CAM_STATE             12
#define MT_HEARTBEAT             13


#define PAN_LEFT_START  0x01
#define PAN_RIGHT_START 0x02
#define PAN_UP_START    0x04
#define PAN_DOWN_START  0x08
#define FIRE_START      0x10
#define PAN_LEFT_STOP   0xFE
#define PAN_RIGHT_STOP  0xFD
#define PAN_UP_STOP     0xFB
#define PAN_DOWN_STOP   0xF7
#define FIRE_STOP       0xEF

#define DEC_X           0x01
#define INC_X           0x02
#define DEC_Y           0x04
#define INC_Y           0x08

#define MAX_USERNAME  10
#define MAX_PASSWORD  20
#define MIN_PASSWORD  10
#define MAX_IP        20
#define MAX_ENGAGE_ORDER        10

enum SystemState_t : unsigned int
{
    UNKNOWN      = 0,
    SAFE         = 0x1,
    PREARMED     = 0x2,
    ENGAGE_AUTO  = 0x4,
    ARMED_MANUAL = 0x8,
    ARMED        = 0x10,
    FIRING       = 0x20,
    LASER_ON     = 0x40,
    CALIB_ON     = 0x80 
};

enum LoginState_t : unsigned int
{
    LOGIN_UNKNOWN = 0,
    LOGIN_OK      = 0x1,
    LOGIN_FAIL    = 0x2,
    DATE_EXPIRED  = 0x4
};

enum CAMState_t : unsigned int
{
    CAM_UNKNOWN   = 0,
    CAM_ON        = 0x1,
    CAM_OFF       = 0x2
};


#define CLEAR_LASER_MASK    (~LASER_ON)
#define CLEAR_FIRING_MASK   (~FIRING)
#define CLEAR_ARMED_MASK    (~ARMED)
#define CLEAR_CALIB_MASK    (~CALIB_ON)
#define CLEAR_LASER_FIRING_ARMED_CALIB_MASK  (~(LASER_ON|FIRING|ARMED|CALIB_ON))

#pragma pack(1)
typedef struct
{
    unsigned int Len;
    unsigned int Type;
} TMesssageHeader;

typedef struct
{
    TMesssageHeader Hdr;
    unsigned char   Commands;
} TMesssageCommands;

typedef struct
{
    TMesssageHeader Hdr;
    char            FiringOrder[11];
} TMesssageTargetOrder;

typedef struct
{
    TMesssageHeader Hdr;
    char            Text[1];
} TMesssageText;

typedef struct
{
    TMesssageHeader Hdr;
    unsigned char   Image[1];
} TMesssageImage;

typedef struct
{
    TMesssageHeader Hdr;
    char            Code[10];
} TMesssagePreArm;

typedef struct
{
    TMesssageHeader Hdr;
    SystemState_t   State;
} TMesssageSystemState;

typedef struct
{
    TMesssageHeader Hdr;
    SystemState_t   State;
} TMesssageChangeStateRequest;

typedef struct
{
    TMesssageHeader Hdr;
    unsigned char   Commands;
} TMesssageCalibCommands;

typedef struct
{
    TMesssageHeader Hdr;
    char            UserName[MAX_USERNAME];
    char            Password[MAX_PASSWORD];
    int             is_change_password;
    char            NewPassword[MAX_PASSWORD]; // valid only if is_change_password = 1
} TMesssageLoginRequest;

typedef struct
{
    TMesssageHeader Hdr;
    LoginState_t    Login;
} TMesssageLoginState;

typedef struct
{
    TMesssageHeader Hdr;
    CAMState_t      State;
} TMesssageChangeCAMStateRequest;

typedef struct
{
    TMesssageHeader Hdr;
    CAMState_t      State;
} TMesssageCAMState;

typedef struct
{
    TMesssageHeader Hdr;
    int HeartBeat;
} TMesssageHeartBeat;
#pragma pack()


#endif
//------------------------------------------------------------------------------------------------
//END of Include
//------------------------------------------------------------------------------------------------

