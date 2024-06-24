#ifndef _AUDIT_LOG
#define _AUDIT_LOG

void createAuditObj(void);
void detroyAuditObj(void);
void auditlog(const char* msg);
#endif