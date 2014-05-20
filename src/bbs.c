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

#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "telnet.h"

#include "bbs.h"

const	char	echo_off_str	[] = { IAC, WILL, TELOPT_ECHO, '\0' };
const	char	echo_on_str	[] = { IAC, WONT, TELOPT_ECHO, '\0' };
const	char	go_ahead_str	[] = { IAC, GA, '\0' };

DESC_DATA *	desc_list;
DESC_DATA *	d_next;
CONFIG		config;
FILE *		fpReserve;
bool		bbs_down;
time_t		current_time;
time_t		boot_time_t;
char		boot_time	[STRING_LENGTH];
int		control, port;

void	bbs_loop		( int control );
int	init_socket		( int port );
void	init_desc		( int control );
bool	read_from_desc		( DESC_DATA *d );
void	read_from_buffer	( DESC_DATA *d );
bool	write_to_desc		( int desc, char *txt, int length );
bool	process_output		( DESC_DATA *d, bool fPrompt );
void	login			( DESC_DATA *d, char *argument );
bool	legal_name		( char *name );
bool	check_login		( DESC_DATA *d, char *name );
bool	check_reconnect		( DESC_DATA *d, char *name, bool fConn );
char *	last_logon		( USER_DATA *usr );
char *	last_logoff		( USER_DATA *usr );
void	do_quit_org		( USER_DATA *usr, char *argument, bool fXing );

int main( int argc, char *argv[] )
{
    struct timeval now_time;
    struct rlimit rl;
    bool fCopyOver = FALSE;

    /*
     * Set max coredump size.
     */
    getrlimit( RLIMIT_CORE, &rl );
    rl.rlim_cur = rl.rlim_max;
    setrlimit( RLIMIT_CORE, &rl );

    gettimeofday(&now_time, NULL);
    current_time = (time_t) now_time.tv_sec;
    boot_time_t	 = current_time;
    strcpy(boot_time, ctime(&current_time));

    if ((fpReserve = fopen(NULL_FILE, "r")) == NULL)
    {
	perror(NULL_FILE);
	exit(1);
    }

    port = 3000;
    if (argc > 1)
    {
	if (!is_number(argv[1]))
	{
	    fprintf(stderr, "Usage: %s port\n", argv[0]);
	    exit(1);
	}
	else if ((port = atoi(argv[1])) <= 1024)
	{
	    fprintf(stderr, "Port number must be above 1024.\n");
	    exit(1);
	}

	if (argv[2] && argv[2][0])
	{
	    fCopyOver	= TRUE;
	    control	= atoi(argv[3]);
	}
	else
	    fCopyOver	= FALSE;
    }

    if (!fCopyOver)
	control = init_socket(port);

    boot_dbase( );
    sprintf(log_buf, "BBS is ready to rock on port %d.", port);
    log_string(log_buf);

    if (fCopyOver)
	copyover_recover( );

    bbs_loop(control);
    close(control);
    log_string("Normal termination of bbs.");
    exit(0);
    return 0;
}

int init_socket( int port )
{
    static struct sockaddr_in sa_zero;
    struct sockaddr_in sa;
    int x = 1;
    int fd;

    if ((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
	perror("Init_socket: socket");
	exit(1);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *) &x,
	sizeof(x)) < 0)
    {
	perror("Init_socket: SO_REUSEADDR");
	close(fd);
	exit(1);
    }

#if defined(SO_DONTLINGER) && !defined(SYSV)
    {
	struct linger ld;

	ld.l_onoff	= 1;
	ld.l_linger	= 1000;

	if (setsockopt(fd, SOL_SOCKET, SO_DONTLINGER, (void *) &ld,
	    sizeof(ld)) < 0)
	{
	    perror("Init_socket: SO_DONTLINGER");
	    close(fd);
	    exit(1);
	}
    }
#endif

    sa			= sa_zero;
    sa.sin_family	= AF_INET;
    sa.sin_port		= htons(port);

    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0)
    {
	perror("Init_socket: bind");
	close(fd);
	exit(1);
    }

    if (listen(fd, 3) < 0)
    {
	perror("Init_socket: listen");
	close(fd);
	exit(1);
    }

    return fd;
}

