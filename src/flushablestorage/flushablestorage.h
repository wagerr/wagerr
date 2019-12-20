// Copyright (c) 2019 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef FLUSHABLE_STORAGE_H
#define FLUSHABLE_STORAGE_H

#include <util.h>
#include <memory>
#include "leveldbwrapper.h"
#include <boost/optional.hpp>

using MapKV = std::map<std::vector<unsigned char>, boost::optional<std::vector<unsigned char>>>;

// Key-Value storage iterator interface
class CStorageKVIterator {
public:
    virtual ~CStorageKVIterator() {};
    virtual void Seek(const std::vector<unsigned char>& key) = 0;
    virtual void Next() = 0;
    virtual bool Valid() = 0;
    virtual std::vector<unsigned char> Key() = 0;
    virtual std::vector<unsigned char> Value() = 0;
};

// Key-Value storage interface
class CStorageKV {
public:
    virtual ~CStorageKV() {};
    virtual bool Exists(const std::vector<unsigned char>& key) = 0;
    virtual bool Write(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value) = 0;
    virtual bool Erase(const std::vector<unsigned char>& key) = 0;
    virtual bool Read(const std::vector<unsigned char>& key, std::vector<unsigned char>& value) = 0;
    virtual std::unique_ptr<CStorageKVIterator> NewIterator() = 0;
};

// LevelDB glue layer Iterator
class CStorageLevelDBIterator : public CStorageKVIterator {
public:
    explicit CStorageLevelDBIterator(std::unique_ptr<leveldb::Iterator>&& it) : it{std::move(it)} { }
    ~CStorageLevelDBIterator() override { }
    void Seek(const std::vector<unsigned char>& key) override { it->Seek(leveldb::Slice((char *)key.data(), key.size())); }
    void Next() override { it->Next(); }
    bool Valid() override { return it->Valid(); }
    std::vector<unsigned char> Key() override {
        return ExtractSlice(it->key());
    }
    std::vector<unsigned char> Value() override {
        return ExtractSlice(it->value());
    }
private:
    std::unique_ptr<leveldb::Iterator> it;
    // No copying allowed
    CStorageLevelDBIterator(const CStorageLevelDBIterator&);
    void operator=(const CStorageLevelDBIterator&);

    std::vector<unsigned char> ExtractSlice(const leveldb::Slice& s) {
        std::vector<unsigned char> v;
        CDataStream stream(s.data(), s.data() + s.size(), SER_DISK, CLIENT_VERSION);
        stream >> v;
        return v;
    }
};

// LevelDB glue layer storage
class CStorageLevelDB : public CStorageKV {
public:
    explicit CStorageLevelDB(std::string dbName, std::size_t cacheSize, bool fMemory = false, bool fWipe = false) : db{dbName, cacheSize, fMemory, fWipe} {}
    ~CStorageLevelDB() override { }
    bool Exists(const std::vector<unsigned char>& key) override { return db.Exists(key); }
    bool Write(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value) override { return db.Write(key, value, true); }
    bool Erase(const std::vector<unsigned char>& key) override { return db.Erase(key, true); }
    bool Read(const std::vector<unsigned char>& key, std::vector<unsigned char>& value) override { return db.Read(key, value); }
    std::unique_ptr<CStorageKVIterator> NewIterator() override {
        return MakeUnique<CStorageLevelDBIterator>(std::unique_ptr<leveldb::Iterator>(db.NewIterator()));
    }
private:
    CLevelDBWrapper db;
};

// Flashable storage

// Flushable Key-Value Storage Iterator
class CFlushableStorageKVIterator : public CStorageKVIterator {
public:
    explicit CFlushableStorageKVIterator(std::unique_ptr<CStorageKVIterator>&& pIt_, MapKV& map_) : pIt{std::move(pIt_)}, map(map_) {
        inited = parentOk = mapOk = false;
    }
         // No copying allowed
    CFlushableStorageKVIterator(const CFlushableStorageKVIterator&) = delete;
    void operator=(const CFlushableStorageKVIterator&) = delete;
    ~CFlushableStorageKVIterator() override { }
    void Seek(const std::vector<unsigned char>& key) override {
        prevKey.clear();
        pIt->Seek(key);
        parentOk = pIt->Valid();
        mIt = map.lower_bound(key);
        mapOk = mIt != map.end();
        inited = true;
        Next();
    }
    void Next() override {
        if (!inited) throw std::runtime_error("Iterator wasn't inited.");
        key.clear();
        value.clear();

        while (mapOk || parentOk) {
            if (mapOk) {
                while (mapOk && (!parentOk || mIt->first <= pIt->Key())) {
                    bool ok = false;

                    if (mIt->second) {
                        ok = prevKey.empty() || mIt->first > prevKey;
                    }
                    else {
                        prevKey = mIt->first;
                    }
                    if (ok) {
                        key = mIt->first, value = *mIt->second;
                        prevKey = mIt->first;
                    }
                    if (mapOk) {
                        mIt++;
                        mapOk = mIt != map.end();
                    }
                    if (ok) return;
                }
            }
            if (parentOk) {
                bool ok = prevKey.empty() || pIt->Key() > prevKey;
                if (ok) {
                    key = pIt->Key();
                    value = pIt->Value();
                    prevKey = key;
                }
                if (parentOk) {
                    pIt->Next();
                    parentOk = pIt->Valid();
                }
                if (ok) return;
            }
        }
    }
    bool Valid() override {
        return !key.empty();
    }
    std::vector<unsigned char> Key() override {
        return key;
    }
    std::vector<unsigned char> Value() override {
        return value;
    }
private:
    bool inited;
    std::unique_ptr<CStorageKVIterator> pIt;
    bool parentOk;
    MapKV& map;
    MapKV::iterator mIt;
    bool mapOk;
    std::vector<unsigned char> key;
    std::vector<unsigned char> value;
    std::vector<unsigned char> prevKey;
};

// Flushable Key-Value Storage
class CFlushableStorageKV : public CStorageKV {
public:
    explicit CFlushableStorageKV(CStorageKV& db_) : db(db_) { }
    CFlushableStorageKV(const CFlushableStorageKV& db) = delete;
    ~CFlushableStorageKV() override { }
    bool Exists(const std::vector<unsigned char>& key) override {
        auto it = changed.find(key);
        if (it != changed.end()) {
            return !!it->second;
        }
        return db.Exists(key);
    }
    bool Write(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value) override {
        changed[key] = boost::optional<std::vector<unsigned char>>{value};
        return true;
    }
    bool Erase(const std::vector<unsigned char>& key) override {
        changed[key] = boost::optional<std::vector<unsigned char>>{};
        return true;
    }
    bool Read(const std::vector<unsigned char>& key, std::vector<unsigned char>& value) override {
        auto it = changed.find(key);
        if (it == changed.end()) {
            return db.Read(key, value);
        }
        else {
            if (it->second) {
                value = it->second.get();
                return true;
            }
            else {
                return false;
            }
        }
    }
    bool Flush() {
        for (auto it = changed.begin(); it != changed.end(); it++) {
            if (!it->second) {
                if (!db.Erase(it->first))
                    return false;
            }
            else {
                if (!db.Write(it->first, it->second.get()))
                    return false;
            }
        }
        changed.clear();
        return true;
    }
    std::unique_ptr<CStorageKVIterator> NewIterator() override {
        return MakeUnique<CFlushableStorageKVIterator>(db.NewIterator(), changed);
    }

private:
    CStorageKV& db;
    MapKV changed;
};

#endif
