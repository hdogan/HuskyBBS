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
#include <time.h>

#include "bbs.h"

void	friend_remove	( USER_DATA *usr, char *argument, bool fAll );
void	notify_remove	( USER_DATA *usr, char *argument, bool fAll );
void	enemy_remove	( USER_DATA *usr, char *argument, bool fAll );
void	ignore_remove	( USER_DATA *usr, char *argument, bool fAll );

BOARD_DATA * moderator_board( USER_DATA *usr );

/* does aliasing and other fun stuff */
void substitute_alias( DESC_DATA *d, char *argument )
{
    char buf[STRING_LENGTH];
    char name[INPUT_LENGTH];
    char *point;
    int alias;

    if (USR(d)->alias[0] == NULL || !str_prefix("alias", argument)
    || !str_prefix("unalias", argument))
    {
	process_command(USR(d), argument);
	return;
    }

    strcpy(buf, argument);

    for (alias = 0; alias < MAX_ALIAS; alias++)
    {
	if (USR(d)->alias[alias] == NULL)
	    break;

	if (!str_prefix(USR(d)->alias[alias], argument))
	{
	    point = one_argument(argument, name);
	    if (!str_cmp(USR(d)->alias[alias], name))
	    {
		buf[0] = '\0';
		strcat(buf, USR(d)->alias_sub[alias]);

		if (point[0])
		{
		    strcat(buf, " ");
		    strcat(buf, point);
		}

	        if (strlen(buf) > INPUT_LENGTH - 1)
	        {
		    send_to_user(
			"Alias substitution too long. Truncated.\r\n", USR(d));
		    buf[INPUT_LENGTH -1] = '\0';
	        }
		break;
	    }
	}
    }

    process_command(USR(d), buf);
}

void do_alia( USER_DATA *usr, char *argument)
{
    send_to_user("I'm sorry, alias must be entered in full.\n\r", usr);
    return;
}

void do_alias( USER_DATA *usr, char *argument)
{
    char buf[STRING_LENGTH];
    char arg[INPUT_LENGTH];
    int pos;

    smash_tilde(argument);

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	if (usr->alias[0] == NULL)
	{
	    send_to_user("You have no aliases defined.\n\r", usr);
	    return;
	}

	send_to_user("Your current aliases are:\n\r", usr);

	for (pos = 0; pos < MAX_ALIAS; pos++)
	{
	    if (usr->alias[pos] == NULL || usr->alias_sub[pos] == NULL)
		break;

	    sprintf(buf, "    %s:  %s\n\r", usr->alias[pos],
		    usr->alias_sub[pos]);
	    send_to_user_bw(buf, usr);
	}
	return;
    }

    if (!str_prefix("unalias", arg) || !str_cmp("alias", arg))
    {
	send_to_user("Sorry, that word is reserved.\n\r", usr);
	return;
    }

    if (strchr(arg, ' ') || strchr(arg, '"') || strchr(arg, '\''))
    {
	send_to_user("The word to be aliased should not contain a space, "
		     "a tick or a double-quote.\n\r", usr);
	return;
    }

    if (argument[0] == '\0')
    {
	for (pos = 0; pos < MAX_ALIAS; pos++)
	{
	    if (usr->alias[pos] == NULL || usr->alias_sub[pos] == NULL)
		break;

	    if (!str_cmp(arg, usr->alias[pos]))
	    {
		sprintf(buf, "%s aliases to '%s'.\n\r", usr->alias[pos],
			usr->alias_sub[pos]);
		send_to_user_bw(buf, usr);
		return;
	    }
	}

	send_to_user("That alias is not defined.\n\r", usr);
	return;
    }

    for (pos = 0; pos < MAX_ALIAS; pos++)
    {
	if (usr->alias[pos] == NULL)
	    break;

	if (!str_cmp(arg, usr->alias[pos]))
	{
	    free_string(usr->alias_sub[pos]);
	    usr->alias_sub[pos] = str_dup(argument);
	    sprintf(buf, "%s is now realiased to '%s'.\n\r", arg, argument);
	    send_to_user_bw(buf, usr);
	    return;
	}
     }

     if (pos >= MAX_ALIAS)
     {
	send_to_user("Sorry, you have reached the alias limit.\n\r", usr);
	return;
     }
  
     usr->alias[pos]	 = str_dup(arg);
     usr->alias_sub[pos] = str_dup(argument);
     sprintf(buf, "%s is now aliased to '%s'.\n\r", arg, argument);
     send_to_user_bw(buf, usr);
}


void do_unalias( USER_DATA *usr, char *argument)
{
    char arg[INPUT_LENGTH];
    int pos;
    bool found = FALSE;
 
    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_user("Unalias what?\n\r", usr);
	return;
    }

    for (pos = 0; pos < MAX_ALIAS; pos++)
    {
	if (usr->alias[pos] == NULL)
	    break;

	if (found)
	{
	    usr->alias[pos-1]		= usr->alias[pos];
	    usr->alias_sub[pos-1]	= usr->alias_sub[pos];
	    usr->alias[pos]		= NULL;
	    usr->alias_sub[pos]		= NULL;
	    continue;
	}

	if (!str_cmp(arg, usr->alias[pos]))
	{
	    send_to_user("Alias removed.\n\r", usr);
	    free_string(usr->alias[pos]);
	    free_string(usr->alias_sub[pos]);
	    usr->alias[pos]	= NULL;
	    usr->alias_sub[pos]	= NULL;
	    found = TRUE;
	}
    }

    if (!found)
	send_to_user("No alias of that name to remove.\n\r", usr);
}