void bbs_loop( int control )
{
    static struct timeval null_time;
    struct timeval last_time;

    signal(SIGPIPE, SIG_IGN);
    gettimeofday(&last_time, NULL);
    current_time = (time_t) last_time.tv_sec;
    
    while (!bbs_down)
    {
	fd_set in_set;
	fd_set out_set;
	fd_set exc_set;
	DESC_DATA *d;
	int maxdesc;

	FD_ZERO(&in_set);
	FD_ZERO(&out_set);
	FD_ZERO(&exc_set);
	FD_SET(control, &in_set);
	maxdesc = control;

	for (d = desc_list; d; d = d->next)
	{
	    maxdesc = UMAX((unsigned) maxdesc, d->descr);
	    FD_SET(d->descr, &in_set);
	    FD_SET(d->descr, &out_set);
	    FD_SET(d->descr, &exc_set);

	    if (d->ifd != -1 && d->ipid != -1)
	    {
		maxdesc = UMAX((unsigned) maxdesc, d->ifd);
		FD_SET(d->ifd, &in_set);
	    }
	}

	if (select(maxdesc + 1, &in_set, &out_set, &exc_set, &null_time) < 0)
	{
	    perror("Bbs_loop: select: poll");
	    exit(1);
	}

	if (FD_ISSET(control, &in_set))
	    init_desc(control);

	for (d = desc_list; d != NULL; d = d_next)
	{
	    d_next = d->next;

	    if (FD_ISSET(d->descr, &exc_set))
	    {
		FD_CLR(d->descr, &in_set);
		FD_CLR(d->descr, &out_set);
		if (USR(d) && USR(d)->level > 0)
		    save_user(USR(d));
		d->outtop = 0;
		close_socket(d);
	    }
	}

	for (d = desc_list; d != NULL; d = d_next)
	{
	    d_next = d->next;
	    d->fcommand = FALSE;

	    if (FD_ISSET(d->descr, &in_set))
	    {
		if (!read_from_desc(d))
		{
		    FD_CLR(d->descr, &out_set);
		    if (USR(d) && USR(d)->level > 0)
			save_user(USR(d));
		    d->outtop = 0;
		    close_socket(d);
		    continue;
	 	}
	    }

	    if ((d->login == CON_LOGIN || USR(d)) && d->ifd != -1
	    && FD_ISSET(d->ifd, &in_set))
		process_resolve(d);

	    read_from_buffer(d);
	    if (d->incomm[0] != '\0')
	    {
		d->fcommand = TRUE;

		if (d->showstr_point)
		{
		    if (USR(d) && !str_cmp(USR(d)->lastCommand, "mx"))
			show_string(d, d->incomm, TRUE);
		    else
			show_string(d, d->incomm, FALSE);
		}
		else if (d->pString)
		    string_add(USR(d), d->incomm);
		else if (d->login == CON_LOGIN)
		{
		    switch (EDIT_MODE(USR(d)))
		    {
			case EDITOR_NOTE_SUBJECT:
			    edit_note_subject(USR(d), d->incomm);
			    break;

			case EDITOR_MAIL_SUBJECT:
			    edit_mail_subject(USR(d), d->incomm);
			    break;

			case EDITOR_MAIL:
			    edit_mail_mode(USR(d), d->incomm);
			    break;

			case EDITOR_XING:
			    edit_xing(USR(d), d->incomm);
			    break;

			case EDITOR_XING_RECEIPT:
			    edit_xing_receipt(USR(d), d->incomm);
			    break;

			case EDITOR_XING_REPLY:
			    edit_xing_reply(USR(d), d->incomm);
			    break;

			case EDITOR_FEELING:
			    edit_feeling(USR(d), d->incomm);
			    break;

			case EDITOR_FEELING_RECEIPT:
			    edit_feeling_receipt(USR(d), d->incomm);
			    break;

			case EDITOR_PLAN_ANSWER:
			    edit_plan_answer(USR(d), d->incomm);
			    break;

			case EDITOR_INFO_ANSWER:
			    edit_info_answer(USR(d), d->incomm);
			    break;

			case EDITOR_QUIT_ANSWER:
			    edit_quit_answer(USR(d), d->incomm);
			    break;

			case EDITOR_PWD_OLD:
			    edit_pwd_old(USR(d), d->incomm);
			    break;

			case EDITOR_PWD_NEW_ONE:
			    edit_pwd_new_one(USR(d), d->incomm);
			    break;

			case EDITOR_PWD_NEW_TWO:
			    edit_pwd_new_two(USR(d), d->incomm);
			    break;

			case EDITOR_NONE:
			    if (USR(d)->pBoard ==
				board_lookup("chat", FALSE))
				process_chat_command(USR(d), d->incomm);
			    else
				substitute_alias(d, d->incomm);
			    break;

			default:
			    close_socket(d);
			    break;
		    }
		}
		else
		    login(d, d->incomm);
		d->incomm[0] = '\0';
	    }
	}

	update_bbs();

	for (d = desc_list; d != NULL; d = d_next)
	{
	    d_next = d->next;

	    if ((d->fcommand || d->outtop > 0)
	    && FD_ISSET(d->descr, &out_set))
	    {
		if (!process_output(d, TRUE))
		{
		    if (USR(d) && USR(d)->level > 0)
			save_user(USR(d));
		    d->outtop = 0;
		    close_socket(d);
	  	}
	    }
	}

	{
	    struct timeval now_time;
	    long secDelta;
	    long usecDelta;

	    gettimeofday(&now_time, NULL);
	    usecDelta = ((int) last_time.tv_usec) - ((int) now_time.tv_usec)
		+ 1000000 / 40;
	    secDelta = ((int) last_time.tv_sec) - ((int) now_time.tv_sec);
	    while (usecDelta < 0)
	    {
		usecDelta += 1000000;
		secDelta -= 1;
	    }

	    while (usecDelta >= 1000000)
	    {
		usecDelta -= 1000000;
		secDelta += 1;
	    }

	    if (secDelta > 0 || (secDelta == 0 && usecDelta > 0))
	    {
		struct timeval stall_time;

		stall_time.tv_usec = usecDelta;
		stall_time.tv_sec = secDelta;
		if (select(0, NULL, NULL, NULL, &stall_time) < 0)
		{
		    perror("Bbs_loop: select: stall");
		    exit(1);
		}
	    }
	}

	gettimeofday(&last_time, NULL);
	current_time = (time_t) last_time.tv_sec;
    }

    return;
}

