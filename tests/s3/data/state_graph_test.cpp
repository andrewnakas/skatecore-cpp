// Ported tests: skate-data tests/{state_graph,graph_attributes,graph_binding,graph_operations}.rs.
#include <cstdio>
#include <optional>
#include <tuple>

#include "s3/data/state_graph.hpp"
#include "test.hpp"

using namespace s3::data;
using namespace s3::data::state_graph_binding;
using s3::data::state_graph_attributes::Attributes;

namespace {

std::vector<uint8_t> element_bytes(const std::string &tag, const std::vector<std::tuple<std::string, std::string, uint32_t, uint8_t>> &attributes,
                                   const std::vector<std::vector<uint8_t>> &children)
{
	std::vector<uint8_t> bytes(tag.begin(), tag.end());
	bytes.push_back(0);
	auto be = [&](uint32_t v) {
		for (int i = 3; i >= 0; i--) bytes.push_back((uint8_t)(v >> (8 * i)));
	};
	be((uint32_t)attributes.size());
	for (auto &a : attributes) {
		const std::string &key = std::get<0>(a), &text = std::get<1>(a);
		bytes.insert(bytes.end(), key.begin(), key.end());
		bytes.push_back(0);
		bytes.insert(bytes.end(), text.begin(), text.end());
		bytes.push_back(0);
		be(std::get<2>(a));
		bytes.push_back(std::get<3>(a));
	}
	be((uint32_t)children.size());
	for (auto &c : children) bytes.insert(bytes.end(), c.begin(), c.end());
	return bytes;
}

GraphAttribute attr(const std::string &name, const std::string &text, uint32_t bits, uint8_t byte)
{
	return GraphAttribute{name, text, bits, byte};
}
GraphElement element(const std::string &tag, std::vector<GraphAttribute> attributes, std::vector<size_t> children, size_t offset = 0)
{
	GraphElement e;
	e.source_offset = offset;
	e.tag = tag;
	e.attributes = std::move(attributes);
	e.children = std::move(children);
	return e;
}

} // namespace

// tests/state_graph.rs: preserves_tree_order_duplicate_attributes_and_encoded_values
S3_TEST(state_graph_preserves_tree_order_duplicate_attributes_and_encoded_values)
{
	auto grandchild = element_bytes("condition", {}, {});
	auto first = element_bytes("expression", {{"op", "and", 0, 0}}, {grandchild});
	auto second = element_bytes("behaviour", {{"value", "0", 0x80000000u, 2}, {"value", "1", 0x7fc00001u, 0}}, {});
	auto bytes = element_bytes("state", {{"name", "Root", 0, 0}}, {first, second});
	auto graph = StateGraph::decode(bytes.data(), bytes.size());
	CHECK_OK(graph);
	const auto &e = graph->elements;
	CHECK_EQ(e.size(), (size_t)4);
	CHECK(e[0].tag == "state" && e[1].tag == "expression" && e[2].tag == "condition" && e[3].tag == "behaviour");
	CHECK(e[0].children == std::vector<size_t>({1, 3}));
	CHECK(e[1].children == std::vector<size_t>({2}));
	CHECK_EQ(e[3].attributes[0].float_bits, 0x80000000u);
	CHECK_EQ(e[3].attributes[0].boolean_byte, (uint8_t)2);
	CHECK_EQ(e[3].attributes[1].float_bits, 0x7fc00001u);
	CHECK_EQ(e[3].attributes[1].text, std::string("1"));
	for (size_t i = 0; i + 1 < e.size(); i++) CHECK(e[i].source_offset < e[i + 1].source_offset);
}

// tests/state_graph.rs: rejects_truncation_and_trailing_data_without_partial_graph
S3_TEST(state_graph_rejects_truncation_and_trailing_data_without_partial_graph)
{
	auto bytes = element_bytes("state", {{"name", "Root", 0, 0}}, {element_bytes("behaviour", {}, {})});
	for (size_t end = 0; end < bytes.size(); end++) CHECK_MSG(!StateGraph::decode(bytes.data(), end).ok, "accepted truncation at " + std::to_string(end));
	auto extra = bytes;
	extra.push_back(0);
	CHECK_ERR_CONTAINS(StateGraph::decode(extra.data(), extra.size()), "trailing");
	const uint8_t excessive[] = {'s', 't', 'a', 't', 'e', 0, 0xff, 0xff, 0xff, 0xff};
	CHECK(!StateGraph::decode(excessive, sizeof excessive).ok);
}

