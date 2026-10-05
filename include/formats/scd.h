/*
 *  RaptorScope  -  SCD Script Disassembler
 *  Dino Crisis (PC) bytecode disassembly and display.
 *
 *  Opcode table derived from DINO.exe dispatch table at VA 0x657698.
 *  Handler function addresses cross-referenced with decompiled code.
 */
#ifndef SCD_H
#define SCD_H

#include "core/types.h"

/*═══════════════════════════════════════════════════════════════════
 *  Opcode Info Table
 *═══════════════════════════════════════════════════════════════════*/

struct ScdOpcodeInfo {
    u8          size;       /* instruction size in bytes (0 = variable/terminal) */
    const char* name;       /* mnemonic */
    const char* desc;       /* short description */
    u8          category;   /* 0=flow, 1=thread, 2=var, 3=scene, 4=entity,
                               5=camera, 6=item, 7=sound, 8=effect, 9=nop */
};

/* DC1 SCD opcode table — 112 entries (0x00..0x6F) + 0x70..0xFF = NOP(1)
   Sizes from DINO.exe analysis. Names from decompiled handler behavior.
   0x28 is variable-size (not in this table — handled specially). */
static const ScdOpcodeInfo g_scd_opcodes[0x70] = {
 /* 0x00 */ {  1, "NOP",           "No operation",                               9 },
 /* 0x01 */ {  4, "UNK_01",        "Unknown (4 bytes; not a thread end)",        0 },
 /* 0x02 */ {  4, "EVT_NEXT",      "Yield to next frame",                        1 },
 /* 0x03 */ {  4, "EVT_CHAIN",     "Chain to event (conditional)",               0 },
 /* 0x04 */ {  4, "EVT_END",       "End of thread",                              0 },
 /* 0x05 */ {  4, "EVT_KILL",      "Kill event thread",                          1 },
 /* 0x06 */ {  4, "IF_END",        "End if block (pop cond stack)",              0 },
 /* 0x07 */ {  4, "GOTO",          "Goto (absolute via ptr table)",              0 },
 /* 0x08 */ {  4, "THREAD_CLR",    "Clear/kill thread slot",                     1 },
 /* 0x09 */ {  8, "THREAD_CTRL",   "Thread state control",                       1 },
 /* 0x0A */ {  4, "UNK_0A",        "Unknown (4 bytes)",                          1 },
 /* 0x0B */ {  4, "LOOP_SET",      "Set loop counter and target",                0 },
 /* 0x0C */ {  0, "LOOP_BACK",     "Decrement loop, branch if >0",              0 },
 /* 0x0D */ {  4, "CALL",          "Push return addr, call subroutine",          0 },
 /* 0x0E */ {  4, "JUMP_REL",      "Relative jump (s16 offset)",                 0 },
 /* 0x0F */ {  4, "JUMP_SKIP",     "Skip next (advance 4)",                      0 },
 /* 0x10 */ {  1, "CK_FLAG_A",     "Check flag type A",                          0 },
 /* 0x11 */ {  1, "CK_FLAG_B",     "Check flag type B",                          0 },
 /* 0x12 */ {  4, "RETURN",        "Return from subroutine",                     0 },
 /* 0x13 */ {  4, "UNK_13",        "Unknown (4 bytes)",                           9 },
 /* 0x14 */ {  4, "CK_GAME",       "Check game state flag",                      0 },
 /* 0x15 */ {  8, "CALC_OP",       "Arithmetic / variable calc",                 2 },
 /* 0x16 */ {  4, "CK_CMP",        "Compare values (conditional)",               0 },
 /* 0x17 */ {  4, "EVAL",          "Evaluate expression",                         2 },
 /* 0x18 */ {  4, "SET_VAR",       "Set variable (thread-local)",                2 },
 /* 0x19 */ {  1, "PUSH_VAR",      "Push variable to stack",                     2 },
 /* 0x1A */ {  1, "UNK_1A",        "Unknown (1 byte)",                           9 },
 /* 0x1B */ {  1, "NOP_1B",        "No operation",                               9 },
 /* 0x1C */ {  1, "NOP_1C",        "No operation",                               9 },
 /* 0x1D */ {  1, "NOP_1D",        "No operation",                               9 },
 /* 0x1E */ {  1, "NOP_1E",        "No operation",                               9 },
 /* 0x1F */ {  1, "NOP_1F",        "No operation",                               9 },
 /* 0x20 */ { 24, "CHAR_SET",      "Character spawn / placement",                4 },
 /* 0x21 */ {  8, "CHAR_CMD",      "Character command",                          4 },
 /* 0x22 */ {  4, "WORK_SET",      "Set working entity context",                 4 },
 /* 0x23 */ { 32, "SEC_INST",      "Section instance (xform mesh)",              3 },
 /* 0x24 */ {  8, "SEC_INFO",      "Section info / query",                       3 },
 /* 0x25 */ {  4, "SEC_POS",       "Section position set",                       3 },
 /* 0x26 */ {  4, "ROOM_SET",      "Room transition setup",                      3 },
 /* 0x27 */ {  4, "MAP_OPEN",      "Open map / area transition",                 3 },
 /* 0x28 */ {  0, "ZONE_DEF",      "Zone definition (variable size)",            3 },
 /* 0x29 */ {  8, "RAND",          "Random number generator",                    2 },
 /* 0x2A */ {  8, "UNK_2A",        "Unknown (8 bytes)",                          9 },
 /* 0x2B */ {  8, "CK_VAR",        "Check variable (conditional)",               2 },
 /* 0x2C */ {  4, "SET_TIMER",     "Set countdown timer",                        2 },
 /* 0x2D */ {  4, "CK_TIMER",      "Check timer expired",                        2 },
 /* 0x2E */ { 20, "CAM_SET",       "Camera eye + target (short 0x4C)",           5 },
 /* 0x2F */ {  4, "MSG_OFF",       "Message display off",                        7 },
 /* 0x30 */ {  4, "MSG_SET",       "Set message parameters",                     7 },
 /* 0x31 */ {  4, "UNK_31",        "Unknown (4 bytes)",                          9 },
 /* 0x32 */ {  4, "FADE_SET",      "Screen fade effect",                         8 },
 /* 0x33 */ {  4, "FADE_ADJ",      "Adjust fade parameters",                     8 },
 /* 0x34 */ {  4, "SHAKE",         "Camera shake effect",                        5 },
 /* 0x35 */ {  8, "POS_SET",       "Set entity position",                        4 },
 /* 0x36 */ {  8, "SPD_ADD",       "Add speed to entity",                        4 },
 /* 0x37 */ {  8, "SND_PLAY",      "Play sound effect",                          7 },
 /* 0x38 */ {  4, "SND_STOP",      "Stop sound",                                 7 },
 /* 0x39 */ {  4, "BGM_SET",       "Set background music",                       7 },
 /* 0x3A */ { 12, "LIGHT_SET",     "Scene light (position + color)",             3 },
 /* 0x3B */ {  4, "LIGHT_ADJ",     "Adjust light parameters",                    3 },
 /* 0x3C */ {  8, "LIGHT_COL",     "Set light color RGB",                        3 },
 /* 0x3D */ { 12, "UNK_3D",        "Unknown (12 bytes)",                         9 },
 /* 0x3E */ {  4, "ITEM_LOSE",     "Remove item from inventory",                 6 },
 /* 0x3F */ {  8, "ITEM_CK",       "Check item in inventory",                    6 },
 /* 0x40 */ { 12, "EM_CTRL",       "Enemy control command",                      4 },
 /* 0x41 */ {  4, "EM_KILL",       "Kill / remove enemy",                        4 },
 /* 0x42 */ { 20, "EM_SET",        "Spawn enemy / entity",                       4 },
 /* 0x43 */ {  8, "EM_POS",        "Set enemy position",                         4 },
 /* 0x44 */ {  8, "EM_DIR",        "Set enemy facing direction",                 4 },
 /* 0x45 */ {  8, "POS_LOAD",      "Load position XYZ",                          4 },
 /* 0x46 */ {  8, "DIR_LOAD",      "Load direction XYZ",                         4 },
 /* 0x47 */ {  4, "ROT_LOAD",      "Load rotation angles",                       4 },
 /* 0x48 */ {  4, "EM_FLAG",       "Set enemy flags",                            4 },
 /* 0x49 */ {  4, "UNK_49",        "Unknown (4 bytes)",                          9 },
 /* 0x4A */ {  4, "UNK_4A",        "Unknown (4 bytes)",                          9 },
 /* 0x4B */ { 12, "CK_BIT",        "Check bit flag (conditional)",               0 },
 /* 0x4C */ { 32, "CAMERA",        "Camera eye + target + FOV",                  5 },
 /* 0x4D */ {  8, "CAM_CTRL",      "Camera control / animation",                 5 },
 /* 0x4E */ {  8, "CAM_MOVE",      "Camera movement command",                    5 },
 /* 0x4F */ {  8, "CAM_SPD",       "Camera speed / interpolation",               5 },
 /* 0x50 */ {  8, "CAM_FLAG",      "Camera flags / mode",                        5 },
 /* 0x51 */ {  8, "CAM_CALC",      "Camera calculation",                         5 },
 /* 0x52 */ {  4, "CAM_LOCK",      "Lock camera angle",                          5 },
 /* 0x53 */ { 12, "CAM_PATH",      "Camera path / spline",                       5 },
 /* 0x54 */ {  4, "CAM_END",       "End camera sequence",                        5 },
 /* 0x55 */ { 12, "ANIM_SET",      "Set animation clip",                         4 },
 /* 0x56 */ { 28, "CUTSCENE",      "Cutscene / cinematic data",                  8 },
 /* 0x57 */ {  8, "EFFECT_A",      "Visual effect type A",                       8 },
 /* 0x58 */ {  8, "EFFECT_B",      "Visual effect type B",                       8 },
 /* 0x59 */ { 20, "EFFECT_C",      "Visual effect type C",                       8 },
 /* 0x5A */ {  8, "MODEL_SET",     "Set model display parameters",               3 },
 /* 0x5B */ { 44, "ITEM_SET",      "Item pickup definition",                     6 },
 /* 0x5C */ {  4, "ITEM_FLAG",     "Item flag / state",                          6 },
 /* 0x5D */ {  4, "UNK_5D",        "Unknown (4 bytes)",                          9 },
 /* 0x5E */ { 20, "CK_ENEMY",      "Check enemy state (conditional)",            4 },
 /* 0x5F */ { 12, "PLR_CTRL",      "Player control override",                    4 },
 /* 0x60 */ {  4, "UNK_60",        "Unknown (4 bytes)",                          9 },
 /* 0x61 */ {  4, "UNK_61",        "Unknown (4 bytes)",                          9 },
 /* 0x62 */ {  8, "UNK_62",        "Unknown (8 bytes)",                          9 },
 /* 0x63 */ {  8, "UNK_63",        "Unknown (8 bytes)",                          9 },
 /* 0x64 */ {  4, "UNK_64",        "Unknown (4 bytes)",                          9 },
 /* 0x65 */ {  4, "UNK_65",        "Unknown (4 bytes)",                          9 },
 /* 0x66 */ {  4, "UNK_66",        "Unknown (4 bytes)",                          9 },
 /* 0x67 */ {  8, "SYS_A",         "System command A",                           9 },
 /* 0x68 */ {  8, "SYS_B",         "System command B",                           9 },
 /* 0x69 */ {  4, "UNK_69",        "Unknown (4 bytes)",                          9 },
 /* 0x6A */ {  8, "SYS_C",         "System command C",                           9 },
 /* 0x6B */ {  4, "SYS_D",         "System command D",                           9 },
 /* 0x6C */ {  4, "SYS_E",         "System command E",                           9 },
 /* 0x6D */ {  4, "SYS_F",         "System command F",                           9 },
 /* 0x6E */ {  4, "SYS_G",         "System command G",                           9 },
 /* 0x6F */ {  4, "SYS_H",         "System command H",                           9 },
};