void init_desc( int control )
{
    char buf[STRING_LENGTH];
    struct sockaddr_in sock;
    DESC_DATA *dnew;
    int desc, size;

    size = sizeof(sock);
    getsockname(control, (struct sockaddr *) &sock, &size);
    if ((desc = accept(control, (struct sockaddr *) &sock, &size)) < 0)
    {
	perror("Init_desc: accept");
	return;
    }

#if !defined(FNDELAY)
#define FNDELAY O_NDELAY
#endif

    if (fcntl(desc, F_SETFL, FNDELAY) == -1)
    {
	perror("Init_desc: fcntl: FNDELAY");
	return;
    }

    dnew		= new_desc();
    dnew->descr		= desc;

    size = sizeof(sock);
    if (getpeername(desc, (struct sockaddr *) &sock, &size) < 0)
    {
	perror("Init_desc: getpeername");
	dnew->host = str_dup("<unknown>");
    }
    else
    {
	int addr;

	if (!IS_SET(config.bbs_flags, BBS_NORESOLVE))
	    create_resolve(dnew, sock.sin_addr.s_addr, ntohs(sock.sin_port));
	addr = ntohl(sock.sin_addr.s_addr);
	sprintf(buf, "%d.%d.%d.%d",
	    (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
	    (addr >>  8) & 0xFF, (addr      ) & 0xFF);
	sprintf(log_buf, "Sock.sinaddr: %s", buf);
	log_string(log_buf);
	dnew->host = str_dup(buf);
    }

    dnew->next	  = desc_list;
    desc_list	  = dnew;

    {
	extern char *greeting1;
	extern char *greeting2;
/*	extern char *greeting3; unused variable BAXTER */
	extern char *greeting4;
	extern char *greeting5;
	extern char *greeting6;

	/*
	 * Random greeting screen.
	 */
	switch (number_range(1, 6))
	{
	    case 1:
		if (greeting1[0] == '.')
		    write_to_buffer(dnew, greeting1+1, 0);
		else
		    write_to_buffer(dnew, greeting1, 0);
		break;
	    case 2:
		if (greeting2[0] == '.')
		    write_to_buffer(dnew, greeting2+1, 0);
		else
		    write_to_buffer(dnew, greeting2, 0);
		break;
	    case 3:
		if (greeting1[0] == '.')
		    write_to_buffer(dnew, greeting1+1, 0);
		else
		    write_to_buffer(dnew, greeting1, 0);
		break;
	    case 4:
		if (greeting4[0] == '.')
		    write_to_buffer(dnew, greeting4+1, 0);
		else
		    write_to_buffer(dnew, greeting4, 0);
		break;
	    case 5:
		if (greeting5[0] == '.')
		    write_to_buffer(dnew, greeting5+1, 0);
		else
		    write_to_buffer(dnew, greeting5, 0);
		break;
	    case 6:
		if (greeting6[0] == '.')
		    write_to_buffer(dnew, greeting6+1, 0);
		else
		    write_to_buffer(dnew, greeting6, 0);
		break;
	    default:
		if (greeting1[0] == '.')
		    write_to_buffer(dnew, greeting1+1, 0);
		else
		    write_to_buffer(dnew, greeting1, 0);
		break;
	}
    }

    write_to_buffer(dnew, "Name: ", 0);
    return;
}

void close_socket( DESC_DATA *dclose )
{
    USER_DATA *usr;
    bool fQuit = FALSE;

    if (dclose->ipid > -1)
    {
	int status;

	kill(dclose->ipid, SIGKILL);
	waitpid(dclose->ipid, &status, WNOHANG);
    }

    if (dclose->ifd > -1)
	close(dclose->ifd);

    if (dclose->outtop > 0)
	process_output(dclose, FALSE);

    if ((usr = USR(dclose)) != NULL)
    {
	sprintf(log_buf, "Closing link to %s.", usr->name);
	log_string(log_buf);
	sprintf(log_buf, "Net death has claimed %s", usr->name);
	syslog(log_buf, usr);
	if (dclose->login == CON_LOGIN && !bbs_down)
	{
	    do_quit_org(usr, "", FALSE);
	    fQuit = TRUE;
	    usr->desc = NULL;
	}
	else
	{
	    free_user(usr);
	}
    }

    if (d_next == dclose)
	d_next = d_next->next;

    if (dclose == desc_list)
    {
	desc_list = desc_list->next;
    }
    else
    {
	DESC_DATA *d;

	for (d = desc_list; d && d->next != dclose; d = d->next)
	    ;
	if (d != NULL)
	    d->next = dclose->next;
	else
	{
	    if (!fQuit)
		bug("Close_socket: dclose not found.", 0);
	}
    }

    close(dclose->descr);
    free_desc(dclose);
    return;
}

bool read_from_desc( DESC_DATA *d )
{
    int iStart;

    if (d->incomm[0] != '\0')
	return TRUE;

    iStart = strlen(d->inbuf);
    if (iStart >= sizeof(d->inbuf) - 10)
    {
	sprintf(log_buf, "%s input overflow!", d->host);
	log_string(log_buf);
	write_to_desc(d->descr, "\n\r*** FLOODING!!! ***\n\r", 0);
	return FALSE;
    }

    for (;;)
    {
	int nRead;

	nRead = recv(d->descr, d->inbuf + iStart, sizeof(d->inbuf) - 10
	    - iStart, 0);
	if (nRead > 0)
	{
	    iStart += nRead;
	    if (d->inbuf[iStart-1] == '\n' || d->inbuf[iStart-1] == '\r')
		break;
	}
	else if (nRead == 0)
	{
	    log_string("EOF encountered on read.");
	    return FALSE;
	}
	else if (errno == EWOULDBLOCK || errno == EAGAIN)
	    break;
	else
	{
	    perror("Read_from_desc");
	    return FALSE;
	}
    }

    d->inbuf[iStart] = '\0';
    return TRUE;
}

void read_from_buffer( DESC_DATA *d )
{
    int i, j, k;
    bool got_n, got_r;

    if (d->incomm[0] != '\0')
	return;

    for (i = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++)
    {
	if (d->inbuf[i] == '\0')
	    return;
    }

    for (i = 0, k = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++)
    {
	if (k >= INPUT_LENGTH - 2)
	{
	    write_to_desc(d->descr, "Line too long.\n\r", 0);
	    for (; d->inbuf[i] != '\0'; i++)
	    {
		if (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
		    break;
	    }

	    d->inbuf[i]   = '\n';
	    d->inbuf[i+1] = '\0';
	    break;
	}

	if (d->inbuf[i] == '\b' && k > 0)
	    --k;
	else if (isascii(d->inbuf[i]) && isprint(d->inbuf[i]))
	    d->incomm[k++] = d->inbuf[i];
    }

    if (k == 0)
	d->incomm[k++] = ' ';

    d->incomm[k] = '\0';
    got_n = got_r = FALSE;

    for (; d->inbuf[i] == '\r' || d->inbuf[i] == '\n'; i++)
    {
	if (d->inbuf[i] == '\r' && got_r++)
	    break;
	else if (d->inbuf[i] == '\n' && got_n++)
	    break;
    }

    for (j = 0; (d->inbuf[j] = d->inbuf[i+j]) != '\0'; j++)
	;

    return;
}

bool process_output( DESC_DATA *d, bool fPrompt )
{
    char buf[STRING_LENGTH];
    extern bool bbs_down;
    char *ptr;
    int	shown_lines = 0;
    int	total_lines = 0;
    USER_DATA *usr;
    DESC_DATA *snoop;

    if (!bbs_down)
    {
	if (d->showstr_point)
	{
	    for (ptr = d->showstr_head; ptr != d->showstr_point; ptr++)
		if (*ptr == '\n')
		    shown_lines++;

	    total_lines = shown_lines;
	    for (ptr = d->showstr_point; *ptr != '\0'; ptr++)
		if (*ptr == '\n')
		    total_lines++;

	    if (USR(d) && IS_TOGGLE(USR(d), TOGGLE_ANSI))
	    {
		if (!str_cmp(USR(d)->lastCommand, "mx"))
		    print_to_user(USR(d), "More (%d%%) [<cr>,p,r,q,?] ",
			100 * shown_lines / total_lines);
		else
		    print_to_user(USR(d),
			"\033[0;0;30;47m--- More --- (%d%%)\033[0;0;37;40m "
			"('h' for help) ", 100 * shown_lines / total_lines);
	    }
	    else
	    {
		if (!str_cmp(USR(d)->lastCommand, "mx") && USR(d))
		    print_to_user(USR(d), "More (%d%%) [<cr>,p,r,q,?] ",
			100 * shown_lines / total_lines);
		else
		{
		    sprintf(buf, "--- More --- (%d%%) ('h' for help) ",
			100 * shown_lines / total_lines);
		    write_to_buffer(d, buf, 0);
		}
	    }
	}
	else if (d->pString && USR(d))
	{
	    if (EDIT_MODE(USR(d)) == EDITOR_PLAN)
		print_to_user(USR(d), "%d] ", EDIT_LINE(USR(d)));
	    else
		print_to_user(USR(d), "%2d] ", EDIT_LINE(USR(d)));
	}
	else if (fPrompt && USR(d) && d->login == CON_LOGIN)
	{
	    usr = USR(d);

	    switch (EDIT_MODE(usr))
	    {
		case EDITOR_NOTE_SUBJECT:
		case EDITOR_MAIL_SUBJECT:
		    write_to_buffer(d, "Subject: ", 0);
		    break;

		case EDITOR_PWD_OLD:
		    write_to_buffer(d, "\n\rOld password: ", 0);
		    break;

		case EDITOR_PWD_NEW_ONE:
		    write_to_buffer(d, "\n\rNew password: ", 0);
		    break;

		case EDITOR_PWD_NEW_TWO:
		    write_to_buffer(d, "\n\rRetype password: ", 0);
		    break;

		case EDITOR_MAIL:
		    write_to_buffer(d, "Mail> ", 0);
		    break;

		case EDITOR_QUIT_ANSWER:
		    write_to_buffer(d, "Do you want to quit now (y/N)? ", 0);
		    break;

		case EDITOR_PLAN_ANSWER:
		case EDITOR_INFO_ANSWER:
		    write_to_buffer(d, "Do you want to change this (y/N)? ", 0);
		    break;

		case EDITOR_XING:
		    sprintf(buf, "%d] ", EDIT_LINE(usr));
		    write_to_buffer(d, buf, 0);
		    break;

		case EDITOR_XING_RECEIPT:
		case EDITOR_FEELING_RECEIPT:
		    if (get_user(usr->last_xing) == NULL)
			sprintf(buf, "Enter recipients name: ");
		    else if (str_cmp(usr->last_xing, "*NONE*"))
			sprintf(buf, "Enter recipients name [ %s ]: ",
			    usr->last_xing);
		    else
			sprintf(buf, "Enter recipients name: ");
		    write_to_buffer(d, buf, 0);
		    break;

		case EDITOR_FEELING:
		    write_to_buffer(d, "You choice: ", 0);
		    break;

		case EDITOR_XING_REPLY:
		    write_to_buffer(d, "Enter recipients name: ", 0);
		    break;

		case EDITOR_NONE:
		    if (buf_string(usr->pBuffer)[0] != '\0')
		    {
			page_to_user(buf_string(usr->pBuffer), usr);
			clear_buf(usr->pBuffer);
		    }

		    if (unread_notes(usr, usr->pBoard) > 0)
			sprintf(buf, "#x[#Y%s #x(#R%d #Ynew#x)] ",
			    usr->pBoard->long_name,
			    unread_notes(usr, usr->pBoard));
		    else
			sprintf(buf, "#x[#Y%s#x] ",
			    usr->pBoard->long_name);
		    send_to_user(buf, usr);
		    break;
	    }
	}
    }

    if (d->outtop == 0)
	return TRUE;

    for (snoop = desc_list; snoop != NULL; snoop = snoop->next)
    {
	if (USR(snoop) && USR(d) && !str_cmp(USR(snoop)->snooped, USR(d)->name))
	{
	    if (isBusySelf(USR(snoop)))
	    {
		add_buffer(USR(snoop), "\n\r#W>#x Snooped: ");
		add_buffer(USR(snoop), USR(d)->name);
		add_buffer(USR(snoop), "\n\r#W>#x Begin\n\r");
		add_buffer(USR(snoop), d->outbuf);
		add_buffer(USR(snoop), "\n\r#W>#x End\n\r\n\r");
	    }
	    else
	    {
		send_to_user("\n\r#W>#x Snooped: ", USR(snoop));
		send_to_user(USR(d)->name, USR(snoop));
		send_to_user("\n\r#W>#x Begin\n\r", USR(snoop));
		send_to_user(d->outbuf, USR(snoop));
		send_to_user("\n\r#W>#x End\n\r\n\r", USR(snoop));
	    }
	}
    }

    if (!write_to_desc(d->descr, d->outbuf, d->outtop))
    {
	d->outtop = 0;
	return FALSE;
    }
    else
    {
	d->outtop = 0;
	return TRUE;
    }
}

void write_to_buffer( DESC_DATA *d, const char *txt, int length )
{
    if (!d)
    {
	bbs_bug("Write_to_buffer: NULL descriptor");
	return;
    }

    if (!d->outbuf)
	return;

    if (length <= 0)
	length = strlen(txt);

    if (d->outtop == 0 && !d->fcommand)
    {
	d->outbuf[0]	= '\n';
	d->outbuf[1]	= '\r';
	d->outtop	= 2;
    }

    while (d->outtop + length >= d->outsize)
    {
	char *outbuf;

	if (d->outsize >= 32000)
	{
	    d->outtop = 0;
	    bug("Buffer overflow. Closing.\n\r", 0);
	    write_to_desc(d->descr, "\n\r*** Buffer Overflow! ***\n\r", 0);
	    close_socket(d);
	    return;
	}

	outbuf = alloc_mem(2 * d->outsize);
	strncpy(outbuf, d->outbuf, d->outtop);
	free_mem(d->outbuf, d->outsize);
	d->outbuf = outbuf;
	d->outsize *= 2;
    }

    strncpy(d->outbuf + d->outtop, txt, length);
    d->outtop += length;
    d->outbuf[d->outtop] = '\0';
    return;
}

bool write_to_desc( int desc, char *txt, int length )
{
    int iStart, nWrite, nBlock;

    if (length <= 0)
	length = strlen(txt);

    for (iStart = 0; iStart < length; iStart += nWrite)
    {
	nBlock = UMIN(length - iStart, 4096);
	if ((nWrite = send(desc, txt + iStart, nBlock, 0)) < 0)
	{
	    perror("Write_to_desc: write");
	    return FALSE;
	}
    }

    return TRUE;
}

void login( DESC_DATA *d, char *argument )
{
    char buf[STRING];
    DESC_DATA *d_old, *d_next;
    USER_DATA *usr;
    char *password_new;
    char *password, *host;
    bool fOld;

    while (isspace(*argument))
	argument++;

    usr = USR(d);

    switch (d->login)
    {
	default:
	    bug("Login: bad d->login %d.", d->login);
	    close_socket(d);
	    return;

	case CON_BREAK_CONNECT:
	     switch (*argument)
	     {
		default:
		    for (d_old = desc_list; d_old != NULL; d_old = d_next)
		    {
			d_next = d_old->next;

			if (d_old != d && USR(d_old)
			&& d_old->login != CON_GET_NAME
			&& d_old->login != CON_GET_OLD_PASSWORD
			&& !str_cmp(usr->name, USR(d_old)->name))
			{
			    if (d_old->login == CON_LOGIN)
			    {
				sprintf(buf, "You have been replaced by "
					     "someone from: %s.\n\r",
				    d->host);
				write_to_buffer(d_old, buf, 0);
				free_user(USR(d));
				USR(d)		   = USR(d_old);
				USR(d_old)->desc   = NULL;
				USR(d)->desc	   = d;
				USR(d_old)	   = NULL;
				write_to_buffer(d,
				"Kicking off old link.\n\r", 0);
				close_socket(d_old);
				d->login	   = CON_LOGIN;
				if (USR(d)->host_name)
				    free_string(USR(d)->host_name);
				USR(d)->host_name   = str_dup(d->host);
				get_small_host(USR(d));
				USR(d)->last_logon  = current_time;
				USR(d)->logon	    = current_time;
				info_notify(USR(d), TRUE);
				save_user(USR(d));
				return;
			    }
			    write_to_buffer(d,
				"Reconnect attempt failed.\n\rName: ", 0);
			    if (USR(d))
			    {
				free_user(USR(d));
				USR(d) = NULL;
			    }
			    d->login = CON_GET_NAME;
			    return;
			}
		    }

		    write_to_buffer(d, "Reconnect attempt failed.\n\rName: ", 0);
		    if (USR(d))
		    {
			free_user(USR(d));
			USR(d) = NULL;
		    }
		    d->login = CON_GET_NAME;
		    break;

		case 'n': case 'N':
		    write_to_buffer(d, "Name: ", 0);
		    if (USR(d))
		    {
			free_user(USR(d));
			USR(d) = NULL;
		    }
		    d->login = CON_GET_NAME;
		    break;
	    }
	    break;

	case CON_GET_NAME:
	    if (argument[0] == '\0')
	    {
		close_socket(d);
		return;
	    }

	    argument[0] = UPPER(argument[0]);

	    if (!legal_name(argument))
	    {
		write_to_buffer(d, "Illegal name, try another name.\n\rName: ", 0);
		return;
	    }

	    if (!str_cmp(argument, "bye"))
	    {
		write_to_buffer(d, "Goodbye...\n\r", 0);
		close_socket(d);
		return;
	    }

	    if (!str_cmp(argument, "new"))
	    {
		if (IS_SET(config.bbs_flags, BBS_ADMLOCK))
		{
		    sprintf(buf, "%s BBS is running *Admin Only* mode. Try again later.\n\r", config.bbs_name);
		    write_to_buffer(d, buf, 0);
		    close_socket(d);
		    return;
		}

		if (IS_SET(config.bbs_flags, BBS_NEWLOCK))
		{
		    sprintf(buf, "%s BBS is not allowing new users temporarily.\n\r", config.bbs_name);
		    write_to_buffer(d, buf, 0);
		    close_socket(d);
		    return;
		}

		if (d->newbie == 0)
		{
		    write_to_buffer(d,
			"\n\r\n\rPlease choose a name for this system.  Please choose carefully, you cannot\n\r"
			"change your name once your account is created, the only way to get a different\n\r"
			"name is to have your old one deleted and create a new one from scratch.  This\n\r"
			"use to login with, and doesn't have to be your real name.  In fact, it is very\n\r"
			"rare that someone use their real name as a login name.  We will need your real\n\r"
			"name later for our records, however.\n\r\n\r"
			"Something important to know:  If you are female, you may not want to choose a\n\r"
			"name for yourself that makes this fact obvious.  The anonymity of this BBS, and\n\r"
			"computers in general, seems to bring out the worst in some people, and it is an\n\r"
			"unfortunate fact that some people believe this anonymity gives them free reign\n\r"
			"to harass others.  The harassment is often directed at those with female\n\r"
			"sounding names, choosing a gender neutral name for yourself can help quite a\n\r"
			"bit in avoiding this.\n\r\n\r", 0);
		    write_to_buffer(d, "Choose a new name (maximum 12 characters): ", 0);
		    d->newbie++;
		    d->login = CON_GET_NAME;
		    return;
		}
		else
		{
		    write_to_buffer(d, "Illegal name, try another.\n\rName: ", 0);
		    return;
		}
	    }
	    
	    fOld = load_user(d, argument);
	    if (!USR(d))
	    {
		sprintf(log_buf, "Bad user file %s@%s.", argument, d->host);
		log_string(log_buf);
		write_to_buffer(d, "ERROR: Your user file is corrupt.\n\r", 0);
		close_socket(d);
		return;
	    }

	    usr = USR(d);

	    if (check_reconnect(d, argument, FALSE))
	    {
		fOld = TRUE;
	    }

	    if (fOld)
	    {
		if (d->newbie != 0)
		{
		    write_to_buffer(d, "That name is already taken. Choose another.\n\rName: ", 0);
		    d->login = CON_GET_NAME;
		    return;
		}

		if (IS_SET(config.bbs_flags, BBS_ADMLOCK) && !IS_ADMIN(usr))
		{
		    sprintf(buf, "%s BBS is running *Admin Only* mode. Try again later.\n\r", config.bbs_name);
		    write_to_buffer(d, buf, 0);
		    close_socket(d);
		    return;
		}

		if (usr && (check_banishes(usr, BANISH_USER)
		||  check_banishes(usr, BANISH_SITE)))
		{
		    close_socket(d);
		    return;
		}

		write_to_buffer(d, "Password: ", 0);
		write_to_buffer(d, echo_off_str, 0);
		d->login = CON_GET_OLD_PASSWORD;
		return;
	    }
	    else
	    {
		if (d->newbie == 0)
		{
		    write_to_buffer(d, "No such user exists.\n\rCheck your spelling, or type 'NEW' to start a new user.\n\rName: ", 0);
		    d->login = CON_GET_NAME;
		    return;
		}

/* Iki tane ayni isimde newbie icin
		if (desc_list)
		{
		    for (d = desc_list; d != NULL; d = d_next) {
			d_next = d->next;
			if (d->login != CON_LOGIN && USR(d) && USR(d)->name
			&& USR(d)->name[0] && !str_cmp(USR(d)->name, argument))
			{
			    write_to_buffer(d, "That name is already taken. Choose another.\n\rName: ", 0);
			    d->login = CON_GET_NAME;
			    return;
			}
		    }
		} BAXTER */

		write_to_buffer(d,
		    "\n\r\n\rNow choose a password for this system.  It is a good idea to choose a password\n\r"
		    "that another person, even one who knows you very well, would be unable to\n\r"
		    "guess.  Try something with punctuation, or a nonsense word such as glumph. It\n\r"
		    "is in your best interest to choose a password that no one else will guess,\n\r"
		    "because you are held responsible for the conduct of your account and your\n\r"
		    "privilege to use this BBS will be suspended if your account is used to take\n\r"
		    "actions that violate BBS rules.  Please do make sure you choose a password you\n\r"
		    "can remember, however, as getting it reset isn't an easy process.\n\r\n\r", 0);
		sprintf(buf, "Choose a new password: %s", echo_off_str);
		write_to_buffer(d, buf, 0);
		d->login = CON_GET_NEW_PASSWORD;
		return;
	    }
	    break;
		
	case CON_GET_VALIDATE:
	    if (atoi(argument) != usr->key)
		write_to_buffer(d, "That's not a valid key.\n\r", 0);
	    else
	    {
		usr->Validated = TRUE;
		write_to_buffer(d, "Ok, you have been validated.\n\r", 0);
	    }
	    usr->next = user_list;
	    user_list = usr;
	    d->login = CON_LOGIN;
	    do_help(usr, "motd");
	    sprintf(buf, "Last login %s until %s from %s.#x\n\r\n\r"
		"You are in the lobby now.\n\r",
		last_logon(usr), last_logoff(usr), usr->host_name);
	    send_to_user(buf, usr);
	    do_unread_mail(usr);
	    if (usr->host_name)
		free_string(usr->host_name);
	    usr->host_name	= str_dup(d->host);
	    get_small_host(usr);
	    usr->last_logon	= current_time;
	    usr->logon		= current_time;
	    usr->total_login++;
	    do_who(usr, "-f");
	    do_who(usr, "-n");
	    info_notify(usr, TRUE);
	    save_user(usr);
	    break;

	case CON_GET_OLD_PASSWORD:
	    write_to_buffer(d, "\n\r", 2);
	    if (strcmp(crypt(argument, usr->password), usr->password))
	    {
		write_to_buffer(d, "Wrong password.\n\r", 0);
		if (usr->bad_pwd == 2)
		{
		    close_socket(d);
		    return;
		}
		usr->bad_pwd++;
		write_to_buffer(d, "Password: ", 0);
		d->login = CON_GET_OLD_PASSWORD;
		return;
	    }

	    write_to_buffer(d, echo_on_str, 0);

	    if (check_login(d, usr->name))
		return;

	    if (check_reconnect(d, usr->name, TRUE))
		return;

	    if (usr->password[0] == '\0')
	    {
		write_to_buffer(d, "Warning! Null password!\n\r", 0);
		write_to_buffer(d, "Type 'password' to fix.\n\r", 0);
	    }

	    if (!usr->Validated)
	    {
		write_to_buffer(d, "Enter your validation key: ", 0);
		d->login = CON_GET_VALIDATE;
		return;
	    }
	    usr->next = user_list;
	    user_list = usr;
	    d->login = CON_LOGIN;
	    do_help(usr, "motd");
	    sprintf(log_buf, "%s@%s has connected", usr->name, d->host);
	    log_string(log_buf);
	    syslog(log_buf, usr);
	    sprintf(buf, "Last login %s until %s from %s.#x\n\r\n\r"
		"You are in the lobby now.\n\r",
		last_logon(usr), last_logoff(usr), usr->host_name);
	    send_to_user(buf, usr);
	    do_unread_mail(usr);
	    if (usr->host_name)
		free_string(usr->host_name);
	    usr->host_name	= str_dup(d->host);
	    get_small_host(usr);
	    usr->last_logon	= current_time;
	    usr->logon		= current_time;
	    usr->total_login++;
	    do_who(usr, "-f");
	    do_who(usr, "-n");
	    info_notify(usr, TRUE);
	    fix_moderator_name(usr);
	    save_user(usr);
	    break;

	case CON_GET_NEW_PASSWORD:
	    write_to_buffer(d, "\n\r", 2);
	    if (strlen(argument) < 5)
	    {
		write_to_buffer(d, "Password must be at least 5 characters long.\n\rPassword: ", 0);
		return;
	    }

	    if (strlen(argument) > 12)
	    {
		write_to_buffer(d, "Password must be maximum 12 characters.\n\r", 0);
		return;
	    }

	    password_new = crypt(argument, usr->name);
	    for (password = password_new; *password != '\0'; password++)
	    {
		if (*password == '~')
		{
		    write_to_buffer(d, "New password not acceptable, try again.\n\rPassword: ", 0);
		    return;
		}
	    }

	    if (usr->password)
		free_string(usr->password);
	    usr->password = str_dup(password_new);
	    write_to_buffer(d, "Retype password: ", 0);
	    d->login = CON_CONFIRM_PASSWORD;
	    break;

	case CON_CONFIRM_PASSWORD:
	    write_to_buffer(d, "\n\r", 2);
	    if (strcmp(crypt(argument, usr->password), usr->password))
	    {
		write_to_buffer(d, "Passwords don't match.\n\rPassword: ", 0);
		d->login = CON_GET_NEW_PASSWORD;
		return;
	    }

	    write_to_buffer(d, echo_on_str, 0);
	    write_to_buffer(d, "\n\r\n\rPlease give your full name, both first and last.  Do not abbreviate your name!\n\r\n\rEnter your real name (first and last): ", 0);
	    d->login = CON_GET_REALNAME;
	    break;

	case CON_GET_REALNAME:
	    if (argument[0] == '\0')
	    {
		write_to_buffer(d, "Invalid real name.\n\rEnter your real name (first and last): ", 0);
		return;
	    }

	    smash_tilde(argument);
	    if (usr->real_name)
		free_string(usr->real_name);
	    usr->real_name = str_dup(argument);
	    sprintf(buf,
		"\n\r\n\rLastly we ask for your internet e-mail address.  ** You must have an e-mail\n\r"
		"address registered in your name! **\n\r"
		"A validation key will be mailed to you at the address you give; your access\n\r"
		"will be severely restricted until you enter this key!\n\r"
		"Your email address should be of the form: userid@host.  Do not give a\n\r"
		"shortened or abbreviated form of your email address, the full address is\n\r"
		"necessary for the validation key to reach you allowing you to gain full\n\r"
		"access to %s BBS.\n\r", config.bbs_name);
	    write_to_buffer(d, buf, 0);
	    write_to_buffer(d, "\n\rEnter your e-mail address (userid@host): ", 0);
	    d->login = CON_GET_EMAIL;
	    break;

	case CON_GET_EMAIL:
	    if (argument[0] == '\0')
	    {
		write_to_buffer(d, "Invalid e-mail address.\n\rEnter your e-mail address (userid@host): ", 0);
		return;
	    }
	    if (!(host = strchr(argument, '@')))
	    {
		write_to_buffer(d, "Address should be in the form userid@host.\n\r", 0);
		write_to_buffer(d, "Enter your e-mail address: ", 0);
		return;
	    }
	    host++;

	    if (!str_cmp(host, "hotmail.com") || !str_cmp(host, "mailcity.com")
	    || !str_cmp(host, "yahoo.com") || !str_cmp(host, "infoseek.com")
	    || !str_cmp(host, "iname.com") || !str_cmp(host, "writeme.com")
	    || !str_cmp(host, "usa.net") || !str_cmp(host, "artemis.efes.net")
	    || !str_cmp(host, "i.am") || !str_cmp(host, "rocketmail.com")
	    || !str_cmp(host, "postmaster.co.uk"))
	    {
		write_to_buffer(d, "This is not a valid e-mail address.\n\r"
				   "Enter your e-mail address: ", 0);
		return;
	    }

	    smash_tilde(argument);
	    if (usr->email)
		free_string(usr->email);
	    usr->email = str_dup(argument);
	    write_to_buffer(d, "\n\rOk, now we're got everything we need.  You can now review what you have\n\r", 0);
	    write_to_buffer(d, "provided and be given a chance to correct yourself if you need a mistake.\n\rYou entered:\n\r\n\r", 0);
	    sprintf(buf, "Name: %s\n\rE-mail: %s\n\r\n\r", usr->real_name, usr->email);
	    write_to_buffer(d, buf, 0);
	    write_to_buffer(d, "Does this look correct (Y/n)? ", 0);
	    d->login = CON_GET_CORRECT;
	    break;

	case CON_GET_CORRECT:
	    switch (*argument)
	    {
		case 'n': case 'N':
		    if (usr->real_name)
			free_string(usr->real_name);
		    usr->real_name	= str_dup("NONE");
		    if (usr->email)
			free_string(usr->email);
		    usr->email		= str_dup("NONE");
		    write_to_buffer(d, "\n\rEnter your real name (first and last): ", 0);
		    d->login = CON_GET_REALNAME;
		    break;
		default:
		    write_to_buffer(d, "\n\rAre you, male or female: ", 0);
		    d->login = CON_GET_GENDER;
		    break;
	    }
	    break;

	case CON_GET_GENDER:
	    switch (*argument)
	    {
		case 'm': case 'M': usr->gender = 0;	break;
		case 'f': case 'F': usr->gender = 1;	break;
		default:
		    write_to_buffer(d, "That's not a valid gender.\n\rAre you, male or female: ", 0);
		    return;
	    }
	    write_to_buffer(d, "\n\rEnter your homepage url, press ENTER if you don't have: ", 0);
	    d->login = CON_GET_HOMEPAGE;
	    break;

	case CON_GET_HOMEPAGE:
	    if (argument[0] == '\0')
	    {
		if (usr->home_url)
		    free_string(usr->home_url);
		usr->home_url = str_dup("NONE");
	    }
	    else
	    {
		smash_tilde(argument);
		if (usr->home_url)
		    free_string(usr->home_url);
		usr->home_url = str_dup(argument);
	    }
	    if (usr->host_name)
		free_string(usr->host_name);
	    usr->host_name	= str_dup(d->host);
	    get_small_host(usr);
	    usr->last_logon	= current_time;
	    usr->logon		= current_time;
	    usr->total_login++;
	    sprintf(buf,
		"\n\r\n\rYour validation key will now be generated and automatically e-mailed to you\n\r"
		"at the e-mail address you have provided.  Please note that each key is unique,\n\r"
		"if you lose this one and a new one must be generated this one will no longer\n\r"
		"work.\n\r\n\rKey created %s for '%s'.\n\r\n\r",
		last_logon(usr), usr->email);
	    write_to_buffer(d, buf, 0);
	    add_validate(usr);
	    sprintf(log_buf, "%s@%s new user", usr->name, d->host);
	    log_string(log_buf);
	    syslog(log_buf, usr);
	    do_help(usr, "motd");
	    write_to_buffer(d, "You are in the lobby now.\n\r", 0);
	    usr->next		= user_list;
	    user_list		= usr;
	    d->login		= CON_LOGIN;
	    usr->level		= 1;
	    usr->version	= 1;
	    SET_HIDE(usr, HIDE_REALNAME);
	    SET_HIDE(usr, HIDE_EMAIL);
	    SET_TOGGLE(usr, TOGGLE_XING);
	    SET_TOGGLE(usr, TOGGLE_BEEP);
	    SET_TOGGLE(usr, TOGGLE_INFO);
	    SET_TOGGLE(usr, TOGGLE_FEEL);
	    info_notify(usr, TRUE);
	    save_user(usr);
	    break;
    }
}

bool legal_name( char *name )
{
    if (is_name_full(name, "god Allah sik yarrak fuck fucker dick suck deneme ege"))
	return FALSE;

    if (is_name_full(name, "fuckyou pussy amcik bbs root admin sysop operator gehenna bilmuh imrahil"))
	return FALSE;

    if (is_name_full(name, "webadmin webmaster creator stop adm user moderator"))
	return FALSE;

    if (is_name_full(name, "dassak dasak yarak shit butt ass asshole bastard ghenna"))
	return FALSE;

    if (str_cmp(capitalize(name), "Baxter")
    && (!str_prefix("bax", name) || !str_suffix("baxter", name)))
	return FALSE;

    if (str_cmp(capitalize(name), "Neruda")
    && (!str_prefix("neru", name) || !str_suffix("neruda", name)))
	return FALSE;

    if (str_cmp(capitalize(name), "Mtb")
    && (!str_prefix("mtb", name) || !str_suffix("mtb", name)))
	return FALSE;

    if (str_cmp(capitalize(name), "Swoop")
    && (!str_prefix("swo", name) || !str_suffix("swoop", name)))
	return FALSE;

    if (strlen(name) < 2 || strlen(name) > 12)
	return FALSE;

    {
	char *pc;
	bool fl = TRUE;

	for (pc = name; *pc != '\0'; pc++)
	{
	    if (!isalpha(*pc))
		return FALSE;

	    if (LOWER(*pc) != 'i' && LOWER(*pc) != 'l')
		fl = FALSE;
	}
	if (fl)
	    return FALSE;
    }

    return TRUE;
}

bool check_reconnect( DESC_DATA *d, char *name, bool fConn )
{
    USER_DATA *usr;

    for (usr = user_list; usr != NULL; usr = usr->next)
    {
	if ((!fConn || usr->desc == NULL)
	&& !str_cmp(USR(d)->name, usr->name))
	{
	    if (fConn == FALSE)
	    {
		if (USR(d)->password)
		    free_string(USR(d)->password);
		USR(d)->password = str_dup(usr->password);
	    }
	    else
	    {
		free_user(USR(d));
		USR(d) = usr;
		usr->desc = d;
		send_to_user("Reconnecting.\n\r", usr);
		sprintf(log_buf, "%s@%s reconnected", usr->name, d->host);
		log_string(log_buf);
		syslog(log_buf, usr);
		d->login = CON_LOGIN;
	    }

	    return TRUE;
	}
    }

    return FALSE;
}

bool check_login( DESC_DATA *d, char *name )
{
    DESC_DATA *dold;

    for (dold = desc_list; dold; dold = dold->next)
    {
	if (dold != d && USR(dold)
	&& dold->login != CON_GET_NAME
	&& dold->login != CON_GET_OLD_PASSWORD
	&& !str_cmp(name, USR(dold)->name))
	{
	    write_to_buffer(d, "That user is already login.\n\r", 0);
	    write_to_buffer(d, "Do you wish to connect anyway (Y/n)? ", 0);
	    d->login = CON_BREAK_CONNECT;
	    return TRUE;
	}
    }

    return FALSE;
}

void syntax( const char *txt, USER_DATA *usr )
{
    char buf[STRING_LENGTH];

    sprintf(buf, "Usage: %s\n\r", txt);
    send_to_user(buf, usr);
    return;
}

void print_to_user( USER_DATA *usr, char *format, ... )
{
    char buf[STRING];
    va_list args;

    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    send_to_user(buf, usr);
    return;
}

void print_to_user_bw( USER_DATA *usr, char *format, ... )
{
    char buf[STRING];
    va_list args;

    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    send_to_user_bw(buf, usr);
    return;
}

void send_to_user_bw( const char *txt, USER_DATA *usr )
{
    if (txt && usr->desc)
	write_to_buffer(usr->desc, txt, strlen(txt));
    return;
}

void send_to_user( const char *txt, USER_DATA *usr )
{
    const char *point;
    char *point2;
    char buf[4 * STRING_LENGTH];
    int skip = 0;

    buf[0] = '\0';
    point2 = buf;

    if (txt && usr->desc)
    {
	if (IS_TOGGLE(usr, TOGGLE_ANSI))
	{
	    for (point = txt ; *point ; point++)
	    {
		if (*point == '#')
		{
		    point++;
		    skip = color(*point, point2);
		    while(skip-- > 0)
			++point2;
		    continue;
		}
		*point2 = *point;
		*++point2 = '\0';
	    }
	    *point2 = '\0';
	    write_to_buffer(usr->desc, buf, point2 - buf);
	}
	else
	{
	    for (point = txt ; *point ; point++)
	    {
		if (*point == '#')
		{
		    point++;
		    continue;
		}
		*point2 = *point;
		*++point2 = '\0';
	    }
	    *point2 = '\0';
	    write_to_buffer(usr->desc, buf, point2 - buf);
	}
    }

    return;
}

void page_to_user_bw( const char *txt, USER_DATA *usr )
{
    if (txt == NULL || usr->desc == NULL)
	return;

    free_string(usr->desc->showstr_head);
    usr->desc->showstr_head = str_dup(txt);
    usr->desc->showstr_point = usr->desc->showstr_head;
    show_string(usr->desc, "", FALSE);
}

void msg_to_user( const char *txt, USER_DATA *usr )
{
    if (txt == NULL || usr->desc == NULL)
	return;

    free_string(usr->desc->showstr_head);
    usr->desc->showstr_head = str_dup(txt);
    usr->desc->showstr_point = usr->desc->showstr_head;
    show_string(usr->desc, "", TRUE);
}

void page_to_user( const char *txt, USER_DATA *usr )
{
    char buf[4 * STRING_LENGTH];
    const char *point;
    char *point2;
    int skip = 0;

    buf[0] = '\0';
    point2 = buf;

    if (txt && usr->desc)
    {
	if (IS_TOGGLE(usr, TOGGLE_ANSI))
	{
	    for (point = txt; *point; point++)
	    {
		if (*point == '#')
		{
		    point++;
		    skip = color(*point, point2);
		    while (skip-- > 0)
			++point2;
		    continue;
		}
		*point2 = *point;
		*++point2 = '\0';
	    }
	    *point2 = '\0';
	    free_string(usr->desc->showstr_head);
	    usr->desc->showstr_head = str_dup(buf);
	    usr->desc->showstr_point = usr->desc->showstr_head;
	    show_string(usr->desc, "", FALSE);
	}
	else
	{
	    for (point = txt; *point; point++)
	    {
		if (*point == '#')
		{
		    point++;
		    continue;
		}
		*point2 = *point;
		*++point2 = '\0';
	    }
	    *point2 = '\0';
	    free_string(usr->desc->showstr_head);
	    usr->desc->showstr_head = str_dup(buf);
	    usr->desc->showstr_point = usr->desc->showstr_head;
	    show_string(usr->desc, "", FALSE);
	}
    }

    return;
}

void show_string( struct desc_data *d, char *input, bool fMx )
{
    char buffer[6 * STRING];
    char buf[INPUT];
    register char *scan;
    int line = 0, toggle = 0;
    int show_lines;

    if (fMx)
	show_lines = 15;
    else
	show_lines = 19;

    one_argument(input,buf);

    switch (UPPER(buf[0]))
    {
	case '\0': case 'C':
	    break;

	case 'H': case '?':
	    if (fMx)
	    write_to_buffer(d,
		"c          - continue\n\r"
		"r          - redraw this page\n\r"
		"p          - back one page\n\r"
		"h or ?     - this help\n\r"
		"q or other - exit\n\r", 0);
	    else
	    write_to_buffer(d,
		"c          - continue\n\r"
		"r          - redraw this page\n\r"
		"b          - back one page\n\r"
		"h or ?     - this help\n\r"
		"q or other - exit\n\r", 0);
	    toggle = 1;
	    break;

	case 'R':
	    toggle = 1;
	    break;

	case 'B': case 'P':
	    toggle = 2;
	    break;

	default:
	    if (d->showstr_head)
	    {
		free_string(d->showstr_head);
		d->showstr_head = str_dup("");
	    }
	    d->showstr_point = 0;
	    return;
    }

    if (toggle)
    {
	if (d->showstr_point == d->showstr_head)
	    return;
	if (toggle == 1)
	    line = -1;
	do
	{
	    if (*d->showstr_point == '\n')
		if ((line++) == (show_lines * toggle))
		    break;

	    d->showstr_point--;
	}
	while (d->showstr_point != d->showstr_head);
    }

    line	= 0;
    *buffer	= 0;
    scan	= buffer;

    if (*d->showstr_point)
    {
	do
	{
	    *scan = *d->showstr_point;
	    if (*scan == '\n')
		if ((line++) == show_lines)
		{
		    scan++;
		    break;
		}

	    scan++;
	    d->showstr_point++;
	    if (*d->showstr_point == 0)
		break;
	}
	while (1);
    }

    *scan	= 0;
    write_to_buffer(d, buffer, strlen(buffer));
    if (*d->showstr_point == 0)
    {
	free_string(d->showstr_head);
	d->showstr_head = str_dup("");
	d->showstr_point = 0;
    }

    return;
}

char * last_logon( USER_DATA *usr )
{
    char *strtime = ctime(&usr->last_logon);
    char buf1[100], buf2[100];

    strtime[strlen(strtime)-6] = '\0';
    strcpy(buf1, strtime);
    strcpy(buf2, &buf1[4]);
    return str_dup(buf2);
}

char * last_logoff( USER_DATA *usr )
{
    char *strtime = ctime(&usr->last_logoff);
    char buf1[100], buf2[100];

    strtime[strlen(strtime)-6] = '\0';
    strcpy(buf1, strtime);
    strcpy(buf2, &buf1[4]);
    return str_dup(buf2);
}

int color( char type, char *string )
{
    char code[20];
    char *p = '\0';

    switch (type)
    {
	default:
	    sprintf(code, COLOR_NORMAL);
	    break;
	case 'x': case 'w':
	    sprintf(code, COLOR_NORMAL);
	    break;
	case 'b':
	    sprintf(code, COLOR_BLUE);
	    break;
	case 'r':
	    sprintf(code, COLOR_RED);
	    break;
	case 'y':
	    sprintf(code, COLOR_YELLOW);
	    break;
	case 'c':
	    sprintf(code, COLOR_CYAN);
	    break;
	case 'g':
	    sprintf(code, COLOR_GREEN);
	    break;
	case 'm':
	    sprintf(code, COLOR_MAGENTA);
	    break;
	case 'B':
	    sprintf(code, COLOR_BLUE_BOLD);
	    break;
	case 'R':
	    sprintf(code, COLOR_RED_BOLD);
	    break;
	case 'Y':
	    sprintf(code, COLOR_YELLOW_BOLD);
	    break;
	case 'C':
	    sprintf(code, COLOR_CYAN_BOLD);
	    break;
	case 'G':
	    sprintf(code, COLOR_GREEN_BOLD);
	    break;
	case 'M':
	    sprintf(code, COLOR_MAGENTA_BOLD);
	    break;
	case 'W':
	    sprintf(code, COLOR_WHITE);
	    break;
	case 'D':
	    sprintf(code, COLOR_DARK);
	    break;
	case '#':
	    sprintf(code, "%c", '#');
	    break;
    }

    p = code;

    while (*p != '\0')
    {
	*string = *p++;
	*++string = '\0';
    }

    return (strlen(code));
}

void get_small_host( USER_DATA *usr )
{
    char buf[INPUT];
    char *host, *old_host;
    char *host_name;

    if (!usr)
	return;

    old_host = usr->host_name;
    host_name = usr->host_name;

    if (!str_cmp(host_name, "localhost")
    || !str_cmp(host_name, "<unknown>"))
    {
	buf[0] = '\0';
	sprintf(buf, "#c%s", host_name);
	if (usr->host_small)
	    free_string(usr->host_small);
	usr->host_small = str_dup(buf);
	if (usr->host_name)
	    free_string(usr->host_name);
	usr->host_name = str_dup(old_host);
	return;
    }

    host = strrchr(host_name, '.');
    host++;
    if (is_number(host))
    {
	buf[0] = '\0';
	host_name[strlen(host_name)-strlen(host)] = '\0';
	sprintf(buf, "#m%s*", host_name);
	if (usr->host_small)
	    free_string(usr->host_small);
	usr->host_small = str_dup(buf);
	if (usr->host_name)
	    free_string(usr->host_name);
	usr->host_name = str_dup(old_host);
	return;
    }
    else
    {
	buf[0] = '\0';
	host = strchr(host_name, '.');
	host++;
	sprintf(buf, "#c%s", host);

	if (strlen(buf) > 18)
	    buf[18] = '\0';

	if (usr->host_small)
	    free_string(usr->host_small);
	usr->host_small = str_dup(buf);
	if (usr->host_name)
	    free_string(usr->host_name);
	usr->host_name = str_dup(old_host);
	return;
    }
}
