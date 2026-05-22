#include "switchbox/Json.hpp"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace switchbox {
namespace {

const std::string kEmptyString;
const std::vector<JsonValue> kEmptyArray;
const std::map<std::string, JsonValue> kEmptyObject;
const JsonValue kNullValue;

int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool parseHex4(const std::string& text, size_t pos, uint32_t& codepoint) {
    if (pos + 4 > text.size()) return false;
    codepoint = 0;
    for (int i = 0; i < 4; ++i) {
        const int value = hexValue(text[pos + i]);
        if (value < 0) return false;
        codepoint = (codepoint << 4) | static_cast<uint32_t>(value);
    }
    return true;
}

void appendUtf8(std::ostringstream& ss, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
        ss << static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        ss << static_cast<char>(0xC0 | (codepoint >> 6));
        ss << static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        ss << static_cast<char>(0xE0 | (codepoint >> 12));
        ss << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        ss << static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        ss << static_cast<char>(0xF0 | (codepoint >> 18));
        ss << static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        ss << static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        ss << static_cast<char>(0x80 | (codepoint & 0x3F));
    }
}

class ParserImpl {
public:
    explicit ParserImpl(const std::string& text) : text_(text) {}

    bool parse(JsonValue& out, std::string& error) {
        skipWhitespace();
        if (!parseValue(out, error)) return false;
        skipWhitespace();
        if (pos_ != text_.size()) {
            error = "unexpected trailing characters at offset " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    void skipWhitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) pos_++;
    }

    bool consume(char ch) {
        skipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == ch) {
            pos_++;
            return true;
        }
        return false;
    }

    bool startsWith(const char* literal) const {
        size_t i = 0;
        while (literal[i]) {
            if (pos_ + i >= text_.size() || text_[pos_ + i] != literal[i]) return false;
            i++;
        }
        return true;
    }

    bool parseValue(JsonValue& out, std::string& error) {
        skipWhitespace();
        if (pos_ >= text_.size()) {
            error = "unexpected end of JSON";
            return false;
        }

        const char ch = text_[pos_];
        if (ch == '"') return parseStringValue(out, error);
        if (ch == '{') return parseObject(out, error);
        if (ch == '[') return parseArray(out, error);
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) return parseNumber(out, error);

        if (startsWith("true")) {
            pos_ += 4;
            out = JsonValue::makeBool(true);
            return true;
        }
        if (startsWith("false")) {
            pos_ += 5;
            out = JsonValue::makeBool(false);
            return true;
        }
        if (startsWith("null")) {
            pos_ += 4;
            out = JsonValue::makeNull();
            return true;
        }

        error = "unexpected token at offset " + std::to_string(pos_);
        return false;
    }

    bool parseString(std::string& out, std::string& error) {
        if (pos_ >= text_.size() || text_[pos_] != '"') {
            error = "expected string at offset " + std::to_string(pos_);
            return false;
        }
        pos_++;
        std::ostringstream ss;
        while (pos_ < text_.size()) {
            char ch = text_[pos_++];
            if (ch == '"') {
                out = ss.str();
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    error = "unterminated escape sequence";
                    return false;
                }
                char esc = text_[pos_++];
                switch (esc) {
                    case '"': ss << '"'; break;
                    case '\\': ss << '\\'; break;
                    case '/': ss << '/'; break;
                    case 'b': ss << '\b'; break;
                    case 'f': ss << '\f'; break;
                    case 'n': ss << '\n'; break;
                    case 'r': ss << '\r'; break;
                    case 't': ss << '\t'; break;
                    case 'u': {
                        uint32_t codepoint;
                        if (!parseHex4(text_, pos_, codepoint)) {
                            error = "invalid unicode escape";
                            return false;
                        }
                        pos_ += 4;
                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                            if (pos_ + 6 <= text_.size() && text_[pos_] == '\\' && text_[pos_ + 1] == 'u') {
                                uint32_t low;
                                if (parseHex4(text_, pos_ + 2, low) && low >= 0xDC00 && low <= 0xDFFF) {
                                    codepoint = 0x10000 + (((codepoint - 0xD800) << 10) | (low - 0xDC00));
                                    pos_ += 6;
                                }
                            }
                        }
                        appendUtf8(ss, codepoint);
                        break;
                    }
                    default:
                        error = "invalid escape sequence";
                        return false;
                }
            } else {
                ss << ch;
            }
        }
        error = "unterminated string";
        return false;
    }

    bool parseStringValue(JsonValue& out, std::string& error) {
        std::string value;
        if (!parseString(value, error)) return false;
        out = JsonValue::makeString(value);
        return true;
    }

    bool parseNumber(JsonValue& out, std::string& error) {
        const size_t start = pos_;
        if (text_[pos_] == '-') pos_++;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            pos_++;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            pos_++;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) pos_++;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        char* end = nullptr;
        const std::string token = text_.substr(start, pos_ - start);
        const double value = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0') {
            error = "invalid number at offset " + std::to_string(start);
            return false;
        }
        out = JsonValue::makeNumber(value);
        return true;
    }

    bool parseArray(JsonValue& out, std::string& error) {
        if (!consume('[')) {
            error = "expected array";
            return false;
        }
        std::vector<JsonValue> values;
        skipWhitespace();
        if (consume(']')) {
            out = JsonValue::makeArray(values);
            return true;
        }
        while (true) {
            JsonValue value;
            if (!parseValue(value, error)) return false;
            values.push_back(value);
            if (consume(']')) {
                out = JsonValue::makeArray(values);
                return true;
            }
            if (!consume(',')) {
                error = "expected ',' or ']' at offset " + std::to_string(pos_);
                return false;
            }
        }
    }

    bool parseObject(JsonValue& out, std::string& error) {
        if (!consume('{')) {
            error = "expected object";
            return false;
        }
        std::map<std::string, JsonValue> values;
        skipWhitespace();
        if (consume('}')) {
            out = JsonValue::makeObject(values);
            return true;
        }
        while (true) {
            skipWhitespace();
            std::string key;
            if (!parseString(key, error)) return false;
            if (!consume(':')) {
                error = "expected ':' after object key at offset " + std::to_string(pos_);
                return false;
            }
            JsonValue value;
            if (!parseValue(value, error)) return false;
            values[key] = value;
            if (consume('}')) {
                out = JsonValue::makeObject(values);
                return true;
            }
            if (!consume(',')) {
                error = "expected ',' or '}' at offset " + std::to_string(pos_);
                return false;
            }
        }
    }

    const std::string& text_;
    size_t pos_ = 0;
};

