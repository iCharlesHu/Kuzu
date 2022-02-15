#pragma once

#include "src/binder/expression/include/rel_expression.h"
#include "src/binder/query/return_with_clause/include/bound_projection_body.h"
#include "src/planner/include/logical_plan/logical_plan.h"
#include "src/storage/include/catalog.h"

using namespace graphflow::storage;
using namespace graphflow::binder;

namespace graphflow {
namespace planner {

class Enumerator;

class ProjectionEnumerator {

public:
    explicit ProjectionEnumerator(const Catalog& catalog, Enumerator* enumerator)
        : catalog{catalog}, enumerator{enumerator} {}

    void enumerateProjectionBody(const BoundProjectionBody& projectionBody,
        const vector<unique_ptr<LogicalPlan>>& plans, bool isFinalReturn);

    static void appendDistinct(
        const vector<shared_ptr<Expression>>& expressionsToDistinct, LogicalPlan& plan);

private:
    void enumerateAggregate(const BoundProjectionBody& projectionBody, LogicalPlan& plan);
    void enumerateOrderBy(const BoundProjectionBody& projectionBody, LogicalPlan& plan);
    void enumerateProjection(const vector<shared_ptr<Expression>>& expressionsToProject,
        bool isDistinct, LogicalPlan& plan);
    void enumerateSkipAndLimit(const BoundProjectionBody& projectionBody, LogicalPlan& plan);

    void appendProjection(
        const vector<shared_ptr<Expression>>& expressionsToProject, LogicalPlan& plan);
    void appendAggregate(const vector<shared_ptr<Expression>>& expressionsToGroupBy,
        const vector<shared_ptr<Expression>>& expressionsToAggregate, LogicalPlan& plan);
    void appendOrderBy(const vector<shared_ptr<Expression>>& expressions,
        const vector<bool>& isAscOrders, LogicalPlan& plan);
    void appendMultiplicityReducer(LogicalPlan& plan);
    void appendLimit(uint64_t limitNumber, LogicalPlan& plan);
    void appendSkip(uint64_t skipNumber, LogicalPlan& plan);

    vector<shared_ptr<Expression>> getExpressionToGroupBy(
        const BoundProjectionBody& projectionBody, const Schema& schema);
    vector<shared_ptr<Expression>> getExpressionsToAggregate(
        const BoundProjectionBody& projectionBody, const Schema& schema);
    vector<shared_ptr<Expression>> getExpressionsToProject(
        const BoundProjectionBody& projectionBody, const Schema& schema,
        bool isRewritingAllProperties);
    vector<shared_ptr<Expression>> rewriteVariableExpression(const shared_ptr<Expression>& variable,
        const Schema& schema, bool isRewritingAllProperties);
    vector<shared_ptr<Expression>> rewriteNodeExpressionAsAllProperties(
        const shared_ptr<NodeExpression>& node);
    vector<shared_ptr<Expression>> rewriteRelExpressionAsAllProperties(
        const shared_ptr<RelExpression>& rel);
    vector<shared_ptr<Expression>> createPropertyExpressions(
        const shared_ptr<Expression>& variable, const vector<PropertyDefinition>& properties);

private:
    const Catalog& catalog;
    Enumerator* enumerator;
};

} // namespace planner
} // namespace graphflow