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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include "bbs.h"

void	do_quit_org	( USER_DATA *usr, char *argument, bool fXing );
void	show_validate	( USER_DATA *usr, char *argument );
void	send_validate	( USER_DATA *usr, char *argument );
void	remove_validate	( USER_DATA *usr, char *argument );
char *	time_str	( time_t last_time );

VALIDATE_DATA *		first_validate;
VALIDATE_DATA *		last_validate;

char *get_status( DESC_DATA *d )
{
    char buf[STRING_LENGTH];
    char bufX[STRING_LENGTH];
    USER_DATA *usr = USR(d);

    strcpy(buf, "");
    if (d->login != CON_LOGIN)
	strcat(buf, "  Not login   ");
    else
    {
	if (usr == NULL)
	    strcat(buf, "   No user   ");

    else if (EDIT_MODE(usr) == EDITOR_NONE)
    {
	if (d->showstr_point > 0)
	    strcat(buf, "     Busy     ");
	else
	    strcat(buf, "    Nobusy    "); 
    }
    else if (EDIT_MODE(usr) == EDITOR_XING
    || EDIT_MODE(usr) == EDITOR_XING_RECEIPT
    || EDIT_MODE(usr) == EDITOR_XING_REPLY
    || EDIT_MODE(usr) == EDITOR_FEELING
    || EDIT_MODE(usr) == EDITOR_FEELING_RECEIPT)
    {
	if (str_cmp(usr->xing_to, "*NONE*"))
	{
	    sprintf(bufX, "X %-12s", usr->xing_to);
	    strcat(buf, bufX);
	}
	else
	    strcat(buf, " Xing someone ");
    }
    else if (EDIT_MODE(usr) == EDITOR_NOTE
    || EDIT_MODE(usr) == EDITOR_NOTE_SUBJECT)
	strcat(buf, "Writing a note");
    else if (EDIT_MODE(usr) == EDITOR_PLAN
    || EDIT_MODE(usr) == EDITOR_PLAN_ANSWER)
	strcat(buf, "Writing a plan");
    else if (EDIT_MODE(usr) == EDITOR_INFO_ANSWER
    || EDIT_MODE(usr) == EDITOR_PLAN)
	strcat(buf, "Writing a info");
    else if (EDIT_MODE(usr) == EDITOR_QUIT_ANSWER)
	strcat(buf, "   Quitting   ");
    else if (EDIT_MODE(usr) == EDITOR_MAIL
    || EDIT_MODE(usr) == EDITOR_MAIL_SUBJECT
    || EDIT_MODE(usr) == EDITOR_MAIL_WRITE)
	strcat(buf, "   Mailing    ");
    else if (EDIT_MODE(usr) == EDITOR_PWD_OLD
    || EDIT_MODE(usr) == EDITOR_PWD_NEW_ONE
    || EDIT_MODE(usr) == EDITOR_PWD_NEW_TWO)
	strcat(buf, "Chng. password");
    else
	strcat(buf, "I don't know? ");
    }

    return str_dup(buf);
}

void do_sockets( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    BUFFER *buffer;
    DESC_DATA *d, *d_next;
    int nMatch = 0;

    buffer = new_buf();

    for (d = desc_list; d != NULL; d = d_next)
    {
	d_next = d->next;

	sprintf(buf, "#W[%3d] [%4d] #y[%s] #Y%-12s #c%s@%s %s#x\n\r", d->descr,
	    USR(d) ? USR(d)->timer : 0,
	    get_status(d), USR(d) ?
	    USR(d)->name : "#R<Connected> ", d->ident, d->host,
	    USR(d) ? IS_TOGGLE(USR(d), TOGGLE_INVIS) ?
	    "#x(Invisible)" : !USR(d)->Validated ?
	    "#R(Unvalidated)" : "" : "");
	add_buf(buffer, buf);
	nMatch++;
    }
    send_to_user("#W[Num] [Idle] [Status]         [Nick name]  [From]#x\n\r", usr);
    page_to_user(buf_string(buffer), usr);
    free_buf(buffer);
    sprintf(buf, "#GTotal #Y%d #Guser%s online.#x\n\r", nMatch, nMatch > 1 ? "s" : "");
    send_to_user(buf, usr);
    return;
}

