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

#include <dos.h>

typedef enum
{
	OEM_ASCII,
	OEM_CP850,
	OEM_CP852,
	OEM_CP866
} OemCp;

OemCp g_encoding = OEM_CP866;

void InitDosCharset()
{
	union REGS r;
	r.x.ax = 0x6601;
	intdos(&r, &r);
	switch( r.x.bx )
	{	
	case 852:
		g_encoding = OEM_CP852;
		break;
	
	case 850:
		g_encoding = OEM_CP850;
		break;

	case 437:
		g_encoding = OEM_ASCII;
		break;
	
	default:
		g_encoding = OEM_CP866;		
	}
	printf("CP%d\n", r.x.bx);
}

void CountOemChars(const char *s, unsigned &cp850, unsigned &cp852, unsigned &cp866)
{
	static const unsigned long m850[7] = {
		0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0x00000000UL,
		0x00020000UL, 0x00000000UL, 0x00000000UL
	};

	static const unsigned long m852[7] = {
		0x09113991UL, 0xB4D86A96UL, 0x34D86A96UL, 0x0F03F0FCUL,
		0x66000000UL, 0xCF33019EUL, 0x7E03C03FUL
	};

	const unsigned char *p = (const unsigned char *)s;
	while( *p )
	{
	    unsigned c;
		unsigned char a = *p++;

		if( a < 0x80 )
			continue;

		if( (a & 0xE0) == 0xC0 && (p[0] & 0xC0) == 0x80 )
		{
			c = ((unsigned)(a & 0x1F) << 6) | (unsigned)(p[0] & 0x3F);
			p += 1;
		}
		else if( (a & 0xF0) == 0xE0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80 )
		{
			c = ((unsigned)(a & 0x0F) << 12) | ((unsigned)(p[0] & 0x3F) << 6) | (unsigned)(p[1] & 0x3F);
			p += 2;
		}
		else
		{
			while( (*p & 0xC0) == 0x80 )
				p++;
			continue;
		}

		if( (c >= 0x0410 && c <= 0x044F) || c == 0x0401 || c == 0x0451 )
			cp866++;

		if( c >= 0x00A0 && c <= 0x017F )
		{
			unsigned x = c - 0x00A0;
			unsigned long bit = 1UL << (x & 31);

			if(m850[x >> 5] & bit )
				cp850++;
			if(m852[x >> 5] & bit )
				cp852++;
		}
		else
		{
			if( c == 0x0192 || c == 0x2017 )
				cp850++;
			if( c == 0x02C7 || c == 0x02D8 || c == 0x02D9 || c == 0x02DB || c == 0x02DD )
				cp852++;
		}
	}
}

static OemCp UpdateEncoding(const char *src)
{
	unsigned cp850 = 0;
	unsigned cp852 = 0;
	unsigned cp866 = 0;
	CountOemChars(src, cp850, cp852, cp866);

	if( cp866 > cp850 && cp866 > cp852 )
		return OEM_CP866;
	if( cp852 > cp850 )
		return OEM_CP852;
	if( cp850 > 0 )
		return OEM_CP850;
	return OEM_ASCII;
}

