#pragma once
bool ConnectToServer(const char* remotehostname, unsigned short remoteport);
bool StartClient(void);
bool StopClient(void);
bool IsClientConnected(void);
bool SendCodeToServer(unsigned char Code);
bool SendTargetOrderToServer(char *TargetOrder);
bool SendPreArmCodeToServer(char* Code);
bool SendStateChangeRequestToServer(SystemState_t State);
bool SendCalibToServer(unsigned char Code);

LoginState_t LoginToServer(const char* p_username, const char* p_password, bool with_new_password=false, const char* p_newpassword=nullptr);

void closeServerSocket();
bool SendChangeCAMStateRequestToServer(CAMState_t State);
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
