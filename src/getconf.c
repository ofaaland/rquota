/*****************************************************************************\
 *  Copyright (C) 2001-2008 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov>.
 *  UCRL-CODE-2003-005.
 *  
 *  This file is part of Quota, a remote NFS quota program.
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

/*
 * Code to parse the quota.conf file.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "getconf.h"
#include "list.h"
#include "util.h"

/* configuration file pointer */
static FILE *fsys = NULL;

/*
 * Helper for getconfent().  Parse out a field within a string.
 * Replace separators with NULL's and return the field or NULL if at end.
 * 	p (IN/OUT)	string pointer - moved past next separator
 * 	sep (IN)	separator character
 *	RETURN		the base string (now \0 terminated at sep position)
 */
static char *
next_field(char **str, char sep)
{
    char *rv = *str;

    while (**str != '\0' && **str != sep && **str != '\n')
        (*str)++;

    *(*str)++ = '\0';

    return rv;
}

/*
 * Open/rewind the config file
 * 	path (IN)	pathname to open if not open already
 */
void 
setconfent(char *path)
{
    if (!fsys) {
        fsys = fopen(path, "r");
        if (fsys == NULL) {
            perror(path);
            exit(1);
        }
    } else
        rewind(fsys);
}

/*
 * Close the config file if open.
 */
void 
endconfent(void)
{
    if (fsys)
        fclose(fsys);
}

/*
 * Return the next configuration file entry.  Open the default config file 
 * path if the config file has not already been opened.
 * 	RETURN		config file entry
 */
confent_t *
getconfent(void)
{
    static char buf[BUFSIZ];
    static confent_t conf;
    confent_t *result = NULL;

    if (!fsys)
        setconfent(_PATH_QUOTA_CONF);

    while (fgets(buf, BUFSIZ, fsys)) {
        char *threshp, *p;

        if ((p = strchr(buf, '#'))) /* zap comment */
            *p = '\0';
        if (strlen(buf) > 0) {
            p = buf;
            conf.cf_label = next_field(&p, ':');
            conf.cf_rhost = next_field(&p, ':');
            conf.cf_rpath = next_field(&p, ':');
            threshp = next_field(&p, ':');
            conf.cf_thresh = 0;
            if (threshp != NULL)
                conf.cf_thresh = atoi(threshp);

            result = &conf;
            break;
        }
    }

    return result;
}

confent_t *
getconflabelsub(char *dir)
{
    confent_t *e;
 
    setconfent(_PATH_QUOTA_CONF);   
    while ((e = getconfent()) != NULL) {
        if (match_path(dir, e->cf_label))
            break;
    }
    return e;
}

confent_t *
getconflabel(char *label)
{
    confent_t *e;
 
    setconfent(_PATH_QUOTA_CONF);   
    while ((e = getconfent()) != NULL) {
        if (!strcmp(e->cf_label, label))
            break;
    }
    return e;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */