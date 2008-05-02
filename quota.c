/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2001-2006 The Regents of the University of California.
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

#include <ctype.h>
#include <pwd.h>
#include <rpc/rpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>          /* MAXHOSTNAMELEN */
#include <signal.h>
#include <assert.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>

#include "getconf.h"
#include "getquota.h"
#include "util.h"

#define TMPSTRSZ        64


static int debug = 0;
static char *prog;

static void quota_usage(void);

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

static int
over_thresh(unsigned long long used, unsigned long long hard, int thresh)
{
    if (thresh && used >= hard * (thresh/100.0))
        return 1;
    return 0;
}

static int
report_warning(char *prefix, char *fsname, quota_t *q, int thresh)
{
    char over[64];
    int msg = 0;

    switch (q->q_bytes_state) {
        case NONE:
            break;
        case UNDER:
            if (over_thresh(q->q_bytes_used, q->q_bytes_hardlim, thresh)) {
                printf("%sBlock usage on %s has exceeded %d%% of quota.\n",
                    prefix, fsname, thresh);
                msg++;
            }
            break;
        case NOTSTARTED:
            size2str(q->q_bytes_used - q->q_bytes_softlim, over, sizeof(over));
            printf("%sOver block quota on %s, remove %s within [7 days].\n", 
                    prefix, fsname, over);
            msg++;
            break;
        case STARTED:
            size2str(q->q_bytes_used - q->q_bytes_softlim, over, sizeof(over));
            printf("%sOver block quota on %s, remove %s within %.1f days.\n", 
                    prefix, fsname, over, 
                    (float)q->q_bytes_secleft / (24*60*60));
            msg++;
            break;
        case EXPIRED:
            printf("%sOver block quota on %s, time limit expired.\n", 
                    prefix, fsname);
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
                    prefix, fsname, over);
            msg++;
            break;
        case STARTED:
            size2str(q->q_files_used - q->q_files_softlim, over, sizeof(over));
            printf("%sOver file quota on %s, remove %s files within %.1f days.\n",
                    prefix, fsname, over, 
                    (float)q->q_files_secleft / (24*60*60));
            msg++;
            break;
        case EXPIRED:
            printf("%sOver file quota on %s, time limit expired\n", 
                    prefix, fsname);
            msg++;
            break;
    }
    return msg;
}

