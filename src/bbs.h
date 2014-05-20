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

#if !defined(FALSE)
#define FALSE	0
#endif

#if !defined(TRUE)
#define TRUE	1
#endif

typedef	short	int		sh_int;
typedef	unsigned char		bool;
typedef	unsigned long int	iptr;
/* Crypt problem fix for BSD/Redhat (glibc) systems */
char *	crypt			( const char *key, const char *salt );

/*
 * Structure types.
 */
typedef	struct	buf_type	BUFFER;
typedef	struct	config_data	CONFIG;
typedef	struct	user_data	USER_DATA;
typedef	struct	desc_data	DESC_DATA;
typedef	struct	help_data	HELP_DATA;
typedef	struct	note_data	NOTE_DATA;
typedef	struct	mail_data	MAIL_DATA;
typedef	struct	board_data	BOARD_DATA;
typedef	struct	kick_data	KICK_DATA;
typedef	struct	banish_data	BANISH_DATA;
typedef	struct	validate_data	VALIDATE_DATA;
typedef	struct	message_data	MESSAGE_DATA;
typedef	void	DO_FUN		( USER_DATA *usr, char *argument );

/*
 * String and memory management parameters.
 */
#define MAX_KEY_HASH		1024

#ifdef STRING
#undef STRING
#endif

#ifdef INPUT
#undef INPUT
#endif

#define STRING			8192
#define INPUT			256

#define STRING_LENGTH		8192
#define INPUT_LENGTH		256
#define PAGELEN			20
#define MAX_BUF			16384
#define MAX_BUF_LIST		10
#define BASE_BUF		1024
#define MAGIC_NUM		52571214
#define MAX_ALIAS		20
#define MAX_FRIEND		50
#define MAX_COMMENT		60

#define KEY( literal, field, value )	\
	if (!str_cmp(word, (literal)))	\
	{				\
	    (field) = (value);		\
	    fMatch  = TRUE;		\
	    break;			\
	}

#define KEYS( literal, field, value )	\
	if (!str_cmp(word, (literal)))	\
	{				\
	    free_string((field));	\
	    (field) = (value);		\
	    fMatch  = TRUE;		\
	    break;			\
	}

#define SKEY( literal, field, value )				\
	if (!str_cmp(word, (literal)))				\
	{							\
	    if ((field) != NULL && (field) != &str_empty[0])	\
		free_string((field));				\
	    (field) = str_dup((value));				\
	    fMatch  = TRUE;					\
	    break;						\
	}

/*
 * Ascii conversions.
 */
#define A			1
#define B			2
#define C			4
#define D			8
#define E			16
#define F			32
#define G			64
#define H			128
#define I			256
#define J			1024
#define K			2048

/*
 * Color defines.
 */
#define COLOR_NORMAL		"[0;0;37m"
#define COLOR_RED		"[0;31m"
#define COLOR_GREEN		"[0;32m"
#define COLOR_YELLOW		"[0;33m"
#define COLOR_BLUE		"[0;34m"
#define COLOR_MAGENTA		"[0;35m"
#define COLOR_CYAN		"[0;36m"
#define COLOR_RED_BOLD		"[1;31m"
#define COLOR_GREEN_BOLD	"[1;32m"
#define COLOR_YELLOW_BOLD	"[1;33m"
#define COLOR_BLUE_BOLD		"[1;34m"
#define COLOR_MAGENTA_BOLD	"[1;35m"
#define COLOR_CYAN_BOLD		"[1;36m"
#define COLOR_WHITE		"[1;37m"
#define COLOR_DARK		"[1;30m"

/*
 * Hide defines.
 */
#define HIDE_REALNAME		(A)
#define HIDE_EMAIL		(B)
#define HIDE_ALTEMAIL		(C)
#define HIDE_HOMEPAGE		(D)
#define HIDE_ICQ		(E)

/*
 * Toggle defines.
 */
