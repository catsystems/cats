///*
// * cli.c
// *
// *  Created on: 3 May 2021
// *      Author: Luca
// */
//


#include "util/cli.h"
#include <string.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>

#define CLI_IN_BUFFER_SIZE 256
#define CLI_OUT_BUFFER_SIZE 256

static uint32_t bufferIndex = 0;

static char cliBuffer[CLI_IN_BUFFER_SIZE];

static fifo_t *cli_in;
static fifo_t *cli_out;

typedef void cliCommandFn(const char* name, char *cmdline);

#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))

typedef struct {
    const char *name;
    const char *description;
    const char *args;
    cliCommandFn *cliCommand;
} clicmd_t;

#define CLI_COMMAND_DEF(name, description, args, cliCommand) \
{ \
    name , \
    description , \
    args , \
    cliCommand \
}

static bool isEmpty(const char *string)
{
    return (string == NULL || *string == '\0') ? true : false;
}

static void cliDefaults(const char *cmdName, char *cmdline);
static void cliHelp(const char *cmdName, char *cmdline);
static void cliSave(const char *cmdName, char *cmdline);
static void cliDump(const char *cmdName, char *cmdline);
static void cliExit(const char *cmdName, char *cmdline);
static void cliGet(const char *cmdName, char *cmdline);
static void cliMcuId(const char *cmdName, char *cmdline);
static void cliSave(const char *cmdName, char *cmdline);
static void cliSet(const char *cmdName, char *cmdline);
static void cliStatus(const char *cmdName, char *cmdline);
static void cliVersion(const char *cmdName, char *cmdline);

void cliPrint(const char *str);
void cliPrintLinefeed(void);
void cliPrintLine(const char *str);

static void cliPrintHashLine(const char *str);
static bool cliDumpPrintLinef(bool equalsDefault, const char *format, ...);
static void cliWrite(uint8_t ch);

void cliPrintf(const char *format, ...);
void cliPrintLinef(const char *format, ...);
static void cliPrintErrorVa(const char *cmdName, const char *format, va_list va);
static void cliPrintError(const char *cmdName, const char *format, ...);
static void cliPrintErrorLinef(const char *cmdName, const char *format, ...);


const clicmd_t cmdTable[] = {
    //CLI_COMMAND_DEF("bl", "reboot into bootloader", "[rom]", cliBootloader),
    CLI_COMMAND_DEF("defaults", "reset to defaults and reboot", "[nosave|show]", cliDefaults),
    CLI_COMMAND_DEF("dump", "dump configuration", "[master|profile|rates|hardware|all] {defaults|bare}", cliDump),
    CLI_COMMAND_DEF("exit", NULL, NULL, cliExit),
    CLI_COMMAND_DEF("get", "get variable value", "[name]", cliGet),
    CLI_COMMAND_DEF("help", "display command help", "[search string]", cliHelp),
    CLI_COMMAND_DEF("mcu_id", "id of the microcontroller", NULL, cliMcuId),
    CLI_COMMAND_DEF("save", "save and reboot", NULL, cliSave),
    CLI_COMMAND_DEF("set", "change setting", "[<name>=<value>]", cliSet),
    CLI_COMMAND_DEF("status", "show status", NULL, cliStatus),
    CLI_COMMAND_DEF("version", "show version", NULL, cliVersion),
};

static void cliDefaults(const char *cmdName, char *cmdline){

}

static void cliDump(const char *cmdName, char *cmdline){

}

static void cliExit(const char *cmdName, char *cmdline){

}

static void cliGet(const char *cmdName, char *cmdline){

}

static void cliMcuId(const char *cmdName, char *cmdline){

}

static void cliSave(const char *cmdName, char *cmdline){

}

static void cliSet(const char *cmdName, char *cmdline){

}

static void cliStatus(const char *cmdName, char *cmdline){

}

static void cliVersion(const char *cmdName, char *cmdline){

}

static void cliHelp(const char *cmdName, char *cmdline)
{
    bool anyMatches = false;

    for (uint32_t i = 0; i < ARRAYLEN(cmdTable); i++) {
        bool printEntry = false;
        if (isEmpty(cmdline)) {
            printEntry = true;
        } else {
            if (strstr(cmdTable[i].name, cmdline)
                || strstr(cmdTable[i].description, cmdline)
               ) {
                printEntry = true;
            }
        }

        if (printEntry) {
            anyMatches = true;
            cliPrint(cmdTable[i].name);
            if (cmdTable[i].description) {
                cliPrintf(" - %s", cmdTable[i].description);
            }
            if (cmdTable[i].args) {
                cliPrintf("\r\n\t%s", cmdTable[i].args);
            }
            cliPrintLinefeed();
        }
    }
    if (!isEmpty(cmdline) && !anyMatches) {
        cliPrintErrorLinef(cmdName, "NO MATCHES FOR '%s'", cmdline);
    }
}


