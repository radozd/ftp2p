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

#include "DTIME.CPP"

#define UI_NAME_MAX          (128)
#define NAME_POOL_BLOCK_SIZE (16u * 1024u)

struct NamePoolBlock
{
	struct NamePoolBlock *next;
	size_t used;
	size_t size;
	char data[1];
};

class NamePool
{
public:
	NamePool()
	{
		_first = NULL;
		_last = NULL;
	}

	~NamePool()
	{
		Clear();
	}

	void Clear()
	{
		NamePoolBlock *b = _first;
		while( b != NULL )
		{
			NamePoolBlock *next = b->next;
			free(b);
			b = next;
		}

		_first = NULL;
		_last = NULL;
	}

	char *Add(const char *name)
	{
		if( name == NULL )
			name = "";

		size_t need = strlen(name) + 1u;
		if( _last == NULL || need > _last->size - _last->used )
		{
			if( AllocBlock(need) == NULL )
				return NULL;
		}

		char *dst = _last->data + _last->used;
		memcpy(dst, name, need);
		_last->used = _last->used + need;
		return dst;
	}

private:
	NamePoolBlock *_first;
	NamePoolBlock *_last;

	NamePoolBlock *AllocBlock(size_t need)
	{
		size_t size = NAME_POOL_BLOCK_SIZE;
		if( need > size )
			size = need;

		NamePoolBlock *b = (NamePoolBlock *)malloc(sizeof(NamePoolBlock) + size - 1u);
		if( b == NULL )
			return NULL;

		b->next = NULL;
		b->used = 0;
		b->size = size;

		if( _last != NULL )
			_last->next = b;
		else
			_first = b;

		_last = b;
		return b;
	}
};


typedef struct
{
	char *name;
	uint32_t modif;
	uint32_t size;
	char kind;   /* 'p' parent, 'd' dir, 'f' file, 'F' selected file */
} UiEntry;

class UiList
{
public:
	UiList(size_t cap, bool dosNames=false);
	~UiList();

	bool Ok() const { return _items != NULL; }
	void Clear();
	void Sort();

	void SetCwd(const char *cwd);
	const char *GetCwd() const { return _cwd; }

	void PrintCwd(char *dst, int maxw);
	void PrintName(char *dst, int maxw, UiEntry *e);
	void PrintStats(char *dst, int maxw, UiEntry *e);
	
	void SetFreeBytes(__int64 freeBytes) { _freeBytes = freeBytes; }

	void FormatSelectedInfo(char *dst, size_t dstSize);
	void FormatListInfo(char *dst, size_t dstSize);

	void UpdateEncoding();
	
	UiEntry *Add(const char *name, char kind, uint32_t size, const char *modif);
	UiEntry *GetCurrent();
	UiEntry *GetFromTop(int idx);
	UiEntry *FindByName(const char *name);
	
	bool SelectByName(const char *name);
	bool SelectByFirstChar(int ch);
	void EnsureVisible(int height);
	bool MoveSel(int delta);
	bool ToggleCurrentSelection(bool moveAfter);
	void ClearSelectionMarks();
	void SelectAllFiles();
	void InvertFileSelection();
	size_t SelectedFileCount() const;
	__int64 SelectedFileBytes() const;
	size_t AllSelected(UiEntry **items, size_t maxItems);
	void SetPendingSelection(const char *name);
	void SetPendingSelectionFromCwdTail();
	void SetFallbackSelectionToCurrent();
	void ClearPendingSelection();

	void Refresh(bool (*reloadFunc)());
	void ResetSelection();
	
	bool IsTruncated() const { return _truncated; }
	bool IsNotRefreshed() const { return _refreshFailed; }
	size_t Count() const { return _count; }
	int Top() const { return _top; }

private:
	bool _dosNames;
	bool _truncated;
	bool _refreshFailed;
	char _cwd[128];
	OemCp _encoding;

	NamePool _namePool;
	DateTimeCodec _dtcodec;

	UiEntry *_items;	
	size_t _count;
	size_t _capacity;
	int _top;
	int _sel;
	
	char _pendingSelection[UI_NAME_MAX];
	int _fallbackSelectionIndex;
	__int64 _freeBytes;