#define TOGGLE_ANSI		(A)
#define TOGGLE_XING		(B)
#define TOGGLE_IDLE		(C)
#define TOGGLE_BEEP		(D)
#define TOGGLE_INFO		(E)
#define TOGGLE_WARN		(F)
#define TOGGLE_FEEL		(G)
#define TOGGLE_ADM		(H)
#define TOGGLE_TURK		(I)
#define TOGGLE_INVIS		(J)

#define BBS_NEWLOCK		(A)
#define BBS_ADMLOCK		(B)
#define BBS_NORESOLVE		(C)

/*
 * Levels.
 */
#define USER			1
#define MODERATOR		2
#define ADMIN			3

#define EDIT_MODE(usr)		((usr)->editor_mode)
#define EDIT_LINE(usr)		((usr)->editor_line)
#define USR(d)			((d)->user)
#define IS_ADMIN(usr)		((usr)->level == ADMIN)
#define IS_MALE(usr)		((usr)->gender == 0)

/*
 * Memory macros.
 */
#ifdef LINK
#undef LINK
#endif

#define LINK( link, first, last )				\
	do {							\
	    if (!(first))	(first)		= (link);	\
	    else		(last)->next    = (link);	\
	    (link)->next			= NULL;		\
	    (link)->prev			= (last);	\
	    (last)				= (link);	\
	} while (0)

#ifdef UNLINK
#undef UNLINK
#endif

#define UNLINK( link, first, last )					\
	do {								\
	    if (!(link)->prev)	(first)		   = (link)->next;	\
	    else		(link)->prev->next = (link)->next;	\
	    if (!(link)->next)	(last)		   = (link)->prev;	\
	    else		(link)->next->prev = (link)->prev;	\
	} while (0)

struct config_data
{
    char *		bbs_name;
    char *		bbs_email;
    char *		bbs_version;
    char *		bbs_host;
    int			bbs_port;
    char *		bbs_state;
    long		bbs_flags;
}; 

struct	buf_type
{
    BUFFER *		next;
    bool		valid;
    sh_int		state;
    sh_int		size;
    char *		string;
};

struct	desc_data
{
    DESC_DATA *		next;
    USER_DATA *		user;
    bool		valid;
    char *		host;
    sh_int		descr;
    sh_int		login;
    bool		fcommand;
    char		inbuf		[4 * INPUT];
    char		incomm		[INPUT];
    char *		outbuf;
    int			outsize;
    int			outtop;
    char *		showstr_head;
    char *		showstr_point;
    sh_int		newbie;
    char **		pString;
    int			ifd;
    pid_t		ipid;
    char *		ident;
};

struct	user_data
{
    USER_DATA *		next;
    DESC_DATA *		desc;
    BOARD_DATA *	pBoard;
    NOTE_DATA *		pNote;
    NOTE_DATA *		current_note;
    BUFFER *		pBuffer;
    MESSAGE_DATA *	pMsgFirst;
    MESSAGE_DATA *	pMsgLast;
    MAIL_DATA *		pMailFirst;
    MAIL_DATA *		pMailLast;
    MAIL_DATA *		pCurrentMail;
    bool		valid;
    char *		name;
    char *		password;
    char *		real_name;
    char *		email;
    char *		alt_email;
    char *		title;
    char *		plan;
    char *		old_plan;
    char *		idlemsg;
    char *		home_url;
    char *		icquin;
    char *		friend		[MAX_FRIEND];
    char *		friend_com	[MAX_FRIEND];
    char *		enemy		[MAX_FRIEND];
    char *		enemy_com	[MAX_FRIEND];
    char *		notify		[MAX_FRIEND];
    char *		notify_com	[MAX_FRIEND];
    char *		ignore		[MAX_FRIEND];
    char *		ignore_com	[MAX_FRIEND];
    char *		host_name;
    char *		host_small;
    char *		lastCommand;
    time_t		last_logon;
    time_t		last_logoff;
    time_t		age;
    time_t		logon;
    int			used;
    int			key;
    int			total_login;
    int			total_note;
    long		id;
    sh_int		level;
    sh_int		gender;
    sh_int		timer;
    int			lines;
    sh_int		bad_pwd;
    int			version;
    bool		Validated;
    char *		snooped;
    char *		xing_to;
    char *		reply_to;
    char *		last_xing;
    char *		xing_time;
    char *		msg;
    char *		alias		[MAX_ALIAS];
    char *		alias_sub	[MAX_ALIAS];
    char *		zap;
    char *		fnotify;
    char *		sClip;
    time_t		last_note	[100];
    long		fToggle;
    long		fHide;
    /* Editor stuff */
    sh_int		editor_mode;
    sh_int		editor_line;
};

