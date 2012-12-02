/**
 * @file Cosa/Ciao.cpp
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2012, Mikael Patel
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * @section Description
 * The Cosa Ciao data stream handler. Please see CIAO.txt for details.
 *
 * This file is part of the Arduino Che Cosa project.
 */

#include "Cosa/Ciao.h"
#include "Cosa/Trace.h"
#include <avr/pgmspace.h>

// Ciao configuration
static const uint16_t ID = 0x00;
static char MAGIC[] = "Cosa::Ciao";
static const uint8_t MAJOR = 1;
static const uint8_t MINOR = 0;

// Ciao header declaration 
static const char magic_name[] PROGMEM = "magic";
static const char major_name[] PROGMEM = "major";
static const char minor_name[] PROGMEM = "minor";
static const char endian_name[] PROGMEM = "endian";
static const Ciao::decl_member_t member[] PROGMEM = {
  {
    Ciao::UINT8_TYPE,		// type
    0,				// count
    magic_name,			// name
    0				// decl
  },
  {
    Ciao::UINT8_TYPE,		// type
    1,				// count
    major_name,			// name
    0				// decl
  },
  {
    Ciao::UINT8_TYPE,		// type
    1,				// count
    minor_name,			// name
    0				// decl
  },
  {
    Ciao::UINT8_TYPE,		// type
    1,				// count
    endian_name,		// name
    0				// decl
  }
};
static const char header_name[] PROGMEM = "Ciao::header_t";
const Ciao::decl_user_t Ciao::_header_decl PROGMEM = {
  ID,				// id
  header_name,			// name
  member,			// member
  membersof(member)		// count
};  

// Ciao header with version string and endian information
Ciao::header_t Ciao::_header = {
  MAGIC,
  MAJOR,
  MINOR,
  LITTLE_ENDIAN
};

void
Ciao::write(uint8_t type, uint16_t count)
{
  // Tag byte contains count[0..7]
  if (count < 8) {
    type |= count;
    _dev->putchar(type);
    return;
  }

  // Tag byte contains marker. Succeeding byte counter[8..255]
  if (count < 256) {
    _dev->putchar(type | COUNT8_ATTR);
    _dev->putchar(count);
    return;
  }
  
  // Tag byte contains marker. Succeeding two bytes counter[255..64K]
  _dev->putchar(type | COUNT16_ATTR);
  _dev->write(&count, sizeof(count));
}

void 
Ciao::write(const decl_user_t* decl)
{
  // Read declaration from program memory
  decl_user_t d;
  memcpy_P(&d, decl, sizeof(d));
  
  // Write declaration start tag and identity number (8 or 16-bit)
  if (d.id < 256) {
    _dev->putchar(USER8_DECL_START);
    _dev->putchar(d.id);
  }
  else {
    _dev->putchar(USER16_DECL_START);
    _dev->write(&d.id, sizeof(uint16_t));
  }

  // Write declaration name null terminated
  _dev->puts_P(d.name);
  _dev->putchar(0);
  
  // Write members with name null terminated
  const decl_member_t* mp = d.member;
  for (uint16_t i = 0; i < d.count; i++) {
    decl_member_t m;
    memcpy_P(&m, mp++, sizeof(m));
    write(m.type, m.count);
    _dev->puts_P(m.name);
    _dev->putchar(0);
  }
	 
  // Write declaration end tag
  if (d.id < 256) {
    _dev->putchar(USER8_DECL_END);
  }
  else {
    _dev->putchar(USER16_DECL_END);
  }
}

static const uint8_t sizeoftype[] PROGMEM = {
  1,
  2,
  4,
  8, 
  0, 
  0, 
  0, 
  0, 
  1, 
  2, 
  4, 
  8, 
  2, 
  4, 
  8, 
  10
};

void 
Ciao::write(const decl_user_t* decl, void* buf, uint16_t count)
{
  // Read declaration from program memory
  decl_user_t d;
  memcpy_P(&d, decl, sizeof(d));
  
  // Write type tag for user data with count and type identity
  if (d.id < 256) {
    write(USER8_TYPE, count);
    _dev->putchar(d.id);
  }
  else {
    write(USER16_TYPE, count);
    _dev->write(&d.id, sizeof(d.id));
  }

  // Write data buffer to stream
  uint8_t* dp = (uint8_t*) buf;
  while (count--) {
    const decl_member_t* mp = d.member;
    for (uint16_t i = 0; i < d.count; i++) {
      decl_member_t m;
      memcpy_P(&m, mp++, sizeof(m));
      // Allow strings and data elements vectors only
      // Fix: Add table with user defined types
      if (m.count == 0 && m.type == UINT8_TYPE) {
	uint8_t* sp = *((uint8_t**) dp);
	uint8_t d;
	do {
	  d = *sp++;
	  _dev->putchar(d);
	} while (d != 0);
	dp += sizeof(sp);
      } 
      else {
	size_t s = pgm_read_byte(&sizeoftype[m.type >> 4]) * m.count;
	_dev->write(dp, s);
	dp += s;
      }
    }
  }
}