char *time_str( time_t last_time )
{
    char *strtime = ctime(&last_time);
    char buf1[100], buf2[100];

    strtime[strlen(strtime)-6] = '\0';
    strcpy(buf1, strtime);
    strcpy(buf2, &buf1[4]);
    return str_dup(buf2);
}

void fread_finger( USER_DATA *usr, FILE *fp )
{
    char buf[STRING];
    char *word = '\0';
    bool fMatch = FALSE;
    char *name, *real_name;
    char *email, *alt_mail, *plan;
    char *url, *title, *host_small;
    char *host_name, *icq;
    int level, gender;
    long last_logon, last_logoff;
    long fToggle = 0;
    int age, t_login, t_note;
    long fHide = 0;
    bool Validated = FALSE;

    name        = NULL;
    real_name   = NULL;
    email       = NULL;
    alt_mail    = NULL;
    url         = NULL;
    title       = NULL;
    host_name   = NULL;
    host_small  = NULL;
    plan        = NULL;
    icq		= NULL;
    level       = -1;
    gender      = -1;
    last_logon  = -1;
    last_logoff = -1;
    age         = -1;
    t_login	= -1;
    t_note	= -1;

    for (;;)
    {
        word    = feof(fp) ? "End" : fread_word(fp);
        fMatch  = FALSE;

        switch (UPPER(word[0]))
        {
            case '*':
                fMatch = TRUE;
                fread_to_eol(fp);
                break;

            case 'A':
                if (!str_cmp(word, "Age"))
                {
                    age = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }

                if (!str_cmp(word, "Aema"))
                {
                    alt_mail = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
                break;

            case 'E':
                if (!str_cmp(word, "Emai"))
                {
                    email = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
                if (!str_cmp(word, "End"))
                {

    sprintf(buf, "#c*%s* %s%-12s #W(#x%s#W)#x\n\r",
        !Validated ? " NEW " :
        level == ADMIN ? "ADMIN" : "HUSKY",
	is_friend(usr, name) ? "#C" :
	is_notify(usr, name) ? "#G" :
	is_enemy(usr, name)  ? "#R" : "#Y", name, title);
    send_to_user(buf, usr);

    if (!IS_SET(fHide, HIDE_REALNAME) || IS_ADMIN(usr))
    {
    sprintf(buf, "#c*HUSKY* #WReal Name    : #x%s%s\n\r", real_name,
        IS_ADMIN(usr) ? gender == 0 ? " (Male)" : " (Female)" : "");
    send_to_user(buf, usr);
    }

    if (!IS_SET(fHide, HIDE_EMAIL) || IS_ADMIN(usr))
    {
    sprintf(buf, "#c*HUSKY* #WE-mail       : #x%s\n\r", email);
    send_to_user(buf, usr);
    }

    if (!str_cmp(alt_mail, "NONE"))
	send_to_user("", usr);
    else if (!IS_SET(fHide, HIDE_ALTEMAIL) || IS_ADMIN(usr))
   {
    sprintf(buf, "#c*HUSKY* #WAlt. E-mail  : #x%s\n\r", alt_mail);
    send_to_user(buf, usr);
    }

    if (!str_cmp(url, "NONE"))
	send_to_user("", usr);
    else if (!IS_SET(fHide, HIDE_HOMEPAGE) || IS_ADMIN(usr))
   {
    sprintf(buf, "#c*HUSKY* #WHomepage Url : #x%s\n\r", url);
    send_to_user(buf, usr);
    }

    if (!str_cmp(icq, "NONE"))
	send_to_user("", usr);
    else if (!IS_SET(fHide, HIDE_ICQ) || IS_ADMIN(usr))
   {
    sprintf(buf, "#c*HUSKY* #WICQ Number   : #x%s\n\r", icq);
    send_to_user(buf, usr);
    }

/*
    sprintf(buf, "#gUser has e#GX#gpress messages %s#x\n\r",
	IS_SET(fToggle, TOGGLE_XING) ? "#WENABLED" : "#RDISABLED");
    send_to_user(buf, usr); Baxter */

    sprintf(buf, "#cTotal login: #C%d#c, and have left #C%d #cnotes.#x\n\r",
	t_login, t_note);
    send_to_user(buf, usr);
    sprintf(buf, "#GSpend #g%s #Gonline.#x\n\r", get_age(age, FALSE));
    send_to_user(buf, usr);
    sprintf(buf, "#yLast on: #Y(%s) #yto #Y(%s) #yfrom #c%s#x\n\r",
        time_str(last_logon), time_str(last_logoff),
        IS_ADMIN(usr) ? host_name : host_small);
    send_to_user(buf, usr);
    finger_mail(usr, name);
    if (strlen(plan) > 0)
    {
	print_to_user_bw(usr, "%s\n\r", plan);
    }

                    if (name) free_string(name);
                    if (real_name) free_string(real_name);
                    if (title) free_string(title);
                    if (email) free_string(email);
                    if (alt_mail) free_string(alt_mail);
                    if (host_name) free_string(host_name);
                    if (host_small) free_string(host_small);
                    if (url) free_string(url);
		    if (plan) free_string(plan);
                    if (icq) free_string(icq);
                    return;
                }
                break;

            case 'G':
                if (!str_cmp(word, "Gend"))
                {
                    gender = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }
            case 'H':
		if (!str_cmp(word, "Hide"))
		{
		    fHide = fread_flag(fp);
		    fMatch = TRUE;
                    break;
                }
                if (!str_cmp(word, "Hurl"))
                {
                    url = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
		if (!str_cmp(word, "HostNam"))
		{
		    host_name  = str_dup(fread_word(fp));
		    host_small = str_dup(fread_string(fp));
		    fMatch = TRUE;
		    break;
		}
                break;

	    case 'I':
		if (!str_cmp(word, "ICQ"))
		{
		    icq = str_dup(fread_string(fp));
		    fMatch = TRUE;
		    break;
		}
		break;

            case 'L':
                if (!str_cmp(word, "Levl"))
                {
                    level = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }

                if (!str_cmp(word, "Llon"))
                {
                    last_logon = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }

                if (!str_cmp(word, "Loff"))
                {
                    last_logoff = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }
                break;

            case 'N':
                if (!str_cmp(word, "Name"))
                {
                    name = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
                break;

            case 'P':
                if (!str_cmp(word, "Plan"))
                {
                    plan = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
                break;

            case 'R':
                if (!str_cmp(word, "Rnam"))
                {
                    real_name = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
                break;

            case 'T':
		if (!str_cmp(word, "Toggle"))
		{
		    fToggle = fread_flag(fp);
		    fMatch = TRUE;
                    break;
                }
                if (!str_cmp(word, "Titl"))
                {
                    title = str_dup(fread_string(fp));
                    fMatch = TRUE;
                    break;
                }
		if (!str_cmp(word, "TotalLN"))
		{
		    t_login = fread_number(fp);
		    t_note  = fread_number(fp);
		    fMatch  = TRUE;
		    break;
		}
                break;

            case 'V':
                if (!str_cmp(word, "Val"))
                {
                    Validated = fread_number(fp);
                    fMatch = TRUE;
                    break;
                }
                break;
        }

        if (!fMatch)
        {
            fread_to_eol(fp);
        }
    }
}


void do_finger( USER_DATA *usr, char *argument )
{
    char arg[INPUT];
    char buf[STRING];
    FILE *fp;
    USER_DATA *fUser; 

    one_argument(argument, arg);

    smash_tilde(arg);

    if (arg[0] == '\0')
    {
	syntax("finger <nickname>", usr);
	return;
    }

    if ((!(fUser = get_user(arg))) || (fUser && IS_TOGGLE(fUser, TOGGLE_INVIS)))
    {
	if (!is_user(arg))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	sprintf(buf, "%s%s", USER_DIR, capitalize(arg));
	if (!(fp = fopen(buf, "r")))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	fread_finger(usr, fp);
	fclose(fp);
	return;
    }

    print_to_user(usr, "#c*%s* %s%-12s #W(#x%s#W) (Location: #c%s#W)\n\r",
	!fUser->Validated ? " NEW " : IS_ADMIN(fUser) ? "ADMIN" : "HUSKY",
	is_friend(usr, fUser->name) ? "#C" :
	is_notify(usr, fUser->name) ? "#G" :
	is_enemy(usr, fUser->name)  ? "#R" : "#Y",
	fUser->name, fUser->title, fUser->pBoard->long_name);

    if (!IS_HIDE(fUser, HIDE_REALNAME) || IS_ADMIN(usr))
	print_to_user(usr, "#c*HUSKY* #WReal Name    : #x%s%s%s\n\r",
	    fUser->real_name, IS_ADMIN(usr) ? IS_MALE(fUser) ? " (Male)"
	    : " (Female)" : "", IS_ADMIN(usr) && IS_HIDE(fUser, HIDE_REALNAME)
	    ? " (Hidden)" : "");

    if (!IS_HIDE(fUser, HIDE_EMAIL) || IS_ADMIN(usr))
	print_to_user(usr, "#c*HUSKY* #WE-Mail       : #x%s%s\n\r",
	    fUser->email, IS_ADMIN(usr) && IS_HIDE(fUser, HIDE_EMAIL)
	    ? " (Hidden)" : "");

    if (!str_cmp(fUser->alt_email, "NONE"))
	send_to_user("", usr);
    else if (!IS_HIDE(fUser, HIDE_ALTEMAIL) || IS_ADMIN(usr))
	print_to_user(usr, "#c*HUSKY* #WAlt. E-Mail  : #x%s%s\n\r",
	    fUser->alt_email, IS_ADMIN(usr) && IS_HIDE(fUser, HIDE_ALTEMAIL)
	    ? " (Hidden)" : "");

    if (!str_cmp(fUser->home_url, "NONE"))
	send_to_user("", usr);
    else if (!IS_HIDE(fUser, HIDE_HOMEPAGE) || IS_ADMIN(usr))
	print_to_user(usr, "#c*HUSKY* #WHomepage Url : #x%s%s\n\r",
	    fUser->home_url, IS_ADMIN(usr) && IS_HIDE(fUser, HIDE_HOMEPAGE)
	    ? " (Hidden)" : "");

    if (!str_cmp(fUser->icquin, "NONE"))
	send_to_user("", usr);
    else if (!IS_HIDE(fUser, HIDE_ICQ) || IS_ADMIN(usr))
	print_to_user(usr, "#c*HUSKY* #WICQ Number   : #x%s%s\n\r",
	    fUser->icquin, IS_ADMIN(usr) && IS_HIDE(fUser, HIDE_ICQ)
	    ? " (Hidden)" : "");

    print_to_user(usr, "#gUser has e#GX#gpress messages %s#x\n\r",
	IS_TOGGLE(fUser, TOGGLE_XING) ? "#WENABLED" : "#RDISABLED");

    print_to_user(usr, "#cTotal login: #C%d#c, "
		       "and have left #C%d #cnotes.#x\n\r", fUser->total_login,
	fUser->total_note);
    print_to_user(usr, "#GSpend #g%s #Gonline.#x\n\r",
	get_age((fUser->used + (int) (current_time - fUser->age)), FALSE));

    print_to_user(usr, "#yONLINE SINCE: #Y%s #yfrom: #c%s#x\n\r",
	time_str(fUser->last_logon), IS_ADMIN(usr) ? fUser->host_name
	: fUser->host_small);

    finger_mail(usr, fUser->name);

    if (strlen(fUser->plan) > 0)
	print_to_user_bw(usr, "%s\n\r", fUser->plan);

    return;
}

void friend_remove( USER_DATA *usr, char *argument, bool fAll )
{
    char arg[INPUT];
    int pos;
    bool found = FALSE;
 
    one_argument(argument, arg);

    if (fAll)
    {
	send_to_user("Flushing the your friend list.\n\r", usr);

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->friend[pos] == NULL)
		break;

	    if (usr->friend[pos])
		free_string(usr->friend[pos]);
	    if (usr->friend_com[pos])
		free_string(usr->friend_com[pos]);
	    usr->friend[pos]		= NULL;
	    usr->friend_com[pos]	= NULL;
	}

	send_to_user("Done.\n\r", usr);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->friend[pos] == NULL)
	    break;

	if (found)
	{
	    usr->friend[pos-1]		= usr->friend[pos];
	    usr->friend_com[pos-1]	= usr->friend_com[pos];
	    usr->friend[pos]		= NULL;
	    usr->friend_com[pos]	= NULL;
	    continue;
	}

	if (!str_cmp(arg, usr->friend[pos]))
	{
	    print_to_user(usr, "Friend #C%s#x removed.\n\r", usr->friend[pos]);
	    if (usr->friend[pos])
		free_string(usr->friend[pos]);
	    if (usr->friend_com[pos])
		free_string(usr->friend_com[pos]);
	    usr->friend[pos]		= NULL;
	    usr->friend_com[pos]	= NULL;
	    found = TRUE;
	}
    }

    if (!found)
	print_to_user(usr,
	    "You don't have an entry for #C%s#x in your friend list.\n\r",
	    capitalize(argument));

    return;
}

void friend_nouser( USER_DATA *usr )
{
    int pos;

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->friend[pos] == NULL)
	    break;

	if (!is_user(usr->friend[pos]))
	{
	    print_to_user(usr, "No such user #C%s#x.\n\r", usr->friend[pos]);
	    friend_remove(usr, usr->friend[pos], FALSE);
	}
    }
}

void do_friend( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char cbuf[STRING];
    char dbuf[STRING];
    char arg[INPUT];
    int pos, vnum;
    BUFFER *output;

    smash_tilde(argument);

    argument	= one_argument(argument, arg);
    vnum	= 0;
    output	= new_buf( );
    strcpy(cbuf, "");

    if (arg[0] == '\0')
    {
	if (usr->friend[0] == NULL)
	{
	    send_to_user("You don't have any entry in your friend list.\n\r",
		usr);
	    return;
	}

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->friend[pos] == NULL)
		break;

	    friend_nouser(usr);

	    if (usr->friend[pos] == NULL)
		break;

	    sprintf(buf, "#C%-12s #W: #Y%s\n\r", usr->friend[pos],
		str_cmp(usr->friend_com[pos], "[none]") ?
		usr->friend_com[pos] : "");
	    strcat(cbuf, buf);
	    vnum++;
	}

	sprintf(dbuf, "#GYou friend list (#Y%d#G):\n\r"
		      "Nickname       Comment\n\r"
		      "--------       -------\n\r", vnum);
	add_buf(output, dbuf);
	add_buf(output, cbuf);
	add_buf(output, "#x");
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->friend[pos] == NULL)
	    break;

	friend_nouser(usr);

	if (!str_cmp(arg, usr->friend[pos]))
	{
	    print_to_user(usr, "Modifying information for #C%s#x.\n\r"
			       "Removing: #C%s#x\n\rNew friend: #C%s#x\n\r",
		usr->friend[pos], usr->friend[pos], usr->friend[pos]);

	    if (strlen(argument) > MAX_COMMENT)
	    {
		send_to_user("Maximum comment length can only be 60 "
			     "characters.\n\r", usr);
		return;
	    }

	    free_string(usr->friend_com[pos]);
	    if (argument[0] == '\0')
		usr->friend_com[pos] = str_dup("[none]");
	    else
		usr->friend_com[pos] = str_dup(argument);
	    return;
	}
    }

    if (!str_cmp(arg, "-r"))
    {
	if (argument[0] != '\0')
	{
	    friend_nouser(usr);
	    friend_remove(usr, argument, FALSE);
	    return;
	}
    }

    if (!str_cmp(arg, "-f"))
    {
	friend_nouser(usr);
	friend_remove(usr, argument, TRUE);
	return;
    }

    if (!is_user(arg))
    {
	print_to_user(usr,
	    "That's creative, but user #C%s#x does not exist.\n\r",
	    capitalize(arg));
	return;
    }

    friend_nouser(usr);

    if (pos >= MAX_FRIEND)
    {
	send_to_user("Sorry, you have reached the friend limit.\n\r", usr);
	return;
    }
  
    if (strlen(argument) > MAX_COMMENT)
    {
	send_to_user("Maximum comment length can only be 60 characters.\n\r",
	    usr);
	return;
    }

    if (is_notify(usr, arg))
	notify_remove(usr, arg, FALSE);

    if (is_enemy(usr, arg))
	enemy_remove(usr, arg, FALSE);

    usr->friend[pos]	 = str_dup(capitalize(arg));
    if (argument[0] == '\0')
	usr->friend_com[pos] = str_dup("[none]");
    else
	usr->friend_com[pos] = str_dup(argument);
    print_to_user(usr, "New friend: #C%s#x\n\r", capitalize(arg));
    return;
}

