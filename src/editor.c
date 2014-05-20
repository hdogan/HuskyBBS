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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bbs.h"

void string_edit( USER_DATA *usr, char **pString )
{
    if (EDIT_MODE(usr) != EDITOR_PLAN)
	send_to_user("'~h' for help. '**' to quit and save. '~q' to quit.\n\r"
		     "Write a message,\n\r", usr);
    EDIT_LINE(usr) = 1;

    if (*pString == NULL)
    {
	*pString = str_dup("");
    }
    else
    {
	**pString = '\0';
    }

    usr->desc->pString = pString;
    return;
}

void string_append( USER_DATA *usr, char **pString )
{
    if (*pString == NULL)
    {
	*pString = str_dup("");
    }

    send_to_user(*pString, usr);
    
    if (*(*pString + strlen(*pString) - 1) != '\r')
	send_to_user("\n\r", usr);

    usr->desc->pString = pString;
    return;
}

char * string_replace( char *orig, char *old, char *new )
{
    char buf[STRING_LENGTH];
    int i;

    buf[0] = '\0';
    strcpy(buf, orig);

    if (strstr(orig, old) != NULL)
    {
	i = strlen(orig) - strlen(strstr(orig, old));
	buf[i] = '\0';
	strcat(buf, new);
	strcat(buf, &orig[i+strlen(old)]);
	if (orig)
	    free_string(orig);
    }

    return str_dup(buf);
}

char * string_insert( USER_DATA *usr, char *orig, int line, char *add )
{
    char buf[STRING_LENGTH];
    char *rdesc;
    int current_line = 1;
    int i = 0;

    buf[0] = '\0';
    strcpy(buf, orig);

    for (rdesc = orig; *rdesc; rdesc++)
    {
	if (current_line == line)
	    break;

	i++;

	if (*rdesc == '\r')
	    current_line++;
    }

    if (!*rdesc)
    {
	send_to_user("That line does not exist.\n\r", usr);
	return str_dup(buf);
    }

    buf[i] = '\0';

    if (*add)
	strcat(buf, add);

    strcat(buf, &orig[i]);
    if (orig)
	free_string(orig);
    send_to_user("Line inserted.\n\r", usr);
    EDIT_LINE(usr)++;
    return str_dup(buf);
}

char * string_delete( char *orig, int line )
{
    char buf[STRING_LENGTH];
    char *rdesc;
    int current_line = 1;
    int i = 0;

    buf[0] = '\0';

    for (rdesc = orig; *rdesc; rdesc++)
    {
	if (current_line != line)
	{
	    buf[i] = *rdesc;
	    i++;
	}

	if (*rdesc == '\r')
	    current_line++;
    }

    if (orig)
	free_string(orig);
    buf[i] = 0;
    return str_dup(buf);
}