// tests/graph_attributes.rs: binary_attributes_keep_first_record_and_separate_native_encodings
S3_TEST(graph_attributes_keep_first_record_and_separate_native_encodings)
{
	std::vector<GraphAttribute> records = {attr("value", "", 0x80000000u, 255), attr("value", "replacement", 0x3f800000u, 0)};
	Attributes attributes(records);
	CHECK(attributes.text("value") && *attributes.text("value") == "");
	CHECK_EQ(attributes.float_bits("value", 0), 0x80000000u);
	CHECK_EQ(attributes.boolean_byte("value", 0), (uint8_t)255);
	CHECK(attributes.text("missing") == nullptr);
	CHECK_EQ(attributes.float_bits("missing", 0x3f800000u), 0x3f800000u);
	CHECK_EQ(attributes.boolean_byte("missing", 2), (uint8_t)2);
}

// tests/graph_binding.rs: constructor_flags_priorities_expressions_and_target_resolution
S3_TEST(graph_binding_constructor_flags_priorities_expressions_and_target_resolution)
{
	StateGraph source;
	source.elements = {
		element("state", {attr("name", "Root", 0, 0)}, {1, 3, 4, 6}),
		element("state", {attr("name", "Off", 0, 0), attr("enabled", "true", 0, 0)}, {2}),
		element("behaviour", {attr("name", "Handler", 0, 0), attr("enabled", "true", 0, 255)}, {}),
		element("transition", {attr("target", "Off", 0, 0), attr("priority", "urgent", 0, 0)}, {}),
		element("state", {attr("name", "On", 0, 0), attr("interruptable", "Root", 0, 0)}, {5}),
		element("transition", {attr("target", "Root.On", 0, 0), attr("priority", "med", 0, 0)}, {}),
		element("transition", {attr("target", "Absent", 0, 0)}, {7}),
		element("expression", {attr("op", "or", 0, 0)}, {8}),
		element("condition", {attr("name", "Condition", 0, 0)}, {}),
	};
	auto b = Binding::from_graph(source);
	CHECK_OK(b);
	const Binding &binding = b.value;
	CHECK(binding.states[0].children == std::vector<size_t>({1, 2}));
	CHECK_EQ(binding.states[1].enabled, (uint8_t)0);
	CHECK_EQ(binding.operations[0].enabled, (uint8_t)0); // Inherited disable wins.
	CHECK_EQ(binding.states[2].interruptibility, 2u);
	CHECK(binding.states[2].interrupt_ancestor == std::optional<size_t>(0));
	CHECK(binding.transitions[0].target == std::optional<size_t>(1));
	CHECK_EQ(binding.transitions[0].enabled, (uint8_t)0); // Target disables transition.
	CHECK_EQ(binding.transitions[0].priority, 3u);
	CHECK(binding.transitions[1].target == std::optional<size_t>(2));
	CHECK_EQ(binding.transitions[1].priority, 1u);
	CHECK(!binding.transitions[2].target.has_value());
	CHECK_EQ(binding.transitions[2].enabled, (uint8_t)0);
	CHECK_EQ(binding.expressions[0].op, 2u);
	CHECK(binding.operations[1].condition_mask == std::optional<uint32_t>(1));
	// Constructor inheritance precedes Process disabling an unresolved target.
	CHECK_EQ(binding.expressions[0].enabled, (uint8_t)1);
}

// tests/graph_binding.rs: condition_masks_match_tu3_parse_mask_ordinals
S3_TEST(graph_binding_condition_masks_match_tu3_parse_mask_ordinals)
{
	auto mask = [](const char *v) {
		std::string s = v ? v : "";
		return parse_condition_mask(v ? &s : nullptr);
	};
	CHECK_EQ(mask(nullptr), 1u);
	CHECK_EQ(mask("precond"), 1u);
	CHECK_EQ(mask("sustain"), 2u);
	CHECK_EQ(mask("always"), 3u);
	CHECK_EQ(mask("postcond"), 4u);
	CHECK_EQ(mask("PRECOND"), 1u);
	CHECK_EQ(mask("unknown"), 1u);
}

