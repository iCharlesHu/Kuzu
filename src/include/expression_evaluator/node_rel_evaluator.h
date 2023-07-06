#pragma once

#include "base_evaluator.h"
#include "binder/expression/node_expression.h"

namespace kuzu {
namespace evaluator {

class NodeRelExpressionEvaluator : public BaseExpressionEvaluator {
public:
    NodeRelExpressionEvaluator(std::shared_ptr<binder::Expression> nodeOrRel,
        std::vector<std::unique_ptr<BaseExpressionEvaluator>> children)
        : BaseExpressionEvaluator{std::move(children)}, nodeOrRel{std::move(nodeOrRel)} {}

    void evaluate() override;

    bool select(common::SelectionVector& selVector) override {
        throw common::NotImplementedException("NodeExpressionEvaluator::select");
    }

    std::unique_ptr<BaseExpressionEvaluator> clone() override {
        std::vector<std::unique_ptr<BaseExpressionEvaluator>> clonedChildren;
        for (auto& child : children) {
            clonedChildren.push_back(child->clone());
        }
        return make_unique<NodeRelExpressionEvaluator>(nodeOrRel, std::move(clonedChildren));
    }

private:
    void resolveResultVector(
        const processor::ResultSet& resultSet, storage::MemoryManager* memoryManager) override;

private:
    std::shared_ptr<binder::Expression> nodeOrRel;
    std::vector<std::shared_ptr<common::ValueVector>> parameters;
};

} // namespace evaluator
} // namespace kuzu
