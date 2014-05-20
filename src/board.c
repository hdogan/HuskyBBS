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

/*
 * Board types.
 */
#define BOARD_NORMAL		1
#define BOARD_ADMIN		2
#define BOARD_GAME		3
#define BOARD_ANONYMOUS		4
#define BOARD_SECRET		5

#define DEFAULT_CAPACITY	150

BOARD_DATA *			first_board;
BOARD_DATA *			last_board;

/*
 * Functions.
 */
void	save_notes		( BOARD_DATA *pBoard );
bool	is_kicked		( USER_DATA *usr, BOARD_DATA *pBoard,
				  bool fMessage );
void	do_quit_org		( USER_DATA *usr, char *argument, bool fXing );

/*
 * Find a board.
 */
BOARD_DATA * board_lookup( const char *name, bool fNumber )
{
    BOARD_DATA *pBoard;

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (fNumber && atoi(name) == pBoard->vnum)
	    return pBoard;
	else if (LOWER(name[0]) == LOWER(pBoard->short_name[0])
	&& !str_prefix(name, pBoard->short_name))
	    return pBoard;
    }

    return NULL;
}

/*
 * Find the number of a board.
 */
int board_number( const BOARD_DATA *pBoard )
{
    BOARD_DATA *qBoard;

    for (qBoard = first_board; qBoard; qBoard = qBoard->next)
    {
	if (pBoard == qBoard)
	    return qBoard->vnum;
    }

    return -1;
}

/*
 * Attach new note.
 */
void note_attach( USER_DATA *usr, bool fAnonymous )
{
    NOTE_DATA *pNote;

    if (usr->pNote)
	return;

    pNote		= (NOTE_DATA *) alloc_mem(sizeof(NOTE_DATA));
    pNote->next		= NULL;
    pNote->prev		= NULL;
    pNote->sender	= fAnonymous ? IS_MALE(usr) ? str_dup("Male User")
			  : str_dup("Female User") : str_dup(usr->name);
    pNote->date		= str_dup("");
    pNote->subject	= str_dup("Undefined subject");
    pNote->text		= str_dup("");
    pNote->vnum		= 0;
    usr->pNote		= pNote;
    return;
}

/*
 * Recycle a note.
 */
void free_note( NOTE_DATA *pNote )
{
    if (!pNote)
    {
	bbs_bug("Free_note: Null pNote");
	return;
    }

    if (pNote->sender)	free_string(pNote->sender);
    if (pNote->subject)	free_string(pNote->subject);
    if (pNote->date)	free_string(pNote->date);
    if (pNote->text)	free_string(pNote->text);

    free_mem(pNote, sizeof(NOTE_DATA));
    return;
}

/*
 * Recycle a kick.
 */
void free_kick( KICK_DATA *pKick )
{
    if (!pKick)
    {
	bbs_bug("Free_kick: Null pKick");
	return;
    }

    if (pKick->name)	free_string(pKick->name);
    if (pKick->reason)	free_string(pKick->reason);

    free_mem(pKick, sizeof(KICK_DATA));
    return;
}

/*
 * Append this note to the given file.
 */
void append_note( FILE *fpNote, NOTE_DATA *pNote )
{
    if (!pNote)
    {
	bbs_bug("Append_note: Null pNote");
	return;
    }

    fprintf(fpNote, "Sender  %s~\n", pNote->sender	);
    fprintf(fpNote, "Date    %s~\n", pNote->date	);
    fprintf(fpNote, "Stamp   %ld\n", (iptr) pNote->stamp);
    fprintf(fpNote, "Vnum    %ld\n", pNote->vnum	);
    fprintf(fpNote, "Subject %s~\n", pNote->subject	);
    fprintf(fpNote, "Text\n%s~\n",   pNote->text	);
    return;
}

/*
 * Arhive removed note.
 */
