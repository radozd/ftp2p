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

#define UI_NAME_MAX     (128)
#define	NAME_POOL_SIZE  (64000u)

typedef struct
{
	char *name;
	char modif[20];
	uint32_t size;
	char kind;   /* 'p' parent, 'd' dir, 'f' file, 'F' selected file */
} UiEntry;

class UiList
{
public:
	UiList(int cap, bool dosNames=false);
	~UiList();

	bool Ok() const { return _items != NULL && _namePool != NULL; }
	void Clear();
	void Sort();

	void SetCwd(const char *cwd);
	const char *GetCwd() const { return _cwd; }

	void PrintCwd(char *dst, int maxw);
	void PrintName(char *dst, int maxw, UiEntry *e);
	void PrintStats(char *dst, int maxw, UiEntry *e);
	
	void SetFreeBytes(__int64 freeBytes) { _freeBytes = freeBytes; }
	__int64 GetFreeBytes() const { return _freeBytes; }

	void UpdateEncoding();
	
	UiEntry *Add(const char *name, char kind, uint32_t size, const char *modif);
	UiEntry *GetCurrent();
	UiEntry *GetFromTop(int idx);
	UiEntry *FindByName(const char *name);
	
	bool SelectByName(const char *name);
	void EnsureVisible(int height);
	bool MoveSel(int delta);
	bool ToggleCurrentSelection(bool moveAfter);
	void ClearSelectionMarks();
	void SelectAllFiles();
	void InvertFileSelection();
	int SelectedFileCount() const;
	uint32_t SelectedFileBytes() const;
	int AllSelected(UiEntry **items, int maxItems);
	void SetPendingSelection(const char *name);
	void SetPendingSelectionFromCwdTail();
	void SetFallbackSelectionToCurrent();
	void ClearPendingSelection();

	bool NeedRefresh() const { return _needRefresh; }
	void Invalidate() { _needRefresh = true; }
	void Validate() { _needRefresh = false; }
	bool Refresh(bool (*reloadFunc)());
	void ResetSelection();
	
	bool IsTruncated() const { return _truncated; }
	int Count() const { return _count; }
	int Top() const { return _top; }

private:
	bool _dosNames;
	bool _truncated;
	bool _needRefresh;
	char _cwd[128];

	char *_namePool;
	uint16_t _namePoolUsed;
	uint16_t _namePoolSize;

	UiEntry *_items;	
	int _count;
	int _capacity;
	int _top;
	int _sel;
	
	char _pendingSelection[UI_NAME_MAX];
	int _fallbackSelectionIndex;
	__int64 _freeBytes;

	char *AllocName(const char *name);
	static int EntryCmp(const void *a, const void *b);
};

UiList::UiList(int cap, bool dosNames)
{
	_capacity = 0;
	_count = 0;
	_sel = 0;
	_top = 0;
	_dosNames = dosNames;
	_truncated = false;
	_needRefresh = true;
	_items = NULL;
	_namePool = NULL;
	_namePoolUsed = 0;
	_cwd[0] = '\0';
	_pendingSelection[0] = '\0';
	_fallbackSelectionIndex = -1;
	_freeBytes = -1;

	if( cap <= 0 )
		return;

	_capacity = cap;
	_items = (UiEntry *)calloc((size_t)cap, sizeof(UiEntry));

	if( _dosNames )
	{
		unsigned long need = (unsigned long)cap * 13; // 8.3\0
		if( need > NAME_POOL_SIZE )
			need = NAME_POOL_SIZE;
		_namePoolSize = (uint16_t)need;
	}
	else
	{
		_namePoolSize = (uint16_t)NAME_POOL_SIZE;
	}
	_namePool = (char *)malloc(_namePoolSize);
}

UiList::~UiList()
{
	if( _items )
		free(_items);
	if( _namePool )
		free(_namePool);
	_items = NULL;
	_namePool = NULL;
	_namePoolUsed = 0;
	_namePoolSize = 0;
	_capacity = 0;
	_count = 0;
	_sel = 0;
	_top = 0;
	_dosNames = false;
	_truncated = false;
	_needRefresh = false;
	_pendingSelection[0] = '\0';
	_fallbackSelectionIndex = -1;
	_freeBytes = -1;
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
	_namePoolUsed = 0;
	_fallbackSelectionIndex = -1;
}

