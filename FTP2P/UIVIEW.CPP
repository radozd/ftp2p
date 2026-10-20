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

static const char *TMP_VIEW = "ftp2p.tmp";

class FileFormatter
{
public:
	FileFormatter(const char *srcName, int width, int height);
	~FileFormatter();

	bool PreprocessFile(bool html);
	long NextLine();
	long PrevLine();
	long Home();
	long End();
	long PgUp();
	long PgDn();
	long GetLine(long pos, char *buf, size_t bufSize) const;

	long GetPos() const { return _pos; }
	long GetSize() const { return _fsize; }

private:
	enum
	{
		TEXT_UNKNOWN,
		TEXT_UTF8,
		TEXT_OEM
	};

	FILE *_f;
	int _width;
	int _height;
	long _fsize;
	long _pos;
	int _nls;
	int _col;
	bool _ready;
	char _srcName[260];

	int _textEncoding;
	int _utf8Len;
	unsigned char _utf8Buf[3];

	void CloseFile();
	bool OpenPreparedFile();
	long FindPrevLine(long pos) const;

	bool PutRawCh(int ch);
	bool PutSimpleCh(int ch);
	bool PutUnicodeCh(unsigned ch);
	bool FlushUtf8AsOem();
	bool PutCh(int ch);
	bool FlushText();

	bool PreprocessTextStream(FILE *src);
	int ReadCh(FILE *f);
	bool TagNameEq(const char *tag, const char *name);
	bool PreprocessHtmlStream(FILE *f);
};

FileFormatter::FileFormatter(const char *srcName, int width, int height)
{
	_f = NULL;
	_width = width;
	_height = height;
	_fsize = 0;
	_pos = 0;
	_nls = 0;
	_col = 0;
	_ready = false;

	_textEncoding = TEXT_UNKNOWN;
	_utf8Len = 0;

	CopyTrunc(_srcName, sizeof(_srcName), srcName);
}

FileFormatter::~FileFormatter()
{
	CloseFile();
	remove(TMP_VIEW);
}

void FileFormatter::CloseFile()
{
	if( _f != NULL )
	{
		fclose(_f);
		_f = NULL;
	}

	_fsize = 0;
	_pos = 0;
	_ready = false;
}

bool FileFormatter::OpenPreparedFile()
{
	CloseFile();

	_f = fopen(TMP_VIEW, "rb");
	if( _f == NULL )
		return false;

	fseek(_f, 0L, SEEK_END);
	_fsize = ftell(_f);
	if( _fsize < 0 )
		_fsize = 0;

	fseek(_f, 0L, SEEK_SET);
	_pos = 0;
	_ready = true;

	return true;
}

bool FileFormatter::PreprocessFile(bool html)
{
	if( _srcName[0] == '\0' )
		return false;

	CloseFile();
	remove(TMP_VIEW);

	FILE *src = fopen(_srcName, "rb");
	if( src == NULL )
		return false;

	FILE *dst = fopen(TMP_VIEW, "wb");
	if( dst == NULL )
	{
		fclose(src);
		return false;
	}

	_f = dst;
	_nls = 0;
	_col = 0;
	_textEncoding = g_noUtf8Arg ? TEXT_OEM : TEXT_UNKNOWN;
	_utf8Len = 0;

	bool ok = !html ? PreprocessTextStream(src) : PreprocessHtmlStream(src);

	fclose(src);
	fclose(dst);
	_f = NULL;

	if( !ok )
	{
		remove(TMP_VIEW);
		return false;
	}

	return OpenPreparedFile();
}

long FileFormatter::NextLine()
{
	if( !_ready )
		return _pos;

	if( _pos >= _fsize )
		return _fsize;

	fseek(_f, _pos, SEEK_SET);

	int col = 0;
	while( _width <= 0 || col < _width )
	{
		int ch = fgetc(_f);
		if( ch == EOF || ch == '\n' )
			break;

		col++;
	}

	if( _width > 0 && col >= _width )
	{
		int ch = fgetc(_f);
		if( ch != EOF && ch != '\n' )
			fseek(_f, -1L, SEEK_CUR);
	}

	_pos = ftell(_f);
	if( _pos < 0 )
		_pos = 0;
	if( _pos > _fsize )
		_pos = _fsize;

	return _pos;
}

