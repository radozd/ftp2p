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


enum {
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
};

#define UI_ATTR(fg, bg) (((bg) << 4) | (fg))

#define UI_ATTR_NORMAL            UI_ATTR(EGA_LIGHT_GRAY, EGA_BLACK)
#define UI_ATTR_SELECTED          UI_ATTR(EGA_BLACK, EGA_LIGHT_GRAY)
#define UI_ATTR_DIRECTORY         UI_ATTR(EGA_WHITE, EGA_BLACK)
#define UI_ATTR_MARKED            UI_ATTR(EGA_YELLOW, EGA_BLACK)
#define UI_ATTR_MARKED_CURRENT    UI_ATTR(EGA_YELLOW, EGA_LIGHT_GRAY)
#define UI_ATTR_FOOTER_TEXT       UI_ATTR(EGA_BLACK, EGA_LIGHT_GRAY)

#define UI_ATTR_STATUS_OK         UI_ATTR(EGA_LIGHT_GRAY, EGA_BLACK)
#define UI_ATTR_STATUS_WARNING    UI_ATTR(EGA_YELLOW, EGA_BLACK)
#define UI_ATTR_STATUS_ERROR      UI_ATTR(EGA_LIGHT_RED, EGA_BLACK)
#define UI_ATTR_STATUS_QUESTION   UI_ATTR(EGA_LIGHT_GREEN, EGA_BLACK)
#define UI_ATTR_STATUS_TRANSFER   UI_ATTR(EGA_CYAN, EGA_BLACK)

class UiScreen
{
public:
	UiScreen();
	~UiScreen();

	bool Init();
	void Clear();
	void HideCursor();
	void ShowCursor();
	void Capture();
	void Restore();

	bool IsReady() const { return _screen != NULL; }
	uint16_t Rows() const { return _screenRows; }
	uint16_t Cols() const { return _screenCols; }

	void PutChAt(int x, int y, int ch);
	void PutChAtAttr(int x, int y, int ch, uint8_t attr);
	void WritePadAt(int x, int y, const char *s, int w, uint8_t attr=UI_ATTR_NORMAL);
	void WriteFillAt(int x, int y, int count, int ch);
	void WriteAt(int x, int y, const char *s, uint8_t attr=UI_ATTR_NORMAL);

private:
	uint8_t far *_screen;
	uint16_t _screenRows;
	uint16_t _screenCols;

	uint16_t _savedScreenBytes;
	uint16_t _savedScreenCapacity;
	uint8_t _savedCursorStart;
	uint8_t _savedCursorEnd;
	uint8_t _savedCursorX;
	uint8_t _savedCursorY;
	uint8_t *_savedScreen;
	bool _valid;
};

UiScreen _screenSnapshot;

UiScreen::UiScreen()
{
	_screen = NULL;
	_screenRows = 25;
	_screenCols = 80;

	_savedScreenBytes = 0;
	_savedScreenCapacity = 0;
	_savedCursorStart = 6;
	_savedCursorEnd = 7;
	_savedCursorX = 0;
	_savedCursorY = 0;
	_savedScreen = NULL;
	_valid = false;
}

UiScreen::~UiScreen()
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

bool UiScreen::Init()
{
	uint8_t mode = *((uint8_t far *)MK_FP(0x40, 0x49));
	if( mode == 7 )
		_screen = (uint8_t far *)MK_FP(0xb000, 0);
	else
		_screen = (uint8_t far *)MK_FP(0xb800, 0);

	if( getEgaMemSize() == 0x10 )
	{
		_screenCols = 80;
		_screenRows = 25;
	}
	else
	{
		_screenCols = *((uint8_t far *)MK_FP(0x40, 0x4A));
		_screenRows = *((uint8_t far *)MK_FP(0x40, 0x84)) + 1;
	}

	if( _screenCols < 80 || _screenRows < 25 )
	{
		printf("Unsupported text mode: %ux%u. Please use 80x25 or larger.\n", _screenCols, _screenRows);
		return false;
	}
	return true;
}

void UiScreen::Clear()
{
	if( !_screen )
		return;

	int cells = _screenRows * _screenCols;
	for( int i = 0; i < cells; i++ )
	{
		_screen[i * 2]     = ' ';
		_screen[i * 2 + 1] = UI_ATTR_NORMAL;
	}
	gotoxy(1, 1);
}

void UiScreen::HideCursor()
{
	union REGS r;
	r.h.ah = 0x01;
	r.x.cx = 0x2607;
	int86(0x10, &r, &r);
}

void UiScreen::ShowCursor()
{
	union REGS r;
	r.h.ah = 0x01;
	r.x.cx = 0x0607;
	int86(0x10, &r, &r);
}

void UiScreen::Capture()
{
	if( !_screen )
		return;

	union REGS r;
	r.h.ah = 0x03;
	r.h.bh = 0x00;
	int86(0x10, &r, &r);

	_savedCursorStart = r.h.ch;
	_savedCursorEnd = r.h.cl;
	_savedCursorX = r.h.dl;
	_savedCursorY = r.h.dh;

	uint32_t bytes32 = (uint32_t)_screenRows * (uint32_t)_screenCols * 2ul;
	if( bytes32 > 65535ul )
		bytes32 = 65535ul;

	uint16_t bytes = (uint16_t)bytes32;
	if( bytes > _savedScreenCapacity )
	{
		uint8_t *buf = (uint8_t *)realloc(_savedScreen, bytes);
		if( buf == NULL )
		{
			_valid = false;
			_savedScreenBytes = 0;
			return;
		}

		_savedScreen = buf;
		_savedScreenCapacity = bytes;
	}

	_savedScreenBytes = bytes;
	for( uint16_t i = 0; i < _savedScreenBytes; i++ )
		_savedScreen[i] = _screen[i];

	_valid = true;
}

void UiScreen::Restore()
{
	if( !_valid || !_screen || _savedScreen == NULL )
	{
		ShowCursor();
		return;
	}

	for( uint16_t i = 0; i < _savedScreenBytes; i++ )
		_screen[i] = _savedScreen[i];

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

void UiScreen::PutChAtAttr(int x, int y, int ch, uint8_t attr)
{
	if( !_screen )
		return;
	if( x < 0 || y < 0 || x >= _screenCols || y >= _screenRows )
		return;
	
	uint16_t off = (uint16_t)((y * _screenCols + x) * 2);
	_screen[off] = (uint8_t)ch;
	_screen[off + 1] = attr;
}

void UiScreen::PutChAt(int x, int y, int ch)
{
	PutChAtAttr(x, y, ch, UI_ATTR_NORMAL);
}

void UiScreen::WritePadAt(int x, int y, const char *s, int w, uint8_t attr)
{
	if( w <= 0 )
		return;

	int i = 0;
	if( s != NULL )
	{
		while( i < w && s[i] != '\0' )
		{
			PutChAtAttr(x + i, y, s[i], attr);
			i++;
		}
	}

	while( i < w )
	{
		PutChAtAttr(x + i, y, ' ', attr);
		i++;
	}
}

void UiScreen::WriteFillAt(int x, int y, int count, int ch)
{
	for( int i = 0; i < count; i++ )
		PutChAt(x + i, y, ch);
}

void UiScreen::WriteAt(int x, int y, const char *s, uint8_t attr)
{
	if( s == NULL )
		return;

	for( int i = 0; s[i] != '\0'; i++ )
		PutChAtAttr(x + i, y, s[i], attr);
}
