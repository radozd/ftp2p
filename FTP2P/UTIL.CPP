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

// Strings

static void TrimRight(char *s)
{
	if( !s )
		return;

	size_t n = strlen(s);
	while( n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n') )
		s[--n] = '\0';
}

static void TrimLeft(char *s)
{
	if( !s )
		return;

	char *p = s;
	while( *p == ' ' || *p == '\t' )
		p++;
	if( p != s )
		memmove(s, p, strlen(p) + 1);
}

static void TrimBoth(char *s)
{
	TrimRight(s);
	TrimLeft(s);
}

static void CopyDataTrunc(char *dst, size_t dstSize, const char *src, size_t srcLen)
{
	if( dst == NULL || dstSize == 0 )
		return;

	dst[0] = '\0';	
	if( src == NULL )
		return;

	size_t copy_len = srcLen;
	if( copy_len > dstSize - 1 )
		copy_len = dstSize - 1;

	if( copy_len > 0 )
		memcpy(dst, src, copy_len);
	
	dst[copy_len] = '\0';
}

static void CopyTrunc(char *dst, size_t dstSize, const char *src)
{
	CopyDataTrunc(dst, dstSize, src, src ? strlen(src) : 0);
}

void SprintfTruncV(char *dst, size_t dstSize, const char *fmt, va_list ap)
{
	if( dst == NULL || dstSize == 0 )
		return;

	vsnprintf(dst, dstSize, fmt, ap);
	dst[dstSize - 1] = '\0';
}

void SprintfTrunc(char *dst, size_t dstSize, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	SprintfTruncV(dst, dstSize, fmt, ap);
	va_end(ap);
}

void CropLeft(char *dst, size_t len, const char *src)
{
	if( dst == NULL || len <= 0 )
		return;

	dst[0] = '\0';
	if( src == NULL )
		return;

	size_t n = strlen(src);
	size_t cap = len - 1;

	if( n <= cap )
		CopyDataTrunc(dst, len, src, n);
	else
		CopyDataTrunc(dst, len, src + (n - cap), cap);
}

// Filenames

void CropRight(char *dst, size_t len, const char *src)
{
	if( dst == NULL || len <= 0 )
		return;

	dst[0] = '\0';
	if( src == NULL )
		return;

	size_t n = strlen(src);
	size_t cap = len - 1;

	if( n <= cap )
	{
		CopyDataTrunc(dst, len, src, n);
		return;
	}

	size_t dot_pos = (size_t)-1;
	for( int i = 0; i < n; i++ )
	{
		if( src[i] == '.' )
			dot_pos = i;
	}

	if( dot_pos == (size_t)-1 || dot_pos == 0 || dot_pos >= n - 1 )
	{
		if( cap <= 3 )
		{
			CopyDataTrunc(dst, len, src, cap);
			return;
		}

		CopyDataTrunc(dst, len, src, cap - 3);
		dst[cap - 3] = '.';
		dst[cap - 2] = '.';
		dst[cap - 1] = '.';
		dst[cap] = '\0';
		return;
	}

	size_t ext_len = n - dot_pos;
	if( ext_len > 8 )
		ext_len = 8;

	if( cap <= ext_len + 3 )
	{
		CopyDataTrunc(dst, len, src, cap);
		return;
	}

	size_t keep_len = cap - ext_len - 3;
	CopyDataTrunc(dst, len, src, keep_len);
	dst[keep_len] = '.';
	dst[keep_len + 1] = '.';
	dst[keep_len + 2] = '.';
	CopyDataTrunc(dst + keep_len + 3, len - keep_len - 3, src + dot_pos, ext_len);
}

static bool IsDots(const char *s)
{
	return strcmp(s, ".") == 0 || strcmp(s, "..") == 0;
}

static bool IsReservedDosName(const char *src)
{
	char name[9];
	int i = 0;
	const char *p = src;

	while( *p && *p != '.' && i < 8 )
	{
		name[i++] = toupper((unsigned char)*p);
		p++;
	}
	name[i] = '\0';

	if( strcmp(name, "CON") == 0 )
		return true;
	if( strcmp(name, "PRN") == 0 )
		return true;
	if( strcmp(name, "AUX") == 0 )
		return true;
	if( strcmp(name, "NUL") == 0 )
		return true;
	if( strcmp(name, "CLOCK$") == 0 )
		return true;
	if( strcmp(name, "CONFIG$") == 0 )
		return true;

	if( strlen(name) == 4 && name[0] == 'C' && name[1] == 'O' && name[2] == 'M' && name[3] >= '1' && name[3] <= '9' )
		return true;

	if( strlen(name) == 4 && name[0] == 'L' && name[1] == 'P' && name[2] == 'T' && name[3] >= '1' && name[3] <= '9' )
		return true;

	return false;
}

static void GetLastPathComponent(char *nameBuf, int nameBufSize, const char *path)
{
	if( nameBuf == NULL || nameBufSize <= 0 )
		return;
	
	nameBuf[0] = '\0';
	
	if( path == NULL || path[0] == '\0' )
		return;

	const char *end = path + strlen(path);
	while( end > path && (end[-1] == '/' || end[-1] == '\\') )
		end--;
	if( end <= path )
		return;

	const char *p = end;
	while( p > path && p[-1] != '/' && p[-1] != '\\' )
		p--;

	CopyDataTrunc(nameBuf, (size_t)nameBufSize, p, (size_t)(end - p));
}

// Misc

static bool ShouldUpdateProgress(clockTicks_t &lastTick, uint32_t &lastBytes, uint32_t bytesNow)
{
	if( bytesNow <= lastBytes )
		return false;

	clockTicks_t now = TIMER_GET_CURRENT();

	if( lastBytes == 0 )
	{
		lastBytes = bytesNow;
		lastTick = now;
		return true;
	}

	if( bytesNow - lastBytes >= 32768ul )
	{
		lastBytes = bytesNow;
		lastTick = now;
		return true;
	}

	uint32_t elapsed = (uint32_t)(Timer_diff(lastTick, now) * TIMER_TICK_LEN);
	if( elapsed >= 500 )
	{
		lastBytes = bytesNow;
		lastTick = now;
		return true;
	}

	return false;
}

void FormatSize(char *dst, size_t dstSize, __int64 bytes)
{
	const __int64 KB = 1024ul;
	const __int64 MB = 1024ul * 1024ul;
	const __int64 GB = 1024ul * 1024ul * 1024ul;

	if( dst == NULL || dstSize == 0 )
		return;

	if( bytes < 10240ul )
	{
		SprintfTrunc(dst, dstSize, "%lu B", (unsigned long)bytes);
	}
	else if( bytes < MB )
	{
		SprintfTrunc(dst, dstSize, "%lu KB", (unsigned long)(bytes / KB));
	}
	else if( bytes < GB )
	{
		unsigned long whole = (unsigned long)(bytes / MB);
		__int64 rem = bytes % MB;
		unsigned long frac = (unsigned long)((rem * 100 + (MB / 2)) / MB);

		if( frac >= 100 )
		{
			whole++;
			frac = 0;
		}
		SprintfTrunc(dst, dstSize, "%lu.%02lu MB", whole, frac);
	}
	else
	{
		unsigned long whole = (unsigned long)(bytes / GB);
		__int64 rem = bytes % GB;
		unsigned long frac = (unsigned long)((rem * 1000 + (GB / 2)) / GB);
		
		if( frac >= 1000 )
		{
			whole++;
			frac = 0;
		}
		SprintfTrunc(dst, dstSize, "%lu.%03lu GB", whole, frac);
	}
}

static int KeyAscii(uint16_t key)
{
	return (int)(key & 0x00FF);
}
