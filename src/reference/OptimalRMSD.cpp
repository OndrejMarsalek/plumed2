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
#include "MetricRegister.h"
#include "RMSDBase.h"
#include "tools/Matrix.h"
#include "tools/Exception.h"

namespace PLMD{

/// this is a class which is needed to share information across the various non-threadsafe routines
/// so that the public function of rmsd are threadsafe while the inner core can safely share information 
class RMSDCoreData
{
	public:
		bool alEqDis;
		bool distanceIsSquared;
		bool hasDistance;
		bool isInitialized;
		bool safe;
// use initialization reference assignment to speed up instead of copying and sucking out memory
// Reference coordinates
                const std::vector<Vector> &positions;
                const std::vector<Vector> &reference;
                // Weights for alignment
                const std::vector<double> &align;
                // Weights for deviation
                const std::vector<double> &displace;
		// the constructor (note: only references are passed, therefore is rather fast)
		RMSDCoreData(const std::vector<double> &a ,const std::vector<double> &d,const std::vector<Vector> &p, const std::vector<Vector> &r ):alEqDis(false),distanceIsSquared(false),hasDistance(false),isInitialized(false),safe(false),positions(p),reference(r),align(a),displace(d){};
		// this is a function that does the core calc 	
		void doCoreCalc(bool safe,bool alEqDis);
// the needed stuff for distance and more (one could use eigenvecs components and eigenvals for some reason)
		double dist;
		std::vector<double> eigenvals;
		Matrix<double> eigenvecs;
		double rr00; //  sum of positions squared (needed for dist calc)
		double rr11; //  sum of reference squared (needed for dist calc)
 
// rotation derived from the eigenvector having the smallest eigenvalue
		Tensor rotation;
// derivative of the rotation only available when align!=displace
		Tensor drotation_drr01[3][3];
		Tensor ddist_drr01;
	        Tensor ddist_drotation;
// difference of components
		std::vector<Vector> d;
// geometric center of the running position and reference
 		Vector cpositions,creference;

};

class OptimalRMSD : public RMSDBase {
private:
  bool fast;
/// TODO: import these into RMSDCoreData?
/// These functions that follow are not threadsafe and must be used always embedded in a public method that creates and destroys the RMSDCoreData
/// in a proper way! That is threadsafe! 
/// this is to be called first: just make the true quaternion and prepare the cd box to calculate other stuff
template <bool safe,bool alEqDis>
  double coreCalcInit(RMSDCoreData &cd);
/// these below must be executed after coreCalc so that the quaternion elements are all updated  
/// this is a simple utility to retrieve the distance you calculated in coreCalcInitAndDist without doing the algebra
  double coreCalcDistance( RMSDCoreData &cd ,bool squared=false);
/// this  calculates the derivatives of the distance respect to the positions.  
  void coreCalcDDistanceDPositions(std::vector<Vector>  &derivatives, const RMSDCoreData &cd );
  void coreCalcDDistanceDReference(std::vector<Vector>  &derivatives,const RMSDCoreData &cd );
  void coreCalcDRotationDPositions(std::vector<std::vector<std::vector  <Vector> > >  &derivatives, const RMSDCoreData &cd );
  void coreCalcDRotationDReference(std::vector<std::vector<std::vector  <Vector> > >  &derivatives, const RMSDCoreData &cd );
public:
  OptimalRMSD(const ReferenceConfigurationOptions& ro);
  void read( const PDB& );
  double calc( const std::vector<Vector>& pos, const bool& squared );

