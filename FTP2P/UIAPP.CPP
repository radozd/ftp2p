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
	void Loop();

private:
	bool _remoteActive;
	bool _panelsRTL;
	int _panelMid;
	bool _needRedraw;
	char _lastStatus[160];
	char _copyPolicy[5];
	
	void Redraw();

	bool IsRemoteActive() const { return _remoteActive; }
	UiList *GetActiveList() const;

	bool IsShiftPressed() const;
	uint16_t ReadKey();
	void FlushKeys();

	bool MovePanelSplit(int delta);

	void SetStatusF(char type, const char *fmt, ...);
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

	void FormatSelectedStats(char *dst, size_t dstSize, UiList *lst);
	void FormatFreeSpace(char *dst, size_t dstSize, UiList *lst);
	void DrawPanel(int x, int y, int w, int h, UiList *lst, bool active);
};

UiApp::UiApp()
{
	_remoteActive = true;
	_panelsRTL = false;
	_panelMid = 40;
	_needRedraw = true;
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

UiList *UiApp::GetActiveList() const
{
	return _remoteActive ? &g_remoteList : &g_localList;
}

void UiApp::Loop()
{
	g_localList.Invalidate();
	g_remoteList.Invalidate();

	while( true )
	{
		if( g_localList.NeedRefresh() )
		{
			if( !RefreshPanel(false) )
				SetStatusF('E', "Local refresh failed");
		}
		
		if( g_remoteList.NeedRefresh() )
		{
			if( !RefreshPanel(true) )
				SetStatusF('E', "Remote refresh failed");
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
			SetStatusF('W', "Control connection closed");
			break;
		}

		bool shift = IsShiftPressed();
		int ascii = KeyAscii(key);

		if( key == UI_KEY_TAB || ascii == 9 )
		{
			if( shift )
				_panelsRTL = !_panelsRTL;
			else
				_remoteActive = !_remoteActive;
			_needRedraw = true;
		}
		else if( ascii == 8 )
		{
			EnterUp();
			_needRedraw = true;
		}
		else if( key == UI_KEY_ENTER || ascii == 13 )
		{
			EnterCurrentItem();
			_needRedraw = true;
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
			_needRedraw = true;
		}
		else if( key == UI_KEY_F10 )
		{
			SetStatusF('I', "Exiting...");
			g_ftp.Shutdown(1500ul);
			break;
		}
		else if( key == UI_KEY_UP )
		{
			_needRedraw = GetActiveList()->MoveSel(-1);
		}
		else if( key == UI_KEY_DOWN )
		{
			_needRedraw = GetActiveList()->MoveSel(1);
		}
		else if( key == UI_KEY_PGUP || key == UI_KEY_LEFT )
		{
			if( shift && key == UI_KEY_LEFT )
				_needRedraw = MovePanelSplit(-4);
			else
				_needRedraw = GetActiveList()->MoveSel(-(g_screen.Rows() - 6));
		}
		else if( key == UI_KEY_PGDN || key == UI_KEY_RIGHT )
		{
			if( shift && key == UI_KEY_RIGHT )
				_needRedraw = MovePanelSplit(4);
			else
				_needRedraw = GetActiveList()->MoveSel(g_screen.Rows() - 6);
		}
		else if( key == UI_KEY_HOME )
		{
			_needRedraw = GetActiveList()->MoveSel(-GetActiveList()->Count());
		}
		else if( key == UI_KEY_END )
		{
			_needRedraw = GetActiveList()->MoveSel(GetActiveList()->Count());
		}
		else if( key == UI_KEY_INS )
		{
			_needRedraw = GetActiveList()->ToggleCurrentSelection(true);
		}
		else if( ascii == ' ' )
		{
			_needRedraw = GetActiveList()->ToggleCurrentSelection(false);
		}
		else if( ('A' <= ascii && ascii <= 'Z') || ('a' <= ascii && ascii <= 'z') || ('0' <= ascii && ascii <= '9') )
		{
			_needRedraw = GetActiveList()->SelectByFirstChar(ascii);
		}
		else if( key == UI_KEY_GRAY_PLUS )
		{
			GetActiveList()->SelectAllFiles();
			_needRedraw = true;
		}
		else if( key == UI_KEY_GRAY_MINUS )
		{
			GetActiveList()->ClearSelectionMarks();
			_needRedraw = true;
		}
		else if( key == UI_KEY_GRAY_STAR )
		{
			GetActiveList()->InvertFileSelection();
			_needRedraw = true;
		}
		else if( key == UI_KEY_F3 )
		{
			QuickViewCurrentItem();
			_needRedraw = true;
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
		//else if( key == UI_KEY_F1 ) // debug
		//{
		//	char buf[80];
		//	ProbeFreeMem(buf, sizeof(buf));
		//	SetStatus('E', buf);
		//}
	}
}

void UiApp::Redraw()
{
	int panel_h = g_screen.Rows() - 2;
	UiList *left   = _panelsRTL ? &g_localList  : &g_remoteList;
	UiList *right  = _panelsRTL ? &g_remoteList : &g_localList;

	DrawPanel(0, 0, _panelMid, panel_h, left, _panelsRTL != _remoteActive);
	DrawPanel(_panelMid, 0, g_screen.Cols() - _panelMid, panel_h, right, _panelsRTL == _remoteActive);
	PrintStatus();

	char footer[160];
	SprintfTrunc(footer, sizeof(footer), "   F3 View   F5 Copy   F6 Rename   F7 MkDir   F8 Delete   F9 %s   F10 Quit", GetCopyPolicy());
	g_screen.WritePadAt(0, g_screen.Rows() - 1, footer, g_screen.Cols(), UI_ATTR_FOOTER_TEXT);
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
	SetStatus('I', "");

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

	char namebuf[UI_NAME_MAX + 8];
	list->PrintName(namebuf, sizeof(namebuf), e);
	ShowQuickView(namebuf, fmt);

	remove(tmp);
}

void UiApp::DoGet()
{
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

	if( g_ftp.DoGetAndWait(e->name, name, e->size) )
		g_localList.Invalidate();
}

void UiApp::DoPut()
{
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

	if( g_ftp.DoPutAndWait(e->name, name, e->size) )
		g_remoteList.Invalidate();
}

void UiApp::CopySelectedItems()
{
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
	uint32_t ok_bytes = 0;

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

	free(items);

	if( ok_count > 0 )
	{
		if( IsRemoteActive() )
			g_localList.Invalidate();
		else
			g_remoteList.Invalidate();
	}

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), ok_bytes);
	if( g_ctrlBreakDetected )
		SetStatusF('W', "Copy cancelled: %d/%d files (%s)", ok_count, count, sizebuf);
	else
		SetStatusF(ok_count == count ? 'I' : 'W', "Copied %d/%d files (%s)", ok_count, count, sizebuf);
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
	return g_ftp.ReadCwd() && g_ftp.ListCwd();
}