static unsigned FoldDiacritic(unsigned ch)
{
	switch( ch )
	{
		case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
		case 0x00C4: case 0x00C5: case 0x0100: case 0x0102:
		case 0x0104: case 0x01CD:
			return 'A';
		case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3:
		case 0x00E4: case 0x00E5: case 0x0101: case 0x0103:
		case 0x0105: case 0x01CE:
			return 'a';

		case 0x00C7: case 0x0106: case 0x0108: case 0x010A: case 0x010C:
			return 'C';
		case 0x00E7: case 0x0107: case 0x0109: case 0x010B: case 0x010D:
			return 'c';

		case 0x010E: case 0x0110:
			return 'D';
		case 0x010F: case 0x0111:
			return 'd';

		case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB:
		case 0x0112: case 0x0114: case 0x0116: case 0x0118: case 0x011A:
			return 'E';
		case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB:
		case 0x0113: case 0x0115: case 0x0117: case 0x0119: case 0x011B:
			return 'e';

		case 0x011C: case 0x011E: case 0x0120: case 0x0122:
			return 'G';
		case 0x011D: case 0x011F: case 0x0121: case 0x0123:
			return 'g';

		case 0x0124: case 0x0126:
			return 'H';
		case 0x0125: case 0x0127:
			return 'h';

		case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF:
		case 0x0128: case 0x012A: case 0x012C: case 0x012E: case 0x0130:
			return 'I';
		case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
		case 0x0129: case 0x012B: case 0x012D: case 0x012F: case 0x0131:
			return 'i';

		case 0x0134:
			return 'J';
		case 0x0135:
			return 'j';

		case 0x0136:
			return 'K';
		case 0x0137: case 0x0138:
			return 'k';

		case 0x0139: case 0x013B: case 0x013D: case 0x013F: case 0x0141:
			return 'L';
		case 0x013A: case 0x013C: case 0x013E: case 0x0140: case 0x0142:
			return 'l';

		case 0x00D1: case 0x0143: case 0x0145: case 0x0147:
			return 'N';
		case 0x00F1: case 0x0144: case 0x0146: case 0x0148: case 0x0149:
			return 'n';

		case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
		case 0x00D6: case 0x00D8: case 0x014C: case 0x014E: case 0x0150:
			return 'O';
		case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5:
		case 0x00F6: case 0x00F8: case 0x014D: case 0x014F: case 0x0151:
			return 'o';

		case 0x0154: case 0x0156: case 0x0158:
			return 'R';
		case 0x0155: case 0x0157: case 0x0159:
			return 'r';

		case 0x015A: case 0x015C: case 0x015E: case 0x0160:
			return 'S';
		case 0x015B: case 0x015D: case 0x015F: case 0x0161:
			return 's';

		case 0x0162: case 0x0164: case 0x0166:
			return 'T';
		case 0x0163: case 0x0165: case 0x0167:
			return 't';

		case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC:
		case 0x0168: case 0x016A: case 0x016C: case 0x016E:
		case 0x0170: case 0x0172:
			return 'U';
		case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC:
		case 0x0169: case 0x016B: case 0x016D: case 0x016F:
		case 0x0171: case 0x0173:
			return 'u';

		case 0x00DD: case 0x0176: case 0x0178:
			return 'Y';
		case 0x00FD: case 0x00FF: case 0x0177:
			return 'y';

		case 0x0179: case 0x017B: case 0x017D:
			return 'Z';
		case 0x017A: case 0x017C: case 0x017E:
			return 'z';

		default:
			break;
	}

	return 0;
}

/// 850 ///
static const unsigned char g_cp850_a0_17f[0xE0] =
{
	0xFF, 0xAD, 0xBD, 0x9C, 0xCF, 0xBE, 0xDD, 0xF5,
	0xF9, 0xB8, 0xA6, 0xAE, 0xAA, 0xF0, 0xA9, 0xEE,
	0xF8, 0xF1, 0xFD, 0xFC, 0xEF, 0xE6, 0xF4, 0xFA,
	0xF7, 0xFB, 0xA7, 0xAF, 0xAC, 0xAB, 0xF3, 0xA8,
	0xB7, 0xB5, 0xB6, 0xC7, 0x8E, 0x8F, 0x92, 0x80,
	0xD4, 0x90, 0xD2, 0xD3, 0xDE, 0xD6, 0xD7, 0xD8,
	0xD1, 0xA5, 0xE3, 0xE0, 0xE2, 0xE5, 0x99, 0x9E,
	0x9D, 0xEB, 0xE9, 0xEA, 0x9A, 0xED, 0xE8, 0xE1,
	0x85, 0xA0, 0x83, 0xC6, 0x84, 0x86, 0x91, 0x87,
	0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B,
	0xD0, 0xA4, 0x95, 0xA2, 0x93, 0xE4, 0x94, 0xF6,
	0x9B, 0x97, 0xA3, 0x96, 0x81, 0xEC, 0xE7, 0x98,
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', 0xD5, '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_'
};

static unsigned char UnicodeToCp850(unsigned ch)
{
	if( ch < 0x80 )
		return (unsigned char)ch;

	if( ch >= 0x00A0 && ch <= 0x017F )
		return g_cp850_a0_17f[ch - 0x00A0];

	switch( ch )
	{
		case 0x0192: return 0x9F;
		case 0x2017: return 0xF2;
		default:
			break;
	}

	return '_';
}

