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

#define UI_KEY_TAB      0x0F00
#define UI_KEY_ENTER    0x1C00
#define UI_KEY_ESC      0x011B

#define UI_KEY_UP       0x4800
#define UI_KEY_DOWN     0x5000
#define UI_KEY_LEFT     0x4B00
#define UI_KEY_RIGHT    0x4D00
#define UI_KEY_HOME     0x4700
#define UI_KEY_END      0x4F00
#define UI_KEY_PGUP     0x4900
#define UI_KEY_PGDN     0x5100

#define UI_KEY_F1       0x3B00
#define UI_KEY_F2       0x3C00
#define UI_KEY_F3       0x3D00
#define UI_KEY_F4       0x3E00
#define UI_KEY_F5       0x3F00
#define UI_KEY_F6       0x4000
#define UI_KEY_F7       0x4100
#define UI_KEY_F8       0x4200
#define UI_KEY_F9       0x4300
#define UI_KEY_F10      0x4400

enum
{
	CH_UL    = 218, // ┌
	CH_UR    = 191, // ┐
	CH_LL    = 192, // └
	CH_LR    = 217, // ┘
	CH_H     = 196, // ─
	CH_V     = 179, // │
	CH_LT    = 195, // ├
	CH_RT    = 180, // ┤
	CH_THUMB = 219, // █
	CH_SHADE = 176  // ░
};

typedef enum {
	EGA_BLACK          = 0x0,
	EGA_BLUE           = 0x1,
	EGA_GREEN          = 0x2,
	EGA_CYAN           = 0x3,
	EGA_RED            = 0x4,
	EGA_MAGENTA        = 0x5,
	EGA_BROWN          = 0x6,
	EGA_LIGHT_GRAY     = 0x7,
	EGA_DARK_GRAY      = 0x8,
	EGA_LIGHT_BLUE     = 0x9,
	EGA_LIGHT_GREEN    = 0xA,
	EGA_LIGHT_CYAN     = 0xB,
	EGA_LIGHT_RED      = 0xC,
	EGA_LIGHT_MAGENTA  = 0xD,
	EGA_YELLOW         = 0xE,
	EGA_WHITE          = 0xF
} ega_color_t;

#define UI_ATTR(fg, bg) (((bg) << 4) | (fg))

#define UI_ATTR_NORMAL            UI_ATTR(EGA_LIGHT_GRAY, EGA_BLACK)
#define UI_ATTR_SELECTED          UI_ATTR(EGA_BLACK, EGA_LIGHT_GRAY)
#define UI_ATTR_DIRECTORY         UI_ATTR(EGA_WHITE, EGA_BLACK)
#define UI_ATTR_FOOTER_TEXT       UI_ATTR(EGA_BLACK, EGA_LIGHT_GRAY)

#define UI_ATTR_STATUS_OK         UI_ATTR(EGA_LIGHT_GRAY, EGA_BLACK)
#define UI_ATTR_STATUS_WARNING    UI_ATTR(EGA_YELLOW, EGA_BLACK)
#define UI_ATTR_STATUS_ERROR      UI_ATTR(EGA_LIGHT_RED, EGA_BLACK)
#define UI_ATTR_STATUS_QUESTION   UI_ATTR(EGA_LIGHT_GREEN, EGA_BLACK)
#define UI_ATTR_STATUS_TRANSFER   UI_ATTR(EGA_CYAN, EGA_BLACK)

#define UI_KEY_CONTROL_CLOSED 0xFFFFu

/* ------------------------------------------------------------------------- */

class UiScreenSnapshot
{
public:
	UiScreenSnapshot();
	~UiScreenSnapshot();

	void Capture(uint8_t far *screen, uint8_t cols, uint8_t rows);
	void Restore(uint8_t far *screen);
	void HideCursor();
	void ShowCursor();

private:
	uint16_t _savedScreenBytes;
	uint16_t _savedScreenCapacity;
	uint8_t _savedCursorStart;
	uint8_t _savedCursorEnd;
	uint8_t _savedCursorX;
	uint8_t _savedCursorY;
	uint8_t *_savedScreen;
	bool _valid;
};

