/*
 * Copyright 2017-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pwd.h>
#include <nss.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

extern "C" {

enum nss_status
_nss_cm4all_logname_setpwent();

enum nss_status
_nss_cm4all_logname_endpwent();

enum nss_status
_nss_cm4all_logname_getpwent_r(struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop);

enum nss_status
_nss_cm4all_logname_getpwnam_r(const char *name,
			       struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop);

enum nss_status
_nss_cm4all_logname_getpwuid_r(uid_t uid,
			       struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop);

}

static unsigned position = 0;

enum nss_status
_nss_cm4all_logname_setpwent()
{
	position = 0;
	return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_cm4all_logname_endpwent()
{
	return NSS_STATUS_SUCCESS;
}

static bool
append(char **dest, char **buffer, size_t *buflen,
       const char *value)
{
	const size_t len = strlen(value) + 1;
	if (len > *buflen)
		return false;

	*dest = *buffer;
	memcpy(*buffer, value, len);
	*buffer += len;
	*buflen -= len;

	return true;
}

static enum nss_status
logname_to_passwd(struct passwd *result,
		  char *buffer, size_t buflen,
		  int *errnop)
{
	const char *username = getenv("LOGNAME");
	const char *home = getenv("HOME");
	const char *shell = getenv("SHELL");
	if (shell == nullptr)
		shell = "/bin/sh";

	if (username == nullptr || home == nullptr) {
		*errnop = 0;
		return NSS_STATUS_NOTFOUND;
	}

	if (!append(&result->pw_name, &buffer, &buflen, username) ||
	    !append(&result->pw_passwd, &buffer, &buflen, "x") ||
	    !append(&result->pw_gecos, &buffer, &buflen, username) ||
	    !append(&result->pw_dir, &buffer, &buflen, home) ||
	    !append(&result->pw_shell, &buffer, &buflen, shell)) {
		*errnop = ERANGE;
		return NSS_STATUS_TRYAGAIN;
	}

	result->pw_uid = geteuid();
	result->pw_gid = getegid();

	*errnop = 0;
	return NSS_STATUS_SUCCESS;
}

enum nss_status
_nss_cm4all_logname_getpwent_r(struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop)
{
	++position;

	if (position == 1) {
		return logname_to_passwd(result, buffer, buflen, errnop);
	} else {
		*errnop = 0;
		return NSS_STATUS_NOTFOUND;
	}
}

enum nss_status
_nss_cm4all_logname_getpwnam_r(const char *name,
			       struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop)
{
	const char *username = getenv("LOGNAME");
	if (username != nullptr && strcmp(name, username) == 0) {
		return logname_to_passwd(result, buffer, buflen, errnop);
	} else {
		*errnop = 0;
		return NSS_STATUS_NOTFOUND;
	}
}

enum nss_status
_nss_cm4all_logname_getpwuid_r(uid_t uid,
			       struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop)
{
	if (uid == geteuid()) {
		return logname_to_passwd(result, buffer, buflen, errnop);
	} else {
		*errnop = 0;
		return NSS_STATUS_NOTFOUND;
	}
}
