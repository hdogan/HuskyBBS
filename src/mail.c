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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "bbs.h"

char *	time_str		( time_t last_time );
void	save_mail		( USER_DATA *usr );
void	mail_attach		( USER_DATA *usr );

int count_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    int count = 0;

    for (pMail = usr->pMailFirst; pMail != NULL; pMail = pMail->next)
	if (pMail)
	    count++;

    if (count < 0)
	count = 0;

    return (count);
}

int unread_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    int count = 0;

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
	if (pMail && pMail->stamp_time > pMail->read_time)
	    count++;
    }

    if (count < 0)
	count = 0;

    return (count);
}

void func_rnew_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    char buf[STRING];
    BUFFER *buffer;
    int vnum = 1;

    buffer    = new_buf();

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
	if (pMail->stamp_time <= pMail->read_time)
	    break;
    }

    if (pMail)
    {
	time_t *last_read = &pMail->read_time;
	*last_read = UMAX(*last_read, pMail->stamp_time);

	sprintf(buf, "Reading new message %d.\n\rDate: %s\n\rFrom: %s\n\r"
		     "Subject: %s\n\r\n\r", vnum, 
	    time_str(pMail->stamp_time), pMail->from, pMail->subject);
	add_buf(buffer, buf);
	add_buf(buffer, pMail->message);
	add_buf(buffer, "#x\n\r");
	page_to_user(buf_string(buffer), usr);
	free_buf(buffer);
	save_mail(usr);
	return;
    }

    send_to_user("No new messages.\n\r", usr);
    return;
}

void func_read_mail( USER_DATA *usr, char *argument )
{
    MAIL_DATA *pMail;
    char buf[STRING];
    BUFFER *buffer;
    int vnum = 1;
    int anum = 0;

    if (argument[0] == '\0' || !is_number(argument))
    {
	syntax("[#Wre#x]ad <mail number>", usr);
	return;
    }

    anum   = atoi(argument);
    buffer = new_buf();

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
	if (pMail && (vnum++ == anum))
	{
	    time_t *last_read = &pMail->read_time;
	    *last_read = UMAX(*last_read, pMail->stamp_time);

	    sprintf(buf, "Reading message %d.\n\rDate: %s\n\rFrom: %s\n\r"
			 "Subject: %s\n\r\n\r", vnum - 1, 
		time_str(pMail->stamp_time), pMail->from, pMail->subject);
	    add_buf(buffer, buf);
	    add_buf(buffer, pMail->message);
	    add_buf(buffer, "#x\n\r");
	    page_to_user(buf_string(buffer), usr);
	    free_buf(buffer);
	    save_mail(usr);
	    return;
	}
    }

    send_to_user("There aren't that many mail.\n\r", usr);
    return;
}

void func_reply_mail( USER_DATA *usr, char *argument )
{
    MAIL_DATA *pMail;
    char buf[STRING];
    BUFFER *buffer;
    int vnum = 1;
    int anum = 0;

    if (argument[0] == '\0' || !is_number(argument))
    {
        syntax("[#Wr#x]eply <mail number>", usr);
        return;
    }

    anum   = atoi(argument);
    buffer = new_buf();

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
        if (pMail && (vnum++ == anum))
	    break;
    }    

    mail_attach(usr);
    if (usr->pCurrentMail->to)
 	free_string(usr->pCurrentMail->to);
    usr->pCurrentMail->to = str_dup(pMail->from);
    if (usr->pCurrentMail->subject)
 	free_string(usr->pCurrentMail->subject);
    if (pMail->subject[0] == 'R' && pMail->subject[1] == 'e'
    && pMail->subject[2] == ':' && pMail->subject[3] == ' ')
	usr->pCurrentMail->subject = str_dup(pMail->subject);
    else
    {
	sprintf(buf, "Re: %s", pMail->subject);
	usr->pCurrentMail->subject = str_dup(buf);
    }
    print_to_user(usr, "To: %s\n\rSubject: %s\n\r", 
	usr->pCurrentMail->to, usr->pCurrentMail->subject);
    EDIT_MODE(usr) = EDITOR_MAIL_WRITE;
    string_edit(usr, &usr->pCurrentMail->message);
}

void func_list_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    char buf[STRING];
    BUFFER *buffer;
    int vnum = 0;

    buffer = new_buf();

    add_buf(buffer, "  Num Date            From            Subject\n\r");
    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
	if (pMail)
	{
	    sprintf(buf, "%s %3d %s %-15s %s\n\r",
		pMail->marked ? "M" :
		(pMail->stamp_time > pMail->read_time) ? "N" : " ",
		vnum + 1, time_str(pMail->stamp_time), pMail->from,
		pMail->subject);
	    add_buf(buffer, buf);
	    vnum++;
	}
    }

    if (vnum > 0)
    {
	page_to_user(buf_string(buffer), usr);
	free_buf(buffer);
	return;
    }

    send_to_user("You don't have any mail.\n\r", usr);
    free_buf(buffer);
    return;
}