  template <bool safe,bool alEqDis>
  double optimalAlignment(const  std::vector<double>  & align,
                          const  std::vector<double>  & displace,
                          const std::vector<Vector> & positions,
                          bool squared=false);
};

PLUMED_REGISTER_METRIC(OptimalRMSD,"OPTIMAL")

OptimalRMSD::OptimalRMSD(const ReferenceConfigurationOptions& ro ):
ReferenceConfiguration(ro),
RMSDBase(ro)
{
  fast=ro.usingFastOption();
}

void OptimalRMSD::read( const PDB& pdb ){
  readReference( pdb ); 

}

double OptimalRMSD::calc( const std::vector<Vector>& pos, const bool& squared ){
  if( fast ){
     if( getAlign()==getDisplace() ) return optimalAlignment<false,true>(getAlign(),getDisplace(),pos,squared); 
     return optimalAlignment<false,false>(getAlign(),getDisplace(),pos,squared);
  } else {
     if( getAlign()==getDisplace() ) return optimalAlignment<true,true>(getAlign(),getDisplace(),pos,squared);
     return optimalAlignment<true,false>(getAlign(),getDisplace(),pos,squared);
  }
}

// standard RMSD calculation, just calculate
#ifdef OLDRMSD
template <bool safe,bool alEqDis>
double OptimalRMSD::optimalAlignment(const  std::vector<double>  & align,
                              const  std::vector<double>  & displace,
                              const std::vector<Vector> & positions,
                              bool squared){
  double dist(0);
  unsigned n=getNumberOfReferencePositions();
// This is the trace of positions*positions + reference*reference
  double rr00(0);
  double rr11(0);
// This is positions*reference
  Tensor rr01; Vector rpos;

  Vector cpositions;

// first expensive loop: compute centers
  for(unsigned iat=0;iat<n;iat++){
    double w=align[iat];
    cpositions+=positions[iat]*w;
  }

// second expensive loop: compute second moments wrt centers
  for(unsigned iat=0;iat<n;iat++){
    double w=align[iat]; rpos=getReferencePosition(iat);
    rr00+=dotProduct(positions[iat]-cpositions,positions[iat]-cpositions)*w;
    rr11+=dotProduct(rpos,rpos)*w;
    rr01+=Tensor(positions[iat]-cpositions,rpos)*w;
  }

  Matrix<double> m=Matrix<double>(4,4);
  m[0][0]=2.0*(-rr01[0][0]-rr01[1][1]-rr01[2][2]);
  m[1][1]=2.0*(-rr01[0][0]+rr01[1][1]+rr01[2][2]);
  m[2][2]=2.0*(+rr01[0][0]-rr01[1][1]+rr01[2][2]);
  m[3][3]=2.0*(+rr01[0][0]+rr01[1][1]-rr01[2][2]);
  m[0][1]=2.0*(-rr01[1][2]+rr01[2][1]);
  m[0][2]=2.0*(+rr01[0][2]-rr01[2][0]);
  m[0][3]=2.0*(-rr01[0][1]+rr01[1][0]);
  m[1][2]=2.0*(-rr01[0][1]-rr01[1][0]);
  m[1][3]=2.0*(-rr01[0][2]-rr01[2][0]);
  m[2][3]=2.0*(-rr01[1][2]-rr01[2][1]);
  m[1][0] = m[0][1];
  m[2][0] = m[0][2];
  m[2][1] = m[1][2];
  m[3][0] = m[0][3];
  m[3][1] = m[1][3];
  m[3][2] = m[2][3];

  Tensor dm_drr01[4][4];
  if(!alEqDis){
    dm_drr01[0][0] = 2.0*Tensor(-1.0, 0.0, 0.0,  0.0,-1.0, 0.0,  0.0, 0.0,-1.0);
    dm_drr01[1][1] = 2.0*Tensor(-1.0, 0.0, 0.0,  0.0,+1.0, 0.0,  0.0, 0.0,+1.0);
    dm_drr01[2][2] = 2.0*Tensor(+1.0, 0.0, 0.0,  0.0,-1.0, 0.0,  0.0, 0.0,+1.0);
    dm_drr01[3][3] = 2.0*Tensor(+1.0, 0.0, 0.0,  0.0,+1.0, 0.0,  0.0, 0.0,-1.0);
    dm_drr01[0][1] = 2.0*Tensor( 0.0, 0.0, 0.0,  0.0, 0.0,-1.0,  0.0,+1.0, 0.0);
    dm_drr01[0][2] = 2.0*Tensor( 0.0, 0.0,+1.0,  0.0, 0.0, 0.0, -1.0, 0.0, 0.0);
    dm_drr01[0][3] = 2.0*Tensor( 0.0,-1.0, 0.0, +1.0, 0.0, 0.0,  0.0, 0.0, 0.0);
    dm_drr01[1][2] = 2.0*Tensor( 0.0,-1.0, 0.0, -1.0, 0.0, 0.0,  0.0, 0.0, 0.0);
    dm_drr01[1][3] = 2.0*Tensor( 0.0, 0.0,-1.0,  0.0, 0.0, 0.0, -1.0, 0.0, 0.0);
    dm_drr01[2][3] = 2.0*Tensor( 0.0, 0.0, 0.0,  0.0, 0.0,-1.0,  0.0,-1.0, 0.0);
    dm_drr01[1][0] = dm_drr01[0][1];
    dm_drr01[2][0] = dm_drr01[0][2];
    dm_drr01[2][1] = dm_drr01[1][2];
    dm_drr01[3][0] = dm_drr01[0][3];
    dm_drr01[3][1] = dm_drr01[1][3];
    dm_drr01[3][2] = dm_drr01[2][3];
  }

  std::vector<double> eigenvals;
  Matrix<double> eigenvecs;
  int diagerror=diagMat(m, eigenvals, eigenvecs );

  if (diagerror!=0){
    std::string sdiagerror;
    Tools::convert(diagerror,sdiagerror);
    std::string msg="DIAGONALIZATION FAILED WITH ERROR CODE "+sdiagerror;
    plumed_merror(msg);
  }

  dist=eigenvals[0]+rr00+rr11;

  Matrix<double> ddist_dm(4,4);

  Vector4d q(eigenvecs[0][0],eigenvecs[0][1],eigenvecs[0][2],eigenvecs[0][3]);

  Tensor dq_drr01[4];
  if(!alEqDis){
    double dq_dm[4][4][4];
    for(unsigned i=0;i<4;i++) for(unsigned j=0;j<4;j++) for(unsigned k=0;k<4;k++){
      double tmp=0.0;
// perturbation theory for matrix m
      for(unsigned l=1;l<4;l++) tmp+=eigenvecs[l][j]*eigenvecs[l][i]/(eigenvals[0]-eigenvals[l])*eigenvecs[0][k];
      dq_dm[i][j][k]=tmp;
    }
// propagation to _drr01
    for(unsigned i=0;i<4;i++){
      Tensor tmp;
      for(unsigned j=0;j<4;j++) for(unsigned k=0;k<4;k++) {
        tmp+=dq_dm[i][j][k]*dm_drr01[j][k];
      }
      dq_drr01[i]=tmp;
    }
  }

// This is the rotation matrix that brings reference to positions
// i.e. matmul(rotation,reference[iat])+shift is fitted to positions[iat]

  Tensor rotation;
  rotation[0][0]=q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
  rotation[1][1]=q[0]*q[0]-q[1]*q[1]+q[2]*q[2]-q[3]*q[3];
  rotation[2][2]=q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];
  rotation[0][1]=2*(+q[0]*q[3]+q[1]*q[2]);
  rotation[0][2]=2*(-q[0]*q[2]+q[1]*q[3]);
  rotation[1][2]=2*(+q[0]*q[1]+q[2]*q[3]);
  rotation[1][0]=2*(-q[0]*q[3]+q[1]*q[2]);
  rotation[2][0]=2*(+q[0]*q[2]+q[1]*q[3]);
  rotation[2][1]=2*(-q[0]*q[1]+q[2]*q[3]);


