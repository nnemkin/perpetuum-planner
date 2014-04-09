/*
    Copyright (C) 2014 Nikita Nemkin <nikita@nemkin.ru>

    This file is part of PerpetuumPlanner.

    PerpetuumPlanner is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PerpetuumPlanner is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PerpetuumPlanner. If not, see <http://www.gnu.org/licenses/>.
*/
// Qt wrapper for rapidjson.

#include <QVariant>
#include <QIODevice>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>


class JsonValue {
public:
    class const_iterator {
    public:
        JsonValue operator*() const { return JsonValue(m_iter->value); }
        const_iterator operator++() { ++m_iter; return *this; }
        bool operator!=(const const_iterator& other) const { return m_iter != other.m_iter; }

        QString key() const { return QString::fromUtf8(m_iter->name.GetString(), m_iter->name.GetStringLength()); }
        JsonValue value() const { return JsonValue(m_iter->value); }

    private:
        const_iterator(rapidjson::Value::ConstMemberIterator iter = 0) : m_iter(iter) {}
        rapidjson::Value::ConstMemberIterator m_iter;
        friend class JsonValue;
    };

    class ArrayProxy {
    public:
        ArrayProxy(const ArrayProxy& other) : m_value(other.m_value) {}

        class const_iterator {
        public:
            JsonValue operator*() const { return JsonValue(*m_iter); }
            const_iterator operator++() { ++m_iter; return *this; }
            bool operator!=(const const_iterator& other) const { return m_iter != other.m_iter; }
        private:
            const_iterator(rapidjson::Value::ConstValueIterator iter = 0) : m_iter(iter) {}
            rapidjson::Value::ConstValueIterator m_iter;
            friend class ArrayProxy;
        };

        const_iterator begin() const { return m_value.IsObject() ? const_iterator(m_value.Begin()) : 0; }
        const_iterator end() const { return m_value.IsObject() ? const_iterator(m_value.End()) : 0; }

    private:
        ArrayProxy(const rapidjson::Value &value) : m_value(value) {}
        const rapidjson::Value &m_value;
        friend class JsonValue;
    };

    JsonValue(const JsonValue &other) : m_value(other.m_value) {}

    const_iterator begin() const { return m_value.IsObject() ? const_iterator(m_value.MemberBegin()) : 0; }
    const_iterator end() const { return m_value.IsObject() ? const_iterator(m_value.MemberEnd()) : 0; }

    const ArrayProxy asArray() const { return ArrayProxy(m_value); }

    int toInt(int defval = 0) const { return m_value.IsInt() ? m_value.GetInt() : defval; }
    uint toUInt(uint defval = 0) const { return m_value.IsUint() ? m_value.GetUint() : defval; }
    quint64 toUInt64(quint64 defval = 0) const { return m_value.IsUint64() ? m_value.GetUint64() : defval; }
    float toFloat(float defval = 0.f) const { return m_value.IsDouble() ? float(m_value.GetDouble()) : defval; }
    double toDouble(double defval = 0.) const { return m_value.IsDouble() ? m_value.GetDouble() : defval; }

    QByteArray toByteArray() const
    {
        if (m_value.IsString())
            return QByteArray(m_value.GetString(), m_value.GetStringLength());
        return QByteArray();
    }

    QString toString() const
    {
        if (m_value.IsString())
            return QString::fromUtf8(m_value.GetString(), m_value.GetStringLength());
        return QString();
    }

    QVariant toVariant() const
    {
        if (m_value.IsBool()) return m_value.GetBool();
        if (m_value.IsInt()) return m_value.GetInt();
        if (m_value.IsUint()) return m_value.GetUint();
        if (m_value.IsUint64()) return m_value.GetUint64();
        if (m_value.IsDouble()) return m_value.GetDouble();
        if (m_value.IsString()) return toString();
        if (m_value.IsObject()) {
            QVariantMap map;
            for (const_iterator i = begin(); i != end(); ++i)
                map.insert(i.key(), i.value().toVariant());
        }
        if (m_value.IsArray()) {
            QVariantList list;
            list.reserve(m_value.Size());
            foreach (JsonValue value, asArray())
                list.append(value.toVariant());
            return list;
        }
        return QVariant();
    }

