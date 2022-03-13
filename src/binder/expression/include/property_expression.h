#pragma once

#include "expression.h"

namespace graphflow {
namespace binder {

const string INTERNAL_ID_SUFFIX = "_id";

class PropertyExpression : public Expression {

public:
    PropertyExpression(DataType dataType, const string& propertyName, uint32_t propertyKey,
        const shared_ptr<Expression>& child);

    inline string getPropertyName() const { return propertyName; }

    inline uint32_t getPropertyKey() const { return propertyKey; }

    inline bool isInternalID() const { return getPropertyName() == INTERNAL_ID_SUFFIX; }

private:
    string propertyName;
    uint32_t propertyKey;
};

using property_vector = vector<shared_ptr<PropertyExpression>>;

} // namespace binder
} // namespace graphflow