// tests/graph_binding.rs: missing_native_operations_and_invalid_ancestry_are_explicit
S3_TEST(graph_binding_missing_native_operations_and_invalid_ancestry_are_explicit)
{
	StateGraph graph;
	graph.elements = {element("include", {}, {})};
	CHECK_ERR_CONTAINS(Binding::from_graph(graph), "unsupported graph element");
	graph.elements = {element("state", {attr("interruptable", "Missing", 0, 0)}, {})};
	CHECK_ERR_CONTAINS(Binding::from_graph(graph), "unresolved interrupt ancestor");
}

namespace {
struct Factory
{
	std::vector<std::string> events;
	bool registered = false;
	static const char *kind_name(OperationKind k)
	{
		return k == OperationKind::Behavior ? "Behavior" : k == OperationKind::Condition ? "Condition" : "Hook";
	}
	static std::string node_name(Node n)
	{
		const char *k = n.kind == Node::State ? "State" : n.kind == Node::Transition ? "Transition" : n.kind == Node::Expression ? "Expression" : "Operation";
		return std::string(k) + "(" + std::to_string(n.index) + ")";
	}
	Result<std::optional<std::string>> create(OperationKind kind, Node parent, const Attributes &attributes)
	{
		std::string name = *attributes.text("name");
		events.push_back(std::string("create:") + kind_name(kind) + ":" + node_name(parent) + ":" + name);
		if (registered) return std::optional<std::string>(name);
		return std::optional<std::string>();
	}
	Status add_parameter(std::string &instance, const Attributes &attributes)
	{
		char hex[16];
		std::snprintf(hex, sizeof hex, "%08X", attributes.float_bits("value", 0));
		events.push_back("param:" + instance + ":" + *attributes.text("name") + ":" + hex + ":" +
		                 std::to_string(attributes.boolean_byte("value", 0)));
		return Unit{};
	}
};

StateGraph operations_source()
{
	StateGraph g;
	g.elements = {
		element("state", {attr("name", "Root", 0, 0)}, {1}, 0x40),
		element("behaviour", {attr("name", "Drive", 0, 0)}, {2, 3}, 0x40),
		element("param", {attr("name", "first", 0, 0), attr("value", "one", 0x3F800000u, 7)}, {}, 0x40),
		element("param", {attr("name", "second", 0, 0), attr("value", "two", 0x40000000u, 9)}, {}, 0x40),
	};
	return g;
}
} // namespace

// tests/graph_operations.rs: factory_receives_complete_operation_then_ordered_typed_parameters
S3_TEST(graph_operations_factory_receives_complete_operation_then_ordered_typed_parameters)
{
	StateGraph source = operations_source();
	auto binding = Binding::from_graph(source);
	CHECK_OK(binding);
	Factory factory;
	factory.registered = true;
	auto instances = binding->instantiate_operations<std::string>(source, factory);
	CHECK_OK(instances);
	CHECK(instances->operations == std::vector<std::string>({"Drive"}));
	CHECK(factory.events ==
	      std::vector<std::string>({"create:Behavior:State(0):Drive", "param:Drive:first:3F800000:7", "param:Drive:second:40000000:9"}));
}

// tests/graph_operations.rs: missing_registration_is_a_source_located_error
S3_TEST(graph_operations_missing_registration_is_a_source_located_error)
{
	StateGraph source = operations_source();
	auto binding = Binding::from_graph(source);
	CHECK_OK(binding);
	Factory factory;
	auto r = binding->instantiate_operations<std::string>(source, factory);
	CHECK(!r.ok);
	CHECK(r.error.find("byte 64") != std::string::npos);
	CHECK(r.error.find("behaviour `Drive`: unregistered operation") != std::string::npos);
}

S3_TEST(graph_hashes)
{
	// FNV-1 of the empty string is the offset basis; key_hash("") is zero by contract.
	CHECK_EQ(name_hash(""), 0x811c9dc5u);
	CHECK_EQ(s3::data::state_graph_attributes::key_hash(""), 0u);
	// Lookup2 (Bob Jenkins) of an empty buffer with the 0xabcdef00 initial value differs from zero.
	CHECK(s3::data::state_graph_attributes::byte_hash(nullptr, 0) != 0u);
}