UiScreenSnapshot::UiScreenSnapshot()
{
	_savedScreenBytes = 0;
	_savedScreenCapacity = 0;
	_savedCursorStart = 6;
	_savedCursorEnd = 7;
	_savedCursorX = 0;
	_savedCursorY = 0;
	_savedScreen = NULL;
	_valid = false;
}

UiScreenSnapshot::~UiScreenSnapshot()
{
	if( _savedScreen != NULL )
	{
		free(_savedScreen);
		_savedScreen = NULL;
	}
	_savedScreenBytes = 0;
	_savedScreenCapacity = 0;
	_valid = false;
}

void UiScreenSnapshot::HideCursor()
{
	union REGS r;
	r.h.ah = 0x01;
	r.x.cx = 0x2607;
	int86(0x10, &r, &r);
}

void UiScreenSnapshot::ShowCursor()
{
	union REGS r;
	r.h.ah = 0x01;
	r.x.cx = 0x0607;
	int86(0x10, &r, &r);
}

void UiScreenSnapshot::Capture(uint8_t far *screen, uint8_t cols, uint8_t rows)
{
	if( !screen )
		return;

	union REGS r;
	r.h.ah = 0x03;
	r.h.bh = 0x00;
	int86(0x10, &r, &r);

	_savedCursorStart = r.h.ch;
	_savedCursorEnd = r.h.cl;
	_savedCursorX = r.h.dl;
	_savedCursorY = r.h.dh;

	uint32_t bytes32 = (uint32_t)rows * (uint32_t)cols * 2ul;
	if( bytes32 > 65535ul )
		bytes32 = 65535ul;

	uint16_t bytes = (uint16_t)bytes32;
	if( bytes > _savedScreenCapacity )
	{
		uint8_t *newBuf = (uint8_t *)realloc(_savedScreen, bytes);
		if( newBuf == NULL )
		{
			_valid = false;
			_savedScreenBytes = 0;
			return;
		}

		_savedScreen = newBuf;
		_savedScreenCapacity = bytes;
	}

	_savedScreenBytes = bytes;
	for( uint16_t i = 0; i < _savedScreenBytes; i++ )
		_savedScreen[i] = screen[i];

	_valid = true;
}

void UiScreenSnapshot::Restore(uint8_t far *screen)
{
	if( !_valid || !screen || _savedScreen == NULL )
	{
		ShowCursor();
		return;
	}

	for( uint16_t i = 0; i < _savedScreenBytes; i++ )
		screen[i] = _savedScreen[i];

	union REGS r;
	r.h.ah = 0x01;
	r.h.ch = _savedCursorStart;
	r.h.cl = _savedCursorEnd;
	int86(0x10, &r, &r);

	r.h.ah = 0x02;
	r.h.bh = 0x00;
	r.h.dh = _savedCursorY;
	r.h.dl = _savedCursorX;
	int86(0x10, &r, &r);

	_valid = false;
}

/* ------------------------------------------------------------------------- */

class UiApp
{
public:
	UiApp();

	bool InitScreen();
	void ClearScreen();
	void Cleanup();

	void SetStatusText(char type, const char *fmt, ...);

	void Loop();

private:
	bool _activePanel;
	bool _screenReady;
	bool _needRedraw;
	char _lastStatus[160];
	char _copyPolicy[5];
	UiScreenSnapshot _screenSnapshot;
	

	void Redraw();

	bool IsRemoteActive() const { return !_activePanel; }
	UiList *GetActiveList() const { return IsRemoteActive() ? &g_RemoteList : &g_LocalList; }

	bool MoveActiveSelection(int delta);

	uint16_t ReadKey();
	void FlushKeys();

