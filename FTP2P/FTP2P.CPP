/*
	FTP2P - dual-pane FTP client for MS-DOS
	Copyright (C) 2026 Konstantin Bortnikov

	This file is part of FTP2P.

	FTP2P is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	FTP2P is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FTP2P. If not, see <https://www.gnu.org/licenses/>.
*/

#include <malloc.h>
#include <bios.h>
#include <conio.h>
#include <ctype.h>
#include <direct.h>
#include <dos.h>
#include <io.h>
#include <mem.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>

#include "types.h"
#include "utils.h"
#include "timer.h"


static volatile bool g_ctrlBreakDetected = false;

class CtrlBreakInstaller
{
public:
	CtrlBreakInstaller() : _oldCtrlBreakHandler(NULL), _oldCtrlCHandler(NULL)
	{
		Install();
	}

	~CtrlBreakInstaller()
	{
		Uninstall();
	}

	static void __interrupt __far Handler()
	{
		g_ctrlBreakDetected = true;
	}

private:
	void (__interrupt __far *_oldCtrlBreakHandler)();
	void (__interrupt __far *_oldCtrlCHandler)();

	void Install()
	{
		if (_oldCtrlBreakHandler || _oldCtrlCHandler)
			return;

		_oldCtrlBreakHandler = getvect(0x1b);
		_oldCtrlCHandler = getvect(0x23);

		setvect(0x1b, Handler);
		setvect(0x23, Handler);
	}

	void Uninstall()
	{
		if (_oldCtrlBreakHandler)
		{
			setvect(0x1b, _oldCtrlBreakHandler);
			_oldCtrlBreakHandler = NULL;
		}

		if (_oldCtrlCHandler)
		{
			setvect(0x23, _oldCtrlCHandler);
			_oldCtrlCHandler = NULL;
		}
	}
};

#define FTPSERVERNAME_MAX_LEN (64)
#define USERNAME_MAX_LEN      (64)
#define PASSWORD_MAX_LEN      (40)

static char g_hostArg[FTPSERVERNAME_MAX_LEN];
static uint16_t g_portArg;
static char g_userArg[USERNAME_MAX_LEN];
static char g_passArg[PASSWORD_MAX_LEN];
static bool g_noUtf8Arg;
static bool g_simpleUtf8;


#include "UTIL.CPP"
#include "ENCODING.CPP"

void Log(char type, const char *s)
{
	time_t now = time(NULL);
	struct tm *tmNow = localtime(&now);

	if( s != NULL && s[0] != '\0' )
	{
		FILE *f = fopen("FTP2P.LOG", "at");
		if( f != NULL )
		{
			if( tmNow != NULL )
			{
				fprintf(f, "%04u-%02u-%02u %02u:%02u:%02u [%c] %s\n",
					(unsigned)(tmNow->tm_year + 1900),
					(unsigned)(tmNow->tm_mon + 1),
					(unsigned)tmNow->tm_mday,
					(unsigned)tmNow->tm_hour,
					(unsigned)tmNow->tm_min,
					(unsigned)tmNow->tm_sec,
					type, s);
			}
			else
			{
				fprintf(f, "??? %s\n", s);
			}
			fclose(f);
		}
	}
}

/* ------------------------------------------------------------------------- */
static bool ParseHostPort(const char *src)
{
	char tmp[FTPSERVERNAME_MAX_LEN];
	CopyTrunc(tmp, sizeof(tmp), src);

	g_portArg = 21;

	char *colon = strrchr(tmp, ':');
	if( colon != NULL )
	{
		char *p = colon + 1;
		if( *p == '\0' )
			return false;

		while( *p != '\0' )
		{
			if( !isdigit((unsigned char)*p) )
				return false;
			++p;
		}

		long val = atol(colon + 1);
		if( val <= 0 || val > 65535 )
			return false;

		*colon = '\0';
		if( tmp[0] == '\0' )
			return false;

		g_portArg = (uint16_t)val;
	}

	CopyTrunc(g_hostArg, sizeof(g_hostArg), tmp);
	return g_hostArg[0] != '\0';
}

