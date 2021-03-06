/*
 * RotationPricer.cpp
 *
 *  Created on: 2015-03-02
 *      Author: legraina
 */

#include "RotationPricer.h"
#include "BcpModeler.h"

/* namespace usage */
using namespace std;

//////////////////////////////////////////////////////////////
//
// R O T A T I O N   P R I C E R
//
//////////////////////////////////////////////////////////////


static char* baseName = "rotation";

/* Constructs the pricer object. */
RotationPricer::RotationPricer(MasterProblem* master, const char* name):
                        MyPricer(name), nbMaxRotationsToAdd_(20), nbSubProblemsToSolve_(15), nursesToSolve_(master->theNursesSorted_),
                        master_(master), pScenario_(master->pScenario_), nbDays_(master->pDemand_->nbDays_), pModel_(master->getModel())
{
   /* sort the nurses */
//   random_shuffle( nursesToSolve_.begin(), nursesToSolve_.end());
}

/* Destructs the pricer object. */
RotationPricer::~RotationPricer() {
   for(pair<const Contract*, SubProblem*> p: subProblems_)
      delete p.second;
}

/******************************************************
 * Perform pricing
 ******************************************************/
bool RotationPricer::pricing(double bound, bool before_fathom){
   //=false if once optimality hasn't be proven
   bool optimal = true;
   //forbidden shifts
   set<pair<int,int> > forbiddenShifts;
   //computed new rotations
   vector<Rotation> rotations;

//   std::cout << "# ------- BEGIN ------- Subproblems..." << std::endl;

   //count and store the nurses for whom their subproblem has generated rotations.
   int nbSubProblemSolved = 0, nbIteration = 0;
   double minDualCoast = 0;
   vector<LiveNurse*> nursesSolved;
   for(vector<LiveNurse*>::iterator it0 = nursesToSolve_.begin(); it0 != nursesToSolve_.end();){
      ++nbIteration;
      LiveNurse* pNurse = *it0;

      /* Build or re-use a subproblem */
      SubProblem* subProblem;
      //search the contract
      map<const Contract*, SubProblem*>::iterator it =  subProblems_.find(pNurse->pContract_);

      //if doesn't find => create new subproblem
      if( it == subProblems_.end() ){
         subProblem = new SubProblem(pScenario_, nbDays_, pNurse->pContract_, master_->pInitState_);
         subProblems_.insert(it, pair<const Contract*, SubProblem*>(pNurse->pContract_, subProblem));
      }

      //otherwise retrieve the subproblem associated to the contract
      else
         subProblem = it->second;

      /* Retrieves dual values */
      vector< vector<double> > workDualCosts(getWorkDualValues(pNurse));
      vector<double> startWorkDualCosts(getStartWorkDualValues(pNurse));
      vector<double> endWorkDualCosts(getEndWorkDualValues(pNurse));
      double workedWeekendDualCost = getWorkedWeekendDualValue(pNurse);

      DualCosts dualCosts (workDualCosts, startWorkDualCosts, endWorkDualCosts, workedWeekendDualCost, true);

      /* Compute forbidden */
      computeForbiddenShifts(forbiddenShifts, rotations);
      set<pair<int,int> > nurseForbiddenShifts(forbiddenShifts);
      pModel_->addForbidenShifts(pNurse, nurseForbiddenShifts);

	   /* Solve options */
	   vector<SolveOption> options;
	   options.push_back(SOLVE_ONE_SINK_PER_LAST_DAY);
	   options.push_back(SOLVE_SHORT_ALL);

	   /* Solve subproblems */
      optimal = false;


      subProblem->solve(pNurse, &dualCosts, options, forbiddenShifts, true , 120, bound);


	   /*
	    * Rotations
	    */

		/* Retrieve rotations */
		rotations = subProblem->getRotations();
		/* sort rotations */
      for(Rotation& rot: rotations){
         rot.computeCost(pScenario_, master_->pPreferences_, nbDays_);
//         rot.computeDualCost(dualCosts);
      }
		std::stable_sort(rotations.begin(), rotations.end(), Rotation::compareDualCost);
		/* add them to the master problem */
		int nbRotationsAdded = 0;
		double best;
		for(Rotation& rot: rotations){
			master_->addRotation(rot, baseName);
			++nbRotationsAdded;
			if(nbRotationsAdded >= nbMaxRotationsToAdd_)
			   break;
		}


      //count if the subproblem has generated some rotations and then store the nurse
      if(rotations.size() > 0){
         ++nbSubProblemSolved;
         if(rotations[0].dualCost_ < minDualCoast)
            minDualCoast = rotations[0].dualCost_;
         nursesToSolve_.erase(it0);
         nursesSolved.push_back(pNurse);
      }
      //try the next nurse
      else {
         ++it0;
      }
		//if the maximum number of subproblem solved is reached, break.
		if(nbSubProblemSolved == nbSubProblemsToSolve_)
		   break;
   }

   //Add the nurse in nursesSolved at the end
   nursesToSolve_.insert(nursesToSolve_.end(), nursesSolved.begin(), nursesSolved.end());

   //set statistics
   BcpModeler* model = dynamic_cast<BcpModeler*>(pModel_);
   if(model){
      model->setLastNbSubProblemsSolved(nbIteration);
      model->setLastMinDualCost(minDualCoast);
   }

//   std::cout << "# -------  END  ------- Subproblems!" << std::endl;

   return optimal;
}