char *UiList::AllocName(const char *name)
{
	if( _namePool == NULL )
		return NULL;

	if( name == NULL )
		name = "";

	size_t need = strlen(name) + 1u;
	if( need > (size_t)(_namePoolSize - _namePoolUsed) )
		return NULL;

	char *dst = _namePool + _namePoolUsed;
	memcpy(dst, name, need);
	_namePoolUsed = (uint16_t)(_namePoolUsed + (uint16_t)need);
	return dst;
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
	qsort(_items, (size_t)_count, sizeof(UiEntry), EntryCmp);
}

void UiList::SetCwd(const char *cwd)
{
	CopyTrunc(_cwd, sizeof(_cwd), cwd);
}

void UiList::PrintCwd(char *dst, int maxw)
{
	char *name = _cwd;
	char tmp[160];
	if( !_dosNames && !g_noUtf8Arg )
	{
		if( g_simpleUtf8 )
			Utf8ToDosNameCharset(tmp, sizeof(tmp), _cwd);
		else
			Utf8ToOem(tmp, sizeof(tmp), _cwd);

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
		if( g_simpleUtf8 )
			Utf8ToDosNameCharset(tmp, sizeof(tmp), name);
		else
			Utf8ToOem(tmp, sizeof(tmp), name);
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
			StrToUpper(dst);
		else
			StrToLower(dst);
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

	int spaces = maxw - strlen(sizebuf) - strlen(e->modif) - 1;
	if( spaces < 1 )
		spaces = 1;

	SprintfTrunc(dst, maxw, "%s%*s%s", sizebuf, spaces, "", e->modif);
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

	char *stored = AllocName(name);
	if( stored == NULL )
	{
		_truncated = true;
		return NULL;
	}

	UiEntry *e = &_items[_count];
	e->name = stored;
	e->kind = kind;
	e->size = size;
	CopyTrunc(e->modif, sizeof(e->modif), modif);

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

	for( int i = 0; i < _count; i++ )
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

	for( int i = 0; i < _count; i++ )
	{
		if( stricmp(_items[i].name, name) == 0 )
		{
			_sel = i;
			return true;
		}
	}

	return false;
}

void UiList::EnsureVisible(int height)
{
	if( _count <= 0 )
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
	if( _count <= 0 )
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
	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'F' )
			_items[i].kind = 'f';
	}
}

void UiList::SelectAllFiles()
{
	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'f' )
			_items[i].kind = 'F';
	}
}

void UiList::InvertFileSelection()
{
	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'f' )
			_items[i].kind = 'F';
		else if( _items[i].kind == 'F' )
			_items[i].kind = 'f';
	}
}

int UiList::SelectedFileCount() const
{
	int n = 0;

	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'F')
			n++;
	}

	return n;
}

uint32_t UiList::SelectedFileBytes() const
{
	uint32_t total = 0;

	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].kind == 'F')
			total += _items[i].size;
	}

	return total;
}

int UiList::AllSelected(UiEntry **items, int maxItems)
{
	int n = 0;

	if( items == NULL || maxItems <= 0 )
		return 0;

	for( int i = 0; i < _count && n < maxItems; i++ )
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

bool UiList::Refresh(bool (*reloadFunc)())
{
	if( reloadFunc == NULL )
		return false;

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

	if( !reloadFunc() )
		return false;

	Sort();

	if( _pendingSelection[0] )
	{
		if( SelectByName(_pendingSelection) )
		{
			ClearPendingSelection();
			_fallbackSelectionIndex = -1;
			return true;
		}
		ClearPendingSelection();
	}

	if( current[0] && SelectByName(current) )
	{
		_fallbackSelectionIndex = -1;
		return true;
	}

	if( _count <= 0 )
	{
		_sel = 0;
		_top = 0;
		_fallbackSelectionIndex = -1;
		return true;
	}

	if( fallback < 0 )
		fallback = 0;
	if( fallback >= _count )
		fallback = _count - 1;
	_sel = fallback;
	_fallbackSelectionIndex = -1;
	return true;
}

void UiList::UpdateEncoding()
{
	if( _count < 2 )
		return;

	unsigned cp850 = 0;
	unsigned cp852 = 0;
	unsigned cp866 = 0;

	for( int i = 0; i < _count; i++ )
	{
		UiEntry *e = &_items[i];
		if( e != NULL && e->kind != 'p' )
			CountOemChars(e->name, cp850, cp852, cp866);
	}

	if( cp866 > cp852 && cp866 > cp850 )
		g_encoding = OEM_CP866;
	else if( cp852 > cp850 )
		g_encoding = OEM_CP852;
	else if( cp850 > 0 )
		g_encoding = OEM_CP850;
	else
		g_encoding = OEM_ASCII;
}