long FileFormatter::FindPrevLine(long pos) const
{
	if( pos <= 0 )
		return 0;
	if( pos > _fsize )
		pos = _fsize;

	long probe = pos - ((_width > 0 ? _width : 1) + 1);
	if( probe < 0 )
		probe = 0;

	long cur = probe;
	while( cur < pos )
	{
		long next;

		fseek(_f, cur, SEEK_SET);

		int col = 0;
		while( _width <= 0 || col < _width )
		{
			int ch = fgetc(_f);
			if( ch == EOF || ch == '\n' )
				break;

			col++;
		}

		if( _width > 0 && col >= _width )
		{
			int ch = fgetc(_f);
			if( ch != EOF && ch != '\n' )
				fseek(_f, -1L, SEEK_CUR);
		}

		next = ftell(_f);
		if( next < 0 )
			next = 0;
		if( next > _fsize )
			next = _fsize;

		if( next >= pos )
			break;
		if( next <= cur )
			break;

		cur = next;
	}

	return cur;
}

long FileFormatter::PrevLine()
{
	if( !_ready )
		return _pos;

	_pos = FindPrevLine(_pos);
	return _pos;
}

long FileFormatter::Home()
{
	_pos = 0;
	return _pos;
}

long FileFormatter::End()
{
	if( !_ready )
		return _pos;

	_pos = _fsize;
	for( int i = 0; i < _height; i++ )
	{
		long prev = FindPrevLine(_pos);
		if( prev >= _pos )
			break;
		
		_pos = prev;
		if( _pos <= 0 )
		{
			_pos = 0;
			break;
		}
	}

	return _pos;
}

long FileFormatter::PgUp()
{
	if( !_ready )
		return _pos;

	for( int i = 0; i < _height; i++ )
	{
		long prev = FindPrevLine(_pos);
		if( prev >= _pos )
			break;
		_pos = prev;
		if( _pos <= 0 )
		{
			_pos = 0;
			break;
		}
	}

	return _pos;
}

long FileFormatter::PgDn()
{
	if( !_ready )
		return _pos;

	for( int i = 0; i < _height; i++ )
	{
		long cur = _pos;
		NextLine();
		if( _pos <= cur )
			break;
		if( _pos >= _fsize )
		{
			_pos = _fsize;
			break;
		}
	}

	return _pos;
}

long FileFormatter::GetLine(long pos, char *buf, size_t bufSize) const
{
	if( buf == NULL || bufSize == 0 )
		return pos;

	buf[0] = '\0';

	if( !_ready )
		return pos;

	if( pos < 0 )
		pos = 0;
	if( pos >= _fsize )
		return _fsize;

	fseek(_f, pos, SEEK_SET);

	int col = 0;
	int ch = EOF;
	while( col < bufSize - 1 && (_width <= 0 || col < _width) )
	{
		ch = fgetc(_f);
		if( ch == EOF || ch == '\n' )
			break;

		buf[col++] = (char)ch;
	}

	buf[col] = '\0';

	if( ch != '\n' && ch != EOF && _width > 0 && col >= _width )
	{
		ch = fgetc(_f);
		if( ch != EOF && ch != '\n' )
			fseek(_f, -1L, SEEK_CUR);

	}

	pos = ftell(_f);
	if( pos < 0 )
		pos = 0;
	if( pos > _fsize )
		pos = _fsize;

	return pos;
}

bool FileFormatter::PutRawCh(int ch)
{
	if( ch == '\n' )
	{
		if( _nls >= 2 )
			return true;

		_nls++;
	}
	else
		_nls = 0;

	return fputc(ch, _f) != EOF;
}

