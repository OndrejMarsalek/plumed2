/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2014 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "MetricRegister.h"
#include "ArgumentOnlyDistance.h"

namespace PLMD {

class EuclideanDistance : public ArgumentOnlyDistance {
public:
  EuclideanDistance( const ReferenceConfigurationOptions& ro );
  void read( const PDB& );
  double calc( const std::vector<Value*>& vals, const std::vector<double>& arg, const bool& squared );
  bool pcaIsEnabledForThisReference() const { return true; }
};

PLUMED_REGISTER_METRIC(EuclideanDistance,"EUCLIDEAN")

EuclideanDistance::EuclideanDistance( const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration(ro),
ArgumentOnlyDistance(ro)
{
}

void EuclideanDistance::read( const PDB& pdb ){
  readArgumentsFromPDB( pdb );
}

double EuclideanDistance::calc( const std::vector<Value*>& vals, const std::vector<double>& arg, const bool& squared ){
  return calculateArgumentDistance( vals, arg, squared );
}


}