void do_disconnect( USER_DATA *usr, char *argument )
{
    char arg[INPUT_LENGTH];
    DESC_DATA *d;
    USER_DATA *dusr;
    int desc;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("disconnect [user|desc vnum]", usr);
	return;
    }

    if (is_number(arg))
    {
	desc = atoi(arg);

	for (d = desc_list; d != NULL; d = d->next)
	{
	    if (d->descr == desc)
	    {
		close_socket(d);
		send_to_user("Ok.\n\r", usr);
		return;
	    }
	}
    }

    if ((dusr = get_user(arg)) == NULL)
    {
	send_to_user("User is not online.\n\r", usr);
	return;
    }

    if (dusr->desc == NULL)
    {
	send_to_user("User doesn't have a descriptor.\n\r", usr);
	return;
    }

    for (d = desc_list; d != NULL; d = d->next)
    {
	if (d == dusr->desc)
	{
	    close_socket(d);
	    send_to_user("Ok.\n\r", usr);
	    return;
	}
    }

    bbs_bug("Do_disconnect: Null desc");
    send_to_user("ERROR: Descriptor not found.\n\r", usr);
    return;    
}

void do_hostname( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    USER_DATA *husr;

    argument = one_argument(argument, arg);

    if (argument[0] == '\0' || arg[0] == '\0')
    {
	syntax("hostname [user] [host]", usr);
	return;
    }

    if (!is_user(arg))
    {
	send_to_user("No such user.\n\r", usr);
	return;
    }

    if ((husr = get_user(arg)) == NULL)
    {
	send_to_user("User is not online.\n\r", usr);
	return;
    }

    if (strlen(argument) > 16)
    {
	send_to_user("Hostname must be 16 characters.\n\r", usr);
	return;
    }

    sprintf(buf, "#c%s", argument);
    if (husr->host_name)
	free_string(husr->host_name);
    husr->host_name = str_dup(buf);
    get_small_host(husr);
    send_to_user("Ok.\n\r", usr);
    return;
}

void do_shutdow( USER_DATA *usr, char *argument )
{
    send_to_user("If you want to SHUTDOWN, spell it out.\n\r", usr);
    return;
}

void do_shutdown( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    extern bool fShutdown;
    extern bool bbs_down;
    extern sh_int shutdown_tick;
    DESC_DATA *d, *d_next;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("shutdown [minute|now|cancel]", usr);
	return;
    }

    if (!str_cmp(arg, "now"))
    {
	bbs_down = TRUE;

	for (d = desc_list; d != NULL; d = d_next)
	{
	    d_next = d->next;

	    write_to_buffer(d, "\007BBS Driver: Saving all users.\n\r"
			       "BBS Driver: Shutting down now!\n\r", 0);
	    if (USR(d) && d->login == CON_LOGIN)
		save_user(USR(d));
	    close_socket(d);
	}
	return;
    }
    else if (!str_cmp(arg, "cancel"))
    {
	if (!fShutdown)
	{
	    send_to_user("Shutdown is not started.\n\r", usr);
	    return;
	}
	else
	{
	    fShutdown = FALSE;
	    for (d = desc_list; d != NULL; d = d_next)
	    {
		d_next = d->next;

		write_to_buffer(d, "BBS Driver: Shutdown cancelled.\n\r", 0);
	    }
	    send_to_user("Ok.\n\r", usr);
	    return;
	}
    }
    else if (!is_number(arg))
    {
	syntax("shutdown [minute|now|cancel]", usr);
	return;
    }
    else if (atoi(arg) < 1 || atoi(arg) > 30)
    {
	send_to_user("Must be 1 to 30 minutes.\n\r", usr);
	return;
    }
    else
    {
	fShutdown = TRUE;
	shutdown_tick = atoi(arg);
	sprintf(buf, "\007BBS Driver: Shutting down in %d minutes.\n\r",
	    shutdown_tick);
	for (d = desc_list; d != NULL; d = d_next)
	{
	    d_next = d->next;

	    write_to_buffer(d, buf, 0);
	}

	send_to_user("Ok.\n\r", usr);
	return;
    }
}

