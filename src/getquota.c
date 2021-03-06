/*****************************************************************************\
 *  Copyright (C) 2001-2008 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov>.
 *  UCRL-CODE-2003-005.
 *  
 *  This file is part of Quota, a remote quota program.
 *  For details, see <http://www.llnl.gov/linux/quota/>.
 *  
 *  Quota is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  Quota is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with Quota; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <assert.h>

#include "list.h"
#include "util.h"
#include "getquota.h"
#include "getquota_private.h"

extern char *prog;

quota_t
quota_create(char *label, char *rhost, char *rpath, int thresh)
{
    quota_t q = xmalloc(sizeof(struct quota_struct));

    memset(q, 0, sizeof(struct quota_struct));
    q->q_magic = QUOTA_MAGIC;
    q->q_name = NULL;
    q->q_label = xstrdup(label);
    q->q_rhost = xstrdup(rhost);
    q->q_rpath = xstrdup(rpath);
    q->q_thresh = thresh;

    return q;
}

void
quota_destroy(quota_t q)
{
    assert(q->q_magic == QUOTA_MAGIC);
    if (q->q_name)
        free(q->q_name);
    if (q->q_label)
        free(q->q_label);
    if (q->q_rhost)
        free(q->q_rhost);
    if (q->q_rpath)
        free(q->q_rpath);
    memset(q, 0, sizeof(struct quota_struct));
    free(q);
}

#ifndef NDEBUG
static int
quota_get_test(uid_t uid, quota_t q)
{
    int rc = 0;

    q->q_uid = uid;

    q->q_bytes_used = 0;
    q->q_bytes_softlim = 0;
    q->q_bytes_hardlim = 0;
    q->q_bytes_secleft = 0;
    q->q_bytes_state = NONE;
    q->q_files_used = 0;
    q->q_files_softlim = 0;
    q->q_files_hardlim = 0;
    q->q_files_secleft = 0;
    q->q_files_state = NONE;

    switch (uid) {
        case 100:   /* test 01 - just usage, no limits */
            q->q_bytes_used = 1024*1024;
            q->q_files_used = 455555;
            break;
        case 101:   /* test 02, 03 - expired block quota */
            q->q_bytes_used = 1024*1024*1024;
            q->q_bytes_softlim = 1024*1024;
            q->q_bytes_hardlim = 1024*1024;
            q->q_bytes_state = EXPIRED;
            q->q_files_used = 455555;
            q->q_files_softlim = 1024*1024;
            q->q_files_hardlim = 1024*1024; 
            q->q_files_state = UNDER;
            break;
        case 102:   /* test 04, 05 - expired file quota */
            q->q_bytes_used = 1024;
            q->q_bytes_softlim = 1024*1024;
            q->q_bytes_hardlim = 1024*1024*1024;
            q->q_bytes_state = UNDER;
            q->q_files_used = 455555;
            q->q_files_softlim = 1024;
            q->q_files_hardlim = 1024; 
            q->q_files_state = EXPIRED;
            break;
        case 103:   /* test 06 - high usage (73PB / 17 trillion files) */
            q->q_bytes_used = (unsigned long long)1024*1024*1024*1024*1024*73;
            q->q_files_used = (unsigned long long)1024*1024*1024*1024*17;
            break;
        case 104:   /* test 07 - usage over 90% of hard quota (quota.conf) */
            q->q_bytes_used = 1024*100;
            q->q_bytes_softlim = 1024*105;
            q->q_bytes_hardlim = 1024*105;
            q->q_bytes_state = UNDER;
            break;
        case 105:   /* test 08 - usage over usage soft, timer started */
            q->q_bytes_used = 1024*100;
            q->q_bytes_softlim = 1024*90;
            q->q_bytes_hardlim = 1024*105;
            q->q_bytes_secleft = 60*60*24*3;
            q->q_bytes_state = STARTED;
            break;
        case 106:   /* test 09 - usage over file soft, timer not started */
            q->q_files_used = 1024*100;
            q->q_files_softlim = 1024*90;
            q->q_files_hardlim = 1024*105;
            q->q_files_secleft = 60*60*24*3;
            q->q_files_state = NOTSTARTED;
            break;
        default:
            rc = 1;
            break;
    }
    return rc;
}
#endif