void archive_note( NOTE_DATA *pNote, BOARD_DATA *pBoard )
{
    char buf[INPUT];
    FILE *fpArch;

    fclose(fpReserve);
    sprintf(buf, "%sarchive/%s.arc", NOTE_DIR, pBoard->short_name);
    if (!(fpArch = fopen(buf, "a")))
    {
	bbs_bug("Archive_note: Could not open to write %s", buf);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    append_note(fpArch, pNote);
    fclose(fpArch);
    fpReserve = fopen(NULL_FILE, "r");
    return;

}

bool can_write( USER_DATA *usr, BOARD_DATA *pBoard )
{
    if (IS_ADMIN(usr))
	return TRUE;

    if (pBoard->type == BOARD_ADMIN
    || pBoard->type == BOARD_GAME)
	return FALSE;

    return TRUE;
}

bool can_remove( USER_DATA *usr, NOTE_DATA *pNote )
{
    if (!pNote)
    {
	bbs_bug("Can_remove: Null pNote");
	return FALSE;
    }

    if (IS_ADMIN(usr))
	return TRUE;

    if (!str_cmp(pNote->sender, usr->name))
	return TRUE;

    if (!str_cmp(usr->pBoard->moderator, usr->name))
	return TRUE;

    return FALSE;
}

void note_remove( USER_DATA *usr, NOTE_DATA *pNote )
{
    BOARD_DATA *pBoard;

    if (!pNote)
    {
	bbs_bug("Note_remove: Null pNote");
	return;
    }

    pBoard = usr->pBoard;

    archive_note(pNote, pBoard);
    UNLINK(pNote, pBoard->first_note, pBoard->last_note);
    free_note(pNote);
    save_notes(pBoard);
    return;
}

int unread_notes( USER_DATA *usr, BOARD_DATA *pBoard )
{
    NOTE_DATA *pNote;
    time_t last_read;
    int count = 0;

    if (!pBoard)
    {
	bbs_bug("Unread_notes: Null pBoard");
	return 0;
    }

    if (board_number(pBoard) == -1)
    {
	bbs_bug("Unread_notes: Null pBoard (board number)");
	return 0;
    }

    last_read = usr->last_note[board_number(pBoard)];

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
	if (last_read < pNote->stamp)
	    count++;

    return count;
}

int total_notes( USER_DATA *usr, BOARD_DATA *pBoard )
{
    NOTE_DATA *pNote;
    int count = 0;

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
	count++;

    return count;
}

/*
 * Update reading notes.
 */
void update_read( USER_DATA *usr, NOTE_DATA *pNote )
{
    time_t *last_note = &usr->last_note[board_number(usr->pBoard)];
    *last_note = UMAX(*last_note, pNote->stamp);
}

/*
 * Save notes on a single board.
 */
void save_notes( BOARD_DATA *pBoard )
{
    char buf[INPUT];
    NOTE_DATA *pNote;
    FILE *fpNote;

    fclose(fpReserve);
    sprintf(buf, "%s%s.o", NOTE_DIR, pBoard->short_name);
    if (!(fpNote = fopen(buf, "w")))
    {
	bbs_bug("Save_notes: Could not open to write %s", buf);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
	append_note(fpNote, pNote);

    fclose(fpNote);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

/*
 * Read notes.
 */
NOTE_DATA *read_note( char *notefile, FILE *fpNote, BOARD_DATA *pBoard )
{
    NOTE_DATA *pNote;

    for (;;)
    {
	char letter;

	do
	{
	    letter = getc(fpNote);
	    if (feof(fpNote))
	    {
		fclose(fpNote);
		return NULL;
	    }
	}
	while (isspace(letter));
	ungetc(letter, fpNote);

	pNote		= (NOTE_DATA *) alloc_mem(sizeof(NOTE_DATA));

	if (str_cmp(fread_word(fpNote), "Sender"))
	    break;
	pNote->sender	= fread_string(fpNote);

	if (str_cmp(fread_word(fpNote), "Date"))
	    break;
	pNote->date	= fread_string(fpNote);

	if (str_cmp(fread_word(fpNote), "Stamp"))
	    break;
	pNote->stamp	= fread_number(fpNote);

	if (str_cmp(fread_word(fpNote), "Vnum"))
	    break;
	pNote->vnum	= fread_number(fpNote);

	if (str_cmp(fread_word(fpNote), "Subject"))
	    break;
	pNote->subject	= fread_string(fpNote);

	if (str_cmp(fread_word(fpNote), "Text"))
	    break;
	pNote->text	= fread_string(fpNote);

	pNote->next	= NULL;
	pNote->prev	= NULL;
	return pNote;
    }

    bbs_bug("Read_note: Bad key word '%s'", fread_word(fpNote));
    exit(1);
    return NULL;
}

/*
 * Read board.
 */
BOARD_DATA *read_board( char *boardfile, FILE *fpBoard )
{
    BOARD_DATA *pBoard;
    KICK_DATA *pKick;
    char *word = '\0';
    bool fMatch = FALSE;
    char letter;

    do
    {
	letter = getc(fpBoard);
	if (feof(fpBoard))
	{
	    fclose(fpBoard);
	    return NULL;
	}
    }
    while (isspace(letter));
    ungetc(letter, fpBoard);

    pBoard = (BOARD_DATA *) alloc_mem(sizeof(BOARD_DATA));

    for (;;)
    {
	word	= feof(fpBoard) ? "End" : fread_word(fpBoard);
	fMatch	= FALSE;

	switch (UPPER(word[0]))
	{
	    case '*':
		fMatch = TRUE;
		fread_to_eol(fpBoard);
		break;

	    case 'E':
		if (!str_cmp(word, "End"))
		{
		    pBoard->first_note	= NULL;
		    pBoard->last_note	= NULL;
		    pBoard->next	= NULL;
		    pBoard->prev	= NULL;
		    return pBoard;
		}
		break;

	    case 'C':
		KEY( "Capacity",  pBoard->capacity,	fread_number(fpBoard));
		break;

	    case 'I':
		KEY( "Info",	  pBoard->info,		fread_string(fpBoard));
		break;

	    case 'K':
		if (!str_cmp(word, "Kick"))
		{
		    pKick		=
			(KICK_DATA *) alloc_mem(sizeof(KICK_DATA));
		    pKick->next		= NULL;
		    pKick->prev		= NULL;
		    pKick->name		= str_dup(fread_word(fpBoard));
		    pKick->duration	= fread_number(fpBoard);
		    pKick->reason	= str_dup(fread_string(fpBoard));
		    LINK(pKick, pBoard->first_kick, pBoard->last_kick);
		    fMatch		= TRUE;
		    break;
		}
		break;

	    case 'L':
		KEY( "Long",	  pBoard->long_name,	fread_string(fpBoard));
		KEY( "Lastvnum",  pBoard->last_vnum,	fread_number(fpBoard));
		break;

	    case 'M':
		KEY( "Moderator", pBoard->moderator,	fread_string(fpBoard));
		break;

	    case 'S':
		KEY( "Short",	  pBoard->short_name,	fread_string(fpBoard));
		break;

	    case 'T':
		KEY( "Type",	  pBoard->type,		fread_number(fpBoard));
		break;

	    case 'V':
		KEY( "Vnum",	  pBoard->vnum,		fread_number(fpBoard));
		break;
	}

	if (!fMatch)
	{
	    bbs_bug("Read_board: No match '%s'", word);
	    exit(1);
	    return NULL;
	}
    }

    return pBoard;
}

/*
 * Initialize structures.  Load all boards and notes.
 */
void load_boards( void )
{
    char buf[INPUT];
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    FILE *fpBoard;
    FILE *fpNote;

    first_board	= NULL;
    last_board	= NULL;

    if (!(fpBoard = fopen(BOARD_FILE, "r")))
    {
	bbs_bug("Load_boards: Could not open to read %s", BOARD_FILE);
	exit(1);
	return;
    }

    while ((pBoard = read_board(BOARD_FILE, fpBoard)) != NULL)
    {
	LINK(pBoard, first_board, last_board);
	sprintf(buf, "%s%s.o", NOTE_DIR, pBoard->short_name);
	if ((fpNote = fopen(buf, "r")))
	{
	    while ((pNote = read_note(buf, fpNote, pBoard)) != NULL)
	    {
		LINK(pNote, pBoard->first_note, pBoard->last_note);
	    }
	}
    }

    log_string("Load_boards: Done");
    return;
}

/*
 * Save boards file.
 */
void save_boards( void )
{
    BOARD_DATA *pBoard;
    KICK_DATA *pKick;
    FILE *fpBoard;

    fclose(fpReserve);
    if (!(fpBoard = fopen(BOARD_FILE, "w")))
    {
	bbs_bug("Save_boards: Could not open to write %s", BOARD_FILE);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	fprintf(fpBoard, "Short     %s~\n",	pBoard->short_name	);
	fprintf(fpBoard, "Long      %s~\n",	pBoard->long_name	);
	fprintf(fpBoard, "Lastvnum  %ld\n",	pBoard->last_vnum	);
	fprintf(fpBoard, "Moderator %s~\n",	pBoard->moderator	);
	fprintf(fpBoard, "Type      %ld\n",	pBoard->type		);
	fprintf(fpBoard, "Vnum      %d\n",	pBoard->vnum		);
	fprintf(fpBoard, "Capacity  %d\n",	pBoard->capacity	);
	fprintf(fpBoard, "Info      %s~\n",	pBoard->info		);

	for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
	    fprintf(fpBoard, "Kick      %s %ld %s~\n", pKick->name,
		pKick->duration, pKick->reason);

	fprintf(fpBoard, "End\n"					);
    }

    fclose(fpBoard);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

void free_board( BOARD_DATA *pBoard )
{
    NOTE_DATA *pNote;
    KICK_DATA *pKick;

    if (!pBoard)
    {
	bbs_bug("Free_board: Null pBoard");
	return;
    }

    if (pBoard->first_note)
    {
	for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
	{
	    UNLINK(pNote, pBoard->first_note, pBoard->last_note);
	    free_note(pNote);
	}
    }

    if (pBoard->first_kick)
    {
	for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
	{
	    UNLINK(pKick, pBoard->first_kick, pBoard->last_kick);
	    free_kick(pKick);
	}
    }

    if (pBoard->short_name)	free_string(pBoard->short_name);
    if (pBoard->long_name)	free_string(pBoard->long_name);
    if (pBoard->moderator)	free_string(pBoard->moderator);
    if (pBoard->info)		free_string(pBoard->info);

    free_mem(pBoard, sizeof(BOARD_DATA));
    return;
}

void board_remove( BOARD_DATA *pBoard )
{
    if (!pBoard)
    {
	bbs_bug("Board_remove: Null pBoard");
	return;
    }

    UNLINK(pBoard, first_board, last_board);
    free_board(pBoard);
    save_boards( );
    return;
}

/*
 * Find user name in all forums (for moderate).
 */
BOARD_DATA * moderator_board( USER_DATA *usr )
{
    BOARD_DATA *pBoard;

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (LOWER(usr->name[0]) == LOWER(pBoard->moderator[0])
	&& !str_cmp(usr->name, pBoard->moderator))
	    return pBoard;
    }

    return NULL;
}

/*
 * Fix moderator names (upper/lower).
 */
void fix_moderator_name( USER_DATA *usr )
{
    BOARD_DATA *pBoard;

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (LOWER(usr->name[0]) == LOWER(pBoard->moderator[0])
	&& !str_cmp(usr->name, pBoard->moderator)
	&& strcmp(usr->name, pBoard->moderator))
	{
	    if (pBoard->moderator)
		free_string(pBoard->moderator);
	    pBoard->moderator = str_dup(usr->name);
	}
    }
}

/*
 * If user is a moderator.
 */
bool is_moderator( USER_DATA *usr )
{
    BOARD_DATA *pBoard;

    if (IS_ADMIN(usr))
	return TRUE;

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (LOWER(usr->name[0]) == LOWER(pBoard->moderator[0])
	&& !str_cmp(usr->name, pBoard->moderator))
	    return TRUE;
    }

    return FALSE;
}

/*
 * Adding a new board (admin command).
 */
void do_addforum( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char arg[INPUT];
    BOARD_DATA *oldBoard;
    BOARD_DATA *pBoard;

    smash_tilde(argument);
    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || argument[0] == '\0' || !is_number(argument))
    {
	syntax("addforum <forum name> <forum vnum>", usr);
	return;
    }

    if (strlen(arg) < 2 || strlen(arg) > 12)
    {
	send_to_user("Forum name must be 2 to 12 characters.\n\r", usr);
	return;
    }

    if (atoi(argument) < 0)
    {
	send_to_user("Illegal forum number.\n\r", usr);
	return;
    }

    if ((oldBoard = board_lookup(arg, FALSE)))
    {
	sprintf(buf, "%s forum is already exists.\n\r",
	    capitalize(oldBoard->short_name));
	send_to_user(buf, usr);
	return;
    }

    for (oldBoard = first_board; oldBoard; oldBoard = oldBoard->next)
    {
	if (board_number(oldBoard) == atoi(argument))
	{
	    sprintf(buf, "%s (%d) forum is already exists.\n\r",
		capitalize(oldBoard->short_name), oldBoard->vnum);
	    send_to_user(buf, usr);
	    return;
	}
    }

    pBoard		= (BOARD_DATA *) alloc_mem(sizeof(BOARD_DATA));
    pBoard->next	= NULL;
    pBoard->prev	= NULL;
    pBoard->short_name	= str_dup(arg);
    pBoard->long_name	= str_dup(capitalize(arg));
    pBoard->vnum	= atoi(argument);
    pBoard->moderator	= str_dup("None");
    pBoard->info	= str_dup("None");
    pBoard->capacity	= DEFAULT_CAPACITY;
    pBoard->type	= BOARD_NORMAL;
    pBoard->last_vnum	= 0;
    LINK(pBoard, first_board, last_board);
    send_to_user("Ok, creating new forum.\n\r", usr);
    sprintf(buf, "Forum name     : %s (%d)\n\r"
		 "Forum type     : Normal\n\r"
		 "Forum capacity : %d\n\r"
		 "Forum moderator: %s\n\r", pBoard->long_name,
	pBoard->vnum, pBoard->capacity, pBoard->moderator);
    send_to_user(buf, usr);
    send_to_user("Use 'setforum' command to set values.\n\r", usr);
    save_boards( );    
    return;
}

