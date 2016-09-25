#include <iostream>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <functional>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include "string.hpp"

long timeit(std::function<void()> fn) {
	using namespace std::chrono;
	auto start = system_clock::now();
	fn();
	return duration_cast<milliseconds>(system_clock::now() - start).count();
}

typedef int StackFrameId;
typedef std::vector<StackFrameId> StackTrace;

namespace std {

template <>
struct hash<StackTrace> {
	std::size_t operator()(const StackTrace &stack) const {
		return std::_Hash_bytes(&stack[0], sizeof(StackFrameId) * stack.size(), static_cast<size_t>(0xc70f6907UL));
	}
};

} /* namespace std */

class ScopeGuard {
private:
	std::function<void()> _fn;
public:
	ScopeGuard(std::function<void()> fn)
		: _fn(fn) {}
	~ScopeGuard() {
		_fn();
	}
};

template <typename HandlerType>
class PerfScriptParser {
private:
	HandlerType &_handler;

public:
	PerfScriptParser(HandlerType &handler)
		: _handler(handler) {}

public:
	void read_file(FILE *fp) {
		ssize_t n;
		size_t capacity = 0;

		char *line = NULL;
		ScopeGuard g([&]{
			if (line) free(line);
		});

		while ((n = getline(&line, &capacity, fp)) != -1) {
			if (n > 0 && line[n - 1] == '\n') {
				line[n - 1] = '\0';
				--n;
			}
			if (n > 0 && line[n - 1] == '\r') {
				line[n - 1] = '\0';
				--n;
			}
			process(line);
		}
	}

private:
	static const char *skip_ws(const char *s) {
		while (*++s == ' ');
		return s;
	}

private:
	void process(const char *line) {
		if (line[0] == '\t') {
			const char *p0, *p1, *p2;

			// skip instruction pointer
			p0 = skip_ws(line+1);
			p1 = std::strchr(p0, ' ');
			if (p1 == NULL) {
				std::cerr << "invalid: " << line << std::endl;
				return;
			}

			p0 = skip_ws(p1);
			p1 = p0;
			p2 = p1;
			while (true) {
				p2 = std::strchr(p2, ' ');
				if (p2 == NULL)
					break;
				p1 = p2;
				p2 = skip_ws(p2+1);
			}

			_handler.on_stack_frame(String::wrap(p0, p1 - p0));
			return;
		}
		if (line[0] == '\0') {
			_handler.on_stack_end();
			return;
		}
		if (line[0] != '\t') {
			const char *p0, *p1, *p2;

			p0 = line;
			p1 = p0;
			while (true) {
				p1 = std::strchr(p1, ' ');
				if (p1 == NULL) {
					std::cerr << "invalid (unexpected EOL): " << line << std::endl;
					return;
				}
				p2 = skip_ws(p1);
				if (*p2 == '\0') {
					std::cerr << "invalid (unexpected EOL): " << line << std::endl;
					return;
				}
				if ('0' <= *p2 && *p2 <= '9')
					break;

				p1 = p2;
			}
			const String &proc = String::wrap(p0, p1 - p0);

			p0 = p2;
			p1 = std::strchr(p0, ' ');
			if (p1 == NULL) {
				std::cerr << "invalid (unexpected EOL, expected PID): " << line << std::endl;
				return;
			}
			int pid, tid = -1;

			try {
				std::string ptid(p0, p1 - p0); // PID/TID or just PID
				std::string::size_type slash_idx = ptid.find('/');
				if (slash_idx == std::string::npos) {
					pid = std::stoi(ptid);
				} else {
					pid = std::stoi(ptid.substr(0, slash_idx));
					tid = std::stoi(ptid.substr(slash_idx+1));
				}
			} catch (std::invalid_argument &e) {
				std::cerr << "invalid (failed to parse PID or PID/TID: " << std::string(p0, p1 - p0) << "): " << line << std::endl;
				return;
			}

			_handler.on_stack_start(proc, pid, tid);
			return;
		}
		std::cerr << "unrecognized: " << line << std::endl;
	}
};

