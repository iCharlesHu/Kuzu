#include "gtest/gtest.h"

#include "src/processor/include/physical_plan/operator/scan/physical_scan.h"

using namespace graphflow::processor;

TEST(ScanTests, ScanTest) {
    auto morsel = make_shared<MorselDesc>(1025013 /*numNodes*/);
    auto scan = make_unique<PhysicalScan<true>>(morsel);
    auto dataChunks = scan->getDataChunks();
    auto dataChunk = dataChunks->getDataChunk(0);
    auto nodeVector =
        static_pointer_cast<NodeIDVector>(dataChunks->getDataChunk(0)->getValueVector(0));
    node_offset_t currNodeOffset = 0;
    auto size = NODE_SEQUENCE_VECTOR_SIZE;
    while (morsel->currNodeOffset < 1025013) {
        ASSERT_EQ(scan->hasNextMorsel(), true);
        scan->getNextTuples();
        if (morsel->currNodeOffset >= 1025013) {
            size = 1025013 % NODE_SEQUENCE_VECTOR_SIZE;
        }
        ASSERT_EQ(dataChunk->size, size);
        nodeID_t node;
        for (uint64_t i = 0; i < dataChunk->size; i++) {
            nodeVector->readNodeOffset(i, node);
            ASSERT_EQ(node.offset, currNodeOffset + i);
        }
        currNodeOffset += NODE_SEQUENCE_VECTOR_SIZE;
    }
    ASSERT_EQ(morsel->currNodeOffset, 1025013);
    ASSERT_EQ(scan->hasNextMorsel(), false);
}