void notify_remove( USER_DATA *usr, char *argument, bool fAll )
{
    char arg[INPUT]; 
    int pos;
    bool found = FALSE;
 
    one_argument(argument, arg);

    if (fAll)
    {
	send_to_user("Flushing the your notify list.\n\r", usr);

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->notify[pos] == NULL)
		break;

	    if (usr->notify[pos])
		free_string(usr->notify[pos]);
	    if (usr->notify_com[pos])
		free_string(usr->notify_com[pos]);
	    usr->notify[pos]		= NULL;
	    usr->notify_com[pos]	= NULL;
	}

	send_to_user("Done.\n\r", usr);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->notify[pos] == NULL)
	    break;

	if (found)
	{
	    usr->notify[pos-1]		= usr->notify[pos];
	    usr->notify_com[pos-1]	= usr->notify_com[pos];
	    usr->notify[pos]		= NULL;
	    usr->notify_com[pos]	= NULL;
	    continue;
	}

	if (!str_cmp(arg, usr->notify[pos]))
	{
	    print_to_user(usr, "Notify #G%s#x removed.\n\r", usr->notify[pos]);
	    if (usr->notify[pos])
		free_string(usr->notify[pos]);
	    if (usr->notify_com[pos])
		free_string(usr->notify_com[pos]);
	    usr->notify[pos]		= NULL;
	    usr->notify_com[pos]	= NULL;
	    found = TRUE;
	}
    }

    if (!found)
	print_to_user(usr,
	    "You don't have an entry for #G%s#x in your notify list.\n\r",
	    capitalize(argument));

    return;
}

