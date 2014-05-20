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

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

typedef struct dirent Dirent;

#include "bbs.h"

#if !defined (OLD_RAND)
/* long	random		( ); BSD */
void	srandom		(unsigned int);
int	getpid		( );
time_t	time		(time_t *tloc);
#endif

char *		string_hash	[MAX_KEY_HASH];
char *		string_space;
char *		top_string;
char		str_empty	[1];
char		log_buf		[2 * INPUT_LENGTH];
USER_DATA *	user_list;
char *		greeting1;
char *		greeting2;
/* char *		greeting3; unused variable BAXTER */
char *		greeting4;
char *		greeting5;
char *		greeting6;

/*
 * Memory management.
 * Increase MAX_STRING if you have too.
 * Tune the others only if you understand what you're doing.
 */
#define		MAX_STRING	1413120
#define		MAX_PERM_BLOCK	131072
#define		MAX_MEM_LIST	11

void *		rgFreeList	[MAX_MEM_LIST];
const	int	rgSizeList	[MAX_MEM_LIST]  =
{
    16, 32, 64, 128, 256, 1024, 2048, 4096, 8192, 16384, 32768-64
};

int		nAllocString;
int		sAllocString;
int		nAllocPerm;
int		sAllocPerm;
bool		fBootDbase;

/*
 * Local booting procedures.
 */
void	init_mm		( void );
void	load_helps	( void );
void	load_config	( void );
void	memory_other	( USER_DATA *usr );
bool	write_to_desc	( int desc, char *txt, int length );

/*
 * Big mama top level function.
 */
void boot_dbase( void )
{
    {
	if ((string_space = calloc(1, MAX_STRING)) == NULL)
	{
	    bug("Boot_dbase: can't alloc %d string space.", MAX_STRING);
	    exit(1);
	}

	top_string = string_space;
	fBootDbase = TRUE;
    }

    first_banish_user	= NULL;
    first_banish_site	= NULL;
    last_banish_user	= NULL;
    last_banish_site	= NULL;

    /*
     * Init random number generator.
     */
    {
	init_mm();
    }

    /*
     * Loop up the config, helps, and other files.
     */
    {
	load_config( );
	load_helps( );
	fBootDbase = FALSE;
	log_string("Load_boards: Loading board and note data");
	load_boards( );
	log_string("Load_banishes: Loading banish data");
	load_banishes( );
	log_string("Load_validates: Loading validate data");
	load_validates( );
    }
	    
    return;
}

HELP_DATA *	first_help;
HELP_DATA *	last_help;

/*
 * Read help.
 */
HELP_DATA *read_help( FILE *fpHelp )
{
    HELP_DATA *pHelp;
    char *word = '\0';
    bool fMatch = FALSE;
    char letter;

    do
    {
	letter = getc(fpHelp);
	if (feof(fpHelp))
	{
	    fclose(fpHelp);
	    fpReserve = fopen(NULL_FILE, "r");
	    return NULL;
	}
    }
    while (isspace(letter));
    ungetc(letter, fpHelp);

    pHelp = (HELP_DATA *) alloc_mem(sizeof(HELP_DATA));
    pHelp->keyword = NULL;
    pHelp->text	   = NULL;

    for (;;)
    {
	word	= feof(fpHelp) ? "End" : fread_word(fpHelp);
	fMatch	= FALSE;

	switch (UPPER(word[0]))
	{
	    case '*': case '#':
		fread_to_eol(fpHelp);
		fMatch = TRUE;
		break;

	    case 'E':
		if (!str_cmp(word, "End"))
		{
		    pHelp->next = NULL;
		    pHelp->prev = NULL;
		    return pHelp;
		}
		break;

	    case 'K':
		KEYS( "Keyword",	pHelp->keyword,	fread_string(fpHelp));
		break;

	    case 'L':
		KEY( "Level",		pHelp->level,	fread_number(fpHelp));
		break;

	    case 'T':
		KEYS( "Text",		pHelp->text,	fread_string(fpHelp));
		break;
	}

	if (!fMatch)
	{
	    bbs_bug("Read_help: No match '%s'", word);
	    exit(1);
	    return NULL;
	}
    }

    return pHelp;
}

/*
 * Load helps.
 */