static bool UiReloadLocalPanel()
{
	return g_local.ReadCwd() && g_local.ListCwd();
}

bool UiApp::RefreshPanel(bool remote)
{
	UiList *list = remote ? &g_remoteList : &g_localList;
	bool (*reloadFunc)() = remote ? UiReloadRemotePanel : UiReloadLocalPanel;

	if( !list->Refresh(reloadFunc) )
		return false;

	list->Validate();
	_needRedraw = true;

	if( list->IsTruncated() )
		SetStatusF('W', "LIST truncated (%d): too many entries", list->Count());
	return true;
}

void UiApp::EnterUp()
{
	UiList *list = GetActiveList();
	list->SetPendingSelectionFromCwdTail();
	bool ok = IsRemoteActive() ? g_ftp.ChangeToParentDir() : g_local.ChangeToParentDir();
	if( ok )
		list->Invalidate();
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
		bool ok = IsRemoteActive() ? g_ftp.ChangeWorkingDir(e->name) : g_local.ChangeWorkingDir(e->name);
		if( ok )
		{
			list->ResetSelection();
			list->Invalidate();
		}
		else
		{
			char namebuf[UI_NAME_MAX + 8];
			list->PrintName(namebuf, sizeof(namebuf), e);
			SetStatusF('E', "%s enter failed: %s", IsRemoteActive() ? "Remote" : "Local", namebuf);
		}
	}
}

void UiApp::DeleteSelectedItems()
{
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
	uint32_t ok_bytes = 0;

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
			ok_bytes += e->size;
		}
	}

	free(items);

	if( ok_count > 0 )
		list->Invalidate();

	if( g_ctrlBreakDetected )
		SetStatusF('W', "Delete cancelled: %d/%d files (%lu bytes)", ok_count, count, (unsigned long)ok_bytes);
	else
		SetStatusF(ok_count == count ? 'I' : 'W', "Deleted %d/%d files (%lu bytes)", ok_count, count, (unsigned long)ok_bytes);
}

