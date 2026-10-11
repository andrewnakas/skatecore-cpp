// Stock `.stategraph` decoding and binding (state_graph.rs, attributes.rs, binding.rs).
#include "state_graph.hpp"

namespace s3::data {

// ---- state_graph.rs ----

namespace {
struct GraphReader
{
	const uint8_t *bytes;
	size_t size;
	size_t offset = 0;

	Error error(const std::string &message) const { return Error{"stategraph byte 0x" + hex_upper(offset) + ": " + message}; }

	Result<const uint8_t *> take(size_t count)
	{
		if (count > size - offset) return error("truncated record");
		const uint8_t *p = bytes + offset;
		offset += count;
		return p;
	}
	Result<uint32_t> word()
	{
		S3_TRY(const uint8_t *p, take(4));
		return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
	}
	Result<std::string> string()
	{
		size_t length = 0;
		while (offset + length < size && bytes[offset + length] != 0) length++;
		if (offset + length >= size) return error("unterminated string");
		size_t start = offset;
		S3_TRY(const uint8_t *s, take(length + 1));
		// Native storage is bytes. Unsupported encodings fail explicitly;
		// lossy conversion could change node names or binding keys.
		if (!is_utf8(s, length)) return Error{"stategraph byte 0x" + hex_upper(start) + ": non-UTF-8 string"};
		return std::string((const char *)s, length);
	}
	Result<std::pair<GraphElement, uint32_t>> element()
	{
		GraphElement e;
		e.source_offset = offset;
		S3_TRY(e.tag, string());
		S3_TRY(uint32_t attribute_count, word());
		// Each attribute requires at least two terminators, four bytes and
		// one byte. Bound allocations by actual input, not a guessed graph cap.
		if ((size_t)attribute_count > (size - offset) / 7) return error("attribute count exceeds remaining data");
		e.attributes.reserve(attribute_count);
		for (uint32_t i = 0; i < attribute_count; i++) {
			GraphAttribute a;
			S3_TRY(a.name, string());
			S3_TRY(a.text, string());
			S3_TRY(a.float_bits, word());
			S3_TRY(const uint8_t *b, take(1));
			a.boolean_byte = b[0];
			e.attributes.push_back(std::move(a));
		}
		S3_TRY(uint32_t children, word());
		// A minimal child is one terminator and two four-byte counts.
		if ((size_t)children > (size - offset) / 9) return error("child count exceeds remaining data");
		return std::make_pair(std::move(e), children);
	}
};
} // namespace

Result<StateGraph> StateGraph::load(const Vfs &vfs, const std::string &path)
{
	S3_TRY_MAP(SharedBytes bytes, vfs.read(path), [&](const std::string &e) { return "Cannot read graph " + path + ": " + e; });
	S3_TRY_MAP(StateGraph g, decode(bytes.data, bytes.size), [&](const std::string &e) { return "Invalid graph " + path + ": " + e; });
	return g;
}

Result<StateGraph> StateGraph::decode(const uint8_t *bytes, size_t size)
{
	GraphReader reader{bytes, size};
	S3_TRY(auto root, reader.element());
	StateGraph g;
	g.elements.push_back(std::move(root.first));
	std::vector<std::pair<size_t, uint32_t>> parents{{0, root.second}};
	// The native reader recurses. An explicit stack preserves preorder and
	// permits deeply nested input without depending on the host stack size.
	while (!parents.empty()) {
		auto &top = parents.back();
		if (top.second == 0) {
			parents.pop_back();
			continue;
		}
		top.second -= 1;
		size_t parent = top.first;
		S3_TRY(auto child, reader.element());
		size_t index = g.elements.size();
		g.elements[parent].children.push_back(index);
		g.elements.push_back(std::move(child.first));
		parents.push_back({index, child.second});
	}
	if (reader.offset != size) return reader.error("trailing bytes after the root element");
	return g;
}

// ---- attributes.rs ----

namespace state_graph_attributes {

static void mix(uint32_t &a, uint32_t &b, uint32_t &c)
{
	a = (a - b - c) ^ (c >> 13);
	b = (b - c - a) ^ (a << 8);
	c = (c - a - b) ^ (b >> 13);
	a = (a - b - c) ^ (c >> 12);
	b = (b - c - a) ^ (a << 16);
	c = (c - a - b) ^ (b >> 5);
	a = (a - b - c) ^ (c >> 3);
	b = (b - c - a) ^ (a << 10);
	c = (c - a - b) ^ (b >> 15);
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

uint32_t byte_hash(const uint8_t *bytes, size_t size)
{
	uint32_t a = 0x9e3779b9u;
	uint32_t b = a;
	uint32_t c = 0xabcdef00u;
	size_t full = size / 12 * 12;
	for (size_t at = 0; at < full; at += 12) {
		a += le32(bytes + at);
		b += le32(bytes + at + 4);
		c += le32(bytes + at + 8);
		mix(a, b, c);
	}
	c += (uint32_t)size;
	for (size_t i = 0; i < size - full; i++) {
		uint32_t byte = bytes[full + i];
		if (i <= 3) a += byte << (8 * i);
		else if (i <= 7) b += byte << (8 * (i - 4));
		else c += byte << (8 * (i - 7));
	}
	mix(a, b, c);
	return c;
}

uint32_t key_hash(const std::string &text)
{
	if (text.empty()) return 0;
	return byte_hash((const uint8_t *)text.data(), text.size());
}

Attributes::Attributes(const std::vector<GraphAttribute> &attributes)
{
	for (const GraphAttribute &a : attributes) entries_.emplace(key_hash(a.name), &a); // emplace keeps the first
}

const GraphAttribute *Attributes::get(const std::string &name) const
{
	auto it = entries_.find(key_hash(name));
	return it == entries_.end() ? nullptr : it->second;
}

const std::string *Attributes::text(const std::string &name) const
{
	const GraphAttribute *a = get(name);
	return a ? &a->text : nullptr;
}

uint8_t Attributes::boolean_byte(const std::string &name, uint8_t default_value) const
{
	const GraphAttribute *a = get(name);
	return a ? a->boolean_byte : default_value;
}

uint32_t Attributes::float_bits(const std::string &name, uint32_t default_value) const
{
	const GraphAttribute *a = get(name);
	return a ? a->float_bits : default_value;
}

} // namespace state_graph_attributes

// ---- binding.rs ----

namespace state_graph_binding {

using state_graph_attributes::Attributes;
using state_graph_attributes::key_hash;

static Error graph_error(const StateGraph &source, size_t element, const std::string &message)
{
	const GraphElement &e = source.elements[element];
	return Error{"Graph " + e.tag + " at byte " + std::to_string(e.source_offset) + ": " + message};
}

static const char *operation_kind(OperationKind kind)
{
	switch (kind) {
	case OperationKind::Behavior: return "behaviour";
	case OperationKind::Condition: return "condition";
	case OperationKind::Hook: return "hook";
	}
	return "";
}

std::string operation_error_text(const StateGraph &source, const Operation &operation, const std::string &message)
{
	return graph_error(source, operation.element, std::string(operation_kind(operation.kind)) + " `" + operation.name + "`: " + message)
	    .message;
}

uint32_t name_hash(const std::string &text)
{
	uint32_t hash = 0x811c9dc5u;
	for (unsigned char byte : text) hash = (hash * 0x01000193u) ^ (uint32_t)byte;
	return hash;
}

uint32_t parse_condition_mask(const std::string *value)
{
	if (value) {
		if (*value == "sustain") return 2;
		if (*value == "always") return 3;
		if (*value == "postcond") return 4;
	}
	return 1;
}

uint8_t Binding::enabled(Node node) const
{
	switch (node.kind) {
	case Node::State: return states[node.index].enabled;
	case Node::Transition: return transitions[node.index].enabled;
	case Node::Expression: return expressions[node.index].enabled;
	case Node::Operation: return operations[node.index].enabled;
	}
	return 0;
}

Result<Binding> Binding::from_graph(const StateGraph &source)
{
	Binding result;
	size_t n = source.elements.size();
	std::vector<std::optional<size_t>> parents(n);
	std::vector<std::optional<Node>> nodes(n);
	const uint32_t STATE = key_hash("state"), TRANSITION = key_hash("transition"), EXPRESSION = key_hash("expression"),
	               PARAM = key_hash("param"), BEHAVIOUR = key_hash("behaviour"), CONDITION = key_hash("condition"),
	               HOOK = key_hash("hook");
	for (size_t i = 0; i < n; i++) {
		const GraphElement &element = source.elements[i];
		for (size_t child : element.children) {
			if (child <= i || child >= parents.size() || parents[child].has_value()) return graph_error(source, i, "invalid preorder ownership");
			parents[child] = i;
		}
		std::optional<Node> parent;
		if (parents[i]) parent = nodes[*parents[i]];
		Attributes attrs(element.attributes);
		uint8_t enabled = (parent && result.enabled(*parent) == 0) ? 0 : attrs.boolean_byte("enabled", 1);
		const std::string *name_p = attrs.text("name");
		std::string name = name_p ? *name_p : "__unknown__";
		uint32_t tag = key_hash(element.tag);
		Node node;
		if (tag == STATE) {
			std::optional<size_t> state_parent;
			if (!parent && i == 0) {
			} else if (parent && parent->kind == Node::State) {
				state_parent = parent->index;
			} else {
				return graph_error(source, i, "state requires a state parent");
			}
			const std::string *interruptable = attrs.text("interruptable");
			std::string value = interruptable ? *interruptable : "true";
			uint32_t interruptibility = value == "true" ? 1 : value == "false" ? 0 : 2;
			size_t id = result.states.size();
			State s;
			s.element = i;
			s.name = name;
			s.parent = state_parent;
			s.enabled = enabled;
			s.active = attrs.boolean_byte("active", 1);
			s.interruptibility = interruptibility;
			result.states.push_back(std::move(s));
			if (state_parent) result.states[*state_parent].children.push_back(id);
			node = Node{Node::State, id};
		} else if (tag == TRANSITION) {
			if (!parent || parent->kind != Node::State) return graph_error(source, i, "transition requires a state parent");
			size_t owner = parent->index;
			size_t id = result.transitions.size();
			const std::string *p = attrs.text("priority");
			uint32_t priority = !p ? 0 : *p == "med" ? 1 : *p == "high" ? 2 : *p == "urgent" ? 3 : 0;
			Transition t;
			t.element = i;
			t.owner = owner;
			t.enabled = enabled;
			t.priority = priority;
			result.transitions.push_back(std::move(t));
			result.states[owner].transitions.push_back(id);
			node = Node{Node::Transition, id};
		} else if (tag == EXPRESSION) {
			size_t id = result.expressions.size();
			const std::string *o = attrs.text("op");
			uint32_t op = !o ? 0 : *o == "and" ? 1 : *o == "or" ? 2 : *o == "not" ? 3 : 0;
			Expression e;
			e.element = i;
			e.enabled = enabled;
			e.op = op;
			result.expressions.push_back(std::move(e));
			if (parent && parent->kind == Node::State) result.states[parent->index].expression = id;
			else if (parent && parent->kind == Node::Transition) result.transitions[parent->index].expression = id;
			else if (parent && parent->kind == Node::Expression) result.expressions[parent->index].children.push_back(Node{Node::Expression, id});
			else return graph_error(source, i, "expression requires state, transition or expression parent");
			node = Node{Node::Expression, id};
		} else if (tag == PARAM) {
			if (!parent || parent->kind != Node::Operation) return graph_error(source, i, "unbound parameter handler");
			result.operations[parent->index].parameters.push_back(i);
			if (!element.children.empty()) return graph_error(source, i, "parameter children have no native object parent");
			continue;
		} else {
			OperationKind kind;
			if (tag == BEHAVIOUR) kind = OperationKind::Behavior;
			else if (tag == CONDITION) kind = OperationKind::Condition;
			else if (tag == HOOK) kind = OperationKind::Hook;
			else return graph_error(source, i, "unsupported graph element");
			if (!parent) return graph_error(source, i, "operation requires a parent");
			size_t id = result.operations.size();
			if (kind == OperationKind::Behavior && parent->kind == Node::State) result.states[parent->index].behaviors.push_back(id);
			else if (kind == OperationKind::Condition && parent->kind == Node::Expression)
				result.expressions[parent->index].children.push_back(Node{Node::Operation, id});
			else if (kind == OperationKind::Hook && parent->kind == Node::Transition) result.transitions[parent->index].hooks.push_back(id);
			else return graph_error(source, i, "unsupported operation parent");
			Operation op;
			op.element = i;
			op.parent = *parent;
			op.kind = kind;
			op.name = name;
			op.enabled = enabled;
			if (kind == OperationKind::Condition) op.condition_mask = parse_condition_mask(attrs.text("mask"));
			result.operations.push_back(std::move(op));
			node = Node{Node::Operation, id};
		}
		nodes[i] = node;
	}
	if (result.states.empty()) return Error{"Graph has no root state"};
	S3_CHECK(result.process_names(source));
	return result;
}

Status Binding::process_names(const StateGraph &source)
{
	for (size_t id = 0; id < states.size(); id++) {
		const State &state = states[id];
		if (state.interruptibility == 2) {
			Attributes attrs(source.elements[state.element].attributes);
			std::string name = *attrs.text("interruptable");
			std::optional<size_t> target = find_state(id, name, true);
			if (!target) return graph_error(source, state.element, "unresolved interrupt ancestor");
			std::optional<size_t> ancestor = id;
			while (ancestor && ancestor != target) ancestor = states[*ancestor].parent;
			if (!ancestor) return graph_error(source, state.element, "interrupt target is not an ancestor");
			states[id].interrupt_ancestor = target;
		}
	}
	for (size_t id = 0; id < transitions.size(); id++) {
		const Transition &transition = transitions[id];
		Attributes attrs(source.elements[transition.element].attributes);
		const std::string *t = attrs.text("target");
		std::optional<size_t> target = find_state(transition.owner, t ? *t : std::string(), true);
		transitions[id].target = target;
		// 82C16FF0 disables unresolved/disabled targets; no invented fallback.
		if (!target || states[*target].enabled == 0) transitions[id].enabled = 0;
	}
	return Unit{};
}

std::optional<size_t> Binding::find_state(size_t start, const std::string &name, bool ascend) const
{
	size_t dot = name.find('.');
	if (dot != std::string::npos) {
		std::optional<size_t> found = find_state(start, name.substr(0, dot), true);
		if (!found) return std::nullopt;
		return find_state(*found, name.substr(dot + 1), false);
	}
	uint32_t key = name_hash(name);
	size_t state = start;
	for (;;) {
		for (size_t child : states[state].children)
			if (name_hash(states[child].name) == key) return child;
		if (!ascend) return std::nullopt;
		if (name_hash(states[state].name) == key) return state;
		if (!states[state].parent) return std::nullopt;
		state = *states[state].parent;
	}
}

} // namespace state_graph_binding

} // namespace s3::data
