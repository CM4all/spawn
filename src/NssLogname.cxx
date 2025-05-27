// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include <pwd.h>
#include <nss.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

extern "C" {

[[gnu::visibility("default")]]
enum nss_status
_nss_cm4all_logname_setpwent();

[[gnu::visibility("default")]]
enum nss_status
_nss_cm4all_logname_endpwent();

[[gnu::visibility("default")]]
enum nss_status
_nss_cm4all_logname_getpwent_r(struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop);

[[gnu::visibility("default")]]
enum nss_status
_nss_cm4all_logname_getpwnam_r(const char *name,
			       struct passwd *result,
			       char *buffer, size_t buflen,
			       int *errnop);

[[gnu::visibility("default")]]
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
