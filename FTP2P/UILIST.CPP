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

#define UI_NAME_MAX           (128)

typedef struct
{
	char name[UI_NAME_MAX];
	char modif[20];
	uint32_t size;
	bool isDir;
	bool isParent;
	bool selected;
} UiEntry;

class UiList
{
public:
	UiList(int cap, bool dosNames=false);
	~UiList();

	bool Ok() const { return _items != NULL; }
	void Clear();
	void Sort();

	void SetCwd(const char *cwd);
	const char *GetCwd() const { return _cwd; }

	void PrintCwd(char *dst, int maxw);
	void PrintName(char *dst, int maxw, UiEntry *e);
	void PrintStats(char *dst, int maxw, UiEntry *e);
	void SetFreeBytes(long freeBytes) { _freeBytes = freeBytes; }
	long GetFreeBytes() const { return _freeBytes; }

	void UpdateEncoding();
	
	UiEntry *Add(const char *name, bool isDir, bool isParent, uint32_t size, const char *modif);
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
	char _cwd[128];
	UiEntry *_items;
	int _count;
	int _capacity;
	int _top;
	int _sel;
	char _pendingSelection[UI_NAME_MAX];
	int _fallbackSelectionIndex;
	long _freeBytes;
	bool _needRefresh;

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
	_cwd[0] = '\0';
	_pendingSelection[0] = '\0';
	_fallbackSelectionIndex = -1;
	_freeBytes = -1;

	if( cap <= 0 )
		return;

	_capacity = cap;
	_items = (UiEntry *)calloc((size_t)cap, sizeof(UiEntry));
}

UiList::~UiList()
{
	if( _items )
		free(_items);
	_items = NULL;
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
	_fallbackSelectionIndex = -1;
}

int UiList::EntryCmp(const void *a, const void *b)
{
	const UiEntry *ea = (const UiEntry *)a;
	const UiEntry *eb = (const UiEntry *)b;

	if( ea->isParent != eb->isParent )
		return ea->isParent ? -1 : 1;
	if( ea->isDir != eb->isDir )
		return ea->isDir ? -1 : 1;

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
	if( !_dosNames && !g_NoUtf8Arg )
	{
		Utf8ToOem(tmp, sizeof(tmp), _cwd, g_Encoding);
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

	if( e->isParent )
	{
		strcpy(dst, "[..]");
		return;
	}
		
	char *name = e->name;
	char tmp[160];
	if( !_dosNames && !g_NoUtf8Arg )
	{
		Utf8ToOem(tmp, sizeof(tmp), name, g_Encoding);
		name = tmp;
	}

	if( e->isDir )
	{
		dst[0] = '[';
		CropRight(dst + 1, maxw - 2, name);
		strcat(dst, "]");
	}
	else
		CropRight(dst, maxw, name);
		
	if( _dosNames )
	{
		if( e->isDir )
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

	if( e == NULL || e->isParent || e->isDir )
		return;

	char left[32];
	sprintf(left, "%lu bytes", (unsigned long)e->size);

	int spaces = maxw - strlen(left) - strlen(e->modif) - 1;
	if( spaces < 1 )
		spaces = 1;

	SprintfTrunc(dst, maxw, "%s%*s%s", left, spaces, "", e->modif);
}

UiEntry *UiList::Add(const char *name, bool isDir, bool isParent, uint32_t size, const char *modif)
{
	if( _items == NULL )
		return NULL;
	if( _count >= _capacity )
	{
		_truncated = true;
		return NULL;
	}

	UiEntry *e = &_items[_count];
	CopyTrunc(e->name, sizeof(e->name), name);
	e->isDir = isDir;
	e->isParent = isParent;
	e->size = size;
	CopyTrunc(e->modif, sizeof(e->modif), modif);
	e->selected = false;

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

	int oldSel = _sel;
	_sel += delta;
	if( _sel < 0 )
		_sel = 0;
	if( _sel >= _count )
		_sel = _count - 1;

	return _sel != oldSel;
}

bool UiList::ToggleCurrentSelection(bool moveAfter)
{
	UiEntry *e = GetCurrent();
	if( e == NULL || e->isParent || e->isDir )
		return false;

	e->selected = !e->selected;

	if( moveAfter && _sel < _count - 1 )
		_sel++;

	return true;
}

void UiList::ClearSelectionMarks()
{
	for( int i = 0; i < _count; i++ )
		_items[i].selected = false;
}

void UiList::SelectAllFiles()
{
	for( int i = 0; i < _count; i++ )
	{
		if( !_items[i].isParent && !_items[i].isDir )
			_items[i].selected = true;
	}
}

void UiList::InvertFileSelection()
{
	for( int i = 0; i < _count; i++ )
	{
		if( !_items[i].isParent && !_items[i].isDir )
			_items[i].selected = !_items[i].selected;
	}
}

int UiList::SelectedFileCount() const
{
	int n = 0;

	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].selected && !_items[i].isParent && !_items[i].isDir )
			n++;
	}

	return n;
}

uint32_t UiList::SelectedFileBytes() const
{
	uint32_t total = 0;

	for( int i = 0; i < _count; i++ )
	{
		if( _items[i].selected && !_items[i].isParent && !_items[i].isDir )
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
		if( _items[i].selected && !_items[i].isParent && !_items[i].isDir )
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

	char currentName[UI_NAME_MAX];
	currentName[0] = '\0';

	UiEntry *cur = GetCurrent();
	if( cur )
	{
		if( cur->isParent )
			GetLastPathComponent(currentName, sizeof(currentName), _cwd);
		else
			CopyTrunc(currentName, sizeof(currentName), cur->name);
	}

	int fallbackIndex = _fallbackSelectionIndex;
	if( fallbackIndex < 0 )
		fallbackIndex = _sel;

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

	if( currentName[0] && SelectByName(currentName) )
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

	if( fallbackIndex < 0 )
		fallbackIndex = 0;
	if( fallbackIndex >= _count )
		fallbackIndex = _count - 1;
	_sel = fallbackIndex;
	_fallbackSelectionIndex = -1;
	return true;
}

void UiList::UpdateEncoding()
{
	if( _count < 2 )
		return;

	int cp850Count = 0;
	int cp866Count = 0;

	for( int i = 0; i < _count; i++ )
	{
		UiEntry *e = &_items[i];
		if( e != NULL && !e->isParent )
			CountDetectedCodePageChars(e->name, cp850Count, cp866Count);
	}

	if( cp866Count > cp850Count )
		g_Encoding = OEM_CP866;
	else if( cp850Count > cp866Count )
		g_Encoding = OEM_CP850;
	else if( cp850Count == 0 && cp866Count == 0 )
		g_Encoding = OEM_ASCII;
}
