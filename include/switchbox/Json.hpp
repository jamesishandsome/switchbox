#pragma once

#include <map>
#include <string>
#include <vector>

namespace switchbox {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

class JsonValue {
public:
    JsonValue();

    static JsonValue makeNull();
    static JsonValue makeBool(bool value);
    static JsonValue makeNumber(double value);
    static JsonValue makeString(std::string value);
    static JsonValue makeArray(std::vector<JsonValue> value);
    static JsonValue makeObject(std::map<std::string, JsonValue> value);

    JsonType type() const;
    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool asBool(bool fallback = false) const;
    int asInt(int fallback = 0) const;
    double asNumber(double fallback = 0.0) const;
    const std::string& asString() const;
    std::string asStringOr(const std::string& fallback) const;
    const std::vector<JsonValue>& asArray() const;
    const std::map<std::string, JsonValue>& asObject() const;

    const JsonValue& get(const std::string& key) const;

private:
    JsonType type_;
    bool bool_;
    double number_;
    std::string string_;
    std::vector<JsonValue> array_;
    std::map<std::string, JsonValue> object_;
};

class JsonParser {
public:
    static bool parse(const std::string& input, JsonValue& output, std::string& error);
};

class JsonWriter {
public:
    static std::string stringify(const JsonValue& value, bool pretty = true);
};

} // namespace switchbox