  Tensor drotation_drr01[3][3];
  if(!alEqDis){
    drotation_drr01[0][0]=2*q[0]*dq_drr01[0]+2*q[1]*dq_drr01[1]-2*q[2]*dq_drr01[2]-2*q[3]*dq_drr01[3];
    drotation_drr01[1][1]=2*q[0]*dq_drr01[0]-2*q[1]*dq_drr01[1]+2*q[2]*dq_drr01[2]-2*q[3]*dq_drr01[3];
    drotation_drr01[2][2]=2*q[0]*dq_drr01[0]-2*q[1]*dq_drr01[1]-2*q[2]*dq_drr01[2]+2*q[3]*dq_drr01[3];
    drotation_drr01[0][1]=2*(+(q[0]*dq_drr01[3]+dq_drr01[0]*q[3])+(q[1]*dq_drr01[2]+dq_drr01[1]*q[2]));
    drotation_drr01[0][2]=2*(-(q[0]*dq_drr01[2]+dq_drr01[0]*q[2])+(q[1]*dq_drr01[3]+dq_drr01[1]*q[3]));
    drotation_drr01[1][2]=2*(+(q[0]*dq_drr01[1]+dq_drr01[0]*q[1])+(q[2]*dq_drr01[3]+dq_drr01[2]*q[3]));
    drotation_drr01[1][0]=2*(-(q[0]*dq_drr01[3]+dq_drr01[0]*q[3])+(q[1]*dq_drr01[2]+dq_drr01[1]*q[2]));
    drotation_drr01[2][0]=2*(+(q[0]*dq_drr01[2]+dq_drr01[0]*q[2])+(q[1]*dq_drr01[3]+dq_drr01[1]*q[3]));
    drotation_drr01[2][1]=2*(-(q[0]*dq_drr01[1]+dq_drr01[0]*q[1])+(q[2]*dq_drr01[3]+dq_drr01[2]*q[3]));
  }

