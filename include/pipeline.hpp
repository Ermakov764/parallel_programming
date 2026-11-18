#pragma once
#include "filters.hpp"
#include <vector>

enum class ExecutionMode { FileFirst, StageFirst, Pipeline };

// Each stage consumes the previous stage's output. The same chain is used
// for all execution modes, so their results are directly comparable.
class Pipeline {
public:
    static Pipeline standard();
    explicit Pipeline(std::vector<Transformation> stages);
    const std::vector<Transformation>& stages() const { return stages_; }
    void run(std::vector<Image>& images, ExecutionMode mode, int threads) const;
private:
    std::vector<Transformation> stages_;
};