/*
 * Data structure for boards.
 */
struct	board_data
{
    BOARD_DATA *	next;
    BOARD_DATA *	prev;
    char *		short_name;
    char *		long_name;
    char *		moderator;
    char *		info;
    long		type;
    long		last_vnum;
    int			capacity;
    sh_int		vnum;
    NOTE_DATA *		first_note;
    NOTE_DATA *		last_note;
    KICK_DATA *		first_kick;
    KICK_DATA *		last_kick;
};

struct	kick_data
{
    KICK_DATA *		next;
    KICK_DATA *		prev;
    char *		name;
    time_t		duration;
    char *		reason;
};

struct	note_data
{
    NOTE_DATA *		next;
    NOTE_DATA *		prev;
    char *		sender;
    char *		date;
    char *		subject;
    char *		text;
    time_t		stamp;
    long		vnum;
};

struct	mail_data
{
    MAIL_DATA *		next;
    MAIL_DATA *		prev;
    char *		from;
    char *		to;
    char *		subject;
    char *		message;
    bool		marked;
    time_t		stamp_time;
    time_t		read_time;
};

#define BANISH_USER	0
#define BANISH_SITE	1
#define BANISH_PERM	-1

struct	banish_data
{
    BANISH_DATA *	next;
    BANISH_DATA *	prev;
    sh_int		type;
    char *		name;
    char *		banned_by;
    char *		reason;
    time_t		duration;
    sh_int		date;
};

struct	validate_data
{
    VALIDATE_DATA *	next;
    VALIDATE_DATA *	prev;
    char *		name;
    char *		email;
    char *		host;
    char *		real_name;
    char *		sended_by;
    time_t		sended_time;
    time_t		valid_time;
    int			key;
};

struct	message_data
{
    MESSAGE_DATA *	next;
    MESSAGE_DATA *	prev;
    char *		message;
};

struct	help_data
{
    HELP_DATA *		next;
    HELP_DATA *		prev;
    sh_int		level;
    char *		keyword;
    char *		text;
};

struct	cmd_type
{
    char *		name;
    DO_FUN *		do_fun;
    sh_int		level;
    sh_int		show;
    sh_int		valid;
};

/*
 * Utility macros.
 */
#define IS_VALID(data)		((data) != NULL && (data)->valid)
#define VALIDATE(data)		((data)->valid = TRUE)
#define INVALIDATE(data)	((data)->valid = FALSE)
#define UMIN(a, b)		((a) < (b) ? (a) : (b))
#define UMAX(a, b)		((a) > (b) ? (a) : (b))
#define URANGE(a, b, c)		((b) < (a) ? (a) : ((b) > (c) ? (c) : (b)))
#define LOWER(c)		((c) >= 'A' && (c) <= 'Z' ? (c)+'a'-'A' : (c))
#define UPPER(c)		((c) >= 'a' && (c) <= 'z' ? (c)+'A'-'a' : (c))
#define IS_SET(flag, bit)	((flag) & (bit))
#define IS_HIDE(usr, bit)	((usr)->fHide & (bit))
#define IS_TOGGLE(usr, bit)	((usr)->fToggle & (bit))
#define SET_BIT(flag, bit)	((flag) |= (bit))
#define REM_BIT(flag, bit)	((flag) &= ~(bit))
#define SET_HIDE(usr, hide)	((usr)->fHide |= (hide))
#define REM_HIDE(usr, hide)	((usr)->fHide &= ~(hide))
#define SET_TOGGLE(usr, toggle)	((usr)->fToggle |= (toggle))
#define REM_TOGGLE(usr, toggle)	((usr)->fToggle &= ~(toggle))

