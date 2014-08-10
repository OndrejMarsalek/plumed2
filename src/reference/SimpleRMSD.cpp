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
#include "RMSDBase.h"
#include "MetricRegister.h"
#include "tools/RMSD.h"

namespace PLMD{

class SimpleRMSD : public RMSDBase {
private:
  RMSD myrmsd;
public:
  SimpleRMSD( const ReferenceConfigurationOptions& ro );
  void read( const PDB& );
  double calc( const std::vector<Vector>& pos, const bool& squared );
  bool pcaIsEnabledForThisReference() const { return true; }
};

PLUMED_REGISTER_METRIC(SimpleRMSD,"SIMPLE")

SimpleRMSD::SimpleRMSD( const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration( ro ),
RMSDBase( ro )
{
}

void SimpleRMSD::read( const PDB& pdb ){
  readReference( pdb );
}

double SimpleRMSD::calc( const std::vector<Vector>& pos, const bool& squared ){
  return myrmsd.simpleAlignment( getAlign(), getDisplace(), pos, getReferencePositions(), atom_ders, squared );
}

}
