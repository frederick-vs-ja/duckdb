//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/aggregate/ungrouped_aggregate_state.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/function/aggregate_function.hpp"
#include "duckdb/planner/expression/bound_aggregate_expression.hpp"
#include "duckdb/execution/operator/aggregate/aggregate_object.hpp"

namespace duckdb {
struct DistinctAggregateData;
struct LocalUngroupedAggregateState;

struct UngroupedAggregateState {
	explicit UngroupedAggregateState(const vector<unique_ptr<Expression>> &aggregate_expressions);
	~UngroupedAggregateState();

	void Move(UngroupedAggregateState &other);

public:
	//! Aggregates
	const vector<unique_ptr<Expression>> &aggregate_expressions;
	//! The aggregate values
	vector<unsafe_unique_array<data_t>> aggregate_data;
	//! The bind data
	vector<optional_ptr<FunctionData>> bind_data;
	//! The destructors
	vector<aggregate_destructor_t> destructors;
	//! Counts (used for verification)
	unique_array<atomic<idx_t>> counts;
};

struct GlobalUngroupedAggregateState {
public:
	GlobalUngroupedAggregateState(Allocator &client_allocator, const vector<unique_ptr<Expression>> &aggregates)
	    : client_allocator(client_allocator), allocator(client_allocator), state(aggregates) {
	}

	mutable mutex lock;
	//! Client allocator
	Allocator &client_allocator;
	//! Global arena allocator
	ArenaAllocator allocator;
	//! Allocator pool
	mutable vector<unique_ptr<ArenaAllocator>> stored_allocators;
	//! The global aggregate state
	UngroupedAggregateState state;

public:
	//! Create an ArenaAllocator with cross-thread lifetime
	ArenaAllocator &CreateAllocator() const;
	void Combine(LocalUngroupedAggregateState &other);
	void CombineDistinct(LocalUngroupedAggregateState &other, DistinctAggregateData &distinct_data);
	void Finalize(DataChunk &result, idx_t column_offset = 0);
};

struct LocalUngroupedAggregateState {
public:
	explicit LocalUngroupedAggregateState(GlobalUngroupedAggregateState &gstate);

	//! Local arena allocator
	ArenaAllocator &allocator;
	//! The local aggregate state
	UngroupedAggregateState state;

public:
	void Sink(DataChunk &payload_chunk, idx_t payload_idx, idx_t aggr_idx);
};

struct UngroupedAggregateExecuteState {
public:
	UngroupedAggregateExecuteState(ClientContext &context, const vector<unique_ptr<Expression>> &aggregates,
	                               const vector<LogicalType> &child_types);

	//! The set of aggregates
	const vector<unique_ptr<Expression>> &aggregates;
	//! The executor
	ExpressionExecutor child_executor;
	//! The payload chunk, containing all the Vectors for the aggregates
	DataChunk aggregate_input_chunk;
	//! Aggregate filter data set
	AggregateFilterDataSet filter_set;

public:
	void Sink(LocalUngroupedAggregateState &state, DataChunk &input);
	void Reset();
};

} // namespace duckdb