/*
 * Removing an old board (admin command).
 */
void do_delforum( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char arg[INPUT];
    BOARD_DATA *pBoard;
    USER_DATA *oUser;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	syntax("delforum <forum name>", usr);
	return;
    }

    if (!(pBoard = board_lookup(arg, FALSE)))
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }

    for (oUser = user_list; oUser; oUser = oUser->next)
    {
	if (oUser->pBoard == pBoard)
	    do_quit_org(oUser, "", FALSE);
    }

    sprintf(buf, "Ok, deleting %s forum.\n\r",
	capitalize(pBoard->short_name));
    send_to_user(buf, usr);
    board_remove(pBoard);
    return;
}

/*
 * Sets a forum values (admin command).
 */
void do_setforum( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char arg1[INPUT];
    char arg2[INPUT];
    BOARD_DATA *pBoard;
    BOARD_DATA *oBoard;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (argument[0] == '\0' || arg1[0] == '\0' || arg2[0] == '\0')
    {
	syntax("setforum <forum name> name|long|capacity|moderator|type <set>",
	    usr);
	return;
    }

    if (!(pBoard = board_lookup(arg1, FALSE)))
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }

    if (!str_cmp(arg2, "name"))
    {
	if (strlen(argument) < 2 || strlen(argument) > 12)
	{
	    send_to_user("Forum name must be 2 to 12 characters.\n\r", usr);
	    return;
	}

	if ((oBoard = board_lookup(argument, FALSE)))
	{
	    sprintf(buf, "%s forum is already exists.\n\r",
		capitalize(oBoard->short_name));
	    send_to_user(buf, usr);
	    return;
	}

	sprintf(buf, "Ok, %s forum renamed to %s.\n\r",
	    capitalize(pBoard->short_name), capitalize(argument));
	send_to_user(buf, usr);
	if (pBoard->short_name)
	    free_string(pBoard->short_name);
	if (pBoard->long_name)
	    free_string(pBoard->long_name);
	pBoard->short_name	= str_dup(argument);
	pBoard->long_name	= str_dup(capitalize(argument));
	save_boards( );
	return;
    }
    else if (!str_cmp(arg2, "long"))
    {
	if (strlen(argument) < 2 || strlen(argument) > 12)
	{
	    send_to_user("Forum name must be 2 to 12 characters.\n\r", usr);
	    return;
	}

	if (str_cmp(pBoard->short_name, argument))
	{
	    send_to_user("The long name must be the same with the short "
			 "name.\n\r", usr);
	    return;
	}

	sprintf(buf, "Ok, %s forum's long name now %s.\n\r",
	    capitalize(pBoard->short_name), argument);
	send_to_user(buf, usr);
	if (pBoard->long_name)
	    free_string(pBoard->long_name);
	pBoard->long_name	= str_dup(argument);
	save_boards( );
	return;
    }
    else if (!str_cmp(arg2, "capacity"))
    {
	if (!is_number(argument) || atoi(argument) < 1
	|| atoi(argument) > 1000)
	{
	    send_to_user("Illegal forum capacity number.\n\r", usr);
	    return;
	}

	pBoard->capacity = atoi(argument);
	sprintf(buf, "Ok, %s forum's note capacity now %d.\n\r",
	    capitalize(pBoard->short_name), pBoard->capacity);
	send_to_user(buf, usr);
	save_boards( );
	return;
    }
    else if (!str_cmp(arg2, "moderator"))
    {
	if (pBoard->type == BOARD_GAME || pBoard->type == BOARD_SECRET
	|| pBoard->type == BOARD_ADMIN)
	{
	    send_to_user("No moderator data on this forum.\n\r", usr);
	    return;
	}

	if (!str_cmp(argument, "none"))
	{
	    if (!str_cmp(pBoard->moderator, "None"))
	    {
		print_to_user(usr, "%s forum's moderator is already None.\n\r",
		    capitalize(pBoard->short_name));
		return;
	    }

	    print_to_user(usr, "Ok, %s forum's moderator is None now.\n\r",
		capitalize(pBoard->short_name));
	    if (pBoard->moderator)
		free_string(pBoard->moderator);
	    pBoard->moderator = str_dup("None");
	    save_boards( );
	    return;
	}
	else
	{
	    if (str_cmp(pBoard->moderator, "None"))
	    {
		sprintf(buf, "%s forum's moderator is already %s.\n\r"
			     "Type 'setforum %s moderator none' to "
			     " remove.\n\r", capitalize(pBoard->short_name),
		    pBoard->moderator, pBoard->short_name);
		send_to_user(buf, usr);
		return;
	    }

	    if (!is_user(argument))
	    {
		send_to_user("No such user.\n\r", usr);
		return;
	    }

	    sprintf(buf, "Ok, %s is new moderator of %s forum.\n\r",
		capitalize(argument), capitalize(pBoard->short_name));
	    send_to_user(buf, usr);
	    if (pBoard->moderator)
		free_string(pBoard->moderator);
	    pBoard->moderator = str_dup(capitalize(argument));
	    save_boards( );
	    return;
	}
    }
    else if (!str_cmp(arg2, "type"))
    {
	if (!str_cmp(argument, "normal"))
	{
	    if (pBoard->type == BOARD_NORMAL)
	    {
		send_to_user("Forum type is already normal.\n\r", usr);
		return;
	    }

	    sprintf(buf, "Added normal type to %s forum.\n\r",
		capitalize(pBoard->short_name));
	    send_to_user(buf, usr);
	    pBoard->type = BOARD_NORMAL;
	    save_boards( );
	    return;
	}
	else if (!str_cmp(argument, "game"))
	{
	    if (pBoard->type == BOARD_GAME)
	    {
		send_to_user("Forum type is already game.\n\r", usr);
		return;
	    }

	    sprintf(buf, "Added game type to %s forum.\n\r",
		capitalize(pBoard->short_name));
	    send_to_user(buf, usr);
	    pBoard->type = BOARD_GAME;
	    save_boards( );
	    return;
	}
	else if (!str_cmp(argument, "admin"))
	{
	    if (pBoard->type == BOARD_ADMIN)
	    {
		send_to_user("Forum type is already admin.\n\r", usr);
		return;
	    }

	    sprintf(buf, "Added admin type to %s forum.\n\r",
		capitalize(pBoard->short_name));
	    send_to_user(buf, usr);
	    pBoard->type = BOARD_ADMIN;
	    save_boards( );
	    return;
	}
	else if (!str_cmp(argument, "anonymous"))
	{
	    if (pBoard->type == BOARD_ANONYMOUS)
	    {
		send_to_user("Forum type is already anonymous.\n\r", usr);
		return;
	    }

	    sprintf(buf, "Added anonymous type to %s forum.\n\r",
		capitalize(pBoard->short_name));
	    send_to_user(buf, usr);
	    pBoard->type = BOARD_ANONYMOUS;
	    save_boards( );
	    return;
	}
	else if (!str_cmp(argument, "secret"))
	{
	    if (pBoard->type == BOARD_SECRET)
	    {
		send_to_user("Forum type is already secret.\n\r", usr);
		return;
	    }

	    sprintf(buf, "Added secret type to %s forum.\n\r",
		capitalize(pBoard->short_name));
	    send_to_user(buf, usr);
	    pBoard->type = BOARD_SECRET;
	    save_boards( );
	    return;
	}
	else
	{
	    send_to_user("That's not a valid forum type.\n\r"
		 "Available arguments are: normal, game, moderator, "
		 "admin, anonymous, secret.\n\r",
		usr);
	    return;
	}
    }
    else
    {
	syntax("setforum <forum name> name|long|capacity|moderator|type <set>",
	    usr);
	return;
    }
}

/*
 * Show all forum moderators (admin command).
 */
void do_showmods( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;
    BUFFER *output;
    char buf[STRING];
    int col = 0;
    bool found = FALSE;

    output = new_buf( );

    add_buf(output, "Forum name    Moderator      | ");
    add_buf(output, "Forum name    Moderator      |\n\r");
    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (str_cmp(pBoard->moderator, "None"))
	{
	    found = TRUE;
	    sprintf(buf, "#y%-10s #W-> #c%-12s#x   | ", pBoard->long_name,
		pBoard->moderator);
	    add_buf(output, buf);
	    if (++col % 2 == 0)
		add_buf(output, "\n\r");
	}
    }

    if (col % 2 != 0)
	add_buf(output, "\n\r");

    if (!found)
    {
	send_to_user("None.\n\r", usr);
	free_buf(output);
	return;
    }

    page_to_user(buf_string(output), usr);
    free_buf(output);
    return;
}

/*
 * Show forum stats (Memory etc.) (admin command).
 */
