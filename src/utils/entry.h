#pragma once
#include <Arduino.h>
#include <StringUtils.h>

#include "entry_stack.h"
#include "types.h"

namespace gson {

class Entry : public Text {
   public:
    Entry(const gsutil::EntryStack* ens = nullptr, parent_t idx = 0) : ens(ens), idx(idx) {
        if (_valid()) *((Text*)this) = ens->valueText(idx);
    }

    size_t printTo(Print& p) const {
        if (isContainer()) return stringify(p), 1;
        else return Text::printTo(p);
    }

    // ===================== BY KEY =====================

    // получить элемент по ключу
    Entry get(const Text& key) const {
        if (_valid() && ens->_get(idx).isObject()) {
            for (uint16_t i = idx + 1; i < ens->length(); i++) {
                if (ens->_get(i).parent == idx &&
                    ens->_get(i).key_offs &&
                    key.compare(ens->keyText(i))) return Entry(ens, i);
            }
        }
        return Entry();
    }

    // содержит элемент с указанным ключом
    bool has(const Text& key) const {
        return get(key).valid();
    }

    // доступ по ключу (контейнер - Object)
    Entry operator[](const char* key) const {
        return get(key);
    }
    Entry operator[](const __FlashStringHelper* key) const {
        return get(key);
    }
    Entry operator[](const String& key) const {
        return get(key);
    }
    Entry operator[](String& key) const {
        return get(key);
    }

    // ===================== BY HASH =====================

    // получить элемент по хэшу ключа
    Entry get(size_t hash) const {
        if (_valid() && ens->hashed() && ens->_get(idx).isObject()) {
            for (uint16_t i = idx + 1; i < ens->length(); i++) {
                if (ens->_get(i).parent == idx && ens->hash[i] == hash) return Entry(ens, i);
            }
        }
        return Entry();
    }

    // содержит элемент с указанным хэшем ключа
    bool has(size_t hash) const {
        return get(hash).valid();
    }

    // доступ по хэшу ключа (контейнер - Object)
    Entry operator[](size_t hash) const {
        return get(hash);
    }

    // ===================== BY INDEX =====================

    // получить элемент по индексу
    Entry get(int index) const {
        if (_valid() && (size_t)index < ens->length() && ens->_get(idx).isContainer()) {
            for (uint16_t i = idx + 1; i < ens->length(); i++) {
                if (ens->_get(i).parent == idx) {
                    if (!index) return Entry(ens, i);
                    --index;
                }
            }
        }
        return Entry();
    }

    // итерация по вложенным
    void loop(void (*cb)(Entry e)) {
        if (_valid() && ens->_get(idx).isContainer()) {
            for (uint16_t i = idx + 1; i < ens->length(); i++) {
                if (ens->_get(i).parent == idx) cb(Entry(ens, i));
            }
        }
    }

    // доступ по индексу (контейнер - Array или Object)
    Entry operator[](int index) const {
        return get(index);
    }

    // ===================== MISC =====================

    // декодировать UCN (unicode) в записи
    void decodeUCN() {
        gsutil::Entry_t& e = ens->_get(idx);
        e.val_len = su::unicode::decodeSelf((char*)e.value(ens->str), e.val_len);
    }

    // получить ключ
    Text key() const {
        return _valid() ? ens->keyText(idx) : Text();
    }

    // получить хэш ключа
    size_t keyHash() const {
        return _valid() ? (ens->hashed() ? ens->hash[idx] : ens->keyText(idx).hash()) : 0;
    }
    // без проверок
    size_t _keyHash() const {
        return ens->hash[idx];
    }

    // получить значение
    Text value() const {
        return *this;
    }

    // получить размер для объектов и массивов
    size_t length() const {
        if (!_valid() || !ens->_get(idx).isContainer()) return 0;
        size_t len = 0;
        for (size_t i = 0; i < ens->length(); i++) {
            if (ens->_get(i).parent == idx) len++;
        }
        return len;
    }

    // получить тип элемента
    gson::Type type() const {
        return _valid() ? ens->_get(idx).type : gson::Type::None;
    }

    // сравнить тип элемента
    bool is(gson::Type type) const {
        return _valid() ? ens->_get(idx).type == type : false;
    }

