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


#include "UTIL.CPP"

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
#define FTPSERVERNAME_MAX_LEN (64)
#define USERNAME_MAX_LEN      (64)
#define PASSWORD_MAX_LEN      (40)

static char g_HostArg[FTPSERVERNAME_MAX_LEN];
static uint16_t g_PortArg;
static char g_UserArg[USERNAME_MAX_LEN];
static char g_PassArg[PASSWORD_MAX_LEN];
static bool g_NoUtf8Arg;

static bool ParseHostPort(const char *src)
{
	char tmp[FTPSERVERNAME_MAX_LEN];
	CopyTrunc(tmp, sizeof(tmp), src);

	g_PortArg = 21;

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

		long portVal = atol(colon + 1);
		if( portVal <= 0 || portVal > 65535 )
			return false;

		*colon = '\0';
		if( tmp[0] == '\0' )
			return false;

		g_PortArg = (uint16_t)portVal;
	}

	CopyTrunc(g_HostArg, sizeof(g_HostArg), tmp);
	return g_HostArg[0] != '\0';
}

static bool ParseArgs(int argc, char *argv[])
{
	bool hostSet = false;

	g_HostArg[0] = '\0';
	g_UserArg[0] = '\0';
	g_PassArg[0] = '\0';
	g_PortArg = 21;
	g_NoUtf8Arg = false;

	if( argc < 2 )
		return false;

	for( int i = 1; i < argc; ++i )
	{
		const char *arg = argv[i];

		if( strcmp(arg, "-u") == 0 )
		{
			if( i > argc - 2 )
				return false;

			++i;
			CopyTrunc(g_UserArg, sizeof(g_UserArg), argv[i]);
			if( g_UserArg[0] == '\0' )
				return false;
		}
		else if( strcmp(arg, "-p") == 0 )
		{
			if( i > argc - 2 )
				return false;

			++i;
			CopyTrunc(g_PassArg, sizeof(g_PassArg), argv[i]);
			if( g_PassArg[0] == '\0' )
				return false;
		}
		else if( strcmp(arg, "-no-utf8") == 0 )
		{
			g_NoUtf8Arg = true;
		}
		else if( arg[0] == '-' )
		{
			return false;
		}
		else
		{
			if( hostSet )
				return false;

			if( !ParseHostPort(arg) )
				return false;

			hostSet = true;
		}
	}

	if( !hostSet )
		return false;

	return true;
}

typedef void (*StatusCallback)(char type, const char *s);

/* ------------------------------------------------------------------------- */
static uint8_t far *g_Screen = NULL;
static uint16_t g_ScreenRows = 25, g_ScreenCols = 80;
/* ------------------------------------------------------------------------- */
#include "UILIST.CPP"

#define UI_MAX_ITEMS          (400)
static UiList g_RemoteList(UI_MAX_ITEMS, false);
static UiList g_LocalList(UI_MAX_ITEMS, true);
/* ------------------------------------------------------------------------- */
#include "FSFTP.CPP"
static FtpTransport g_ftp;
/* ------------------------------------------------------------------------- */
#include "FSLOCAL.CPP"
static LocalFsTransport g_local;
/* ------------------------------------------------------------------------- */
#include "UIAPP.CPP"
static UiApp g_app;

void SetStatusText(char type, const char *s)
{
	g_app.SetStatusText(type, "%s", s);
	//Log(type, s);
}

/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	printf("FTP2P by Konstantin Bortnikov (radozd@ya.ru) (C)opyright 2026\nVersion: " __DATE__ "\n");

	if( !g_RemoteList.Ok() || !g_LocalList.Ok() )
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
		printf("\n");
		printf("Dual-pane MS-DOS FTP client: left=remote, right=local.\n");
		printf("Ops: cd/get/put/rename/mkdir/delete, multi-file copy.\n");
		printf("\n");
		printf("Keys: Tab panel, Enter open, Esc abort, F10 quit\n");
		printf("      Up/Down/PgUp/PgDn/Home/End move\n");
		printf("      Ins/Space mark, Gray +/-/* select\n");
		printf("      F5 copy, F6 rename, F7 mkdir, F8 delete\n");
		printf("      F9 overwrite policy: ASK/SKIP/OVRW\n");
		return 1;
	}

	if( !g_local.Init(SetStatusText, &g_LocalList) )
		return 1;

	if( !g_ftp.Init(SetStatusText, &g_RemoteList) )
		return 1;

	if( !g_ftp.ConnectAndLogin(g_HostArg, g_PortArg, g_UserArg, g_PassArg) )
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

	g_app.Loop();

	g_app.Cleanup();
	g_ftp.Cleanup();
	return 0;
}