	void PrintStatus();
	bool AskYesNo(const char *fmt, ...);
	bool AskString(char *buf, size_t bufSize, const char *title, const char *initialValue);
	void SwitchCopyPolicy();
	const char *GetCopyPolicy() const { return _copyPolicy; }

	bool IsPolicy(const char *name) const;
	bool ConfirmOverwrite(bool remote, const char *name);

	bool PanelHasName(bool remote, const char *name) const;
	void DoGet();
	void DoPut();
	void CopyCurrentItem();

	bool RefreshPanel(bool remote);
	void EnterCurrentItem();
	void DeleteCurrentItem();
	void MakeDirInCurrentPanel();
	void RenameCurrentItem();

	void PutChAt(int x, int y, int ch);
	void PutChAtAttr(int x, int y, int ch, uint8_t attr);
	void WritePadAt(int x, int y, const char *s, int w, uint8_t attr=UI_ATTR_NORMAL);
	void WriteFillAt(int x, int y, int count, int ch);
	void WriteAt(int x, int y, const char *s, uint8_t attr=UI_ATTR_NORMAL);
	void FormatFreeSpace(char *dst, size_t dstSize, long bytes);
	void DrawPanel(int x, int y, int w, int h, UiList *lst, bool active);
};

UiApp::UiApp()
{
	_activePanel = false;
	_screenReady = false;
	_needRedraw = true;
	_lastStatus[0] = '\0';
	strcpy(_copyPolicy, "ASK ");
}

bool UiApp::InitScreen()
{
	uint8_t screenMode = *((uint8_t far *)MK_FP(0x40, 0x49));
	if( screenMode == 7 )
		g_Screen = (uint8_t far *)MK_FP(0xb000, 0);
	else
		g_Screen = (uint8_t far *)MK_FP(0xb800, 0);

	if( getEgaMemSize() == 0x10 )
	{
		g_ScreenCols = 80;
		g_ScreenRows = 25;
	}
	else
	{
		g_ScreenCols = *((uint8_t far *)MK_FP(0x40, 0x4A));
		g_ScreenRows = *((uint8_t far *)MK_FP(0x40, 0x84)) + 1;
	}

	if( g_ScreenCols < 80 || g_ScreenRows < 25 )
	{
		printf("Unsupported text mode: %ux%u. Please use 80x25 or larger.\n", g_ScreenCols, g_ScreenRows);
		return false;
	}

	_screenReady = true;
	_screenSnapshot.Capture(g_Screen, g_ScreenCols, g_ScreenRows);
	_screenSnapshot.HideCursor();
	ClearScreen();
	
	return true;
}

void UiApp::ClearScreen()
{
	if( !g_Screen )
		return;

	int cells = g_ScreenRows * g_ScreenCols;
	for( int i = 0; i < cells; i++ )
	{
		g_Screen[i * 2]     = ' ';
		g_Screen[i * 2 + 1] = UI_ATTR_NORMAL;
	}
	gotoxy(1, 1);
}

void UiApp::Cleanup()
{
	_screenSnapshot.Restore(g_Screen);
}

void UiApp::SetStatusText(char type, const char *fmt, ...)
{
	char tmp[160];
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	char *s = tmp;
	while( *s == '\r' || *s == '\n' )
		s++;

	const char *eol = s;
	while( *eol != '\0' && *eol != '\r' && *eol != '\n' )
		eol++;

	while( eol > s && (eol[-1] == ' ' || eol[-1] == '\t') )
		eol--;

	_lastStatus[0] = type;
	CopyDataTrunc(_lastStatus+1, sizeof(_lastStatus)-1, s, (size_t)(eol - s));

	PrintStatus();
}