void bbs_shutdown( void )
{
    extern bool bbs_down;
    DESC_DATA *d, *d_next;

    bbs_down = TRUE;

    for (d = desc_list; d != NULL; d = d_next)
    {
	d_next = d->next;

	write_to_buffer(d, "\007BBS Driver: Saving all users.\n\r"
			   "BBS Driver: Shutting down now!\n\r", 0);
	if (USR(d) && d->login == CON_LOGIN)
	    save_user(USR(d));
	close_socket(d);
    }

    return;
}

void do_userset( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg1[INPUT_LENGTH];
    char arg2[INPUT_LENGTH];
    USER_DATA *susr;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
	syntax("userset <user> <type> <set>", usr);
	return; 
    }

    if ((susr = get_user(arg1)) == NULL)
    {
	if (!is_user(arg1))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	send_to_user("User is not online.\n\r", usr);
	return;
    }

    smash_tilde(argument);

    if (!str_cmp(arg2, "name"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> name <set>", usr);
	    return; 
	}

	free_string(susr->real_name);
	susr->real_name = str_dup(argument);
	sprintf(buf, "%s's real name now '%s'.\n\r", susr->name,
	    susr->real_name);
	send_to_user(buf, usr);
	return;
    }
    else if (!str_cmp(arg2, "email"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> email <set>", usr);
	    return; 
	}

	free_string(susr->email);
	susr->email = str_dup(argument);
	sprintf(buf, "%s's e-mail now '%s'.\n\r", susr->name,
	    susr->email);
	send_to_user(buf, usr);
	return;
    }
    else if (!str_cmp(arg2, "alt"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> alt <set>", usr);
	    return; 
	}

	free_string(susr->alt_email);
	susr->alt_email = str_dup(argument);
	sprintf(buf, "%s's alternative e-mail now '%s'.\n\r", susr->name,
	    susr->alt_email);
	send_to_user(buf, usr);
	return;
    }
    else if (!str_cmp(arg2, "title"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> title <set>", usr);
	    return; 
	}

	free_string(susr->title);
	susr->title = str_dup(argument);
	sprintf(buf, "%s's title now '%s'.\n\r", susr->name,
	    susr->title);
	send_to_user(buf, usr);
	return;
    }
    else if (!str_cmp(arg2, "url"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> url <set>", usr);
	    return; 
	}

	free_string(susr->home_url);
	susr->home_url = str_dup(argument);
	sprintf(buf, "%s's homepage url now '%s'.\n\r", susr->name,
	    susr->home_url);
	send_to_user(buf, usr);
	return;
    }
    else if (!str_cmp(arg2, "icq"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> icq <set>", usr);
	    return; 
	}

	free_string(susr->icquin);
	susr->icquin = str_dup(argument);
	sprintf(buf, "%s's ICQ number now '%s'.\n\r", susr->name,
	    susr->icquin);
	send_to_user(buf, usr);
	return;
    }
    else if (!str_cmp(arg2, "gender"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> gender <male|female>", usr);
	    return; 
	}

	if (!str_cmp(argument, "male"))
	{
	    susr->gender = 0;
	    sprintf(buf, "%s's gender now 'male'.\n\r", susr->name);
	    send_to_user(buf, usr);
	}
	else if (!str_cmp(argument, "female"))
	{
	    susr->gender = 1;
	    sprintf(buf, "%s's gender now 'female'.\n\r", susr->name);
	    send_to_user(buf, usr);
	}
	else
	    syntax("userset <user> gender <male|female>", usr);
	return;
    }
    else if (!str_cmp(arg2, "level"))
    {
	if (argument[0] == '\0')
	{
	    syntax("userset <user> level admin", usr);
	    return; 
	}

	if (!str_cmp(argument, "admin"))
	{
	    susr->level = ADMIN;
	    sprintf(buf, "%s now new admin of %s BBS.\n\r", susr->name,
		config.bbs_name);
	    send_to_user(buf, usr);
	    sprintf(buf, "You are new admin of %s BBS now!\n\r",
		config.bbs_name);
	    send_to_user(buf, susr);
	}
	else
	    syntax("userset <user> level admin", usr);
	return;
    }
    else if (!str_cmp(arg2, "validate"))
    {
	if (susr->Validated)
	{
	    send_to_user("User is already validated.\n\r", usr);
	    return;
	}

	susr->Validated = TRUE;
	send_to_user("Ok.\n\r", usr);
	send_to_user("You have been validated by admins.\n\r", susr);
	return;
    }
    else
    {
	send_to_user("That's not a valid argument.\n\r", usr);
	send_to_user(
	    "Available arguments are: name, email, alt, title, url, gender, level,\n\r"
	    "                         validate, icq.\n\r", usr);
	return;
    }
}