  double prefactor=2.0;

  if(!squared && alEqDis) prefactor*=0.5/sqrt(dist);

// if "safe", recompute dist here to a better accuracy
  if(safe || !alEqDis) dist=0.0;

// If safe is set to "false", MSD is taken from the eigenvalue of the M matrix
// If safe is set to "true", MSD is recomputed from the rotational matrix
// For some reason, this last approach leads to less numerical noise but adds an overhead

  Tensor ddist_drotation;
  Vector ddist_dcpositions;

// third expensive loop: derivatives
  for(unsigned iat=0;iat<n;iat++){
    Vector d(positions[iat]-cpositions - matmul(rotation,getReferencePosition(iat)));
    if(alEqDis){
// there is no need for derivatives of rotation and shift here as it is by construction zero
// (similar to Hellman-Feynman forces)
       addAtomicDerivatives( iat, prefactor*align[iat]*d );
       if(safe) dist+=align[iat]*modulo2(d);
    } else {
// the case for align != displace is different, sob:
      dist+=displace[iat]*modulo2(d);
// these are the derivatives assuming the roto-translation as frozen
      atom_ders[iat]=2*displace[iat]*d;
// here I accumulate derivatives wrt rotation matrix ..
      ddist_drotation+=-2*displace[iat]*extProduct(d,getReferencePosition(iat));
// .. and cpositions
      ddist_dcpositions+=-2*displace[iat]*d;
    }
  }

  if(!alEqDis){
    Tensor ddist_drr01;
    for(unsigned i=0;i<3;i++) for(unsigned j=0;j<3;j++) ddist_drr01+=ddist_drotation[i][j]*drotation_drr01[i][j];
    for(unsigned iat=0;iat<n;iat++){
// this is propagating to positions.
// I am implicitly using the derivative of rr01 wrt positions here
      addAtomicDerivatives( iat, matmul(ddist_drr01,getReferencePosition(iat))*align[iat] );
      addAtomicDerivatives( iat, ddist_dcpositions*align[iat] );
    }
  }
  if(!squared){
    dist=sqrt(dist);
    if(!alEqDis){
      double xx=0.5/dist;
      for(unsigned iat=0;iat<atom_ders.size();iat++) atom_ders[iat]*=xx;
    }
  }