/*
 * Global variables.
 */
extern	USER_DATA *		user_list;
extern	DESC_DATA *		desc_list;
extern	HELP_DATA *		first_help;
extern	HELP_DATA *		last_help;
extern	BANISH_DATA *		first_banish_user;
extern	BANISH_DATA *		last_banish_user;
extern	BANISH_DATA *		first_banish_site;
extern	BANISH_DATA *		last_banish_site;
extern	VALIDATE_DATA *		first_validate;
extern	VALIDATE_DATA *		last_validate;
extern	BOARD_DATA *		first_board;
extern	BOARD_DATA *		last_board;
extern	CONFIG			config;
extern	char			log_buf		[];
extern	char			str_empty	[1];
extern	time_t			current_time;
extern	time_t			boot_time_t;
extern	char			boot_time	[STRING_LENGTH];
extern	int			last_note_vnum;
extern	FILE *			fpReserve;
extern	bool			fBootDbase;
extern	char *			greeting1;
extern	char *			greeting2;
/* extern	char *			greeting3; unused variable BAXTER */
extern	char *			greeting4;
extern	char *			greeting5;
extern	char *			greeting6;

extern	const	struct	cmd_type	cmd_table	[];


/* admin.c */
void	syslog			( char *txt, USER_DATA *usr );
void	system_info		( char *txt, ... );
void	send_snoop		( USER_DATA *usr, const char *txt );
void	stop_snoop		( USER_DATA *usr );
void	load_validates		( void );
void	add_validate		( USER_DATA *usr );
void	memory_validates	( USER_DATA *pUser );
void	save_config		( void );

/* banish.c */
void	load_banishes		( void );
bool	check_banishes		( USER_DATA *usr, int type );
void	memory_banishes		( USER_DATA *pUser );

/* board.c */
BOARD_DATA *	board_lookup	( const char *name, bool fNumber );
int	board_number		( const BOARD_DATA *pBoard );
void	free_note		( NOTE_DATA *pNote );
void	free_board		( BOARD_DATA *pBoard );
void	append_note		( FILE *fpNote, NOTE_DATA *pNote );
int	unread_notes		( USER_DATA *usr, BOARD_DATA *pBoard );
int	total_notes		( USER_DATA *usr, BOARD_DATA *pBoard );
void	load_boards		( void );
void	save_boards		( void );
void	fix_moderator_name	( USER_DATA *usr );
bool	is_moderator		( USER_DATA *usr );
void	edit_info_answer	( USER_DATA *usr, char *argument );
void	edit_send_note		( USER_DATA *usr );
void	edit_note_free		( USER_DATA *usr );
void	edit_note_subject	( USER_DATA *usr, char *argument );

/* bbs.c */
void	close_socket		( DESC_DATA *dclose );
void	write_to_buffer		( DESC_DATA *d, const char *txt,
				  int length );
void	send_to_user_bw		( const char *txt, USER_DATA *usr );
void	print_to_user		( USER_DATA *usr, char *format, ... );
void	print_to_user_bw	( USER_DATA *usr, char *format, ... );
void	syntax			( const char *txt, USER_DATA *usr );
void	send_to_user		( const char *txt, USER_DATA *usr );
void	page_to_user_bw		( const char *txt, USER_DATA *usr );
void	msg_to_user		( const char *txt, USER_DATA *usr );
void	page_to_user		( const char *txt, USER_DATA *usr );
void	show_string		( struct desc_data *d, char *input, bool fMx );
int	color			( char type, char *string );
void	get_small_host		( USER_DATA *usr );

/* command.c */
void	process_command		( USER_DATA *usr, char *argument );
char *	get_age			( int second, bool fSec );
void	edit_plan_answer	( USER_DATA *usr, char *argument );
void	edit_plan		( USER_DATA *usr, char *argument );
void	edit_quit_answer	( USER_DATA *usr, char *argument );
void	edit_pwd_old		( USER_DATA *usr, char *argument );
void	edit_pwd_new_one	( USER_DATA *usr, char *argument );
void	edit_pwd_new_two	( USER_DATA *usr, char *argument );