/* Opcode 0x28 sub-type sizes (zone definitions) */
static const int g_scd_op28_sizes[12] = {
    48, 40, 32, 36, 44, 32, 32, 52, 32, 32, 32, 32
};
static const char* g_scd_op28_names[12] = {
    "ZONE_ENTER",  "ZONE_FLOOR",  "ZONE_EXEC",   "ZONE_LADDER",
    "ZONE_DOOR",   "ZONE_CUT",    "ZONE_T6",     "ZONE_LIGHT",
    "ZONE_T8",     "ZONE_T9",     "ZONE_T10",    "ZONE_T11"
};

/* Category names (for color coding) */
static const char* g_scd_cat_names[] = {
    "flow", "thread", "var", "scene", "entity",
    "camera", "item", "sound", "effect", "nop"
};

/*═══════════════════════════════════════════════════════════════════
 *  Disassembled SCD Data
 *═══════════════════════════════════════════════════════════════════*/

struct ScdLine {
    u32     offset;         /* byte offset from SCD base */
    u8      thread;         /* which thread (0xFF = header) */
    u8      opcode;         /* raw opcode byte */
    u8      op28_type;      /* sub-type for 0x28 */
    int     size;           /* instruction byte count */
    int     indent;         /* nesting depth for control flow */
    char    text[256];      /* formatted disassembly line */
    char    hex[128];       /* raw hex bytes */
    char    comment[128];   /* auto-generated comment */
    bool    is_label;       /* true = this line is a jump target label */
    bool    is_branch;      /* true = this is a branch instruction */
    s32     branch_target;  /* absolute offset of branch target (-1 = none) */
};

