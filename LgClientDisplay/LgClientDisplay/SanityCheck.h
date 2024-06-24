#ifndef _SANITY_CHECK
#define _SANITY_CHECK

#define INPUT_TOO_LONG_ID                               (-1)
#define INPUT_WRONG_ID_RULE                             (-2)

#define INPUT_TOO_SHORD_PASSWORD                        (-1)
#define INPUT_TOO_LONG_PASSWORD                         (-2)
#define INPUT_WRONG_PASSWORD_RULE                       (-3)

#define INPUT_TOO_LONG_IP                               (-1)
#define INPUT_WRONG_IP_RULE                             (-2)

#define INPUT_TOO_LONG_ENGAGE_ORDER                     (-1)
#define INPUT_WRONG_ENGAGE_ORDER_RULE                   (-2)

int sanity_id(char* buf, int len);
int sanity_password(char* buf, int len);
int sanity_ip(char* buf, int len);
int sanity_engage_order(char* buf, int len);

#endif