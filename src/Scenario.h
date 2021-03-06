//
//  Scenario.h
//  RosterDesNurses
//

#ifndef __Scenario__
#define __Scenario__

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include "MyTools.h"
#include "Nurse.h"

using std::map;
using std::pair;
using std::string;
using std::vector;

// the penalties for violating the soft constraints on the nurses' schedules
// are in the problem definition
// they are set as static constant values in case they need to be shared with
// other classes (e.g. solvers)
//
static const int WEIGHT_OPTIMAL_DEMAND    = 30;
static const int WEIGHT_CONS_SHIFTS       = 15;
static const int WEIGHT_CONS_DAYS_WORK    = 30;
static const int WEIGHT_CONS_DAYS_OFF	  = 30;
static const int WEIGHT_PREFERENCES       = 10;
static const int WEIGHT_COMPLETE_WEEKEND  = 30;
static const int WEIGHT_TOTAL_SHIFTS      = 20;
static const int WEIGHT_TOTAL_WEEKENDS    = 30;

//-----------------------------------------------------------------------------
//
//  C l a s s   S c e n a r i o
//
//  Class that contains all the attributes describing the scenario
//
//-----------------------------------------------------------------------------

class Scenario {

public:

	// Constructor and destructor
	//
	Scenario(string name, int nbWeeks,
			int nbSkills, vector<string> intToSkill, map<string,int> skillToInt,
			int nbShifts, vector<string> intToShift, map<string,int> shiftToInt,
			vector<int> minConsShifts, vector<int> maxConsShifts,
			vector<int> nbForbiddenSuccessors, vector2D forbiddenSuccessors,
			int nbContracts, vector<string> intToContract, map<string,Contract*> contracts,
			int nbNurses, vector<Nurse> theNurses, map<string,int> nurseNameToInt);

	~Scenario();


	//constant attributes are public
public:

	// name of the scenario
	//
	const std::string name_;

	// total number of weeks and current week being planned
	//
	const int nbWeeks_;

	// number of skills, a map and a vector matching the name of each skill to an
	// index and reversely
	//
	const int nbSkills_;
	const vector<string> intToSkill_;
	const map<string,int> skillToInt_;

	// number of shifts, a map and a vector matching the name of each shift to an
	// index and reversely
	// minimum and maximum number consecutive assignments for each shift,
	// and penalty for violations of these bounds
	//
	const int nbShifts_;
	const vector<string> intToShift_;
	const map<string,int> shiftToInt_;
	const vector<int> minConsShifts_, maxConsShifts_;

	// for each shift, the number of forbidden successors and a table containing
	// the indices of these forbidden successors
	//
	const vector<int> nbForbiddenSuccessors_;
	const vector2D forbiddenSuccessors_;

	// Vector of possible contract types
	//
	const int nbContracts_;
	const vector<string> intToContract_;
	const map<string, Contract*> contracts_;

	// number of nurses, and vector of all the nurses
	//
	const int nbNurses_;
	const vector<Nurse> theNurses_;
	map<string,int> nurseNameToInt_;


private:
	//------------------------------------------------
	// From the Week data file
	//------------------------------------------------
	// Name of the week
	string weekName_;
	// Current week demand for each DAY, SHIFT, and SKILL
	//
	Demand* pWeekDemand_;

	// Shift off requests : Preferences for each nurse : which (day,shift) do they want off ?
	//
	int nbShiftOffRequests_;
	Preferences weekPreferences_;
	//------------------------------------------------


	//------------------------------------------------
	// From the History data file
	//------------------------------------------------
	// Initial historical state of the nurses
	//
	vector<State> initialState_;
	// range of the weeks that are being scheduled
	//
	int thisWeek_;
	int nbWeeksLoaded_;
	//------------------------------------------------


	//------------------------------------------------
	// From the custom file
	//------------------------------------------------
	//------------------------------------------------

	//------------------------------------------------
	// From the preprocessing of the nurses
	//------------------------------------------------
	// Vector of existing positions
	//
	int nbPositions_;
	vector<Position*> pPositions_;
	vector<vector<Nurse*> > nursesPerPosition_;