void do_statforum( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char arg[INPUT];
    int count_board = 0;
    int count_kick  = 0;
    int count_note  = 0;
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    KICK_DATA *pKick;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	for (pBoard = first_board; pBoard; pBoard = pBoard->next)
	{
	    count_board++;

	    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
		count_note++;

	    for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
		count_kick++;
	 }

	sprintf(buf, "Forum: %4d -- %5d bytes\n\r", count_board,
	    count_board * (sizeof(*pBoard)));
	send_to_user(buf, usr);

	sprintf(buf, "Note : %4d -- %5d bytes\n\r", count_note,
	    count_note * (sizeof(*pNote)));
	send_to_user(buf, usr);

	sprintf(buf, "Kick : %4d -- %5d bytes\n\r", count_kick,
	    count_kick * (sizeof(*pKick)));
	send_to_user(buf, usr);
	return;
    }

    if (!(pBoard = board_lookup(arg, FALSE)))
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }

    sprintf(buf, "%s (%d) forum stats -- %d bytes:\n\r", pBoard->long_name,
	pBoard->vnum, sizeof(*pBoard));
    send_to_user(buf, usr);

    count_note = count_kick = 0;

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
	count_note++;

    for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
	count_kick++;

    sprintf(buf, "* Note: %4d -- %5d bytes\n\r", count_note,
	count_note * (sizeof(*pNote)));
    send_to_user(buf, usr);

    sprintf(buf, "* Kick: %4d -- %5d bytes\n\r", count_kick,
	count_kick * (sizeof(*pKick)));
    send_to_user(buf, usr);

    sprintf(buf, "* Capacity: %d - Type: %s\n\r", pBoard->capacity,
	pBoard->type == BOARD_NORMAL	? "Normal"    :
	pBoard->type == BOARD_ADMIN	? "Admin"     :
	pBoard->type == BOARD_GAME	? "Game"      :
	pBoard->type == BOARD_ANONYMOUS	? "Anonymous" :
	pBoard->type == BOARD_SECRET	? "Secret"    : "*Undefined*");
    send_to_user(buf, usr);

    sprintf(buf, "* Moderator: %s\n\r", pBoard->moderator);
    send_to_user(buf, usr);
    return;
}

void do_jump( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char arg[INPUT];
    BOARD_DATA *pBoard;
    bool fNumber = FALSE;
    int i;
    time_t *last_read;

    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	send_to_user("Jump which forum?\n\r", usr);
	return;
    }

    if (is_number(arg))
	fNumber = TRUE;

    if (((!(pBoard = board_lookup(arg, FALSE))) && !fNumber)
    || ((!(pBoard = board_lookup(arg, TRUE))) && fNumber))
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }

    if (pBoard->type == BOARD_SECRET && !IS_ADMIN(usr))
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }

    if (usr->pBoard == pBoard)
    {
	print_to_user(usr, "You are already in %s.\n\r",
	    capitalize(pBoard->short_name));
	return;
    }

    if (pBoard == board_lookup("chat", FALSE))
    {
	if (!usr->Validated)
	{
	    send_to_user("Unvalidated users can't use chat room.\n\r", usr);
	    return;
	}

	send_to_user("You are in the chat room now.\n\r\n\r"
		     "Chat room commands are:\n\r"
		     "    /look     to list the users in chat room\n\r"
		     "    /emote    to emote\n\r"
		     "    /exit     to leave the chat room\n\r\n\r", usr);
	cmd_chat_emote(usr, "enters the chat room.");
	usr->pBoard = pBoard;
	cmd_chat_look(usr);
	return;
    }

    if (pBoard->type == BOARD_ADMIN || pBoard->type == BOARD_GAME)
    {
	if (usr->pBoard == board_lookup("chat", FALSE))
	{
	    cmd_chat_emote(usr, "leaves the chat room.");
	    send_to_user("You left the chat room.\n\r", usr);
	}

	sprintf(buf, "You are in the %s now.\n\r", pBoard->short_name);
	send_to_user(buf, usr);
	usr->pBoard = pBoard;
	return;
    }

    if (usr->pBoard == board_lookup("chat", FALSE))
    {
	cmd_chat_emote(usr, "leaves the chat room.");
	send_to_user("You left the chat room.\n\r", usr);
    }

    print_to_user(usr, "You are in the forum called %s.\n\r",
	pBoard->long_name);
    send_to_user("You can set up new notes with the command 'note <subject>'."
		 "\n\r"
		 "Read a note with 'read num'. You can clip a note to your"
		 "\n\r"
		 "clipboard with 'clip num'. And you can see notes with 'list'"
		 "\n\r"
		 "command. Type 'new' to read new notes.\n\r", usr);
    print_to_user(usr, "Forum moderator: %s\n\r", pBoard->moderator);

    i = total_notes(usr, pBoard);

    if (i > 0)
	print_to_user(usr, "Total %d message%s.\n\r", i, i > 1 ? "s" : "");

    i = unread_notes(usr, pBoard);

    if (i > 0)
	sprintf(buf, "%d NEW MESSAGE%s.\n\r", i, i > 1 ? "s" : "");
    else
	sprintf(buf, "NO NEW MESSAGES.\n\r");

    send_to_user(buf, usr);
    usr->pBoard = pBoard;
    if (!pBoard->last_note)
    {
	usr->current_note = NULL;
	return;
    }

    usr->current_note = pBoard->last_note;
    last_read = &usr->last_note[board_number(pBoard)];
    again:
    if (!usr->current_note)
    {
	if (pBoard->last_note)
	    usr->current_note = pBoard->last_note;
	else
	    usr->current_note = NULL;
	return;
    }

    if (*last_read < usr->current_note->stamp)
    {
	usr->current_note = usr->current_note->prev;
	goto again;
    }

    return;
}

void do_forumlist( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char forum_list[STRING];
    BUFFER *output;
    BOARD_DATA *pBoard;
    int col = 0;

    forum_list[0] = '\0';
    output	  = new_buf( );

    add_buf(output, "#GKnown forums:#x\n\r");

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (pBoard->type == BOARD_SECRET)
	    continue;

	sprintf(buf, "%s%s%s%2d. %-12s ",
	    is_name_full(pBoard->short_name, usr->zap)	   ? "#R" :
	    is_name_full(pBoard->short_name, usr->fnotify) ? "#W" : "#G",
	    can_write(usr, pBoard) ? " " : "*",
	    is_kicked(usr, pBoard, FALSE) ? "!" : " ",
	    pBoard->vnum, pBoard->long_name);
	strcat(forum_list, buf);
	if (++col % 4 == 0)
	    strcat(forum_list, "\n\r");
    }

    if (col % 4 != 0)
	strcat(forum_list, "\n\r");

    add_buf(output, forum_list);
    add_buf(output, "#x");
    page_to_user(buf_string(output), usr);
    free_buf(output);
    return;
}

void do_zap( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char zap_list[STRING];
    char arg[INPUT];
    BUFFER *output;
    BOARD_DATA *pBoard;
    int col = 0;

    one_argument(argument, arg);

    zap_list[0] = '\0';
    output	= new_buf( );

    if (arg[0] == '\0')
    {
	add_buf(output, "#GList of zapped forums:#R\n\r");

	for (pBoard = first_board; pBoard; pBoard = pBoard->next)
	{
	    if (is_name_full(pBoard->short_name, usr->zap))
	    {
		sprintf(buf, "%2d. %-10s   ", pBoard->vnum, pBoard->long_name);
		strcat(zap_list, buf);
		if (++col % 4 == 0)
		    strcat(zap_list, "\n\r");
	    }
	}

	if (col % 4 != 0)
	    strcat(zap_list, "\n\r");

	add_buf(output, zap_list);
	add_buf(output, "\n\r#xTry giving a forum name as an "
			"argument to this command.\n\r");
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }
    else
    {
	if (!(pBoard = board_lookup(arg, FALSE)))
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (pBoard->type == BOARD_SECRET)
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (is_name_full(pBoard->short_name, usr->zap))
	{
	    sprintf(buf, "You don't actuaclly read %s forum.\n\r",
		pBoard->long_name);
	    send_to_user(buf, usr);
	    return;
	}

	if (pBoard->type == BOARD_GAME || pBoard->type == BOARD_ADMIN)
	{
	    send_to_user("You can't zap this forum.\n\r", usr);
	    return;
	}

	sprintf(buf, "You zapped forum called %s.\n\r", pBoard->long_name);
	send_to_user(buf, usr);
	buf[0]	 = '\0';
	strcpy(buf, pBoard->short_name);
	strcat(buf, " ");
	strcat(buf, usr->zap);
	if (usr->zap)	free_string(usr->zap);
	usr->last_note[board_number(pBoard)] = 0;
	usr->zap = str_dup(buf);

	buf[0] = '\0';
	sprintf(buf, "-r %s", pBoard->short_name);
	if (is_name_full(pBoard->short_name, usr->fnotify))
	    do_fnotify(usr, buf);
	return;
    }
}