void UiApp::Loop()
{
	g_LocalList.Invalidate();
	g_RemoteList.Invalidate();

	while( 1 )
	{
		if( g_LocalList.NeedRefresh() )
			RefreshPanel(false);
		
		if( g_RemoteList.NeedRefresh() )
		{
			if( !RefreshPanel(true) )
				SetStatusText('E', "Remote refresh failed");
		}

		if( _needRedraw )
		{
			FlushKeys();
			Redraw();
			_needRedraw = false;
		}
		
		uint16_t key = ReadKey();

		if( key == UI_KEY_CONTROL_CLOSED )
		{
			SetStatusText('W', "Control connection closed");
			break;
		}

		int ascii = KeyAscii(key);

		if( key == UI_KEY_TAB || ascii == 9 )
		{
			_activePanel = !_activePanel;
			_needRedraw = true;
		}
		else if( key == UI_KEY_ENTER || ascii == 13 )
		{
			EnterCurrentItem();
			_needRedraw = true;
		}
		else if( key == UI_KEY_ESC || ascii == 27 )
		{
			FtpTransport::_ctrlBreakDetected = true;
			SetStatusText('W', "Abort requested");
		}
		else if( key == UI_KEY_F9 )
		{
			SwitchCopyPolicy();
			SetStatusText('I', "Copy policy: %s", GetCopyPolicy());
			_needRedraw = true;
		}
		else if( key == UI_KEY_F10 )
		{
			SetStatusText('I', "Exiting...");
			g_ftp.Quit();
			break;
		}
		else if( key == UI_KEY_UP )
		{
			_needRedraw = MoveActiveSelection(-1);
		}
		else if( key == UI_KEY_DOWN )
		{
			_needRedraw = MoveActiveSelection(1);
		}
		else if( key == UI_KEY_PGUP || key == UI_KEY_LEFT )
		{
			_needRedraw = MoveActiveSelection(-(g_ScreenRows - 6));
		}
		else if( key == UI_KEY_PGDN || key == UI_KEY_RIGHT )
		{
			_needRedraw = MoveActiveSelection(g_ScreenRows - 6);
		}
		else if( key == UI_KEY_HOME )
		{
			_needRedraw = MoveActiveSelection(-GetActiveList()->Count());
		}
		else if( key == UI_KEY_END )
		{
			_needRedraw = MoveActiveSelection(GetActiveList()->Count());
		}
		else if( key == UI_KEY_F5 )
		{
			CopyCurrentItem();
			_needRedraw = true;
		}
		else if( key == UI_KEY_F6 )
		{
			RenameCurrentItem();
			_needRedraw = true;
		}
		else if( key == UI_KEY_F7 )
		{
			MakeDirInCurrentPanel();
			_needRedraw = true;
		}
		else if( key == UI_KEY_F8 )
		{
			DeleteCurrentItem();
			_needRedraw = true;
		}
	}
}

void UiApp::Redraw()
{
	int mid = g_ScreenCols / 2;
	int panelH = g_ScreenRows - 2;

	DrawPanel(0, 0, mid, panelH, &g_RemoteList, !_activePanel);
	DrawPanel(mid, 0, g_ScreenCols - mid, panelH, &g_LocalList, _activePanel);
	PrintStatus();

	char footer[160];
	SprintfTrunc(footer, sizeof(footer), "      F5 Copy    F6 Rename    F7 MkDir    F8 Delete    F9 %s    F10 Quit", GetCopyPolicy());
	WritePadAt(0, g_ScreenRows - 1, footer, g_ScreenCols, UI_ATTR_FOOTER_TEXT);
}

bool UiApp::MoveActiveSelection(int delta)
{
	return GetActiveList()->MoveSel(delta);
}

uint16_t UiApp::ReadKey()
{
	while( 1 )
	{
		g_ftp.PollControlSocket(50);
		if( g_ftp.IsControlClosed() )
			return UI_KEY_CONTROL_CLOSED;
		if( biosIsKeyReady() )
			return biosKeyRead();

	#ifdef SLEEP_CALLS
		SLEEP( );
	#endif
	}
}

