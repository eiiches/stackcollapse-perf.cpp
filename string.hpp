#ifndef _STRING_HPP_
#define _STRING_HPP_

class String {
private:
	char *_p;
	int _len;
	bool _own;

protected:
	String(char *p, size_t len, bool own) noexcept
		: _p(p), _len(len), _own(own) {
	}

public:
	String() noexcept
		: _p(nullptr), _len(0), _own(false) {
	}

public:
	static String wrap(const char *p, size_t len) {
		return String(const_cast<char *>(p), len, false);
	}

	static String wrap(const std::string &s) {
		return String(const_cast<char *>(s.c_str()), s.size(), false);
	}

public:
	String(const String &s) {
		_p = (char *) malloc(sizeof(char) * s._len);
		if (!_p)
			throw std::bad_alloc();
		memcpy(_p, s._p, s._len);
		_len = s._len;
		_own = true;
	}

	String(String &&s) noexcept {
		_p = s._p;
		_len = s._len;
		_own = s._own;
		s._p = nullptr;
		s._len = 0;
	}

	~String() {
		if (_own && _p)
			free(_p);
	}

public:
	bool operator==(const String &rhs) const {
		if (_len != rhs._len)
			return false;
		return memcmp(_p, rhs._p, _len) == 0;
	}

	bool operator!=(const String &rhs) const {
		return !operator==(rhs);
	}

	String &operator=(const String &rhs) {
		if (_own && _p)
			free(_p);
		_p = (char *) malloc(sizeof(char) * rhs._len);
		if (!_p)
			throw std::bad_alloc();
		memcpy(_p, rhs._p, rhs._len);
		_len = rhs._len;
		_own = true;
		return *this;
	}

	String &operator=(const char *p) {
		if (_own && _p)
			free(_p);
		_p = strdup(_p);
		_len = strlen(_p);
		_own = true;
		return *this;
	}

	String &operator=(const std::string &s) {
		return operator=(s.c_str());
	}

	explicit operator std::string() const {
		return std::string(_p, _len);
	}

	char operator[](int i) const {
		return _p[i];
	}

public:
	int size() const {
		return _len;
	}

	size_t hash() const {
		return std::_Hash_bytes(_p, _len, static_cast<size_t>(0xc70f6907UL));
	}
};

namespace std {
template <>
struct hash<String> {
	std::size_t operator()(const String &s) const {
		return s.hash();
	}
};
}

#endif /* _STRING_HPP_ */
