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

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "bbs.h"

/*
 * Local functions
 */
void	fread_banish		( FILE *fpBan, int bType );
void	free_banish		( BANISH_DATA *pBan );
void	dispose_banish		( BANISH_DATA *pBan, int type );
bool	check_expire		( BANISH_DATA *pBan );
void	save_banishes		( void );
void	do_quit_org		( USER_DATA *usr, char *argument, bool fXing );

/*
 * Global variables
 */
BANISH_DATA *	first_banish_user;
BANISH_DATA *	last_banish_user;
BANISH_DATA *	first_banish_site;
BANISH_DATA *	last_banish_site;

/*
 * Functions
 */
void load_banishes( void )
{
    char *word;
    FILE *fpBan;
    bool fMatch = FALSE;

    if (!(fpBan = fopen(BANISH_FILE, "r")))
    {
	bbs_bug("Load_banishes: Could not open to read %s", BANISH_FILE);
	return;
    }

    for (;;)
    {
	word	= feof(fpBan) ? "END" : fread_word(fpBan);
	fMatch	= FALSE;

	switch (UPPER(word[0]))
	{
	    case 'E':
		if (!str_cmp(word, "END"))
		{
		    fclose(fpBan);
		    log_string("Load_banishes: Done");
		    return;
		}

	    case 'S':
		if (!str_cmp(word, "SITE"))
		{
		    fread_banish(fpBan, BANISH_SITE);
		    fMatch = TRUE;
		}
		break;

	   case 'U':
		if (!str_cmp(word, "USER"))
		{
		    fread_banish(fpBan, BANISH_USER);
		    fMatch = TRUE;
		}
		break;
	}

	if (!fMatch)
	{
	    bbs_bug("Load_banishes: No match '%s'", word);
	    fread_to_eol(fpBan);
	}
    }
}

void fread_banish( FILE *fpBan, int bType )
{
    BANISH_DATA *pBan;

    pBan		= (BANISH_DATA *) alloc_mem(sizeof(BANISH_DATA));
    pBan->name		= fread_string(fpBan);
    pBan->duration	= fread_number(fpBan);
    pBan->date		= fread_number(fpBan);
    pBan->banned_by	= fread_string(fpBan);
    pBan->reason	= fread_string(fpBan);

    if (bType == BANISH_USER)
	LINK(pBan, first_banish_user, last_banish_user);
    else if (bType == BANISH_SITE)
	LINK(pBan, first_banish_site, last_banish_site);
    else
    {
	bbs_bug("Fread_banish: Bad type %d", bType );
	free_banish(pBan);
    }

    if (bType == BANISH_USER)
    {
	if (!is_user(pBan->name))
	{
	    sprintf(log_buf, "Load_banishes: No such user %s, removed",
		pBan->name);
	    log_string(log_buf);
	    dispose_banish(pBan, BANISH_USER);
	    save_banishes( );
	}
    }


    return;
}

