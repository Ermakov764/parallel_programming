#pragma once
#include "filters.hpp"
#include <vector>

enum class ExecutionMode { Image, Stage, Pipeline };

// Each stage consumes the previous stage's output. The same chain is used
// for all execution modes, so their results are directly comparable.
class Pipeline {
public:
    explicit Pipeline(std::vector<Transformation> stages);
    void run(std::vector<Image>& images, ExecutionMode mode, int threads) const;
private:
    std::vector<Transformation> stages_;
};
