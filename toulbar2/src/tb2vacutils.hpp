/** \file tb2vacutils.hpp
 *  \brief Set of useful classes to enforce VAC
 */
 
#ifndef TB2VACUTILS_HPP_
#define TB2VACUTILS_HPP_

#include "tb2binconstr.hpp"


class VACVariable : public EnumeratedVariable {

public:

private:
  
  VACExtension* vac;         /**< Ref. to the VAC data-structures */

  vector<Long>  mark;        /**< The boolean M used to mark values whose deletion is needed to wipe-out */
  vector<Long>  k_timeStamp; /**< timestamp for the counter k (one per value) */
  vector<int>   k;           /**< Number of cost requests per value for all cost functions */
  vector<int>   killer;      /**< The killer of each value : the other variable index (binary case)*/

  int  maxk;                 /**< The Max number of cost requests seen on this variable, used for stats */
  Long maxk_timeStamp;       /**< timestamp for maxk */


  StoreCost myThreshold; /** The local thresold used to break loops */
  //Cost 	        myThreshold;

  DLink<VariableWithTimeStamp> linkVACQueue;
  DLink<VariableWithTimeStamp> linkSeekSupport;
  DLink<Variable *>            linkVAC2Queue;

  void init ();

public:

  VACVariable (WCSP *wcsp, string n, Value iinf, Value isup);
  VACVariable (WCSP *wcsp, string n, Value *d, int dsize);
  ~VACVariable ();

  void reset ();

  bool removeVAC(Value v);
  bool increaseVAC(Value newInf);
  bool decreaseVAC(Value supInf);

  int 	getMaxK( long timeStamp ) {
  	if(maxk_timeStamp < timeStamp) return 0;
  	else return maxk; 
  } 

  int 	getK( Value v, long timeStamp ) {
  	if(k_timeStamp[toIndex(v)] < timeStamp) return 0;
  	else return k[toIndex(v)]; 
  } 
  void 	setK( Value v, int c, long timeStamp ) { 
  	k[toIndex(v)] = c; 
  	k_timeStamp[toIndex(v)] = timeStamp; 
  	if(maxk_timeStamp < timeStamp) {
  		maxk = 0;
  		maxk_timeStamp = timeStamp;
  	}
  }

  void	addToK(Value v, int c, long timeStamp ) {
  	 if(k_timeStamp[toIndex(v)] < timeStamp) k[toIndex(v)] = c;
  	 else k[toIndex(v)] += c; 
  	 if(maxk_timeStamp < timeStamp) maxk = k[toIndex(v)];
  	 else if(maxk < k[toIndex(v)]) maxk = k[toIndex(v)];
  	 maxk_timeStamp = timeStamp; 
  	 k_timeStamp[toIndex(v)] = timeStamp; 
  }


  bool  isMarked( Value v, long timeStamp ) { return (mark[toIndex(v)] >= timeStamp); }
  void  setMark( Value v, long timeStamp )  { mark[toIndex(v)] = timeStamp; }
  
  int   getKiller( Value v ) { return killer[toIndex(v)]; }
  void  setKiller( Value v, int i ) { killer[toIndex(v)] = i; }


  Cost getVACCost( Value v ) {
	Cost c = getCost(v);
	if(isNull(c)) return MIN_COST;
	else return c;
  }

  void setThreshold (Cost c) { myThreshold = c; } 
  Cost getThreshold () { return myThreshold; } 
  bool isNull (Cost c);
   
  void queueVAC();
  void queueSeekSupport();
  void queueVAC2();
  void dequeueVAC2();
  
  // virtual void assign(Value newValue);
  virtual void remove(Value value);
  virtual void removeFast(Value value);
  // virtual void extendAll(Cost cost);
  // virtual void project (Value value, Cost cost);
  // virtual void extend (Value value, Cost cost);
  virtual void increase (Value newInf);
  virtual void decrease (Value newSup);

  void decreaseCost(Value v, Cost c);
  void VACproject(Value v, const Cost c); /**< Increases unary cost and may queue for NC enforcing */
  void VACextend(Value v, const Cost c);  /**< Decreases unary cost and may queue for NC enforcing */
 
  bool averaging();  /**< For Min-Sum diffusion */

  friend ostream& operator<< (ostream& os, VACVariable &v) {
    return os;
  }
};


/**
 * A class that stores information about a binary constraint
 */
class VACConstraint : public BinaryConstraint {

private:
  
  vector<int>  kX;             /**< The k_XY(X,v) counters: nb. of cost request on X,v by this cost function */
  vector<int>  kY;             /**< The k_XY(Y,v) counters: nb. of cost request on Y,v by this cost function */
  vector<Long>  kX_timeStamp;
  vector<Long>  kY_timeStamp;

public:

  VACConstraint (WCSP *wcsp, EnumeratedVariable *xx, EnumeratedVariable *yy, vector<Cost> &tab, StoreStack<Cost, Cost> *storeCost);
  VACConstraint (WCSP *wcsp, StoreStack<Cost, Cost> *storeCost);
  ~VACConstraint ();

  int getK (VACVariable* var, Value v, long timeStamp);
  void setK (VACVariable* var, Value v, int c, long timeStamp);

  Cost getVACCost(VACVariable *xx, VACVariable *yy, Value v, Value w) {
	Cost c = getCost(xx, yy, v, w);
	if(xx->isNull(c) || (yy->isNull(c))) return MIN_COST;
	else return c;
  }
  void VACproject(VACVariable* x, Value v, Cost c); /**< Modifies Delta counters, then VAC projects on value */
  void VACextend (VACVariable* x, Value v, Cost c); /**< Modifies Delta counters, then VAC extends from value */
  
  bool revise (VACVariable* var, Value v);  /**< AC2001 based Revise for Pass1 : Revise value wrt this cost function */

  friend ostream& operator<< (ostream& os, VACConstraint &c) {
    return os;
  }
};


#endif /*TB2VACUTILS_HPP_*/