void UiApp::FlushKeys()
{
	while( biosIsKeyReady() )
		biosKeyRead();
}

void UiApp::PrintStatus()
{
	if( !_screenReady )
		return;
	
	uint8_t attr = UI_ATTR_NORMAL;
	switch( _lastStatus[0] )
	{
	case 'I': attr = UI_ATTR_STATUS_OK; break;
	case 'W': attr = UI_ATTR_STATUS_WARNING; break;
	case 'E': attr = UI_ATTR_STATUS_ERROR; break;
	case 'Q': attr = UI_ATTR_STATUS_QUESTION; break;
	case 'T': attr = UI_ATTR_STATUS_TRANSFER; break;
	}
	WritePadAt(0, g_ScreenRows-2, _lastStatus+1, g_ScreenCols, attr);
}

bool UiApp::AskYesNo(const char *fmt, ...)
{
	char tmp[160];
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	SetStatusText('Q', "%s [Y/N]", tmp);

	while( 1 )
	{
		uint16_t key = ReadKey();
		if( key == UI_KEY_CONTROL_CLOSED )
		{
			SetStatusText('I', "");
			return false;
		}

		char ch = KeyAscii(key);
		if( ch == 'y' || ch == 'Y' )
		{
			SetStatusText('I', "");
			return true;
		}
		if( ch == 'n' || ch == 'N' || ch == 27 )
		{
			SetStatusText('I', "");
			return false;
		}
	}
}

bool UiApp::AskString(char *buf, size_t bufSize, const char *title, const char *initialValue)
{
	if( buf == NULL || bufSize <= 1 )
		return false;

	CopyTrunc(buf, (size_t)bufSize, initialValue ? initialValue : "");
	size_t len = strlen(buf);

	while( 1 )
	{
		SetStatusText('Q', "%s: %s_", title ? title : "Input", buf);

		uint16_t key = ReadKey();
		if( key == UI_KEY_CONTROL_CLOSED )
		{
			SetStatusText('I', "");
			return false;
		}

		int ascii = KeyAscii(key);
		if( key == UI_KEY_ESC || ascii == 27 )
		{
			SetStatusText('I', "");
			return false;
		}
		if( key == UI_KEY_ENTER || ascii == 13 )
		{
			TrimBoth(buf);
			if( buf[0] == '\0' )
			{
				SetStatusText('W', "Empty value not allowed");
				return false;
			}
			return true;
		}
		if( ascii == 8 )
		{
			if( len > 0 )
				buf[--len] = '\0';
			continue;
		}
		if( ascii >= 32 )
		{
			if( len < bufSize - 1 )
			{
				buf[len++] = (char)ascii;
				buf[len] = '\0';
			}
		}
	}
}

void UiApp::SwitchCopyPolicy()
{
	if( strcmp(_copyPolicy, "ASK ") == 0 )
		CopyTrunc(_copyPolicy, sizeof(_copyPolicy), "SKIP");
	else if( strcmp(_copyPolicy, "SKIP") == 0 )
		CopyTrunc(_copyPolicy, sizeof(_copyPolicy), "OVRW");
	else
		CopyTrunc(_copyPolicy, sizeof(_copyPolicy), "ASK ");
}

bool UiApp::IsPolicy(const char *name) const
{
	return name != NULL && strcmp(_copyPolicy, name) == 0;
}

bool UiApp::ConfirmOverwrite(bool remote, const char *name)
{
	if( !PanelHasName(remote, name) )
		return true;

	if( IsPolicy("SKIP") )
	{
		SetStatusText('I', "Skipped");
		return false;
	}

	if( !IsPolicy("ASK ") )
		return true;

	if( AskYesNo("Overwrite %s file '%s'?", remote ? "remote" : "local", name) )
		return true;

	SetStatusText('I', "Skipped");
	return false;
}