void func_delete_mail( USER_DATA *usr, char *argument )
{
    MAIL_DATA *pMail;
    int vnum = 1;
    int anum = 0;

    if (argument[0] == '\0' || !is_number(argument))
    {
	syntax("[#Wd#x]elete <mail number>", usr);
	return;
    }

    anum   = atoi(argument);

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
	if (pMail && (vnum++ == anum))
	{
	    if (pMail->marked)
	    {
		send_to_user(
		    "This message is already marked for deletion.\n\r", usr);
		return;
	    }

	    print_to_user(usr, "Message %d marked for deletion.\n\r", vnum - 1);
	    pMail->marked = TRUE;
	    return;
	}
    }

    send_to_user("There aren't that many mail.\n\r", usr);
    return;
}

void func_quit_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    int marked = 0;
    int count  = 0;

    edit_mail_free(usr);

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
    {
	if (pMail && pMail->marked)
	{
	    marked++;
	    mail_remove(usr, pMail, TRUE);
	}
    }

    
    count = count_mail(usr) - marked;
    if (count < 1)
	count = 0;

    if (marked > 0)
	print_to_user(usr, "Keeping %d messages and deleting %d.\n\r", count,
	    marked);
    EDIT_MODE(usr) = EDITOR_NONE;
    return;
}

MAIL_DATA *read_mail( FILE *fpMail )
{
    MAIL_DATA *pMail;
    char *word = '\0';
    bool fMatch = FALSE;
    char letter;

    do
    {
	letter = getc(fpMail);
	if (feof(fpMail))
	{
	    fclose(fpMail);
	    return NULL;
	}
    }
    while (isspace(letter));
    ungetc(letter, fpMail);

    pMail		= (MAIL_DATA *) alloc_mem(sizeof(MAIL_DATA));

    for (;;)
    {
	word	= feof(fpMail) ? "End" : fread_word(fpMail);
	fMatch	= FALSE;

	switch (UPPER(word[0]))
	{
	    case '*':
		fMatch = TRUE;
		fread_to_eol(fpMail);
		break;

	    case 'E':
		if (!str_cmp(word, "End"))
		{
		    pMail->next = NULL;
		    pMail->prev = NULL;
		    return pMail;
		}
		break;

	    case 'F':
		KEY( "From",	pMail->from,		fread_string(fpMail) );
		break;

	    case 'M':
		KEY( "Message",	pMail->message,		fread_string(fpMail) );
		break;

	    case 'R':
		KEY( "Read",	pMail->read_time,	fread_number(fpMail) );
		break;

	    case 'S':
		KEY( "Stamp",	pMail->stamp_time,	fread_number(fpMail) );
		KEY( "Subject",	pMail->subject,		fread_string(fpMail) );
		break;
	}

	if (!fMatch)
	{
	    bbs_bug("Read_mail: No match '%s'", word);
	    exit(1);
	    return NULL;
	}
    }

    return pMail;
}

void load_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    char buf[INPUT];
    FILE *fpMail;

    usr->pMailFirst = NULL;
    usr->pMailLast  = NULL;

    sprintf(buf, "%s%s", MAIL_DIR, capitalize(usr->name));
    if (!(fpMail = fopen(buf, "r")))
    {
	bbs_bug("Load_mail: Could not open to read %s", buf);
	return;
    }

    while ((pMail = read_mail(fpMail)) != NULL)
	LINK(pMail, usr->pMailFirst, usr->pMailLast);

    return;
}

void append_mail( MAIL_DATA *pMail, FILE *fpMail )
{
    if (!pMail)
    {
	bbs_bug("Append_mail: Null pMail");
	return;
    }

    fprintf(fpMail, "Stamp    %ld\n",	(iptr) pMail->stamp_time );
    fprintf(fpMail, "Read     %ld\n",	(iptr) pMail->read_time );
    fprintf(fpMail, "From     %s~\n",	pMail->from	);
    fprintf(fpMail, "Subject  %s~\n",	pMail->subject	);
    fprintf(fpMail, "Message\n%s~\n",	pMail->message	);
    fprintf(fpMail, "End\n" );
}