void do_deluser( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    USER_DATA *dusr;
    FILE *fp;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("deluser <user>", usr);
	return;
    }

    if (!is_user(arg))
    {
	send_to_user("No such user.\n\r", usr);
	return;
    }

    if ((dusr = get_user(arg)) != NULL)
    {
	sprintf(buf, "%s%s.clp", CLIP_DIR, capitalize(dusr->name));
	if ((fp = fopen(buf, "r")))
	{
	    fclose(fp);
	    unlink(buf);
	}

	send_to_user("#W\077\n\rYou have been deleted!\n\r\n\r", dusr);
	do_quit_org(dusr, "", TRUE);
	sprintf(buf, "%s%s", USER_DIR, capitalize(dusr->name));
	if ((fp = fopen(buf, "r")))
	{
	    fclose(fp);
	    unlink(buf);
	    sprintf(buf, "Deleted user '%s'.\n\r", capitalize(arg));
	    send_to_user(buf, usr);
	    return;
	}
	else
	{
	    bbs_bug("Do_deluser: Could not open user file");
	    send_to_user("ERROR: Could not open user file.\n\r", usr);
	    return;
	}
    }
    else
    {
	sprintf(buf, "%s%s", USER_DIR, capitalize(arg));
	if ((fp = fopen(buf, "r")) != NULL)
	{
	    fclose(fp);
	    unlink(buf);
	    sprintf(buf, "Deleted user '%s'.\n\r", capitalize(arg));
	    send_to_user(buf, usr);
	    return;
	}
	else
	{
	    bbs_bug("Do_deluser: Could not open user file");
	    send_to_user("ERROR: Cannot open user file.\n\r", usr);
	    return;
	}
    }
}

void do_saveall( USER_DATA *usr, char *argument )
{
    USER_DATA *all, *all_next;

    for (all = user_list; all != NULL; all = all_next)
    {
	all_next = all->next;

	if (all)
	    save_user(all);
    }

    send_to_user("Saving all users.\n\r", usr);
    return;
}
	
void syslog( char *txt, USER_DATA *usr )
{
    char buf[STRING_LENGTH];
    DESC_DATA *d;

    if (!txt)
	return;

    for (d = desc_list; d; d = d->next)
    {
	if (d->login == CON_LOGIN && USR(d) && IS_ADMIN(USR(d)) && usr)
	{
	    set_xing_time(usr);

	    sprintf(buf, "#g[Syslog (%s): %s]#x\n\r", usr->xing_time, txt);
	    if (isBusySelf(USR(d)))
		add_buffer(USR(d), buf);
	    else
		send_to_user(buf, USR(d));
	}
    }

    return;
}