int 
quota_get(uid_t uid, quota_t q)
{
    int rc;

    assert(q->q_magic == QUOTA_MAGIC);
    if (!strcmp(q->q_rhost, "test")) {
#ifndef NDEBUG
        rc = quota_get_test(uid, q);
#else
        fprintf(stderr, "%s: compiled with -DNDEBUG\n", prog);
        rc = 1;
#endif
    } else if (!strcmp(q->q_rhost, "lustre")) {
#if HAVE_LIBLUSTREAPI
        rc = quota_get_lustre(uid, q);        
#else
        fprintf(stderr, "%s: not configured with lustre support\n", prog);
        rc = 1;
#endif
    } else {
        rc = quota_get_nfs(uid, q);        
    }
    return rc;
}

void
quota_adduser(quota_t q, char *name)
{
    assert(q->q_magic == QUOTA_MAGIC);
    q->q_name = xstrdup(name);
}

int
quota_match_uid(quota_t x, uid_t *key)
{
    assert(x->q_magic == QUOTA_MAGIC);

    return (x->q_uid == *key);
}

int
quota_cmp_uid(quota_t x, quota_t y)
{
    assert(x->q_magic == QUOTA_MAGIC);
    assert(y->q_magic == QUOTA_MAGIC);

    if (x->q_uid < y->q_uid)
        return -1;
    if (x->q_uid == y->q_uid)
        return 0;
    return 1;
}

int
quota_cmp_uid_reverse(quota_t x, quota_t y)
{
    assert(x->q_magic == QUOTA_MAGIC);
    assert(y->q_magic == QUOTA_MAGIC);

    if (x->q_uid < y->q_uid)
        return 1;
    if (x->q_uid == y->q_uid)
        return 0;
    return -1;
}

int
quota_cmp_bytes(quota_t x, quota_t y)
{
    assert(x->q_magic == QUOTA_MAGIC);
    assert(y->q_magic == QUOTA_MAGIC);

    if (x->q_bytes_used < y->q_bytes_used)
        return -1;
    if (x->q_bytes_used == y->q_bytes_used)
        return 0;
    return 1;
}

int
quota_cmp_bytes_reverse(quota_t x, quota_t y)
{
    assert(x->q_magic == QUOTA_MAGIC);
    assert(y->q_magic == QUOTA_MAGIC);

    if (x->q_bytes_used < y->q_bytes_used)
        return 1;
    if (x->q_bytes_used == y->q_bytes_used)
        return 0;
    return -1;
}

int
quota_cmp_files(quota_t x, quota_t y)
{
    assert(x->q_magic == QUOTA_MAGIC);
    assert(y->q_magic == QUOTA_MAGIC);

    if (x->q_files_used < y->q_files_used)
        return -1;
    if (x->q_files_used == y->q_files_used)
        return 0;
    return 1;
}

int
quota_cmp_files_reverse(quota_t x, quota_t y)
{
    assert(x->q_magic == QUOTA_MAGIC);
    assert(y->q_magic == QUOTA_MAGIC);

    if (x->q_files_used < y->q_files_used)
        return 1;
    if (x->q_files_used == y->q_files_used)
        return 0;
    return -1;
}

void
quota_report_heading(void)
{
    printf("%-10s %-11s %-11s %-11s %-12s %-12s %-12s\n", "User", 
            "Space-used", "Space-soft", "Space-hard",
            "Files-used", "Files-soft", "Files-hard");
}

void
quota_report_heading_usageonly(void)
{
    printf("%-10s %-11s %-12s\n", "User", "Space-used", "Files-used");
}

int
quota_report(quota_t x, unsigned long *bsize)
{
    assert(x->q_magic == QUOTA_MAGIC);
    if (x->q_name) {
        printf("%-10s %-11llu %-11llu %-11llu %-12llu %-12llu %-12llu\n",
            x->q_name, 
            x->q_bytes_used    / *bsize,
            x->q_bytes_softlim / *bsize,
            x->q_bytes_hardlim / *bsize,
            x->q_files_used,
            x->q_files_softlim,
            x->q_files_hardlim);
    } else {
        printf("%-10u %-11llu %-11llu %-11llu %-12llu %-12llu %-12llu\n",
            x->q_uid, 
            x->q_bytes_used    / *bsize,
            x->q_bytes_softlim / *bsize,
            x->q_bytes_hardlim / *bsize,
            x->q_files_used,
            x->q_files_softlim,
            x->q_files_hardlim);
    }
    return 0;
}

