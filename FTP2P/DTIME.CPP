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
	MERMERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FTP2P. If not, see <https://www.gnu.org/licenses/>.
*/

/*
	Store the date/time part of each directory row in exactly 4 bytes while
	preserving a printable canonical form for file-panel display and
	navigation.

	bits 31..28  pattern_id    4 bits = 15 patterns x 21 bytes
	bits 27..0   payload       28 bits

	Payload internals:
		TEXT    4 bits.  Text dictionary id 0..15.
		NUM32   5 bits.  Number 0..31.
		NUM60   6 bits.  Number 0..59.
		YEAR60  7 bits.  Year 1980..2107, stored as year-1980.

	Examples:
		"May 06 22:45"
		TEXT + NUM32 + NUM32 + NUM60
		4 + 5 + 5 + 6 = 20 bits

		"Apr 15  2006"
		TEXT + NUM32 + YEAR60
		4 + 5 + 6 = 15 bits

		"2017-04-10 22:00"
		YEAR60 + NUM32 + NUM32 + NUM32 + NUM60
		6 + 5 + 5 + 5 + 6 = 27 bits
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>

#define MAX_OPS           (12)
#define MAX_FIELDS        (6)
#define DATE_PAYLOAD_BITS (28)

struct DatePatternOp
{
	unsigned char v;

	static DatePatternOp MakeLiteralOp(char ch, size_t count)
	{
		DatePatternOp p;
		p.v = (unsigned char)(((LiteralCode(ch) & 7) << 4) | (count & 15));
		return p;
	}

	static DatePatternOp MakeFieldOp(size_t field_index)
	{
		DatePatternOp p;
		p.v = (unsigned char)(0x80 | (field_index & 7));
		return p;
	}

	bool IsField() const { return (v & 0x80) != 0; }
	size_t FieldIndex() const { return (size_t)(v & 7); }
	char LiteralCh() const { return LiteralFromCode((v >> 4) & 7); }
	size_t LiteralCount() const { return (size_t)(v & 15); }

	static int LiteralCode(char ch)
	{
		switch( ch )
		{
		case ' ': return 0;
		case '/': return 1;
		case '-': return 2;
		case ':': return 3;
		case '.': return 4;
		}
		return -1;
	}

	static char LiteralFromCode(int code)
	{
		switch( code )
		{
		case 0: return ' ';
		case 1: return '/';
		case 2: return '-';
		case 3: return ':';
		case 4: return '.';
		}
		return '?';
	}
};

bool operator==(DatePatternOp a, DatePatternOp b)
{
	return a.v == b.v;
}

bool operator!=(DatePatternOp a, DatePatternOp b)
{
	return a.v != b.v;
}

struct DatePatternField
{
	unsigned char v;

	static DatePatternField MakeFieldDesc(int type, size_t width, bool pad)
	{
		if( width > 15 )
			width = 0;

		DatePatternField pf;
		pf.v = (unsigned char)(((type & 7) << 5) | ((pad ? 1 : 0) << 4) | (width & 15));
		return pf;
	}

	int FieldDescType() const { return (v >> 5) & 7; }
	bool FieldDescPad() const { return ((v >> 4) & 1) != 0; }
	size_t FieldDescWidth() const { return (size_t)(v & 15); }
};

bool operator==(DatePatternField a, DatePatternField b)
{
	return a.v == b.v;
}

bool operator!=(DatePatternField a, DatePatternField b)
{
	return a.v != b.v;
}

enum FieldType
{
	FIELD_TEXT = 1,
	FIELD_NUM32 = 2,
	FIELD_NUM60 = 3,
	FIELD_YEAR128 = 4
};

class DatePattern
{
	friend bool operator==(const DatePattern &a, const DatePattern &b);

public:
	DatePattern()
	{
		Clear();
	}

	void Clear()
	{
		memset(this, 0, sizeof(*this));
	}

	DatePatternOp GetOp(size_t i) const { return _ops[i]; }
	unsigned char OpCount() const { return _opCount; }

	DatePatternField GetField(size_t i) const { return _fields[i]; }
	unsigned char FieldCount() const { return _fieldCount; }

	static unsigned char FieldBits(int type)
	{
		switch( type )
		{
		case FIELD_TEXT:    return 4;
		case FIELD_NUM32:   return 5;
		case FIELD_NUM60:   return 6;
		case FIELD_YEAR128: return 7;
		}
		return 0;
	}

	uint32_t PackPayload(const uint8_t *values) const
	{
		uint32_t payload = 0;
		for( int i = 0; i < _fieldCount; i++ )
		{
			unsigned char bits = FieldBits(_fields[i].FieldDescType());
			uint32_t mask = ((uint32_t)1 << bits) - 1;
			payload <<= bits;
			payload |= values[i] & mask;
		}
		return payload;
	}

	void FillValues(uint8_t *values, uint32_t payload) const
	{
		for( int i = _fieldCount - 1; i >= 0; i-- )
		{
			unsigned char bits = FieldBits(_fields[i].FieldDescType());
			uint32_t mask = ((uint32_t)1 << bits) - 1;
			values[i] = payload & mask;
			payload >>= bits;
		}
	}

	bool AddLiteral(const char *s, size_t len)
	{
		if( len == 0 )
			return true;

		char ch = s[0];
		for( size_t i = 1; i < len; i++ )
		{
			if( s[i] != ch )
				return false;
		}

		if( DatePatternOp::LiteralCode(ch) < 0 || len > 15 || _opCount >= MAX_OPS )
			return false;

		_ops[_opCount++] = DatePatternOp::MakeLiteralOp(ch, len);
		return true;
	}

	bool AddField(int type, size_t width, bool pad)
	{
		if( _opCount >= MAX_OPS || _fieldCount >= MAX_FIELDS || width > 15 )
			return false;

		unsigned char bits = FieldBits(type);
		if( bits == 0 || _payloadBits + bits > DATE_PAYLOAD_BITS )
			return false;

		_fields[_fieldCount] = DatePatternField::MakeFieldDesc(type, width, pad);
		_ops[_opCount++] = DatePatternOp::MakeFieldOp(_fieldCount);
		_fieldCount++;
		_payloadBits += bits;
		return true;
	}

private:
	unsigned char _payloadBits;

	DatePatternOp _ops[MAX_OPS];
	unsigned char _opCount;

	DatePatternField _fields[MAX_FIELDS];
	unsigned char _fieldCount;
};

bool operator==(const DatePattern &a, const DatePattern &b)
{
	if( a._opCount != b._opCount || a._fieldCount != b._fieldCount || a._payloadBits != b._payloadBits )
		return false;

	for( size_t i = 0; i < a._opCount; i++ )
	{
		DatePatternOp ao = a._ops[i];
		DatePatternOp bo = b._ops[i];
		if( ao.IsField() != bo.IsField() )
			return false;

		if( ao.IsField() )
		{
			if( ao.FieldIndex() != bo.FieldIndex() )
				return false;
		}
		else if( ao != bo )
			return false;
	}

	for( size_t i = 0; i < a._fieldCount; i++ )
	{
		if( a._fields[i] != b._fields[i] )
			return false;
	}

	return true;
}

static void AppendFiller(char *buf, size_t bufSize, size_t &pos, char c, size_t count)
{
	if( bufSize == 0 )
		return;

	size_t n = count;
	if( n > bufSize - 1 - pos )
		n = bufSize - 1 - pos;

	for( size_t i = 0; i < n; i++ )
		buf[pos++] = c;

	buf[pos] = '\0';
}

static void AppendString(char *buf, size_t bufSize, size_t &pos, const char *s)
{
	if( bufSize == 0 )
		return;

	while( *s && pos < bufSize - 1 )
		buf[pos++] = *s++;

	buf[pos] = '\0';
}

static void AppendNumber(char *buf, size_t bufSize, size_t &pos, int value, size_t width, bool pad0)
{
	if( bufSize == 0 || pos >= bufSize )
		return;

	int written = snprintf(buf + pos, bufSize - pos, pad0 ? "%0*d" : "%*d", (int)width, value);
	if( written > 0 )
	{
		size_t available = bufSize - 1 - pos;
		pos += (size_t)written < available ? (size_t)written : available;
	}
}

#define MAX_TEXTS     (16)
#define MAX_TEXT_POOL (160)
#define MAX_PATTERNS  (15)

struct DateTimeCodecContextMark
{
	unsigned char textCount;
	unsigned char textPoolUsed;
	unsigned char patternCount;
};

class DateTimeCodecContext
{
public:
	DateTimeCodecContext()
	{
		Clear();
	}

	void Clear()
	{
		_textCount = 0;
		_textPoolUsed = 0;
		memset(_textOffset, 0, sizeof(_textOffset));
		memset(_textPool, 0, sizeof(_textPool));

		for( int i = 0; i < MAX_PATTERNS; i++ )
			_patterns[i].Clear();

		_patternCount = 0;
	}

	DateTimeCodecContextMark Mark() const
	{
		DateTimeCodecContextMark m;
		m.textCount = _textCount;
		m.textPoolUsed = _textPoolUsed;
		m.patternCount = _patternCount;
		return m;
	}

	void Rollback(const DateTimeCodecContextMark &m)
	{
		_textCount = m.textCount;
		_textPoolUsed = m.textPoolUsed;
		_patternCount = m.patternCount;
	}

	unsigned char TextCount() const
	{
		return _textCount;
	}

	const char *GetText(int id) const
	{
		if( 0 <= id && id < _textCount )
			return _textPool + _textOffset[id];
		return "?";
	}

	int FindText(const char *s, size_t len) const
	{
		for( int i = 0; i < _textCount; i++ )
		{
			const char *p = _textPool + _textOffset[i];
			if( strlen(p) == len && memcmp(p, s, len) == 0 )
				return i;
		}
		return -1;
	}

	int AddText(const char *s, size_t len)
	{
		int id = FindText(s, len);
		if( id >= 0 )
			return id;

		if( _textCount >= MAX_TEXTS )
			return -1;

		if( _textPoolUsed + len + 1 > MAX_TEXT_POOL )
			return -1;

		id = _textCount++;
		_textOffset[id] = (unsigned char)_textPoolUsed;
		memcpy(_textPool + _textPoolUsed, s, len);
		_textPoolUsed += len;
		_textPool[_textPoolUsed++] = 0;
		return id;
	}

	unsigned char PatternCount() const
	{
		return _patternCount;
	}

	const DatePattern &GetPattern(int id) const
	{
		return _patterns[id];
	}

	int FindPattern(const DatePattern &p) const
	{
		for( int i = 0; i < _patternCount; i++ )
		{
			if( _patterns[i] == p )
				return i;
		}
		return -1;
	}

	int AddPattern(const DatePattern &p)
	{
		int id = FindPattern(p);
		if( id >= 0 )
			return id;

		if( _patternCount >= MAX_PATTERNS )
			return -1;

		id = _patternCount++;
		_patterns[id] = p;
		return id;
	}

private:
	unsigned char _textCount;
	unsigned char _textPoolUsed;
	unsigned char _textOffset[MAX_TEXTS];
	char _textPool[MAX_TEXT_POOL];

	DatePattern _patterns[MAX_PATTERNS];
	unsigned char _patternCount;
};

#define MAX_ITEMS                (12)
#define MAX_TOKEN                (16)
#define DATE_PATTERN_UNSUPPORTED (15)
#define DATE_PAYLOAD_MASK        (0x0FFFFFFFUL)

class DateTimeCodec
{
public:
	DateTimeCodec();

	void Clear();
	uint32_t Encode(const char *s);
	bool Decode(char *dst, size_t dstSize, uint32_t dt) const;

private:
	enum ItemType
	{
		ITEM_LITERAL = 1,
		ITEM_TEXT = 2,
		ITEM_NUMBER = 3
	};

	struct Item
	{
		unsigned char type;
		unsigned char len;
		char text[MAX_TOKEN];
		uint16_t value;

		bool IsColonLiteral() const
		{
			return type == ITEM_LITERAL && len == 1 && text[0] == ':';
		}
	};

	DateTimeCodecContext _ctx;

	static bool ParseInt(const char *s, size_t len, uint16_t &value);
	static size_t Tokenize(Item *items, size_t itemCapacity, const char *s);
	static int ClassifyNumber(const Item *items, size_t index);
	static uint32_t MakeToken(int pattern_id, uint32_t payload);

	bool Build(const char *s, DatePattern &p, uint8_t *values);
};

DateTimeCodec::DateTimeCodec()
{
	Clear();
}

void DateTimeCodec::Clear()
{
	_ctx.Clear();
}

uint32_t DateTimeCodec::MakeToken(int pattern_id, uint32_t payload)
{
	return ((uint32_t)(pattern_id & 15) << DATE_PAYLOAD_BITS) | (payload & DATE_PAYLOAD_MASK);
}

uint32_t DateTimeCodec::Encode(const char *s)
{
	if( s == NULL || *s == '\0' )
		return MakeToken(DATE_PATTERN_UNSUPPORTED, 0UL);

	DateTimeCodecContextMark mark = _ctx.Mark();
	DatePattern pattern;

	uint8_t values[MAX_FIELDS];
	memset(values, 0, sizeof(values));

	if( !Build(s, pattern, values) )
	{
		_ctx.Rollback(mark);
		return MakeToken(DATE_PATTERN_UNSUPPORTED, 0UL);
	}

	int pid = _ctx.AddPattern(pattern);
	if( pid < 0 )
	{
		_ctx.Rollback(mark);
		return MakeToken(DATE_PATTERN_UNSUPPORTED, 0UL);
	}

	uint32_t payload = pattern.PackPayload(values);
	return MakeToken(pid, payload);
}

bool DateTimeCodec::Decode(char *dst, size_t dstSize, uint32_t dt) const
{
	if( dstSize == 0 )
		return false;

	dst[0] = '\0';

	int pid = (int)((dt >> DATE_PAYLOAD_BITS) & 15U);
	uint32_t payload = dt & DATE_PAYLOAD_MASK;

	if( pid == DATE_PATTERN_UNSUPPORTED )
	{
		strncpy(dst, "???", dstSize - 1);
		dst[dstSize - 1] = '\0';
		return false;
	}

	if( pid < 0 || pid >= _ctx.PatternCount() )
		return false;

	uint8_t values[MAX_FIELDS];
	memset(values, 0, sizeof(values));

	const DatePattern &p = _ctx.GetPattern(pid);
	p.FillValues(values, payload);

	size_t pos = 0;
	for( size_t i = 0; i < p.OpCount(); i++ )
	{
		DatePatternOp op = p.GetOp(i);
		if( !op.IsField() )
		{
			AppendFiller(dst, dstSize, pos, op.LiteralCh(), op.LiteralCount());
		}
		else
		{
			size_t fi = op.FieldIndex();
			if( fi >= p.FieldCount() || fi >= MAX_FIELDS )
				return false;

			int value = (int)values[fi];

			DatePatternField f = p.GetField(fi);
			int type = f.FieldDescType();
			size_t width = f.FieldDescWidth();
			bool pad0 = f.FieldDescPad();

			if( type == FIELD_TEXT )
				AppendString(dst, dstSize, pos, _ctx.GetText(value));
			else if( type == FIELD_YEAR128 )
				AppendNumber(dst, dstSize, pos, value + 1980, width, pad0);
			else
				AppendNumber(dst, dstSize, pos, value, width, pad0);
		}
	}

	return true;
}

bool DateTimeCodec::ParseInt(const char *s, size_t len, uint16_t &value)
{
	int v = 0;
	for( size_t i = 0; i < len; i++ )
	{
		int digit = s[i] - '0';
		if( v > (INT_MAX - digit) / 10 )
			return false;
		v = v * 10 + digit;
	}
	value = v;
	return true;
}

size_t DateTimeCodec::Tokenize(Item *items, size_t itemCapacity, const char *s)
{
	size_t count = 0;
	size_t i = 0;
	size_t len = strlen(s);

	while( i < len )
	{
		if( count >= itemCapacity )
			return 0;

		Item &it = items[count];

		if( isalpha((unsigned char)s[i]) )
		{
			it.type = ITEM_TEXT;
			size_t j = 0;
			while( i < len && isalpha((unsigned char)s[i]) )
			{
				if( j >= MAX_TOKEN - 1 )
					return 0;
				it.text[j++] = s[i++];
			}
			it.text[j] = '\0';
			it.len = (unsigned char)j;
			count++;
		}
		else if( isdigit((unsigned char)s[i]) )
		{
			it.type = ITEM_NUMBER;
			size_t j = 0;
			while( i < len && isdigit((unsigned char)s[i]) )
			{
				if( j >= MAX_TOKEN - 1 )
					return 0;
				it.text[j++] = s[i++];
			}
			it.text[j] = '\0';
			it.len = (unsigned char)j;
			if( !ParseInt(it.text, it.len, it.value) )
				return 0;
			count++;
		}
		else
		{
			it.type = ITEM_LITERAL;
			size_t j = 0;
			while( i < len && !isalpha((unsigned char)s[i]) && !isdigit((unsigned char)s[i]) )
			{
				if( j >= MAX_TOKEN - 1 )
					return 0;
				it.text[j++] = s[i++];
			}
			it.text[j] = '\0';
			it.len = (unsigned char)j;
			count++;
		}
	}

	return count;
}

int DateTimeCodec::ClassifyNumber(const Item *items, size_t index)
{
	const Item &it = items[index];
	int v = it.value;

	if( it.len == 4 && 1980 <= v && v <= 2107 )
		return FIELD_YEAR128;

	if( index > 0 && items[index-1].IsColonLiteral() )
	{
		if( v < 0 || v > 59 )
			return 0;
		return FIELD_NUM60;
	}

	if( 0 <= v && v <= 31 )
		return FIELD_NUM32;

	return 0;
}

bool DateTimeCodec::Build(const char *s, DatePattern &p, uint8_t *values)
{
	Item items[MAX_ITEMS];
	memset(items, 0, sizeof(items));

	size_t item_count = Tokenize(items, MAX_ITEMS, s);
	for( size_t i = 0; i < item_count; i++ )
	{
		const Item &it = items[i];

		if( it.type == ITEM_LITERAL )
		{
			if( !p.AddLiteral(it.text, it.len) )
				return false;
		}
		else if( it.type == ITEM_TEXT )
		{
			int text_id = _ctx.AddText(it.text, it.len);
			if( text_id < 0 )
				return false;

			if( !p.AddField(FIELD_TEXT, 0, false) )
				return false;
			
			values[p.FieldCount() - 1] = (uint8_t)text_id;
		}
		else if( it.type == ITEM_NUMBER )
		{
			int type = ClassifyNumber(items, i);
			if( type == 0 )
				return false;

			size_t width = it.len;
			bool pad0 = false;
			if( type == FIELD_YEAR128 )
			{
				width = 4;
				pad0 = true;
			}
			else if( type == FIELD_NUM60 )
			{
				pad0 = it.len == 2;
			}
			else if( type == FIELD_NUM32 )
			{
				pad0 = it.len == 2 && it.text[0] == '0';
			}

			if( !p.AddField(type, width, pad0) )
				return false;

			int value = it.value;
			if( type == FIELD_YEAR128 )
				value -= 1980;

			values[p.FieldCount() - 1] = (uint8_t)value;
		}
	}

	return p.FieldCount() != 0;
}