void save_mail( USER_DATA *usr )
{
    MAIL_DATA *pMail;
    FILE *fpMail;
    char buf[INPUT];

    sprintf(buf, "%s%s", MAIL_DIR, capitalize(usr->name));
    fclose(fpReserve);

    if (!(fpMail = fopen(buf, "w")))
    {
	send_to_user("ERROR: Could not open to write your mail file.\n\r", usr);
	bbs_bug("Save_mail: Could not open to write %s", buf);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    for (pMail = usr->pMailFirst; pMail; pMail = pMail->next)
	if (pMail)
	    append_mail(pMail, fpMail);

    fclose(fpMail);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

void free_mail( MAIL_DATA *pMail )
{
    if (!pMail)
    {
	bbs_bug("Free_mail: Null pMail");
	return;
    }

/*    if (pMail->subject)	free_string(pMail->subject);
    if (pMail->message)	free_string(pMail->message);
    if (pMail->from)	free_string(pMail->from);
    if (pMail->to)	free_string(pMail->to); */

    free_mem(pMail, sizeof(MAIL_DATA));
    return;
}

void mail_remove( USER_DATA *usr, MAIL_DATA *pMail, bool fSave )
{
    if (!pMail)
    {
	send_to_user("ERROR: Null pMail.\n\r", usr);
	bbs_bug("Mail_remove: Null pMail");
	return;
    }

    UNLINK(pMail, usr->pMailFirst, usr->pMailLast);
    free_mail(pMail);
    if (fSave)
	save_mail(usr);
    return;
}

void mail_attach( USER_DATA *usr )
{
    MAIL_DATA *pMail;

    if (usr->pCurrentMail)
	return;

    pMail		= (MAIL_DATA *) alloc_mem(sizeof(MAIL_DATA));
    pMail->prev		= NULL;
    pMail->next		= NULL;
    pMail->from		= str_dup(usr->name);
    pMail->to		= str_dup("None");
    pMail->subject	= str_dup("Undefined subject");
    pMail->stamp_time	= current_time;
    pMail->read_time	= 0;
    pMail->message	= str_dup("");
    pMail->marked	= FALSE;
    usr->pCurrentMail	= pMail;
    return;
}

void edit_mail_free( USER_DATA *usr )
{
    if (usr->pCurrentMail)
    {
	free_mail(usr->pCurrentMail);
	usr->pCurrentMail = NULL;
    }
}

void edit_mail_send( USER_DATA *usr )
{
    USER_DATA *to;
    FILE *fpMail;
    char buf[INPUT];
     
    if (!usr->pCurrentMail)
    {
	send_to_user("ERROR: Null usr->pCurrentMail.\n\r", usr);
	bbs_bug("Edit_mail_send: Null usr->pCurrentMail");
	return;
    }

    if ((to = get_user(usr->pCurrentMail->to)) != NULL) /* if user is online */
    {
	LINK(usr->pCurrentMail, to->pMailFirst, to->pMailLast);
	save_mail(to);
/*	edit_mail_free(usr); BAXTER */
	usr->pCurrentMail = NULL;
	if (isBusySelf(to))
	    add_buffer(to, "#WYou have new mail.#x\n\r");
	else
	    send_to_user("#WYou have new mail.#x\n\r", to);
	return;
    }

    sprintf(buf, "%s%s", MAIL_DIR, capitalize(usr->pCurrentMail->to));
    fclose(fpReserve);
        
    if (!(fpMail = fopen(buf, "a")))
    {
        print_to_user(usr, "ERROR: Could not open to write %s's mail file.\n\r",
	    capitalize(usr->pCurrentMail->to));
        bbs_bug("Save_mail: Could not open to write %s", buf);
        fpReserve = fopen(NULL_FILE, "r");
/*	edit_mail_free(usr); BAXTER */
	usr->pCurrentMail = NULL;
        return;
    }

    append_mail(usr->pCurrentMail, fpMail);
    fclose(fpMail);
    fpReserve = fopen(NULL_FILE, "r");
/*    edit_mail_free(usr); */
    usr->pCurrentMail = NULL;
    return;
}

void edit_mail_mode( USER_DATA *usr, char *argument )
{
    char arg[INPUT];

    while (isspace(*argument))
	argument++;

    smash_tilde(argument);
    usr->timer = 0;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	func_rnew_mail(usr);
	return;
    }
    else if (!str_cmp(arg, "?") || !str_cmp(arg, "h"))
    {
	do_help(usr, "MAIL_DATA-INDEX");
	return;
    }
    else if (!str_cmp(arg, "l"))
    {
	func_list_mail(usr);
	return;
    }
    else if (!str_cmp(arg, "r"))
    {
	func_reply_mail(usr, argument);
	return;
    }
    else if (!str_cmp(arg, "d"))
    {
	func_delete_mail(usr, argument);
	return;
    }
    else if (!str_cmp(arg, "q"))
    {
	func_quit_mail(usr);
	return;
    }
    else if (!str_cmp(arg, "c"))
    {
	if (argument[0] == '\0')
	{
	    syntax("[#Wc#x]ompose <user name>", usr);
	    return;
	}

	if (!is_user(argument))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	if (is_enemy(usr, argument))
	{
	    send_to_user("You can't sent mail to your enemies.\n\r", usr);
	    return;
	}

	mail_attach(usr);
	if (usr->pCurrentMail->to)
	    free_string(usr->pCurrentMail->to);
	usr->pCurrentMail->to = str_dup(argument);
	EDIT_MODE(usr)	      = EDITOR_MAIL_SUBJECT;
	return;
    }
    else if (is_number(arg))
    {
	func_read_mail(usr, arg);
	return;
    }
    else
    {
	send_to_user("Unknown mail command, try '?' in order to show help.\n\r",
	    usr);
	return;
    }
}

void edit_mail_subject( USER_DATA *usr, char *argument )
{
    while (isspace(*argument))
	argument++;

    usr->timer = 0;

    mail_attach(usr);
    smash_tilde(argument);

    if (argument[0] == '\0')
    {
/*	edit_mail_free(usr); BAXTER */
	EDIT_MODE(usr) = EDITOR_NONE;
	return;
    }
    else if (strlen(argument) > 20)
    {
	send_to_user("Subject too long.\n\r", usr);
	return;
    }

    if (usr->pCurrentMail->subject)
	free_string(usr->pCurrentMail->subject);
    usr->pCurrentMail->subject = str_dup(argument);
    EDIT_MODE(usr) = EDITOR_MAIL_WRITE;
    string_edit(usr, &usr->pCurrentMail->message);
}

void do_mail( USER_DATA *usr, char *argument )
{
    char arg[INPUT];

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	print_to_user(usr, "\n\rTotal messages: %-3d\n\r\n\r", count_mail(usr));
	do_help(usr, "MAIL_DATA-INDEX");
	EDIT_MODE(usr) = EDITOR_MAIL;
	return;
    }

    if (!is_user(arg))
    {
	send_to_user("No such user.\n\r", usr);
	return;
    }

    if (is_enemy(usr, arg))
    {
	send_to_user("You can't sent mail to your enemies.\n\r", usr);
	return;
    }

    mail_attach(usr);
    if (usr->pCurrentMail->to)
	free_string(usr->pCurrentMail->to);
    usr->pCurrentMail->to = str_dup(arg);
    EDIT_MODE(usr)	  = EDITOR_MAIL_SUBJECT;
}