    // элемент Array или Object
    bool isContainer() const {
        return _valid() ? ens->_get(idx).isContainer() : false;
    }

    // элемент Object
    bool isObject() const {
        return _valid() ? ens->_get(idx).isObject() : false;
    }

    // элемент Array
    bool isArray() const {
        return _valid() ? ens->_get(idx).isArray() : false;
    }

    // вывести в Print с форматированием
    void stringify(Print& pr) const {
        if (!_valid()) return;
        if (ens->_get(idx).isContainer()) {
            uint8_t depth = 1;
            parent_t index = idx + 1;
            pr.println(ens->_get(idx).isObject() ? '{' : '[');
            _stringify(pr, index, ens->_get(index).parent, depth);
            pr.println();
            pr.print(ens->_get(idx).isObject() ? '}' : ']');
        } else {
            _print(pr, idx);
        }
        pr.println();
    }

    // парсить в массив длины length()
    template <typename T>
    bool parseTo(T& arr) const {
        if (!isArray()) return false;
        for (size_t i = 0; i < length(); i++) {
            arr[i] = ens->valueText(idx + 1 + i);
        }
        return true;
    }

    // индекс элемента в общем массиве парсера
    parent_t index() {
        return idx;
    }

    // проверить коллизии хэшей в объектe
    bool checkCollisions(bool recursive = true) const {
        return (isObject() && ens->hashed()) ? _checkCollisions(*this, recursive) : 0;
    }

    void reset() {
        ens = nullptr;
    }

    // ===========================
    bool includes(size_t hash) const __attribute__((deprecated)) {
        return has(hash);
    }
    bool includes(const Text& key) const __attribute__((deprecated)) {
        return has(key);
    }

   private:
    const gsutil::EntryStack* ens = nullptr;
    parent_t idx = 0;

    // массив и строка существуют
    bool _valid() const {
        return ens && ens->valid();
    }

    void _printTab(Print& p, uint8_t amount) const {
        while (amount--) {
            p.print(' ');
            p.print(' ');
        }
    }

    void _print(Print& p, parent_t idx) const {
        gsutil::Entry_t& ent = ens->_get(idx);
        if (ent.key_offs) {
            p.print('\"');
            p.print(ens->keyText(idx));
            p.print("\":");
        }
        if (ent.is(gson::Type::String)) p.print('\"');
        switch ((gson::Type)ent.type) {
            case gson::Type::String:
            case gson::Type::Int:
            case gson::Type::Float:
                p.print(ens->valueText(idx));
                break;
            case gson::Type::Bool:
                p.print((ens->valueText(idx)[0] == 't') ? F("true") : F("false"));
                break;
            case gson::Type::Null:
                p.print(F("null"));
                break;
            default:
                break;
        }
        if (ent.is(gson::Type::String)) p.print('\"');
    }

    void _stringify(Print& p, parent_t& idx, parent_t parent, uint8_t& depth) const {
        bool first = true;
        while (idx < ens->length()) {
            gsutil::Entry_t& ent = ens->_get(idx);
            if (ent.parent != parent) return;

            if (first) first = false;
            else p.print(",\n");

            if (ent.isContainer()) {
                _printTab(p, depth);
                if (ent.key_offs) {
                    p.print('\"');
                    p.print(ens->keyText(idx));
                    p.print("\": ");
                }
                p.print((ent.isArray()) ? '[' : '{');
                p.print('\n');
                parent_t prev = idx;
                ++idx;
                ++depth;
                _stringify(p, idx, prev, depth);
                --depth;
                p.print('\n');
                _printTab(p, depth);
                p.print((ent.isArray()) ? ']' : '}');
            } else {
                _printTab(p, depth);
                _print(p, idx);
                ++idx;
            }
        }
    }

    static bool _checkCollisions(const Entry& ent, bool recursive) {
        int16_t len = ent.length();
        for (int16_t i = 0; i < len; i++) {
            Entry e = ent.get(i);
            for (int16_t j = i + 1; j < len; j++) {
                if (e.keyHash() == ent.get(j).keyHash()) return 1;
            }
            if (recursive && e.isObject() && _checkCollisions(e, true)) return 1;
        }
        return 0;
    }
};

}  // namespace gson