bool FileFormatter::PutSimpleCh(int ch)
{
	if( ch != '\n' && _width > 0 && _col >= _width )
	{
		if( !PutRawCh('\n') )
			return false;

		_col = 0;
	}

	if( !PutRawCh(ch) )
		return false;

	if( ch == '\n' )
		_col = 0;
	else
		_col++;

	return true;
}

bool FileFormatter::PutUnicodeCh(unsigned ch)
{
	if( ch == 0xFEFF )
		return true;

	if( g_simpleUtf8 )
	{
		char buf[4];
		size_t pos = 0;
		if( !AppendDosChar(buf, sizeof(buf), pos, ch) )
			return false;

		for( size_t i = 0; i < pos; i++ )
		{
			if( !PutSimpleCh((unsigned char)buf[i]) )
				return false;
		}
		return true;
	}

	switch( g_encoding )
	{
	case OEM_CP850:
		ch = UnicodeToCp850(ch);
		break;

	case OEM_CP852:
		ch = UnicodeToCp852(ch);
		break;

	case OEM_CP866:
		ch = UnicodeToCp866(ch);
		break;

	default:
		break;
	}

	return PutSimpleCh((unsigned char)ch);
}

bool FileFormatter::FlushUtf8AsOem()
{
	int n = _utf8Len;
	_utf8Len = 0;

	for( int i = 0; i < n; i++ )
	{
		if( !PutSimpleCh(_utf8Buf[i]) )
			return false;
	}
	return true;
}

bool FileFormatter::PutCh(int ch)
{
	if( _textEncoding == TEXT_OEM )
		return PutSimpleCh(ch);

	if( _utf8Len == 0 )
	{
		if( ch < 0x80 )
			return PutSimpleCh(ch);

		if( (ch >= 0xC2 && ch <= 0xDF) || (ch >= 0xE0 && ch <= 0xEF) )
		{
			_utf8Buf[0] = (unsigned char)ch;
			_utf8Len = 1;
			return true;
		}

		if( _textEncoding == TEXT_UNKNOWN )
		{
			_textEncoding = TEXT_OEM;
			return PutSimpleCh(ch);
		}

		return PutSimpleCh('_');
	}

	if( (ch & 0xC0) != 0x80 )
	{
		if( _textEncoding == TEXT_UNKNOWN )
		{
			_textEncoding = TEXT_OEM;
			if( !FlushUtf8AsOem() )
				return false;
			return PutSimpleCh(ch);
		}

		_utf8Len = 0;
		if( !PutSimpleCh('_') )
			return false;

		return PutCh(ch);
	}

	_utf8Buf[_utf8Len++] = (unsigned char)ch;
	int n = _utf8Buf[0] < 0xE0 ? 2 : 3;
	if( _utf8Len < n )
		return true;

	unsigned unicode;
	int used = DecodeUtf8Char(_utf8Buf, _utf8Len, &unicode);

	if( _textEncoding == TEXT_UNKNOWN && (used == 0 || !IsUtf8Char(unicode)) )
	{
		_textEncoding = TEXT_OEM;
		return FlushUtf8AsOem();
	}

	_utf8Len = 0;
	if( used == 0 )
		return PutSimpleCh('_');

	_textEncoding = TEXT_UTF8;
	return PutUnicodeCh(unicode);
}

bool FileFormatter::FlushText()
{
	if( _utf8Len == 0 )
		return true;

	if( _textEncoding == TEXT_UNKNOWN )
	{
		_textEncoding = TEXT_OEM;
		return FlushUtf8AsOem();
	}

	_utf8Len = 0;
	return PutSimpleCh('_');
}

int FileFormatter::ReadCh(FILE *f)
{
	int ch;

	while( true )
	{
		ch = fgetc(f);
		if( ch != '\r' )
			break;
	}

	if( ch == '\t' )
		ch = ' ';

	return ch;
}