bool UiApp::PanelHasName(bool remote, const char *name) const
{
	UiEntry *e = remote ? g_RemoteList.FindByName(name) : g_LocalList.FindByName(name);
	return e != NULL && !e->isParent;
}

void UiApp::DoGet()
{
	UiEntry *e = g_RemoteList.GetCurrent();
	if( !e || e->isDir || e->isParent )
		return;

	char localFile[LOCALFILE_MAX_LEN];
	if( !MakeDosName(localFile, e->name, !g_NoUtf8Arg) )
	{
		SetStatusText('E', "Invalid local DOS name for GET");
		return;
	}

	if( !ConfirmOverwrite(false, localFile) )
		return;

	if( g_ftp.DoGetAndWait(e->name, localFile, e->size) )
		g_LocalList.Invalidate();
}

void UiApp::DoPut()
{
	UiEntry *e = g_LocalList.GetCurrent();
	if( !e || e->isDir || e->isParent )
		return;

	char remoteName[UI_NAME_MAX * 3];
	if( !MakeRemoteName(remoteName, sizeof(remoteName), e->name, true) )
	{
		SetStatusText('E', "Invalid remote name for PUT");
		return;
	}

	if( !ConfirmOverwrite(true, remoteName) )
		return;

	if( g_ftp.DoPutAndWait(remoteName, NULL, e->size) )
		g_RemoteList.Invalidate();
}

void UiApp::CopyCurrentItem()
{
	if( IsRemoteActive() )
		DoGet();
	else
		DoPut();
}

static bool UiReloadRemotePanel()
{
	return g_ftp.ReadCwd() && g_ftp.ListCwd();
}

static bool UiReloadLocalPanel()
{
	return g_local.ReadCwd() && g_local.ListCwd();
}

bool UiApp::RefreshPanel(bool remote)
{
	UiList *list = remote ? &g_RemoteList : &g_LocalList;
	bool (*reloadFunc)() = remote ? UiReloadRemotePanel : UiReloadLocalPanel;

	if( !list->Refresh(reloadFunc) )
		return false;

	list->Validate();
	_needRedraw = true;

	if( list->IsTruncated() )
		SetStatusText('W', "LIST truncated: too many entries");
	return true;
}

void UiApp::EnterCurrentItem()
{
	UiList *list = GetActiveList();
	UiEntry *e = list->GetCurrent();
	if( !e )
		return;

	if( e->isParent )
	{
		list->SetPendingSelectionFromCwdTail();
		bool ok = IsRemoteActive() ? g_ftp.ChangeToParentDir() : g_local.ChangeToParentDir();
		if( ok )
			list->Invalidate();
		else
			list->ClearPendingSelection();
		return;
	}

	if( e->isDir )
	{
		bool ok = IsRemoteActive() ? g_ftp.ChangeWorkingDir(e->name) : g_local.ChangeWorkingDir(e->name);
		if( ok )
		{
			list->ResetSelection();
			list->Invalidate();
		}
	}
}

void UiApp::DeleteCurrentItem()
{
	UiList *list = GetActiveList();
	UiEntry *e = list->GetCurrent();
	if( !e || e->isParent )
		return;

	char namebuf[UI_NAME_MAX + 8];
	list->PrintName(namebuf, sizeof(namebuf), e);
	if( !AskYesNo("Delete %s %s '%s'?", IsRemoteActive() ? "remote" : "local", e->isDir ? "directory" : "file", namebuf) )
		return;

	bool ok;
	if( e->isDir )
		ok = IsRemoteActive() ? g_ftp.RemoveDir(e->name) : g_local.RemoveDir(e->name);
	else
		ok = IsRemoteActive() ? g_ftp.DeleteFile(e->name) : g_local.DeleteFile(e->name);

	if( ok )
	{
		list->SetFallbackSelectionToCurrent();
		list->Invalidate();
		SetStatusText('I', "Deleted: %s", namebuf);
	}
	else
	{
		SetStatusText('E', "Delete failed: %s", namebuf);
	}
}

