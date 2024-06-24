#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "SanityCheck.h"
#include "message.h"

int validateStrongPassword(char* password, int passlen) {

    int hasDigit = 0, hasSpecial = 0;

    for (int idx = 0; idx<passlen; idx++) {
        if (isdigit(password[idx])) {
            hasDigit = 1;
        } else if (ispunct(password[idx])) {
            hasSpecial = 1;
        }
    }

    return hasDigit && hasSpecial;
}

int sanity_id(char* buf, int len)
{
    unsigned char inputCnt = 0;
    printf("input your id : ");

    if(len > MAX_USERNAME)  return INPUT_TOO_LONG_ID;
    while (len--) {
        if (isalpha(buf[inputCnt]) || isdigit(buf[inputCnt])) {
        }
        else{
            return INPUT_WRONG_ID_RULE;
        }
        inputCnt++;
    }

    return 1;
}

int sanity_password(char* buf, int len)
{
    printf("input your password : ");

    if(len < MIN_PASSWORD)     return INPUT_TOO_SHORD_PASSWORD;
    if(len > MAX_PASSWORD)         return INPUT_TOO_LONG_PASSWORD;

    if(validateStrongPassword(buf, len)){     
    }
    else{
        return (INPUT_WRONG_PASSWORD_RULE);
    }

    return 1;
}

int sanity_ip(char* buf, int len)
{
    unsigned char inputCnt = 0;
    printf("input your ip : ");

    if(len > MAX_IP)  return INPUT_TOO_LONG_IP;
    while (len--) {
        if (isdigit(buf[inputCnt]) || buf[inputCnt] == '.') {
        }
        else{
            return INPUT_WRONG_IP_RULE;
        }
        inputCnt++;
    }

    return 1;
}

int sanity_engage_order(char* buf, int len)
{
    unsigned char inputCnt = 0;

    if(len > MAX_ENGAGE_ORDER)  return INPUT_TOO_LONG_ENGAGE_ORDER;
    while (len--) {
        if (isdigit(buf[inputCnt])) {
        }
        else{
            return INPUT_WRONG_ENGAGE_ORDER_RULE;
        }
        inputCnt++;
    }

    return 1;    
}

#if 0
int main(void)
{
    int ret = sanity_id();
    if(ret == INPUT_TOO_LONG_ID){
        printf("you entered too long id!\n"); 
        goto ERROR_FINISH;
    }else if(ret == INPUT_WRONG_ID_RULE){
        printf("id rule!\n");
        goto ERROR_FINISH;
    }else{
        printSanityId();
    }

    ret = sanity_password();
    if(ret == INPUT_TOO_SHORD_PASSWORD){
        printf("you entered too shord password!\n");
        goto ERROR_FINISH;
    }else if(ret == INPUT_TOO_LONG_PASSWORD){
        printf("you entered too long password!\n");
        goto ERROR_FINISH;
    }else if(ret == INPUT_WRONG_PASSWORD_RULE){
        printf("wrong password rule!\n");
        goto ERROR_FINISH;
    }else{
        printSanityPass();
    }

    ERROR_FINISH:
    return 0;
}
#endif