/* command2.c */
void	substitute_alias	( DESC_DATA *d, char *input );
bool	is_friend		( USER_DATA *usr, char *name );
bool	is_enemy		( USER_DATA *usr, char *name );
bool	is_notify		( USER_DATA *usr, char *name );
bool	is_ignore		( USER_DATA *usr, char *name );
void	cmd_chat_look		( USER_DATA *usr );
void	cmd_chat_exit		( USER_DATA *usr );
void	cmd_chat_emote		( USER_DATA *usr, char *argument );
void	process_chat_command	( USER_DATA *usr, char *argument );

/* dbase.c */
void	boot_dbase		( void );
char	fread_letter		( FILE *fp );
int	fread_number		( FILE *fp );
char *	fread_string		( FILE *fp );
void	fread_to_eol		( FILE *fp );
char *	fread_word		( FILE *fp );
long	fread_flag		( FILE *fp );
long	flag_convert		( char letter );
void	fput_string		( FILE *pFile, char *pFormat, ... );
char *	fget_string		( FILE *pFile );
char *	print_flags		( int flag );
void *	alloc_mem		( int sMem );
void *	alloc_perm		( int sMem );
void	free_mem		( void *pMem, int sMem );
char *	str_dup			( const char *str );
void	free_string		( char *pstr );
void	smash_tilde		( char *str );
void	strip_spaces		( char *str );
bool	str_cmp			( const char *astr, const char *bstr );
bool	str_prefix		( const char *astr, const char *bstr );
bool	str_prefix_two		( const char *astr, const char *bstr );
bool	str_suffix		( const char *astr, const char *bstr );
char *	capitalize		( const char *str );
char *	one_argument		( char *argument, char *arg_first );
char *	one_argument_two	( char *argument, char *arg_first );
int	number_argument		( char *argument, char *arg );
BUFFER *new_buf			( void );
BUFFER *new_buf_size		( int size );
void	free_buf		( BUFFER *buffer );
bool	add_buf			( BUFFER *buffer, char *string );
void	clear_buf		( BUFFER *buffer );
char *	buf_string		( BUFFER *buffer );
void	bug			( const char *str, int param );
void	bbs_bug			( const char *str, ... );
void	log_string		( const char *str );
void	log_file		( char *file, const char *str );
bool	is_number		( char *arg );
int	strlen_color		( char *txt );
int	number_range		( int from, int to );
int	count_files		( char *path );
void	copyover_recover	( void );
void	tail_chain		( void );

/* editor.c */
void	string_edit		( USER_DATA *usr, char **pString );
void	string_append		( USER_DATA *usr, char **pString );
void	string_add		( USER_DATA *usr, char *argument );
char *	string_replace		( char *orig, char *old, char *new );
char *	format_string		( char *oldstring );
char *	first_arg		( char *argument, char *arg_first, bool fCase );

/* mail.c */
void	edit_mail_free		( USER_DATA *usr );
void	edit_mail_send		( USER_DATA *usr );
void	edit_mail_mode		( USER_DATA *usr, char *argument );
void	edit_mail_subject	( USER_DATA *usr, char *argument );
void	mail_remove		( USER_DATA *usr, MAIL_DATA *pMail,
				  bool fSave );
void	load_mail		( USER_DATA *usr );
void	do_unread_mail		( USER_DATA *usr );
void	finger_mail		( USER_DATA *usr, char *name );

/* message.c */
void	set_xing_time		( USER_DATA *usr );
void	edit_xing		( USER_DATA *usr, char *argument );
void	edit_xing_receipt	( USER_DATA *usr, char *argument );
void	edit_xing_reply		( USER_DATA *usr, char *argument );
void	edit_feeling_receipt	( USER_DATA *usr, char *argument );
void	edit_feeling		( USER_DATA *usr, char *argument );
void	send_feeling		( USER_DATA *usr, int type );

