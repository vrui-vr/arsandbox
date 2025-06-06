/***********************************************************************
Bits - Type declarations representing fixed-maximum-length sequences of
bits for Huffman coding.
Copyright (c) 2023 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#ifndef BITS_INCLUDED
#define BITS_INCLUDED

#include <Misc/SizedTypes.h>

typedef Misc::UInt32 Bits; // Use 32-bit integers to hold bit sequences
const unsigned int maxNumBits=sizeof(Bits)*8U; // Maximum number of bits in a bit sequence

#endif