static bool ParseArgs(int argc, char *argv[])
{
	bool host_set = false;

	g_hostArg[0] = '\0';
	g_userArg[0] = '\0';
	g_passArg[0] = '\0';
	g_portArg = 21;
	g_noUtf8Arg = false;

	if( argc < 2 )
		return false;

	for( int i = 1; i < argc; i++ )
	{
		const char *arg = argv[i];

		if( strcmp(arg, "-u") == 0 )
		{
			if( i > argc - 2 )
				return false;

			i++;
			CopyTrunc(g_userArg, sizeof(g_userArg), argv[i]);
			if( g_userArg[0] == '\0' )
				return false;
		}
		else if( strcmp(arg, "-p") == 0 )
		{
			if( i > argc - 2 )
				return false;

			i++;
			CopyTrunc(g_passArg, sizeof(g_passArg), argv[i]);
			if( g_passArg[0] == '\0' )
				return false;
		}
		else if( strcmp(arg, "-no-utf8") == 0 )
		{
			g_noUtf8Arg = true;
		}
		else if( strcmp(arg, "-simple-utf8") == 0 )
		{
			g_simpleUtf8 = true;
		}
		else if( arg[0] == '-' )
		{
			return false;
		}
		else
		{
			if( host_set )
				return false;

			if( !ParseHostPort(arg) )
				return false;

			host_set = true;
		}
	}

	return host_set;
}

typedef void (*StatusCallback)(char type, const char *s);

/* ------------------------------------------------------------------------- */
#include "UISCREEN.CPP"
static UiScreen g_screen;
/* ------------------------------------------------------------------------- */
#include "UILIST.CPP"
#define UI_MAX_ITEMS (3000)
static UiList g_remoteList(UI_MAX_ITEMS, false);
static UiList g_localList(UI_MAX_ITEMS, true);
/* ------------------------------------------------------------------------- */
#include "FSFTP.CPP"
static FtpTransport g_ftp;
/* ------------------------------------------------------------------------- */
#include "FSLOCAL.CPP"
static LocalFsTransport g_local;
/* ------------------------------------------------------------------------- */
#include "UIVIEW.CPP"
/* ------------------------------------------------------------------------- */
#include "UIAPP.CPP"
static UiApp g_app;

void SetStatusCB(char type, const char *s)
{
	if( !g_screen.IsReady() )
		return;

	g_app.SetStatus(type, s);
	//Log(type, s);
}

/* ------------------------------------------------------------------------- */
#define APP_VERSION "v3.6"

int main(int argc, char *argv[])
{
	printf("FTP2P by Konstantin Bortnikov (radozd@ya.ru) (C)opyright 2026\nVersion: " APP_VERSION ", " __DATE__ "\n");

	if( !g_remoteList.Ok() || !g_localList.Ok() )
	{
		printf("Not enough free memory\n");
		return 1;
	}

	if( !ParseArgs(argc, argv) )
	{
		printf("Usage: FTP2P <host[:port]> [-u user] [-p pass] [-no-utf8]\n");
		printf("  <host[:port]> FTP server name or IP address\n");
		printf("  -u user       User name\n");
		printf("  -p pass       Password\n");
		printf("  -no-utf8      Disable UTF-8 file name conversion\n");
		printf("  -simple-utf8  Fold UTF-8 diacritics to ASCII (CP850/852)\n");
		printf("\n");
		printf("Dual-pane MS-DOS FTP client: left=remote, right=local.\n");
		printf("Ops: cd/get/put/rename/mkdir/delete, multi-file copy.\n");
		printf("\n");
		printf("Keys: Tab active panel, Shift+Tab swap panels, Enter open, Backspace up\n");
		printf("      Left/Right/PgUp/PgDn move by page, Up/Down/Home/End move\n");
		printf("      Shift+Left/Right resize panels, Esc abort, F10 quit\n");
		printf("      Ins/Space mark, Gray +/-/* select\n");
		printf("      F5 copy, F6 rename, F7 mkdir, F8 delete\n");
		printf("      F9 overwrite policy: ASK/SKIP/OVRW\n");
		return 1;
	}

	CtrlBreakInstaller cbreak;
	InitDosCharset();

	if( !g_local.Init(SetStatusCB, &g_localList) )
		return 1;

	if( !g_ftp.Init(SetStatusCB, &g_remoteList) )
		return 1;

	if( !g_ftp.ConnectAndLogin(CtrlBreakInstaller::Handler, g_hostArg, g_portArg, g_userArg, g_passArg) )
	{
		g_ftp.Cleanup();
		return 1;
	}

	delay(1000);

	if( !g_app.InitScreen() )
	{
		g_ftp.Cleanup();
		return 1;
	}

	const char *message = g_app.Loop();

	g_ftp.Cleanup();
	g_app.Cleanup();

	printf("%s\n", message);
	return 0;
}
