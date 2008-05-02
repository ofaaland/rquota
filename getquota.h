typedef enum { NONE, UNDER, NOTSTARTED, STARTED, EXPIRED } qstate_t;

typedef struct {
    unsigned long long q_bytes_used;
    unsigned long long q_bytes_softlim;  /* 0 = no limit */
    unsigned long long q_bytes_hardlim;  /* 0 = no limit */
    unsigned long long q_bytes_secleft;	 /* valid if state == STARTED */
    qstate_t           q_bytes_state;

    unsigned long long q_files_used;
    unsigned long long q_files_softlim;  /* 0 = no limit */
    unsigned long long q_files_hardlim;  /* 0 = no limit */
    unsigned long long q_files_secleft;	 /* valid if state == STARTED */
    qstate_t           q_files_state;
} quota_t;

int getquota_lustre(char *fsname, uid_t uid, char *mnt, quota_t *q);
int getquota_nfs(char *fsname, uid_t uid, char *rhost, char *rpath, quota_t *q);