void do_fnotify( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char not_list[STRING];
    char arg[INPUT];
    BUFFER *output;
    BOARD_DATA *pBoard;
    int col = 0;
    bool found = FALSE;

    argument = one_argument(argument, arg);

    not_list[0] = '\0';
    buf[0]	= '\0';
    output	= new_buf( );

    if (arg[0] == '\0')
    {
	add_buf(output, "#GList of notified forums:#W\n\r");

	for (pBoard = first_board; pBoard; pBoard = pBoard->next)
	{
	    if (is_name_full(pBoard->short_name, usr->fnotify))
	    {
		found = TRUE;
		sprintf(buf, "%2d. %-10s   ", pBoard->vnum, pBoard->long_name);
		strcat(not_list, buf);
		if (++col % 4 == 0)
		    strcat(not_list, "\n\r");
	    }
	}

	if (col % 4 != 0)
	    strcat(not_list, "\n\r");

	if (!found)
	{
	    send_to_user(
		"You don't have any entry in you forum notify list.\n\r", usr);
	    free_buf(output);
	    return;
	}

	add_buf(output, not_list);
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }
    else if (!str_cmp(arg, "-r"))
    {
	if (argument[0] == '\0')
	{
	    syntax("fnotify -r <forum name>", usr);
	    return;
	}

	if (!(pBoard = board_lookup(argument, FALSE)))
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (pBoard->type == BOARD_SECRET)
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (!is_name_full(pBoard->short_name, usr->fnotify))
	{
	    sprintf(buf, "%s forum isn't in your forum notify list.\n\r",
		pBoard->long_name);
	    send_to_user(buf, usr);
	    return;
	}

	print_to_user(usr, "Forum notify #W%s#x removed.\n\r",
	    pBoard->long_name);
	buf[0] = '\0';
	sprintf(buf, "%s ", pBoard->short_name);
	if (is_name_full(buf, usr->fnotify))
	    usr->fnotify = string_replace(usr->fnotify, buf, "");
	return;
    }
    else if (!str_cmp(arg, "-f"))
    {
	send_to_user("Flushing the your forum notify list.\n\r", usr);
	if (usr->fnotify) free_string(usr->fnotify);
	usr->fnotify = str_dup("");
	send_to_user("Done.\n\r", usr);
	return;
    }
    else
    {
	if (!(pBoard = board_lookup(arg, FALSE)))
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (pBoard->type == BOARD_SECRET)
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (is_name_full(pBoard->short_name, usr->fnotify))
	{
	    sprintf(buf, "%s forum is already in your forum notify list.\n\r",
		pBoard->long_name);
	    send_to_user(buf, usr);
	    return;
	}

	if (pBoard->type == BOARD_GAME || pBoard->type == BOARD_ADMIN)
	{
	    send_to_user("You can't add your notify list this forum.\n\r", usr);
	    return;
	}

	sprintf(buf, "New forum notify: #W%s#x.\n\r", pBoard->long_name);
	send_to_user(buf, usr);
	buf[0]	 = '\0';
	strcpy(buf, pBoard->short_name);
	strcat(buf, " ");
	strcat(buf, usr->fnotify);
	if (usr->fnotify) free_string(usr->fnotify);
	usr->fnotify = str_dup(buf);

	buf[0] = '\0';
	sprintf(buf, "%s ", pBoard->short_name);
	if (is_name_full(buf, usr->zap))
	{
	    usr->zap = string_replace(usr->zap, buf, "");
	    print_to_user(usr, "%s forum unzapped.\n\r", pBoard->long_name);
	}
	return;
    }

    send_to_user("Try 'help fnotify' for more information.\n\r", usr);
    return;
}

void do_info( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    BUFFER *output;
    BOARD_DATA *pBoard;

    output = new_buf( );
    pBoard = usr->pBoard;

    if (pBoard->type == BOARD_GAME || pBoard->type == BOARD_SECRET)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    sprintf(buf, "Forum name     : %s\n\r"
		 "Forum moderator: %s\n\r"
		 "Total notes    : %d\n\r"
		 "Unread notes   : %d\n\r"
		 "Forum info     :\n\r", pBoard->long_name,
	pBoard->moderator, total_notes(usr, pBoard),
	unread_notes(usr, pBoard));
    add_buf(output, buf);
    add_buf(output, pBoard->info);
    add_buf(output, "\n\r");
    page_to_user_bw(buf_string(output), usr);
    free_buf(output);
    return;
}