int
quota_report_h(quota_t x, unsigned long *bsize)
{
    char used[64], soft[64], hard[64];

    assert(x->q_magic == QUOTA_MAGIC);

    size2str(x->q_bytes_used, used, sizeof(used));
    size2str(x->q_bytes_softlim, soft, sizeof(soft));
    size2str(x->q_bytes_hardlim, hard, sizeof(hard));

    if (x->q_name) {
        printf("%-10s %-11s %-11s %-11s %-12llu %-12llu %-12llu\n", x->q_name,
            used, soft, hard,
            x->q_files_used,
            x->q_files_softlim,
            x->q_files_hardlim);
    } else {
        printf("%-10u %-11s %-11s %-11s %-12llu %-12llu %-12llu\n", x->q_uid, 
            used, soft, hard,
            x->q_files_used,
            x->q_files_softlim,
            x->q_files_hardlim);
    }
    return 0;
}

int
quota_report_usageonly(quota_t x, unsigned long *bsize)
{
    assert(x->q_magic == QUOTA_MAGIC);
    if (x->q_name) {
        printf("%-10s %-11llu %-12llu\n", x->q_name, 
            x->q_bytes_used / *bsize, x->q_files_used);
    } else {
        printf("%-10u %-11llu %-12llu\n", x->q_uid, 
            x->q_bytes_used / *bsize, x->q_files_used);
    }
    return 0;
}

int
quota_report_usageonly_h(quota_t x, unsigned long *bsize)
{
    char used[64];

    assert(x->q_magic == QUOTA_MAGIC);

    size2str(x->q_bytes_used, used, sizeof(used));

    if (x->q_name)
        printf("%-10s %-11s %-12llu\n", x->q_name, used, x->q_files_used);
    else
        printf("%-10u %-11s %-12llu\n", x->q_uid, used, x->q_files_used);
    return 0;
}

void
quota_print_heading(char *name)
{
    printf("Disk quotas for %s:\n", name);
    printf("%s%s\n", "Filesystem     used   quota  limit    timeleft  ",
                                    "files  quota  limit    timeleft");
}

/* helper for quota_print() */
static void 
daystr(qstate_t state, unsigned long long secs, char *str, int len)
{
    float days;

    assert(len >= 1);
    switch (state) {
        case UNDER:
        case NONE:
            str[0] = '\0';
            break;
        case NOTSTARTED:
            snprintf(str, len, "[7 days]");
            break;
        case STARTED:
            days = (float)secs / (24*60*60);
            snprintf(str, len, "%.1f day%s", days, days > 1.0 ? "s" : "");
            break;
        case EXPIRED:
            sprintf(str, "expired");
            break;
    }
}

/* helper for quota_print() */
static int
over_thresh(unsigned long long used, unsigned long long hard, int thresh)
{
    if (thresh && used >= hard * (thresh/100.0))
        return 1;
    return 0;
}

/* helper for quota_print() */
static int
report_warning(quota_t q, char *label, char *prefix)
{
    char over[64];
    int msg = 0;

    switch (q->q_bytes_state) {
        case NONE:
            break;
        case UNDER:
            if (over_thresh(q->q_bytes_used, q->q_bytes_hardlim, q->q_thresh)) {
                printf("%sBlock usage on %s has exceeded %d%% of quota.\n",
                    prefix, label, q->q_thresh);
                msg++;
            }
            break;
        case NOTSTARTED:
            size2str(q->q_bytes_used - q->q_bytes_softlim, over, sizeof(over));
            printf("%sOver block quota on %s, remove %s within [7 days].\n", 
                    prefix, label, over);
            msg++;
            break;
        case STARTED:
            size2str(q->q_bytes_used - q->q_bytes_softlim, over, sizeof(over));
            printf("%sOver block quota on %s, remove %s within %.1f days.\n", 
                    prefix, label, over, 
                    (float)q->q_bytes_secleft / (24*60*60));
            msg++;
            break;
        case EXPIRED:
            printf("%sOver block quota on %s, time limit expired.\n", 
                    prefix, label);
            msg++;
            break;
    }
    switch (q->q_files_state) {
        case NONE:
        case UNDER:
            break;
        case NOTSTARTED:
            size2str(q->q_files_used - q->q_files_softlim, over, sizeof(over));
            printf("%sOver file quota on %s, remove %s files within [7 days].\n", 
                    prefix, label, over);
            msg++;
            break;
        case STARTED:
            size2str(q->q_files_used - q->q_files_softlim, over, sizeof(over));
            printf("%sOver file quota on %s, remove %s files within %.1f days.\n",
                    prefix, label, over, 
                    (float)q->q_files_secleft / (24*60*60));
            msg++;
            break;
        case EXPIRED:
            printf("%sOver file quota on %s, time limit expired\n", 
                    prefix, label);
            msg++;
            break;
    }
    return msg;
}

