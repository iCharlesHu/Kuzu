#include "storage/store/rel_table_data.h"

#include "catalog/catalog_entry/rel_table_catalog_entry.h"
#include "common/enums/rel_direction.h"
#include "common/types/types.h"
#include "main/client_context.h"
#include "storage/storage_utils.h"
#include "storage/store/node_group.h"
#include "storage/store/rel_table.h"
#include "transaction/transaction.h"

using namespace kuzu::catalog;
using namespace kuzu::common;
using namespace kuzu::transaction;

namespace kuzu {
namespace storage {

PersistentVersionRecordHandler::PersistentVersionRecordHandler(RelTableData* relTableData)
    : relTableData(relTableData) {}

void PersistentVersionRecordHandler::applyFuncToChunkedGroups(version_record_handler_op_t func,
    common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow, common::row_idx_t numRows,
    common::transaction_t commitTS) const {
    if (nodeGroupIdx < relTableData->getNumNodeGroups()) {
        auto& nodeGroup = relTableData->getNodeGroupNoLock(nodeGroupIdx)->cast<CSRNodeGroup>();
        auto* persistentChunkedGroup = nodeGroup.getPersistentChunkedGroup();
        if (persistentChunkedGroup) {
            std::invoke(func, *persistentChunkedGroup, startRow, numRows, commitTS);
        }
    }
}

void PersistentVersionRecordHandler::rollbackInsert(const transaction::Transaction* transaction,
    common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
    common::row_idx_t numRows) const {
    VersionRecordHandler::rollbackInsert(transaction, nodeGroupIdx, startRow, numRows);
    relTableData->rollbackGroupCollectionInsert(numRows, true);
}

InMemoryVersionRecordHandler::InMemoryVersionRecordHandler(RelTableData* relTableData)
    : relTableData(relTableData) {}

void InMemoryVersionRecordHandler::applyFuncToChunkedGroups(version_record_handler_op_t func,
    common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow, common::row_idx_t numRows,
    common::transaction_t commitTS) const {
    auto* nodeGroup = relTableData->getNodeGroupNoLock(nodeGroupIdx);
    nodeGroup->applyFuncToChunkedGroups(func, startRow, numRows, commitTS);
}

void InMemoryVersionRecordHandler::rollbackInsert(const transaction::Transaction* transaction,
    common::node_group_idx_t nodeGroupIdx, common::row_idx_t startRow,
    common::row_idx_t numRows) const {
    VersionRecordHandler::rollbackInsert(transaction, nodeGroupIdx, startRow, numRows);
    auto* nodeGroup = relTableData->getNodeGroupNoLock(nodeGroupIdx);
    const auto numRowsToRollback = std::min(numRows, nodeGroup->getNumRows() - startRow);
    nodeGroup->rollbackInsert(startRow);
    relTableData->rollbackGroupCollectionInsert(numRowsToRollback, false);
}

RelTableData::RelTableData(FileHandle* dataFH, MemoryManager* mm, ShadowFile* shadowFile,
    const TableCatalogEntry* tableEntry, RelDataDirection direction, bool enableCompression,
    Deserializer* deSer)
    : dataFH{dataFH}, tableID{tableEntry->getTableID()}, tableName{tableEntry->getName()},
      memoryManager{mm}, shadowFile{shadowFile}, enableCompression{enableCompression},
      direction{direction}, persistentVersionRecordHandler(this),
      inMemoryVersionRecordHandler(this) {
    multiplicity = tableEntry->constCast<RelTableCatalogEntry>().getMultiplicity(direction);
    initCSRHeaderColumns();
    initPropertyColumns(tableEntry);

    // default to using the persistent version record handler
    // if we want to use the in-memory handler we will explicitly pass it into
    // nodeGroups.pushInsertInfo()
    nodeGroups = std::make_unique<NodeGroupCollection>(*mm, getColumnTypes(), enableCompression,
        dataFH, deSer, &persistentVersionRecordHandler);
}

void RelTableData::initCSRHeaderColumns() {
    // No NULL values is allowed for the csr length and offset column.
    auto csrOffsetColumnName = StorageUtils::getColumnName("", StorageUtils::ColumnType::CSR_OFFSET,
        RelDataDirectionUtils::relDirectionToString(direction));
    csrHeaderColumns.offset = std::make_unique<Column>(csrOffsetColumnName, LogicalType::UINT64(),
        dataFH, memoryManager, shadowFile, enableCompression, false /* requireNUllColumn */);
    auto csrLengthColumnName = StorageUtils::getColumnName("", StorageUtils::ColumnType::CSR_LENGTH,
        RelDataDirectionUtils::relDirectionToString(direction));
    csrHeaderColumns.length = std::make_unique<Column>(csrLengthColumnName, LogicalType::UINT64(),
        dataFH, memoryManager, shadowFile, enableCompression, false /* requireNUllColumn */);
}

void RelTableData::initPropertyColumns(const TableCatalogEntry* tableEntry) {
    const auto maxColumnID = tableEntry->getMaxColumnID();
    columns.resize(maxColumnID + 1);
    auto nbrIDColName = StorageUtils::getColumnName("NBR_ID", StorageUtils::ColumnType::DEFAULT,
        RelDataDirectionUtils::relDirectionToString(direction));
    auto nbrIDColumn = std::make_unique<InternalIDColumn>(nbrIDColName, dataFH, memoryManager,
        shadowFile, enableCompression);
    columns[NBR_ID_COLUMN_ID] = std::move(nbrIDColumn);
    for (auto i = 0u; i < tableEntry->getNumProperties(); i++) {
        auto& property = tableEntry->getProperty(i);
        const auto columnID = tableEntry->getColumnID(property.getName());
        const auto colName =
            StorageUtils::getColumnName(property.getName(), StorageUtils::ColumnType::DEFAULT,
                RelDataDirectionUtils::relDirectionToString(direction));
        columns[columnID] = ColumnFactory::createColumn(colName, property.getType().copy(), dataFH,
            memoryManager, shadowFile, enableCompression);
    }
    // Set common tableID for nbrIDColumn and relIDColumn.
    const auto nbrTableID = tableEntry->constCast<RelTableCatalogEntry>().getNbrTableID(direction);
    columns[NBR_ID_COLUMN_ID]->cast<InternalIDColumn>().setCommonTableID(nbrTableID);
    columns[REL_ID_COLUMN_ID]->cast<InternalIDColumn>().setCommonTableID(tableID);
}

bool RelTableData::update(Transaction* transaction, ValueVector& boundNodeIDVector,
    const ValueVector& relIDVector, column_id_t columnID, const ValueVector& dataVector) const {
    KU_ASSERT(boundNodeIDVector.state->getSelVector().getSelSize() == 1);
    KU_ASSERT(relIDVector.state->getSelVector().getSelSize() == 1);
    const auto boundNodePos = boundNodeIDVector.state->getSelVector()[0];
    const auto relIDPos = relIDVector.state->getSelVector()[0];
    if (boundNodeIDVector.isNull(boundNodePos) || relIDVector.isNull(relIDPos)) {
        return false;
    }
    const auto [source, rowIdx] = findMatchingRow(transaction, boundNodeIDVector, relIDVector);
    KU_ASSERT(rowIdx != INVALID_ROW_IDX);
    const auto boundNodeOffset = boundNodeIDVector.getValue<nodeID_t>(boundNodePos).offset;
    const auto nodeGroupIdx = StorageUtils::getNodeGroupIdx(boundNodeOffset);
    auto& csrNodeGroup = getNodeGroup(nodeGroupIdx)->cast<CSRNodeGroup>();
    csrNodeGroup.update(transaction, source, rowIdx, columnID, dataVector);
    return true;
}

bool RelTableData::delete_(Transaction* transaction, ValueVector& boundNodeIDVector,
    const ValueVector& relIDVector) {
    const auto boundNodePos = boundNodeIDVector.state->getSelVector()[0];
    const auto relIDPos = relIDVector.state->getSelVector()[0];
    if (boundNodeIDVector.isNull(boundNodePos) || relIDVector.isNull(relIDPos)) {
        return false;
    }
    const auto [source, rowIdx] = findMatchingRow(transaction, boundNodeIDVector, relIDVector);
    if (rowIdx == INVALID_ROW_IDX) {
        return false;
    }
    const auto boundNodeOffset = boundNodeIDVector.getValue<nodeID_t>(boundNodePos).offset;
    const auto nodeGroupIdx = StorageUtils::getNodeGroupIdx(boundNodeOffset);
    auto& csrNodeGroup = getNodeGroup(nodeGroupIdx)->cast<CSRNodeGroup>();
    bool isDeleted = csrNodeGroup.delete_(transaction, source, rowIdx);
    if (isDeleted && transaction->shouldAppendToUndoBuffer()) {
        transaction->pushDeleteInfo(nodeGroupIdx, rowIdx, 1, getVersionRecordHandler(source));
    }
    return isDeleted;
}

void RelTableData::addColumn(Transaction* transaction, TableAddColumnState& addColumnState) {
    auto& definition = addColumnState.propertyDefinition;
    columns.push_back(ColumnFactory::createColumn(definition.getName(), definition.getType().copy(),
        dataFH, memoryManager, shadowFile, enableCompression));
    nodeGroups->addColumn(transaction, addColumnState);
}

std::pair<CSRNodeGroupScanSource, row_idx_t> RelTableData::findMatchingRow(Transaction* transaction,
    ValueVector& boundNodeIDVector, const ValueVector& relIDVector) const {
    KU_ASSERT(boundNodeIDVector.state->getSelVector().getSelSize() == 1);
    KU_ASSERT(relIDVector.state->getSelVector().getSelSize() == 1);
    const auto boundNodePos = boundNodeIDVector.state->getSelVector()[0];
    const auto relIDPos = relIDVector.state->getSelVector()[0];
    const auto boundNodeOffset = boundNodeIDVector.getValue<nodeID_t>(boundNodePos).offset;
    const auto relOffset = relIDVector.getValue<nodeID_t>(relIDPos).offset;
    const auto nodeGroupIdx = StorageUtils::getNodeGroupIdx(boundNodeOffset);

    DataChunk scanChunk(1);
    // RelID output vector.
    scanChunk.insert(0, std::make_shared<ValueVector>(LogicalType::INTERNAL_ID()));
    std::vector<column_id_t> columnIDs = {REL_ID_COLUMN_ID, ROW_IDX_COLUMN_ID};
    std::vector<const Column*> columns{getColumn(REL_ID_COLUMN_ID), nullptr};
    const auto scanState = std::make_unique<RelTableScanState>(
        *transaction->getClientContext()->getMemoryManager(), tableID, columnIDs, columns,
        csrHeaderColumns.offset.get(), csrHeaderColumns.length.get(), direction);
    scanState->nodeIDVector = &boundNodeIDVector;
    scanState->outputVectors.push_back(&scanChunk.getValueVectorMutable(0));
    const auto scannedIDVector = scanState->outputVectors[0];
    scanState->outState = scannedIDVector->state.get();
    scanState->rowIdxVector->state = scannedIDVector->state;
    scanState->initState(transaction, getNodeGroup(nodeGroupIdx));
    row_idx_t matchingRowIdx = INVALID_ROW_IDX;
    auto source = CSRNodeGroupScanSource::NONE;
    while (true) {
        const auto scanResult = scanState->nodeGroup->scan(transaction, *scanState);
        if (scanResult == NODE_GROUP_SCAN_EMMPTY_RESULT) {
            break;
        }
        for (auto i = 0u; i < scanState->outState->getSelVector().getSelSize(); i++) {
            const auto pos = scanState->outState->getSelVector()[i];
            if (scannedIDVector->getValue<internalID_t>(pos).offset == relOffset) {
                const auto rowIdxPos = scanState->rowIdxVector->state->getSelVector()[i];
                matchingRowIdx = scanState->rowIdxVector->getValue<row_idx_t>(rowIdxPos);
                source = scanState->nodeGroupScanState->cast<CSRNodeGroupScanState>().source;
                break;
            }
        }
        if (matchingRowIdx != INVALID_ROW_IDX) {
            break;
        }
    }
    return {source, matchingRowIdx};
}

bool RelTableData::checkIfNodeHasRels(Transaction* transaction,
    ValueVector* srcNodeIDVector) const {
    KU_ASSERT(srcNodeIDVector->state->isFlat());
    const auto nodeIDPos = srcNodeIDVector->state->getSelVector()[0];
    const auto nodeOffset = srcNodeIDVector->getValue<nodeID_t>(nodeIDPos).offset;
    const auto nodeGroupIdx = StorageUtils::getNodeGroupIdx(nodeOffset);
    if (nodeGroupIdx >= getNumNodeGroups()) {
        return false;
    }
    DataChunk scanChunk(1);
    // RelID output vector.
    scanChunk.insert(0, std::make_shared<ValueVector>(LogicalType::INTERNAL_ID()));
    std::vector<column_id_t> columnIDs = {REL_ID_COLUMN_ID};
    std::vector<const Column*> columns{getColumn(REL_ID_COLUMN_ID)};
    const auto scanState = std::make_unique<RelTableScanState>(
        *transaction->getClientContext()->getMemoryManager(), tableID, columnIDs, columns,
        csrHeaderColumns.offset.get(), csrHeaderColumns.length.get(), direction);
    scanState->nodeIDVector = srcNodeIDVector;
    scanState->outputVectors.push_back(&scanChunk.getValueVectorMutable(0));
    scanState->outState = scanState->outputVectors[0]->state.get();
    scanState->initState(transaction, getNodeGroup(nodeGroupIdx));
    while (true) {
        const auto scanResult = scanState->nodeGroup->scan(transaction, *scanState);
        if (scanResult == NODE_GROUP_SCAN_EMMPTY_RESULT) {
            break;
        }
        if (scanState->outState->getSelVector().getSelSize() > 0) {
            return true;
        }
    }
    return false;
}

void RelTableData::pushInsertInfo(transaction::Transaction* transaction,
    const CSRNodeGroup& nodeGroup, common::row_idx_t numRows_, CSRNodeGroupScanSource source) {
    // we shouldn't be appending directly to the to the persistent data
    // unless we are performing batch insert and the persistent chunked group is empty
    KU_ASSERT(source != CSRNodeGroupScanSource::COMMITTED_PERSISTENT ||
              !nodeGroup.getPersistentChunkedGroup() ||
              nodeGroup.getPersistentChunkedGroup()->getNumRows() == 0);

    const auto [startRow, shouldIncrementNumRows] =
        (source == CSRNodeGroupScanSource::COMMITTED_PERSISTENT) ?
            std::make_pair(static_cast<row_idx_t>(0), false) :
            std::make_pair(nodeGroup.getNumRows(), true);

    nodeGroups->pushInsertInfo(transaction, nodeGroup.getNodeGroupIdx(), startRow, numRows_,
        getVersionRecordHandler(source), shouldIncrementNumRows);
}

void RelTableData::checkpoint(const std::vector<column_id_t>& columnIDs) {
    std::vector<std::unique_ptr<Column>> checkpointColumns;
    for (auto i = 0u; i < columnIDs.size(); i++) {
        const auto columnID = columnIDs[i];
        checkpointColumns.push_back(std::move(columns[columnID]));
    }
    columns = std::move(checkpointColumns);

    std::vector<Column*> checkpointColumnPtrs;
    for (const auto& column : columns) {
        checkpointColumnPtrs.push_back(column.get());
    }

    CSRNodeGroupCheckpointState state{columnIDs, std::move(checkpointColumnPtrs), *dataFH,
        memoryManager, csrHeaderColumns.offset.get(), csrHeaderColumns.length.get()};
    nodeGroups->checkpoint(*memoryManager, state);
}

void RelTableData::serialize(Serializer& serializer) const {
    nodeGroups->serialize(serializer);
}

const VersionRecordHandler* RelTableData::getVersionRecordHandler(CSRNodeGroupScanSource source) {
    if (source == CSRNodeGroupScanSource::COMMITTED_PERSISTENT) {
        return &persistentVersionRecordHandler;
    } else {
        KU_ASSERT(source == CSRNodeGroupScanSource::COMMITTED_IN_MEMORY);
        return &inMemoryVersionRecordHandler;
    }
}

void RelTableData::rollbackGroupCollectionInsert(common::row_idx_t numRows_, bool isPersistent) {
    nodeGroups->rollbackInsert(numRows_, !isPersistent);
}

} // namespace storage
} // namespace kuzu