void UiApp::MakeDirInCurrentPanel()
{
	char name[UI_NAME_MAX];
	if( !AskString(name, sizeof(name), IsRemoteActive() ? "Remote MKD" : "Local MkDir", "") )
		return;

	UiList *list = GetActiveList();
	bool ok = false;
	const char *selName = name;
	char remoteName[UI_NAME_MAX * 3];

	if( IsRemoteActive() )
	{
		if( !MakeRemoteName(remoteName, sizeof(remoteName), name, true) )
		{
			SetStatusText('E', "Invalid remote name");
			return;
		}

		selName = remoteName;
		ok = g_ftp.MakeDir(remoteName);
	}
	else
	{
		ok = g_local.MakeDir(name);
	}

	if( ok )
	{
		list->SetPendingSelection(selName);
		list->Invalidate();
		SetStatusText('I', "MkDir: %s", name);
	}
	else
	{
		SetStatusText('E', "MkDir failed: %s", name);
	}
}

void UiApp::RenameCurrentItem()
{
	UiList *list = GetActiveList();
	UiEntry *e = list->GetCurrent();
	if( !e || e->isParent )
		return;

	char oldName[UI_NAME_MAX];
	if( IsRemoteActive() )
		list->PrintName(oldName, sizeof(oldName), e);
	else
		CopyTrunc(oldName, sizeof(oldName), e->name);

	char newName[UI_NAME_MAX];
	if( !AskString(newName, sizeof(newName), IsRemoteActive() ? "Remote rename to" : "Local rename to", oldName) )
		return;

	bool ok = false;
	const char *selName = newName;
	char remoteNewName[UI_NAME_MAX * 3];

	if( IsRemoteActive() )
	{
		if( !MakeRemoteName(remoteNewName, sizeof(remoteNewName), newName, true) )
		{
			SetStatusText('E', "Invalid remote name");
			return;
		}

		if( strcmp(remoteNewName, e->name) == 0 )
			return;

		selName = remoteNewName;
		ok = g_ftp.RenameItem(e->name, remoteNewName);
	}
	else
	{
		if( stricmp(newName, oldName) == 0 )
			return;

		ok = g_local.RenameItem(e->name, newName);
	}

	if( ok )
	{
		list->SetPendingSelection(selName);
		list->Invalidate();
		SetStatusText('I', "Renamed: %s", newName);
	}
	else
	{
		SetStatusText('E', "Rename failed: %s", oldName);
	}
}

void UiApp::PutChAt(int x, int y, int ch)
{
	PutChAtAttr(x, y, ch, UI_ATTR_NORMAL);
}

void UiApp::PutChAtAttr(int x, int y, int ch, uint8_t attr)
{
	if( !_screenReady || !g_Screen )
		return;
	if( x < 0 || y < 0 || x >= g_ScreenCols || y >= g_ScreenRows )
		return;
	
	uint16_t off = (uint16_t)((y * g_ScreenCols + x) * 2);
	g_Screen[off] = (uint8_t)ch;
	g_Screen[off + 1] = attr;
}

void UiApp::WritePadAt(int x, int y, const char *s, int w, uint8_t attr)
{
	char tmp[160];
	CopyTrunc(tmp, sizeof(tmp), s);
	
	int n = strlen(tmp);
	while( n < w )
		tmp[n++] = ' ';
	tmp[n] = '\0';

	WriteAt(x, y, tmp, attr);
}

void UiApp::WriteFillAt(int x, int y, int count, int ch)
{
	for( int i = 0; i < count; i++ )
		PutChAt(x + i, y, ch);
}

void UiApp::WriteAt(int x, int y, const char *s, uint8_t attr)
{
	if( s == NULL )
		return;

	for( int i = 0; s[i] != 0; i++ )
		PutChAtAttr(x + i, y, s[i], attr);
}