void system_info( char *txt, ... )
{
    char buf[STRING];
    char str[STRING];
    va_list args;
    DESC_DATA *d;

    va_start(args, txt);
    vsprintf(buf, txt, args);
    va_end(args);

    for (d = desc_list; d; d = d->next)
    {
	if (d->login == CON_LOGIN && USR(d) && IS_ADMIN(USR(d)))
	{
	    sprintf(str, "#g[System: %s]#x\n\r", buf);

	    if (isBusySelf(USR(d)))
		add_buffer(USR(d), str);
	    else
		send_to_user(str, USR(d));
	}
    }

    return;
}

void do_snoop( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    USER_DATA *spy;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("snoop <user>", usr);
	return;
    }

    if (!str_cmp(arg, "stop"))
    {
	if (!str_cmp(usr->snooped, "*NONE*"))
	{
	    send_to_user("You don't snoop any person.\n\r", usr);
	    usr->snooped = "*NONE*";
	    return;
	}

	sprintf(buf, "Ok, stopped snooping %s.\n\r", usr->snooped);
	send_to_user(buf, usr);
	usr->snooped = "*NONE*";
	return;
    }

    if (!is_user(arg))
    {
	send_to_user("No such user.\n\r", usr);
	return;
    }

    if ((spy = get_user(arg)) == NULL)
    {
	send_to_user("User is not online.\n\r", usr);
	return;
    }

    if (spy == usr)
    {
	send_to_user("You can't snoop yourself.\n\r", usr);
	return;
    }

    if (is_name(spy->name, "Desii"))
    {
	send_to_user("If you wanna play tennis? You can play with my penis.\n\r", usr);
	return;
    }

    if (IS_ADMIN(spy))
    {
	send_to_user("You can't snoop your brothers.\n\r", usr);
	return;
    }

    usr->snooped = spy->name;
    sprintf(buf, "Ok, snoop started for %s.\n\r", spy->name);
    send_to_user(buf, usr);
    return;
}

void stop_snoop( USER_DATA *usr )
{
    USER_DATA *adm;

    if (!usr)
	return;

    for (adm = user_list; adm != NULL; adm = adm->next)
    {
	if (!str_cmp(adm->snooped, usr->name) && adm->desc->login == CON_LOGIN)
	{
	    do_snoop(adm, "stop");
	    return;
	}
    }

    return;
}

void do_force( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    USER_DATA *fusr;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
	syntax("force <user> <command>", usr);
	return;
    }

    if (!is_user(arg))
    {
	send_to_user("No such user.\n\r", usr);
	return;
    }

    if ((fusr = get_user(arg)) == NULL)
    {
	send_to_user("User is not online.\n\r", usr);
	return;
    }

    if (IS_ADMIN(fusr))
    {
	send_to_user("You can't force your brothers.\n\r", usr);
	return;
    }

    if (isBusySelf(fusr))
    {
	sprintf(buf, "%s is busy now, you can't force him(er).\n\r",
	    fusr->name);
	send_to_user(buf, usr);
	return;
    }

    process_command(fusr, argument);
    sprintf(buf, "Ok, you force %s to: %s.\n\r", fusr->name, argument);
    send_to_user(buf, usr);
    return;
}

void do_invis( USER_DATA *usr, char *argument )
{
    if (IS_TOGGLE(usr, TOGGLE_INVIS))
    {
	REM_TOGGLE(usr, TOGGLE_INVIS);
	send_to_user("You slowly fade back into existence.\n\r", usr);
	return;
    }
    else
    {
	SET_TOGGLE(usr, TOGGLE_INVIS);
	send_to_user("You slowly vanish into thin air.\n\r", usr);
	return;
    }
}

