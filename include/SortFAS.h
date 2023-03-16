/**
 * SortFAS.h - SortFAS Solver
*/
#ifndef __SORT_FAS_H__
#define __SORT_FAS_H__

#include "Strategy.h"

class SortFAS: public FASStrategy {
public:
    std::vector<int> getFAS(Graph &g) override;
};

#endif // __SORT_FAS_H__