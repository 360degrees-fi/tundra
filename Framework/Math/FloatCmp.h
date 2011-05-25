/** @file FloatCmp.h
	@author Jukka Jyl�nki

	This work is copyrighted material and may NOT be used for any kind of commercial or 
	personal advantage and may NOT be copied or redistributed without prior consent
	of the author(s). 
*/
#pragma once

#include <cassert>

bool Equal(double a, double b, double epsilon = 1e-6)
{
	return std::abs(a-b) < epsilon;
}
