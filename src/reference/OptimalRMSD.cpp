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
#include "RMSDBase.h"
#include "tools/Matrix.h"
#include "tools/RMSD.h"

namespace PLMD{

class OptimalRMSD : public RMSDBase {
private:
  bool pca,fast;
  RMSD myrmsd;
  Vector cpos;
  Tensor rot, trot;
  Matrix<Tensor> drot;
  Matrix<std::vector<Vector>  > DRotDPos;
public:
  OptimalRMSD(const ReferenceConfigurationOptions& ro);
  void read( const PDB& );
  double calc( const std::vector<Vector>& pos, const bool& squared );
  bool pcaIsEnabledForThisReference(){ pca=true; return true; }
  Vector getAtomicDisplacement( const unsigned& iatom );
  double projectAtomicDisplacementOnVector( const unsigned& iv, const Matrix<Vector>& vecs, const std::vector<Vector>& pos, std::vector<Vector>& derivatives );
};

PLUMED_REGISTER_METRIC(OptimalRMSD,"OPTIMAL")

OptimalRMSD::OptimalRMSD(const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration(ro),
RMSDBase(ro),
pca(false),
drot(3,3),
DRotDPos(3,3)
{
  fast=ro.usingFastOption();
}

void OptimalRMSD::read( const PDB& pdb ){
  readReference( pdb ); 
}

double OptimalRMSD::calc( const std::vector<Vector>& pos, const bool& squared ){
  double val; 

  if( fast ){
     if( getAlign()==getDisplace() && !pca ){
        val=myrmsd.optimalAlignment<false,true>(getAlign(),getDisplace(),pos,getReferencePositions(),displacement,cpos,trot,drot,atom_ders,squared); 
     }
     val=myrmsd.optimalAlignment<false,false>(getAlign(),getDisplace(),pos,getReferencePositions(),displacement,cpos,trot,drot,atom_ders,squared);
  } else {
     if( getAlign()==getDisplace() && !pca ){
        val=myrmsd.optimalAlignment<true,true>(getAlign(),getDisplace(),pos,getReferencePositions(),displacement,cpos,trot,drot,atom_ders,squared);
     }
     val=myrmsd.optimalAlignment<true,false>(getAlign(),getDisplace(),pos,getReferencePositions(),displacement,cpos,trot,drot,atom_ders,squared);
  }

  if( pca ){
      rot = trot.transpose();
      if( DRotDPos(0,0).size()!=getNumberOfAtoms() ){
         for(unsigned a=0;a<3;++a){ 
             for(unsigned b=0;b<3;b++) DRotDPos(a,b).resize( getNumberOfAtoms() ); 
         }
      }
      Vector csum; std::vector<Vector> v( getNumberOfAtoms() );
      for(unsigned iat=0;iat<getNumberOfAtoms();iat++) csum+=getReferencePosition(iat)*getAlign()[iat];
      for(unsigned iat=0;iat<getNumberOfAtoms();iat++) v[iat]=( getReferencePosition(iat)-csum )*getAlign()[iat];
      for(unsigned a=0;a<3;a++){
          for(unsigned b=0;b<3;b++){
              for(unsigned iat=0;iat<getNumberOfAtoms();iat++) DRotDPos[b][a][iat]=matmul(drot[a][b],v[iat]);
          }
      }
  }
  return val;
}

Vector OptimalRMSD::getAtomicDisplacement( const unsigned& iatom ){
  return matmul( rot, displacement[iatom] );
}

double OptimalRMSD::projectAtomicDisplacementOnVector( const unsigned& iv, const Matrix<Vector>& vecs, const std::vector<Vector>& pos, std::vector<Vector>& derivatives ){

  std::vector<Vector> centeredpos( getNumberOfAtoms() );
  for(unsigned i=0;i<pos.size();++i) centeredpos[i] = pos[i]-cpos;

  double proj=0.0; derivatives.clear();
  for(unsigned i=0;i<pos.size();++i){
      proj += dotProduct( matmul( rot, centeredpos[i] ) - getReferencePosition(i), vecs(iv,i) );
      for(unsigned a=0;a<3;a++){
          for(unsigned b=0;b<3;b++){
              for(unsigned iat=0;iat<getNumberOfAtoms();iat++){
                  double tmp1=0.;
                  for(unsigned n=0;n<getNumberOfAtoms();n++) tmp1+=centeredpos[n][b]*vecs(iv,n)[a];
                  derivatives[iat]+=DRotDPos[a][b][iat]*tmp1;
              }
          }
      }
  }
  Vector v1; v1.zero(); double prefactor = 1. / static_cast<double>( getNumberOfAtoms() );
  for(unsigned n=0;n<getNumberOfAtoms();n++) v1+=prefactor*matmul(trot,vecs(iv,n));
  for(unsigned iat=0;iat<getNumberOfAtoms();iat++) derivatives[iat]+=matmul(trot,vecs(iv,iat))-v1;

  return proj;
}

}