/*
 * Read validate.
 */
VALIDATE_DATA *read_validate( char *fName, FILE *fp )
{
    VALIDATE_DATA *pValidate;
    char *word = '\0';
    bool fMatch = FALSE;
    char letter;

    do
    {
	letter = getc(fp);
	if (feof(fp))
	{
	    fclose(fp);
	    return NULL;
	}
    }
    while (isspace(letter));
    ungetc(letter, fp);

    pValidate = alloc_mem(sizeof(VALIDATE_DATA));

    for (;;)
    {
	word	= feof(fp) ? "End" : fread_word(fp);
	fMatch	= FALSE;

	switch (UPPER(word[0]))
	{
	    case '*':
		fMatch = TRUE;
		fread_to_eol(fp);
		break;

	    case 'E':
		KEY( "Email",	pValidate->email,	fread_string(fp));
		if (!str_cmp(word, "End"))
		{
		    pValidate->next	= NULL;
		    pValidate->prev	= NULL;
		    return pValidate;
		}
		break;

	    case 'H':
		KEY( "Host",	 pValidate->host,	fread_string(fp));
		break;

	    case 'K':
		KEY( "Key",	 pValidate->key,	fread_number(fp));
		break;

	    case 'N':
		KEY( "Name",	 pValidate->name,	fread_string(fp));
		break;

	    case 'R':
		KEY( "Realname", pValidate->real_name,	fread_string(fp));
		break;

	    case 'S':
		KEY( "SendedBy", pValidate->sended_by,	fread_string(fp));
		KEY( "SendedTm", pValidate->sended_time,fread_number(fp));
		break;

	    case 'T':
		KEY( "Time",	pValidate->valid_time,	fread_number(fp));
		break;
	}

	if (!fMatch)
	{
	    bbs_bug("Read_validate: No match '%s'", word);
	    exit(1);
	    return NULL;
	}
    }

    return pValidate;
}

/*
 * Load all validates.
 */
void load_validates( void )
{
    VALIDATE_DATA *pValidate;
    FILE *fp;

    first_validate	= NULL;
    last_validate	= NULL;

    if (!(fp = fopen(VALIDATE_FILE, "r")))
    {
	bbs_bug("Load_validates: Could not open to read %s", VALIDATE_FILE);
	return;
    }

    while ((pValidate = read_validate(VALIDATE_FILE, fp)) != NULL)
	LINK(pValidate, first_validate, last_validate);

    log_string("Load_validates: Done");
    return;
}

/*
 * Save all validates.
 */
