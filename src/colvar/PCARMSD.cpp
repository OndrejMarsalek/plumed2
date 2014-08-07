/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2013 The plumed team
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
#include "Colvar.h"
#include "core/Atoms.h"
#include "core/PlumedMain.h"
#include "ActionRegister.h"
#include "tools/PDB.h"
#include "reference/RMSDBase.h"
#include "reference/MetricRegister.h"
#include "tools/Tools.h"

using namespace std;

namespace PLMD{
namespace colvar{
   
class PCARMSD : public Colvar {
	
  PLMD::RMSDBase* rmsd;
  bool squared; 
  std::vector< std::vector<Vector> > eigenvectors;
  std::vector<PDB> pdbv;
  std::vector<string> pca_names;
public:
  PCARMSD(const ActionOptions&);
  ~PCARMSD();
  virtual void calculate();
  static void registerKeywords(Keywords& keys);
};


using namespace std;

//+PLUMEDOC DCOLVAR PCARMSD 
/*
Calculate the PCA components for a number of provided eigenvectors and an average structure. Performs optimal alignment at every step. 

\par Examples

\verbatim
PCARMSD AVERAGE=file.pdb EIGENVECTORS=eigenvectors.pdb 
\endverbatim

...

*/
//+ENDPLUMEDOC

PLUMED_REGISTER_ACTION(PCARMSD,"PCARMSD")

void PCARMSD::registerKeywords(Keywords& keys){
  Colvar::registerKeywords(keys);
  keys.add("compulsory","AVERAGE","a file in pdb format containing the reference structure and the atoms involved in the CV.");
  keys.add("compulsory","EIGENVECTORS","a file in pdb format containing the reference structure and the atoms involved in the CV.");
  useCustomisableComponents(keys);
  //keys.addOutputComponent("err","COMPONENTS","the error component ");
  //keys.add("compulsory","TYPE","SIMPLE","the manner in which RMSD alignment is performed.  Should be OPTIMAL or SIMPLE.");
  //keys.addFlag("SQUARED",false," This should be setted if you want MSD instead of RMSD ");
}

PCARMSD::PCARMSD(const ActionOptions&ao):
PLUMED_COLVAR_INIT(ao),squared(true)
{
  string f_average;
  parse("AVERAGE",f_average);
  string type;	
  type.assign("OPTIMAL");
  string f_eigenvectors;
  parse("EIGENVECTORS",f_eigenvectors);
  checkRead();

  PDB pdb;

  // read everything in ang and transform to nm if we are not in natural units
  if( !pdb.read(f_average,plumed.getAtoms().usingNaturalUnits(),0.1/atoms.getUnits().getLength()) )
      error("missing input file " + f_average );


  rmsd = metricRegister().create<RMSDBase>(type,pdb);
  // here align and displace are a simple vector of ones
  std::vector<double> align; align=pdb.getOccupancy();for(unsigned i=0;i<align.size();i++){align[i]=1.;} ;
  std::vector<double> displace;  displace=pdb.getBeta();for(unsigned i=0;i<displace.size();i++){displace[i]=1.;} ; 
  // reset again to reimpose unifrom weights (safe to disable this)
  rmsd->setReferenceAtoms( pdb.getPositions(), align, displace );
  
  std::vector<AtomNumber> atomsn;
  rmsd->getAtomRequests( atomsn );
  rmsd->setNumberOfAtoms( atomsn.size() );
  requestAtoms( atomsn );

  //addComponent("err"); componentIsNotPeriodic("err"); 

  log.printf("  average from file %s\n",f_average.c_str());
  log.printf("  which contains %d atoms\n",getNumberOfAtoms());
  log.printf("  method for alignment : %s \n",type.c_str() );

  // now get the eigenvectors 
  // open the file
  FILE* fp=fopen(f_eigenvectors.c_str(),"r");
  std::vector<AtomNumber> aaa;
  unsigned neigenvects;
  neigenvects=0;
  if (fp!=NULL)
  {
    log<<"  Opening the eigenvectors file "<<f_eigenvectors.c_str()<<"\n";
    bool do_read=true;
    while (do_read){
         PDB mypdb; 
	 // check the units for reading this file: how can they make sense? 
         do_read=mypdb.readFromFilepointer(fp,plumed.getAtoms().usingNaturalUnits(),0.1/atoms.getUnits().getLength());
         if(do_read){
            unsigned nat=0;
            neigenvects++;
            if(mypdb.getAtomNumbers().size()==0) error("number of atoms in a frame should be more than zero");
            if(nat==0) nat=mypdb.getAtomNumbers().size();
            if(nat!=mypdb.getAtomNumbers().size()) error("frames should have the same number of atoms");
            if(aaa.empty()) aaa=mypdb.getAtomNumbers();
            if(aaa!=mypdb.getAtomNumbers()) error("frames should contain same atoms in same order");
            log<<"  Found eigenvector: "<<neigenvects<<" containing  "<<mypdb.getAtomNumbers().size()<<" atoms\n"; 
	    pdbv.push_back(mypdb); 
            eigenvectors.push_back(mypdb.getPositions()); 		
         }else{break ;}
    }
    fclose (fp);
    log<<"  Found total "<<neigenvects<< " eigenvectors in the file "<<f_eigenvectors.c_str()<<" \n"; 
    if(neigenvects==0) error("at least one eigenvector is expected");
  } 
  // the components 
  for(unsigned i=0;i<neigenvects;i++){
        string name; name=string("pca-")+std::to_string(i);
	pca_names.push_back(name);
	addComponentWithDerivatives(name.c_str()); componentIsNotPeriodic(name.c_str());	
  }  
  turnOnDerivatives();

}

PCARMSD::~PCARMSD(){
  delete rmsd;
}


// calculator
void PCARMSD::calculate(){
        Tensor rotation,invrotation;
        Matrix<std::vector<Vector> > drotdpos(3,3);
        std::vector<Vector> DDistDRef;
        std::vector<Vector> alignedpos;
        std::vector<Vector> centeredpos;
        std::vector<Vector> centeredref;
        double r=rmsd->calc_PCA( getPositions(), squared, rotation , drotdpos , alignedpos ,centeredpos, centeredref );
	invrotation=rotation.transpose();
	
//	getPntrToComponent("err")->set(r); < can set only when got the derivative (isn't is stupid that one can have value with only derivative or without???)
	for(unsigned i=0;i<eigenvectors.size();i++){
      	 	string name; name=pca_names[i];
		Value* value=getPntrToComponent(name.c_str());
		double val;val=0.;
		for(unsigned iat=0;iat<getNumberOfAtoms();iat++){
			//pca value
			val+=dotProduct(alignedpos[iat]-centeredref[iat],eigenvectors[i][iat]);	
		}
		value->set(val);
		// here the loop is reversed to better suit the structure of the derivative of the rotation matrix
		std::vector< Vector > der;
		der.resize(getNumberOfAtoms());
		double tmp1;
		for(unsigned a=0;a<3;a++){
			for(unsigned b=0;b<3;b++){
				for(unsigned iat=0;iat<getNumberOfAtoms();iat++){
					tmp1=0.;
					for(unsigned n=0;n<getNumberOfAtoms();n++){
						tmp1+=alignedpos[n][b]*eigenvectors[i][n][a];
					}
					der[iat]+=drotdpos[a][b][iat]*tmp1;	
				}
			}
		}
		Vector v1;
		for(unsigned n=0;n<getNumberOfAtoms();n++){
				v1+=(1./getNumberOfAtoms())*matmul(invrotation,eigenvectors[i][n]);	
		}	
		for(unsigned iat=0;iat<getNumberOfAtoms();iat++){
			der[iat]+=matmul(invrotation,eigenvectors[i][iat])-v1;	
		        setAtomsDerivatives (value,iat,der[iat]);
		}		
 	 }

//  setBoxDerivativesNoPbc();

}

}
}