	char *AllocName(const char *name);
	static int EntryCmp(const void *a, const void *b);
};

UiList::UiList(size_t cap, bool dosNames)
{
	_capacity = 0;
	_count = 0;
	_sel = 0;
	_top = 0;
	_dosNames = dosNames;
	_truncated = false;
	_refreshFailed = true;
	_items = NULL;
	_cwd[0] = '\0';
	_encoding = g_encoding;
	_pendingSelection[0] = '\0';
	_fallbackSelectionIndex = -1;
	_freeBytes = -1;

	if( cap > 0 )
	{
		_capacity = cap;
		_items = (UiEntry *)calloc(cap, sizeof(UiEntry));
	}
}

UiList::~UiList()
{
	if( _items )
		free(_items);
	_items = NULL;
	
	Clear();
}

void UiList::ClearPendingSelection()
{
	_pendingSelection[0] = '\0';
}

void UiList::ResetSelection()
{
	_sel = 0;
	_top = 0;
	ClearPendingSelection();
	_fallbackSelectionIndex = -1;
}

void UiList::Clear()
{
	_count = 0;
	_sel = 0;
	_top = 0;
	_truncated = false;
	_fallbackSelectionIndex = -1;

	_dtcodec.Clear();
	_namePool.Clear();
}

int UiList::EntryCmp(const void *a, const void *b)
{
	const UiEntry *ea = (const UiEntry *)a;
	const UiEntry *eb = (const UiEntry *)b;

	if( ea->kind != eb->kind )
	{
		if( ea->kind == 'p' ) return -1;
		if( eb->kind == 'p' ) return 1;

		if( ea->kind == 'd' ) return -1;
		if( eb->kind == 'd' ) return 1;
	}

	int rc = stricmp(ea->name, eb->name);
	if( rc != 0 )
		return rc;

	return strcmp(ea->name, eb->name);
}

void UiList::Sort()
{
	if( _items == NULL || _count <= 1 )
		return;
	qsort(_items, _count, sizeof(UiEntry), EntryCmp);
}

void UiList::SetCwd(const char *cwd)
{
	CopyTrunc(_cwd, sizeof(_cwd), cwd);
}

void UiList::PrintCwd(char *dst, int maxw)
{
	if( _cwd[0] == '\0' || (_cwd[1] == '\0' && _cwd[0] == '/') )
	{
		dst[0] = '\0';
		return;
	}

	char *name = _cwd;
	char tmp[160];
	if( !_dosNames && !g_noUtf8Arg )
	{
		if( g_simpleUtf8 || g_encoding != _encoding )
			Utf8ToDosNameCharset(tmp, sizeof(tmp), _cwd);
		else
			Utf8ToOem(tmp, sizeof(tmp), _cwd, _encoding);

		name = tmp;
	}

	dst[0] = ' ';
	CropLeft(dst + 1, maxw - 2, name);
	strcat(dst, " ");
}

void UiList::PrintName(char *dst, int maxw, UiEntry *e)
{
	if( dst == NULL || maxw <= 0 )
		return;
	
	dst[0] = '\0';
	if( !e )
		return;

	if( e->kind == 'p' )
	{
		strcpy(dst, "[..]");
		return;
	}
		
	char *name = e->name;
	char tmp[160];
	if( !_dosNames && !g_noUtf8Arg )
	{
		if( g_simpleUtf8 || g_encoding != _encoding )
			Utf8ToDosNameCharset(tmp, sizeof(tmp), name);
		else
			Utf8ToOem(tmp, sizeof(tmp), name, _encoding);
		name = tmp;
	}

	if( e->kind == 'd' )
	{
		dst[0] = '[';
		CropRight(dst + 1, maxw - 2, name);
		strcat(dst, "]");
	}
	else
		CropRight(dst, maxw, name);
		
	if( _dosNames )
	{
		if( e->kind == 'd' )
			StrToUpper(dst, _encoding);
		else
			StrToLower(dst, _encoding);
	}
}