void load_helps( void )
{
    HELP_DATA *pHelp;
    FILE *fpHelp;

    first_help = NULL;
    last_help  = NULL;

    log_string("Load_helps: Loading help file");

    fclose(fpReserve);
    if (!(fpHelp = fopen(HELP_FILE, "r")))
    {
	bbs_bug("Load_helps: Could not open to read %s", HELP_FILE);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    while ((pHelp = read_help(fpHelp)) != NULL)
    {
	if (!str_cmp(pHelp->keyword, "greeting1"))
	    greeting1 = pHelp->text;

	if (!str_cmp(pHelp->keyword, "greeting2"))
	    greeting2 = pHelp->text;

	if (!str_cmp(pHelp->keyword, "greeting4"))
	    greeting4 = pHelp->text;

	if (!str_cmp(pHelp->keyword, "greeting5"))
	    greeting5 = pHelp->text;

	if (!str_cmp(pHelp->keyword, "greeting6"))
	    greeting6 = pHelp->text;

	LINK(pHelp, first_help, last_help);
    }

    log_string("Load_helps: Done");
    return;
}

/*
 * Read a letter from a file.
 */
char fread_letter( FILE *fp )
{
    char c;

    do
    {
	c = getc(fp);
    }
    while (isspace(c));

    return c;
}

/*
 * Read a number from a file.
 */
int fread_number( FILE *fp )
{
    int number = 0;
    bool sign = FALSE;
    char c;

    do
    {
	c = getc(fp);
    }
    while (isspace(c));

    if (c == '+')
    {
	c = getc(fp);
    }
    else if (c == '-')
    {
	sign = TRUE;
	c = getc(fp);
    }

    if (!isdigit(c))
    {
	bug("Fread_number: bad format.", 0);
	exit(1);
    }

    while (isdigit(c))
    {
	number	= number * 10 + c - '0';
	c	= getc(fp);
    }

    if (sign)
	number = 0 - number;

    if (c == '|')
	number += fread_number(fp);
    else if (c != ' ')
	ungetc(c, fp);

    return number;
}

/*
 * Read and allocate space for a string from a file.
 * These strings are read-only and shared.
 * Strings are hashed:
 *   each string prepended with hash pointer to prev string,
 *   hash code is simply the string length.
 *   this function takes 40% to 50% of boot-up time.
 */
char *fread_string( FILE *fp )
{
    char *plast;
    char c;

    plast = top_string + sizeof(char *);
    if (plast > &string_space[MAX_STRING - STRING_LENGTH] )
    {
	bug("Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
	exit(1);
    }

    do
    {
	c = getc(fp);
    }
    while (isspace(c));

    if ((*plast++ = c) == '~')
	return &str_empty[0];

    for (;;)
    {
	switch (*plast = getc(fp))
        {
	    default:
		plast++;
		break;

	    case EOF:
		bug("Fread_string: EOF", 0);
		return NULL;
		break;

	    case '\n':
		plast++;
		*plast++ = '\r';
		break;

	    case '\r':
		break;

	    case '~':
		plast++;
		{
		    union
		    {
			char *	pc;
			char	rgc[sizeof(char *)];
		    } u1;
		    int ic, iHash;
		    char *pHash, *pHashPrev, *pString;

		    plast[-1] = '\0';
		    iHash = UMIN(MAX_KEY_HASH - 1, plast - 1 - top_string);
		    for (pHash = string_hash[iHash]; pHash; pHash = pHashPrev)
		    {
			for (ic = 0; ic < sizeof(char *); ic++)
			    u1.rgc[ic] = pHash[ic];
			pHashPrev	= u1.pc;
			pHash		+= sizeof(char *);
			if (top_string[sizeof(char *)] == pHash[0]
			&& !strcmp(top_string + sizeof(char *) + 1, pHash+1))
			    return pHash;
		    }

		    if (fBootDbase)
		    {
			pString		= top_string;
			top_string	= plast;
			u1.pc		= string_hash[iHash];
			for (ic = 0; ic < sizeof(char *); ic++)
			    pString[ic] = u1.rgc[ic];
			string_hash[iHash] = pString;

			nAllocString += 1;
			sAllocString += top_string - pString;
			return pString + sizeof(char *);
		    }
		    else
		    {
			return str_dup(top_string + sizeof(char *));
		    }
		}
	}
    }
}

/*
 * Read to end of line (for comments).
 */
void fread_to_eol( FILE *fp )
{
    char c;

    do
    {
	c = getc(fp);
    }
    while (c != '\n' && c != '\r');

    do
    {
	c = getc(fp);
    }
    while (c == '\n' || c == '\r');

    ungetc(c, fp);
    return;
}

/*
 * Read one word (into static buffer).
 */
char *fread_word( FILE *fp )
{
    static char word[INPUT];
    char *pword;
    char cEnd;

    do
    {
	cEnd = getc(fp);
    }
    while (isspace(cEnd));

    if (cEnd == '\'' || cEnd == '"')
    {
	 pword = word;
    }
    else
    {
	word[0]	= cEnd;
	pword	= word+1;
	cEnd	= ' ';
    }

    for (; pword < word + INPUT_LENGTH; pword++)
    {
	*pword = getc(fp);
	if (cEnd == ' ' ? isspace(*pword) : *pword == cEnd)
	{
	    if (cEnd == ' ')
		ungetc(*pword, fp);
	    *pword = '\0';
	    return word;
	}
    }

    bug("Fread_word: word too long.", 0);
    exit(1);
    return NULL;
}

long fread_flag( FILE *fp )
{
    int number = 0;
    char c;
    bool negative = FALSE;

    do
    {
	c = getc(fp);
    }
    while (isspace(c));

    if (c == '-')
    {
	negative = TRUE;
	c = getc(fp);
    }

    if (!isdigit(c))
    {
	while (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'))
	{
	    number += flag_convert(c);
	    c = getc(fp);
	}
    }

    while (isdigit(c))
    {
	number = number * 10 + c - '0';
	c = getc(fp);
    }

    if (c == '|')
	number += fread_flag(fp);
    else if (c != ' ')
	ungetc(c, fp);

    if (negative)
	return -1 * number;

    return number;
}

long flag_convert( char letter )
{
    long bitsum = 0;
    char i;

    if ('A' <= letter && letter <= 'Z')
    {
	bitsum = 1;
	for (i = letter; i > 'A'; i--)
	    bitsum *= 2;
    }
    else if ('a' <= letter && letter <= 'z')
    {
	bitsum = 67108864;
	for (i = letter; i > 'a'; i--)
	    bitsum *= 2;
    }

    return bitsum;
}

char * print_flags( int flag )
{
    int count, pos = 0;
    static char buf[52];

    for (count = 0; count < 32; count++)
    {
	if (IS_SET(flag, 1 << count))
	{
	    if (count < 26)
		buf[pos] = 'A' + count;
	    else
		buf[pos] = 'a' + (count - 26);

	    pos++;
	}
    }

    if (pos == 0)
    {
	buf[pos] = '0';
	pos++;
    }

    buf[pos] = '\0';
    return buf;
}

/*
 * Allocate some ordinary memory,
 *   with the expectation of freeing it someday.
 */
void *alloc_mem( int sMem )
{
    void *pMem;
    int *magic;
    int iList;

    sMem += sizeof(*magic);

    for (iList = 0; iList < MAX_MEM_LIST; iList++)
    {
	if (sMem <= rgSizeList[iList])
	    break;
    }

    if (iList == MAX_MEM_LIST)
    {
	bug("Alloc_mem: size %d too large.", sMem);
	exit(1);
    }

    if (rgFreeList[iList] == NULL)
    {
	pMem = alloc_perm(rgSizeList[iList]);
    }
    else
    {
	pMem = rgFreeList[iList];
	rgFreeList[iList] = * ((void **) rgFreeList[iList]);
    }

    magic = (int *) pMem;
    *magic = MAGIC_NUM;
    pMem += sizeof(*magic);
    return pMem;
}

/*
 * Free some memory.
 * Recycle it back onto the free list for blocks of that size.
 */
void free_mem( void *pMem, int sMem )
{
    int iList;
    int *magic;

    pMem -= sizeof(*magic);
    magic = (int *) pMem;

    if (*magic != MAGIC_NUM)
    {
	bug("Attempt to recyle invalid memory of size %d.", sMem);
	bug((char*) pMem + sizeof(*magic), 0);
	abort();
	return;
    }

    *magic = 0;
    sMem += sizeof(*magic);

    for (iList = 0; iList < MAX_MEM_LIST; iList++)
    {
	if (sMem <= rgSizeList[iList])
	    break;
    }

    if (iList == MAX_MEM_LIST)
    {
	bug("Free_mem: size %d too large.", sMem);
	exit(1);
    }

    * ((void **) pMem)	= rgFreeList[iList];
    rgFreeList[iList]	= pMem;
    return;
}

/*
 * Allocate some permanent memory.
 * Permanent memory is never freed,
 *   pointers into it may be copied safely.
 */
void *alloc_perm( int sMem )
{
    static char *pMemPerm;
    static int iMemPerm;
    void *pMem;

    while (sMem % sizeof(long) != 0)
	sMem++;

    if (sMem > MAX_PERM_BLOCK)
    {
	bug("Alloc_perm: %d too large.", sMem);
	exit(1);
    }

    if (pMemPerm == NULL || iMemPerm + sMem > MAX_PERM_BLOCK)
    {
	iMemPerm = 0;
	if ((pMemPerm = calloc(1, MAX_PERM_BLOCK)) == NULL)
	{
	    perror("Alloc_perm");
	    exit(1);
	}
    }

    pMem	 = pMemPerm + iMemPerm;
    iMemPerm	+= sMem;
    nAllocPerm	+= 1;
    sAllocPerm	+= sMem;
    return pMem;
}

/*
 * Duplicate a string into dynamic memory.
 * Fread_strings are read-only and shared.
 */
char *str_dup( const char *str )
{
    char *str_new;

    if (str[0] == '\0')
	return &str_empty[0];

    if (str >= string_space && str < top_string)
	return (char *) str;

    str_new = alloc_mem(strlen(str) + 1);
    strcpy(str_new, str);
    return str_new;
}

/*
 * Free a string.
 * Null is legal here to simplify callers.
 * Read-only shared strings are not touched.
 */
void free_string( char *pstr )
{
    if (pstr == NULL || *pstr == '\0')
	return;

    if (pstr == &str_empty[0])
	return;

    if (pstr >= string_space && pstr < top_string)
	return;

    free_mem(pstr, strlen(pstr) + 1);
    return;
}

/*
 * Removes the tildes from a string.
 * Used for user-entered strings that go into disk files.
 */
void smash_tilde( char *str )
{
    for (; *str != '\0'; str++)
    {
	if (*str == '~')
	    *str = '-';
    }

    return;
}

void strip_spaces( char *str )
{
    for (; *str != '\0'; str++)
    {
	if (*str == ' ')
	    *str = '\0';
    }

    return;
}

/*
 * Compare strings, case insensitive.
 * Return TRUE if different
 *   (compatibility with historical functions).
 */
bool str_cmp( const char *astr, const char *bstr )
{
    if (astr == NULL)
    {
	bbs_bug("Str_cmp: Null astr, bstr = %s", bstr ? bstr : "(null)");
	return TRUE;
    }

    if (bstr == NULL)
    {
	bbs_bug("Str_cmp: Null bstr, astr = %s", astr ? astr : "(null)");
	return TRUE;
    }

    for (; *astr || *bstr; astr++, bstr++)
    {
	if (LOWER(*astr) != LOWER(*bstr))
	    return TRUE;
    }

    return FALSE;
}

/*
 * Compare strings, case insensitive, for prefix matching.
 * Return TRUE if astr not a prefix of bstr
 *   (compatibility with historical functions).
 */
bool str_prefix( const char *astr, const char *bstr )
{
    if (astr == NULL)
    {
	bbs_bug("Str_prefix: Null astr, bstr = %-20.20s%s",
	    bstr ? bstr : "(null)", bstr && strlen(bstr) > 20 ? ".." : "");
	return TRUE;
    }

    if (bstr == NULL)
    {
	bbs_bug("Str_prefix: Null bstr, astr = %-20.20s%s",
	    astr ? astr : "(null)", astr && strlen(astr) > 20 ? ".." : "");
	return TRUE;
    }

    for (; *astr; astr++, bstr++)
    {
	if (LOWER(*astr) != LOWER(*bstr))
	    return TRUE;
    }

    return FALSE;
}

bool str_prefix_two( const char *astr, const char *bstr )
{
    if (astr == NULL)
    {
	bbs_bug("Str_prefix_two: Null astr, bstr = %-20.20s%s",
	    bstr ? bstr : "(null)", bstr && strlen(bstr) > 20 ? ".." : "");
	return TRUE;
    }

    if (bstr == NULL)
    {
	bbs_bug("Str_prefix_two: Null bstr, astr = %-20.20s%s",
	    astr ? astr : "(null)", astr && strlen(astr) > 20 ? ".." : "");
	return TRUE;
    }

    for (; *astr; astr++, bstr++)
    {
	if (*astr != *bstr)
	    return TRUE;
    }

    return FALSE;
}

/*
 * Compare strings, case insensitive, for suffix matching.
 * Return TRUE if astr not a suffix of bstr
 * (compatibility with historical functions).
 */
bool str_suffix( const char *astr, const char *bstr )
{
    int len_astr, len_bstr;

    if (astr == NULL)
    {
	bbs_bug("Str_prefix: Null astr, bstr = %-20.20s%s",
	    bstr ? bstr : "(null)", bstr && strlen(bstr) > 20 ? ".." : "");
	return TRUE;
    }

    if (bstr == NULL)
    {
	bbs_bug("Str_prefix: Null bstr, astr = %-20.20s%s",
	    astr ? astr : "(null)", astr && strlen(astr) > 20 ? ".." : "");
	return TRUE;
    }

    len_astr = strlen(astr);
    len_bstr = strlen(bstr);

    if (len_astr <= len_bstr && !str_cmp(astr, bstr + len_bstr - len_astr))
	return FALSE;
    else
	return TRUE;
}

/*
 * Returns an initial-capped string.
 */
char *capitalize( const char *str )
{
    static char strcap[STRING];
    int i;

    strcap[0] = '\0';

    for (i = 0; str[i] != '\0'; i++)
	strcap[i] = LOWER(str[i]);

    strcap[i] = '\0';
    strcap[0] = UPPER(strcap[0]);
    return strcap;
}

/*
 * Return true if an argument is completely numeric.
 */
bool is_number( char *arg )
{
    if (*arg == '\0')
	return FALSE;

    if (*arg == '+' || *arg == '-')
	arg++;

    for (; *arg != '\0'; arg++)
    {
	if (!isdigit(*arg))
	    return FALSE;
    }

    return TRUE;
}

/*
 * Pick off one argument from a string and return the rest.
 * Understands quotes.
 */
char *one_argument( char *argument, char *arg_first )
{
    char cEnd;

    while (isspace(*argument))
	argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"')
	cEnd = *argument++;

    while (*argument != '\0')
    {
	if (*argument == cEnd)
	{
	    argument++;
	    break;
	}
	*arg_first = LOWER(*argument);
	arg_first++;
	argument++;
    }
    *arg_first = '\0';

    while (isspace(*argument))
	argument++;

    return argument;
}

char *one_argument_two( char *argument, char *arg_first )
{
    char cEnd;

    while (isspace(*argument))
	argument++;

    cEnd = ' ';
    if (*argument == '\'' || *argument == '"')
	cEnd = *argument++;

    while (*argument != '\0')
    {
	if (*argument == cEnd)
	{
	    argument++;
	    break;
	}
	*arg_first = *argument;
	arg_first++;
	argument++;
    }
    *arg_first = '\0';

    while (isspace(*argument))
	argument++;

    return argument;
}

/*
 * Given a string like 14.foo, return 14 and 'foo'
 */
int number_argument( char *argument, char *arg )
{
    char *pdot;
    int number;

    for (pdot = argument; *pdot != '\0'; pdot++)
    {
	if (*pdot == '.')
	{
	    *pdot = '\0';
	    number = atoi(argument);
	    *pdot = '.';
	    strcpy(arg, pdot+1);
	    return number;
	}
    }

    strcpy(arg, argument);
    return 1;
}

/* buffer sizes */
const	int	buf_size	[MAX_BUF_LIST] =
{
    16,32,64,128,256,1024,2048,4096,8192,16384
};

/* local procedure for finding the next acceptable size */
/* -1 indicates out-of-boundary error */
int get_size( int val )
{
    int i;

    for (i = 0; i < MAX_BUF_LIST; i++)
	if (buf_size[i] >= val)
	{
	    return buf_size[i];
	}

    return -1;
}

BUFFER *	buf_free;

BUFFER * new_buf( )
{
    BUFFER *buffer;

    if (buf_free == NULL)
	buffer = (BUFFER *) alloc_perm(sizeof(*buffer));
    else
    {
	buffer = buf_free;
	buf_free = buf_free->next;
    }

    buffer->next	= NULL;
    buffer->state	= BUFFER_SAFE;
    buffer->size	= get_size(BASE_BUF);
    buffer->string	= alloc_mem(buffer->size);
    buffer->string[0]	= '\0';
    VALIDATE(buffer);
    return buffer;
}

BUFFER * new_buf_size( int size )
{
    BUFFER *buffer;

    if (buf_free == NULL)
	 buffer = (BUFFER *) alloc_perm(sizeof(*buffer));
    else
    {
	buffer = buf_free;
	buf_free = buf_free->next;
    }

    buffer->next	= NULL;
    buffer->state	= BUFFER_SAFE;
    buffer->size	= get_size(size);
    if (buffer->size == -1)
    {
	bug("New_buf_size: buffer size %d too large.", size);
	exit(1);
    }

    buffer->string	= alloc_mem(buffer->size);
    buffer->string[0]	= '\0';
    VALIDATE(buffer);
    return buffer;
}

void free_buf( BUFFER *buffer )
{
    if (!IS_VALID(buffer))
	return;

    free_mem(buffer->string, buffer->size);
    buffer->string	= NULL;
    buffer->size	= 0;
    buffer->state	= BUFFER_FREED;
    INVALIDATE(buffer);
    buffer->next	= buf_free;
    buf_free		= buffer;
}

void clear_buf( BUFFER *buffer )
{
    buffer->string[0]	= '\0';
    buffer->state	= BUFFER_SAFE;
}


char *buf_string( BUFFER *buffer )
{
    return buffer->string;
}

bool add_buf( BUFFER *buffer, char *string )
{
    int len;
    char *oldstr;
    int oldsize;

    oldstr = buffer->string;
    oldsize = buffer->size;

    if (buffer->state == BUFFER_OVERFLOW)
	return FALSE;

    len = strlen(buffer->string) + strlen(string) + 1;

    while (len >= buffer->size)
    {
	buffer->size = get_size(buffer->size + 1);
	{
	    if (buffer->size == -1)
	    {
		buffer->size = oldsize;
		buffer->state = BUFFER_OVERFLOW;
		bug("Add_buf: buffer overflow past size %d", buffer->size);
		return FALSE;
	    }
	}
    }

    if (buffer->size != oldsize)
    {
	buffer->string = alloc_mem(buffer->size);
	strcpy(buffer->string, oldstr);
	free_mem(oldstr, oldsize);
    }

    strcat(buffer->string, string);
    return TRUE;
}

void do_memory( USER_DATA *usr, char *argument )
{
    HELP_DATA *fHelp;
    char buf[STRING];
    int count1, count2;

    memory_other(usr);
    do_statforum(usr, "");
    memory_validates(usr);
    memory_banishes(usr);

    count1 = count2 = 0;

    for (fHelp = first_help; fHelp; fHelp = fHelp->next)
        count1++;

    sprintf(buf, "Help : %4d -- %d bytes\n\r", count1,
        count1 * (sizeof(*fHelp)));
    send_to_user(buf, usr);

    sprintf(buf, "Strings %5d strings of %7d bytes (max %d).\n\r",
	nAllocString, sAllocString, MAX_STRING);
    send_to_user(buf, usr);

    sprintf(buf, "Perms   %5d blocks  of %7d bytes.\n\r",
	nAllocPerm, sAllocPerm);
    send_to_user(buf, usr);

    sprintf(buf, "Total %d users found.\n\r", count_files(USER_DIR));
    send_to_user(buf, usr);

    return;
}

/*
 * Report a bug.
 */
void bug( const char *str, int param )
{
    char buf[STRING];

    strcpy(buf, "[***] BUG: ");
    sprintf(buf + strlen(buf), str, param);
    log_string(buf);
    return;
}

/*
 * Report a bug.
 */
void bbs_bug( const char *str, ... )
{
    char buf[STRING];

    strcpy(buf, "[***] BUG: ");
    {
	va_list param;

	va_start(param, str);
	vsprintf(buf + strlen(buf), str, param);
	va_end(param);
    }

    log_string(buf);
}

/*
 * Writes a string to the log.
 */
void log_string( const char *str )
{
    char *strtime;

    strtime = ctime(&current_time);
    strtime[strlen(strtime)-1] = '\0';
    fprintf(stderr, "%s :: %s\n", strtime, str );
    return;
}

void log_file( char *file, const char *str )
{
    char buf[INPUT];
    FILE *fp;
    char *strtime;

    sprintf(buf, "log/%s.log", file);
    if (!(fp = fopen(buf, "a")))
    {
	bug("Log_file: Cannot open to append log file", 0);
	return;
    }

    strtime = ctime(&current_time);
    strtime[strlen(strtime)-1] = '\0';
    fprintf(fp, "%s :: %s\n", strtime, str);
    fclose(fp);
    return;
}

int strlen_color( char *txt )
{
    char *str;
    int length = 0;

    if (txt == NULL || txt[0] == '\0')
	return 0;

    str = txt;
    while (*str != '\0')
    {
	if (*str != '#')
	{
	    str++;
	    length++;
	    continue;
	}
	if (*(++str) == '#')
	    length++;

	str++;
    }
    return length;
}

#if defined (OLD_RAND)
static	int	rgiState[2+55];
#endif

void init_mm( void )
{
#if defined (OLD_RAND)
    int *piState;
    int iState;
         
    piState     = &rgiState[2];
        
    piState[-2] = 55 - 55;
    piState[-1] = 55 - 24;
   
    piState[0]  = ((int) current_time) & ((1 << 30) - 1);
    piState[1]  = 1;
    for (iState = 2; iState < 55; iState++)
    {
	piState[iState] = (piState[iState-1] + piState[iState-2])
			  & ((1 << 30) - 1);
    }
#else
    srandom(time(NULL)^getpid());
#endif
    return;
}

long number_mm( void )
{  
#if defined (OLD_RAND)
    int *piState;   
    int iState1;
    int iState2;
    int iRand;
                        
    piState = &rgiState[2];
    iState1 = piState[-2];
    iState2 = piState[-1];
    iRand   = (piState[iState1] + piState[iState2])
	       & ((1 << 30) - 1);
    piState[iState1] = iRand;
    if (++iState1 == 55)
	iState1 = 0;
    if (++iState2 == 55)
	iState2 = 0;  

    piState[-2] = iState1;
    piState[-1] = iState2;
    return iRand >> 6;
#else
    return random() >> 6;
#endif
}

/*  
 * Generate a random number.
 */
int number_range( int from, int to )
{
    int power;
    int number;
    
    if (from == 0 && to == 0)
	return 0;

    if ((to = to - from + 1) <= 1)
	return from;
 
    for (power = 2; power < to; power <<= 1)
	;
        
    while ((number = number_mm() & (power -1)) >= to)
	;
   
    return from + number;
}

int count_files( char *path )
{
    DIR *dirp;
    Dirent *de;
    int count;

    if ((dirp = opendir(path)) == (DIR *) NULL)
    {
	return -1;
    }

    for (count = 0, de = readdir(dirp); de != (Dirent *) NULL;
	de = readdir(dirp))
    {
	if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
	    continue;

	count++;
    }

    closedir(dirp);
    return count;
}

#define COPYOVER_FILE	"copyover.data"
#define EXE_FILE	"../src/bbs"

void do_reboo( USER_DATA *usr, char *argument )
{
    send_to_user("If you want to REBOOT, spell it out.\n\r", usr);
    return;
}

void do_reboot( USER_DATA *usr, char *argument )
{
    DESC_DATA *d, *d_next;
    FILE *fp;
    char p_buf[100];
    char c_buf[100];
    extern int port;
    extern int control;

    if (!(fp = fopen(COPYOVER_FILE, "w")))
    {
	send_to_user("ERROR: Could not open to save copyover.data\n\r", usr);
	bbs_bug("Do_copyover: Could not open to save %s", COPYOVER_FILE);
	return;
    }

    system_info("Rebooting, please reamin seated");
    for (d = desc_list; d; d = d_next)
    {
	USER_DATA *usr	= d->user;
	d_next		= d->next;

	if (!USR(d) || d->login > CON_LOGIN)
	{
	    write_to_desc(d->descr, "\n\r\007Sorry, we are rebooting. "
				    "Come back in a few minutes.\n\r", 0);
	    close_socket(d);
	}
	else if (EDIT_MODE(USR(d)) != EDITOR_NONE)
	{
	    write_to_desc(d->descr, "\n\r\007Sorry, we are rebooting. "
				    "Come back in a few minutes.\n\r", 0);
	    close_socket(d);
	}
	else
	{
	    fprintf(fp, "%d %s %s %s %s\n", d->descr,
		usr->pBoard->short_name, usr->name, d->ident, d->host);
	    save_user(usr);
	}
    }

    fprintf(fp, "-1\n");
    fclose(fp);

    fclose(fpReserve);

    sprintf(p_buf, "%d", port);
    sprintf(c_buf, "%d", control);
    execl(EXE_FILE, "bbs", p_buf, "copyover", c_buf, (char *) NULL);
    perror("Do_copyover: execl");
    send_to_user("ERROR: Copyover failed!\n\r", usr);
    fpReserve = fopen(NULL_FILE, "r");
}

void copyover_recover( void )
{
    DESC_DATA *d;
    FILE *fp;
    char name[100];
    char host[STRING];
    char board[100];
    char user[100]; 
    int desc;
    bool fOld;

    log_string("Copyover: Copyover recovery initiated.");
    if (!(fp = fopen(COPYOVER_FILE, "r")))
    {
	bbs_bug("Copyover_recover: Could not open to read %s", COPYOVER_FILE);
	exit(1);
    }

    unlink(COPYOVER_FILE);

    for (;;)
    {
	fscanf(fp, "%d %s %s %s %s\n", &desc, board, name, user, host);
	if (desc == -1)
	    break;

	if (!write_to_desc(desc, "", 0))
	{
	    close(desc);
	    continue;
	}

	d		= new_desc( );
	d->descr	= desc;
	if (d->host)
	    free_string(d->host);
	d->host		= str_dup(host);
	if (d->ident)
	    free_string(d->ident);
	d->ident	= str_dup(user);
	d->next		= desc_list;
	desc_list	= d;
	d->login	= CON_REBOOT_RECOVER;

	fOld		= load_user(d, name);

	if (!fOld)
	{
	    write_to_desc(desc,
		"\n\rSomehow, your user was lost in the reboot, sorry.\n\r", 0);
	    close_socket(d);
	}
	else
	{
	    if (!d->user->pBoard)
		d->user->pBoard = board_lookup("lobby", FALSE);

	    d->user->next	= user_list;
	    user_list		= d->user;
	    d->login		= CON_LOGIN;
	    USR(d)->logon	= current_time;
	    USR(d)->pBoard	= board_lookup(board, FALSE);
	    save_user(USR(d));
	}
    }

    system_info("Reboot recover complete");
    log_string("Copyover: Done.");
    fclose(fp);
    return;
}

/*
 * Prints a string to a file in the format fget_string() reads.
 * from Sapphire Mud 0.3
 */
void fput_string( FILE *pFile, char *pFormat, ... )
{
    va_list pArgs;
    char cBuf[STRING];
    int i;

    va_start(pArgs, pFormat);
    vsnprintf(cBuf, STRING, pFormat, pArgs);
    putc('\"', pFile);

    for (i = 0; cBuf[i] != '\0'; i++)
    {
	switch (cBuf[i])
	{
	    case '\\': putc('\\', pFile); putc('\\', pFile);	break;
	    case '"' : putc('\\', pFile); putc('"', pFile);	break;
	    case '\n':
		putc('\\', pFile);
		putc('n', pFile);

		if (cBuf[i + 1] != '\r')
		    putc('\n', pFile);

		break;

	    case '\r':
		putc('\\', pFile);
		putc('r', pFile);

		if (i > 0 && cBuf[i - 1] == '\n')
		    putc('\n', pFile);

		break;

	    case '\t': putc('\\', pFile); putc('t', pFile);	break;
	    case '\a': putc('\\', pFile); putc('a', pFile);	break;
	    default  : putc(cBuf[i], pFile);			break;
	}
    }

    putc('\"', pFile);
    va_end(pArgs);
}

/*
 * Gets one letter from a file.
 * from Sapphire Mud 0.3
 */
char fget_letter( FILE *pFile )
{
    char c;

    do
    {
	if (feof(pFile) != 0)
	{
	    bbs_bug("Fget_letter: Unexpected EOF");
	    return ('\0');
	}

	c = getc(pFile);
    }

    while (isspace(c) != 0);

    return (c);
}

/*
 * Gets a number from a file.
 * from Sapphire Mud 0.3
 */
long fget_number( FILE *pFile )
{
    char cBuf[INPUT];
    long lOutput;
    char c;
    int i;
    bool b = FALSE;

    do
    {
	if (feof(pFile) != 0)
	{
	    bbs_bug("Fget_number: Unexpected EOF");
	    return (0);
	}

	c = getc(pFile);
    }

    while (isspace(c) != 0);

    if (c == '+')
	c = getc(pFile);
    else if (c == '-')
    {
	b = TRUE;
	c = getc(pFile);
    }

    for (i = 0; isdigit(c); i++)
    {
	if (i >= (INPUT - 1))
	{
	    bbs_bug("Fget_number: Word too long");
	    return (0);
	}

	if (feof(pFile) != 0)
	{
	    bbs_bug("Fget_number: Unexpected EOF");
	    return (0);
	}

	cBuf[i] = c;
	c	= getc(pFile);
    }

    cBuf[i] = '\0';

    if (cBuf[0] == '\0')
    {
	bbs_bug("Fget_number: Number not found");
	return (0);
    }

    lOutput = atol(cBuf);
    return ((b == TRUE ? -lOutput : lOutput));
}

/*
 * Gets a one word (terminated by a space) from a file.
 * from Sapphire Mud 0.3
 */
char *fget_word( FILE *pFile )
{
    char *pOutput;
    char c = '\0';
    int i;

    pOutput = top_string + sizeof(char *);
    if (pOutput > &string_space[MAX_STRING - STRING])
    {           
        bbs_bug("Fget_word: MAX_STRING %d exceeded.", MAX_STRING);
        exit(1);
    } 

    *pOutput = '\0';

    do
    {
	if (feof(pFile) != 0)
	    break;

	c = getc(pFile);
    }

    while (isspace(c) != 0);

    for (i = 0; isspace(c) == 0; i++)
    {
	if (i >= (INPUT - 1))
	{
	    bbs_bug("Fget_word: Word too long");
	    return &str_empty[0];
	}

	if (feof(pFile) != 0)
	    break;

	pOutput[i] = c;
	c	   = getc(pFile);
    }

    pOutput[i] = '\0';
    return (pOutput);
}

/*
 * Gets a formatted string from a file.
 * from Sapphire Mud 0.3
 */
char *fget_string( FILE *pFile )
{
    char *pOutput;
    char c[2] = { '\0', '\0' };
    int i;

    pOutput = top_string + sizeof(char *);
    if (pOutput > &string_space[MAX_STRING - STRING])
    {           
        bbs_bug("Fget_string: MAX_STRING %d exceeded.", MAX_STRING);
        exit(1);
    } 

    *pOutput = '\0';

    do
    {
	if (feof(pFile) != 0)
	{
	    bbs_bug("Fget_string: Unexpected EOF");
	    return &str_empty[0];
	}

	c[0] = getc(pFile);
    }

    while (isspace(c[0]) != 0);

    if (c[0] != '"')
    {
	bbs_bug("Fget_string: Symbol '\"' not found");
	return &str_empty[0];
    }

    c[0] = getc(pFile);

    if (c[0] == '"')
	return &str_empty[0];

    for (i = 0; ; i++)
    {
	if (i >= (STRING - 1))
	{
	    bbs_bug("Fget_string: String too long");
	    return &str_empty[0];
	}

	if (feof(pFile) != 0)
	{
	    bbs_bug("Fget_string: Unexpected EOF");
	    return &str_empty[0];
	}

	if (c[0] == '\\')
	{
	    switch ((c[0] = getc(pFile)))
	    {
		case '\\': pOutput[i] = '\\';	break;
		case '"' : pOutput[i] = '"';	break;
		case 'n' : pOutput[i] = '\n';	break;
		case 'r' : pOutput[i] = '\r';	break;
		case 't' : pOutput[i] = '\t';	break;
		case 'a' : pOutput[i] = '\a';	break;
		default:
		    bbs_bug("Fget_string: Unknown escape '\\%s'", c);
		    break;
	    }

	    if (feof(pFile) != 0)
	    {
		bbs_bug("Fget_string: Unexpected EOF");
		return &str_empty[0];
	    }
	}
	else if (c[0] != '\n' && c[0] != '\r' && c[0] != '\t')
	    pOutput[i] = c[0];
	else
	    i--;

	c[0] = getc(pFile);

	if (c[0] == '"')
	    break;
    }

    pOutput[i + 1] = '\0';
    return (pOutput);
}

void do_test( USER_DATA *usr, char *argument )
{
    FILE *pFile;

    pFile = fopen("../lib/alo", "w");
    if (pFile)
    {
	fput_string(pFile, "%s", argument);
	fputc(' ', pFile);
	fput_string(pFile, "%s", usr->name);
	fputc('\n', pFile);
	fclose(pFile);
	return;
    }

    send_to_user("Acilamiyor.\n\r", usr);
    return;
}

void do_rtest( USER_DATA *usr, char *argument )
{
    FILE *pFile;
    char *alo, *isim;

    pFile = fopen("../lib/alo", "r");
    if (pFile)
    {
	alo = str_dup(fget_string(pFile));
	isim = str_dup(fget_string(pFile));
	fclose(pFile);
	print_to_user(usr, "%s, %s\n\r", alo, isim);
	return;
    }

    send_to_user("Acilamiyor.\n\r", usr);
    return;
}

/*
 * Load BBS config file.
 */
void load_config( void )
{
    FILE *fpConfig;
    char *word = '\0';
    bool fMatch = FALSE;

    log_string("Load_config: Loading config file");

    fclose(fpReserve);
    if (!(fpConfig = fopen(CONFIG_FILE, "r")))
    {
	bbs_bug("Load_config: Could not open to read %s", CONFIG_FILE);
	fpReserve = fopen(NULL_FILE, "r");
	config.bbs_name	   = "(Noname)";
	config.bbs_email   = "userid@host";
	config.bbs_host	   = "localhost";
	config.bbs_port	   = 3000;
	config.bbs_version = "3.0";
	config.bbs_flags   = 0;
	config.bbs_state   = "Turkiye";
	return;
    }

    for (;;)
    {
	word	= feof(fpConfig) ? "End" : fread_word(fpConfig);
	fMatch	= FALSE;

	switch (UPPER(word[0]))
	{
	    case '*': case '#':
		fread_to_eol(fpConfig);
		fMatch = TRUE;
		break;

	    case 'E':
		SKEY( "Email",	 config.bbs_email,	fget_string(fpConfig));
		if (!str_cmp(word, "End"))
		{
		    fclose(fpConfig);
		    fpReserve = fopen(NULL_FILE, "r");
		    log_string("Load_config: Done");
		    return;
		}
		break;

	    case 'F':
		KEY( "Flags",	 config.bbs_flags,	fread_flag(fpConfig));
		break;

	    case 'H':
		SKEY( "Host",	 config.bbs_host,	fget_string(fpConfig));
		break;

	    case 'N':
		SKEY( "Name",	 config.bbs_name,	fget_string(fpConfig));
		break;

	    case 'P':
		KEY( "Port",	 config.bbs_port,	fread_number(fpConfig));
		break;

	    case 'S':
		SKEY( "State",	 config.bbs_state,	fget_string(fpConfig));
		break;

	    case 'V':
		SKEY( "Version", config.bbs_version,	fget_string(fpConfig));
		break;
	}

	if (!fMatch)
	{
	    bbs_bug("Load_config: No match '%s'", word);
	    exit(1);
	}
    }

    return;
}

/*
 * Save BBS config file.
 */
void save_config( void )
{
    FILE *fpConfig;
    char *strtime;

    fclose(fpReserve);
    if (!(fpConfig = fopen(CONFIG_FILE, "w")))
    {
	bbs_bug("Save_config: Could not open to save %s", CONFIG_FILE);
	fpReserve = fopen(NULL_FILE, "r");
	return;
    }

    strtime = ctime(&current_time);
    strtime[strlen(strtime)-1] = '\0';

    fprintf(fpConfig, "# "				);
    fput_string(fpConfig, "%s", config.bbs_name		);
    fprintf(fpConfig, " BBS config file\n# "		);
    fput_string(fpConfig, "%s", strtime			);
    fprintf(fpConfig, " by Baxter\n#\n"			);
    fprintf(fpConfig, "Name    "			);
    fput_string(fpConfig, "%s", config.bbs_name		);
    fputc('\n', fpConfig				);
    fprintf(fpConfig, "Email   "			);
    fput_string(fpConfig, "%s", config.bbs_email	);
    fputc('\n', fpConfig				);
    fprintf(fpConfig, "Host    "			);
    fput_string(fpConfig, "%s", config.bbs_host		);
    fputc('\n', fpConfig				);
    fprintf(fpConfig, "Port    %d\n", config.bbs_port	);
    fprintf(fpConfig, "Version "			);
    fput_string(fpConfig, "%s", config.bbs_version	);
    fputc('\n', fpConfig				);
    fprintf(fpConfig, "State   "			);
    fput_string(fpConfig, "%s", config.bbs_state	);
    fputc('\n', fpConfig				);
    fprintf(fpConfig, "Flags   %s\n",
	print_flags(config.bbs_flags)			);
    fprintf(fpConfig, "End\n"				);
    fclose(fpConfig);
    fpReserve = fopen(NULL_FILE, "r");
    return;
}

/*
 * Change and show config settings (admin command).
 */
void do_config( USER_DATA *usr, char *argument )
{
    char arg[INPUT];

    argument = one_argument(argument, arg);

    if (arg[0] == '\0')
    {
	print_to_user(usr,
	    "---[BBS Config]-----------------------------------------------\n\r"
	    "Name : %-25s E-Mail : %s\n\r"
	    "Host : %-25s Port   : %d\n\r"
	    "State: %-25s Version: %s\n\r"
	    "---[Toggles]--------------------------------------------------\n\r"
	    "Newbie lock: %s  Admin lock: %s  Noresolve: %s\n\r\n\r",
	    config.bbs_name, config.bbs_email, config.bbs_host,
	    config.bbs_port, config.bbs_state, config.bbs_version,
	    IS_SET(config.bbs_flags, BBS_NEWLOCK)   ? "Yes" : "No",
	    IS_SET(config.bbs_flags, BBS_ADMLOCK)   ? "Yes" : "No",
	    IS_SET(config.bbs_flags, BBS_NORESOLVE) ? "Yes" : "No" );
	return;
    }

    if (!str_cmp(arg, "name"))
    {
	if (argument[0] == '\0')
	{
	    syntax("config name <argument>", usr);
	    return;
	}

	free_string(config.bbs_name);
	config.bbs_name = str_dup(capitalize(argument));
	print_to_user(usr, "Ok, BBS name now: %s\n\r", config.bbs_name);
	save_config( );
	return;
    }
    else if (!str_cmp(arg, "email"))
    {
	if (argument[0] == '\0')
	{
	    syntax("config email <argument>", usr);
	    return;
	}

	free_string(config.bbs_email);
	config.bbs_email = str_dup(argument);
	print_to_user(usr, "Ok, BBS email now: %s\n\r", config.bbs_email);
	save_config( );
	return;
    }
    else if (!str_cmp(arg, "version"))
    {
	if (argument[0] == '\0')
	{
	    syntax("config version <argument>", usr);
	    return;
	}

	free_string(config.bbs_version);
	config.bbs_version = str_dup(argument);
	print_to_user(usr, "Ok, BBS version now: %s\n\r", config.bbs_version);
	save_config( );
	return;
    }
    else if (!str_cmp(arg, "host"))
    {
	if (argument[0] == '\0')
	{
	    syntax("config host <argument>", usr);
	    return;
	}

	free_string(config.bbs_host);
	config.bbs_host = str_dup(argument);
	print_to_user(usr, "Ok, BBS host now: %s\n\r", config.bbs_host);
	save_config( );
	return;
    }
    else if (!str_cmp(arg, "state"))
    {
	if (argument[0] == '\0')
	{
	    syntax("config state <argument>", usr);
	    return;
	}

	free_string(config.bbs_state);
	config.bbs_state = str_dup(capitalize(argument));
	print_to_user(usr, "Ok, BBS state now: %s\n\r", config.bbs_state);
	save_config( );
	return;
    }
    else if (!str_cmp(arg, "port"))
    {
	if (argument[0] == '\0' || !is_number(argument))
	{
	    syntax("config port <argument>", usr);
	    return;
	}

	if (atoi(argument) > 9999 || atoi(argument) <= 1234)
	{
	    send_to_user("That's not a valid BBS port.\n\r", usr);
	    return;
	}

	config.bbs_port = atoi(argument);
	print_to_user(usr, "Ok, BBS email now: %d\n\r", config.bbs_email);
	save_config( );
	return;
    }
    else if (!str_cmp(arg, "newbie"))
    {
	if (!str_cmp(argument, "on"))
	{
	    if (IS_SET(config.bbs_flags, BBS_NEWLOCK))
	    {
		send_to_user("BBS newbie lock is already On.\n\r", usr);
		return;
	    }

	    SET_BIT(config.bbs_flags, BBS_NEWLOCK);
	    send_to_user("Ok, BBS newbie lock is On.\n\r", usr);
	    save_config( );
	    return;
	}
	else if (!str_cmp(argument, "off"))
	{
	    if (!IS_SET(config.bbs_flags, BBS_NEWLOCK))
	    {
		send_to_user("BBS newbie lock is already Off.\n\r", usr);
		return;
	    }

	    REM_BIT(config.bbs_flags, BBS_NEWLOCK);
	    send_to_user("Ok, BBS newbie lock is Off.\n\r", usr);
	    save_config( );
	    return;
	}
	else
	{
	    syntax("config newbie on|off", usr);
	    return;
	}
    }
    else if (!str_cmp(arg, "admin"))
    {
	if (!str_cmp(argument, "on"))
	{
	    if (IS_SET(config.bbs_flags, BBS_ADMLOCK))
	    {
		send_to_user("BBS admin lock is already On.\n\r", usr);
		return;
	    }

	    SET_BIT(config.bbs_flags, BBS_ADMLOCK);
	    send_to_user("Ok, BBS admin lock is On.\n\r", usr);
	    save_config( );
	    return;
	}
	else if (!str_cmp(argument, "off"))
	{
	    if (!IS_SET(config.bbs_flags, BBS_ADMLOCK))
	    {
		send_to_user("BBS admin lock is already Off.\n\r", usr);
		return;
	    }

	    REM_BIT(config.bbs_flags, BBS_ADMLOCK);
	    send_to_user("Ok, BBS admin lock is Off.\n\r", usr);
	    save_config( );
	    return;
	}
	else
	{
	    syntax("config admin on|off", usr);
	    return;
	}
    }
    else if (!str_cmp(arg, "noresolve"))
    {
	if (!str_cmp(argument, "on"))
	{
	    if (IS_SET(config.bbs_flags, BBS_NORESOLVE))
	    {
		send_to_user("BBS noresolve is already On.\n\r", usr);
		return;
	    }

	    SET_BIT(config.bbs_flags, BBS_NORESOLVE);
	    send_to_user("Ok, BBS noresolve is On.\n\r", usr);
	    save_config( );
	    return;
	}
	else if (!str_cmp(argument, "off"))
	{
	    if (!IS_SET(config.bbs_flags, BBS_NORESOLVE))
	    {
		send_to_user("BBS noresolve is already Off.\n\r", usr);
		return;
	    }

	    REM_BIT(config.bbs_flags, BBS_NORESOLVE);
	    send_to_user("Ok, BBS noresolve is Off.\n\r", usr);
	    save_config( );
	    return;
	}
	else
	{
	    syntax("config noresolve on|off", usr);
	    return;
	}
    }
    else
    {
	syntax("config <type> <argument>", usr);
	return;
    }
}

void tail_chain( void )
{
    return;
}