void notify_nouser( USER_DATA *usr )
{
    int pos;

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->notify[pos] == NULL)
	    break;

	if (!is_user(usr->notify[pos]))
	{
	    print_to_user(usr, "No such user #G%s#x.\n\r", usr->notify[pos]);
	    notify_remove(usr, usr->notify[pos], FALSE);
	}
    }
}

void do_notify( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char cbuf[STRING];
    char dbuf[STRING];
    char arg[INPUT];
    int pos, vnum;
    BUFFER *output;

    smash_tilde(argument);

    argument	= one_argument(argument, arg);
    vnum	= 0;
    output	= new_buf( );
    strcpy(cbuf, "");

    if (arg[0] == '\0')
    {
	if (usr->notify[0] == NULL)
	{
	    send_to_user("You don't have any entry in your notify list.\n\r",
		usr);
	    return;
	}

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->notify[pos] == NULL)
		break;

	    notify_nouser(usr);

	    if (usr->notify[pos] == NULL)
		break;

	    sprintf(buf, "#G%-12s #W: #Y%s\n\r", usr->notify[pos],
		str_cmp(usr->notify_com[pos], "[none]") ?
		usr->notify_com[pos] : "");
	    strcat(cbuf, buf);
	    vnum++;
	}

	sprintf(dbuf, "#GYou notify list (#Y%d#G):\n\r"
		      "Nickname       Comment\n\r"
		      "--------       -------\n\r", vnum);
	add_buf(output, dbuf);
	add_buf(output, cbuf);
	add_buf(output, "#x");
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->notify[pos] == NULL)
	    break;

	notify_nouser(usr);

	if (!str_cmp(arg, usr->notify[pos]))
	{
	    print_to_user(usr, "Modifying information for #G%s#x.\n\r"
			       "Removing: #G%s#x\n\rNew notify: #G%s#x\n\r",
		usr->notify[pos], usr->notify[pos], usr->notify[pos]);

	    if (strlen(argument) > MAX_COMMENT)
	    {
		send_to_user("Maximum comment length can only be 60 "
			     "characters.\n\r", usr);
		return;
	    }

	    free_string(usr->notify_com[pos]);
	    if (argument[0] == '\0')
		usr->notify_com[pos] = str_dup("[none]");
	    else
		usr->notify_com[pos] = str_dup(argument);
	    return;
	}
    }

    if (!str_cmp(arg, "-r"))
    {
	if (argument[0] != '\0')
	{
	    notify_nouser(usr);
	    notify_remove(usr, argument, FALSE);
	    return;
	}
    }

    if (!str_cmp(arg, "-f"))
    {
	notify_nouser(usr);
	notify_remove(usr, argument, TRUE);
	return;
    }

    if (!is_user(arg))
    {
	print_to_user(usr,
	    "That's creative, but user #G%s#x does not exist.\n\r",
	    capitalize(arg));
	return;
    }

    notify_nouser(usr);

    if (pos >= MAX_FRIEND)
    {
	send_to_user("Sorry, you have reached the notify limit.\n\r", usr);
	return;
    }
  
    if (strlen(argument) > MAX_COMMENT)
    {
	send_to_user("Maximum comment length can only be 60 characters.\n\r",
	    usr);
	return;
    }

    if (is_friend(usr, arg))
	friend_remove(usr, arg, FALSE);

    if (is_enemy(usr, arg))
	enemy_remove(usr, arg, FALSE);

    usr->notify[pos]	 = str_dup(capitalize(arg));
    if (argument[0] == '\0')
	usr->notify_com[pos] = str_dup("[none]");
    else
	usr->notify_com[pos] = str_dup(argument);
    print_to_user(usr, "New notify: #G%s#x\n\r", capitalize(arg));
    return;
}