/* resolve.c */
void	create_resolve		( DESC_DATA *d, long ip, sh_int port );
void	resolve_kill		( DESC_DATA *d );
void	process_resolve		( DESC_DATA *d );

/* user.c */
#define DD DESC_DATA
DD *	new_desc		( void );
void	free_desc		( DESC_DATA *d );
#undef DD
#define UD USER_DATA
UD *	new_user		( void );
void	free_user		( USER_DATA *usr );
#undef UD
bool	is_name			( char *str, char *namelist );
bool	is_name_full		( char *str, char *namelist );
bool	is_user			( char *name );
void	extract_user		( USER_DATA *usr );
#define UD USER_DATA
UD *	get_user		( char *argument );
UD *	get_user_full		( char *argument );
#undef UD
long	get_id			( void );
void	save_user		( USER_DATA *usr );
bool	load_user		( DESC_DATA *d, char *name );
void	info_notify		( USER_DATA *notify, bool fLogin );
void	check_fnotify		( USER_DATA *sender, BOARD_DATA *pBoard );
void	update_bbs		( void );
bool	isBusySelf		( USER_DATA *usr );
void	add_buffer		( USER_DATA *pUser, char *pBuffer );
void	free_buffer		( USER_DATA *pUser );
bool	is_turkish		( USER_DATA *usr );

/*
 * Buffer valid states.
 */
#define BUFFER_SAFE		0
#define BUFFER_OVERFLOW		1
#define BUFFER_FREED		2

/*
 * Editor statuses.
 */
#define EDITOR_NONE		0
#define EDITOR_NOTE		1
#define EDITOR_NOTE_SUBJECT	2
#define EDITOR_MAIL		3
#define EDITOR_MAIL_SUBJECT	4
#define EDITOR_MAIL_WRITE	5

#define EDITOR_XING		6
#define EDITOR_XING_RECEIPT	7
#define EDITOR_XING_REPLY	8
#define EDITOR_PLAN		9
#define EDITOR_PLAN_ANSWER	10
#define EDITOR_QUIT_ANSWER	11
#define EDITOR_PWD_OLD		12
#define EDITOR_PWD_NEW_ONE	13
#define EDITOR_PWD_NEW_TWO	14

#define EDITOR_FEELING		15
#define EDITOR_FEELING_RECEIPT	16

#define EDITOR_INFO		20
#define EDITOR_INFO_ANSWER	21

#define EDITOR_UNDEFINED	23

/*
 * Login state for a channel.
 */
#define CON_LOGIN		0
#define CON_GET_NAME		1
#define CON_GET_OLD_PASSWORD	2
#define CON_GET_NEW_PASSWORD	3
#define CON_CONFIRM_PASSWORD	4
#define CON_GET_REALNAME	5
#define CON_GET_EMAIL		6
#define CON_GET_CORRECT		7
#define CON_GET_GENDER		8
#define CON_GET_HOMEPAGE	9
#define CON_GET_VALIDATE	10
#define CON_BREAK_CONNECT	11
#define CON_REBOOT_RECOVER	12

/*
 * Data files user by the server.
 */
#define TEMP_FILE		"user/bbstmp"
#define TEMP_DIR		"../tmp/"
#define USER_DIR		"user/"
#define CLIP_DIR		"clip/"
#define NOTE_DIR		"boards/"
#define MAIL_DIR		"mails/"
#define NULL_FILE		"/dev/null"
#define HELP_FILE		"data/helps.db"
#define BANISH_FILE		"data/banish.db"
#define VALIDATE_FILE		"data/validate.db"
#define BOARD_FILE		"data/boards.db"

#define BUG_FILE		"log/bug.log"
#define IDEA_FILE		"log/idea.log"
#define CONFIG_FILE		"bbs.config"

#define CMD_DO_FUN( fun )	DO_FUN	fun

/*
 * Command functions.
 */
