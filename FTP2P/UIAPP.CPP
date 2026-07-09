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
#define UI_KEY_INS      0x5200
#define UI_KEY_GRAY_PLUS  0x4E2B
#define UI_KEY_GRAY_MINUS 0x4A2D
#define UI_KEY_GRAY_STAR  0x372A

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

#define UI_PANEL_MIN_WIDTH 20

#define UI_REDRAW_NONE        0x0000
#define UI_REDRAW_LEFT_PANEL  0x0001
#define UI_REDRAW_RIGHT_PANEL 0x0002
#define UI_REDRAW_PANELS      (UI_REDRAW_LEFT_PANEL | UI_REDRAW_RIGHT_PANEL)
#define UI_REDRAW_STATUS      0x0004
#define UI_REDRAW_FOOTER      0x0008
#define UI_REDRAW_ALL         (UI_REDRAW_PANELS | UI_REDRAW_STATUS | UI_REDRAW_FOOTER)

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

#define UI_KEY_CONTROL_CLOSED 0xFFFFu

class UiApp
{
public:
	UiApp();

	bool InitScreen();
	void Cleanup();

	void SetStatus(char type, const char *s);
	const char *Loop();

private:
	bool _remoteActive;
	bool _panelsRTL;
	int _panelMid;
	int _needRedraw;
	char _lastStatus[160];
	char _copyPolicy[5];
	
	void Redraw(int flags);
	void RequestRedraw(int flags) { _needRedraw |= flags; }
	int PanelRedrawFlag(bool remote) const;
	int ActivePanelRedrawFlag() const { return PanelRedrawFlag(IsRemoteActive()); }

	bool IsRemoteActive() const { return _remoteActive; }
	UiList *GetActiveList() const;

	bool IsShiftPressed() const;
	uint16_t ReadKey();
	void FlushKeys();

	bool MovePanelSplit(int delta);

	void SetStatusF(char type, const char *fmt, ...);
	void SetTransferSummary(int filesOk, int filesTotal, __int64 bytes, uint32_t elapsedMs);
	void PrintStatus();
	bool AskYesNo(const char *fmt, ...);
	bool AskString(char *buf, size_t bufSize, const char *title, const char *initialValue);
	void SwitchCopyPolicy();
	const char *GetCopyPolicy() const { return _copyPolicy; }

	bool IsPolicy(const char *name) const;
	bool ConfirmOverwrite(bool remote, const char *name);

	bool PanelHasName(bool remote, const char *name) const;
	bool IsViewable(const UiEntry *e, bool &html) const;
	void ShowQuickView(const char *title, FileFormatter &fmt);
	void QuickViewCurrentItem();
	void DoGet();
	void DoPut();
	void CopyCurrentItem();
	void CopySelectedItems();
	void DeleteSelectedItems();

	bool RefreshPanel(bool remote);
	void EnterUp();
	void EnterCurrentItem();
	void DeleteCurrentItem();
	void MakeDirInCurrentPanel();
	void RenameCurrentItem();

	void DrawPanel(int x, int y, int w, int h, UiList *list, bool active);
};

UiApp::UiApp()
{
	_remoteActive = true;
	_panelsRTL = false;
	_panelMid = 40;
	_needRedraw = UI_REDRAW_ALL;
	_lastStatus[0] = '\0';
	strcpy(_copyPolicy, "ASK ");
}

bool UiApp::InitScreen()
{
	if( !g_screen.Init() )
		return false;

	_panelMid = g_screen.Cols() / 2;

	g_screen.Capture();
	g_screen.HideCursor();
	g_screen.Clear();
	
	return true;
}

void UiApp::Cleanup()
{
	g_screen.Restore();
}

