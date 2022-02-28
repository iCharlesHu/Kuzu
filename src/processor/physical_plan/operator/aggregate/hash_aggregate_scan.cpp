#include "src/processor/include/physical_plan/operator/aggregate/hash_aggregate_scan.h"

namespace graphflow {
namespace processor {

shared_ptr<ResultSet> HashAggregateScan::initResultSet() {
    auto result = BaseAggregateScan::initResultSet();
    for (auto i = 0u; i < groupByKeyVectorsPos.size(); i++) {
        auto valueVector =
            make_shared<ValueVector>(context.memoryManager, groupByKeyVectorDataTypes[i]);
        auto outDataChunk = resultSet->dataChunks[groupByKeyVectorsPos[i].dataChunkPos];
        outDataChunk->insert(groupByKeyVectorsPos[i].valueVectorPos, valueVector);
        groupByKeyVectors.push_back(valueVector);
    }
    groupByKeyVectorsColIdxes.resize(groupByKeyVectors.size());
    iota(groupByKeyVectorsColIdxes.begin(), groupByKeyVectorsColIdxes.end(), 1);
    return result;
}

bool HashAggregateScan::getNextTuples() {
    metrics->executionTime.start();
    auto [startOffset, endOffset] = sharedState->getNextRangeToRead();
    if (startOffset >= endOffset) {
        metrics->executionTime.stop();
        return false;
    }
    auto numRowsToScan = endOffset - startOffset;
    sharedState->getFactorizedTable()->scan(
        groupByKeyVectors, startOffset, numRowsToScan, groupByKeyVectorsColIdxes);
    for (auto pos = 0u; pos < numRowsToScan; ++pos) {
        auto entry = sharedState->getRow(startOffset + pos);
        auto offset = sharedState->getFactorizedTable()->getTableSchema().getColOffset(
            1 + groupByKeyVectors.size());
        for (auto& vector : aggregateVectors) {
            auto aggState = (AggregateState*)(entry + offset);
            writeAggregateResultToVector(vector, pos, aggState);
            offset += aggState->getStateSize();
        }
    }
    metrics->executionTime.stop();
    metrics->numOutputTuple.increase(numRowsToScan);
    return true;
}

} // namespace processor
} // namespace graphflow
