/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed-code.org for more information.

   This file is part of plumed, version 2.0.

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
#ifndef __PLUMED_reference_SingleDomainRMSD_h
#define __PLUMED_reference_SingleDomainRMSD_h

#include "ReferenceAtoms.h"

namespace PLMD {

class Pbc;

class SingleDomainRMSD : public ReferenceAtoms {
  bool reset_com, normalize_weights;
protected:
  void readReference( const PDB& pdb );
public:
  SingleDomainRMSD( const ReferenceConfigurationOptions& ro );
/// Set the reference structure
  virtual void setReferenceAtoms( const std::vector<Vector>& conf, const std::vector<double>& align_in, const std::vector<double>& displace_in );
/// Calculate
  double calc( const std::vector<Vector>& pos, const Pbc& pbc, const std::vector<Value*>& vals, const std::vector<double>& arg, const bool& squared );  
  double calculate( const std::vector<Vector>& pos, const Pbc& pbc,  const bool& squared );
/// Calculate the distance using the input position
  virtual double calc( const std::vector<Vector>& pos, const Pbc& pbc, const bool& squared )=0;
/// This sets upper and lower bounds on distances to be used in DRMSD (here it does nothing)
  virtual void setBoundsOnDistances( bool dopbc, double lbound=0.0, double ubound=std::numeric_limits<double>::max( ) ){};
  /// just few accessories not to break virtuals in setReferenceAtoms 
  bool const getNormalizeWeights(){return normalize_weights;};
  void const setNormalizeWeights(bool b){normalize_weights=b;};
  bool const getResetCom(){return reset_com;};
  void const setResetCom(bool b){reset_com=b;};
};

}

#endif