/******************************************************
 * Get the duals values per day for a nurse
 ******************************************************/
vector< vector<double> > RotationPricer::getWorkDualValues(LiveNurse* pNurse){
   vector< vector<double> > dualValues(nbDays_);
   int i = pNurse->id_;
   int p = pNurse->pContract_->id_;

   /* Min/Max constraints */
   double minWorkedDays = pModel_->getDual(master_->minWorkedDaysCons_[i], true);
   double maxWorkedDays = pModel_->getDual(master_->maxWorkedDaysCons_[i], true);

   double minWorkedDaysAvg = master_->isMinWorkedDaysAvgCons_[i] ? pModel_->getDual(master_->minWorkedDaysAvgCons_[i], true):0.0;
   double maxWorkedDaysAvg = master_->isMaxWorkedDaysAvgCons_[i] ? pModel_->getDual(master_->maxWorkedDaysAvgCons_[i], true):0.0;

   double minWorkedDaysContractAvg = master_->isMinWorkedDaysContractAvgCons_[p] ?
      pModel_->getDual(master_->minWorkedDaysContractAvgCons_[p], true):0.0;
   double maxWorkedDaysContractAvg = master_->isMaxWorkedDaysContractAvgCons_[p] ?
      pModel_->getDual(master_->maxWorkedDaysContractAvgCons_[p], true):0.0;

   for(int k=0; k<nbDays_; ++k){
      //initialize vector
      vector<double> dualValues2(pScenario_->nbShifts_-1);

      for(int s=1; s<pScenario_->nbShifts_; ++s){
         /* Min/Max constraints */
         dualValues2[s-1] = minWorkedDays + minWorkedDaysAvg + minWorkedDaysContractAvg;
         dualValues2[s-1] += maxWorkedDays + maxWorkedDaysAvg + maxWorkedDaysContractAvg;

         /* Skills coverage */
         dualValues2[s-1] += pModel_->getDual(
            master_->numberOfNursesByPositionCons_[k][s-1][pNurse->pPosition_->id_], true);
      }

      //store vector
      dualValues[k] = dualValues2;
   }

   return dualValues;
}


vector<double> RotationPricer::getStartWorkDualValues(LiveNurse* pNurse){
   int i = pNurse->id_;
   vector<double> dualValues(nbDays_);

   //get dual value associated to the source
   dualValues[0] =  pModel_->getDual(master_->restFlowCons_[i][0], true);
   //get dual values associated to the work flow constraints
   //don't take into account the last which is the sink
   for(int k=1; k<nbDays_; ++k)
      dualValues[k] = pModel_->getDual(master_->workFlowCons_[i][k-1], true);

   return dualValues;
}

vector<double> RotationPricer::getEndWorkDualValues(LiveNurse* pNurse){
   int i = pNurse->id_;
   vector<double> dualValues(nbDays_);

   //get dual values associated to the work flow constraints
   //don't take into account the first which is the source
   //take into account the cost, if the last day worked is k
   for(int k=0; k<nbDays_-1; ++k)
      dualValues[k] = -pModel_->getDual(master_->restFlowCons_[i][k+1], true);

   //get dual value associated to the sink
   dualValues[nbDays_-1] =  pModel_->getDual(
      master_->workFlowCons_[i][nbDays_-1], true);

   return dualValues;
}

