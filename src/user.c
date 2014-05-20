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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "bbs.h"

void	do_quit_org	( USER_DATA *usr, char *argument, bool fXing );
bool	legal_name	( char *name );
void	bbs_shutdown	( void );
void	banish_update	( void );
bool	fShutdown;
sh_int	shutdown_tick;

/*
 * See if a string is one of the names of an object.
 */
bool is_name( char *str, char *namelist )
{
    char name[INPUT_LENGTH], part[INPUT_LENGTH];
    char *list, *string;

    if (namelist == NULL || namelist[0] == '\0')
	return FALSE;

    if (str == NULL || str[0] == '\0')
	return FALSE;

    string = str;
    for (;;)
    {
	str = one_argument(str, part);

	if (part[0] == '\0')
	    return TRUE;

	list = namelist;
	for (;;)
	{
	    list = one_argument(list, name);
	    if (name[0] == '\0')
		return FALSE;

	    if (!str_prefix(string, name))
		return TRUE;

	    if (!str_prefix(part, name))
		break;
	}
    }
}

bool is_name_full( char *str, char *namelist )
{
    char name[INPUT_LENGTH], part[INPUT_LENGTH];
    char *list, *string;

    if (namelist == NULL || namelist[0] == '\0')
	return FALSE;

    if (str == NULL || str[0] == '\0')
	return FALSE;

    string = str;
    for (;;)
    {
	str = one_argument(str, part);

	if (part[0] == '\0')
	    return TRUE;

	list = namelist;
	for (;;)
	{
	    list = one_argument(list, name);
	    if (name[0] == '\0')
		return FALSE;

	    if (!str_cmp(string, name))
		return TRUE;

	    if (!str_cmp(part, name))
		break;
	}
    }
}

/*
 * Stuff for setting ids.
 */
long	last_id;

long get_id( void )
{
    int val;

    val = (current_time <= last_id) ? last_id + 1 : current_time;
    last_id = val;
    return val;
}

/*
 * Extract a user from the bbs.
 */
void extract_user( USER_DATA *usr )
{
    if (usr == user_list )
    {
	user_list = usr->next;
    }
    else
    {
	USER_DATA *prev;

	for (prev = user_list; prev != NULL; prev = prev->next)
	{
	    if (prev->next == usr)
	    {
		prev->next = usr->next;
		break;
	    }
	}

	if (prev == NULL)
	{
	    bbs_bug("Extract_user: User not found");
	    return;
	}
    }

    if (usr->desc != NULL)
	USR(usr->desc) = NULL;
    free_user(usr);
    return;
}

/*
 * Find a user in the bbs (prefix).
 */
USER_DATA *get_user( char *argument )
{
    char arg[INPUT_LENGTH];
    USER_DATA *usr;
    int number, count;

    number = number_argument(argument, arg);
    count  = 0;

    for (usr = user_list; usr != NULL; usr = usr->next)
    {
	if (!is_name(arg, usr->name))
	    continue;

	if (++count == number)
	    return usr;
    }

    return NULL;
}

/*
 * Find a user in the bbs (full).
 */
USER_DATA *get_user_full( char *argument )
{
    char arg[INPUT_LENGTH];
    USER_DATA *usr;
    int number, count;

    number = number_argument(argument, arg);
    count  = 0;

    for (usr = user_list; usr != NULL; usr = usr->next)
    {
	if (!is_name_full(arg, usr->name))
	    continue;

	if (++count == number)
	    return usr;
    }

    return NULL;
}

void clear_user( USER_DATA *usr )
{
    char buf[STRING_LENGTH];
    FILE *fpClip;
    BOARD_DATA *pBoard;

    sprintf(buf, "%s%s.clp", CLIP_DIR, capitalize(usr->name));
    if ((fpClip = fopen(buf, "r")) != NULL)
    {
	fclose(fpClip);
	unlink(buf);
    }

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
	usr->last_note[board_number(pBoard)] = 0;

    usr->real_name	= str_dup("NONE");
    usr->email		= str_dup("NONE");
    usr->alt_email	= str_dup("NONE");
    usr->home_url	= str_dup("NONE");
    usr->icquin		= str_dup("NONE");
    usr->title		= str_dup("has no title");
    usr->idlemsg	= str_dup("NONE");
    usr->plan		= str_dup("");
    usr->old_plan	= str_dup("");
    usr->host_name	= str_dup("#UNSET");
    usr->host_small	= str_dup("#UNSET");
    usr->sClip		= str_dup("#UNSET");
    usr->zap		= str_dup("");
    usr->fnotify	= str_dup("");
    usr->gender		= 0;
    usr->bad_pwd	= 0;
    usr->fToggle	= 0;
    usr->fHide		= 0;
    usr->snooped	= "*NONE*";
    usr->xing_to	= "*NONE*";
    usr->last_xing	= "*NONE*";
    usr->reply_to	= "*NONE*";
    usr->key		= 1;
    usr->Validated	= FALSE;
    usr->version	= 1;
    usr->total_login	= 0;
    usr->total_note	= 0;

    if (IS_TOGGLE(usr, TOGGLE_INVIS))
	REM_TOGGLE(usr, TOGGLE_INVIS);
}