void UiList::PrintStats(char *dst, int maxw, UiEntry *e)
{
	if( dst == NULL || maxw <= 0 )
		return;
	
	dst[0] = '\0';
	if( e == NULL || e->kind == 'p' || e->kind == 'd' )
		return;

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), e->size);

	char dtbuf[20];
	_dtcodec.Decode(dtbuf, sizeof(dtbuf), e->modif);

	int spaces = maxw - strlen(sizebuf) - strlen(dtbuf) - 1;
	if( spaces < 1 )
		spaces = 1;

	SprintfTrunc(dst, maxw, "%s%*s%s", sizebuf, spaces, "", dtbuf);
}

UiEntry *UiList::Add(const char *name, char kind, uint32_t size, const char *modif)
{
	if( _items == NULL )
		return NULL;
	if( _count >= _capacity )
	{
		_truncated = true;
		return NULL;
	}

	char *stored = _namePool.Add(name);
	if( stored == NULL )
	{
		_truncated = true;
		return NULL;
	}

	UiEntry *e = &_items[_count];
	e->name = stored;
	e->kind = kind;
	e->size = size;
	e->modif = _dtcodec.Encode(modif);

	_count++;
	return e;
}

UiEntry *UiList::GetCurrent()
{
	if( _count > 0 && _sel >= 0 && _sel < _count )
		return &_items[_sel];
	return NULL;
}

UiEntry *UiList::GetFromTop(int idx)
{
	int i = _top + idx;
	if( idx >= 0 && i < _count )
		return &_items[i];

	return NULL;
}

UiEntry *UiList::FindByName(const char *name)
{
	if( name == NULL || *name == '\0' )
		return NULL;

	for( size_t i = 0; i < _count; i++ )
	{
		if( stricmp(_items[i].name, name) == 0 )
			return &_items[i];
	}

	return NULL;
}

bool UiList::SelectByName(const char *name)
{
	if( name == NULL || *name == '\0' )
		return false;

	for( size_t i = 0; i < _count; i++ )
	{
		if( stricmp(_items[i].name, name) == 0 )
		{
			_sel = i;
			return true;
		}
	}

	return false;
}

bool UiList::SelectByFirstChar(int ch)
{
	if( _count == 0 || ch <= 0 )
		return false;

	int needle = CharToUpper(ch & 0xFF, g_encoding);
	int start = 0 <= _sel && _sel < _count ? _sel + 1 : 0;

	for( size_t n = 0; n < _count; n++ )
	{
		size_t i = (size_t)((start + (int)n) % (int)_count);
		UiEntry *e = &_items[i];

		if( e->kind == 'p' || e->name == NULL || e->name[0] == '\0' )
			continue;

		if( CharToUpper((unsigned char)e->name[0], g_encoding) == needle )
		{
			_sel = (int)i;
			return true;
		}
	}

	return false;
}

void UiList::EnsureVisible(int height)
{
	if( _count == 0 )
	{
		ResetSelection();
		return;
	}

	if( _sel < 0 )
		_sel = 0;
	if( _sel >= _count )
		_sel = _count - 1;
	if( _top > _sel )
		_top = _sel;
	if( _sel >= _top + height )
		_top = _sel - height + 1;
	if( _top < 0 )
		_top = 0;
}

bool UiList::MoveSel(int delta)
{
	if( _count == 0 )
		return false;

	int old = _sel;
	_sel += delta;
	if( _sel < 0 )
		_sel = 0;
	if( _sel >= _count )
		_sel = _count - 1;

	return _sel != old;
}

bool UiList::ToggleCurrentSelection(bool moveAfter)
{
	UiEntry *e = GetCurrent();
	if( e == NULL )
		return false;

	if( e->kind == 'f' )
		e->kind = 'F';
	else if( e->kind == 'F' )
		e->kind = 'f';
	else
		return false;

	if( moveAfter && _sel < _count - 1 )
		_sel++;

	return true;
}

void UiList::ClearSelectionMarks()
{
	for( size_t i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'F' )
			_items[i].kind = 'f';
	}
}

void UiList::SelectAllFiles()
{
	for( size_t i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'f' )
			_items[i].kind = 'F';
	}
}

void UiList::InvertFileSelection()
{
	for( size_t i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'f' )
			_items[i].kind = 'F';
		else if( _items[i].kind == 'F' )
			_items[i].kind = 'f';
	}
}