void string_add( USER_DATA *usr, char *argument )
{
    char buf[STRING_LENGTH];

    usr->timer = 0;

    if (*argument == '~')
    {
	char arg1[INPUT_LENGTH];
	char arg2[INPUT_LENGTH];
	char arg3[INPUT_LENGTH];

	argument = one_argument(argument, arg1);
	argument = first_arg(argument, arg2, FALSE);
	argument = first_arg(argument, arg3, FALSE);

	if (!str_cmp(arg1, "~c"))
	{
	    send_to_user("String cleared.\n\r", usr);
	    **usr->desc->pString = '\0';
	    return;
	}

	if (!str_cmp(arg1, "~s"))
	{
	    send_to_user("String so far:\n\r", usr);
	    send_to_user(*usr->desc->pString, usr);
	    return;
	}

	if (!str_cmp(arg1, "~r"))
	{
	    if (arg2[0] == '\0')
	    {
		syntax("~r \"old string\" \"new string\"", usr);
		return;
	    }

	    smash_tilde(arg2);
	    smash_tilde(arg3);
	    *usr->desc->pString =
		string_replace(*usr->desc->pString, arg2, arg3);
	    sprintf(buf, "'%s' replaced with '%s'.\n\r", arg2, arg3);
	    send_to_user(buf, usr);
	    return;
	}

	if (!str_cmp(arg1, "~f"))
	{
	    *usr->desc->pString = format_string(*usr->desc->pString);
	    send_to_user("String formatted.\n\r", usr);
	    return;
	}
        
	if (!str_cmp(arg1, "~q"))
	{
	    if (EDIT_MODE(usr) == EDITOR_NOTE)
		edit_note_free(usr);
	    if (EDIT_MODE(usr) == EDITOR_MAIL_WRITE)
		edit_mail_free(usr);
	    **usr->desc->pString = '\0';
	    usr->desc->pString   = NULL;
	    if (EDIT_MODE(usr) == EDITOR_PLAN)
	    {
		if (usr->plan) free_string(usr->plan);
		usr->plan = usr->old_plan ? str_dup(usr->old_plan) :
			    str_dup("*ERROR*");
	    }
	    EDIT_MODE(usr) = EDITOR_NONE;
	    send_to_user("Aborted.\n\r", usr);
	    return;
	}

	if (!str_cmp(arg1, "~i"))
	{
	    if (arg2[0] == '\0' || !is_number(arg2))
	    {
		syntax("~i \"line\" \"text\"", usr);
		return;
	    }

	    smash_tilde(arg2);
	    smash_tilde(arg3);
	    *usr->desc->pString =
		string_insert(usr, *usr->desc->pString, atoi(arg2), arg3);
	    return;
	}

	if (!str_cmp(arg1, "~d"))
	{
	    if (arg2[0] == '\0' || !is_number(arg2))
	    {
		syntax("~d \"line\"", usr);
		return;
	    }

	    smash_tilde(arg2);
	    smash_tilde(arg3);
	    *usr->desc->pString =
		string_delete(*usr->desc->pString, atoi(arg2));
	    sprintf(buf, "Line %d deleted.\n\r", atoi(arg2));
	    send_to_user(buf, usr);
	    return;
	}
	    
	if (!str_cmp(arg1, "~h"))
	{
	    send_to_user("\n\rEditor help (commands on blank line):\n\r", usr);
	    send_to_user("~r 'old' 'new'   - replace a substring \n\r", usr);
	    send_to_user("                   (requires '', \"\") \n\r", usr);
	    send_to_user("~h               - get help (this info)\n\r", usr);
	    send_to_user("~s               - show string so far  \n\r", usr);
	    send_to_user("~d 'line'        - deletes a line      \n\r", usr);
	    send_to_user("~f               - (word wrap) string  \n\r", usr);
	    send_to_user("~c               - clear string so far \n\r", usr);
	    send_to_user("~i 'line' 'text' - inserts a line      \n\r", usr);
	    send_to_user("~q               - quit, do not save   \n\r", usr);
	    send_to_user("**               - save and quit     \n\r\n\r", usr);
	    return;
	}
            
	send_to_user("Invalid dot command.\n\r", usr);
	return;
    }

    if (!str_cmp(argument, "**"))
    {
	if (EDIT_MODE(usr) == EDITOR_NOTE)
	    edit_send_note(usr);
	if (EDIT_MODE(usr) == EDITOR_MAIL_WRITE)
	    edit_mail_send(usr);
	if (EDIT_MODE(usr) == EDITOR_INFO)
	    save_boards( );
	if (EDIT_MODE(usr) == EDITOR_PLAN)
	{
	    if (EDIT_LINE(usr) == 1)
		send_to_user("Your plan is now empty.\n\r", usr);
	    else
		send_to_user("Your plan has been set.\n\r", usr);
	}
	else
	    send_to_user("You finish writing.\n\r", usr);
	usr->desc->pString = NULL;
	EDIT_MODE(usr) = EDITOR_NONE;
	return;
    }

    strcpy(buf, *usr->desc->pString);

    if (strlen(*usr->desc->pString) + strlen(argument) >= (STRING_LENGTH - 4))
    {
	send_to_user("String too long, last line skipped.\n\r", usr);
	usr->desc->pString = NULL;
	return;
    }

    smash_tilde(argument);
    strcat(buf, argument);
    strcat(buf, "\n\r");
    free_string(*usr->desc->pString);
    *usr->desc->pString = str_dup(buf);
    EDIT_LINE(usr)++;

    if (strlen(argument) > 80)
	EDIT_LINE(usr)++;

    if (EDIT_LINE(usr) > 5 && EDIT_MODE(usr) == EDITOR_PLAN)
    {
	usr->desc->pString = NULL;
	EDIT_MODE(usr) = EDITOR_NONE;
	send_to_user("Your plan has been set.\n\r", usr);
	return;
    }

    if (EDIT_LINE(usr) > 100)
    {
	if (EDIT_MODE(usr) == EDITOR_NOTE)
	    edit_send_note(usr);
	if (EDIT_MODE(usr) == EDITOR_MAIL_WRITE)
	    edit_mail_send(usr);
	if (EDIT_MODE(usr) == EDITOR_INFO)
	    save_boards( );
	usr->desc->pString = NULL;
	EDIT_MODE(usr) = EDITOR_NONE;
	send_to_user("You finish writing.\n\r", usr);
	return;
    }

    return;
}