bool is_user( char *name )
{
    char buf[STRING_LENGTH];
    FILE *fp;

    if (!legal_name(name))
	return FALSE;

    sprintf(buf, "%s%s", USER_DIR, capitalize(name));
    if ((fp = fopen(buf, "r")) != NULL)
    {
	fclose(fp);
	return TRUE;
    }

    return FALSE; 
}

DESC_DATA *	desc_free;

DESC_DATA * new_desc( void )
{
    static DESC_DATA d_zero;
    DESC_DATA *d;

    if (desc_free == NULL)
	d = (DESC_DATA *) alloc_perm(sizeof(*d));
    else
    {
	d = desc_free;
	desc_free = desc_free->next;
    }

    *d = d_zero;
    VALIDATE(d);

    USR(d)		= NULL;
    d->login		= CON_GET_NAME;
    d->showstr_head	= str_dup("");
    d->showstr_point	= 0;
    d->outsize		= 2000;
    d->outbuf		= alloc_mem(d->outsize);
    d->pString		= NULL;
    d->ipid		= -1;
    d->ifd		= -1;
    d->ident		= str_dup("???");
    return d;
}

void free_desc( DESC_DATA *d )
{
    if (!IS_VALID(d))
	return;

    if (d->host)
	free_string(d->host);
    if (d->ident)
	free_string(d->ident);

    free_mem(d->outbuf, d->outsize);
    INVALIDATE(d);
    d->next = desc_free;
    desc_free = d;
}

USER_DATA *	user_free;

USER_DATA * new_user( void )
{
    static USER_DATA usr_zero;
    USER_DATA *usr;

    if (user_free == NULL)
	usr = (USER_DATA *) alloc_perm(sizeof(*usr));
    else
    {
	usr = user_free;
	user_free = user_free->next;
    }

    *usr		= usr_zero;
    usr->pBuffer	= new_buf( );
    usr->pMsgFirst	= NULL;
    usr->pMsgLast	= NULL;

    VALIDATE(usr);
    usr->name		= &str_empty[0];
    usr->age		= current_time;
    EDIT_MODE(usr)	= EDITOR_NONE;
    return usr;
}

void free_user( USER_DATA *usr )
{
    MESSAGE_DATA *fMessage;
    MAIL_DATA *pMail;

    if (!IS_VALID(usr))
	return;

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
	mail_remove(usr, pMail, FALSE);

    for (fMessage = usr->pMsgFirst; fMessage; fMessage = fMessage->next)
    {
	UNLINK(fMessage, usr->pMsgFirst, usr->pMsgLast);
	if (fMessage->message)
	    free_string(fMessage->message);
	free_mem(fMessage, sizeof(MESSAGE_DATA));
    }

    if (usr->name)	free_string(usr->name);
    if (usr->password)	free_string(usr->password); 

    if (usr->lastCommand)
	free_string(usr->lastCommand);

    free_buffer(usr); 
    usr->next = user_free;
    user_free = usr;
    INVALIDATE(usr);
    return;
}

