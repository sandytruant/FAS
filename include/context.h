/**
 * context.h - 上下文类
*/
#ifndef __CONTEXT_H__
#define __CONTEXT_H__
#include "PageRankFAS.hpp"

// 上下文类
class FASContext {
public:
    std::vector<std::pair<int, int>> getFeedbackArcSet(std::string input) {
        PageRankFAS *strategy = new PageRankFAS();
        return strategy->getFeedbackArcSet(input);
    }
};

#endif // __CONTEXT_H__