void UiApp::DeleteCurrentItem()
{
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

	char namebuf[UI_NAME_MAX + 8];
	list->PrintName(namebuf, sizeof(namebuf), e);
	if( !AskYesNo("Delete %s %s '%s'?", IsRemoteActive() ? "remote" : "local", e->kind == 'd' ? "directory" : "file", namebuf) )
		return;

	bool ok;
	if( e->kind == 'd' )
		ok = IsRemoteActive() ? g_ftp.RemoveDir(e->name) : g_local.RemoveDir(e->name);
	else
		ok = IsRemoteActive() ? g_ftp.DeleteFile(e->name) : g_local.DeleteFile(e->name);

	if( ok )
	{
		list->SetFallbackSelectionToCurrent();
		list->Invalidate();
		SetStatusF('I', "Deleted: %s", namebuf);
	}
	else
	{
		if( g_ctrlBreakDetected )
			SetStatusF('W', "Delete cancelled: %s", namebuf);
		else
			SetStatusF('E', "Delete failed: %s", namebuf);
	}
}

void UiApp::MakeDirInCurrentPanel()
{
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

	if( ok )
	{
		UiList *list = GetActiveList();
		list->SetPendingSelection(sel_name);
		list->Invalidate();
		SetStatusF('I', "MkDir: %s", name);
	}
	else
	{
		SetStatusF('E', "MkDir failed: %s", name);
	}
}

void UiApp::RenameCurrentItem()
{
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

	if( ok )
	{
		list->SetPendingSelection(sel_name);
		list->Invalidate();
		SetStatusF('I', "Renamed: %s", new_name);
	}
	else
	{
		SetStatusF('E', "Rename failed: %s", old_name);
	}
}

void UiApp::FormatSelectedStats(char *dst, size_t dstSize, UiList *lst)
{
	if( dst == NULL || dstSize == 0 )
		return;

	dst[0] = '\0';
	if( lst == NULL )
		return;

	int count = lst->SelectedFileCount();
	if( count <= 0 )
		return;

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), lst->SelectedFileBytes());

	SprintfTrunc(dst, dstSize, " %d / %s ", count, sizebuf);
}

void UiApp::FormatFreeSpace(char *dst, size_t dstSize, UiList *lst)
{
	if( dst == NULL || dstSize == 0 )
		return;

	dst[0] = '\0';
	if( lst == NULL )
		return;

	if( lst->GetFreeBytes() < 0 )
		return;

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), lst->GetFreeBytes());

	SprintfTrunc(dst, dstSize, " %s free ", sizebuf);
}

void UiApp::DrawPanel(int x, int y, int w, int h, UiList *lst, bool active)
{
	if( lst == NULL || w < UI_PANEL_MIN_WIDTH || h < 4 )
		return;

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

	if( rows > 0 && lst->Count() > rows )
	{	
		int thumb_size = (rows * rows) / lst->Count();
		if( thumb_size < 1 )
			thumb_size = 1;
		if( thumb_size > rows )
			thumb_size = rows;
		int thumb_pos = (lst->Top() * (rows - thumb_size)) / (lst->Count() - rows);
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
	lst->PrintCwd(namebuf, w - 2);
	size_t len = strlen(namebuf);
	g_screen.WritePadAt(x + 2, y, namebuf, len, active ? UI_ATTR_SELECTED : UI_ATTR_NORMAL);
	g_screen.WriteFillAt(x + 2 + len, y, w - len - 3, CH_H);
	g_screen.PutChAt(x + w - 1, y, CH_UR);

	g_screen.PutChAt(x, y + h - 3, CH_LT);
	g_screen.WriteFillAt(x + 1, y + h - 3, w - 2, CH_H);
	g_screen.PutChAt(x + w - 1, y + h - 3, CH_RT);
	g_screen.PutChAt(x, y + h - 1, CH_LL);
	//g_screen.WriteFillAt(x + 1, y + h - 1, w - 2, CH_H);
	g_screen.PutChAt(x + w - 1, y + h - 1, CH_LR);
	g_screen.PutChAt(x, y + h - 2, CH_V);
	g_screen.PutChAt(x + w - 1, y + h - 2, CH_V);

	g_screen.WriteFillAt(x + 1, y + h - 2, w - 2, ' ');
	lst->PrintStats(namebuf, w - 3, cur);
	g_screen.WritePadAt(x + 2, y + h - 2, namebuf, w - 4);

	memset(namebuf, CH_H, w - 2);
	namebuf[w - 2] = '\0';

	char selbuf[32];
	FormatSelectedStats(selbuf, sizeof(selbuf), lst);
	len = strlen(selbuf);
	if( len > w - 2 )
		len = w - 2;
	if( len > 0 )
		memcpy(namebuf, selbuf, len);

	char freebuf[32];
	FormatFreeSpace(freebuf, sizeof(freebuf), lst);
	len = strlen(freebuf);
	if( len > w - 2 )
		len = w - 2;
	if( len > 0 )
		memcpy(namebuf + w - 2 - len, freebuf, len);
	g_screen.WriteAt(x + 1, y + h - 1, namebuf);
}