    JsonValue operator[](const char *key) const
    {
        if (m_value.IsObject()) {
            const rapidjson::Value::Member *member = m_value.FindMember(key);
            if (member)
                return JsonValue(member->value);
        }
        return JsonValue(rapidjson::Value()); // XXX
    }

    int size() const
    {
        if (m_value.IsArray()) return m_value.Size();
        if (m_value.IsObject()) return m_value.MemberEnd() - m_value.MemberBegin();
        return 0;
    }

    bool contains(const char *key) const { return m_value.IsObject() ? m_value.HasMember(key) : false; }

    bool isObject() const { return m_value.IsObject(); }
    bool isArray() const { return m_value.IsArray(); }
    bool isEmpty() const
    {
        if (m_value.IsObject()) return m_value.MemberBegin() == m_value.MemberEnd();
        if (m_value.IsString()) return m_value.GetStringLength() == 0;
        if (m_value.IsArray()) return m_value.Empty();
        return false;
    }

    template<typename T> T value() const;
    template<> int value<int>() const { return toInt(); }
    template<> uint value<uint>() const { return toUInt(); }
    template<> quint64 value<quint64>() const { return toUInt64(); }
    template<> QByteArray value<QByteArray>() const { return toByteArray(); }
    template<> QString value<QString>() const { return toString(); }

protected:
    JsonValue(const rapidjson::Value &value) : m_value(value) {}

    const rapidjson::Value &m_value;
};

class JsonDocument : public JsonValue {
public:
    JsonDocument(QByteArray json) : JsonValue(m_doc), m_json(json)
    {
        m_doc.ParseInsitu<0>(m_json.data());
    }

    bool hasError() const { return m_doc.HasParseError(); }
    QString errorString() const
    {
        if (m_doc.HasParseError() && m_doc.GetParseError())
            return QString("JSON Error at Offset %1: %2").arg(m_doc.GetErrorOffset()).arg(m_doc.GetParseError());
        return QString();
    }

private:
    QByteArray m_json;  // NB: m_doc will reference strings inside this buffer
    rapidjson::Document m_doc;
};


class QIODeviceStream {
public:
    typedef char Ch;

    QIODeviceStream(QIODevice *device) : m_device(device) {}

    void Put(char c) { m_device->putChar(c); }
    void PutN(char c, size_t n)
    {
        static char buf[32];
        memset(buf, c, qMin(sizeof buf, n));
        for (; n > sizeof buf; n -= sizeof(buf))
            m_device->write(buf, sizeof buf);
        m_device->write(buf, n);
    }
    void Flush() {}

    // Not implemented
    char Peek() const { assert(false); return 0; }
    char Take() { assert(false); return 0; }
    size_t Tell() const { assert(false); return 0; }
    char* PutBegin() { assert(false); return 0; }
    size_t PutEnd(char*) { assert(false); return 0; }

private:
    QIODevice *m_device;
};

namespace rapidjson
{
    // accelerated whitespace writing
    template<>
    inline void PutN(QIODeviceStream& stream, char c, size_t n) { stream.PutN(c, n); }
}


class JsonWriter {
public:
    JsonWriter(QIODevice *device) : m_stream(device), m_writer(m_stream) {}

    void writeNull() { m_writer.Null(); }
    void writeBool(bool b) { m_writer.Bool(b); }
    void writeInt(int i) { m_writer.Int(i); }
    void writeUlonglong(quint64 i) { m_writer.Uint64(i); }
    void writeFloat(float f) { m_writer.Double(f); }
    void writeDouble(double d) { m_writer.Double(d); }

    // string writes are chainable, because they double as keys for maps.
    template<int n>
    JsonWriter &writeString(const char (&s)[n]) { m_writer.String(s, n - 1); return *this; }
    JsonWriter &writeString(const QByteArray &s) { m_writer.String(s.constData(), s.length()); return *this; }
    JsonWriter &writeString(const QString &s) { writeString(s.toUtf8()); return *this; }

    void startArray() { m_writer.StartArray(); }
    void endArray() { m_writer.EndArray(); }

    void startObject() { m_writer.StartObject(); }
    void endObject() { m_writer.EndObject(); }

private:
    QIODeviceStream m_stream;
    rapidjson::PrettyWriter<QIODeviceStream> m_writer;
};