struct ScdDisasm {
    ScdLine*    lines;
    int         n_lines;
    int         capacity;
    int         n_threads;
    u32         thread_offsets[64];  /* relative offsets from SCD start */

    void init()  { lines = 0; n_lines = 0; capacity = 0; n_threads = 0; }
    void clear() { free(lines); lines = 0; n_lines = 0; capacity = 0; n_threads = 0; }

    ScdLine& add() {
        if (n_lines >= capacity) {
            capacity = capacity ? capacity * 2 : 256;
            lines = (ScdLine*)realloc(lines, capacity * sizeof(ScdLine));
        }
        ScdLine& l = lines[n_lines++];
        memset(&l, 0, sizeof(l));
        l.branch_target = -1;
        return l;
    }
};

/*═══════════════════════════════════════════════════════════════════
 *  Disassembly Function
 *═══════════════════════════════════════════════════════════════════*/

/* Disassemble SCD bytecode from an RDT's ptr[5] section.
   dec       = decompressed RDT data
   dec_size  = size of dec
   base_addr = PSX base address (usually 0x80100000)
   out       = output disassembly */
bool scd_disassemble(const u8* dec, size_t dec_size, u32 base_addr,
                     ScdDisasm& out);

/*═══════════════════════════════════════════════════════════════════
 *  DC1 Text String Decoder
 *═══════════════════════════════════════════════════════════════════*/

