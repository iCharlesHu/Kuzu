#pragma once

#include <memory>

#include "common/copy_constructors.h"
#include "common/json_enums.h"
#include "common/vector/value_vector.h"
#include "main/client_context.h"
#include "yyjson.h"

namespace kuzu {
namespace json_extension {

class JsonMutWrapper;

class JsonWrapper {
    JsonWrapper() : ptr{nullptr} {}

public:
    explicit JsonWrapper(yyjson_doc* ptr, std::shared_ptr<char[]> buffer = nullptr)
        : ptr{ptr}, buffer{buffer} {}
    ~JsonWrapper();
    JsonWrapper(JsonWrapper& other) = delete;
    JsonWrapper(JsonWrapper&& other) {
        ptr = other.ptr;
        other.ptr = nullptr;
        buffer = std::move(other.buffer);
    }

    yyjson_doc* ptr;
    std::shared_ptr<char[]> buffer;
};

class JsonMutWrapper {
public:
    JsonMutWrapper() : ptr{yyjson_mut_doc_new(nullptr)} {}
    explicit JsonMutWrapper(yyjson_mut_doc* ptr) : ptr{ptr} {}
    ~JsonMutWrapper();
    JsonMutWrapper(JsonMutWrapper& other) = delete;
    JsonMutWrapper(JsonMutWrapper&& other) {
        ptr = other.ptr;
        other.ptr = nullptr;
    }

    yyjson_mut_doc* ptr;
};

JsonWrapper jsonify(const common::ValueVector& vec, uint64_t pos);
yyjson_mut_val* jsonify(JsonMutWrapper& wrapper, const common::ValueVector& vec, uint64_t pos);
yyjson_mut_val* jsonifyAsString(JsonMutWrapper& wrapper, const common::ValueVector& vec,
    uint64_t pos);
// Converts an internal Kuzu Value into json

std::vector<JsonWrapper> jsonifyQueryResult(
    const std::vector<std::shared_ptr<common::ValueVector>>& columns,
    const std::vector<std::string>& names);
// Converts an entire query result into a sequence of json values
common::LogicalType jsonSchema(const JsonWrapper& wrapper, int64_t depth = -1,
    int64_t breadth = -1);
common::LogicalType jsonSchema(yyjson_val* val, int64_t depth, int64_t breadth);
// depth indicates at what nested depth to stop
// breadth indicates the limit of how many children the root nested type is sampled
// -1 means to scan the whole thing
// may return ANY

void readJsonToValueVector(yyjson_val* val, common::ValueVector& vec, uint64_t pos);

std::string jsonToString(const JsonWrapper& wrapper);
std::string jsonToString(const yyjson_val* val);
JsonWrapper stringToJson(const std::string& str);
JsonWrapper stringToJsonNoError(const std::string& str);
// format can be 'unstructured' or 'array'

JsonWrapper mergeJson(const JsonWrapper& A, const JsonWrapper& B);

std::string jsonExtractToString(const JsonWrapper& wrapper, uint64_t pos);
std::string jsonExtractToString(const JsonWrapper& wrapper, std::string path);

uint32_t jsonArraySize(const JsonWrapper& wrapper);

// Recursive functions for json_contain:
// Helper function to check scalar values for strict equality
bool jsonContainsScalar(yyjson_val* haystack, yyjson_val* needle);
// Function to check if a needle array is contained within a haystack array
bool jsonContainsArrArr(yyjson_val* haystack, yyjson_val* needle);
// Function to check if any element in a haystack array matches the needle
bool jsonContainsArr(yyjson_val* haystack, yyjson_val* needle);
// Helper function to check if the current level of a haystack object matches the needle object
bool jsonContainsObjObjAtCurrentLevel(yyjson_val* haystack, yyjson_val* needle);
// Function to search recursively for a needle object within a haystack object
bool jsonContainsObjObj(yyjson_val* haystack, yyjson_val* needle);
// Function to search recursively through all values in a haystack object
bool jsonContainsObj(yyjson_val* haystack, yyjson_val* needle);
// Main jsonContains function that delegates to specific functions based on type
bool jsonContains(yyjson_val* haystack, yyjson_val* needle);
// Wrapper function for use with JsonWrapper objects
bool jsonContains(const JsonWrapper& haystack, const JsonWrapper& needle);

std::vector<std::string> jsonGetKeys(const JsonWrapper& wrapper);

} // namespace json_extension
} // namespace kuzu