CMD_DO_FUN( do_age	);
CMD_DO_FUN( do_alia	);
CMD_DO_FUN( do_alias	);
CMD_DO_FUN( do_anonnote	);
CMD_DO_FUN( do_banishes	);
CMD_DO_FUN( do_bug	);
CMD_DO_FUN( do_clip	);
CMD_DO_FUN( do_cls	);
CMD_DO_FUN( do_config	);
CMD_DO_FUN( do_credits	);
CMD_DO_FUN( do_enemy	);
CMD_DO_FUN( do_feeling	);
CMD_DO_FUN( do_finger	);
CMD_DO_FUN( do_fnotify	);
CMD_DO_FUN( do_forumlist);
CMD_DO_FUN( do_friend	);
CMD_DO_FUN( do_from	);
CMD_DO_FUN( do_help	);
CMD_DO_FUN( do_hide	);
CMD_DO_FUN( do_idea	);
CMD_DO_FUN( do_ignore	);
CMD_DO_FUN( do_info	);
CMD_DO_FUN( do_jump	);
CMD_DO_FUN( do_mx	);
CMD_DO_FUN( do_list	);
CMD_DO_FUN( do_look	);
CMD_DO_FUN( do_mail	);
CMD_DO_FUN( do_new	);
CMD_DO_FUN( do_newmsgs	);
CMD_DO_FUN( do_next	);
CMD_DO_FUN( do_note	);
CMD_DO_FUN( do_notify	);
CMD_DO_FUN( do_passwd	);
CMD_DO_FUN( do_plan	);
CMD_DO_FUN( do_previous	);
CMD_DO_FUN( do_read	);
CMD_DO_FUN( do_readall	);
CMD_DO_FUN( do_readlast	);
CMD_DO_FUN( do_remove	);
CMD_DO_FUN( do_reply	);
CMD_DO_FUN( do_search	);
CMD_DO_FUN( do_save	);
CMD_DO_FUN( do_sendclip	);
CMD_DO_FUN( do_set	);
CMD_DO_FUN( do_showclip	);
CMD_DO_FUN( do_time	);
CMD_DO_FUN( do_title	);
CMD_DO_FUN( do_toggle	);
CMD_DO_FUN( do_quit	);
CMD_DO_FUN( do_unalias	);
CMD_DO_FUN( do_unflash	);
CMD_DO_FUN( do_uptime	);
CMD_DO_FUN( do_version	);
CMD_DO_FUN( do_who	);
CMD_DO_FUN( do_Who	);
CMD_DO_FUN( do_whoami	);
CMD_DO_FUN( do_x	);
CMD_DO_FUN( do_xers	);
CMD_DO_FUN( do_zap	);

/* Moderator commands */
CMD_DO_FUN( do_foruminfo);
CMD_DO_FUN( do_kick	);
CMD_DO_FUN( do_stats	);
CMD_DO_FUN( do_unkick	);

/* Admin commands */
CMD_DO_FUN( do_admin	);
CMD_DO_FUN( do_banish	);
CMD_DO_FUN( do_deluser	);
CMD_DO_FUN( do_disconnect);
CMD_DO_FUN( do_echo	);
CMD_DO_FUN( do_force	);
CMD_DO_FUN( do_hostname	);
CMD_DO_FUN( do_invis	);
CMD_DO_FUN( do_memory	);
CMD_DO_FUN( do_reboo	);
CMD_DO_FUN( do_reboot	);
CMD_DO_FUN( do_saveall	);
CMD_DO_FUN( do_shutdow	);
CMD_DO_FUN( do_shutdown	);
CMD_DO_FUN( do_sockets	);
CMD_DO_FUN( do_snoop	);
CMD_DO_FUN( do_unbanish	);
CMD_DO_FUN( do_userset	);
CMD_DO_FUN( do_validate	);

CMD_DO_FUN( do_addforum	);
CMD_DO_FUN( do_delforum	);
CMD_DO_FUN( do_setforum	);
CMD_DO_FUN( do_showmods	);
CMD_DO_FUN( do_statforum);


CMD_DO_FUN( do_test );
CMD_DO_FUN( do_rtest );