std::string escapeJson(const std::string& input) {
    std::ostringstream ss;
    for (char ch : input) {
        switch (ch) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default: ss << ch; break;
        }
    }
    return ss.str();
}

void writeIndent(std::ostringstream& ss, int count) {
    for (int i = 0; i < count; ++i) ss << ' ';
}

void stringifyImpl(const JsonValue& value, std::ostringstream& ss, bool pretty, int indent) {
    switch (value.type()) {
        case JsonType::Null:
            ss << "null";
            break;
        case JsonType::Bool:
            ss << (value.asBool() ? "true" : "false");
            break;
        case JsonType::Number:
            ss << value.asNumber();
            break;
        case JsonType::String:
            ss << '"' << escapeJson(value.asString()) << '"';
            break;
        case JsonType::Array: {
            ss << '[';
            const auto& array = value.asArray();
            for (size_t i = 0; i < array.size(); ++i) {
                if (i > 0) ss << ',';
                if (pretty) { ss << '\n'; writeIndent(ss, indent + 2); }
                stringifyImpl(array[i], ss, pretty, indent + 2);
            }
            if (pretty && !array.empty()) { ss << '\n'; writeIndent(ss, indent); }
            ss << ']';
            break;
        }
        case JsonType::Object: {
            ss << '{';
            const auto& object = value.asObject();
            size_t i = 0;
            for (const auto& pair : object) {
                if (i++ > 0) ss << ',';
                if (pretty) { ss << '\n'; writeIndent(ss, indent + 2); }
                ss << '"' << escapeJson(pair.first) << '"' << ':';
                if (pretty) ss << ' ';
                stringifyImpl(pair.second, ss, pretty, indent + 2);
            }
            if (pretty && !object.empty()) { ss << '\n'; writeIndent(ss, indent); }
            ss << '}';
            break;
        }
    }
}

} // namespace

JsonValue::JsonValue() : type_(JsonType::Null), bool_(false), number_(0.0) {}
JsonValue JsonValue::makeNull() { return JsonValue(); }
JsonValue JsonValue::makeBool(bool value) { JsonValue v; v.type_ = JsonType::Bool; v.bool_ = value; return v; }
JsonValue JsonValue::makeNumber(double value) { JsonValue v; v.type_ = JsonType::Number; v.number_ = value; return v; }
JsonValue JsonValue::makeString(std::string value) { JsonValue v; v.type_ = JsonType::String; v.string_ = std::move(value); return v; }
JsonValue JsonValue::makeArray(std::vector<JsonValue> value) { JsonValue v; v.type_ = JsonType::Array; v.array_ = std::move(value); return v; }
JsonValue JsonValue::makeObject(std::map<std::string, JsonValue> value) { JsonValue v; v.type_ = JsonType::Object; v.object_ = std::move(value); return v; }
JsonType JsonValue::type() const { return type_; }
bool JsonValue::isNull() const { return type_ == JsonType::Null; }
bool JsonValue::isBool() const { return type_ == JsonType::Bool; }
bool JsonValue::isNumber() const { return type_ == JsonType::Number; }
bool JsonValue::isString() const { return type_ == JsonType::String; }
bool JsonValue::isArray() const { return type_ == JsonType::Array; }
bool JsonValue::isObject() const { return type_ == JsonType::Object; }
bool JsonValue::asBool(bool fallback) const { return isBool() ? bool_ : fallback; }
int JsonValue::asInt(int fallback) const { return isNumber() ? static_cast<int>(number_) : fallback; }
double JsonValue::asNumber(double fallback) const { return isNumber() ? number_ : fallback; }
const std::string& JsonValue::asString() const { return isString() ? string_ : kEmptyString; }
std::string JsonValue::asStringOr(const std::string& fallback) const { return isString() ? string_ : fallback; }
const std::vector<JsonValue>& JsonValue::asArray() const { return isArray() ? array_ : kEmptyArray; }
const std::map<std::string, JsonValue>& JsonValue::asObject() const { return isObject() ? object_ : kEmptyObject; }
const JsonValue& JsonValue::get(const std::string& key) const {
    if (!isObject()) return kNullValue;
    auto it = object_.find(key);
    return it == object_.end() ? kNullValue : it->second;
}

bool JsonParser::parse(const std::string& input, JsonValue& output, std::string& error) {
    ParserImpl parser(input);
    return parser.parse(output, error);
}

std::string JsonWriter::stringify(const JsonValue& value, bool pretty) {
    std::ostringstream ss;
    stringifyImpl(value, ss, pretty, 0);
    return ss.str();
}

} // namespace switchbox
