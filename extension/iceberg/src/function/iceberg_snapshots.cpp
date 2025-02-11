#include "function/iceberg_functions.h"

namespace kuzu {
namespace iceberg_extension {

using namespace function;
using namespace common;

static std::unique_ptr<TableFuncBindData> snapshotBindFunc(main::ClientContext* context,
    TableFuncBindInput* input) {
    return bindFuncHelper(context, input, IcebergSnapshotsFunction::name);
}

function_set IcebergSnapshotsFunction::getFunctionSet() {
    function_set functionSet;
    functionSet.push_back(std::make_unique<TableFunction>(name, delta_extension::tableFunc,
        snapshotBindFunc, delta_extension::initDeltaScanSharedState,
        delta_extension::initEmptyLocalState, std::vector<LogicalTypeID>{LogicalTypeID::STRING}));
    return functionSet;
}

} // namespace iceberg_extension
} // namespace kuzu
