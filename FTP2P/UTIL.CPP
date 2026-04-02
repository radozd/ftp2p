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

	const char *dot = strrchr(src, '.');
	if( dot == NULL || dot == src || dot[1] == '\0' )
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

	size_t ext_len = strlen(dot);
	if( ext_len > 8 )
		ext_len = 8;

	size_t baseLen = dot - src;
	size_t maxBaseLen = cap - ext_len;

	if( baseLen <= maxBaseLen )
	{
		CopyDataTrunc(dst, len, src, baseLen);
		CopyDataTrunc(dst + baseLen, len - baseLen, dot, ext_len);
		return;
	}

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
	CopyDataTrunc(dst + keep_len + 3, len - keep_len - 3, dot, ext_len);
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

	uint32_t elapsedMs = (uint32_t)(Timer_diff(lastTick, now) * TIMER_TICK_LEN);
	if( elapsedMs >= 500 )
	{
		lastBytes = bytesNow;
		lastTick = now;
		return true;
	}

	return false;
}

void FormatSize(char *dst, size_t dstSize, __int64 bytes)
{
	if( dst == NULL || dstSize == 0 )
		return;

	if( bytes < 10240ul )
		SprintfTrunc(dst, (size_t)dstSize, "%lu B", (unsigned long)bytes);
	else if( bytes < 1024ul * 1024ul )
		SprintfTrunc(dst, (size_t)dstSize, "%lu KB", (unsigned long)(bytes / 1024ul));
	else if( bytes < 1024ul * 1024ul * 1024ul )
		SprintfTrunc(dst, (size_t)dstSize, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
	else
		SprintfTrunc(dst, (size_t)dstSize, "%.3f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
}

static int KeyAscii(uint16_t key)
{
	return (int)(key & 0x00FF);
}

static bool IsEscKey(uint16_t key)
{
	return KeyAscii(key) == 27;
}

// Encoding
typedef enum
{
	OEM_ASCII,
	OEM_CP850,
	OEM_CP866
} OemCp;

OemCp g_Encoding = OEM_CP866;

static const unsigned char g_cp850_a0_ff[0x60] =
{
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	0xB7, 0xB5, 0xB6, 0xC7, 0x8E, 0x8F, 0x92, 0x80,
	0xD4, 0x90, 0xD2, 0xD3, 0x8D, 0xA1, 0x8C, 0x8B,
	'_',  0xA5, 0x95, 0xA2, 0x93, 0xE5, 0x99, '_',
	0x9D, 0x97, 0xA3, 0x96, 0x9A, 0xEC, '_',  0xE1,
	0x85, 0xA0, 0x83, 0xC6, 0x84, 0x86, 0x91, 0x87,
	0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,
	'_',  0xA4, 0x95, 0xA2, 0x93, 0xE4, 0x94, '_',
	0x9B, 0x97, 0xA3, 0x96, 0x81, 0xEC, '_',  0x98
};

static unsigned char UnicodeToCp850(unsigned ch)
{
	if( ch < 0x80 )
		return (unsigned char)ch;

	if( ch >= 0xA0 && ch <= 0xFF )
		return g_cp850_a0_ff[ch - 0xA0];

	return '_';
}

static const uint16_t g_cp850_to_unicode[0x80] =
{
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, '_',    '_',    '_',    '_',    '_',
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, '_',    '_',
	'_',    '_',    '_',    '_',    '_',    '_',    '_',    '_',
	'_',    '_',    '_',    '_',    '_',    0x00C1, 0x00C2, 0x00C0,
	'_',    '_',    '_',    '_',    '_',    '_',    '_',    '_',
	'_',    '_',    '_',    '_',    '_',    '_',    0x00E3, 0x00C3,
	'_',    '_',    '_',    '_',    '_',    '_',    '_',    '_',
	'_',    '_',    0x00CA, 0x00CB, 0x00C8, '_',    '_',    '_',
	'_',    '_',    '_',    '_',    '_',    '_',    '_',    '_',
	'_',    0x00DF, '_',    '_',    0x00F5, 0x00D5, '_',    '_',
	'_',    '_',    '_',    '_',    0x00FC, '_',    '_',    '_',
	'_',    '_',    '_',    '_',    '_',    '_',    '_',    '_',
	'_',    '_',    '_',    '_',    '_',    '_',    '_',    '_'
};

static unsigned OemCp850ToUnicode(unsigned char c)
{
	if( c < 0x80 )
		return c;

	return g_cp850_to_unicode[c - 0x80];
}

static unsigned char UnicodeToCp866(unsigned ch)
{
	if( ch < 0x80 )
		return (unsigned char)ch;

	if( ch >= 0x0410 && ch <= 0x043F )
		return (unsigned char)(0x80 + (ch - 0x0410));

	if( ch >= 0x0440 && ch <= 0x044F )
		return (unsigned char)(0xE0 + (ch - 0x0440));

	if( ch == 0x0401 ) return 0xF0;
	if( ch == 0x0451 ) return 0xF1;

	return '_';
}

static unsigned OemCp866ToUnicode(unsigned char c)
{
	if( c < 0x80 )
		return c;

	if( c >= 0x80 && c <= 0x8F )
		return 0x0410 + (c - 0x80);

	if( c >= 0x90 && c <= 0x9F )
		return 0x0420 + (c - 0x90);

	if( c >= 0xA0 && c <= 0xAF )
		return 0x0430 + (c - 0xA0);

	if( c >= 0xE0 && c <= 0xEF )
		return 0x0440 + (c - 0xE0);

	if( c == 0xF0 ) return 0x0401;
	if( c == 0xF1 ) return 0x0451;

	return '_';
}

static bool Utf8Next(const char **s, unsigned *ch)
{
	const unsigned char *p = (const unsigned char *)*s;

	if( *p == '\0' )
		return false;

	if( p[0] < 0x80 )
	{
		*ch = p[0];
		*s += 1;
		return true;
	}

	if( (p[0] & 0xE0) == 0xC0 && p[1] != 0 && (p[1] & 0xC0) == 0x80 )
	{
		*ch = ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F);
		*s += 2;
		return true;
	}

	if( (p[0] & 0xF0) == 0xE0 && p[1] != 0 && p[2] != 0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 )
	{
		*ch = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (unsigned)(p[2] & 0x3F);
		*s += 3;
		return true;
	}

	*ch = '_'; // bad
	*s += 1;
	return true;
}

void Utf8ToOem(char *dst, int dstSize, const char *src, OemCp cp)
{
	if( dst == NULL || dstSize <= 0 )
		return;

	while( dstSize > 1 && src != NULL && *src )
	{
		unsigned ch;

		if( !Utf8Next(&src, &ch) )
			break;

		switch( cp )
		{
		case OEM_CP850:
			ch = UnicodeToCp850(ch);
			break;

		case OEM_CP866:
			ch = UnicodeToCp866(ch);
			break;
		}

		*dst++ = (char)(unsigned char)ch;
		--dstSize;
	}

	*dst = '\0';
}

static bool Utf8Append(char *dst, int dstSize, int &pos, unsigned ch)
{
	if( dst == NULL || dstSize <= 0 )
		return false;

	if( ch < 0x80 )
	{
		if( pos + 1 >= dstSize )
			return false;

		dst[pos++] = (char)ch;
		return true;
	}

	if( ch < 0x800 )
	{
		if( pos + 2 >= dstSize )
			return false;

		dst[pos++] = (char)(0xC0 | (ch >> 6));
		dst[pos++] = (char)(0x80 | (ch & 0x3F));
		return true;
	}

	if( pos + 3 >= dstSize )
		return false;

	dst[pos++] = (char)(0xE0 | (ch >> 12));
	dst[pos++] = (char)(0x80 | ((ch >> 6) & 0x3F));
	dst[pos++] = (char)(0x80 | (ch & 0x3F));
	return true;
}

void OemToUtf8(char *dst, int dstSize, const char *src)
{
	if( dst == NULL || dstSize <= 0 )
		return;

	int pos = 0;

	while( src != NULL && *src )
	{
		unsigned char c = (unsigned char)*src++;
		unsigned ch;

		if( c < 0x80 )
			ch = c;
		else if( g_Encoding == OEM_CP866 )
			ch = OemCp866ToUnicode(c);
		else if( g_Encoding == OEM_CP850 )
			ch = OemCp850ToUnicode(c);
		else
			ch = '_';

		if( !Utf8Append(dst, dstSize, pos, ch) )
			break;
	}

	dst[pos] = '\0';
}

static void CountDetectedCodePageChars(const char *src, int &cp850, int &cp866)
{
	if( src == NULL )
		return;

	while( *src )
	{
		unsigned ch;

		if( !Utf8Next(&src, &ch) )
			break;

		if( ch < 0x80 )
			continue;

		if( UnicodeToCp850(ch) != '_' )
			cp850++;

		if( UnicodeToCp866(ch) != '_' )
			cp866++;
	}
}

static bool UpdateEncoding(const char *src)
{
	int cp850 = 0;
	int cp866 = 0;
	CountDetectedCodePageChars(src, cp850, cp866);

	if( cp866 > cp850 )
	{
		g_Encoding = OEM_CP866;
		return true;
	}
	if( cp850 > cp866 )
	{
		g_Encoding = OEM_CP850;
		return true;
	}
	if( cp850 == 0 && cp866 == 0 && strlen(src) > 256 )
	{
		g_Encoding = OEM_ASCII;
		return true;
	}

	return false;
}

static unsigned char ToUpperAsciiCh(unsigned char c)
{
	if( c >= 'a' && c <= 'z' )
		return (unsigned char)(c - 'a' + 'A');

	return c;
}

static unsigned char ToUpperCp866Ch(unsigned char c)
{
	if( c >= 'a' && c <= 'z' )
		return (unsigned char)(c - 'a' + 'A');
	if( c >= 0xA0 && c <= 0xAF )
		return (unsigned char)(c - 0x20);
	if( c >= 0xE0 && c <= 0xEF )
		return (unsigned char)(c - 0x50);
	if( c == 0xF1 )
		return 0xF0;

	return c;
}

static unsigned char ToUpperCp850Ch(unsigned char c)
{
	if( c >= 'a' && c <= 'z' )
		return (unsigned char)(c - 'a' + 'A');

	switch( c )
	{
		case 0x84: return 0x8E;
		case 0x81: return 0x9A;
		case 0x94: return 0x99;
		case 0x82: return 0x90;
		case 0xA0: return 0xB5;
		case 0x83: return 0xB6;
		case 0x85: return 0xB7;
		case 0x86: return 0x8F;
		case 0x91: return 0x92;
		case 0x87: return 0x80;
		case 0x8A: return 0xD4;
		case 0x88: return 0xD2;
		case 0x89: return 0xD3;
		case 0xA1: return 0xD6;
		case 0x8C: return 0xD7;
		case 0x8B: return 0xD8;
		case 0xA2: return 0xE0;
		case 0x93: return 0xE2;
		case 0xA4: return 0xE3;
		case 0xA3: return 0xE9;
		case 0x96: return 0xEA;
		default:
			break;
	}

	return c;
}

void StrToUpper(char *s)
{
	if( s == NULL )
		return;

	while( *s )
	{
		unsigned char c = (unsigned char)*s;

		switch( g_Encoding )
		{
		case OEM_CP866:
			c = ToUpperCp866Ch(c);
			break;

		case OEM_CP850:
			c = ToUpperCp850Ch(c);
			break;

		default:
			c = ToUpperAsciiCh(c);
			break;
		}

		*s++ = (char)c;
	}
}

static unsigned char ToLowerAsciiCh(unsigned char c)
{
	if( c >= 'A' && c <= 'Z' )
		return (unsigned char)(c - 'A' + 'a');

	return c;
}

static unsigned char ToLowerCp866Ch(unsigned char c)
{
	if( c >= 'A' && c <= 'Z' )
		return (unsigned char)(c - 'A' + 'a');
	if( c >= 0x80 && c <= 0x8F )
		return (unsigned char)(c + 0x20);
	if( c >= 0x90 && c <= 0x9F )
		return (unsigned char)(c + 0x50);
	if( c == 0xF0 )
		return 0xF1;

	return c;
}

static unsigned char ToLowerCp850Ch(unsigned char c)
{
	if( c >= 'A' && c <= 'Z' )
		return (unsigned char)(c - 'A' + 'a');

	switch( c )
	{
		case 0x8E: return 0x84;
		case 0x9A: return 0x81;
		case 0x99: return 0x94;
		case 0x90: return 0x82;
		case 0xB5: return 0xA0;
		case 0xB6: return 0x83;
		case 0xB7: return 0x85;
		case 0x8F: return 0x86;
		case 0x92: return 0x91;
		case 0x80: return 0x87;
		case 0xD4: return 0x8A;
		case 0xD2: return 0x88;
		case 0xD3: return 0x89;
		case 0xD6: return 0xA1;
		case 0xD7: return 0x8C;
		case 0xD8: return 0x8B;
		case 0xE0: return 0xA2;
		case 0xE2: return 0x93;
		case 0xE3: return 0xA4;
		case 0xE9: return 0xA3;
		case 0xEA: return 0x96;
		default:
			break;
	}

	return c;
}

void StrToLower(char *s)
{
	if( s == NULL )
		return;

	while( *s )
	{
		unsigned char c = (unsigned char)*s;

		switch( g_Encoding )
		{
		case OEM_CP866:
			c = ToLowerCp866Ch(c);
			break;

		case OEM_CP850:
			c = ToLowerCp850Ch(c);
			break;

		default:
			c = ToLowerAsciiCh(c);
			break;
		}

		*s++ = (char)c;
	}
}

static inline bool IsLikelyTextChar(unsigned char c)
{
	return ((c >= '0' && c <= '9') ||
	        (c >= 'A' && c <= 'Z') ||
	        (c >= 'a' && c <= 'z') ||
	        (c >= 0x80 && !(c >= 0xB0 && c <= 0xDF) && c < 0xF0));
}

static bool MakeDosName(char *dst, const char *src, bool fromUtf8)
{
	if( dst == NULL || src == NULL )
		return false;

	char tmp[160];
	const char *name = src;
	if( fromUtf8 )
	{
		Utf8ToOem(tmp, sizeof(tmp), src, g_Encoding);
		name = tmp;
	}
	
	const char *p;
	for( p = name; *p; ++p )
	{
		if( *p == '/' || *p == '\\' || *p == ':' )
			name = p + 1;
	}

	const char *dot = strrchr(name, '.');
	if( dot == name )
		dot = NULL;

	char base[9];
	char ext[4];
	int bi = 0;
	for( int i = 0; name[i] && &name[i] != dot && bi < 8; i++ )
	{
		unsigned char c = (unsigned char)name[i];

		if( IsLikelyTextChar(c) )
			base[bi++] = c;
		else if( c == '_' || c == '$' || c == '~' || c == '-' )
			base[bi++] = c;
	}

	if( bi == 0 )
		base[bi++] = 'F';
	base[bi] = '\0';

	int ei = 0;
	if( dot && dot[1] )
	{
		for( int i = 1; dot[i] && ei < 3; i++ )
		{
			char c = dot[i];
			if( IsLikelyTextChar(c) )
				ext[ei++] = c;
			else if( c == '_' || c == '$' || c == '~' || c == '-' )
				ext[ei++] = c;
		}
	}
	ext[ei] = '\0';

	strcpy(dst, base);
	if( ext[0] )
	{
		strcat(dst, ".");
		strcat(dst, ext);
	}

	StrToUpper(dst);

	return !IsReservedDosName(dst);
}

bool MakeRemoteName(char *dst, size_t dstSize, const char *src, bool fromOem)
{
	if( dst == NULL || dstSize == 0 || src == NULL )
		return false;

	char tmp[160];
	const char *name = src;

	if( fromOem )
	{
		OemToUtf8(tmp, sizeof(tmp), src);
		name = tmp;
	}

	const char *p;
	for( p = name; *p; ++p )
	{
		if( *p == '/' || *p == '\\' )
			name = p + 1;
	}

	if( *name == '\0' )
	{
		dst[0] = '\0';
		return false;
	}

	CopyTrunc(dst, dstSize, name);
	return dst[0] != '\0';
}