/* DC1 text encoding: 16-bit words (glyph, 0x80).
   Uppercase A-Z at 0x02..0x34 (stride 2).
   Lowercase a-z at 0x36..0x68 (stride 2).
   Digits 0-9 at 0x6A..0x7C (stride 2).
   0x00 = space, 0x82 = '.', 0x84 = ',', 0x86 = '!',
   0x88 = '?', 0x8A = '-', 0x8C = ':', 0x90 = '"',
   0x92 = "'", 0x9A = '.', 0x9E = '/'.
   (0x00, 0xC0) = newline.
   (0x00, 0x01)(0x00, 0x90)(0x01, 0x10) = message separator.
   (0x00, 0xA0)(0x01, 0x10) = message end + pause.
   (0x00, 0x08) = page break. */

struct RdtTextBlock {
    int     offset;         /* byte offset in decompressed RDT */
    int     raw_size;       /* raw byte count of this block */
    char    text[2048];     /* decoded ASCII text */
    int     text_len;
};

struct RdtTextData {
    RdtTextBlock  blocks[64];
    int           n_blocks;
    int           total_chars;

    void init() { n_blocks = 0; total_chars = 0; }
};

/* Extract and decode text strings from decompressed RDT data.
   Text is located after the SCD section. */
bool rdt_extract_text(const u8* dec, size_t dec_size, u32 base_addr,
                      RdtTextData& out);

#endif /* SCD_H */

