/** @file times.cpp
 *
 *  Main program that calls the individual timings. */

/*
 *  GiNaC Copyright (C) 1999-2002 Johannes Gutenberg University Mainz, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdexcept>
#include "times.h"

int main()
{
	unsigned result = 0;
	
#define TIME(which) \
try { \
	result += time_ ## which (); \
} catch (const exception &e) { \
	cout << "Error: caught exception " << e.what() << endl; \
	++result; \
}

	TIME(dennyfliegner)
	TIME(gammaseries)
	TIME(vandermonde)
	TIME(toeplitz)
	TIME(lw_A)
	TIME(lw_B)
	TIME(lw_C)
	TIME(lw_D)
	TIME(lw_E)
	TIME(lw_F)
	TIME(lw_G)
	TIME(lw_H)
	TIME(lw_IJKL)
	TIME(lw_M1)
	TIME(lw_M2)
	TIME(lw_N)
	TIME(lw_O)
	TIME(lw_P)
	TIME(lw_Pprime)
	TIME(lw_Q)
	TIME(lw_Qprime)
	TIME(antipode)
	
	if (result) {
		cout << "Error: something went wrong. ";
		if (result == 1) {
			cout << "(one failure)" << endl;
		} else {
			cout << "(" << result << " individual failures)" << endl;
		}
		cout << "please check times.out against times.ref for more details."
		     << endl << "happy debugging!" << endl;
	}
	
	return result;
}