/* helper for quota_print() */
static void 
report_usage(quota_t q, char *label)
{
    char used[16], soft[16], hard[16], days[16];

    printf("%-15s", label);
    if (strlen(label) > 14)
        printf("\n%-15s", "");  /* make future columns line up */

    size2str(q->q_bytes_used, used, sizeof(used));
    if (q->q_bytes_state == NONE) {
        strcpy(soft, "n/a");
        strcpy(hard, "n/a");
        strcpy(days, "");
    } else {
        size2str(q->q_bytes_softlim, soft, sizeof(soft));
        size2str(q->q_bytes_hardlim, hard, sizeof(hard));
        daystr(q->q_bytes_state, q->q_bytes_secleft, days, sizeof(days));
    }
    printf("%-7s%-7s%-9s%-10s", used, soft, hard, days);

    size2str(q->q_files_used, used, sizeof(used));
    if (q->q_files_state == NONE) {
        strcpy(soft, "n/a");
        strcpy(hard, "n/a");
        strcpy(days, "");
    } else {
        size2str(q->q_files_softlim, soft, sizeof(soft));
        size2str(q->q_files_hardlim, hard, sizeof(hard));
        daystr(q->q_files_state, q->q_files_secleft, days, sizeof(days));
    }
    printf("%-7s%-7s%-9s%s\n", used, soft, hard, days);
}

int
quota_print_raw(quota_t q, void *arg)
{
    assert(q->q_magic == QUOTA_MAGIC);
    printf("uid:           %u\n", q->q_uid);
    printf("label:         %s\n", q->q_label);
    printf("rhost:         %s\n", q->q_rhost);
    printf("rpath:         %s\n", q->q_rpath);
    printf("thresh(pct):   %d\n", q->q_thresh);
    printf("bytes_used:    %llu\n", q->q_bytes_used);
    printf("bytes_softlim: %llu\n", q->q_bytes_softlim);
    printf("bytes_hardlim: %llu\n", q->q_bytes_hardlim);
    printf("bytes_secleft: %llu\n", q->q_bytes_secleft);
    printf("bytes_state:   %d\n", q->q_bytes_state);
    printf("files_used:    %llu\n", q->q_files_used);
    printf("files_softlim: %llu\n", q->q_files_softlim);
    printf("files_hardlim: %llu\n", q->q_files_hardlim);
    printf("files_secleft: %llu\n", q->q_files_secleft);
    printf("files_state:   %d\n", q->q_files_state);
    return 0;
}

/* helper for quota_print_realpath() */
static char *
make_realpath(quota_t q)
{
    static char tmpstr[64];
    char *label = q->q_rpath;

    if (strcmp(q->q_rhost, "lustre") != 0) {
        snprintf(tmpstr, sizeof(tmpstr), "%s:%s", q->q_rhost, q->q_rpath);
        label = tmpstr;
    }
    return label;
}

int
quota_print_realpath(quota_t q, void *arg)
{
    assert(q->q_magic == QUOTA_MAGIC);
    report_usage(q, make_realpath(q));
    report_warning(q, make_realpath(q), "*** ");
    return 0;
}

int
quota_print(quota_t q, void *arg)
{
    assert(q->q_magic == QUOTA_MAGIC);
    report_usage(q, q->q_label);
    report_warning(q, q->q_label, "*** ");
    return 0;
}

int
quota_print_justwarn(quota_t q, int *msgcount)
{
    assert(q->q_magic == QUOTA_MAGIC);
    *msgcount += report_warning(q, q->q_label, "");
    return 0;
}

int
quota_print_justwarn_realpath(quota_t q, int *msgcount)
{  
    assert(q->q_magic == QUOTA_MAGIC);
    *msgcount += report_warning(q, make_realpath(q), "");
    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
