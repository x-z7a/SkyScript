// picojson.h - a header-file-only, JSON parser / serializer
// https://github.com/kazuho/picojson
//
// Copyright 2009-2010 Cybozu Labs, Inc.
// Copyright 2011-2021 Kazuho Oku
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef PICOJSON_H
#define PICOJSON_H

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace picojson {

enum {
  null_type,
  boolean_type,
  number_type,
  string_type,
  array_type,
  object_type
};

class value {
 public:
  typedef std::vector<value> array;
  typedef std::map<std::string, value> object;

  value() : type_(null_type), u_() {}
  value(int type) : type_(type), u_() {
    switch (type_) {
    case boolean_type:
      u_.boolean_ = false;
      break;
    case number_type:
      u_.number_ = 0;
      break;
    case string_type:
      u_.string_ = new std::string();
      break;
    case array_type:
      u_.array_ = new array();
      break;
    case object_type:
      u_.object_ = new object();
      break;
    default:
      break;
    }
  }
  value(double n) : type_(number_type), u_() { u_.number_ = n; }
  value(bool b) : type_(boolean_type), u_() { u_.boolean_ = b; }
  value(const std::string &s) : type_(string_type), u_() { u_.string_ = new std::string(s); }
  value(const char *s) : type_(string_type), u_() { u_.string_ = new std::string(s); }
  value(const array &a) : type_(array_type), u_() { u_.array_ = new array(a); }
  value(const object &o) : type_(object_type), u_() { u_.object_ = new object(o); }
  value(const value &x) : type_(x.type_), u_() {
    switch (type_) {
    case boolean_type:
      u_.boolean_ = x.u_.boolean_;
      break;
    case number_type:
      u_.number_ = x.u_.number_;
      break;
    case string_type:
      u_.string_ = new std::string(*x.u_.string_);
      break;
    case array_type:
      u_.array_ = new array(*x.u_.array_);
      break;
    case object_type:
      u_.object_ = new object(*x.u_.object_);
      break;
    default:
      break;
    }
  }
  ~value() { clear(); }

  value &operator=(const value &x) {
    if (this != &x) {
      value t(x);
      swap(t);
    }
    return *this;
  }

  void swap(value &x) {
    std::swap(type_, x.type_);
    std::swap(u_, x.u_);
  }

  int get_type() const { return type_; }

  bool is(int type) const { return type_ == type; }
  bool is_null() const { return type_ == null_type; }
  bool is_bool() const { return type_ == boolean_type; }
  bool is_number() const { return type_ == number_type; }
  bool is_string() const { return type_ == string_type; }
  bool is_array() const { return type_ == array_type; }
  bool is_object() const { return type_ == object_type; }

  bool get_bool() const { return u_.boolean_; }
  double get_number() const { return u_.number_; }
  const std::string &get_string() const { return *u_.string_; }
  const array &get_array() const { return *u_.array_; }
  const object &get_object() const { return *u_.object_; }

  std::string &get_string() { return *u_.string_; }
  array &get_array() { return *u_.array_; }
  object &get_object() { return *u_.object_; }

  void clear() {
    switch (type_) {
    case string_type:
      delete u_.string_;
      break;
    case array_type:
      delete u_.array_;
      break;
    case object_type:
      delete u_.object_;
      break;
    default:
      break;
    }
    type_ = null_type;
  }

 private:
  union {
    bool boolean_;
    double number_;
    std::string *string_;
    array *array_;
    object *object_;
  } u_;
  int type_;
};

namespace detail {

inline bool isws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

inline void skipws(const char *&p, const char *end) {
  while (p != end && isws(*p))
    ++p;
}

inline bool match(const char *&p, const char *end, const char *pat) {
  size_t len = std::strlen(pat);
  if (end - p < static_cast<ptrdiff_t>(len))
    return false;
  if (std::strncmp(p, pat, len) != 0)
    return false;
  p += len;
  return true;
}

inline bool parse_string(const char *&p, const char *end, std::string &out) {
  if (p == end || *p != '"')
    return false;
  ++p;
  std::string s;
  while (p != end) {
    char c = *p++;
    if (c == '"') {
      out.swap(s);
      return true;
    }
    if (c == '\\') {
      if (p == end)
        return false;
      char e = *p++;
      switch (e) {
      case '"': s.push_back('"'); break;
      case '\\': s.push_back('\\'); break;
      case '/': s.push_back('/'); break;
      case 'b': s.push_back('\b'); break;
      case 'f': s.push_back('\f'); break;
      case 'n': s.push_back('\n'); break;
      case 'r': s.push_back('\r'); break;
      case 't': s.push_back('\t'); break;
      case 'u':
        // skip basic \uXXXX; not fully decoding to UTF-8 here
        if (end - p < 4) return false;
        s.append("\\u");
        s.append(p, p + 4);
        p += 4;
        break;
      default:
        return false;
      }
    } else {
      s.push_back(c);
    }
  }
  return false;
}

inline bool parse_number(const char *&p, const char *end, double &out) {
  const char *start = p;
  if (p != end && (*p == '-' || *p == '+'))
    ++p;
  if (p == end)
    return false;
  if (*p == '0') {
    ++p;
  } else {
    if (!std::isdigit(static_cast<unsigned char>(*p))) return false;
    while (p != end && std::isdigit(static_cast<unsigned char>(*p)))
      ++p;
  }
  if (p != end && *p == '.') {
    ++p;
    if (p == end || !std::isdigit(static_cast<unsigned char>(*p))) return false;
    while (p != end && std::isdigit(static_cast<unsigned char>(*p)))
      ++p;
  }
  if (p != end && (*p == 'e' || *p == 'E')) {
    ++p;
    if (p != end && (*p == '-' || *p == '+')) ++p;
    if (p == end || !std::isdigit(static_cast<unsigned char>(*p))) return false;
    while (p != end && std::isdigit(static_cast<unsigned char>(*p)))
      ++p;
  }
  char *endptr = nullptr;
  out = std::strtod(start, &endptr);
  return endptr == p;
}

inline bool parse_value(const char *&p, const char *end, value &out);

inline bool parse_array(const char *&p, const char *end, value::array &out) {
  if (p == end || *p != '[') return false;
  ++p;
  skipws(p, end);
  if (p != end && *p == ']') { ++p; return true; }
  while (p != end) {
    value v;
    if (!parse_value(p, end, v)) return false;
    out.push_back(v);
    skipws(p, end);
    if (p != end && *p == ',') { ++p; skipws(p, end); continue; }
    if (p != end && *p == ']') { ++p; return true; }
    return false;
  }
  return false;
}

inline bool parse_object(const char *&p, const char *end, value::object &out) {
  if (p == end || *p != '{') return false;
  ++p;
  skipws(p, end);
  if (p != end && *p == '}') { ++p; return true; }
  while (p != end) {
    std::string key;
    if (!parse_string(p, end, key)) return false;
    skipws(p, end);
    if (p == end || *p != ':') return false;
    ++p;
    skipws(p, end);
    value v;
    if (!parse_value(p, end, v)) return false;
    out[key] = v;
    skipws(p, end);
    if (p != end && *p == ',') { ++p; skipws(p, end); continue; }
    if (p != end && *p == '}') { ++p; return true; }
    return false;
  }
  return false;
}

inline bool parse_value(const char *&p, const char *end, value &out) {
  skipws(p, end);
  if (p == end) return false;
  if (*p == 'n') { if (!match(p, end, "null")) return false; out = value(); return true; }
  if (*p == 't') { if (!match(p, end, "true")) return false; out = value(true); return true; }
  if (*p == 'f') { if (!match(p, end, "false")) return false; out = value(false); return true; }
  if (*p == '"') { std::string s; if (!parse_string(p, end, s)) return false; out = value(s); return true; }
  if (*p == '[') { value::array a; if (!parse_array(p, end, a)) return false; out = value(a); return true; }
  if (*p == '{') { value::object o; if (!parse_object(p, end, o)) return false; out = value(o); return true; }
  double n;
  if (!parse_number(p, end, n)) return false;
  out = value(n);
  return true;
}

} // namespace detail

inline std::string parse(value &out, const std::string &s) {
  const char *p = s.c_str();
  const char *end = p + s.size();
  if (!detail::parse_value(p, end, out))
    return "parse error";
  detail::skipws(p, end);
  if (p != end)
    return "trailing garbage";
  return "";
}

} // namespace picojson

#endif // PICOJSON_H