double RotationPricer::getWorkedWeekendDualValue(LiveNurse* pNurse){
  int id = pNurse->id_;
  double dualVal = pModel_->getDual(master_->maxWorkedWeekendCons_[id], true);
  if (master_->isMaxWorkedWeekendAvgCons_[id]) {
    dualVal += pModel_->getDual(master_->maxWorkedWeekendAvgCons_[id], true);
  }
  if (master_->isMaxWorkedWeekendContractAvgCons_[pNurse->pContract_->id_]) {
     dualVal += pModel_->getDual(master_->maxWorkedWeekendContractAvgCons_[pNurse->pContract_->id_], true);
  }

  return dualVal;
}

/******************************************************
 * add some forbidden shifts
 ******************************************************/
void RotationPricer::computeForbiddenShifts(set<pair<int,int>>& forbiddenShifts, vector<Rotation> rotations){
   //search best rotation
   vector<Rotation>::iterator bestRotation;
   double bestDualcost = DBL_MAX;
   for(vector<Rotation>::iterator it = rotations.begin(); it != rotations.end(); ++it)
      if(it->dualCost_ < bestDualcost){
         bestDualcost = it->dualCost_;
         bestRotation = it;
      }

   //forbid shifts of the best rotation
   if(bestDualcost != DBL_MAX)
      for(pair<int,int> pair: bestRotation->shifts_)
            forbiddenShifts.insert(pair);

}


//////////////////////////////////////////////////////////////
//
// B R A N C H I N G  R U L E
//
//////////////////////////////////////////////////////////////

bool compareObject(const pair<MyVar*,double>& p1, const pair<MyVar*,double>& p2){
   return (p1.second < p2.second);
}

/*************************************************************
 * Diving branching rule: dive then close to .5
 *************************************************************/

/* Constructs the branching rule object. */
DiveBranchingRule::DiveBranchingRule(MasterProblem* master, const char* name):
                        MyBranchingRule(name), master_(master), pModel_(master->getModel()), nbBranchingCandidates_(5)
{ }

//add all good candidates
void DiveBranchingRule::logical_fixing(vector<MyVar*>& fixingCandidates){
   //look for fractional columns
   //Fix all column above BRANCH_LB
   //search the good candidates
   vector<pair<MyVar*,double> > candidates;
   for(int i=0; i<master_->getRotations().size(); ++i)
      for(pair<MyVar*, Rotation> var: master_->getRotations()[i]){
         double value = pModel_->getVarValue(var.first);
         //if the rotation is not a real rotation (length = 0), continue
         if( var.second.length_==0)
            continue;
         //if var fractional
         if(value > EPSILON && value < 1 - EPSILON)
            candidates.push_back(pair<MyVar*, double>(var.first, 1 - value));
      }

   stable_sort(candidates.begin(), candidates.end(), compareObject);

   double valueLeft = 0.99;
   for(pair<MyVar*,double>& p: candidates){
      if(valueLeft < p.second)
         break;
      fixingCandidates.push_back(p.first);
      valueLeft -= p.second;
   }
}

void DiveBranchingRule::branching_candidates(vector<MyVar*>& branchingCandidates){
   branchOnRestingArcs(branchingCandidates);
}

/* branch on a set of resting arcs */
void DiveBranchingRule::branchOnRestingArcs(vector<MyVar*>& branchingCandidates){
   //set of rest variable closest to .5
   int bestDay = -1;
   LiveNurse* pBestNurse(nullptr);
   double bestValue = DBL_MAX;

   for(LiveNurse* pNurse: master_->theLiveNurses_)
      for(int k=0; k<master_->pDemand_->nbDays_; ++k){
         double value = 0;
         //choose the set of arcs the closest to .5
         for(MyVar* var: master_->getRestsPerDay(pNurse)[k])
            value += pModel_->getVarValue(var);

         //The value has to be not integer
         if(value < EPSILON || value > 1 - EPSILON)
            continue;

         double frac = value - floor(value);
         double closeTo5 = abs(0.5-frac);

         if(closeTo5 < bestValue){
            bestDay = k;
            pBestNurse = pNurse;
            bestValue = closeTo5;
            if(closeTo5<EPSILON)
               break;
         }
      }

   if(pBestNurse != nullptr)
      for(MyVar* var: master_->getRestsPerDay(pBestNurse)[bestDay])
         branchingCandidates.push_back(var);

   master_->pModel_->setLastBranchingRest(pair<LiveNurse*, int>(pBestNurse, bestDay));
}

