//
//  ErrorPercentageRefinementStrategy.h
//  Camellia-debug
//
//  Created by Nate Roberts on 6/20/14.
//
//

#ifndef __Camellia_debug__ErrorPercentageRefinementStrategy__
#define __Camellia_debug__ErrorPercentageRefinementStrategy__

#include "RefinementStrategy.h"

#include "Solution.h"

class ErrorPercentageRefinementStrategy : public RefinementStrategy {
  double _percentageThreshold;
public:
  ErrorPercentageRefinementStrategy(SolutionPtr soln, double percentageThreshold, double min_h = 0, int max_p = 10, bool preferPRefinements = false);
  virtual void refine(bool printToConsole);
};


#endif /* defined(__Camellia_debug__ErrorPercentageRefinementStrategy__) */