bool FileFormatter::PreprocessTextStream(FILE *src)
{
	int pending_spaces = 0;

	while( true )
	{
		int ch = ReadCh(src);
		if( ch == EOF )
			break;

		if( ch == ' ' )
		{
			pending_spaces++;
			continue;
		}

		if( ch == '\n' )
		{
			pending_spaces = 0;

			if( !PutCh('\n') )
				return false;

			continue;
		}

		while( pending_spaces > 0 )
		{
			if( !PutCh(' ') )
				return false;
			pending_spaces--;
		}

		if( !PutCh(ch) )
			return false;
	}

	return !ferror(src) && FlushText();
}

bool FileFormatter::TagNameEq(const char *tag, const char *name)
{
	while( *tag && *name )
	{
		if( tolower((unsigned char)*tag) != tolower((unsigned char)*name) )
			return false;

		tag++;
		name++;
	}

	return *tag == '\0' && *name == '\0';
}

bool FileFormatter::PreprocessHtmlStream(FILE *f)
{
	bool have_body = false;
	bool in_body = false;
	bool have_text = false;
	bool pending_space = false;

	while( true )
	{
		int ch = ReadCh(f);
		if( ch == EOF )
			break;

		if( ch == '<' )
		{
			if( !FlushText() )
				return false;

			char tag[16];
			int n = 0;
			bool closing = false;

			ch = ReadCh(f);
			while( ch == ' ' || ch == '\n' )
				ch = ReadCh(f);

			if( ch == '/' )
			{
				closing = true;
				ch = ReadCh(f);

				while( ch == ' ' || ch == '\n' )
					ch = ReadCh(f);
			}

			while( ch != EOF )
			{
				if( ch == '>' || ch == '/' || ch == ' ' || ch == '\n' )
					break;

				if( n < (int)sizeof(tag) - 1 )
					tag[n++] = (char)ch;

				ch = ReadCh(f);
			}
			tag[n] = '\0';

			if( TagNameEq(tag, "body") )
			{
				have_body = true;

				if( closing )
					break;

				in_body = true;
			}
			else if( !closing )
			{
				if( (!have_body || in_body) && TagNameEq(tag, "br") )
				{
					if( have_text && !PutCh('\n') )
						return false;
					pending_space = false;
				}
			}
			else
			{
				if( !have_body || in_body )
				{
					if( TagNameEq(tag, "p") || TagNameEq(tag, "div") || TagNameEq(tag, "tr") ||
						TagNameEq(tag, "li") || TagNameEq(tag, "ul") || TagNameEq(tag, "ol") ||
						TagNameEq(tag, "table") ||
						TagNameEq(tag, "h1") || TagNameEq(tag, "h2") || TagNameEq(tag, "h3") ||
						TagNameEq(tag, "h4") || TagNameEq(tag, "h5") || TagNameEq(tag, "h6") )
					{
						if( have_text && !PutCh('\n') )
							return false;
						pending_space = false;
					}
				}
			}

			while( ch != EOF && ch != '>' )
				ch = ReadCh(f);

			continue;
		}

		if( have_body && !in_body )
			continue;

		if( ch == '&' )
		{
			if( !FlushText() )
				return false;

			char ent[8];
			int n = 0;

			while( true )
			{
				ch = ReadCh(f);
				if( ch == EOF )
					break;

				if( n < (int)sizeof(ent) - 1 )
					ent[n++] = (char)ch;

				if( ch == ';' || ch == ' ' || ch == '\n' || ch == '<' || ch == '&' )
					break;
			}
			ent[n] = '\0';

			if( strcmp(ent, "nbsp;") == 0 )
				ch = ' ';
			else if( strcmp(ent, "lt;") == 0 )
				ch = '<';
			else if( strcmp(ent, "gt;") == 0 )
				ch = '>';
			else if( strcmp(ent, "amp;") == 0 )
				ch = '&';
			else if( strcmp(ent, "quot;") == 0 )
				ch = '"';
			else
				continue;
		}

		if( ch == ' ' || ch == '\n' )
		{
			if( have_text )
				pending_space = true;
			continue;
		}

		if( pending_space && have_text )
		{
			if( !PutCh(' ') )
				return false;
		}
		pending_space = false;

		if( !PutCh(ch) )
			return false;

		have_text = true;
	}

	return !ferror(f) && FlushText();
}