char * format_string( char *oldstring )
{
    char buf1[STRING_LENGTH];
    char buf2[STRING_LENGTH];
    char *rdesc;
    int i = 0;
    bool cap = TRUE;
  
    buf1[0] = buf2[0] = 0;
    i = 0;
  
    for (rdesc = oldstring; *rdesc; rdesc++)
    {
	if (*rdesc == '\n')
	{
	    if (buf1[i-1] != ' ')
	    {
		buf1[i] = ' ';
		i++;
	    }
	}
	else if (*rdesc == '\r')
	    ;
	else if (*rdesc == ' ')
	{
	    if (buf1[i-1] != ' ')
	    {
		buf1[i] = ' ';
		i++;
	    }
	}
	else if (*rdesc == ')')
	{
	    if (buf1[i-1] == ' ' && buf1[i-2] == ' '
	    && (buf1[i-3] == '.' || buf1[i-3] == '?'
	    || buf1[i-3] == '!'))
	    {
		buf1[i-2] = *rdesc;
		buf1[i-1] = ' ';
		buf1[i] = ' ';
		i++;
	    }
	    else
	    {
		buf1[i] = *rdesc;
		i++;
	    }
	}
	else if (*rdesc == '.' || *rdesc == '?' || *rdesc == '!')
	{
	    if (buf1[i-1] == ' ' && buf1[i-2] == ' '
	    && (buf1[i-3] == '.' || buf1[i-3] == '?'
	    || buf1[i-3] == '!'))
	    {
		buf1[i-2] = *rdesc;
		if (*(rdesc+1) != '\"')
		{
		    buf1[i-1] = ' ';
		    buf1[i] = ' ';
		    i++;
		}
		else
		{
		    buf1[i-1] = '\"';
		    buf1[i] = ' ';
		    buf1[i+1] = ' ';
		    i += 2;
		    rdesc++;
		}
	    }
	    else
	    {
		buf1[i] = *rdesc;
		if (*(rdesc+1) != '\"')
		{
		    buf1[i+1] = ' ';
		    buf1[i+2] = ' ';
		    i += 3;
		}
		else
		{
		    buf1[i+1] = '\"';
		    buf1[i+2] = ' ';
		    buf1[i+3] = ' ';
		    i += 4;
		    rdesc++;
		}
	    }

	    cap = TRUE;
	}
	else
	{
	    buf1[i] = *rdesc;
	    if (cap)
	    {
		cap = FALSE;
		buf1[i] = UPPER(buf1[i]);
	    }

	    i++;
	}
    }

    buf1[i] = 0;
    strcpy(buf2, buf1);
    rdesc = buf2;
    buf1[0] = 0;
  
    for (;;)
    {
	for (i = 0; i < 77; i++)
	{
	    if (!*(rdesc+i))
		break;
	}

	if (i < 77)
	{
	    break;
	}

	for (i = (buf1[0] ? 76 : 73); i; i--)
	{
	    if (*(rdesc+i) == ' ')
		break;
	}

	if (i)
	{
	    *(rdesc+i) = 0;
	    strcat(buf1, rdesc);
	    strcat(buf1, "\n\r");
	    rdesc += i+1;

	    while (*rdesc == ' ')
		rdesc++;
	}
	else
	{
	    bug("Format_string: No spaces", 0);
	    *(rdesc+75) = 0;
	    strcat(buf1, rdesc);
	    strcat(buf1, "-\n\r");
	    rdesc += 76;
	}
    }

    while (*(rdesc+i) && (*(rdesc+i) == ' '
    || *(rdesc+i) == '\n' || *(rdesc+i) == '\r'))
	i--;

    *(rdesc+i+1) = 0;
    strcat(buf1, rdesc);

    if (buf1[strlen(buf1)-2] != '\n')
	strcat(buf1, "\n\r");

    free_string(oldstring);
    return (str_dup(buf1));
}

char * first_arg( char *argument, char *arg_first, bool fCase )
{
    char cEnd;

    while (*argument == ' ')
	argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"'
    || *argument == '%' || *argument == '(')
    {
	if (*argument == '(')
	{
	    cEnd = ')';
	    argument++;
	}
	else
	    cEnd = *argument++;
    }

    while (*argument != '\0')
    {
	if (*argument == cEnd)
	{
	    argument++;
	    break;
	}

	if (fCase)
	    *arg_first = LOWER(*argument);
	else
	    *arg_first = *argument;

	arg_first++;
	argument++;
    }

    *arg_first = '\0';

    while (*argument == ' ')
	argument++;

    return argument;
}

