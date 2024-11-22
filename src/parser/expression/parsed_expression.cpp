#include "parser/expression/parsed_expression.h"

#include "common/exception/binder.h"
#include "common/serializer/deserializer.h"
#include "common/serializer/serializer.h"
#include "function/sequence/sequence_functions.h"
#include "parser/expression/parsed_case_expression.h"
#include "parser/expression/parsed_function_expression.h"
#include "parser/expression/parsed_literal_expression.h"
#include "parser/expression/parsed_parameter_expression.h"
#include "parser/expression/parsed_property_expression.h"
#include "parser/expression/parsed_subquery_expression.h"
#include "parser/expression/parsed_variable_expression.h"

using namespace kuzu::common;

namespace kuzu {
namespace parser {

ParsedExpression::ParsedExpression(ExpressionType type, std::unique_ptr<ParsedExpression> child,
    std::string rawName)
    : type{type}, rawName{std::move(rawName)} {
    children.push_back(std::move(child));
}

ParsedExpression::ParsedExpression(ExpressionType type, std::unique_ptr<ParsedExpression> left,
    std::unique_ptr<ParsedExpression> right, std::string rawName)
    : type{type}, rawName{std::move(rawName)} {
    children.push_back(std::move(left));
    children.push_back(std::move(right));
}

void ParsedExpression::serialize(Serializer& serializer) const {
    serializer.serializeValue(type);
    serializer.serializeValue(alias);
    serializer.serializeValue(rawName);
    serializer.serializeVectorOfPtrs(children);
    serializeInternal(serializer);
}

std::unique_ptr<ParsedExpression> ParsedExpression::deserialize(Deserializer& deserializer) {
    auto type = ExpressionType::INVALID;
    std::string alias;
    std::string rawName;
    parsed_expr_vector children;
    deserializer.deserializeValue(type);
    deserializer.deserializeValue(alias);
    deserializer.deserializeValue(rawName);
    deserializer.deserializeVectorOfPtrs(children);
    std::unique_ptr<ParsedExpression> parsedExpression;
    switch (type) {
    case ExpressionType::CASE_ELSE: {
        parsedExpression = ParsedCaseExpression::deserialize(deserializer);
    } break;
    case ExpressionType::FUNCTION: {
        parsedExpression = ParsedFunctionExpression::deserialize(deserializer);
    } break;
    case ExpressionType::LITERAL: {
        parsedExpression = ParsedLiteralExpression::deserialize(deserializer);
    } break;
    case ExpressionType::PARAMETER: {
        parsedExpression = ParsedParameterExpression::deserialize(deserializer);
    } break;
    case ExpressionType::PROPERTY: {
        parsedExpression = ParsedPropertyExpression::deserialize(deserializer);
    } break;
    case ExpressionType::SUBQUERY: {
        parsedExpression = ParsedSubqueryExpression::deserialize(deserializer);
    } break;
    case ExpressionType::VARIABLE: {
        parsedExpression = ParsedVariableExpression::deserialize(deserializer);
    } break;
    default: {
        KU_UNREACHABLE;
    }
    }
    parsedExpression->alias = std::move(alias);
    parsedExpression->rawName = std::move(rawName);
    parsedExpression->children = std::move(children);
    return parsedExpression;
}

std::unique_ptr<ParsedExpression> ParsedExpressionUtils::getSerialDefaultExpr(
    const std::string& sequenceName) {
    auto literalExpr = std::make_unique<parser::ParsedLiteralExpression>(Value(sequenceName), "");
    return std::make_unique<parser::ParsedFunctionExpression>(function::NextValFunction::name,
        std::move(literalExpr), "");
}

void ParsedExpressionUtils::validateExpressionType(const kuzu::parser::ParsedExpression& expr,
    common::ExpressionType type) {
    if (expr.getExpressionType() == type) {
        return;
    }
    throw common::BinderException(stringFormat("{} has type {} but {} was expected.",
        expr.toString(), ExpressionTypeUtil::toString(expr.getExpressionType()),
        ExpressionTypeUtil::toString(type)));
}

void ParsedExpressionUtils::validateDataType(const kuzu::parser::ParsedExpression& expr,
    const common::LogicalType& type) {
    KU_ASSERT(expr.getExpressionType() == ExpressionType::LITERAL);
    auto& literalExpr = expr.constCast<ParsedLiteralExpression>();
    if (literalExpr.getValue().getDataType() == type) {
        return;
    }
    throw common::BinderException(stringFormat("{} has type {} but {} was expected.",
        expr.toString(), literalExpr.getValue().getDataType().toString(), type.toString()));
}

std::string ParsedExpressionUtils::getStringLiteralValue(
    const kuzu::parser::ParsedExpression& expr) {
    return expr.constCast<parser::ParsedLiteralExpression>().getValue().getValue<std::string>();
}

} // namespace parser
} // namespace kuzu
