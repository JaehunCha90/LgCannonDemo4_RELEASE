#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "AuditLog.h"

static FILE* file = NULL;
time_t timer;
#ifdef SERVER
struct tm* t; 
#else
struct tm t;
#endif

char Filename[50];

#define LOG_FILE_PATH       "Log.txt"
#define LOG_TEMPFILE_PATH   "Log_temp.txt"
#define USE_LOGROTATE
#define MAX_LOG_LINES 10

void createAuditObj(void)
{
#ifdef SERVER
    file = fopen(LOG_FILE_PATH, "a+");
#else
    errno_t err;
    err = fopen_s(&file, LOG_FILE_PATH, "a+");
    if (err != 0)
    {
        printf("File Open Fail!\n");
        exit(1);
    }
#endif
}
void detroyAuditObj(void)
{
    fclose(file);
}

#ifndef USE_LOGROTATE
void trimLogFile()
{
    FILE* originalFile = fopen(LOG_FILE_PATH, "r");
    if (originalFile == NULL) {
        printf("Failed to open original log file.\n");
        return;
    }

    FILE* tempFile = fopen("Log_temp.txt", "w");
    if (tempFile == NULL) {
        printf("Failed to create temporary file.\n");
        fclose(originalFile);
        return;
    }

    char** lines = (char**)malloc(MAX_LOG_LINES * sizeof(char*));
    if (lines == NULL) {
        printf("Failed to allocate memory for lines buffer.\n");
        fclose(originalFile);
        fclose(tempFile);
        return;
    }
    int lineCount = 0;
    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), originalFile) != NULL) {
        if (lineCount < MAX_LOG_LINES) {
            lines[lineCount] = strdup(buffer);
            if (lines[lineCount] == NULL) {
                printf("Failed to allocate memory for line.\n");
                fclose(originalFile);
                fclose(tempFile);
                for (int i = 0; i < lineCount; i++) {
                    free(lines[i]);
                }
                free(lines);
                return;
            }
            lineCount++;
        } else {
            free(lines[0]);
            memmove(lines, lines + 1, (MAX_LOG_LINES - 1) * sizeof(char*));
            lines[MAX_LOG_LINES - 1] = strdup(buffer);
            if (lines[MAX_LOG_LINES - 1] == NULL) {
                printf("Failed to allocate memory for line.\n");
                fclose(originalFile);
                fclose(tempFile);
                for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
                    free(lines[i]);
                }
                free(lines);
                return;
            }
        }
    }

    for (int i = 0; i < lineCount; i++) {
        fputs(lines[i], tempFile);
        free(lines[i]);
    }

    free(lines);
    fclose(originalFile);
    fclose(tempFile);

    remove(LOG_FILE_PATH);
    rename(LOG_TEMPFILE_PATH, LOG_FILE_PATH);

    createAuditObj();
}
#endif

void destroyAuditObj(void)
{
    if (file != NULL) {
        fclose(file);
        file = NULL;
    }
}

void auditlog(const char* msg)
{
    if (file != NULL) {
#ifdef SERVER
        time(&timer);
        t = localtime(&timer);
        fprintf(file, "[%d-%d-%d %02d:%02d:%02d] %s\n",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec,
            msg);
#else
        errno_t err = 0;
        timer = time(NULL);
        err = localtime_s(&t, &timer);
        if (err != 0)
        {
            printf("Cannot read Cur Time\n");
        }
        fprintf(file, "[%d-%d-%d %02d:%02d:%02d] %s\n",
            t.tm_year + 1900,
            t.tm_mon + 1,
            t.tm_mday,
            t.tm_hour,
            t.tm_min,
            t.tm_sec,
            msg);
#endif
        fflush(file);
        #ifndef USE_LOGROTATE
        trimLogFile();
        #endif
    }
}