  return dist;
}
#else
// this is the standard version: no renewal of reference
template <bool safe,bool alEqDis>
double OptimalRMSD::optimalAlignment(const  std::vector<double>  & align,
                              const  std::vector<double>  & displace,
                              const std::vector<Vector> & positions,
                              bool squared){
   //initialize the data into the structure
   RMSDCoreData cd(align,displace,positions,getReferencePositions()); 
   // TODO: now just ignorance....
   //           safe alEqDis
   // Perform the diagonalization and all the needed stuff
   cd.doCoreCalc(safe,alEqDis); 
//   // make the core calc distance
//   double dist=coreCalcDistance(cd,squared); 
//   // make the derivatives by using pieces calculated in coreCalc 
//   coreCalcDDistanceDReference(derivatives,cd); 
//   return dist;    

}
/// This calculates the elements needed by the quaternion to calculate everything that is needed
/// additional calls retrieve different components
void RMSDCoreData::doCoreCalc(bool safe,bool alEqDis){

  const unsigned n=static_cast<unsigned int>(reference.size());

  cpositions.zero();
  creference.zero();

// first expensive loop: compute centers
// TODO: additional flags could avoid it in case the reference is already centered 
  for(unsigned iat=0;iat<n;iat++){
    double w=align[iat];
    cpositions+=positions[iat]*w;
    creference+=reference[iat]*w;
  }

// This is the trace of positions*positions + reference*reference
  rr00=0.;
  rr11=0.;
// This is positions*reference
  Tensor rr01;
// second expensive loop: compute second moments wrt centers
  for(unsigned iat=0;iat<n;iat++){
    double w=align[iat];
    rr00+=dotProduct(positions[iat]-cpositions,positions[iat]-cpositions)*w;
    rr11+=dotProduct(reference[iat]-creference,reference[iat]-creference)*w;
    rr01+=Tensor(positions[iat]-cpositions,reference[iat]-creference)*w;
  }

// the quaternion matrix: this is internal
  Matrix<double> m=Matrix<double>(4,4);

  m[0][0]=2.0*(-rr01[0][0]-rr01[1][1]-rr01[2][2]);
  m[1][1]=2.0*(-rr01[0][0]+rr01[1][1]+rr01[2][2]);
  m[2][2]=2.0*(+rr01[0][0]-rr01[1][1]+rr01[2][2]);
  m[3][3]=2.0*(+rr01[0][0]+rr01[1][1]-rr01[2][2]);
  m[0][1]=2.0*(-rr01[1][2]+rr01[2][1]);
  m[0][2]=2.0*(+rr01[0][2]-rr01[2][0]);
  m[0][3]=2.0*(-rr01[0][1]+rr01[1][0]);
  m[1][2]=2.0*(-rr01[0][1]-rr01[1][0]);
  m[1][3]=2.0*(-rr01[0][2]-rr01[2][0]);
  m[2][3]=2.0*(-rr01[1][2]-rr01[2][1]);
  m[1][0] = m[0][1];
  m[2][0] = m[0][2];
  m[2][1] = m[1][2];
  m[3][0] = m[0][3];
  m[3][1] = m[1][3];
  m[3][2] = m[2][3];

  
  Tensor dm_drr01[4][4];
  if(!alEqDis){
    dm_drr01[0][0] = 2.0*Tensor(-1.0, 0.0, 0.0,  0.0,-1.0, 0.0,  0.0, 0.0,-1.0); 
    dm_drr01[1][1] = 2.0*Tensor(-1.0, 0.0, 0.0,  0.0,+1.0, 0.0,  0.0, 0.0,+1.0);
    dm_drr01[2][2] = 2.0*Tensor(+1.0, 0.0, 0.0,  0.0,-1.0, 0.0,  0.0, 0.0,+1.0);
    dm_drr01[3][3] = 2.0*Tensor(+1.0, 0.0, 0.0,  0.0,+1.0, 0.0,  0.0, 0.0,-1.0);
    dm_drr01[0][1] = 2.0*Tensor( 0.0, 0.0, 0.0,  0.0, 0.0,-1.0,  0.0,+1.0, 0.0);
    dm_drr01[0][2] = 2.0*Tensor( 0.0, 0.0,+1.0,  0.0, 0.0, 0.0, -1.0, 0.0, 0.0);
    dm_drr01[0][3] = 2.0*Tensor( 0.0,-1.0, 0.0, +1.0, 0.0, 0.0,  0.0, 0.0, 0.0);
    dm_drr01[1][2] = 2.0*Tensor( 0.0,-1.0, 0.0, -1.0, 0.0, 0.0,  0.0, 0.0, 0.0);
    dm_drr01[1][3] = 2.0*Tensor( 0.0, 0.0,-1.0,  0.0, 0.0, 0.0, -1.0, 0.0, 0.0);
    dm_drr01[2][3] = 2.0*Tensor( 0.0, 0.0, 0.0,  0.0, 0.0,-1.0,  0.0,-1.0, 0.0);
    dm_drr01[1][0] = dm_drr01[0][1];
    dm_drr01[2][0] = dm_drr01[0][2];
    dm_drr01[2][1] = dm_drr01[1][2];
    dm_drr01[3][0] = dm_drr01[0][3];
    dm_drr01[3][1] = dm_drr01[1][3];
    dm_drr01[3][2] = dm_drr01[2][3];
  }


  int diagerror=diagMat(m, eigenvals, eigenvecs );

  if (diagerror!=0){
    std::string sdiagerror;
    Tools::convert(diagerror,sdiagerror);
    std::string msg="DIAGONALIZATION FAILED WITH ERROR CODE "+sdiagerror;
    plumed_merror(msg);
  }

  Vector4d q(eigenvecs[0][0],eigenvecs[0][1],eigenvecs[0][2],eigenvecs[0][3]);

  Tensor dq_drr01[4];
  if(!alEqDis){
    double dq_dm[4][4][4];
    for(unsigned i=0;i<4;i++) for(unsigned j=0;j<4;j++) for(unsigned k=0;k<4;k++){
      double tmp=0.0;
// perturbation theory for matrix m
      for(unsigned l=1;l<4;l++) tmp+=eigenvecs[l][j]*eigenvecs[l][i]/(eigenvals[0]-eigenvals[l])*eigenvecs[0][k];
      dq_dm[i][j][k]=tmp;
    }
// propagation to _drr01
    for(unsigned i=0;i<4;i++){
      Tensor tmp;
      for(unsigned j=0;j<4;j++) for(unsigned k=0;k<4;k++) {
        tmp+=dq_dm[i][j][k]*dm_drr01[j][k];
      }
      dq_drr01[i]=tmp;
    }
  }

// This is the rotation matrix that brings reference to positions
// i.e. matmul(rotation,reference[iat])+shift is fitted to positions[iat]

  rotation[0][0]=q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
  rotation[1][1]=q[0]*q[0]-q[1]*q[1]+q[2]*q[2]-q[3]*q[3];
  rotation[2][2]=q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];
  rotation[0][1]=2*(+q[0]*q[3]+q[1]*q[2]);
  rotation[0][2]=2*(-q[0]*q[2]+q[1]*q[3]);
  rotation[1][2]=2*(+q[0]*q[1]+q[2]*q[3]);
  rotation[1][0]=2*(-q[0]*q[3]+q[1]*q[2]);
  rotation[2][0]=2*(+q[0]*q[2]+q[1]*q[3]);
  rotation[2][1]=2*(-q[0]*q[1]+q[2]*q[3]);
  
  if(!alEqDis){
    drotation_drr01[0][0]=2*q[0]*dq_drr01[0]+2*q[1]*dq_drr01[1]-2*q[2]*dq_drr01[2]-2*q[3]*dq_drr01[3];
    drotation_drr01[1][1]=2*q[0]*dq_drr01[0]-2*q[1]*dq_drr01[1]+2*q[2]*dq_drr01[2]-2*q[3]*dq_drr01[3];
    drotation_drr01[2][2]=2*q[0]*dq_drr01[0]-2*q[1]*dq_drr01[1]-2*q[2]*dq_drr01[2]+2*q[3]*dq_drr01[3];
    drotation_drr01[0][1]=2*(+(q[0]*dq_drr01[3]+dq_drr01[0]*q[3])+(q[1]*dq_drr01[2]+dq_drr01[1]*q[2]));
    drotation_drr01[0][2]=2*(-(q[0]*dq_drr01[2]+dq_drr01[0]*q[2])+(q[1]*dq_drr01[3]+dq_drr01[1]*q[3]));
    drotation_drr01[1][2]=2*(+(q[0]*dq_drr01[1]+dq_drr01[0]*q[1])+(q[2]*dq_drr01[3]+dq_drr01[2]*q[3]));
    drotation_drr01[1][0]=2*(-(q[0]*dq_drr01[3]+dq_drr01[0]*q[3])+(q[1]*dq_drr01[2]+dq_drr01[1]*q[2]));
    drotation_drr01[2][0]=2*(+(q[0]*dq_drr01[2]+dq_drr01[0]*q[2])+(q[1]*dq_drr01[3]+dq_drr01[1]*q[3]));
    drotation_drr01[2][1]=2*(-(q[0]*dq_drr01[1]+dq_drr01[0]*q[1])+(q[2]*dq_drr01[3]+dq_drr01[2]*q[3]));
  }

  d.resize(n);

  // calculate rotation matrix derivatives and components distances needed for components only when align!=displacement
  if(!alEqDis)ddist_drotation.zero();
  for(unsigned iat=0;iat<n;iat++){
    // components differences: this is useful externally
    d[iat]=positions[iat]-cpositions - matmul(rotation,reference[iat]-creference);	
    // ddist_drotation if needed
    if(!alEqDis) ddist_drotation+=-2*displace[iat]*extProduct(d[iat],reference[iat]-creference);
  }

  if(!alEqDis){
          ddist_drr01.zero();
          for(unsigned i=0;i<3;i++) for(unsigned j=0;j<3;j++) ddist_drr01+=ddist_drotation[i][j]*drotation_drr01[i][j];
  }
  // transfer this bools to the cd so that this settings will be reflected in the other calls
  this->alEqDis=alEqDis; 
  this->safe=safe; 
  isInitialized=true;

}

#endif


}