void UiApp::SetStatus(char type, const char *s)
{
	if( type == 'S' )
	{
		g_screen.PutChAtAttr(0, g_screen.Rows() - 1, s[0], UI_ATTR_NORMAL);
		return;
	}

	if( type == 'C' )
	{
		_lastStatus[0] = 'C';
		_lastStatus[1] = '\0';
		PrintStatus();
		return;
	}

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

void UiApp::SetStatusF(char type, const char *fmt, ...)
{
	char tmp[160];
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	SetStatus(type, tmp);
}

void UiApp::SetTransferSummary(int filesOk, int filesTotal, __int64 bytes, uint32_t elapsedMs)
{
	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), bytes);

	char timebuf[32];
	FormatDuration(timebuf, sizeof(timebuf), elapsedMs);

	uint32_t rate = ComputeRate(bytes, elapsedMs);
	uint32_t rate_frac = ((rate % 1024u) * 10u) / 1024u;

	if( filesTotal == 1 && filesOk == filesTotal )
		SetStatusF('T', "%s transferred in %s (%lu.%1lu KBytes/sec)", sizebuf, timebuf, rate / 1024u, rate_frac);
	else if( filesOk == filesTotal )
		SetStatusF('T', "%d files, %s transferred in %s (%lu.%1lu KBytes/sec)", filesOk, sizebuf, timebuf, rate / 1024u, rate_frac);
	else
		SetStatusF('T', "%d of %d files, %s transferred in %s (%lu.%1lu KBytes/sec)", filesOk, filesTotal, sizebuf, timebuf, rate / 1024u, rate_frac);
}

UiList *UiApp::GetActiveList() const
{
	return _remoteActive ? &g_remoteList : &g_localList;
}