void do_unread_mail( USER_DATA *usr )
{
    if (unread_mail(usr) > 0)
	print_to_user(usr, "#YMail: %d new mail%s.#x\n\r\n\r",
	    unread_mail(usr), unread_mail(usr) > 1 ? "s" : "");
    else
	send_to_user("Mail: No new mails.\n\r\n\r", usr);

    if (!IS_TOGGLE(usr, TOGGLE_XING))
	send_to_user("You toggle message eXpress Off!\n\r", usr);

    if (IS_TOGGLE(usr, TOGGLE_IDLE))
	send_to_user("You toggle idle mode On!\n\r", usr);
}

void finger_mail( USER_DATA *usr, char *name )
{
    MAIL_DATA *pMail;
    MAIL_DATA *fMailFirst;
    MAIL_DATA *fMailLast;
    USER_DATA *to;
    char buf[INPUT];
    FILE *fpMail;
    int count = 0;
    int new_count = 0;

    if (!(to = get_user(name)))
    {
	if (!is_user(name))
	{
	    bbs_bug("Finger_mail: No such user %s", name);
	    send_to_user("ERROR: No such user.\n\r", usr);
	    return;
	}
	fMailFirst = NULL;
	fMailLast  = NULL;

	sprintf(buf, "%s%s", MAIL_DIR, capitalize(name));
	if (!(fpMail = fopen(buf, "r")))
	{
	    bbs_bug("Finger_mail: Could not open to read %s", buf);
/*	    send_to_user("ERROR: Could not open mail file.\n\r", usr);
 BAXTER */
	    send_to_user("Mail: No new messages.\n\r\n\r", usr);
	    return;
	}

	while ((pMail = read_mail(fpMail)) != NULL)
	    LINK(pMail, fMailFirst, fMailLast);

	for (pMail = fMailFirst; pMail; pMail = pMail->next)
	{
	    if (pMail)
	    {
		if (pMail->stamp_time > pMail->read_time)
		    new_count++;

		count++;
	    }
	}

	if (new_count > 0)
	    sprintf(buf, "Mail: %d new message%s.\n\r\n\r", new_count,
		new_count > 1 ? "s" : "");
	else
	    sprintf(buf, "Mail: No new messages.\n\r\n\r");
	send_to_user(buf, usr);
	return;
    }

    if (unread_mail(to) > 0)
	sprintf(buf, "Mail: %d new message%s.\n\r\n\r", unread_mail(to),
	    unread_mail(to) > 1 ? "s" : "");
    else
	sprintf(buf, "Mail: No new messages.\n\r\n\r");
    send_to_user(buf, usr);
    return;
}

void do_from( USER_DATA *usr, char *argument )
{
    func_list_mail( usr );    
    return;
}