void enemy_remove( USER_DATA *usr, char *argument, bool fAll )
{
    char arg[INPUT];
    int pos;
    bool found = FALSE;
 
    one_argument(argument, arg);

    if (fAll)
    {
	send_to_user("Flushing the your enemy list.\n\r", usr);

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->enemy[pos] == NULL)
		break;

	    if (usr->enemy[pos])
		free_string(usr->enemy[pos]);
	    if (usr->enemy_com[pos])
		free_string(usr->enemy_com[pos]);
	    usr->enemy[pos]	= NULL;
	    usr->enemy_com[pos]	= NULL;
	}

	send_to_user("Done.\n\r", usr);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->enemy[pos] == NULL)
	    break;

	if (found)
	{
	    usr->enemy[pos-1]		= usr->enemy[pos];
	    usr->enemy_com[pos-1]	= usr->enemy_com[pos];
	    usr->enemy[pos]		= NULL;
	    usr->enemy_com[pos]		= NULL;
	    continue;
	}

	if (!str_cmp(arg, usr->enemy[pos]))
	{
	    print_to_user(usr, "Enemy #R%s#x removed.\n\r", usr->enemy[pos]);
	    if (usr->enemy[pos])
		free_string(usr->enemy[pos]);
	    if (usr->enemy_com[pos])
		free_string(usr->enemy_com[pos]);
	    usr->enemy[pos]	= NULL;
	    usr->enemy_com[pos]	= NULL;
	    found = TRUE;
	}
    }

    if (!found)
	print_to_user(usr,
	    "You don't have an entry for #R%s#x in your enemy list.\n\r",
	    capitalize(argument));

    return;
}