void save_validates( void )
{
    VALIDATE_DATA *pValidate;
    FILE *fp;

    fclose(fpReserve);
    if (!(fp = fopen(VALIDATE_FILE, "w")))
    {
	bbs_bug("Save_validates: Could not open to write %s", VALIDATE_FILE);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    for (pValidate = first_validate; pValidate; pValidate = pValidate->next)
    {
	fprintf(fp, "Name     %s~\n",	pValidate->name		);
	fprintf(fp, "Host     %s~\n",	pValidate->host		);
	fprintf(fp, "Email    %s~\n",	pValidate->email	);
	fprintf(fp, "Realname %s~\n",	pValidate->real_name	);
	fprintf(fp, "SendedBy %s~\n",	pValidate->sended_by	);
	fprintf(fp, "SendedTm %ld\n",	pValidate->sended_time	);
	fprintf(fp, "Key      %d\n",	pValidate->key		);
	fprintf(fp, "Time     %ld\n",	pValidate->valid_time	);
	fprintf(fp, "End\n"					);
    }

    fclose(fp);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

void add_validate( USER_DATA *usr )
{
    VALIDATE_DATA *pValidate;
    int key;

    key			   = number_range(10000, 99999);
    if (key < 10000) key   = 10895;
    if (key > 99999) key   = 84719;
    usr->key		   = key;

    pValidate		   = alloc_mem(sizeof(VALIDATE_DATA));
    pValidate->next	   = NULL;
    pValidate->prev	   = NULL;
    pValidate->name	   = str_dup(usr->name);
    pValidate->real_name   = str_dup(usr->real_name);
    pValidate->email	   = str_dup(usr->email);
    pValidate->host	   = str_dup(usr->desc->host);
    pValidate->valid_time  = current_time;
    pValidate->sended_by   = str_dup("[none]");
    pValidate->key	   = usr->key;
    pValidate->sended_time = 0;
    LINK(pValidate, first_validate, last_validate);
    save_validates( );
    return;
}

void free_validate( VALIDATE_DATA *pValidate )
{
    if (!pValidate)
    {
	bbs_bug("Free_validate: Null pValidate");
	return;
    }

    if (pValidate->name)	free_string(pValidate->name);
    if (pValidate->email)	free_string(pValidate->email);
    if (pValidate->host)	free_string(pValidate->host);
    if (pValidate->real_name)	free_string(pValidate->real_name);
    if (pValidate->sended_by)	free_string(pValidate->sended_by);

    free_mem(pValidate, sizeof(VALIDATE_DATA));
    return;
}

void validate_remove( VALIDATE_DATA *pValidate )
{
    if (!pValidate)
    {
	bbs_bug("Validate_remove: Null pValidate");
	return;
    }

    UNLINK(pValidate, first_validate, last_validate);
    free_validate(pValidate);
    save_validates( );
    return;
}

void do_validate( USER_DATA *usr, char *argument )
{
    char arg[INPUT];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("validate [show|send|remove] [argument]", usr);
	return;
    }

    if (!str_cmp(arg, "show"))
	show_validate(usr, argument);
    else if (!str_cmp(arg, "send"))
	send_validate(usr, argument);
    else if (!str_cmp(arg, "remove"))
	remove_validate(usr, argument);
    else
	syntax("validate [show|send|remove] [argument]", usr);

    return;
}

void show_validate( USER_DATA *usr, char *argument )
{
    VALIDATE_DATA *pValidate;
    BUFFER *output;
    char cBuf[STRING];
    char buf[INPUT];
    int vnum = 0;

    output	= new_buf( );
    buf[0]	= '\0';

    for (pValidate = first_validate; pValidate; pValidate = pValidate->next)
    {
	vnum++;

	sprintf(buf, "by %s", pValidate->sended_by);

	sprintf(cBuf, "--(%d)---------------------\n\r"
		      "Name     : %s (%s) Key: %d\n\r"
		      "From     : %s (%s)\n\r"
		      "Sended   : %s %s\n\r\n\r",
	    vnum, pValidate->name, pValidate->real_name, pValidate->key,
	    pValidate->host, time_str(pValidate->valid_time),
	    pValidate->sended_time > 0 ? "Yes" : "No",
	    pValidate->sended_time > 0 ? buf : "");
	add_buf(output, cBuf);
    }

    if (!vnum)
    {
	send_to_user("None.\n\r", usr);
	return;
    }

    page_to_user(buf_string(output), usr);
    free_buf(output);
    return;
}

void validate_mail( USER_DATA *usr, VALIDATE_DATA *pValidate )
{
    char mBuf[INPUT];
    char buf[INPUT];
    FILE *fp, *fpMail;

    if (!pValidate)
    {
	bbs_bug("Validate_mail: Null pValidate");
	return;
    }

    sprintf(buf, "%s%s.key", TEMP_DIR, capitalize(pValidate->name));
    fclose(fpReserve);
    if (!(fp = fopen(buf, "a")))
    {
	print_to_user(usr, "ERROR: Could not open to append %s.\n\r", buf);
	bbs_bug("Validate_mail: Could not open to append %s", buf);
	if (pValidate->sended_by)	free_string(pValidate->sended_by);
	pValidate->sended_time	= current_time;
	pValidate->sended_by	= str_dup("*Can't create key file*");
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    fprintf(fp, "From: %s\n"
		"To: %s\n"
		"Errors-To: %s\n"
		"Subject: %s BBS Validation Key\n\n"
		"Dear %s,\n\n"
		"Your username is: %s\n"
		"Your validation key is: %d\n\n----\n"
		"%s BBS - %s %d\n"
		"%s\n",
	config.bbs_email, pValidate->email, config.bbs_email, config.bbs_name,
	pValidate->real_name, pValidate->name, pValidate->key,
	config.bbs_email, config.bbs_host, config.bbs_port, config.bbs_email);
    fclose(fp);

    fpReserve = fopen(NULL_FILE, "r");
    sprintf(mBuf, "sendmail -t < %s", buf);
    if (!(fpMail = popen(mBuf, "r")))
    {
	bbs_bug("Validate_mail: Could not open to read %s", mBuf);
	send_to_user("ERROR: Could not execute mail command.\n\r", usr);
	return;
    }
    pclose(fpMail);
    if (pValidate->sended_by)	free_string(pValidate->sended_by);
    pValidate->sended_by	= str_dup(usr->name);
    pValidate->sended_time	= current_time;
    unlink(buf);
    print_to_user(usr, "Sending validation mail to %s.\n\r", pValidate->name);
    return;
}

void send_validate( USER_DATA *usr, char *argument )
{
    VALIDATE_DATA *pValidate;
    char arg[INPUT];
    int vnum = 0;
    bool found = FALSE;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("validate send [name|vnum]", usr);
	return;
    }

    if (is_number(arg))
	vnum = atoi(arg);

    for (pValidate = first_validate; pValidate; pValidate = pValidate->next)
    {
	if (vnum == 1 || !str_cmp(pValidate->name, arg))
	{
	    found = TRUE;
	    validate_mail(usr, pValidate);
	    break;
	}

	if (vnum > 1)
	    vnum--;
    }

    if (found)
	save_validates( );
    else
	send_to_user("No such validation log.\n\r", usr);

    return;
}

void remove_validate( USER_DATA *usr, char *argument )
{
    VALIDATE_DATA *pValidate;
    char arg[INPUT];
    int vnum = 0;
    bool found = FALSE;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("validate remove [name|vnum]", usr);
	return;
    }

    if (is_number(arg))
	vnum = atoi(arg);

    for (pValidate = first_validate; pValidate; pValidate = pValidate->next)
    {
	if (vnum == 1 || !str_cmp(pValidate->name, arg))
	{
	    found = TRUE;
	    print_to_user(usr, "Okay, removed %s from validation log.\n\r",
		pValidate->name);
	    validate_remove(pValidate);
	    break;
	}

	if (vnum > 1)
	    vnum--;
    }

    if (found)
	save_validates( );
    else
	send_to_user("No such validation log.\n\r", usr);

    return;
}

void do_echo( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    DESC_DATA *d;

    if (argument[0] == '\0')
    {
	syntax("echo [message]", usr);
	return;
    }

    sprintf(buf, "BBS Driver [%s]: %s#x\n\r", usr->name, argument);

    for (d = desc_list; d; d = d->next)
    {
	if (USR(d) && d->login == CON_LOGIN)
	{
	    if (isBusySelf(USR(d)))
		add_buffer(USR(d), buf);
	    else
		send_to_user(buf, USR(d));
	}
    }

    return;
}

void memory_validates( USER_DATA *pUser )
{
    VALIDATE_DATA *fValidate;
    int count = 0;

    for (fValidate = first_validate; fValidate; fValidate = fValidate->next)
	count++;

    print_to_user(pUser, "Valid: %4d -- %d bytes\n\r", count,
	count * (sizeof(*fValidate)));
}