size_t UiList::SelectedFileCount() const
{
	size_t n = 0;

	for( size_t i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'F')
			n++;
	}

	return n;
}

__int64 UiList::SelectedFileBytes() const
{
	__int64 total = 0;

	for( size_t i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'F')
			total += (__int64)_items[i].size;
	}

	return total;
}

size_t UiList::AllSelected(UiEntry **items, size_t maxItems)
{
	if( items == NULL || maxItems == 0 )
		return 0;

	size_t n = 0;
	for( size_t i = 0; i < _count && n < maxItems; i++ )
	{
		if( _items[i].kind == 'F')
			items[n++] = &_items[i];
	}

	return n;
}

void UiList::SetPendingSelection(const char *name)
{
	_fallbackSelectionIndex = -1;
	CopyTrunc(_pendingSelection, sizeof(_pendingSelection), name);
}

void UiList::SetPendingSelectionFromCwdTail()
{
	_fallbackSelectionIndex = -1;
	GetLastPathComponent(_pendingSelection, sizeof(_pendingSelection), _cwd);
}

void UiList::SetFallbackSelectionToCurrent()
{
	ClearPendingSelection();
	_fallbackSelectionIndex = _sel;
}

void UiList::Refresh(bool (*reloadFunc)())
{
	char current[UI_NAME_MAX];
	current[0] = '\0';

	UiEntry *cur = GetCurrent();
	if( cur )
	{
		if( cur->kind == 'p' )
			GetLastPathComponent(current, sizeof(current), _cwd);
		else
			CopyTrunc(current, sizeof(current), cur->name);
	}

	int fallback = _fallbackSelectionIndex;
	if( fallback < 0 )
		fallback = _sel;

	_refreshFailed = !reloadFunc();
	if( _refreshFailed )
		return;

	Sort();

	if( _pendingSelection[0] )
	{
		if( SelectByName(_pendingSelection) )
		{
			ClearPendingSelection();
			_fallbackSelectionIndex = -1;
			return;
		}
		ClearPendingSelection();
	}

	if( current[0] && SelectByName(current) )
	{
		_fallbackSelectionIndex = -1;
		return;
	}

	if( _count == 0 )
	{
		_sel = 0;
		_top = 0;
		_fallbackSelectionIndex = -1;
		return;
	}

	if( fallback < 0 )
		fallback = 0;
	if( fallback >= _count )
		fallback = _count - 1;
	_sel = fallback;
	_fallbackSelectionIndex = -1;
}

void UiList::UpdateEncoding()
{
	if( _count < 2 )
		return;

	unsigned cp850 = 0;
	unsigned cp852 = 0;
	unsigned cp866 = 0;

	for( size_t i = 0; i < _count; i++ )
	{
		UiEntry *e = &_items[i];
		if( e != NULL && e->kind != 'p' )
			CountOemChars(e->name, cp850, cp852, cp866);
	}

	if( cp866 > cp852 && cp866 > cp850 )
		_encoding = OEM_CP866;
	else if( cp852 > cp850 )
		_encoding = OEM_CP852;
	else if( cp850 > 0 )
		_encoding = OEM_CP850;
	else
		_encoding = OEM_ASCII;
}

void UiList::FormatSelectedInfo(char *dst, size_t dstSize)
{
	if( dst == NULL || dstSize == 0 )
		return;

	dst[0] = '\0';

	int count = SelectedFileCount();
	if( count <= 0 )
		return;

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), SelectedFileBytes());

	SprintfTrunc(dst, dstSize, " %d / %s ", count, sizebuf);
}

void UiList::FormatListInfo(char *dst, size_t dstSize)
{
	if( dst == NULL || dstSize == 0 )
		return;

	dst[0] = '\0';

	if( _truncated )
	{
		CopyTrunc(dst, dstSize, " truncated ");
		return;
	}

	if( _refreshFailed )
	{
		CopyTrunc(dst, dstSize, " failed ");
		return;
	}

	if( _freeBytes < 0 )
		return;

	char sizebuf[32];
	FormatSize(sizebuf, sizeof(sizebuf), _freeBytes);

	SprintfTrunc(dst, dstSize, " %s free ", sizebuf);
}