const char *UiApp::Loop()
{
	if( !RefreshPanel(false) || !RefreshPanel(true) )
		return "Error refreshing panel";

	while( true )
	{
		if( _needRedraw != UI_REDRAW_NONE )
		{
			int flags = _needRedraw;
			FlushKeys();
			Redraw(flags);
			_needRedraw = UI_REDRAW_NONE;
		}

		if( g_remoteList.IsNotRefreshed() && RefreshPanel(true) )
		{
			SetStatus('C', "");
			RequestRedraw(PanelRedrawFlag(true));
		}
		
		uint16_t key = ReadKey();

		if( key == UI_KEY_CONTROL_CLOSED )
			return "Control connection closed";

		bool shift = IsShiftPressed();
		int ascii = KeyAscii(key);

		if( key == UI_KEY_TAB || ascii == 9 )
		{
			if( shift )
				_panelsRTL = !_panelsRTL;
			else
				_remoteActive = !_remoteActive;
			RequestRedraw(UI_REDRAW_PANELS);
		}
		else if( ascii == 8 )
		{
			EnterUp();
		}
		else if( key == UI_KEY_ENTER || ascii == 13 )
		{
			EnterCurrentItem();
		}
		else if( key == UI_KEY_ESC || ascii == 27 )
		{
			g_ctrlBreakDetected = true;
			SetStatusF('W', "Abort requested");
		}
		else if( key == UI_KEY_F9 )
		{
			SwitchCopyPolicy();
			SetStatusF('I', "Copy policy: %s", GetCopyPolicy());
			RequestRedraw(UI_REDRAW_FOOTER);
		}
		else if( key == UI_KEY_F10 )
		{
			SetStatusF('I', "Exiting...");
			g_ftp.Shutdown(1500ul);
			return "Bye!";
		}
		else if( key == UI_KEY_UP )
		{
			if( GetActiveList()->MoveSel(-1) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_DOWN )
		{
			if( GetActiveList()->MoveSel(1) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_PGUP || key == UI_KEY_LEFT )
		{
			if( shift && key == UI_KEY_LEFT )
			{
				if( MovePanelSplit(-4) )
					RequestRedraw(UI_REDRAW_PANELS);
			}
			else
			{
				if( GetActiveList()->MoveSel(-(g_screen.Rows() - 6)) )
					RequestRedraw(ActivePanelRedrawFlag());
			}
		}
		else if( key == UI_KEY_PGDN || key == UI_KEY_RIGHT )
		{
			if( shift && key == UI_KEY_RIGHT )
			{
				if( MovePanelSplit(4) )
					RequestRedraw(UI_REDRAW_PANELS);
			}
			else
			{
				if( GetActiveList()->MoveSel(g_screen.Rows() - 6) )
					RequestRedraw(ActivePanelRedrawFlag());
			}
		}
		else if( key == UI_KEY_HOME )
		{
			if( GetActiveList()->MoveSel(-GetActiveList()->Count()) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_END )
		{
			if( GetActiveList()->MoveSel(GetActiveList()->Count()) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_INS )
		{
			if( GetActiveList()->ToggleCurrentSelection(true) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( ascii == ' ' )
		{
			if( GetActiveList()->ToggleCurrentSelection(false) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( ('A' <= ascii && ascii <= 'Z') || ('a' <= ascii && ascii <= 'z') || ('0' <= ascii && ascii <= '9') )
		{
			if( GetActiveList()->SelectByFirstChar(ascii) )
				RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_GRAY_PLUS )
		{
			GetActiveList()->SelectAllFiles();
			RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_GRAY_MINUS )
		{
			GetActiveList()->ClearSelectionMarks();
			RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_GRAY_STAR )
		{
			GetActiveList()->InvertFileSelection();
			RequestRedraw(ActivePanelRedrawFlag());
		}
		else if( key == UI_KEY_F3 )
		{
			QuickViewCurrentItem();
			RequestRedraw(UI_REDRAW_ALL);
		}
		else if( key == UI_KEY_F5 )
		{
			CopyCurrentItem();
		}
		else if( key == UI_KEY_F6 )
		{
			RenameCurrentItem();
		}
		else if( key == UI_KEY_F7 )
		{
			MakeDirInCurrentPanel();
		}
		else if( key == UI_KEY_F8 )
		{
			DeleteCurrentItem();
		}
		//else if( key == UI_KEY_F1 ) // debug
		//{
		//	char buf[80];
		//	ProbeFreeMem(buf, sizeof(buf));
		//	SetStatus('E', buf);
		//}
	}
}

int UiApp::PanelRedrawFlag(bool remote) const
{
	return _panelsRTL != remote ? UI_REDRAW_LEFT_PANEL : UI_REDRAW_RIGHT_PANEL;
}

void UiApp::Redraw(int flags)
{
	int panel_h = g_screen.Rows() - 2;
	UiList *left   = _panelsRTL ? &g_localList  : &g_remoteList;
	UiList *right  = _panelsRTL ? &g_remoteList : &g_localList;

	if( flags & UI_REDRAW_LEFT_PANEL )
		DrawPanel(0, 0, _panelMid, panel_h, left, _panelsRTL != _remoteActive);
	if( flags & UI_REDRAW_RIGHT_PANEL )
		DrawPanel(_panelMid, 0, g_screen.Cols() - _panelMid, panel_h, right, _panelsRTL == _remoteActive);
	if( flags & UI_REDRAW_STATUS )
		PrintStatus();

	if( flags & UI_REDRAW_FOOTER )
	{
		char footer[160];
		SprintfTrunc(footer, sizeof(footer), "   F3 View   F5 Copy   F6 Rename   F7 MkDir   F8 Delete   F9 %s   F10 Quit", GetCopyPolicy());
		g_screen.WritePadAt(0, g_screen.Rows() - 1, footer, g_screen.Cols(), UI_ATTR_FOOTER_TEXT);
	}
}

bool UiApp::MovePanelSplit(int delta)
{
	int old = _panelMid;
	_panelMid += delta;

	if( _panelMid < UI_PANEL_MIN_WIDTH )
		_panelMid = UI_PANEL_MIN_WIDTH;
	if( _panelMid > g_screen.Cols() - UI_PANEL_MIN_WIDTH )
		_panelMid = g_screen.Cols() - UI_PANEL_MIN_WIDTH;

	return old != _panelMid;
}

bool UiApp::IsShiftPressed() const
{
	union REGS r;
	r.h.ah = 0x02;
	int86(0x16, &r, &r);
	return (r.h.al & 0x03) != 0;
}

uint16_t UiApp::ReadKey()
{
	while( true )
	{
		if( biosIsKeyReady() )
			return biosKeyRead();

		g_ftp.PollControlSocket(50);
		if( g_ftp.IsControlClosed() )
			return UI_KEY_CONTROL_CLOSED;

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
	uint8_t attr = UI_ATTR_NORMAL;
	switch( _lastStatus[0] )
	{
	case 'I': attr = UI_ATTR_STATUS_OK; break;
	case 'W': attr = UI_ATTR_STATUS_WARNING; break;
	case 'E': attr = UI_ATTR_STATUS_ERROR; break;
	case 'Q': attr = UI_ATTR_STATUS_QUESTION; break;
	case 'T': attr = UI_ATTR_STATUS_TRANSFER; break;
	}
	g_screen.WritePadAt(0, g_screen.Rows()-2, _lastStatus+1, g_screen.Cols()-1, attr);
}

bool UiApp::AskYesNo(const char *fmt, ...)
{
	char tmp[160];
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	SetStatusF('Q', "%s [Y/N]", tmp);

	while( true )
	{
		uint16_t key = ReadKey();
		if( key == UI_KEY_CONTROL_CLOSED )
		{
			SetStatusF('I', "");
			return false;
		}

		char ch = KeyAscii(key);
		if( ch == 'y' || ch == 'Y' )
		{
			SetStatusF('I', "");
			return true;
		}
		if( ch == 'n' || ch == 'N' || ch == 27 )
		{
			SetStatusF('I', "");
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

	while( true )
	{
		SetStatusF('Q', "%s: %s_", title ? title : "Input", buf);

		uint16_t key = ReadKey();
		if( key == UI_KEY_CONTROL_CLOSED )
		{
			SetStatusF('I', "");
			return false;
		}

		int ascii = KeyAscii(key);
		if( key == UI_KEY_ESC || ascii == 27 )
		{
			SetStatusF('I', "");
			return false;
		}
		if( key == UI_KEY_ENTER || ascii == 13 )
		{
			TrimBoth(buf);
			if( buf[0] == '\0' )
			{
				SetStatusF('W', "Empty value not allowed");
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
		SetStatusF('I', "Skipped");
		return false;
	}

	if( !IsPolicy("ASK ") )
		return true;

	if( AskYesNo("Overwrite %s file '%s'?", remote ? "remote" : "local", name) )
		return true;

	SetStatusF('I', "Skipped");
	return false;
}

bool UiApp::PanelHasName(bool remote, const char *name) const
{
	UiEntry *e = remote ? g_remoteList.FindByName(name) : g_localList.FindByName(name);
	return e != NULL && e->kind != 'p';
}

bool UiApp::IsViewable(const UiEntry *e, bool &html) const
{
	if( e == NULL || e->name[0] == '\0' )
		return false;
	if( e->size == 0 || e->size > 65535ul )
		return false;

	html = false;

	if( stricmp(e->name, "00_INDEX") == 0 || stricmp(e->name, "FILES") == 0 ||
		stricmp(e->name, "INDEX") == 0 || stricmp(e->name, "00README") == 0 )
	{
		return true;
	}

	const char *ext = strrchr(e->name, '.');
	if( ext == NULL || ext[1] == '\0' )
		return false;

	if( stricmp(ext, ".bbs") == 0 || stricmp(ext, ".txt") == 0 ||
		stricmp(ext, ".ion") == 0 || stricmp(ext, ".diz") == 0 ||
		stricmp(ext, ".nfo") == 0 )
	{
		return true;
	}

	if( stricmp(ext, ".html") == 0 || stricmp(ext, ".htm") == 0 )
	{
		html = true;
		return true;
	}
	return false;
}

void UiApp::ShowQuickView(const char *title, FileFormatter &fmt)
{
	if( fmt.GetSize() <= 0 )
	{
		SetStatus('W', "Nothing to see here");
		return;
	}
	SetStatus('C', "");

	g_screen.WritePadAt(0, g_screen.Rows() - 1, "   F3/Esc Exit", g_screen.Cols(), UI_ATTR_FOOTER_TEXT);

	char line[256];
	bool eof = false;
	while( true )
	{
		FlushKeys();

		long pos = fmt.GetPos();
		eof = false;
		for( int row = 0; row < g_screen.Rows() - 1; row++ )
		{
			if( eof )
			{
				g_screen.WritePadAt(0, row, "", g_screen.Cols());
				continue;
			}

			long next = fmt.GetLine(pos, line, sizeof(line));
			g_screen.WritePadAt(0, row, line, g_screen.Cols());

			if( next <= pos )
				eof = true;

			pos = next;
		}

		int percent = (int)((pos * 100L) / fmt.GetSize());
		if( percent > 100 )
			percent = 100;

		SprintfTrunc(line, sizeof(line), " %3d%% ", percent);
		g_screen.WriteAt(g_screen.Cols() - (int)strlen(line) - 1, g_screen.Rows() - 1, line, UI_ATTR_SELECTED);

		uint16_t key = ReadKey();
		if( key == UI_KEY_CONTROL_CLOSED || key == UI_KEY_ESC || key == UI_KEY_F3 || KeyAscii(key) == 27 )
			break;

		switch( key )
		{
		case UI_KEY_HOME:
			fmt.Home();
			continue;

		case UI_KEY_END:
			if( !eof )
				fmt.End();
			continue;

		case UI_KEY_UP:
			fmt.PrevLine();
			continue;

		case UI_KEY_DOWN:
			if( !eof )
				fmt.NextLine();
			continue;

		case UI_KEY_PGUP:
			fmt.PgUp();
			continue;

		case UI_KEY_PGDN:
			if( !eof )
				fmt.PgDn();
			continue;
		}
	}
}

void UiApp::QuickViewCurrentItem()
{
	SetStatus('C', "");

	UiList *list = GetActiveList();
	UiEntry *e = list->GetCurrent();
	if( !e || e->kind == 'p' || e->kind == 'd' )
		return;

	bool html;
	if( !IsViewable(e, html) )
	{
		SetStatusF('W', "View not available: %s", e->name);
		return;
	}

	char src[LOCALFILE_MAX_LEN];
	char tmp[LOCALFILE_MAX_LEN];
	bool cleanup = false;

	CopyTrunc(tmp, sizeof(tmp), "ftp2p.tmp");
	remove(tmp);

	if( IsRemoteActive() )
	{
		CopyTrunc(src, sizeof(src), "ftp2p.$$$");
		remove(src);

		g_ctrlBreakDetected = false;
		if( !g_ftp.DoGetAndWait(e->name, src, e->size) )
			return;

		cleanup = true;
	}
	else
	{
		CopyTrunc(src, sizeof(src), e->name);
	}

	FileFormatter fmt(src, g_screen.Cols(), g_screen.Rows());
	bool ok = fmt.PreprocessFile(html);
	if( cleanup )
		remove(src);

	if( !ok )
	{
		remove(tmp);
		SetStatusF('E', "View failed: %s", e->name);
		return;
	}

	char namebuf[UI_NAME_MAX];
	list->PrintName(namebuf, sizeof(namebuf), e);
	ShowQuickView(namebuf, fmt);

	remove(tmp);
}

void UiApp::DoGet()
{
	SetStatus('C', "");
	g_ctrlBreakDetected = false;

	UiEntry *e = g_remoteList.GetCurrent();
	if( !e || e->kind == 'p' || e->kind == 'd' )
		return;

	char name[LOCALFILE_MAX_LEN];
	if( !MakeDosName(name, e->name) )
	{
		SetStatusF('E', "Invalid local DOS name for GET");
		return;
	}

	if( !ConfirmOverwrite(false, name) )
		return;

	clockTicks_t start = TIMER_GET_CURRENT();
	if( g_ftp.DoGetAndWait(e->name, name, e->size) )
	{
		uint32_t elapsed = Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN;

		RefreshPanel(false);
		SetTransferSummary(1, 1, e->size, elapsed);
	}
}

void UiApp::DoPut()
{
	SetStatus('C', "");
	g_ctrlBreakDetected = false;

	UiEntry *e = g_localList.GetCurrent();
	if( !e || e->kind == 'p' || e->kind == 'd' )
		return;

	char name[UI_NAME_MAX * 3];
	if( !MakeRemoteName(name, sizeof(name), e->name) )
	{
		SetStatusF('E', "Invalid remote name for PUT");
		return;
	}

	if( !ConfirmOverwrite(true, name) )
		return;


	clockTicks_t start = TIMER_GET_CURRENT();
	if( g_ftp.DoPutAndWait(e->name, name, e->size) )
	{
		uint32_t elapsed = Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN;

		RefreshPanel(true);
		SetTransferSummary(1, 1, e->size, elapsed);
	}
}

void UiApp::CopySelectedItems()
{
	SetStatus('C', "");
	g_ctrlBreakDetected = false;

	UiList *list = GetActiveList();
	int selected = list->SelectedFileCount();
	if( selected <= 0 )
		return;

	UiEntry **items = (UiEntry **)malloc((size_t)selected * sizeof(UiEntry *));
	if( items == NULL )
	{
		SetStatusF('E', "Not enough free memory");
		return;
	}

	int count = list->AllSelected(items, selected);
	int ok_count = 0;
	__int64 ok_bytes = 0;
	clockTicks_t start = TIMER_GET_CURRENT();

	for( int i = 0; i < count; i++ )
	{
		if( g_ctrlBreakDetected )
			break;

		UiEntry *e = items[i];
		bool ok = false;

		if( IsRemoteActive() )
		{
			char name[LOCALFILE_MAX_LEN];
			if( MakeDosName(name, e->name) && ConfirmOverwrite(false, name) )
				ok = g_ftp.DoGetAndWait(e->name, name, e->size);
		}
		else
		{
			char name[UI_NAME_MAX * 3];
			if( MakeRemoteName(name, sizeof(name), e->name) && ConfirmOverwrite(true, name) )
				ok = g_ftp.DoPutAndWait(e->name, name, e->size);
		}

		if( ok )
		{
			if( e->kind == 'F' )
				e->kind = 'f';
			ok_count++;
			ok_bytes += e->size;
		}
	}
	uint32_t elapsed = Timer_diff(start, TIMER_GET_CURRENT()) * TIMER_TICK_LEN;

	free(items);

	if( ok_count > 0 )
		RefreshPanel(!IsRemoteActive());

	SetTransferSummary(ok_count, count, ok_bytes, elapsed);
}

void UiApp::CopyCurrentItem()
{
	if( GetActiveList()->SelectedFileCount() > 0 )
	{
		CopySelectedItems();
		return;
	}

	if( IsRemoteActive() )
		DoGet();
	else
		DoPut();
}

static bool UiReloadRemotePanel()
{
	return g_ftp.RefreshList();
}

static bool UiReloadLocalPanel()
{
	return g_local.RefreshList();
}

bool UiApp::RefreshPanel(bool remote)
{
	UiList *list = remote ? &g_remoteList : &g_localList;
	bool (*reloadFunc)() = remote ? UiReloadRemotePanel : UiReloadLocalPanel;

	list->Refresh(reloadFunc);

	if( list->IsNotRefreshed() )
		return false;

	RequestRedraw(PanelRedrawFlag(remote));

	if( list->IsTruncated() )
		SetStatusF('W', "LIST truncated (%d): too many entries", list->Count());

	return true;
}

void UiApp::EnterUp()
{
	SetStatus('C', "");

	UiList *list = GetActiveList();
	list->SetPendingSelectionFromCwdTail();
	bool ok = IsRemoteActive() ? g_ftp.ChangeToParentDir() : g_local.ChangeToParentDir();
	if( ok )
		RefreshPanel(IsRemoteActive());
	else
	{
		list->ClearPendingSelection();
		SetStatusF('E', "%s cd .. failed", IsRemoteActive() ? "Remote" : "Local");
	}
}

void UiApp::EnterCurrentItem()
{
	UiList *list = GetActiveList();
	UiEntry *e = list->GetCurrent();
	if( !e )
		return;

	if( e->kind == 'p' )
	{
		EnterUp();
		return;
	}

	if( e->kind == 'd' )
	{
		SetStatus('C', "");

		bool ok = IsRemoteActive() ? g_ftp.ChangeWorkingDir(e->name) : g_local.ChangeWorkingDir(e->name);
		if( ok )
		{
			list->ResetSelection();
			RefreshPanel(IsRemoteActive());
		}
		else
		{
			char namebuf[UI_NAME_MAX];
			list->PrintName(namebuf, sizeof(namebuf), e);
			SetStatusF('E', "%s enter failed: %s", IsRemoteActive() ? "Remote" : "Local", namebuf);
		}
	}
}

void UiApp::DeleteSelectedItems()
{
	SetStatus('C', "");
	g_ctrlBreakDetected = false;

	UiList *list = GetActiveList();
	int selected = list->SelectedFileCount();
	if( selected <= 0 )
		return;

	if( !AskYesNo("Delete %d selected files?", selected) )
		return;

	UiEntry **items = (UiEntry **)malloc((size_t)selected * sizeof(UiEntry *));
	if( items == NULL )
	{
		SetStatusF('E', "Not enough free memory");
		return;
	}

	int count = list->AllSelected(items, selected);
	int ok_count = 0;

	for( int i = 0; i < count; i++ )
	{
		if( g_ctrlBreakDetected )
			break;

		UiEntry *e = items[i];
		bool ok = IsRemoteActive() ? g_ftp.DeleteFile(e->name) : g_local.DeleteFile(e->name);
		if( ok )
		{
			if( e->kind == 'F' )
				e->kind = 'f';
			ok_count++;
		}
	}

	free(items);

	if( ok_count > 0 )
		RefreshPanel(IsRemoteActive());
	
	SetStatusF('I', "Deleted %d/%d files", ok_count, count);
}

void UiApp::DeleteCurrentItem()
{
	SetStatus('C', "");
	g_ctrlBreakDetected = false;

	UiList *list = GetActiveList();
	if( list->SelectedFileCount() > 0 )
	{
		DeleteSelectedItems();
		return;
	}

	UiEntry *e = list->GetCurrent();
	if( !e || e->kind == 'p' )
		return;

	char namebuf[UI_NAME_MAX];
	list->PrintName(namebuf, sizeof(namebuf), e);
	if( !AskYesNo("Delete %s %s '%s'?", IsRemoteActive() ? "remote" : "local", e->kind == 'd' ? "directory" : "file", namebuf) )
		return;

	bool ok;
	if( e->kind == 'd' )
		ok = IsRemoteActive() ? g_ftp.RemoveDir(e->name) : g_local.RemoveDir(e->name);
	else
		ok = IsRemoteActive() ? g_ftp.DeleteFile(e->name) : g_local.DeleteFile(e->name);

	if( !ok )
	{
		if( g_ctrlBreakDetected )
			SetStatusF('W', "Delete cancelled: %s", namebuf);
		else
			SetStatusF('E', "Delete failed: %s", namebuf);

		return;
	}

	list->SetFallbackSelectionToCurrent();
	
	RefreshPanel(IsRemoteActive());
	SetStatusF('I', "Deleted: %s", namebuf);
}

void UiApp::MakeDirInCurrentPanel()
{
	SetStatus('C', "");

	char name[UI_NAME_MAX];
	if( !AskString(name, sizeof(name), IsRemoteActive() ? "Remote MKD" : "Local MkDir", "") )
		return;

	bool ok = false;
	const char *sel_name = name;
	char remote_name[UI_NAME_MAX * 3];

	if( IsRemoteActive() )
	{
		if( !MakeRemoteName(remote_name, sizeof(remote_name), name) )
		{
			SetStatusF('E', "Invalid remote name");
			return;
		}

		sel_name = remote_name;
		ok = g_ftp.MakeDir(remote_name);
	}
	else
	{
		ok = g_local.MakeDir(name);
	}

	if( !ok )
	{
		SetStatusF('E', "MkDir failed: %s", name);
		return;
	}

	UiList *list = GetActiveList();
	list->SetPendingSelection(sel_name);
	
	RefreshPanel(IsRemoteActive());
	SetStatusF('I', "MkDir: %s", name);
}

void UiApp::RenameCurrentItem()
{
	SetStatus('C', "");

	UiList *list = GetActiveList();
	if( list->SelectedFileCount() != 0 )
	{
		SetStatusF('W', "Clear selection before rename");
		return;
	}

	UiEntry *e = list->GetCurrent();
	if( !e || e->kind == 'p' )
		return;

	char old_name[UI_NAME_MAX];
	if( IsRemoteActive() )
		list->PrintName(old_name, sizeof(old_name), e);
	else
		CopyTrunc(old_name, sizeof(old_name), e->name);

	char new_name[UI_NAME_MAX];
	if( !AskString(new_name, sizeof(new_name), IsRemoteActive() ? "Remote rename to" : "Local rename to", old_name) )
		return;

	bool ok = false;
	const char *sel_name = new_name;
	char remote_new_name[UI_NAME_MAX * 3];

	if( IsRemoteActive() )
	{
		if( !MakeRemoteName(remote_new_name, sizeof(remote_new_name), new_name) )
		{
			SetStatusF('E', "Invalid remote name");
			return;
		}

		if( strcmp(remote_new_name, e->name) == 0 )
			return;

		sel_name = remote_new_name;
		ok = g_ftp.RenameItem(e->name, remote_new_name);
	}
	else
	{
		if( stricmp(new_name, old_name) == 0 )
			return;

		ok = g_local.RenameItem(e->name, new_name);
	}

	if( !ok )
	{
		SetStatusF('E', "Rename failed: %s", old_name);
		return;
	}

	list->SetPendingSelection(sel_name);
	
	RefreshPanel(IsRemoteActive());
	SetStatusF('I', "Renamed: %s", new_name);
}

void UiApp::DrawPanel(int x, int y, int w, int h, UiList *list, bool active)
{
	if( list == NULL || w < UI_PANEL_MIN_WIDTH || h < 4 )
		return;

	char namebuf[UI_NAME_MAX];

	int rows = h - 4;
	list->EnsureVisible(rows);
	int maxName = w - 1;

	UiEntry *cur = list->GetCurrent();
	for( int row = 0; row < rows; row++ )
	{
		UiEntry *e = list->GetFromTop(row);

		namebuf[0] = '\0';
		list->PrintName(namebuf, maxName, e);

		bool is_current = active && e == cur;
		bool is_marked = e != NULL && e->kind == 'F';
		uint8_t attr;
		if( is_current && is_marked )
			attr = UI_ATTR_MARKED_CURRENT;
		else if( is_current )
			attr = UI_ATTR_SELECTED;
		else if( is_marked )
			attr = UI_ATTR_MARKED;
		else
			attr = e != NULL && e->kind == 'd' ? UI_ATTR_DIRECTORY : UI_ATTR_NORMAL;

		g_screen.WritePadAt(x + 1, y + 1 + row, namebuf, w - 2, attr);
	}

	if( rows > 0 && list->Count() > rows )
	{	
		int thumb_size = (rows * rows) / list->Count();
		if( thumb_size < 1 )
			thumb_size = 1;
		if( thumb_size > rows )
			thumb_size = rows;
		int thumb_pos = (list->Top() * (rows - thumb_size)) / (list->Count() - rows);
		for( int i = 0; i < rows; i++ )
		{
			g_screen.PutChAt(x, y + 1 + i, CH_V);
			g_screen.PutChAt(x + w - 1, y + 1 + i, (i >= thumb_pos && i < thumb_pos + thumb_size) ? CH_THUMB : CH_SHADE);
		}
	}
	else
	{
		for( int i = 1; i < h - 1; i++ )
		{
			g_screen.PutChAt(x, y + i, CH_V);
			g_screen.PutChAt(x + w - 1, y + i, CH_V);
		}
	}

	g_screen.PutChAt(x, y, CH_UL);
	g_screen.PutChAt(x + 1, y, CH_H);
	list->PrintCwd(namebuf, w - 2);
	size_t len = strlen(namebuf);
	g_screen.WritePadAt(x + 2, y, namebuf, len, active ? UI_ATTR_SELECTED : UI_ATTR_NORMAL);
	g_screen.PutCharsAt(x + 2 + len, y, CH_H, w - len - 3);
	g_screen.PutChAt(x + w - 1, y, CH_UR);

	g_screen.PutChAt(x, y + h - 3, CH_LT);
	g_screen.PutCharsAt(x + 1, y + h - 3, CH_H, w - 2);
	g_screen.PutChAt(x + w - 1, y + h - 3, CH_RT);
	g_screen.PutChAt(x, y + h - 1, CH_LL);
	g_screen.PutChAt(x + w - 1, y + h - 1, CH_LR);
	g_screen.PutChAt(x, y + h - 2, CH_V);
	g_screen.PutChAt(x + w - 1, y + h - 2, CH_V);

	g_screen.PutChAt(x + 1, y + h - 2, ' ');
	list->PrintStats(namebuf, w - 3, cur);
	g_screen.WritePadAt(x + 2, y + h - 2, namebuf, w - 3);

	memset(namebuf, CH_H, w - 2);
	namebuf[w - 2] = '\0';

	char selbuf[32];
	list->FormatSelectedInfo(selbuf, sizeof(selbuf));
	len = strlen(selbuf);
	if( len > w - 2 )
		len = w - 2;
	if( len > 0 )
		memcpy(namebuf, selbuf, len);

	char freebuf[32];
	list->FormatListInfo(freebuf, sizeof(freebuf));
	len = strlen(freebuf);
	if( len > w - 2 )
		len = w - 2;
	if( len > 0 )
		memcpy(namebuf + w - 2 - len, freebuf, len);
	g_screen.WriteAt(x + 1, y + h - 1, namebuf);
}
