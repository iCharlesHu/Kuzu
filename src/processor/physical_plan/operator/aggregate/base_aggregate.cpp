#include "src/processor/include/physical_plan/operator/aggregate/base_aggregate.h"

namespace graphflow {
namespace processor {

BaseAggregateSharedState::BaseAggregateSharedState(
    const vector<unique_ptr<AggregateFunction>>& aggregateFunctions)
    : currentOffset{0} {
    for (auto& aggregateFunction : aggregateFunctions) {
        this->aggregateFunctions.push_back(aggregateFunction->clone());
    }
}

bool BaseAggregate::containDistinctAggregate() const {
    for (auto& aggregateFunction : aggregateFunctions) {
        if (aggregateFunction->isFunctionDistinct()) {
            return true;
        }
    }
    return false;
}

shared_ptr<ResultSet> BaseAggregate::initResultSet() {
    resultSet = children[0]->initResultSet();
    for (auto& dataPos : aggregateVectorsPos) {
        if (dataPos.dataChunkPos == UINT32_MAX) {
            // COUNT(*) aggregate does not take any input vector.
            aggregateVectors.push_back(nullptr);
        } else {
            auto dataChunk = resultSet->dataChunks[dataPos.dataChunkPos];
            aggregateVectors.push_back(dataChunk->valueVectors[dataPos.valueVectorPos].get());
        }
    }
    return resultSet;
}

} // namespace processor
} // namespace graphflow