void enemy_nouser( USER_DATA *usr )
{
    int pos;

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->enemy[pos] == NULL)
	    break;

	if (!is_user(usr->enemy[pos]))
	{
	    print_to_user(usr, "No such user #R%s#x.\n\r", usr->enemy[pos]);
	    enemy_remove(usr, usr->enemy[pos], FALSE);
	}
    }
}

void do_enemy( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char cbuf[STRING];
    char dbuf[STRING];
    char arg[INPUT];
    int pos, vnum;
    BUFFER *output;

    smash_tilde(argument);

    argument	= one_argument(argument, arg);
    vnum	= 0;
    output	= new_buf( );
    strcpy(cbuf, "");

    if (arg[0] == '\0')
    {
	if (usr->enemy[0] == NULL)
	{
	    send_to_user("You don't have any entry in your enemy list.\n\r",
		usr);
	    return;
	}

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->enemy[pos] == NULL)
		break;

	    enemy_nouser(usr);

	    if (usr->enemy[pos] == NULL)
		break;

	    sprintf(buf, "#R%-12s #W: #Y%s\n\r", usr->enemy[pos],
		str_cmp(usr->enemy_com[pos], "[none]") ?
		usr->enemy_com[pos] : "");
	    strcat(cbuf, buf);
	    vnum++;
	}

	sprintf(dbuf, "#GYou enemy list (#Y%d#G):\n\r"
		      "Nickname       Comment\n\r"
		      "--------       -------\n\r", vnum);
	add_buf(output, dbuf);
	add_buf(output, cbuf);
	add_buf(output, "#x");
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->enemy[pos] == NULL)
	    break;

	enemy_nouser(usr);

	if (!str_cmp(arg, usr->enemy[pos]))
	{
	    print_to_user(usr, "Modifying information for #R%s#x.\n\r"
			       "Removing: #R%s#x\n\rNew enemy: #R%s#x\n\r",
		usr->enemy[pos], usr->enemy[pos], usr->enemy[pos]);

	    if (strlen(argument) > MAX_COMMENT)
	    {
		send_to_user("Maximum comment length can only be 60 "
			     "characters.\n\r", usr);
		return;
	    }

	    free_string(usr->enemy_com[pos]);
	    if (argument[0] == '\0')
		usr->enemy_com[pos] = str_dup("[none]");
	    else
		usr->enemy_com[pos] = str_dup(argument);
	    return;
	}
    }

    if (!str_cmp(arg, "-r"))
    {
	if (argument[0] != '\0')
	{
	    enemy_nouser(usr);
	    enemy_remove(usr, argument, FALSE);
	    return;
	}
    }

    if (!str_cmp(arg, "-f"))
    {
	enemy_nouser(usr);
	enemy_remove(usr, argument, TRUE);
	return;
    }

    if (!is_user(arg))
    {
	print_to_user(usr,
	    "That's creative, but user #R%s#x does not exist.\n\r",
	    capitalize(arg));
	return;
    }

    enemy_nouser(usr);

    if (pos >= MAX_FRIEND)
    {
	send_to_user("Sorry, you have reached the enemy limit.\n\r", usr);
	return;
    }
  
    if (strlen(argument) > MAX_COMMENT)
    {
	send_to_user("Maximum comment length can only be 60 characters.\n\r",
	    usr);
	return;
    }

    if (is_notify(usr, arg))
	notify_remove(usr, arg, FALSE);

    if (is_friend(usr, arg))
	friend_remove(usr, arg, FALSE);

    usr->enemy[pos]	 = str_dup(capitalize(arg));
    if (argument[0] == '\0')
	usr->enemy_com[pos] = str_dup("[none]");
    else
	usr->enemy_com[pos] = str_dup(argument);
    print_to_user(usr, "New enemy: #R%s#x\n\r", capitalize(arg));
    return;
}