/* branch on the number of nurses */
void DiveBranchingRule::branchOnNumberOfNurses(vector<MyVar*>& branchingCandidates){
   //search all candidates
   if(mediumCandidates_.size() == 0)
      for(MyVar* var: pModel_->getIntegerCoreVars()){
         string str2 = "nursesNumber";
         string str0(var->name_);
         string str1 = str0.substr(0,str2.size());
         if(strcmp(str1.c_str(), str2.c_str()) == 0)
            bestCandidates_.push_back(var);
         else
            mediumCandidates_.push_back(var);
      }

   MyVar *bestVar(0);
   double bestValue = DBL_MAX;

   //manage integrality on the skill allocation variables
   switch(searchStrategy_){
   case DepthFirstSearch:
      //variable closest to upper integer
      for(MyVar* var: bestCandidates_){
         if(pModel_->isInteger(var))
            continue;

         double value = pModel_->getVarValue(var);
         double frac = value - floor(value);
         double closeToInt = 1-frac;

         if(closeToInt < bestValue){
            bestVar = var;
            bestValue = closeToInt;
            if(closeToInt<EPSILON)
               break;
         }
      }

      if(bestVar != 0)
         break;

      for(MyVar* var: mediumCandidates_){
         if(pModel_->isInteger(var))
            continue;

         double value = pModel_->getVarValue(var);
         double frac = value - floor(value);
         double closeToInt = 1-frac;

         if(closeToInt < bestValue){
            bestVar = var;
            bestValue = closeToInt;
            if(closeToInt<EPSILON)
               break;
         }
      }

      break;
   default:
      //variable closest to .5
      for(MyVar* var: bestCandidates_){
         if(pModel_->isInteger(var))
            continue;

         double value = pModel_->getVarValue(var);
         double frac = value - floor(value);
         double closeTo5 = abs(0.5-frac);

         if(closeTo5 < bestValue){
            bestVar = var;
            bestValue = closeTo5;
            if(closeTo5<EPSILON)
               break;
         }
      }

      if(bestVar != 0)
         break;

      for(MyVar* var: mediumCandidates_){
         if(pModel_->isInteger(var))
            continue;

         double value = pModel_->getVarValue(var);
         double frac = value - floor(value);
         double closeTo5 = abs(0.5-frac);

         if(closeTo5 < bestValue){
            bestVar = var;
            bestValue = closeTo5;
            if(closeTo5<EPSILON)
               break;
         }
      }

      break;
   }

   if(bestVar != 0)
      branchingCandidates.push_back(bestVar);
}

bool DiveBranchingRule::compareColumnCloseToInt(pair<MyVar*, double> obj1, pair<MyVar*, double> obj2){
   double frac1 = obj1.second - floor(obj1.second), frac2 = obj2.second - floor(obj2.second);
   double closeToInt1 = 1-frac1, closeToInt2 = 1-frac2;
   return (closeToInt1 < closeToInt2);
}

bool DiveBranchingRule::compareColumnCloseTo5(pair<MyVar*, double> obj1, pair<MyVar*, double> obj2){
   double frac1 = obj1.second - floor(obj1.second), frac2 = obj2.second - floor(obj2.second);
   double closeTo5_1 = abs(0.5-frac1), closeTo5_2 = abs(0.5-frac2);
   return (closeTo5_1 < closeTo5_2);
}

/*************************************************************
 * CorePriority branching rule: branch on core variables first
 *************************************************************/

/* Constructs the branching rule object. */
CorePriorityBranchingRule::CorePriorityBranchingRule(Modeler* pModel, const char* name):
                        MyBranchingRule(name), pModel_(pModel)
{ }

//remove all bad candidates from fixingCandidates while keeping the order
void CorePriorityBranchingRule::logical_fixing(vector<MyVar*>& fixingCandidates){
   //choose the var nursesNumber
   for(MyVar* var: pModel_->getIntegerCoreVars()){
      string str2 = "nursesNumber";
      string str0(var->name_);
      string str1 = str0.substr(0,str2.size());
      if(strcmp(str1.c_str(), str2.c_str()) == 0)
         fixingCandidates.push_back(var);
   }
}

//remove all worst/best candidates from fixingCandidates while keeping the order
void CorePriorityBranchingRule::branching_candidates(vector<MyVar*>& branchingCandidates){
   //choose the var nursesNumber
   for(MyVar* var: pModel_->getIntegerCoreVars()){
      string str2 = "skillsAlloc";
      string str0(var->name_);
      string str1 = str0.substr(0,str2.size());
      if(strcmp(str1.c_str(), str2.c_str()) == 0)
         branchingCandidates.push_back(var);
   }
}