static const uint16_t g_cp850_to_unicode[0x80] =
{
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
	0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0,
	0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
	0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE,
	0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580,
	0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE,
	0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4,
	0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8,
	0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0
};

static unsigned OemCp850ToUnicode(unsigned char c)
{
	if( c < 0x80 )
		return c;

	return g_cp850_to_unicode[c - 0x80];
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

/// 852 ///
static const unsigned char g_cp852_a0_17f[0xE0] =
{
	0xFF, '_', '_', '_', 0xCF, '_', '_', 0xF5,
	0xF9, '_', '_', 0xAE, 0xAA, 0xF0, '_', '_',
	0xF8, '_', '_', '_', 0xEF, '_', '_', '_',
	0xF7, '_', '_', 0xAF, '_', '_', '_', '_',
	'_', 0xB5, 0xB6, '_', 0x8E, '_', '_', 0x80,
	'_', 0x90, '_', 0xD3, '_', 0xD6, 0xD7, '_',
	'_', '_', '_', 0xE0, 0xE2, '_', 0x99, 0x9E,
	'_', '_', 0xE9, '_', 0x9A, 0xED, '_', 0xE1,
	'_', 0xA0, 0x83, '_', 0x84, '_', '_', 0x87,
	'_', 0x82, '_', 0x89, '_', 0xA1, 0x8C, '_',
	'_', '_', '_', 0xA2, 0x93, '_', 0x94, 0xF6,
	'_', '_', 0xA3, '_', 0x81, 0xEC, '_', '_',
	'_', '_', 0xC6, 0xC7, 0xA4, 0xA5, 0x8F, 0x86,
	'_', '_', '_', '_', 0xAC, 0x9F, 0xD2, 0xD4,
	0xD1, 0xD0, '_', '_', '_', '_', '_', '_',
	0xA8, 0xA9, 0xB7, 0xD8, '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', '_', '_', '_', '_', '_', '_', '_',
	'_', 0x91, 0x92, '_', '_', 0x95, 0x96, '_',
	'_', 0x9D, 0x88, 0xE3, 0xE4, '_', '_', 0xD5,
	0xE5, '_', '_', '_', '_', '_', '_', '_',
	0x8A, 0x8B, '_', '_', 0xE8, 0xEA, '_', '_',
	0xFC, 0xFD, 0x97, 0x98, '_', '_', 0xB8, 0xAD,
	0xE6, 0xE7, 0xDD, 0xEE, 0x9B, 0x9C, '_', '_',
	'_', '_', '_', '_', '_', '_', 0xDE, 0x85,
	0xEB, 0xFB, '_', '_', '_', '_', '_', '_',
	'_', 0x8D, 0xAB, 0xBD, 0xBE, 0xA6, 0xA7, '_'
};

static unsigned char UnicodeToCp852(unsigned ch)
{
	if( ch < 0x80 )
		return (unsigned char)ch;

	if( ch >= 0x00A0 && ch <= 0x017F )
		return g_cp852_a0_17f[ch - 0x00A0];

	switch( ch )
	{
		case 0x02DD: return 0xF1;
		case 0x02DB: return 0xF2;
		case 0x02C7: return 0xF3;
		case 0x02D8: return 0xF4;
		case 0x02D9: return 0xFA;
		default:
			break;
	}

	return '_';
}

static const uint16_t g_cp852_to_unicode[0x80] =
{
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x016F, 0x0107, 0x00E7,
	0x0142, 0x00EB, 0x0150, 0x0151, 0x00EE, 0x0179, 0x00C4, 0x0106,
	0x00C9, 0x0139, 0x013A, 0x00F4, 0x00F6, 0x013D, 0x013E, 0x015A,
	0x015B, 0x00D6, 0x00DC, 0x0164, 0x0165, 0x0141, 0x00D7, 0x010D,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x0104, 0x0105, 0x017D, 0x017E,
	0x0118, 0x0119, 0x00AC, 0x017A, 0x010C, 0x015F, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x011A,
	0x015E, 0x2563, 0x2551, 0x2557, 0x255D, 0x017B, 0x017C, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x0102, 0x0103,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
	0x0111, 0x0110, 0x010E, 0x00CB, 0x010F, 0x0147, 0x00CD, 0x00CE,
	0x011B, 0x2518, 0x250C, 0x2588, 0x2584, 0x0162, 0x016E, 0x2580,
	0x00D3, 0x00DF, 0x00D4, 0x0143, 0x0144, 0x0148, 0x0160, 0x0161,
	0x0154, 0x00DA, 0x0155, 0x0170, 0x00FD, 0x00DD, 0x0163, 0x00B4,
	0x00AD, 0x02DD, 0x02DB, 0x02C7, 0x02D8, 0x00A7, 0x00F7, 0x00B8,
	0x00B0, 0x00A8, 0x02D9, 0x0171, 0x0158, 0x0159, 0x25A0, 0x00A0
};

static unsigned OemCp852ToUnicode(unsigned char c)
{
	if( c < 0x80 )
		return c;

	return g_cp852_to_unicode[c - 0x80];
}

static unsigned char ToUpperCp852Ch(unsigned char c)
{
	if( c >= 'a' && c <= 'z' )
		return (unsigned char)(c - 'a' + 'A');

	switch( c )
	{
		case 0x87: return 0x80;
		case 0x81: return 0x9A;
		case 0x82: return 0x90;
		case 0x83: return 0xB6;
		case 0x84: return 0x8E;
		case 0x85: return 0xDE;
		case 0x86: return 0x8F;
		case 0x88: return 0x9D;
		case 0x89: return 0xD3;
		case 0x8B: return 0x8A;
		case 0x8C: return 0xD7;
		case 0x92: return 0x91;
		case 0x93: return 0xE2;
		case 0x94: return 0x99;
		case 0x96: return 0x95;
		case 0x98: return 0x97;
		case 0x9C: return 0x9B;
		case 0x9F: return 0xAC;
		case 0xA0: return 0xB5;
		case 0xA1: return 0xD6;
		case 0xA2: return 0xE0;
		case 0xA3: return 0xE9;
		case 0xA5: return 0xA4;
		case 0xA7: return 0xA6;
		case 0xA9: return 0xA8;
		case 0xAB: return 0x8D;
		case 0xAD: return 0xB8;
		case 0xBE: return 0xBD;
		case 0xC7: return 0xC6;
		case 0xD0: return 0xD1;
		case 0xD4: return 0xD2;
		case 0xD8: return 0xB7;
		case 0xE4: return 0xE3;
		case 0xE5: return 0xD5;
		case 0xE7: return 0xE6;
		case 0xEA: return 0xE8;
		case 0xEC: return 0xED;
		case 0xEE: return 0xDD;
		case 0xFB: return 0xEB;
		case 0xFD: return 0xFC;
		default:
			break;
	}

	return c;
}

static unsigned char ToLowerCp852Ch(unsigned char c)
{
	if( c >= 'A' && c <= 'Z' )
		return (unsigned char)(c - 'A' + 'a');

	switch( c )
	{
		case 0x80: return 0x87;
		case 0x9A: return 0x81;
		case 0x90: return 0x82;
		case 0xB6: return 0x83;
		case 0x8E: return 0x84;
		case 0xDE: return 0x85;
		case 0x8F: return 0x86;
		case 0x9D: return 0x88;
		case 0xD3: return 0x89;
		case 0x8A: return 0x8B;
		case 0xD7: return 0x8C;
		case 0x91: return 0x92;
		case 0xE2: return 0x93;
		case 0x99: return 0x94;
		case 0x95: return 0x96;
		case 0x97: return 0x98;
		case 0x9B: return 0x9C;
		case 0xAC: return 0x9F;
		case 0xB5: return 0xA0;
		case 0xD6: return 0xA1;
		case 0xE0: return 0xA2;
		case 0xE9: return 0xA3;
		case 0xA4: return 0xA5;
		case 0xA6: return 0xA7;
		case 0xA8: return 0xA9;
		case 0x8D: return 0xAB;
		case 0xB8: return 0xAD;
		case 0xBD: return 0xBE;
		case 0xC6: return 0xC7;
		case 0xD1: return 0xD0;
		case 0xD2: return 0xD4;
		case 0xB7: return 0xD8;
		case 0xE3: return 0xE4;
		case 0xD5: return 0xE5;
		case 0xE6: return 0xE7;
		case 0xE8: return 0xEA;
		case 0xED: return 0xEC;
		case 0xDD: return 0xEE;
		case 0xEB: return 0xFB;
		case 0xFC: return 0xFD;
		default:
			break;
	}

	return c;
}

/// 866 ///
static unsigned char UnicodeToCp866(unsigned ch)
{
	if( ch < 0x80 )
		return (unsigned char)ch;
	if( ch >= 0x0410 && ch <= 0x043F )
		return (unsigned char)(0x80 + (ch - 0x0410));
	if( ch >= 0x0440 && ch <= 0x044F )
		return (unsigned char)(0xE0 + (ch - 0x0440));
	if( ch == 0x0401 )
		return 0xF0;
	if( ch == 0x0451 )
		return 0xF1;

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
	if( c == 0xF0 )
		return 0x0401;
	if( c == 0xF1 )
		return 0x0451;

	return '_';
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

/// Utf ///
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

	if( (p[0] & 0xE0) == 0xC0 && p[1] != '\0' && (p[1] & 0xC0) == 0x80 )
	{
		*ch = ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F);
		*s += 2;
		return true;
	}

	if( (p[0] & 0xF0) == 0xE0 && p[1] != '\0' && p[2] != '\0' && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 )
	{
		*ch = ((unsigned)(p[0] & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (unsigned)(p[2] & 0x3F);
		*s += 3;
		return true;
	}

	*ch = '_'; // bad
	*s += 1;
	return true;
}

void Utf8ToOem(char *dst, size_t dstSize, const char *src, OemCp encoding)
{
	if( dst == NULL || dstSize == 0 )
		return;

	while( dstSize > 1 && src != NULL && *src )
	{
		unsigned ch;
		if( !Utf8Next(&src, &ch) )
			break;

		switch( encoding )
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
		}

		*dst++ = (char)(unsigned char)ch;
		--dstSize;
	}

	*dst = '\0';
}

static bool Utf8Append(char *dst, size_t dstSize, size_t &pos, unsigned ch)
{
	if( dst == NULL || dstSize == 0 )
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

void OemToUtf8(char *dst, size_t dstSize, const char *src, OemCp encoding)
{
	if( dst == NULL || dstSize == 0 )
		return;

	size_t pos = 0;
	while( src != NULL && *src )
	{
		unsigned char c = (unsigned char)*src++;
		unsigned ch;

		if( c < 0x80 )
			ch = c;
		else if( encoding == OEM_CP866 )
			ch = OemCp866ToUnicode(c);
		else if( encoding == OEM_CP852 )
			ch = OemCp852ToUnicode(c);
		else if( encoding == OEM_CP850 )
			ch = OemCp850ToUnicode(c);
		else
			ch = '_';

		if( !Utf8Append(dst, dstSize, pos, ch) )
			break;
	}

	dst[pos] = '\0';
}

unsigned char CharToUpper(unsigned char c, OemCp encoding)
{
	switch( encoding )
	{
	case OEM_CP866:
		return ToUpperCp866Ch(c);

	case OEM_CP852:
		return ToUpperCp852Ch(c);

	case OEM_CP850:
		return ToUpperCp850Ch(c);

	default:
		if( c >= 'a' && c <= 'z' )
			c = (unsigned char)(c - 'a' + 'A');
		return c;
	}
}

void StrToUpper(char *s, OemCp encoding)
{
	if( s == NULL )
		return;

	while( *s )
	{
		unsigned char c = (unsigned char)*s;
		*s++ = (char)CharToUpper(c, encoding);
	}
}

unsigned char CharToLower(unsigned char c, OemCp encoding)
{
	switch( encoding )
	{
	case OEM_CP866:
		return ToLowerCp866Ch(c);

	case OEM_CP852:
		return ToLowerCp852Ch(c);

	case OEM_CP850:
		return ToLowerCp850Ch(c);

	default:
		if( c >= 'A' && c <= 'Z' )
			c = (unsigned char)(c - 'A' + 'a');
		return c;
	}
}

void StrToLower(char *s, OemCp encoding)
{
	if( s == NULL )
		return;

	while( *s )
	{
		unsigned char c = (unsigned char)*s;
		*s++ = (char)CharToLower(c, encoding);
	}
}

// Dos names
bool MakeRemoteName(char *dst, size_t dstSize, const char *src)
{
	if( dst == NULL || dstSize == 0 || src == NULL )
		return false;

	const char *name = src;
	char tmp[160];
	if( !g_noUtf8Arg )
	{
		OemToUtf8(tmp, sizeof(tmp), src, g_encoding);
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

static bool IsGoodDosChar(unsigned char c, bool strict = false)
{
	if( strict )
	{
		return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= 0x80 && !(c >= 0xB0 && c <= 0xDF) && c < 0xF0) ||
				c == '_' || c == '~' || c == '-' ||
				c == '$' || c == '!' || c == '#' || c == '%' || c == '&' || c == '\'' ||
				c == '(' || c == ')' || c == '@' || c == '^' || c == '`' || c == '{' || c == '}';
	}

	return (c >= 0x20 && c <= 0x7E) || (c >= 0x80 && c < 0xF0);
}

static bool IsBadDosTailChar(unsigned char c)
{
	return c == '_' || c == '-' || c == '~' || c == '\'' || c == '(' || c == '`' || c == '{';
}

static bool AppendDosChar(char *dst, size_t dstSize, size_t &pos, unsigned ch)
{
	if( dst == NULL || dstSize == 0 )
		return false;

	if( ch < 0x80 )
	{
		unsigned char c = (unsigned char)ch;
		if( !IsGoodDosChar(c) )
			return true;

		if( pos >= dstSize - 1 )
			return false;

		dst[pos++] = (char)c;
		return true;
	}

	if( (ch >= 0x0410 && ch <= 0x044F) || ch == 0x0401 || ch == 0x0451 )
	{
		unsigned char c = UnicodeToCp866(ch);

		if( pos >= dstSize - 1 )
			return false;

		dst[pos++] = (char)c;
		return true;
	}

	unsigned folded_ch = FoldDiacritic(ch);
	if( folded_ch != 0 )
		return AppendDosChar(dst, dstSize, pos, folded_ch);

	if( ch == 0x00DF )
	{
		if( pos >= dstSize - 1 )
			return false;
		dst[pos++] = 's';

		if( pos >= dstSize - 1 )
			return false;
		dst[pos++] = 's';

		return true;
	}

	return true;
}

static void Utf8ToDosNameCharset(char *dst, size_t dstSize, const char *src)
{
	if( dst == NULL || dstSize == 0 )
		return;

	if( src == NULL )
	{
		dst[0] = '\0';
		return;
	}

	size_t pos = 0;
	while( *src && pos < dstSize - 1 )
	{
		unsigned ch;
		if( !Utf8Next(&src, &ch) )
			break;
		if( !AppendDosChar(dst, dstSize, pos, ch) )
			break;
	}
	dst[pos] = '\0';
}

static bool MakeDosName(char *dst, const char *src)
{
	if( dst == NULL || src == NULL )
		return false;

	const char *name = src;
	char tmp[160];
	if( !g_noUtf8Arg )
	{
		Utf8ToDosNameCharset(tmp, sizeof(tmp), src);
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
	int bi = 0;
	for( int i = 0; name[i] && &name[i] != dot && bi < 8; i++ )
	{
		if( IsGoodDosChar((unsigned char)name[i], true) )
			base[bi++] = name[i];
	}

	while( bi > 0 && IsBadDosTailChar((unsigned char)base[bi - 1]) )
		bi--;

	if( bi == 0 )
		base[bi++] = 'F';
	base[bi] = '\0';

	char ext[4];
	int ei = 0;
	if( dot && dot[1] )
	{
		for( int i = 1; dot[i] && ei < 3; i++ )
		{
			if( IsGoodDosChar((unsigned char)dot[i], true) )
				ext[ei++] = dot[i];
		}
	}
	ext[ei] = '\0';

	strcpy(dst, base);
	if( ext[0] )
	{
		strcat(dst, ".");
		strcat(dst, ext);
	}

	char *s = dst;
	while( *s )
	{
		unsigned char c = (unsigned char)*s;
		*s++ = (char)ToUpperCp866Ch(c);
	}

	return !IsReservedDosName(dst);
}