void save_user( USER_DATA *usr )
{
    char strsave[INPUT];
    BOARD_DATA *pBoard;
    FILE *fp;
    int alias;
    int friend;
    int notify;
    int enemy;
    int ignore;

    if (usr == NULL)
    {
	bbs_bug("Save_user: Null usr");
	return;
    }

    if (usr->level < 1)
	return;

    if (!IS_VALID(usr))
    {
	bbs_bug("save_user: Trying to save an invalidated user.");
	return;
    }

    fclose(fpReserve);
    sprintf(strsave, "%s%s", USER_DIR, capitalize(usr->name));
    if ((fp = fopen(TEMP_FILE, "w")) == NULL)
    {
	bbs_bug("Save_user: Fopen");
	perror(strsave);
    }
    else
    {
	fprintf(fp, "Name %s~\n",	usr->name	);
	fprintf(fp, "Pass %s~\n",	usr->password	);
	fprintf(fp, "Version %d\n",	usr->version	);
	fprintf(fp, "Id   %ld\n",	usr->id		);
	fprintf(fp, "Key  %d\n",	usr->key	);
	fprintf(fp, "Age  %d\n",
	    usr->used + (int) (current_time - usr->age)	);
	fprintf(fp, "HostNam %s %s~\n",	usr->host_name, usr->host_small );
	fprintf(fp, "TotalLN %d %d\n",	usr->total_login, usr->total_note );
	fprintf(fp, "Llon %ld\n",	usr->last_logon	);
	fprintf(fp, "Loff %ld\n",	usr->last_logoff);
	fprintf(fp, "Rnam %s~\n",	usr->real_name	);
	fprintf(fp, "Emai %s~\n",	usr->email	);
	fprintf(fp, "Aema %s~\n",	usr->alt_email	);
	fprintf(fp, "Hurl %s~\n",	usr->home_url	);
	fprintf(fp, "ICQ  %s~\n",	usr->icquin	);
	fprintf(fp, "Imsg %s~\n",	usr->idlemsg	);
	fprintf(fp, "Titl %s~\n",	usr->title	);
	fprintf(fp, "Gend %d\n",	usr->gender	);
	fprintf(fp, "Levl %d\n",	usr->level	);	
	fprintf(fp, "Zap  %s~\n",	usr->zap	);
	fprintf(fp, "FNotify %s~\n",	usr->fnotify	);
	fprintf(fp, "Plan %s~\n",	usr->plan	);
	fprintf(fp, "Val  %d\n",	usr->Validated	);
	if (usr->fToggle)
	    fprintf(fp, "Toggle %s\n",	print_flags(usr->fToggle));
	if (usr->fHide)
	    fprintf(fp, "Hide %s\n",	print_flags(usr->fHide)	);
	for (alias = 0; alias < MAX_ALIAS; alias++)
	{
	    if (usr->alias[alias] == NULL || usr->alias_sub[alias] == NULL)
		break;

	    fprintf(fp, "Alia %s %s~\n", usr->alias[alias],
		usr->alias_sub[alias]);
	}

	for (friend = 0; friend < MAX_FRIEND; friend++)
	{
	    if (usr->friend[friend] == NULL)
		break;

	    fprintf(fp, "Friend %s %s~\n", usr->friend[friend],
		usr->friend_com[friend]);
	}

	for (notify = 0; notify < MAX_FRIEND; notify++)
	{
	    if (usr->notify[notify] == NULL)
		break;

	    fprintf(fp, "Notify %s %s~\n", usr->notify[notify],
		usr->notify_com[notify]);
	}

	for (enemy = 0; enemy < MAX_FRIEND; enemy++)
	{
	    if (usr->enemy[enemy] == NULL)
		break;

	    fprintf(fp, "Enemy  %s %s~\n", usr->enemy[enemy],
		usr->enemy_com[enemy]);
	}

	for (ignore = 0; ignore < MAX_FRIEND; ignore++)
	{
	    if (usr->ignore[ignore] == NULL)
		break;

	    fprintf(fp, "Ignore %s %s~\n", usr->ignore[ignore],
		usr->ignore_com[ignore]);
	}

	for (pBoard = first_board; pBoard; pBoard = pBoard->next)
	    fprintf(fp, "Note %s %ld\n", pBoard->short_name,
		(iptr) usr->last_note[board_number(pBoard)]);

	fprintf(fp, "End\n");
    }
    fclose(fp);
    rename(TEMP_FILE, strsave);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

/*
 * Load one user.
 */
bool load_user( DESC_DATA *d, char *name )
{
    char buf[STRING_LENGTH];
    char strsave[STRING_LENGTH];
    USER_DATA *usr;
    bool found;
    char *word = '\0';
    bool fMatch = FALSE;
    FILE *fp;
    int count = 0;
    int cNotify = 0;
    int cEnemy  = 0;
    int cFriend = 0;
    int cIgnore = 0;

    usr			= new_user();
    USR(d)		= usr;
    usr->desc		= d;
    usr->name		= str_dup(name);
    usr->id		= get_id();
    usr->pBoard		= board_lookup("lobby", FALSE);
    if (usr->pBoard->last_note)
	usr->current_note	= usr->pBoard->last_note;
    else
	usr->current_note	= NULL;
    usr->password	= str_dup("");

    clear_user(usr);
    found		= FALSE;

    fclose(fpReserve);
    sprintf(strsave, "%s%s", USER_DIR, capitalize(name));
    if ((fp = fopen(strsave, "r")) != NULL)
    {
	found = TRUE;

	sprintf(buf, "Loading %s.", usr->name);
	log_string(buf);
	load_mail(usr);

	for (;;)
	{
	    word = feof(fp) ? "End" : fread_word(fp);
	    fMatch = FALSE;

	    switch (UPPER(word[0]))
	    {
		case '*':
		    fMatch = TRUE;
		    fread_to_eol(fp);
		    break;

		case 'A':
		    KEYS("Aema",	usr->alt_email,	fread_string(fp));
		    KEY("Age",		usr->used,	fread_number(fp));
		    if (!str_cmp(word, "Alia"))
		    {
			if (count >= MAX_ALIAS)
			{
			    fread_to_eol(fp);
			    fMatch = TRUE;
			    break;
			}

			usr->alias[count]	= str_dup(fread_word(fp));
			usr->alias_sub[count]	= fread_string(fp);
			count++;
			fMatch = TRUE;
			break;
		    }
		    break;

	 	case 'E':
		    KEYS("Emai",	usr->email,	fread_string(fp));
		    if (!str_cmp(word, "Enemy"))
		    {
			if (cEnemy >= MAX_FRIEND)
			{
			    fread_to_eol(fp);
			    fMatch = TRUE;
			    break;
			}

			usr->enemy[cEnemy]	= str_dup(fread_word(fp));
			usr->enemy_com[cEnemy]	= fread_string(fp);
			cEnemy++;
			fMatch = TRUE;
			break;
		    }
		    if (!str_cmp(word, "End"))
		    {
			return found;
		    }
		    break;

		case 'F':
		    KEY("FNotify",	usr->fnotify,	fread_string(fp));
		    if (!str_cmp(word, "Friend"))
		    {
			if (cFriend >= MAX_FRIEND)
			{
			    fread_to_eol(fp);
			    fMatch = TRUE;
			    break;
			}

			usr->friend[cFriend]	 = str_dup(fread_word(fp));
			usr->friend_com[cFriend] = fread_string(fp);
			cFriend++;
			fMatch = TRUE;
			break;
		    }
		    break;

		case 'G':
		    KEY("Gend",		usr->gender,	fread_number(fp));
		    break;

		case 'H':
		    KEYS("Hurl",	usr->home_url,	fread_string(fp));
		    if (!str_cmp(word, "HostNam"))
		    {
			if (usr->host_name)
			    free_string(usr->host_name);
			usr->host_name = str_dup(fread_word(fp));
			if (usr->host_small)
			    free_string(usr->host_small);
			usr->host_small = fread_string(fp);
			fMatch = TRUE;
			break;
		    }
/*		    KEYS("HostNam",	usr->host_name,	fread_string(fp)); */
		    KEY( "Hide",	usr->fHide,	fread_flag(fp)	);
		    break;

		case 'I':
		    KEY("Id",		usr->id,	fread_number(fp));
		    KEYS("Imsg",	usr->idlemsg,	fread_string(fp));
		    KEYS("ICQ",		usr->icquin,	fread_string(fp));
		    if (!str_cmp(word, "Ignore"))
		    {
			if (cIgnore >= MAX_FRIEND)
			{
			    fread_to_eol(fp);
			    fMatch = TRUE;
			    break;
			}

			usr->ignore[cIgnore]	 = str_dup(fread_word(fp));
			usr->ignore_com[cIgnore] = fread_string(fp);
			cIgnore++;
			fMatch = TRUE;
			break;
		    }
		    break;

		case 'K':
		    KEY("Key",		usr->key,	fread_number(fp));
		    break;

		case 'L':
		    KEY("Levl",		usr->level,	fread_number(fp));
		    KEY("Llon",		usr->last_logon,fread_number(fp));
		    KEY("Loff",		usr->last_logoff,fread_number(fp));
		    break;

	 	case 'N':
		    KEYS("Name",	usr->name,	fread_string(fp));
		    if (!str_cmp(word, "Notify"))
		    {
			if (cNotify >= MAX_FRIEND)
			{
			    fread_to_eol(fp);
			    fMatch = TRUE;
			    break;
			}

			usr->notify[cNotify]	 = str_dup(fread_word(fp));
			usr->notify_com[cNotify] = fread_string(fp);
			cNotify++;
			fMatch = TRUE;
			break;
		    }
		    if (!str_cmp(word, "Note"))
		    {
			BOARD_DATA *pBoard;
			char *name;

			name	= str_dup(fread_word(fp));
			pBoard	= board_lookup(name, FALSE);
			if (!pBoard)
			{
			    fMatch = TRUE;
			    fread_to_eol(fp);
			    break;
			}

			usr->last_note[board_number(pBoard)] =
			    fread_number(fp);
			fMatch = TRUE;
			break;
		    }
		    break;

		case 'P':
		    KEY("Pass",		usr->password,	fread_string(fp));
		    KEYS("Plan",	usr->plan,	fread_string(fp));
		    break;

		case 'R':
		    KEYS("Rnam",	usr->real_name,	fread_string(fp));
		    break;

		case 'T':
		    KEYS("Titl",	usr->title,	fread_string(fp));
		    KEY( "Toggle",	usr->fToggle,	fread_flag(fp)	);
		    if (!str_cmp(word, "TotalLN"))
		    {
			usr->total_login = fread_number(fp);
			usr->total_note	 = fread_number(fp);
			fMatch		 = TRUE;
			break;
		    }
		    break;

		case 'V':
		    KEY("Version",	usr->version,	fread_number(fp));
		    KEY("Val",		usr->Validated,	fread_number(fp));
		    break;

		case 'Z':
		    KEY("Zap",		usr->zap,	fread_string(fp));
		    break;
	    }

	    if (!fMatch)
	    {
		bbs_bug("Load_user: No match '%s'", word);
		fread_to_eol(fp);
	    }
	}
	fclose(fp);
    }

    fpReserve = fopen(NULL_FILE, "r");

    return found;
}

bool isBusySelf( USER_DATA *usr )
{
    if (EDIT_MODE(usr) != EDITOR_NONE
    ||  usr->desc->showstr_point > 0)
	return TRUE;

    return FALSE;
}

void check_fnotify( USER_DATA *sender, BOARD_DATA *pBoard )
{
    USER_DATA *usr;
    char buf[INPUT];

    set_xing_time(sender);

    for (usr = user_list; usr != NULL; usr = usr->next)
    {
	if (usr->desc && usr->desc->login == CON_LOGIN
	&& is_name_full(pBoard->short_name, usr->fnotify)
	&& IS_TOGGLE(usr, TOGGLE_INFO))
	{
	    sprintf(buf, "\n\r%s#W**** #yMessage from: #GNotify Faerie #yat #c%s #W****#x\n\r"
			 "] #G%s#x has left a new note on #G%s#x forum.\n\r\n\r",
		IS_TOGGLE(usr, TOGGLE_BEEP) ? "\007" : "", sender->xing_time,
		sender->name, pBoard->long_name);

	    if (isBusySelf(usr))
	    {
		add_buffer(usr, buf);
		return;
	    }
	    else
	    {
		send_to_user(buf, usr);
		return;
	    }
	}
    }
}

void check_notify( USER_DATA *usr, USER_DATA *notify, bool fLogin )
{
    char buf1[STRING_LENGTH];
    char buf2[STRING_LENGTH];
    bool fNotify = FALSE;

    set_xing_time(notify);

    if (is_notify(usr, notify->name))
	fNotify = TRUE;

    if (is_friend(usr, notify->name) || is_notify(usr, notify->name))
    {
	if (IS_TOGGLE(usr, TOGGLE_INFO))
	{
	    sprintf(buf1, "\n\r%s#W**** #yMessage from: %s #yat #c%s #W****#x\n\r",
		IS_TOGGLE(usr, TOGGLE_BEEP) ? "\007" : "", fNotify ? "#GNotify Faerie" : "#CFriend Faerie", notify->xing_time);
	    if (fLogin)
	    {
		if (fNotify)
		    sprintf(buf2, "] #G%s#x has logged into %s.\n\r\n\r", notify->name, config.bbs_name);
		else
		    sprintf(buf2, "] Your friend #C%s#x has logged into %s.\n\r\n\r",
			notify->name, config.bbs_name);
	    }
	    else
	    {
		if (fNotify)
		    sprintf(buf2, "] #G%s#x has logged out.\n\r\n\r", notify->name);
		else
		    sprintf(buf2, "] Your friend #C%s#x has logged out.\n\r\n\r",
			notify->name);
	    }

	    if (isBusySelf(usr))
	    {
		add_buffer(usr, buf1);
		add_buffer(usr, buf2);
		return;
	    }
	    else
	    {
		send_to_user(buf1, usr);
		send_to_user(buf2, usr);
		return;
	    }
	}
    }
}

void info_notify( USER_DATA *notify, bool fLogin )
{
    USER_DATA *usr;

    for (usr = user_list; usr != NULL; usr = usr->next)
	check_notify(usr, notify, fLogin);
}

void user_update( void )
{
    USER_DATA *usr;
    USER_DATA *usr_next;

    for (usr = user_list; usr != NULL; usr = usr_next)
    {
	if (!IS_VALID(usr))
	{
	    bbs_bug("user_update: Trying to work with an invalidated user.");
	    break;
	}

	usr_next = usr->next;

	if (!usr)
	    return;

	save_user(usr);

	if (++usr->timer == 5 && !IS_ADMIN(usr))
	    print_to_user(usr, "%sHey! You had better act more alive.\n\r",
		IS_TOGGLE(usr, TOGGLE_BEEP) ? "\007" : "");

	if (usr->timer >= 10 && !IS_ADMIN(usr))
	{
	    print_to_user(usr, "%sYou are idle too long.\n\r\n\r",
		IS_TOGGLE(usr, TOGGLE_BEEP) ? "\007" : "");
	    do_quit_org(usr, "", FALSE);
	}
    }

    return;
}

void shutdown_update( void )
{
    DESC_DATA *d, *d_next;
    char buf[STRING];

    if (!fShutdown)
	return;

    switch (--shutdown_tick)
    {
	case 0:
	    bbs_shutdown();
	    break;

	case 1: case 2: case 3: case 4: case 5: case 10:
	case 15: case 20: case 30:
	    sprintf(buf, "\007BBS Driver: Shutting down in %d minutes.\n\r",
		shutdown_tick);
	    break;
    }

    for (d = desc_list; d != NULL; d = d_next)
    {
	d_next = d->next;

	write_to_buffer(d, buf, 0);
    }

    return;
}

void update_bbs( void )
{
    static int pulse_point;

    if (--pulse_point <= 0)
    {
	pulse_point = 2400;
	user_update( );
	shutdown_update( );
	banish_update( );
    }

    tail_chain();
    return;
}

void memory_other( USER_DATA *usr )
{
    char buf[STRING_LENGTH];
    int count1, count2;
    int count_msg  = 0;
    int count_mail = 0;
    USER_DATA *fusr;
    DESC_DATA *fd;
    MESSAGE_DATA *fMessage;
    MAIL_DATA *fMail;

    count1 = count2 = 0;

    for (fusr = user_list; fusr != NULL; fusr = fusr->next)
    {
	count1++;

	for (fMessage = fusr->pMsgFirst; fMessage; fMessage = fMessage->next)
	    count_msg++;

	for (fMail = fusr->pMailFirst; fMail; fMail = fMail->next)
	    count_mail++;
    }

    for (fusr = user_free; fusr != NULL; fusr = fusr->next)
	count2++;

    sprintf(buf, "User : %4d (%8d bytes), %2d free (%d bytes)\n\r",
	count1, count1 * (sizeof(*fusr)), count2, count2 * (sizeof(*fusr)));
    send_to_user(buf, usr);

    sprintf(buf, "Mesg : %4d -- %d bytes\n\r", count_msg,
	count_msg * (sizeof(*fMessage)));
    send_to_user(buf, usr);

    sprintf(buf, "Mail : %4d -- %d bytes\n\r", count_mail,
	count_mail * (sizeof(*fMail)));
    send_to_user(buf, usr);

    count1 = count2 = 0;

    for (fd = desc_list; fd != NULL; fd = fd->next)
	count1++;

    for (fd = desc_free; fd != NULL; fd = fd->next)
	count2++;

    sprintf(buf, "Desc : %4d (%8d bytes), %2d free (%d bytes)\n\r",
	count1, count1 * (sizeof(*fd)), count2, count2 * (sizeof(*fd)));
    send_to_user(buf, usr);
    return;
}

void add_buffer( USER_DATA *pUser, char *pBuffer )
{
    add_buf(pUser->pBuffer, pBuffer);
    return;
}

void free_buffer( USER_DATA *pUser )
{
    free_buf(pUser->pBuffer);
    return;
}

bool is_turkish( USER_DATA *usr )
{
    if (IS_TOGGLE(usr, TOGGLE_TURK))
	return TRUE;

    return FALSE;
}

