/* terminfo.h gets generated from terminfo.def.h and the terminfo database */

#undef ENTRY_BOOLEAN
#undef ENTRY_NUMBER
#undef ENTRY_STRING

#ifdef TERMINFO_ENUM
#ifdef TERMINFO_BOOLEAN
#define ENTRY_BOOLEAN(x,i) TI_ ## x ,
#endif
#ifdef TERMINFO_NUMBER
#define ENTRY_NUMBER(x,i) TI_ ## x ,
#endif
#ifdef TERMINFO_STRING
#define ENTRY_STRING(x,i) TI_ ## x ,
#endif
#endif
#ifdef TERMINFO_RAW_NAMES
#ifdef TERMINFO_BOOLEAN
#define ENTRY_BOOLEAN(x,i) x
#endif
#ifdef TERMINFO_NUMBER
#define ENTRY_NUMBER(x,i) x
#endif
#ifdef TERMINFO_STRING
#define ENTRY_STRING(x,i) x
#endif
#endif
#ifdef TERMINFO_MAP
#ifdef TERMINFO_BOOLEAN
#define ENTRY_BOOLEAN(x,i) i,
#endif
#ifdef TERMINFO_NUMBER
#define ENTRY_NUMBER(x,i) i,
#endif
#ifdef TERMINFO_STRING
#define ENTRY_STRING(x,i) i,
#endif
#endif
#ifndef TERMINFO_BOOLEAN
#define ENTRY_BOOLEAN(x,i)
#endif
#ifndef TERMINFO_NUMBER
#define ENTRY_NUMBER(x,i)
#endif
#ifndef TERMINFO_STRING
#define ENTRY_STRING(x,i)
#endif

#ifdef TERMINFO_STRING
#ifdef TERMINFO_ENUM
enum terminfo_string {
#endif
#ifdef TERMINFO_MAP
static int terminfo_map_string[] = {
#endif
#endif

/* supported terminal extensions */
/* taken from ncurses' term.h */

/* functions */
ENTRY_STRING(CLEAR_SCREEN        ,5 )
ENTRY_STRING(CURSOR_INVISIBLE    ,13)
ENTRY_STRING(CURSOR_NORMAL       ,16)
ENTRY_STRING(ENTER_BLINK_MODE    ,26)
ENTRY_STRING(ENTER_BOLD_MODE     ,27)
ENTRY_STRING(ENTER_CA_MODE       ,28)
ENTRY_STRING(ENTER_REVERSE_MODE  ,34)
ENTRY_STRING(ENTER_UNDERLINE_MODE,36)
ENTRY_STRING(EXIT_ATTRIBUTE_MODE ,39)
ENTRY_STRING(EXIT_CA_MODE        ,40)
ENTRY_STRING(KEYPAD_LOCAL        ,88)
ENTRY_STRING(KEYPAD_XMIT         ,89)
/* keys */
ENTRY_STRING(KEY_F1              ,66)
ENTRY_STRING(KEY_F2              ,68)
ENTRY_STRING(KEY_F3              ,69)
ENTRY_STRING(KEY_F4              ,70)
ENTRY_STRING(KEY_F5              ,71)
ENTRY_STRING(KEY_F6              ,72)
ENTRY_STRING(KEY_F7              ,73)
ENTRY_STRING(KEY_F8              ,74)
ENTRY_STRING(KEY_F9              ,75)
ENTRY_STRING(KEY_F10             ,67)
ENTRY_STRING(KEY_F11             ,216)
ENTRY_STRING(KEY_F12             ,217)
ENTRY_STRING(KEY_IC              ,77)
ENTRY_STRING(KEY_DC              ,58)
ENTRY_STRING(KEY_HOME            ,76)
ENTRY_STRING(KEY_END             ,164)
ENTRY_STRING(KEY_PPAGE           ,82)
ENTRY_STRING(KEY_NPAGE           ,81)
ENTRY_STRING(KEY_UP              ,87)
ENTRY_STRING(KEY_DOWN            ,61)
ENTRY_STRING(KEY_LEFT            ,79)
ENTRY_STRING(KEY_RIGHT           ,83)

#ifdef TERMINFO_STRING
#if defined(TERMINFO_ENUM)
TI_MAX
#endif
#if defined(TERMINFO_ENUM) || defined(TERMINFO_MAP)
};
#endif
#endif