void UiApp::FormatFreeSpace(char *dst, size_t dstSize, long bytes)
{
	if( dst == NULL || dstSize == 0 )
		return;

	if( bytes >= 1024ul * 1024ul )
		SprintfTrunc(dst, (size_t)dstSize, "%luM free", (unsigned long)(bytes / (1024ul * 1024ul)));
	else if( bytes >= 1024ul )
		SprintfTrunc(dst, (size_t)dstSize, "%luK free", (unsigned long)(bytes / 1024ul));
	else
		SprintfTrunc(dst, (size_t)dstSize, "%lu free", (unsigned long)bytes);
}

void UiApp::DrawPanel(int x, int y, int w, int h, UiList *lst, bool active)
{
	char namebuf[UI_NAME_MAX + 8];

	int rows = h - 4;
	lst->EnsureVisible(rows);
	int maxName = w - 1;

	UiEntry *cur = lst->GetCurrent();
	for( int row = 0; row < rows; row++ )
	{
		namebuf[0] = '\0';
		UiEntry *e = lst->GetFromTop(row);
		lst->PrintName(namebuf, maxName, e);

		if( active && e == cur )
			WritePadAt(x + 1, y + 1 + row, namebuf, w - 2, UI_ATTR_SELECTED);
		else
			WritePadAt(x + 1, y + 1 + row, namebuf, w - 2, (e != NULL && e->isDir) ? UI_ATTR_DIRECTORY : UI_ATTR_NORMAL);
	}

	if( rows > 0 && lst->Count() > rows )
	{	
		int thumbSize = (rows * rows) / lst->Count();
		if( thumbSize < 1 )
			thumbSize = 1;
		if( thumbSize > rows )
			thumbSize = rows;
		int thumbPos = (lst->Top() * (rows - thumbSize)) / (lst->Count() - rows);
		for( int i = 0; i < rows; i++ )
		{
			PutChAt(x, y + 1 + i, CH_V);
			PutChAt(x + w - 1, y + 1 + i, (i >= thumbPos && i < thumbPos + thumbSize) ? CH_THUMB : CH_SHADE);
		}
	}
	else
	{
		for( int i = 1; i < h - 1; i++ )
		{
			PutChAt(x, y + i, CH_V);
			PutChAt(x + w - 1, y + i, CH_V);
		}
	}

	PutChAt(x, y, CH_UL);
	PutChAt(x + 1, y, CH_H);
	lst->PrintCwd(namebuf, w - 2);
	size_t len = strlen(namebuf);
	WritePadAt(x + 2, y, namebuf, len, active ? UI_ATTR_SELECTED : UI_ATTR_NORMAL);
	WriteFillAt(x + 2 + len, y, w - len - 3, CH_H);
	PutChAt(x + w - 1, y, CH_UR);

	PutChAt(x, y + h - 3, CH_LT);
	WriteFillAt(x + 1, y + h - 3, w - 2, CH_H);
	PutChAt(x + w - 1, y + h - 3, CH_RT);
	PutChAt(x, y + h - 1, CH_LL);
	//WriteFillAt(x + 1, y + h - 1, w - 2, CH_H);
	PutChAt(x + w - 1, y + h - 1, CH_LR);
	PutChAt(x, y + h - 2, CH_V);
	PutChAt(x + w - 1, y + h - 2, CH_V);

	lst->PrintStats(namebuf, w - 3, cur);
	WritePadAt(x + 2, y + h - 2, namebuf, w - 4);

	memset(namebuf, CH_H, w - 2);
	namebuf[w - 2] = '\0';
	if( lst->GetFreeBytes() >= 0 )
	{
		char freebuf[32];
		FormatFreeSpace(freebuf, sizeof(freebuf), lst->GetFreeBytes());
		len = strlen(freebuf);
		memcpy(namebuf + w - 3 - len, freebuf, len);
	}
	WriteAt(x + 1, y + h - 1, namebuf);
}
