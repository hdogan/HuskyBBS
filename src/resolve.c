/*
 * Husky BBS Server for Linux
 * Version 3.0 Copyright (C) 1997-1998-1999-2000-2001 by Hidayet Dogan
 *
 * E-mail: hdogan@bilcag.net or hidayet.dogan@pleksus.net.tr
 *
 * Refer to the file "LICENSE" included in this package for further
 * information and before using any of the following.
 *
 * + Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,
 *   Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.
 * + Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael
 *   Chastain, Michael Quan, and Mitchell Tse.
 * + ROM 2.4 is copyright 1993-1998 Russ Taylor
 * + SMAUG 1.4 (C) 1994, 1995, 1996, 1998 by Derek Snider
 *
 * Original Diku license in the file 'doc/diku.license',
 * Merc license in the file 'doc/merc.license',
 * ROM license in the file 'doc/rom.license' and
 * Smaug license in the file 'doc/smaug.license'
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "bbs.h"

#if !defined( STDOUT_FILENO )
#define STDOUT_FILENO 1
#endif

#define IS_NULL(str)	((str) == NULL || (str)[0] == '\0')
#define BBS_PORT	3000

bool read_from_resolve( int fd, char *buffer )
{
    static char inbuf[STRING];
    int iStart, i, j, k;

    iStart = strlen(inbuf);
    if (iStart >= sizeof(inbuf) - 10)
    {
	log_string("Resolve inbuf overflow!");
	return FALSE;
    }

    for (;;)
    {
	int nRead;

	nRead = read(fd, inbuf + iStart, sizeof(inbuf) - 10 - iStart);
	if (nRead > 0)
	{
	    iStart += nRead;
	    if (inbuf[iStart-2] == '\n' || inbuf[iStart-2] == '\r')
		break;
	}
	else if (nRead == 0)
	    return FALSE;
	else if (errno == EWOULDBLOCK)
	    break;
	else
	{
	    perror("Read_from_resolve");
	    return FALSE;
	}
    }

    inbuf[iStart] = '\0';

    for (i = 0; inbuf[i] != '\n' && inbuf[i] != '\r'; i++)
    {
	if (inbuf[i] == '\0')
	    return FALSE;
    }

    for (i = 0, k = 0; inbuf[i] != '\n' && inbuf[i] != '\r'; i++)
    {
	if (inbuf[i] == '\b' && k > 0)
	    --k;
	else if (isascii(inbuf[i]) && isprint(inbuf[i]))
	    buffer[k++] = inbuf[i];
    }

    if (k == 0)
	buffer[k++] = ' ';

    buffer[k] = '\0';

    while (inbuf[i] == '\n' || inbuf[i] == '\r')
	i++;

    for (j = 0; (inbuf[j] = inbuf[i+j]) != '\0'; j++)
	;

    return TRUE;
}

void process_resolve( DESC_DATA *d )
{
    char buffer[INPUT];
    char address[INPUT];
    char *user;
    int status;

    buffer[0] = '\0';

    if (!read_from_resolve(d->ifd, buffer) || IS_NULL(buffer))
	return;

    user = first_arg(buffer, address, FALSE);

    if (!IS_NULL(user))
    {
	if (d->ident) free_string(d->ident);
	d->ident = str_dup(user);
    }

    if (!IS_NULL(address))
    {
	if (d->host) free_string(d->host);
	d->host = str_dup(address);
	if (USR(d) && USR(d)->host_name)
	    free_string(USR(d)->host_name);
	USR(d)->host_name = str_dup(address);
	get_small_host(USR(d));
    }

    close(d->ifd);
    d->ifd = -1;
    waitpid(d->ipid, &status, WNOHANG);
    d->ipid = -1;

/*    resolve_kill(d); */
    return;
}

void resolve_kill( DESC_DATA *d )
{
    int status;

    if (d == NULL)
	return;

    if (d->ipid > -1)
    {
	kill(d->ipid, SIGKILL);
	waitpid(d->ipid, &status, WNOHANG);
	d->ipid = -1; /* ? */
    }

    if (d->ifd > -1)
    {
	close(d->ifd);
	d->ifd = -1; /* ? */
    }
}

void create_resolve( DESC_DATA *d, long ip, sh_int port )
{
    int fds[2];
    pid_t pid;

    if (pipe(fds) != 0)
    {
	perror("Create_resolve: pipe: ");
	return;
    }

    if (dup2(fds[1], STDOUT_FILENO) != STDOUT_FILENO)
    {
	perror("Create_resolve: dup2 (stdout): ");
	return;
    }

    close(fds[1]);

    if ((pid = fork()) > 0)
    {
	d->ifd = fds[0];
	d->ipid = pid;
    }
    else if (pid == 0)
    {
	char str_ip[64], str_local[64], str_remote[64];

	d->ifd = fds[0];
	d->ipid = pid;

	sprintf(str_local, "%d", BBS_PORT);
	sprintf(str_remote, "%d", port);
	sprintf(str_ip, "%ld", ip);
	execl("../lib/resolve", "resolve", str_local, str_ip, str_remote, 0);
	log_string("Exec failed; Closing child.");
	d->ifd = -1;
	d->ipid = -1;
	exit(0);
    }
    else
    {
	perror("Create_resolve: fork");
	close(fds[0]);
    }
}