void save_banishes( void )
{
    BANISH_DATA *pBan;
    FILE *fpBan;

    fclose(fpReserve);
    if (!(fpBan = fopen(BANISH_FILE, "w")))
    {
	bbs_bug("Save_banishes: Could not open to write %s", BANISH_FILE);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    for (pBan = first_banish_user; pBan; pBan = pBan->next)
    {
	fprintf(fpBan, "USER\n");
	fprintf(fpBan, "%s~\n%ld\n%d\n%s~\n%s~\n",pBan->name, pBan->duration,
		pBan->date, pBan->banned_by, pBan->reason);
    }

    for (pBan = first_banish_site; pBan; pBan = pBan->next)
    {
	fprintf(fpBan, "SITE\n");
	fprintf(fpBan, "%s~\n%ld\n%d\n%s~\n%s~\n",pBan->name, pBan->duration,
		pBan->date, pBan->banned_by, pBan->reason);
    }

    fprintf(fpBan, "END\n");
    fclose(fpBan);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

void add_banish( USER_DATA *usr, char *argument, int type )
{
    char arg1[INPUT];
    char arg2[INPUT];
    char buf[STRING];
    BANISH_DATA *pBan, *pTemp;
    USER_DATA *ban;
    int duration;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0' || argument[0] == '\0')
    {
	syntax("banish <user|site> <nickname> <duration> <reason>", usr);
	return;
    }

    if (!str_cmp(arg2, "perm"))
	duration = BANISH_PERM;
    else if (is_number(arg2))
    {
	duration = atoi(arg2);
	if (duration < 1 || duration > 999)
	{
	    send_to_user("Banish time must be 1 (day) to 999 (day).\n\r", usr);
	    return;
	}
    }
    else
    {
	syntax("banish <user|site> <nickname> <duration> <reason>", usr);
	return;
    }

    if (strlen(argument) > 20)
    {
	send_to_user("Reason too long.\n\r", usr);
	return;
    }

    smash_tilde(argument);

    switch (type)
    {
	case BANISH_USER:
	    if (!is_user(arg1))
	    {
		send_to_user("No such user.\n\r", usr);
		return;
	    }

	    for (pTemp = first_banish_user; pTemp; pTemp = pTemp->next)
	    {
		if (!str_cmp(pTemp->name, arg1))
		{
		    if (pTemp->duration == BANISH_PERM)
			sprintf(buf,
			    "%s is already banished permentaly (%s [%s]).\n\r",
			    pTemp->name, pTemp->reason, pTemp->banned_by);
		    else
			sprintf(buf,
			    "%s is already banished (%s [%s]).\n\r",
			    pTemp->name, pTemp->reason, pTemp->banned_by);
		    send_to_user(buf, usr);
		    return;
		}
	    }

	    pBan		=
		(BANISH_DATA *) alloc_mem(sizeof(BANISH_DATA));
	    pBan->name		= str_dup(capitalize(arg1));
	    pBan->banned_by	= str_dup(usr->name);
	    pBan->reason	= str_dup(argument);
	    if (duration == BANISH_PERM)
	    {
		pBan->duration	= BANISH_PERM;
		pBan->date	= BANISH_PERM;
	    }
	    else
	    {
		pBan->duration	= (int) current_time + (duration * 3600 * 24);
		pBan->date	= duration;
	    }
	    LINK(pBan, first_banish_user, last_banish_user);
	    if (duration == BANISH_PERM)
		sprintf(buf, "Ok, %s banished permentaly (%s).\n\r", pBan->name,
		    pBan->reason);
	    else
		sprintf(buf, "Ok, %s banished %d days (%s).\n\r", pBan->name,
		    duration, pBan->reason);
	    send_to_user(buf, usr);
	    if (pBan->duration == BANISH_PERM)
		sprintf(buf, "%s banishes %s permentaly (%s)",
		    pBan->banned_by, pBan->name, pBan->reason);
	    else
		sprintf(buf, "%s banished %s %d days (%s)", pBan->banned_by,
		    pBan->name, duration, pBan->reason);
	    log_file("banish", buf);
	    save_banishes( );
	    if ((ban = get_user(pBan->name)))
		do_quit_org(ban, "", FALSE);
	    break;

	case BANISH_SITE:
	    for (pTemp = first_banish_site; pTemp; pTemp = pTemp->next)
	    {
		if (!str_cmp(pTemp->name, arg1))
		{
		    if (pTemp->duration == BANISH_PERM)
			sprintf(buf,
			    "%s is already banished permentaly (%s [%s]).\n\r",
			    pTemp->name, pTemp->reason, pTemp->banned_by);
		    else
			sprintf(buf,
			    "%s is already banished (%s [%s]).\n\r",
			    pTemp->name, pTemp->reason, pTemp->banned_by);
		    send_to_user(buf, usr);
		    return;
		}
	    }

	    pBan		=
		(BANISH_DATA *) alloc_mem(sizeof(BANISH_DATA));
	    pBan->name		= str_dup(arg1);
	    pBan->banned_by	= str_dup(usr->name);
	    pBan->reason	= str_dup(argument);
	    if (duration == BANISH_PERM)
	    {
		pBan->duration	= BANISH_PERM;
		pBan->date	= BANISH_PERM;
	    }
	    else
	    {
		pBan->duration	= (int) current_time + (duration * 3600 * 24);
		pBan->date	= duration;
	    }
	    LINK(pBan, first_banish_site, last_banish_site);
	    if (duration == BANISH_PERM)
		sprintf(buf, "Ok, %s banished permentaly (%s).\n\r", pBan->name,
		    pBan->reason);
	    else
		sprintf(buf, "Ok, %s banished %d days (%s).\n\r", pBan->name,
		    duration, pBan->reason);
	    send_to_user(buf, usr);
	    if (pBan->duration == BANISH_PERM)
		sprintf(buf, "%s banishes %s permentaly (%s)",
		    pBan->banned_by, pBan->name, pBan->reason);
	    else
		sprintf(buf, "%s banished %s %d days (%s)", pBan->banned_by,
		    pBan->name, duration, pBan->reason);
	    log_file("banish", buf);
	    save_banishes( );
	    break;
    }

    return;
}

void do_banish( USER_DATA *usr, char *argument )
{
    char arg[INPUT_LENGTH];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
	syntax("banish <user|site> <nickname> <duration> <reason>", usr);
	return;
    }

    if (!str_cmp(arg, "user"))
    {
	add_banish(usr, argument, BANISH_USER);
	return;
    }
    else if (!str_cmp(arg, "site"))
    {
	add_banish(usr, argument, BANISH_SITE);
	return;
    }
    else
    {
	syntax("banish <user|site> <nickname> <duration> <reason>", usr);
	return;
    }
}

void do_unbanish( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    BANISH_DATA *pBan;
    int value = 0;
    bool found = FALSE;

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0')
    {
	syntax("unbanish <user|site> <nickname>", usr);
	return;
    }

    if (is_number(argument))
	value = atoi(argument);

    if (!str_cmp(arg, "user"))
    {
	for (pBan = first_banish_user; pBan; pBan = pBan->next)
	{
	    if (value == 1 || !str_cmp(pBan->name, argument))
	    {
		found = TRUE;
		sprintf(buf, "%s unbanishes %s (%s)", usr->name, pBan->name,
		    pBan->reason);
		log_file("banish", buf);
		dispose_banish(pBan, BANISH_USER);
		break;
	    }
	    if (value > 1)
		value--;
	}
    }
    else if (!str_cmp(arg, "site"))
    {
	for (pBan = first_banish_site; pBan; pBan = pBan->next)
	{
	    if (value == 1 || !str_cmp(pBan->name, argument))
	    {
		found = TRUE;
		sprintf(buf, "%s unbanishes %s (%s)", usr->name, pBan->name,
		    pBan->reason);
		log_file("banish", buf);
		dispose_banish(pBan, BANISH_SITE);
		break;
	    }
	    if (value > 1)
		value--;
	}
    }
    else
    {
	syntax("unbanish <user|site> <vnum>", usr);
	return;
    }

    if (found)
    {
	save_banishes( );
	sprintf(buf, "%s is now unbanished.\n\r", capitalize(argument));
	send_to_user(buf, usr);
    }
    else
    {
	sprintf(buf, "%s is not banished.\n\r", capitalize(argument));
	send_to_user(buf, usr);
    }

    return;
}

void free_banish( BANISH_DATA *pBan )
{
    if (!pBan)
	return;

    if (pBan->name)		free_string(pBan->name);
    if (pBan->banned_by)	free_string(pBan->banned_by);
    if (pBan->reason)		free_string(pBan->reason);

    free_mem(pBan, sizeof(BANISH_DATA));    
    return;
}

bool check_expire( BANISH_DATA *pBan )
{
    if (pBan->duration == BANISH_PERM)
	return FALSE;

    if (pBan->duration <= current_time)
	return TRUE;

    return FALSE;
}

void dispose_banish( BANISH_DATA *pBan, int type )
{
    if (!pBan)
	return;

    if (type != BANISH_USER && type != BANISH_SITE)
    {
	bbs_bug("Dispose_banish: Unknown bad type %d", type);
	return;
    }

    UNLINK(pBan, (type == BANISH_USER) ? first_banish_user : first_banish_site,
		 (type == BANISH_USER) ? last_banish_user : last_banish_site);
    free_banish(pBan);
    return;
}

bool check_banishes( USER_DATA *usr, int type )
{
    char buf[STRING_LENGTH];
    BANISH_DATA *pBan;
    int remove;
    bool fMatch = FALSE;

    switch (type)
    {
	case BANISH_USER:
	    pBan = first_banish_user;
	    break;

	case BANISH_SITE:
	    pBan = first_banish_site;
	    break;

	default:
	    bbs_bug("Check_banishes: Unknown bad type %d", type);
	    return FALSE;
    }

    for (; pBan; pBan = pBan->next)
    {
	if (type == BANISH_USER)
	{
	    if (!str_cmp(pBan->name, usr->name))
	    {
		if (pBan->duration == BANISH_PERM)
		{
		    sprintf(buf, "You are banished permantaly.\n\r"
				 "Due to: %s.\n\r", pBan->reason);
		    fMatch = TRUE;
		}
		else if (pBan->duration > current_time)
		{
		    remove = (int) pBan->duration - current_time;
		    sprintf(buf, "You are banished due to %s.\n\r"
				 "Removal date: %s\n\r",
			pBan->reason, get_age(remove, FALSE));
		    fMatch = TRUE;
		}
		else
		    fMatch = FALSE;
	    }

	    if (fMatch)
	    {
		send_to_user(buf, usr);
		return TRUE;
	    }
	    else
	    {
		if (check_expire(pBan))
		{
		    dispose_banish(pBan, BANISH_USER);
		    save_banishes( );
		    return FALSE;
		}
	    }
	}

	if (type == BANISH_SITE)
	{
	    if (usr->desc != NULL && !str_cmp(pBan->name, usr->desc->host))
	    {
	        if (pBan->duration == BANISH_PERM)
		{
		    sprintf(buf, "Your site has been banished permantaly.\n\r"
				 "Due to: %s.\n\r", pBan->reason);
		    fMatch = TRUE;
		}
		else if (pBan->duration > current_time)
		{
		    remove = (int) pBan->duration - current_time;
		    sprintf(buf, "Your site has been banished due to %s.\n\r"
				 "Removal date: %s\n\r",
			pBan->reason, get_age(remove, FALSE));
		    fMatch = TRUE;
		}
		else
		    fMatch = FALSE;
	    }

	    if (fMatch)
	    {
		send_to_user(buf, usr);
		return TRUE;
	    }
	    else
	    {
		if (check_expire(pBan))
		{
		    dispose_banish(pBan, BANISH_USER);
		    save_banishes( );
		    return FALSE;
		}
	    }
	}
    }

    return FALSE;
}

void show_banishes( USER_DATA *usr, int type )
{
    char bufA[STRING];
    char bufB[STRING];
    char dbuf[INPUT];
    BANISH_DATA *pBan;
    int vnum;

    strcpy(bufA, "Banished ");

    switch (type)
    {
	case BANISH_USER:
	    strcat(bufA, "users:\n\r");
	    pBan = first_banish_user;
	    for (vnum = 1; pBan; pBan = pBan->next, vnum++)
	    {
		sprintf(dbuf, "%3d day(s)", pBan->date);
		sprintf(bufB, "[%2d] %-12s %-11s %-20s\n\r", 
		    vnum, pBan->name, pBan->date == BANISH_PERM ?
		    "Permenantly" : dbuf, pBan->reason);
		strcat(bufA, bufB);
	    }
	    page_to_user(bufA, usr);
	    return;

	case BANISH_SITE:
	    strcat(bufA, "sites:\n\r");
	    pBan = first_banish_site;
	    for (vnum = 1; pBan; pBan = pBan->next, vnum++)
	    {
		sprintf(dbuf, "%3d day(s)", pBan->date);
		sprintf(bufB, "[%2d] %-30s %-11s %-20s\n\r", 
		    vnum, pBan->name, pBan->date == BANISH_PERM
		    ? "Permenantly" : dbuf, pBan->reason);
		strcat(bufA, bufB);
	    }
	    page_to_user(bufA, usr);
	    return;

	default:
	    bbs_bug("Show_banishes: Unknown bad type %d", type);
	    return;
    }
}

void do_banishes( USER_DATA *usr, char *argument )
{
    if (IS_ADMIN(usr))
    {
	show_banishes(usr, BANISH_USER);
	show_banishes(usr, BANISH_SITE);
	return;
    }
    else
    {
	show_banishes(usr, BANISH_USER);
	return;
    }
}

/*
 * End of banish.c
 */

void memory_banishes( USER_DATA *pUser )
{
    BANISH_DATA *fBanishUser;
    BANISH_DATA *fBanishSite;
    int count_user, count_site;

    count_user = count_site = 0;

    for (fBanishUser = first_banish_user; fBanishUser;
	 fBanishUser = fBanishUser->next)
	count_user++;

    for (fBanishSite = first_banish_site; fBanishSite;
	 fBanishSite = fBanishSite->next)
	count_site++;

    print_to_user(pUser, "Banish (User): %4d -- %d bytes\n\r",
	count_user, count_user * (sizeof(*fBanishUser)));
    print_to_user(pUser, "Banish (Site): %4d -- %d bytes\n\r",
	count_site, count_site * (sizeof(*fBanishSite)));
}

void banish_update( void )
{
    BANISH_DATA *pBan;

    for (pBan = first_banish_user; pBan; pBan = pBan->next)
    {
	if (check_expire(pBan) && pBan->type == BANISH_USER)
	{
	    dispose_banish(pBan, BANISH_USER);
	    save_banishes( );
	    return;
	}
    }

    for (pBan = first_banish_site; pBan; pBan = pBan->next)
    {
	if (check_expire(pBan) && pBan->type == BANISH_SITE)
	{
	    dispose_banish(pBan, BANISH_SITE);
	    save_banishes( );
	    return;
	}
    }
}

