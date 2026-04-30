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

class LocalFsTransport
{
public:
	LocalFsTransport();

	bool Init(StatusCallback statusCallback, UiList *localList);

	bool ReadCwd();
	bool ListCwd();
	bool RefreshList();

	bool ChangeToParentDir();
	bool ChangeWorkingDir(const char *path);
	bool RemoveDir(const char *path);
	bool DeleteFile(const char *path);
	bool MakeDir(const char *path);
	bool RenameItem(const char *oldPath, const char *newPath);

private:
	StatusCallback UiStatusCallback;
	UiList *_localList;

	void SetStatus(char type, const char *fmt, ...);
};

LocalFsTransport::LocalFsTransport()
{
	UiStatusCallback = NULL;
	_localList = NULL;
}

bool LocalFsTransport::Init(StatusCallback statusCallback, UiList *localList)
{
	UiStatusCallback = statusCallback;
	_localList = localList;
	return _localList != NULL;
}

void LocalFsTransport::SetStatus(char type, const char *fmt, ...)
{
	if( !UiStatusCallback )
		return;

	char tmp[160];
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);

	UiStatusCallback(type, tmp);
}

bool LocalFsTransport::ReadCwd()
{
	char tmp[160];
	if( getcwd(tmp, sizeof(tmp) - 1) == NULL )
	{
		_localList->SetCwd("");
		return false;
	}
	_localList->SetCwd(tmp);
	return true;
}

static void DosDateTimeToString(char *dst, size_t dstSize, unsigned short wr_date, unsigned short wr_time)
{
	unsigned day   =  wr_date        & 31;
	unsigned month = (wr_date >> 5)  & 15;
	unsigned year  = ((wr_date >> 9) & 127) + 1980;

	//unsigned sec   = (wr_time & 31) * 2;
	unsigned min   = (wr_time >> 5) & 63;
	unsigned hour  = (wr_time >> 11) & 31;

	SprintfTrunc(dst, dstSize, "%04u-%02u-%02u %02u:%02u", year, month, day, hour, min);
}

bool LocalFsTransport::ListCwd()
{
	if( _localList == NULL )
	{
		SetStatus('E', "Local refresh failed");
		return false;
	}

	_localList->Clear();

	struct diskfree_t df;
	if( _dos_getdiskfree(0, &df) == 0 )
	{
		__int64 freeBytes = (__int64)df.avail_clusters * (__int64)df.sectors_per_cluster * (__int64)df.bytes_per_sector;
		_localList->SetFreeBytes(freeBytes);
	}
	else
	{
		_localList->SetFreeBytes(-1);
	}

	struct find_t ff;
	_localList->Add("..", 'p', 0, "");

	int rc = _dos_findfirst("*.*", _A_NORMAL | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR | _A_ARCH, &ff);
	while( rc == 0 )
	{
		if( !IsDots(ff.name) )
		{
			char mod[32];
			DosDateTimeToString(mod, sizeof(mod), ff.wr_date, ff.wr_time);

			UiEntry *e = _localList->Add(ff.name, (ff.attrib & _A_SUBDIR) != 0 ? 'd':'f', (uint32_t)ff.size, mod);
			if( !e )
				break;
		}
		rc = _dos_findnext(&ff);
	}

	_localList->Sort();
	return true;
}

bool LocalFsTransport::RefreshList()
{
	return ReadCwd() && ListCwd();
}

bool LocalFsTransport::ChangeToParentDir()
{
	return chdir("..") == 0;
}

bool LocalFsTransport::ChangeWorkingDir(const char *path)
{
	if( path == NULL || path[0] == '\0' )
		return false;
	return chdir(path) == 0;
}

bool LocalFsTransport::RemoveDir(const char *path)
{
	return path != NULL && path[0] != '\0' && rmdir(path) == 0;
}

bool LocalFsTransport::DeleteFile(const char *path)
{
	return path != NULL && path[0] != '\0' && unlink(path) == 0;
}

bool LocalFsTransport::MakeDir(const char *path)
{
	return path != NULL && path[0] != '\0' && mkdir(path) == 0;
}

bool LocalFsTransport::RenameItem(const char *oldPath, const char *newPath)
{
	if( oldPath == NULL || oldPath[0] == '\0' || newPath == NULL || newPath[0] == '\0' )
		return false;
	return rename(oldPath, newPath) == 0;
}
