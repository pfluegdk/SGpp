/*****************************************************************************/
/* This file is part of sgpp, a program package making use of spatially      */
/* adaptive sparse grids to solve numerical problems                         */
/*                                                                           */
/* Copyright (C) 2009 Alexander Heinecke (Alexander.Heinecke@mytum.de)       */
/*                                                                           */
/* sgpp is free software; you can redistribute it and/or modify              */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 3 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* sgpp is distributed in the hope that it will be useful,                   */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with sgpp; if not, write to the Free Software                       */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */
/* or see <http://www.gnu.org/licenses/>.                                    */
/*****************************************************************************/

#ifndef LINEARBOUNDARYOSCALEDBASE_HPP
#define LINEARBOUNDARYOSCALEDBASE_HPP

#include <cmath>

namespace sg
{

/**
 * linear base functions with over-scaled boundaries
 * And here we have another implicit dependence on tensor products
 */
template<class LT, class IT>
class linearboundaryOScaledBase
{
public:
	/**
	 * Evaluate a base functions.
	 * Has a dependence on the absolute position of grid point and support.
	 */
	double eval(LT level, IT index, double p)
	{
		if (level == 0)
		{
			if (index == 0)
			{
				return 1.0 - p;
			}
			if (index == 1)
			{
				return p;
			}
		}
		else
		{
			return 1.0 - fabs((1<<level) * p - index);
		}
	}
};

}

#endif /* LINEARBOUNDARYOSCALEDBASE_HPP */