void do_list( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    BUFFER *output;
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    bool fName	= FALSE;
    bool found	= FALSE;
    int vnum	= 0;

    output	= new_buf( );
    pBoard	= usr->pBoard;

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (argument[0] != '\0')
    {
	if (!is_user(argument))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	fName = TRUE;
    }

    sprintf(buf, "#GThe Bulletin Board contains #Y%d #Gnotes:#x\n\r\n\r",
	total_notes(usr, pBoard));
    add_buf(output, buf);

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
    {
	vnum++;

	if (fName && str_cmp(pNote->sender, argument))
	    continue;

	if (is_ignore(usr, pNote->sender))
	    continue;

	sprintf(buf, "%3d #m##%-6ld #C%-12s  #G%-26s #y%s#x\n\r", vnum,
	    pNote->vnum, pNote->sender, pNote->subject, pNote->date);
	add_buf(output, buf);
	found = TRUE;
    }

    if (found)
    {
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    free_buf(output);
    send_to_user("No such message.\n\r", usr);
    return;
}

void do_remove_org( USER_DATA *usr, char *argument, bool fMessage )
{
    char buf[STRING];
    char arg[INPUT];
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    bool fNumber = FALSE;
    char *arg_num;
    int vnum = 0;
    int anum = 0;

    one_argument(argument, arg);
    pBoard	= usr->pBoard;
    arg_num	= str_dup(arg);

    if (pBoard->type == BOARD_GAME)
    {
	if (fMessage)
	    send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (pBoard->type == BOARD_ADMIN && !IS_ADMIN(usr))
    {
	if (fMessage)
	    send_to_user(
		"Users are not allowed to remove notes in this forum.\n\r",
		usr);
	return;
    }

    if (arg[0] == '\0')
    {
	if (fMessage)
	    send_to_user("Remove which note?\n\r", usr);
	return;
    }

    if (arg[0] == '#')
    {
	fNumber = TRUE;
	arg_num++;
    }

    if (!is_number(arg_num))
    {
	if (fMessage)
	    send_to_user("Remove which note?\n\r", usr);
	return;
    }

    anum = atoi(arg_num);

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
    {
	if ((++vnum == anum && !fNumber) || (fNumber && anum == pNote->vnum))
	{
	    if (can_remove(usr, pNote))
	    {
		sprintf(buf, "You remove the note %s (%s).\n\r",
		    pNote->sender, pNote->subject);
		if (fMessage)
		    send_to_user(buf, usr);
		note_remove(usr, pNote);
		return;
	    }
	    else
	    {
		if (fMessage)
		    send_to_user("Sorry, you will have to ask a moderator to "
				 "remove the message for you.\n\r", usr);
		return;
	    }
	}
    }

    if (fMessage)
	send_to_user("No such message.\n\r", usr);
    return;
}

void do_remove( USER_DATA *usr, char *argument )
{
    do_remove_org(usr, argument, TRUE);
}

void do_newmsgs( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char nw_buf[STRING];
    char no_buf[STRING];
    char fr_buf[INPUT];
    char zap[INPUT];
    BUFFER *output;
    BOARD_DATA *pBoard;
    bool foundNew = FALSE;
    bool foundNo  = FALSE;
    int col_new = 0;
    int col_no  = 0;
    int i;

    output	= new_buf( );
    strcpy(nw_buf, "");
    strcpy(no_buf, "");

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	sprintf(zap, "%s ", pBoard->short_name);

	if (pBoard->type != BOARD_GAME && pBoard->type != BOARD_SECRET
	&& !is_name_full(zap, usr->zap)
	&& board_number(pBoard) < 100
	&& unread_notes(usr, pBoard) > 0)
	{
	    foundNew = TRUE;
	    strcpy(fr_buf, "");
	    for (i = 0; i < (13 - strlen(pBoard->long_name)); i++)
		strcat(fr_buf, ".");

	    sprintf(buf, "%s%s%s%2d. %s#W%s#c%-3d  ",
		is_name_full(pBoard->short_name, usr->fnotify) ? "#W" : "#Y",
		can_write(usr, pBoard) ? " " : "*",
		is_kicked(usr, pBoard, FALSE) ? "!" : " ",
		pBoard->vnum, pBoard->long_name, fr_buf,
		unread_notes(usr, pBoard));
	    strcat(nw_buf, buf);
	    if (++col_new % 3 == 0)
		strcat(nw_buf, "\n\r");
	}
	else if (pBoard->type != BOARD_GAME && pBoard->type != BOARD_SECRET
	&& !is_name_full(zap, usr->zap))
	{
	    foundNo = TRUE;
	    sprintf(buf, "%s%s%s%2d. %-12s  ",
		is_name_full(pBoard->short_name, usr->fnotify) ? "#W" : "#R",
		can_write(usr, pBoard) ? " " : "*",
		is_kicked(usr, pBoard, FALSE) ? "!" : " ",
		pBoard->vnum, pBoard->long_name);
	    strcat(no_buf, buf);
	    if (++col_no % 4 == 0)
		strcat(no_buf, "\n\r");
	}
    }

    if (col_new % 3 != 0)
	strcat(nw_buf, "\n\r");
    if (col_no % 4 != 0)
	strcat(no_buf, "\n\r");

    if (foundNew)
    {
	add_buf(output, "#GNEW MESSAGES in:#x\n\r");
	add_buf(output, nw_buf);
    }

    if (!foundNo)
    {
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    if (foundNew)
	add_buf(output, "\n\r#GNO NEW MESSAGES in:#x\n\r");
    else
	add_buf(output, "#GNO NEW MESSAGES in:#x\n\r");

    add_buf(output, no_buf);
    add_buf(output, "#x");
    page_to_user(buf_string(output), usr);
    free_buf(output);
    return;
}

void do_read( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char arg[INPUT];
    BUFFER *output;
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    bool fNumber = FALSE;
    char *arg_num;
    int vnum = 0;
    int anum = 0;
    time_t *last_read;

    one_argument(argument, arg);
    arg_num	= str_dup(arg);
    pBoard	= usr->pBoard;
    output	= new_buf( );

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_user("Read which note?\n\r", usr);
	return;
    }

    if (arg[0] == '#')
    {
	fNumber = TRUE;
	arg_num++;
    }

    if (!is_number(arg_num))
    {
	send_to_user("Read which note?\n\r", usr);
	return;
    }

    anum = atoi(arg_num);

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
    {
	if ((++vnum == anum && !fNumber) || (fNumber && anum == pNote->vnum))
	{
	    sprintf(buf, "%d #m##%ld  #C%s  #y%s, #GSubject: #x%s#x\n\r\n\r",
		vnum, pNote->vnum, pNote->sender, pNote->date,
		pNote->subject);
	    add_buf(output, buf);
	    add_buf(output, pNote->text);
	    add_buf(output, "#x");
	    page_to_user(buf_string(output), usr);
	    free_buf(output);

	    if (!str_cmp(usr->lastCommand, "next")
	    || !str_cmp(usr->lastCommand, "previous"))
	    {
		last_read = &usr->last_note[board_number(pBoard)];
		if (*last_read >= pNote->stamp)
		    update_read(usr, pNote);
	    }
	    else
		update_read(usr, pNote);

	    usr->current_note = pNote;

	    sprintf(buf, "%s ", pBoard->short_name);
	    if (is_name_full(buf, usr->zap))
		usr->zap = string_replace(usr->zap, buf, "");
	    return;
	}
    }

    send_to_user("No such message.\n\r", usr);
    return;
}

void do_readall( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;

    for (pBoard = first_board; pBoard; pBoard = pBoard->next)
    {
	if (!is_name_full(pBoard->short_name, usr->zap)
	&&  pBoard->type != BOARD_GAME && pBoard->type != BOARD_SECRET)
	    usr->last_note[board_number(pBoard)] = current_time;
    }

    if (usr->pBoard->last_note)
	usr->current_note = usr->pBoard->last_note;
    else
	usr->current_note = NULL;
    send_to_user("You read all forums.\n\r", usr);
    return;
}

void do_next( USER_DATA *usr, char *argument )
{
    NOTE_DATA *pNote;
    char buf[INPUT];

    if (usr->pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (!usr->current_note)
    {
	send_to_user("No such message.\n\r", usr);
	return;
    }

    pNote = usr->current_note->next;

    if (pNote)
    {
	sprintf(buf, "#%ld", pNote->vnum);
	do_read(usr, buf);
	return;
    }	

    send_to_user("No such message.\n\r", usr);
    return;
}

void do_previous( USER_DATA *usr, char *argument )
{
    NOTE_DATA *pNote;
    char buf[INPUT];

    if (usr->pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (!usr->current_note)
    {
	send_to_user("No such message.\n\r", usr);
	return;
    }

    pNote = usr->current_note->prev;

    if (pNote)
    {
	sprintf(buf, "#%ld", pNote->vnum);
	do_read(usr, buf);
	return;
    }	

    send_to_user("No such message.\n\r", usr);
    return;
}

void do_readlast( USER_DATA *usr, char *argument )
{
    NOTE_DATA *pNote;
    char buf[INPUT];

    if (usr->pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    pNote  = usr->pBoard->last_note;

    if (pNote)
    {
	sprintf(buf, "#%ld", pNote->vnum);
	do_read(usr, buf);
	return;
    }

    send_to_user("No such message.\n\r", usr);
    return;
}

void do_search( USER_DATA *usr, char *argument )
{
    char arg[INPUT];
    char buf[STRING];
    BOARD_DATA *pBoard;
    BUFFER *output;
    NOTE_DATA *pNote;
    bool found		= FALSE;
    int vnum		= 0;
    
    one_argument(argument, arg);

    pBoard = usr->pBoard;
    output = new_buf( );

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (arg[0] == '\0')
    {
	syntax("search <string>", usr);
	return;
    }

    sprintf(buf, "#GThe following notes in #Y%s #Gcontain "
		 "the search string:\n\r", pBoard->long_name);
    add_buf(output, buf);

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
    {
	vnum++;

	if (strstr(pNote->text, arg) == NULL)
	    continue;

	sprintf(buf, "%3d #m##%-6ld #C%-12s  #G%-26s #y%s#x\n\r", vnum,
	    pNote->vnum, pNote->sender, pNote->subject, pNote->date);
	add_buf(output, buf);
	found = TRUE;
    }

    if (found)
    {
	page_to_user(buf_string(output), usr);
	free_buf(output);
	return;
    }

    free_buf(output);
    print_to_user(usr, "%s doesn't contain the search string.\n\r",
	pBoard->long_name);
    return;
}


void do_new( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    BUFFER *output;
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    int vnum = 1;
    time_t *last_read;

    pBoard	= usr->pBoard;
    output	= new_buf( );
    last_read	= &usr->last_note[board_number(pBoard)];

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next, vnum++)
    {
	if (is_ignore(usr, pNote->sender))
	    continue;

	if (*last_read < pNote->stamp)
	    break;
    }

    if (pNote)
    {
	sprintf(buf, "%d #m##%ld  #C%s  #y%s, #GSubject: #x%s#x\n\r\n\r",
	    vnum, pNote->vnum, pNote->sender, pNote->date, pNote->subject);
	add_buf(output, buf);
	add_buf(output, pNote->text);
	add_buf(output, "#x");
	page_to_user(buf_string(output), usr);
	free_buf(output);
	update_read(usr, pNote);
	usr->current_note = pNote;

	sprintf(buf, "%s ", pBoard->short_name);
	if (is_name_full(buf, usr->zap))
	    usr->zap = string_replace(usr->zap, buf, "");
	return;
    }

    send_to_user("No new messages.\n\r", usr);
    return;
}

bool is_kicked( USER_DATA *usr, BOARD_DATA *pBoard, bool fMessage )
{
    char buf[STRING];
    KICK_DATA *pKick;
    int remove;

    for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
    {
	remove = (int) pKick->duration - current_time;

	if (!str_cmp(pKick->name, usr->name) && pKick->duration >= current_time)
	{
	    if (fMessage)
	    {
		sprintf(buf, "You are kicked due to %s\n\r"
			     "Removal date: %s\n\r", pKick->reason,
		    get_age(remove, FALSE));
		send_to_user(buf, usr);
	    }
	    return TRUE;
	}
	else if (!str_cmp(pKick->name, usr->name)
	&& pKick->duration < current_time && fMessage)
	{
	    UNLINK(pKick, usr->pBoard->first_kick, usr->pBoard->last_kick);
	    free_kick(pKick);
	    save_boards( );
	    send_to_user("Your kick removed automatically.\n\r", usr);
	    return FALSE;
	}
	else
	    return FALSE;
    }

    return FALSE;
}

void do_kick( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;
    KICK_DATA *pKick;
    KICK_DATA *oKick;
    char arg1[INPUT];
    char arg2[INPUT];
    char arg3[INPUT];
    char buf[STRING];

    smash_tilde(argument);

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (IS_ADMIN(usr))
	argument = one_argument(argument, arg3);

    if (arg1[0] == '\0' || arg2[0] == '\0' || argument[0] == '\0'
    || (IS_ADMIN(usr) && arg3[0] == '\0'))
    {
	if (IS_ADMIN(usr))
	    syntax("kick <forum name> <nickname> <duration> <reason>", usr);
	else
	    syntax("kick <nickname> <duration> <reason>", usr);
	return;
    }

    if (!IS_ADMIN(usr))
    {
	pBoard = moderator_board(usr);

	if (!is_user(arg1))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	if (atoi(arg2) < 1 || atoi(arg2) > 99)
	{
	    send_to_user("Kick duration must be 1 (day) to 99 (day).\n\r", usr);
	    return;
	}

	if (strlen(argument) > 20)
	{
	    send_to_user("Reason too long (Maximum 20 characters).\n\r", usr);
	    return;
	}

	for (oKick = pBoard->first_kick; oKick; oKick = oKick->next)
	{
	    if (!str_cmp(oKick->name, arg1))
	    {
		sprintf(buf, "%s is already kicked on %s forum.\n\r",
		    oKick->name, pBoard->long_name);
		send_to_user(buf, usr);
		return;
	    }
	}

	pKick		= (KICK_DATA *) alloc_mem(sizeof(KICK_DATA));
	pKick->next	= NULL;
	pKick->prev	= NULL;
	pKick->name	= str_dup(capitalize(arg1));
	pKick->duration	= (int) current_time + (atoi(arg2) * 3600 * 24);
	pKick->reason	= str_dup(argument);
	LINK(pKick, pBoard->first_kick, pBoard->last_kick);
	save_boards( );
	sprintf(buf, "Ok, %s has been kicked.\n\r", pKick->name);
	send_to_user(buf, usr);
	return;
    }
    else if (IS_ADMIN(usr))
    {
	if (!(pBoard = board_lookup(arg1, FALSE)))
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	if (!is_user(arg2))
	{
	    send_to_user("No such user.\n\r", usr);
	    return;
	}

	if (atoi(arg3) < 1 || atoi(arg3) > 99)
	{
	    send_to_user("Kick duration must be 1 (day) to 99 (day).\n\r", usr);
	    return;
	}

	if (strlen(argument) > 20)
	{
	    send_to_user("Reason too long (Maximum 20 characters).\n\r", usr);
	    return;
	}

	for (oKick = pBoard->first_kick; oKick; oKick = oKick->next)
	{
	    if (!str_cmp(oKick->name, arg2))
	    {
		sprintf(buf, "%s is already kicked on %s forum.\n\r",
		    oKick->name, pBoard->long_name);
		send_to_user(buf, usr);
		return;
	    }
	}

	pKick		= (KICK_DATA *) alloc_mem(sizeof(KICK_DATA));
	pKick->next	= NULL;
	pKick->prev	= NULL;
	pKick->name	= str_dup(capitalize(arg2));
	pKick->duration	= (int) current_time + (atoi(arg3) * 3600 * 24);
	pKick->reason	= str_dup(argument);
	LINK(pKick, pBoard->first_kick, pBoard->last_kick);
	save_boards( );
	sprintf(buf, "Ok, %s has been kicked.\n\r", pKick->name);
	send_to_user(buf, usr);
	return;
    }
    else
    {
	bbs_bug("Do_kick: User is not admin or moderator");
	send_to_user("ERROR: User is not admin or moderator.\n\r", usr);
	return;
    }
}

void do_unkick( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;
    KICK_DATA *pKick;
    char buf[STRING];
    char arg[INPUT];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0' || (IS_ADMIN(usr) && argument[0] == '\0'))
    {
	if (IS_ADMIN(usr))
	    syntax("unkick <forum name> <nickname>", usr);
	else
	    syntax("unkick <nickname>", usr);
	return;
    }

    if (!IS_ADMIN(usr))
    {
	pBoard = moderator_board(usr);

	for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
	{
	    if (!str_cmp(pKick->name, arg))
	    {
		sprintf(buf, "Ok, %s has been unkicked.\n\r", pKick->name);
		send_to_user(buf, usr);
		UNLINK(pKick, pBoard->first_kick, pBoard->last_kick);
		free_kick(pKick);
		save_boards( );
		return;
	    }
	}

	sprintf(buf, "%s is not kicked.\n\r", capitalize(arg));
	send_to_user(buf, usr);
	return;
    }
    else if (IS_ADMIN(usr))
    {
	if (!(pBoard = board_lookup(arg, FALSE)))
	{
	    send_to_user("No such forum.\n\r", usr);
	    return;
	}

	for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
	{
	    if (!str_cmp(pKick->name, argument))
	    {
		sprintf(buf, "Ok, %s has been unkicked.\n\r", pKick->name);
		send_to_user(buf, usr);
		UNLINK(pKick, pBoard->first_kick, pBoard->last_kick);
		free_kick(pKick);
		save_boards( );
		return;
	    }
	}

	sprintf(buf, "%s is not kicked.\n\r", capitalize(arg));
	send_to_user(buf, usr);
	return;
    }
    else
    {
	bbs_bug("Do_unkick: User is not admin or moderator");
	send_to_user("ERROR: User is not admin or moderator.\n\r", usr);
	return;
    }
}

void do_note( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;

    pBoard = usr->pBoard;

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (pBoard->type == BOARD_ADMIN && !IS_ADMIN(usr))
    {
	send_to_user("Users are not allowed to write notes in this forum.\n\r",
	    usr);
	return;
    }

    if (is_kicked(usr, pBoard, TRUE))
	return;

    if (argument[0] == '\0')
    {
	EDIT_MODE(usr) = EDITOR_NOTE_SUBJECT;
	return;
    }
    else
    {
	smash_tilde(argument);

	if (strlen(argument) > 26)
	{
	    send_to_user("Subject too long.\n\r", usr);
	    return;
	}

	note_attach(usr, FALSE);
	if (usr->pNote->subject)
	    free_string(usr->pNote->subject);
	usr->pNote->subject	= str_dup(argument);
	EDIT_MODE(usr)		= EDITOR_NOTE;
	string_edit(usr, &usr->pNote->text);
    }
}

void do_anonnote( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;

    pBoard = usr->pBoard;

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (pBoard->type == BOARD_ADMIN && !IS_ADMIN(usr))
    {
	send_to_user("Users are not allowed to write notes in this forum.\n\r",
	    usr);
	return;
    }

    if (pBoard->type != BOARD_ANONYMOUS)
    {
	send_to_user("Anonymous posting is not allowed in this forum.\n\r",
	    usr);
	return;
    }

    if (is_kicked(usr, pBoard, TRUE))
	return;

    if (argument[0] == '\0')
    {
	syntax("anonnote <subject>", usr);
	return;
    }
    else
    {
	smash_tilde(argument);

	if (strlen(argument) > 26)
	{
	    send_to_user("Subject too long.\n\r", usr);
	    return;
	}

	note_attach(usr, TRUE);
	if (usr->pNote->subject)
	    free_string(usr->pNote->subject);
	usr->pNote->subject	= str_dup(argument);
	EDIT_MODE(usr)		= EDITOR_NOTE;
	string_edit(usr, &usr->pNote->text);
    }
}

void do_sendclip( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char name[INPUT];
    FILE *fpClip;
    FILE *fpMail;

    if (!str_cmp(usr->sClip, "#UNSET"))
    {
	send_to_user("No messages in your clipboard.\n\r", usr);
	return;
    }

    sprintf(name, "%s%s.clp", CLIP_DIR, capitalize(usr->name));
    if (!(fpClip = fopen(name, "r")))
    {
	send_to_user("No messages in your clipboard.\n\r", usr);
	bbs_bug("Do_sendclip: Could not open to read %s", name);
	return;
    }
    fclose(fpClip);

    sprintf(buf, "/usr/bin/sendmail -t < %s", name);
    if (!(fpMail = popen(buf, "r")))
    {
	bbs_bug("Do_sendclip: Could not open to read %s", buf);
	send_to_user("ERROR: Could not execute mail command.\n\r", usr);
	return;
    }
    pclose(fpMail);
    send_to_user("Sending.\n\r", usr);
    return;
}

void do_showclip( USER_DATA *usr, char *argument )
{
    char buf[4 * STRING];

    buf[0] = '\0';

    if (!str_cmp(usr->sClip, "#UNSET"))
    {
	send_to_user("No messages in your clipboard.\n\r", usr);
	return;
    }
    else
    {
	sprintf(buf, "*** Begin ***\n\r%s\n\r*** End of clipboard ***\n\r",
	    usr->sClip);
	page_to_user_bw(buf, usr);
	return;
    }
}

void do_clip( USER_DATA *usr, char *argument )
{
    char buf[4 * STRING];
    char arg[INPUT];
    BOARD_DATA *pBoard;
    NOTE_DATA *pNote;
    bool fNumber = FALSE;
    FILE *fpClip;
    char *arg_num;
    int vnum = 0;
    int anum = 0;

    one_argument(argument, arg);
    arg_num	= str_dup(arg);
    pBoard	= usr->pBoard;

    if (pBoard->type == BOARD_GAME)
    {
	send_to_user("Unknown command.\n\r", usr);
	return;
    }

    if (arg[0] == '\0')
    {
	send_to_user("Clip which note?\n\r", usr);
	return;
    }

    if (arg[0] == '#')
    {
	fNumber = TRUE;
	arg_num++;
    }

    if (!is_number(arg_num))
    {
	send_to_user("Clip which note?\n\r", usr);
	return;
    }

    anum = atoi(arg_num);

    for (pNote = pBoard->first_note; pNote; pNote = pNote->next)
    {
	if ((++vnum == anum && !fNumber) || (fNumber && anum == pNote->vnum))
	{
	    fclose(fpReserve);
	    sprintf(buf, "%s%s.clp", CLIP_DIR, capitalize(usr->name));
	    if (!(fpClip = fopen(buf, "w")))
	    {
		bbs_bug("Do_clip: Could not open to write %s", buf);
		send_to_user(
		    "ERROR: Could not open to save your clip file.\n\r", usr);
		fpReserve = fopen(NULL_FILE, "r");
		return;
	    }

	    sprintf(buf, "%d #%ld  %s  %s, Subject: %s\n\r\n\r%s",
		vnum, pNote->vnum, pNote->sender, pNote->date,
		pNote->subject, pNote->text);
	    if (usr->sClip)	free_string(usr->sClip);
	    usr->sClip = str_dup(buf);
	    fprintf(fpClip, "From: %s\nTo: %s\n"
			    "Errors-To: %s\n"
			    "Subject: [CLIPBOARD] %s\n\n"
			    "Sender: %s\nDate: %s\n-----\n%s\n------\n"
			    "%s BBS - %s %d\n"
			    "%s\n",
		config.bbs_email, usr->email, config.bbs_email, pNote->subject,
		pNote->sender, pNote->date, pNote->text, config.bbs_name,
		config.bbs_host, config.bbs_port, config.bbs_email);
	    fclose(fpClip);
	    fpReserve = fopen(NULL_FILE, "r");
	    send_to_user("Clipped.\n\r", usr);
	    return;
	}
    }

    send_to_user("No such message.\n\r", usr);
    return;
}

void do_Who( USER_DATA *usr, char *argument )
{
    char buf[STRING];
    char cBuf[STRING];
    char for_buf[INPUT];
    char arg[INPUT];
    BOARD_DATA *pBoard;
    BUFFER *output;
    DESC_DATA *d;
    int nMatch = 0;
    int col = 0;
    bool noArg = FALSE;
    int i;

    one_argument(argument, arg);

    pBoard = board_lookup(arg, FALSE);

    if (arg[0] == '\0')
	noArg = TRUE;
    else if (!pBoard)
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }

    output = new_buf( );
    strcpy(cBuf, "");

    for (d = desc_list; d; d = d->next)
    {
	if ((d->login != CON_LOGIN) || !USR(d)
	|| (IS_TOGGLE(USR(d), TOGGLE_INVIS) && !IS_ADMIN(usr))
	|| (!USR(d)->Validated && !IS_ADMIN(usr)))
	    continue;

	if (!noArg && USR(d)->pBoard != pBoard)
	    continue;

	nMatch++;

	strcpy(for_buf, "");
	for (i = 1; i < (13 - strlen(USR(d)->name)); i++)
	    strcat(for_buf, "_");

	sprintf(buf, "#c%s%s%s%s#W%s#c%-10s ",
	    IS_TOGGLE(USR(d), TOGGLE_IDLE) ? "%" : " ",
	    IS_TOGGLE(USR(d), TOGGLE_XING) ? " " : "*",
	    !USR(d)->Validated ? "#D" :
	    is_friend(usr, USR(d)->name) ? "#C" :
	    is_notify(usr, USR(d)->name) ? "#G" :
	    is_enemy(usr, USR(d)->name)  ? "#R" : "#Y",
	    USR(d)->name, for_buf,
	    (EDIT_MODE(USR(d)) == EDITOR_MAIL_SUBJECT
	    || EDIT_MODE(USR(d)) == EDITOR_MAIL_WRITE
	    || EDIT_MODE(USR(d)) == EDITOR_MAIL) ? "<Mail>" :
	    USR(d)->pBoard->type == BOARD_SECRET ? "<Secret>" :
	    USR(d)->pBoard->long_name);
	strcat(cBuf, buf);
	if (++col % 3 == 0)
	    strcat(cBuf, "\n\r");
    }

    if (col % 3 != 0)
	strcat(cBuf, "\n\r");

    if (nMatch < 1 && !noArg)
    {
	print_to_user(usr, "There is no one in %s forum.\n\r",
	    pBoard->long_name);
	return;
    }

    sprintf(buf, "#Y%s BBS Who List (#GTotal #Y%d #Guser%s#Y):\n\r"
		 "#W=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n\r",
	config.bbs_name, nMatch, nMatch > 1 ? "s" : "");
    add_buf(output, buf);
    add_buf(output, cBuf);
    add_buf(output, "#x");
    page_to_user(buf_string(output), usr);
    free_buf(output);
    return;
}

void do_look( USER_DATA *usr, char *argument )
{
    if (usr->pBoard == board_lookup("chat", FALSE))
    {
	cmd_chat_look(usr);
	return;
    }

    do_Who(usr, usr->pBoard->short_name);
    return;
}

void edit_info_answer( USER_DATA *usr, char *argument )
{
    while (isspace(*argument))
	argument++;

    usr->timer = 0;

    switch (*argument)
    {
	case 'y': case 'Y':
	    string_edit(usr, &moderator_board(usr)->info);
	    EDIT_MODE(usr) = EDITOR_INFO;
	    break;

	default:
	    send_to_user("Aborted.\n\r", usr);
	    EDIT_MODE(usr) = EDITOR_NONE;
	    break;
    }
}

void do_foruminfo( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;
    BUFFER *output;

    pBoard = moderator_board(usr);

    if (!pBoard)
    {
	send_to_user("ERROR: No such forum.\n\r", usr);
	bbs_bug("Do_foruminfo: Null pBoard");
	return;
    }

    if (str_cmp(pBoard->info, "None"))
    {
	output = new_buf( );

	add_buf(output, "Currently forum info is:\n\r");
	add_buf(output, pBoard->info);
	page_to_user_bw(buf_string(output), usr);
	free_buf(output);
	EDIT_MODE(usr) = EDITOR_INFO_ANSWER;
	return;
    }
    else
    {
	string_edit(usr, &pBoard->info);
	EDIT_MODE(usr) = EDITOR_INFO;
	return;
    }
}

void do_stats( USER_DATA *usr, char *argument )
{
    BOARD_DATA *pBoard;
    KICK_DATA *pKick;
    char buf[STRING];
    BUFFER *output;
    char arg[INPUT];
    int remove;

    one_argument(argument, arg);

    if (arg[0] == '\0' && IS_ADMIN(usr))
    {
	syntax("stats <forum name>", usr);
	return;
    }

    pBoard = IS_ADMIN(usr) ? board_lookup(arg, FALSE) : moderator_board(usr);

    if (!pBoard)
    {
	send_to_user("No such forum.\n\r", usr);
	return;
    }
    
    output = new_buf( );

    sprintf(buf, "Forum name  : %s\n\r"
		 "Kicked users:\n\r", pBoard->long_name);
    add_buf(output, buf);

    for (pKick = pBoard->first_kick; pKick; pKick = pKick->next)
    {
	remove = (int) pKick->duration - current_time;

	if (pKick->duration >= current_time)
	{
	    sprintf(buf, "Name: %-12s Rem. date: %s Reason: %s\n\r",
		pKick->name, get_age(remove, FALSE), pKick->reason);
	    add_buf(output, buf);
	}
    }

    sprintf(buf, "Total notes : %d\n\r"
		 "First note  : %s, %s [%s]\n\r"
		 "Last note   : %s, %s [%s]\n\r", total_notes(usr, pBoard),
	pBoard->first_note ? pBoard->first_note->sender : "None",
	pBoard->first_note ? pBoard->first_note->subject : "",
	pBoard->first_note ? pBoard->first_note->date : "",
	pBoard->last_note ? pBoard->last_note->sender : "None",
	pBoard->last_note ? pBoard->last_note->subject : "",
	pBoard->last_note ? pBoard->last_note->date : "");
    add_buf(output, buf);
    page_to_user_bw(buf_string(output), usr);
    free_buf(output);
    return;
}

void edit_note_free( USER_DATA *usr )
{
    if (usr->pNote != NULL)
    {
        free_note(usr->pNote);
        usr->pNote = NULL;
    }
}

void edit_note_subject( USER_DATA *usr, char *argument )
{
    while (isspace(*argument))
	argument++;

    usr->timer = 0;

    smash_tilde(argument);
    note_attach(usr, FALSE);

    if (argument[0] == '\0')
    {
	EDIT_MODE(usr) = EDITOR_NONE;
	return;
    }
    else if (strlen(argument) > 26)
    {
	send_to_user("Subject too long.\n\r", usr);
	return;
    }

    if (usr->pNote->subject)
	free_string(usr->pNote->subject);
    usr->pNote->subject	= str_dup(argument);
    EDIT_MODE(usr)	= EDITOR_NOTE;
    string_edit(usr, &usr->pNote->text);
}

/*
 * Send new note.
 */
void edit_send_note( USER_DATA *usr )
{
    char buf[INPUT];
    char *Strtime;
    int msg_150, msg;

    Strtime			= ctime(&current_time);
    Strtime[strlen(Strtime)-9]	= '\0';
    usr->pNote->date		= str_dup(Strtime);
    usr->pNote->stamp		= current_time;
    usr->pNote->vnum		= ++usr->pBoard->last_vnum;
    usr->total_note++;
    LINK(usr->pNote, usr->pBoard->first_note, usr->pBoard->last_note);
    save_notes(usr->pBoard);
    save_boards( ); /* Save last note vnum */
    check_fnotify(usr, usr->pBoard);
    usr->pNote			= NULL;

    if (total_notes(usr, usr->pBoard) > usr->pBoard->capacity)
    {
	msg_150 = total_notes(usr, usr->pBoard) - usr->pBoard->capacity;

	for (msg = 1; msg <= msg_150; msg++)
	{
	    sprintf(buf, "%d", msg);
	    do_remove_org(usr, buf, FALSE);
	}
    }
}

/*
 * End of board.c
 */