	//------------------------------------------------

public:

	//------------------------------------------------
	// Getters and setters
	//------------------------------------------------

	// getters for the private class attributes
	//
	int nbWeeks() {return nbWeeks_;}
	int thisWeek() {return thisWeek_;}
	int nbWeeksLoaded() {return nbWeeksLoaded_;}
	string weekName() {return weekName_;}
	Demand* pWeekDemand() {return pWeekDemand_;}
	int nbShifts() {return nbShifts_;}
	int nbShiftOffRequests() {return nbShiftOffRequests_;}
	Preferences* pWeekPreferences() {return &weekPreferences_;}
	vector<State>* pInitialState() {return &initialState_;}
	int nbSkills() {return nbSkills_;}
	int nbPositions() {return nbPositions_;}
	vector<Position*> pPositions() {return pPositions_;}
	Position* pPosition(int p) {return pPositions_[p];}
	int nbNurses() {return nbNurses_;}

	// getters for the attributes of the nurses
	//
	const int minTotalShiftsOf(int whichNurse) {
		return theNurses_[whichNurse].minTotalShifts();
	}
	int maxTotalShiftsOf(int whichNurse) {
		return theNurses_[whichNurse].maxTotalShifts();
	}
	int minConsDaysWorkOf(int whichNurse) {
		return theNurses_[whichNurse].minConsDaysWork();
	}
	int maxConsDaysWorkOf(int whichNurse) {
		return theNurses_[whichNurse].maxConsDaysWork();
	}
	int minConsDaysOffOf(int whichNurse) {
		return theNurses_[whichNurse].maxConsDaysOff();
	}
	int maxConsDaysOffOf(int whichNurse) {
		return theNurses_[whichNurse].maxConsDaysOff();
	}
	int maxTotalWeekendsOf(int whichNurse) {
		return theNurses_[whichNurse].maxTotalWeekends();
	}
	bool isCompleteWeekendsOf(int whichNurse) {
		return theNurses_[whichNurse].needCompleteWeekends();
	}

	// getters for the attribute of the demand
	//
	int nbDays() {return pWeekDemand_->nbDays_;}

	// Setters to class attributes

	// when reading the week file (Demand and preferences)
	//
	inline void setWeekName(string weekName){ weekName_ = weekName;}
	inline void setWeekDemand(Demand* pDemand) {pWeekDemand_ = pDemand;}
	inline void setTNbShiftOffRequests(int nbShiftOffRequests){ nbShiftOffRequests_ = nbShiftOffRequests; }
	inline void setWeekPreferences(Preferences weekPreferences){ weekPreferences_ = weekPreferences; }

	// when reading the history file
	//
	inline void setThisWeek(int thisWeek){ thisWeek_ = thisWeek; }
   inline void addAWeek(){ ++nbWeeksLoaded_; }
	inline void setInitialState(vector<State> initialState){ initialState_ = initialState;}

	// return true if the shift shNext is a forbidden successor of sh
	//
	bool isForbiddenSuccessor(int shNext, int sh);

	// update the scenario to treat a new week
	//
	void updateNewWeek(Demand* pDemand, Preferences &preferences, vector<State> &initialStates);

	// Link the scenario with the Demand and the Preferences
	//
	inline void linkWithDemand(Demand* pDemand){
	   weekName_ = pDemand->name_;
	   pWeekDemand_ = pDemand;
	}

	inline void linkWithPreferences(Preferences pPreferences){
      nbShiftOffRequests_ = 0;
      for(int i=0; i<pPreferences.wishesOff_.size(); ++i)
         nbShiftOffRequests_ += pPreferences.wishesOff_[i].size();
      weekPreferences_ = pPreferences;
   }

public:

	//------------------------------------------------
	// Display functions
	//------------------------------------------------

  // display the whole scenario
  //
  string toString();

	//------------------------------------------------
	// Preprocess functions
	//------------------------------------------------

	// preprocess the nurses to get the types
	//
	void preprocessTheNurses();





};


#endif /* defined(__ATCSolver__CftSolver__) */