void cliPrint(const char *str)
{
	while (*str) {
		fifo_write(cli_out, *str++);
	}
}

void cliPrintLinefeed(void)
{
    cliPrint("\r\n");
}

void cliPrintLine(const char *str)
{
    cliPrint(str);
    cliPrintLinefeed();
}

static void cliPrintHashLine(const char *str)
{
    cliPrint("\r\n# ");
    cliPrintLine(str);
}

static void cliPrintfva(const char *format, va_list va)
{
	char buffer[CLI_OUT_BUFFER_SIZE];
	vsnprintf(buffer, CLI_OUT_BUFFER_SIZE, format, va);
	cliPrint(buffer);
}

static bool cliDumpPrintLinef(bool equalsDefault, const char *format, ...)
{
        va_list va;
        va_start(va, format);
        cliPrintfva(format, va);
        va_end(va);
        cliPrintLinefeed();
        return true;

}

static void cliWrite(uint8_t ch)
{
	fifo_write(cli_out, ch);
}

static bool cliDefaultPrintLinef(bool equalsDefault, const char *format, ...)
{

	cliWrite('#');

	va_list va;
	va_start(va, format);
	cliPrintfva(format, va);
	va_end(va);
	cliPrintLinefeed();
	return true;

}

void cliPrintf(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    cliPrintfva(format, va);
    va_end(va);
}


void cliPrintLinef(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    cliPrintfva(format, va);
    va_end(va);
    cliPrintLinefeed();
}

static void cliPrintErrorVa(const char *cmdName, const char *format, va_list va)
{

	cliPrint("###ERROR IN ");
	cliPrint(cmdName);
	cliPrint(": ");
	char buffer[CLI_OUT_BUFFER_SIZE];
	vsnprintf(buffer, CLI_OUT_BUFFER_SIZE, format, va);
	cliPrint(buffer);
	cliPrint(": ");
	va_end(va);
	cliPrint( "###");

}

static void cliPrintError(const char *cmdName, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    cliPrintErrorVa(cmdName, format, va);
}

static void cliPrintErrorLinef(const char *cmdName, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    cliPrintErrorVa(cmdName, format, va);
    cliPrint("\r\n");
}

static char *skipSpace(char *buffer)
{
    while (*(buffer) == ' ') {
        buffer++;
    }
    return buffer;
}

static char *checkCommand(char *cmdline, const char *command)
{
    if (!strncasecmp(cmdline, command, strlen(command))   // command names match
        && (isspace((unsigned)cmdline[strlen(command)]) || cmdline[strlen(command)] == 0)) {
        return skipSpace(cmdline + strlen(command) + 1);
    } else {
        return 0;
    }
}

static void processCharacter(const char c){
    if (bufferIndex && (c == '\n' || c == '\r')) {
        // enter pressed
        cliPrintLinefeed();

        // Strip comment starting with # from line
        char *p = cliBuffer;
        p = strchr(p, '#');
        if (NULL != p) {
            bufferIndex = (uint32_t)(p - cliBuffer);
        }
        // Strip trailing whitespace
        while (bufferIndex > 0 && cliBuffer[bufferIndex - 1] == ' ') {
            bufferIndex--;
        }

        // Process non-empty lines
        if (bufferIndex > 0) {
            cliBuffer[bufferIndex] = 0; // null terminate

            const clicmd_t *cmd;
            char *options;
            for (cmd = cmdTable; cmd < cmdTable + ARRAYLEN(cmdTable); cmd++) {
                if ((options = checkCommand(cliBuffer, cmd->name))) {
                    break;
                }
            }
            if (cmd < cmdTable + ARRAYLEN(cmdTable)) {
                cmd->cliCommand(cmd->name, options);
            } else {
            	cliPrintLine("UNKNOWN COMMAND, TRY 'HELP'");
            }
            bufferIndex = 0;
        }

        memset(cliBuffer, 0, sizeof(cliBuffer));

        // 'exit' will reset this flag, so we don't need to print prompt again

    } else if (bufferIndex < sizeof(cliBuffer) && c >= 32 && c <= 126) {
        if (!bufferIndex && c == ' ')
            return; // Ignore leading spaces
        cliBuffer[bufferIndex++] = c;
        cliWrite(c);
    }
}

void cli_process(void){
    while (fifo_get_length(cli_in) > 0) {
        processCharacter(fifo_read(cli_in));
    }
}


void cli_enter(fifo_t *in, fifo_t *out){
    cli_in = in;
    cli_out = out;
    cliPrintLine("\r\nEntering CLI Mode, type 'exit' to return, or 'help'");
}