void ignore_remove( USER_DATA *usr, char *argument, bool fAll )
{
    char arg[INPUT];
    int pos;
    bool found = FALSE;
 
    one_argument(argument, arg);

    if (fAll)
    {
	send_to_user("Flushing the your ignore list.\n\r", usr);

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->ignore[pos] == NULL)
		break;

	    if (usr->ignore[pos])
		free_string(usr->ignore[pos]);
	    if (usr->ignore_com[pos])
		free_string(usr->ignore_com[pos]);
	    usr->ignore[pos]		= NULL;
	    usr->ignore_com[pos]	= NULL;
	}

	send_to_user("Done.\n\r", usr);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->ignore[pos] == NULL)
	    break;

	if (found)
	{
	    usr->ignore[pos-1]		= usr->ignore[pos];
	    usr->ignore_com[pos-1]	= usr->ignore_com[pos];
	    usr->ignore[pos]		= NULL;
	    usr->ignore_com[pos]	= NULL;
	    continue;
	}

	if (!str_cmp(arg, usr->ignore[pos]))
	{
	    print_to_user(usr, "Ignore #Y%s#x removed.\n\r", usr->ignore[pos]);
	    if (usr->ignore[pos])
		free_string(usr->ignore[pos]);
	    if (usr->ignore_com[pos])
		free_string(usr->ignore_com[pos]);
	    usr->ignore[pos]		= NULL;
	    usr->ignore_com[pos]	= NULL;
	    found = TRUE;
	}
    }

    if (!found)
	print_to_user(usr,
	    "You don't have an entry for #Y%s#x in your ignore list.\n\r",
	    capitalize(argument));

    return;
}

void ignore_nouser( USER_DATA *usr )
{
    int pos;

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->ignore[pos] == NULL)
	    break;

	if (!is_user(usr->ignore[pos]))
	{
	    print_to_user(usr, "No such user #Y%s#x.\n\r", usr->ignore[pos]);
	    friend_remove(usr, usr->ignore[pos], FALSE);
	}
    }
}