class StackCollapsingHandler {
public:
	void on_stack_start(const String &proc, int pid, int tid) {
		_current_process = proc;
	}

	void on_stack_frame(const String &sym) {
		_current_stack.emplace_back(get_or_create_symbol_id(sym));
	}

	void on_stack_end() {
		++_n_stacks;

		_current_stack.emplace_back(get_or_create_symbol_id(_current_process));

		auto const &iter = _stack_counts.find(_current_stack);
		if (iter == _stack_counts.end()) {
			_stack_counts.emplace_hint(iter, _current_stack, 1);
		} else {
			++iter->second;
		}

		_current_stack.clear();
	}

public:
	int n_unique_stacks() const {
		return _stack_counts.size();
	}
	int n_stacks() const {
		return _n_stacks;
	}

private:
	int _n_stacks = 0;
	String _current_process;
	StackTrace _current_stack;

	std::unordered_map<String, StackFrameId> _symbol_names_r;
	std::unordered_map<StackTrace, int> _stack_counts;

public:
	StackCollapsingHandler() {
		_symbol_names_r.reserve(8192);
		_stack_counts.reserve(8192);
	}

private:
	int get_or_create_symbol_id(const String &sym) {
		auto const &iter = _symbol_names_r.find(sym);
		if (iter == _symbol_names_r.end()) {
			auto id = _symbol_names_r.size();
			_symbol_names_r.emplace_hint(iter, sym, id);
			return id;
		}
		return iter->second;
	}

public:
	void dump(FILE *fp) {
		typedef int OrderedStackFrameId;

		std::vector<std::pair<std::string, StackFrameId>> sorted_names;
		sorted_names.reserve(_symbol_names_r.size());
		for (auto const &sym_and_id : _symbol_names_r) {
			std::string sym = static_cast<std::string>(sym_and_id.first);
			std::replace(sym.begin(), sym.end(), ';', ':');
			std::replace(sym.begin(), sym.end(), ' ', '_');
			sorted_names.emplace_back(sym, sym_and_id.second);
		}
		std::sort(sorted_names.begin(), sorted_names.end());

		OrderedStackFrameId seq = 0;
		std::vector<OrderedStackFrameId> new_ids(sorted_names.size());
		for (auto const &sym_and_id : sorted_names)
			new_ids[sym_and_id.second] = seq++;

		std::vector<std::pair<std::vector<OrderedStackFrameId>, int>> sorted_counts;
		sorted_counts.reserve(_stack_counts.size());
		for (auto const &stack_count : _stack_counts) {
			std::vector<OrderedStackFrameId> frame;
			frame.reserve(stack_count.first.size());
			for (auto iter = stack_count.first.rbegin(); iter != stack_count.first.rend(); ++iter)
				frame.push_back(new_ids[*iter]);
			sorted_counts.emplace_back(frame, stack_count.second);
		}
		std::sort(sorted_counts.begin(), sorted_counts.end());

		for (auto const &stack_count : sorted_counts) {
			const char *sep = "";
			for (OrderedStackFrameId id : stack_count.first) {
				const std::string &sym = sorted_names[id].first;
				fputs(sep, fp);
				fputs(sym.c_str(), fp);
				sep = ";";
			}
			fprintf(fp, " %d\n", stack_count.second);
		}
	}
};

int main(int argc, char const* argv[]) {
	auto handler = StackCollapsingHandler();

	auto read_time = timeit([&] {
		auto parser = PerfScriptParser<StackCollapsingHandler>(handler);
		parser.read_file(stdin);
	});
	auto write_time = timeit([&] {
		handler.dump(stdout);
	});

	std::cerr << "reading and processing time: " << read_time << "ms" << std::endl;
	std::cerr << "sorting and writing time: " << write_time << "ms" << std::endl;
	std::cerr << "stacks: " << handler.n_stacks() << std::endl;
	std::cerr << "unique stacks: " << handler.n_unique_stacks() << std::endl;

	return 0;
}