static void 
report_usage(char *label, quota_t *q, int thresh)
{
    char used[TMPSTRSZ], soft[TMPSTRSZ], hard[TMPSTRSZ], days[TMPSTRSZ];

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

static void
report_one(char *label, int vopt, confent_t *conf, uid_t uid)
{
    quota_t q;
    int rc;

    if (!strcmp(conf->cf_host, "lustre"))
        rc = getquota_lustre(label, uid, conf->cf_path, &q);
    else
        rc = getquota_nfs(label, uid, conf->cf_host, conf->cf_path, &q);

    if (rc == 0) {
        if (vopt) {
            report_usage(label, &q, conf->cf_thresh);
            report_warning("*** ", label, &q, conf->cf_thresh);
        } else if (report_warning("", label, &q, conf->cf_thresh) > 0) {
            printf("Run quota -v for more detailed information.\n");
        }
    }
}

static void quota_usage(void)
{
    fprintf(stderr, "Usage: quota [-v] [-l] [-t sec] [-r] [-f config_file] [user]\n");
    exit(1);
}

static void 
alarm_handler(int arg)
{
    fprintf(stderr, "quota: timeout, aborting\n");
    exit(1);
}

static int 
quota_main(int argc, char *argv[])
{
    int vopt = 0, ropt = 0, lopt = 0;
    char *user = NULL;
    struct passwd *pw;
    confent_t *conf;
    int c;
    extern char *optarg;
    extern int optind;

    /* handle args */
    while ((c = getopt(argc, argv, "df:rvlt:T")) != EOF) {
        switch (c) {
        case 'l':              /* display home directory quota only */
            lopt = 1;
            break;
        case 't':               /* set timeout in seconds */
            signal(SIGALRM, alarm_handler);
            alarm(strtoul(optarg, NULL, 10));
            break;
        case 'r':              /* display remote mount pt. */
            ropt = 1;
            break;
        case 'v':              /* verbose output */
            vopt = 1;
            break;
        case 'f':              /* alternate config file */
            setconfent(optarg); /* perror/exit on error */
            break;
        case 'd':              /* turn on debugging */
            debug++;
            break;
        case 'T':              /* (undocumented) internal unit tests */
            test_match_path();
            exit(0);
            break;
        default:
            quota_usage();
        }
    }
    if (optind < argc)
        user = argv[optind++];
    if (optind < argc)
        quota_usage();

    /* getlogin() not appropriate here */
    if (!user)
        pw = getpwuid(getuid());
    else {
        if (isdigit(*user))
            pw = getpwuid(atoi(user));
        else
            pw = getpwnam(user);
    }
    if (!pw) {
        fprintf(stderr, "quota: no such user%s%s\n",
                user ? ": " : "", 
                user ? user : "");
        exit(1);
    }

    /* stderr -> stdout */
    if (close(2) < 0) {
        perror("close stderr");
        exit(1);
    }
    if (dup(1) < 0) {
        perror("dup");
        exit(1);
    }

    if (vopt) {
        printf("Disk quotas for %s:\n", pw->pw_name);
        printf("%s\n",
               "Filesystem     used   quota  limit    timeleft  files  quota  limit    timeleft");
    }

    if (lopt) {
        if ((conf = getconfdescsub(pw->pw_dir)) != NULL) {
            char label[16];

            if (ropt)
                snprintf(label, sizeof(label), "%s:%s", 
                    conf->cf_host, conf->cf_path);
            else
                snprintf(label, sizeof(label), "%s", conf->cf_desc);
            report_one(label, vopt, conf, pw->pw_uid);
        }
    } else {
        while ((conf = getconfent()) != NULL) {
            char label[16];

            if (ropt)
                snprintf(label, sizeof(label), "%s:%s", 
                    conf->cf_host, conf->cf_path);
            else
                snprintf(label, sizeof(label), "%s", conf->cf_desc);
            report_one(label, vopt, conf, pw->pw_uid);
        }
    }
    alarm(0);

    return 0;
}

static void
repquota_usage(void)
{
    fprintf(stderr, "Usage: %s [-s] filesystem\n", prog);
    exit(1);
}

static int 
repquota_main(int argc, char *argv[])
{
    struct passwd *pw;
    confent_t *conf;
    int c;
    int sopt = 0;

    while ((c = getopt(argc, argv, "s")) != EOF) {
        switch (c) {
            case 's':              /* generate uid's from top level fs dirs */
                sopt++;
                break;
            default:
                repquota_usage();
                break;
        }
    }

    if (argc - optind != 1)
        repquota_usage();

    if (!(conf = getconfdesc(argv[optind]))) {
        fprintf(stderr, "%s: %s: not found in quota.conf\n", prog, argv[1]);
        exit(1);
    }

    printf("%s\n",
        "User           used   quota  limit    timeleft  files  quota  limit    timeleft");

    if (sopt) {
        struct dirent *dp;
        DIR *dir;
        char fqp[MAXPATHLEN];
        struct stat sb;
        char tmp[64];

        if (!(dir = opendir(conf->cf_path))) {
            fprintf(stderr, "%s: could not open %s\n", prog, conf->cf_path);
            exit(1);
        }
        while ((dp = readdir(dir))) {
            if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
                continue;
            snprintf(fqp, sizeof(fqp), "%s/%s", conf->cf_path, dp->d_name);
            if (stat(fqp, &sb) == 0) {
                if ((pw = getpwuid(sb.st_uid)))
                    snprintf(tmp, sizeof(tmp), "%s", pw->pw_name);
                else
                    snprintf(tmp, sizeof(tmp), "%s-%u", 
                        basename(fqp), sb.st_uid);
                report_one(tmp, 1, conf, sb.st_uid);
            }
        }
        if (closedir(dir) < 0)
            fprintf(stderr, "%s: closedir %s: %m\n", prog, conf->cf_path);
    } else {
        while ((pw = getpwent()) != NULL) {
            if (pw->pw_uid < 500)
                continue;
            report_one(pw->pw_name, 1, conf, pw->pw_uid);
        }
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int rc;

    prog = basename(argv[0]);
    if (!strcmp(prog, "repquota"))
        rc = repquota_main(argc, argv);
    else
        rc = quota_main(argc, argv);
    exit(rc);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