void do_ignore( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char cbuf[STRING];
    char dbuf[STRING];
    char arg[INPUT];
    int pos, vnum;
    BUFFER *output;

    smash_tilde(argument);

    argument	= one_argument(argument, arg);
    vnum	= 0;
    output	= new_buf( );
    strcpy(cbuf, "");

    if (arg[0] == '\0')
    {
	if (usr->ignore[0] == NULL)
	{
	    send_to_user("You don't have any entry in your ignore list.\n\r",
		usr);
	    return;
	}

	for (pos = 0; pos < MAX_FRIEND; pos++)
	{
	    if (usr->ignore[pos] == NULL)
		break;

	    ignore_nouser(usr);

	    sprintf(buf, "#Y%-12s #W: #Y%s\n\r", usr->ignore[pos],
		str_cmp(usr->ignore_com[pos], "[none]") ?
		usr->ignore_com[pos] : "");
	    strcat(cbuf, buf);
	    vnum++;
	}

	sprintf(dbuf, "#GYou ignore list (#Y%d#G):\n\r"
		      "Nickname       Comment\n\r"
		      "--------       -------\n\r", vnum);
	add_buf(output, dbuf);
	add_buf(output, cbuf);
	add_buf(output, "#x");
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    for (pos = 0; pos < MAX_FRIEND; pos++)
    {
	if (usr->ignore[pos] == NULL)
	    break;

	ignore_nouser(usr);

	if (!str_cmp(arg, usr->ignore[pos]))
	{
	    print_to_user(usr, "Modifying information for #Y%s#x.\n\r"
			       "Removing: #Y%s#x\n\rNew ignore: #Y%s#x\n\r",
		usr->ignore[pos], usr->ignore[pos], usr->ignore[pos]);

	    if (strlen(argument) > MAX_COMMENT)
	    {
		send_to_user("Maximum comment length can only be 60 "
			     "characters.\n\r", usr);
		return;
	    }

	    free_string(usr->ignore_com[pos]);
	    if (argument[0] == '\0')
		usr->ignore_com[pos] = str_dup("[none]");
	    else
		usr->ignore_com[pos] = str_dup(argument);
	    return;
	}
    }

    if (!str_cmp(arg, "-r"))
    {
	if (argument[0] != '\0')
	{
	    ignore_nouser(usr);
	    ignore_remove(usr, argument, FALSE);
	    return;
	}
    }

    if (!str_cmp(arg, "-f"))
    {
	ignore_nouser(usr);
	ignore_remove(usr, argument, TRUE);
	return;
    }

    if (!is_user(arg))
    {
	print_to_user(usr,
	    "That's creative, but user #Y%s#x does not exist.\n\r",
	    capitalize(arg));
	return;
    }

    ignore_nouser(usr);

    if (pos >= MAX_FRIEND)
    {
	send_to_user("Sorry, you have reached the ignore limit.\n\r", usr);
	return;
    }
  
    if (strlen(argument) > MAX_COMMENT)
    {
	send_to_user("Maximum comment length can only be 60 characters.\n\r",
	    usr);
	return;
    }

    usr->ignore[pos]	 = str_dup(capitalize(arg));
    if (argument[0] == '\0')
	usr->ignore_com[pos] = str_dup("[none]");
    else
	usr->ignore_com[pos] = str_dup(argument);
    print_to_user(usr, "New ignore: #Y%s#x\n\r", capitalize(arg));
    return;
}

bool is_friend( USER_DATA *usr, char *name )
{
    int friend;

    for (friend = 0; friend < MAX_FRIEND; friend++)
    {
	if (usr->friend[friend] == NULL)
	    break;

	if (!str_cmp(name, usr->friend[friend]))
	    return TRUE;
    }

    return FALSE;
}

bool is_notify( USER_DATA *usr, char *name )
{
    int notify;

    for (notify = 0; notify < MAX_FRIEND; notify++)
    {
	if (usr->notify[notify] == NULL)
	    break;

	if (!str_cmp(name, usr->notify[notify]))
	    return TRUE;
    }

    return FALSE;
}

bool is_enemy( USER_DATA *usr, char *name )
{
    int enemy;

    for (enemy = 0; enemy < MAX_FRIEND; enemy++)
    {
	if (usr->enemy[enemy] == NULL)
	    break;

	if (!str_cmp(name, usr->enemy[enemy]))
	    return TRUE;
    }

    return FALSE;
}

bool is_ignore( USER_DATA *usr, char *name )
{
    int ignore;

    if (is_moderator(usr) && usr->pBoard == moderator_board(usr))
	return FALSE;

    if (IS_ADMIN(usr))
	return FALSE;

    for (ignore = 0; ignore < MAX_FRIEND; ignore++)
    {
	if (usr->ignore[ignore] == NULL)
	    break;

	if (!str_cmp(name, usr->ignore[ignore]))
	    return TRUE;
    }

    return FALSE;
}

void cmd_chat_talk( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    DESC_DATA *d;

    for (d = desc_list; d; d = d->next)
    {
	if (USR(d) && d->login == CON_LOGIN
	&& USR(d)->pBoard == board_lookup("chat", FALSE))
	{
	    sprintf(buf, "%s%s#x: %s#x\n\r",
		USR(d) == usr ? "#W" :
		is_friend(USR(d), usr->name) ? "#C" :
		is_notify(USR(d), usr->name) ? "#G" :
		is_enemy(USR(d), usr->name)  ? "#R" : "#x",
		usr->name, argument);

	    if (isBusySelf(USR(d)))
		add_buffer(USR(d), buf);
	    else
		send_to_user(buf, USR(d));
	}
    }

    return;
}

void cmd_chat_emote( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    DESC_DATA *d;

    for (d = desc_list; d; d = d->next)
    {
	if (USR(d) && d->login == CON_LOGIN
	&& USR(d)->pBoard == board_lookup("chat", FALSE))
	{
	    sprintf(buf, "%s%s#x %s#x\n\r",
		USR(d) == usr ? "#W" :
		is_friend(USR(d), usr->name) ? "#C" :
		is_notify(USR(d), usr->name) ? "#G" :
		is_enemy(USR(d), usr->name)  ? "#R" : "#x",
		usr->name, argument);

	    if (isBusySelf(USR(d)))
		add_buffer(USR(d), buf);
	    else
		send_to_user(buf, USR(d));
	}
    }

    return;
}

void cmd_chat_exit( USER_DATA *usr )
{
    do_jump(usr, "lobby");    
    return;
}

void cmd_chat_look( USER_DATA *usr )
{
    DESC_DATA *d;
    char buf[STRING];

    strcpy(buf, "Users in the chat room are:\n\r\n\r");

    for (d = desc_list; d; d = d->next)
    {
	if (USR(d) && d->login == CON_LOGIN
	&& USR(d)->pBoard == board_lookup("chat", FALSE))
	{
	    strcat(buf, USR(d)->name);
	    strcat(buf, " ");
	}
    }

    strcat(buf, "\n\r\n\r");
    send_to_user(buf, usr);
    return;
}

bool chat_command( USER_DATA *usr, char *argument )
{
    char command[STRING];
    char arg[INPUT];

    if (*argument != '/')
	return FALSE;

    argument = one_argument(argument, arg);

    if (!str_cmp(arg + 1, "exit"))
	cmd_chat_exit(usr);
    else if (!str_cmp(arg + 1, "look"))
	cmd_chat_look(usr);
    else if (!str_cmp(arg + 1, "emote"))
	cmd_chat_emote(usr, argument);
    else
    {
	sprintf(command, "%s %s", arg + 1, argument);
	process_command(usr, command);
    }

    return TRUE;
}

void process_chat_command( USER_DATA *usr, char *argument )
{
    while (isspace(*argument))
	argument++;

    usr->timer = 0;
    EDIT_MODE(usr) = EDITOR_NONE;

    if (!*argument)
	return;

    if (chat_command(usr, argument))
	return;

    cmd_chat_talk(usr, argument);
    return;
}

void do_credits( USER_DATA *usr, char *argument )
{
    do_help(usr, "diku");
    return;
}

