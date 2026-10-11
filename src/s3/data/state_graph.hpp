// Stock TU3 `.stategraph` decoding, independent of graph execution.
//
// ReadElement 82C15DE0 reads a NUL-terminated tag, BinaryAttributeMap
// 82C14F38, a big-endian child count, then children in file order.
// Each attribute stores two strings, a float word and a boolean byte.
// Keep all three representations: parsing the string again loses stock data.
// (skate-data state_graph.rs + state_graph/{attributes,binding}.rs)
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "result.hpp"
#include "vfs.hpp"

namespace s3::data {

struct GraphAttribute
{
	std::string name;
	std::string text;
	uint32_t float_bits = 0;
	uint8_t boolean_byte = 0;
	bool operator==(const GraphAttribute &o) const
	{
		return name == o.name && text == o.text && float_bits == o.float_bits && boolean_byte == o.boolean_byte;
	}
};

struct GraphElement
{
	/// Byte offset of the tag in the original file, for binding diagnostics.
	size_t source_offset = 0;
	std::string tag;
	/// File order, including duplicate names. Native hash-map binding is a
	/// separate operation; the decoder must not silently discard attributes.
	std::vector<GraphAttribute> attributes;
	/// Indices into StateGraph::elements, in original child order.
	std::vector<size_t> children;
};

struct StateGraph
{
	/// Preorder arena. Element zero is the single file root.
	std::vector<GraphElement> elements;

	static Result<StateGraph> load(const Vfs &vfs, const std::string &path);
	static Result<StateGraph> decode(const uint8_t *bytes, size_t size);
};

namespace state_graph_attributes {
//! BinaryAttributeMap keys (82B92B98) and lookup/insert semantics.
//! This is the graph's 32-bit hash, not Attribulator's 64-bit field hash.

/// Attribute/tag callers use zero for missing or empty strings. The underlying
/// 82B92B98 byte hash has a different, nonzero result for a zero-length buffer.
uint32_t key_hash(const std::string &text);
/// Complete 82B92B98, including each tail length and wrapping 32-bit arithmetic.
uint32_t byte_hash(const uint8_t *bytes, size_t size);

/// Native 82C19BB0 is unique insertion by the hashed key: the first record wins
/// for duplicate keys, including collisions between differently spelled keys.
/// Original record order and all encodings remain available in GraphElement.
class Attributes
{
public:
	explicit Attributes(const std::vector<GraphAttribute> &attributes);
	const GraphAttribute *get(const std::string &name) const;
	/// 82C14E10. A present empty string is not the default. nullptr when absent.
	const std::string *text(const std::string &name) const;
	/// 82C151B0 returns the stored byte without boolean normalization.
	uint8_t boolean_byte(const std::string &name, uint8_t default_value) const;
	/// 82C15308; callers of 82C15258 supply the native zero default.
	uint32_t float_bits(const std::string &name, uint32_t default_value) const;

private:
	std::map<uint32_t, const GraphAttribute *> entries_;
};
} // namespace state_graph_attributes

namespace state_graph_binding {
//! Dynamic graph object construction and name binding from TU3 82C15F50,
//! 82C11930, 82C16708, 82C169B8, 82C16D68, 82C16FF0 and 82C16C10.

struct Node
{
	enum Kind { State, Transition, Expression, Operation } kind = State;
	size_t index = 0;
	bool operator==(const Node &o) const { return kind == o.kind && index == o.index; }
	bool operator!=(const Node &o) const { return !(*this == o); }
};

struct State
{
	size_t element = 0;
	std::string name;
	std::optional<size_t> parent;
	std::vector<size_t> children;
	std::vector<size_t> behaviors;
	std::vector<size_t> transitions;
	std::optional<size_t> expression;
	uint8_t enabled = 0;
	uint8_t active = 0;
	uint32_t interruptibility = 0;
	std::optional<size_t> interrupt_ancestor;
};

struct Transition
{
	size_t element = 0;
	size_t owner = 0;
	std::optional<size_t> target;
	uint8_t enabled = 0;
	uint32_t priority = 0;
	std::optional<size_t> expression;
	std::vector<size_t> hooks;
};

struct Expression
{
	size_t element = 0;
	uint8_t enabled = 0;
	uint32_t op = 0; // Rust `operator`
	/// Ordered nested expressions and concrete condition operations.
	std::vector<Node> children;
};

enum class OperationKind { Behavior, Condition, Hook };

struct Operation
{
	size_t element = 0;
	Node parent;
	OperationKind kind = OperationKind::Behavior;
	std::string name;
	uint8_t enabled = 0;
	/// Condition::mMask from 82C12CB0/82C12BB0. Other operation kinds have no
	/// condition mask.
	std::optional<uint32_t> condition_mask;
	/// Ordered `param` elements, consumed by the concrete virtual40 handler.
	std::vector<size_t> parameters;
};

template <class T>
struct OperationInstances
{
	/// Same indices as `Binding::operations`.
	std::vector<T> operations;
};

std::string operation_error_text(const StateGraph &source, const Operation &operation, const std::string &message);

struct Binding
{
	std::vector<State> states;
	std::vector<Transition> transitions;
	std::vector<Expression> expressions;
	std::vector<Operation> operations;
	size_t root = 0;

	/// Binds the compiled dynamic graph structure, not its registered gameplay
	/// operations. The source graph remains the owner of raw typed attributes.
	static Result<Binding> from_graph(const StateGraph &source);

	/// Instantiates every concrete operation from its complete attribute map,
	/// then passes its `param` maps one at a time in stock child order.
	/// Factory (8241BFF0 boundary): `Result<std::optional<Instance>> create(OperationKind, Node,
	/// const Attributes&)` and `Status add_parameter(Instance&, const Attributes&)`.
	/// A missing registration (`std::nullopt`) is rejected with source context.
	template <class Instance, class Factory>
	Result<OperationInstances<Instance>> instantiate_operations(const StateGraph &source, Factory &factory) const
	{
		OperationInstances<Instance> out;
		out.operations.reserve(operations.size());
		for (const Operation &operation : operations) {
			state_graph_attributes::Attributes attributes(source.elements[operation.element].attributes);
			Result<std::optional<Instance>> created = factory.create(operation.kind, operation.parent, attributes);
			if (!created.ok) return Error{operation_error_text(source, operation, created.error)};
			if (!created.value) return Error{operation_error_text(source, operation, "unregistered operation")};
			Instance instance = std::move(*created.value);
			for (size_t element : operation.parameters) {
				state_graph_attributes::Attributes parameter(source.elements[element].attributes);
				Status added = factory.add_parameter(instance, parameter);
				if (!added.ok) return Error{operation_error_text(source, operation, added.error)};
			}
			out.operations.push_back(std::move(instance));
		}
		return out;
	}

	/// Complete 82C16C10 search: first direct child, then self/ancestors if
	/// allowed. Dotted paths search their first component with ascent enabled,
	/// even if the caller disabled ascent, then resolve the suffix locally.
	std::optional<size_t> find_state(size_t start, const std::string &name, bool ascend) const;

private:
	uint8_t enabled(Node node) const;
	Status process_names(const StateGraph &source);
};

/// Native interned-name comparison 82C118A0 uses FNV-1, without string equality
/// after the hash comparison. This differs from BinaryAttributeMap's key hash.
uint32_t name_hash(const std::string &text);
/// StateGraph::ParseMask 82C12BB0. Comparison is case-sensitive. TU3 returns
/// the precondition mask for a missing or unrecognized value. (nullptr = None)
uint32_t parse_condition_mask(const std::string *value);

} // namespace state_graph_binding

} // namespace s3::data
