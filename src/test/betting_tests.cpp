// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "betting/bet.h"
#include "random.h"
#include "uint256.h"
#include "test/test_wagerr.h"
#include <cstdlib>
#include <cstdio>

#include <vector>
#include <map>

#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>

static constexpr uint32_t nMassRecordCount{333}; // must be greater then MaxReorganizationDepth

BOOST_FIXTURE_TEST_SUITE(betting_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(betting_flushable_db_test)
{
    auto &db = *bettingsView->mappings.get();
    CBettingDB dbCache{db};
    // for writing
    CMapping mapping{};
    // for reading
    CMapping mapping_r{};
    MappingKey key{};

    mapping.nId = 1;
    mapping.nMType = teamMapping;
    mapping.sName = "Team1";
    key = {mapping.nMType, mapping.nId};

    BOOST_CHECK(dbCache.Write(key, mapping));
    BOOST_CHECK(dbCache.Read(key, mapping_r));

    BOOST_CHECK_EQUAL(mapping_r.sName, mapping.sName);

    mapping.nId = 2;
    mapping.nMType = sportMapping;
    mapping.sName = "Sport1";
    key = {mapping.nMType, mapping.nId};

    BOOST_CHECK(dbCache.Write(key, mapping));
    BOOST_CHECK(dbCache.Read(key, mapping_r));
    BOOST_CHECK_EQUAL(mapping_r.sName, mapping.sName);

    key = {teamMapping, 1};
    BOOST_CHECK(!db.Read(key, mapping_r));
    key = {sportMapping, 2};
    BOOST_CHECK(!db.Read(key, mapping_r));

    BOOST_CHECK(dbCache.Flush());

    key = {teamMapping, 1};
    BOOST_CHECK(db.Read(key, mapping_r));
    BOOST_CHECK_EQUAL(mapping_r.sName, "Team1");

    key = {sportMapping, 2};
    BOOST_CHECK(db.Read(key, mapping_r));
    BOOST_CHECK_EQUAL(mapping_r.sName, "Sport1");

    // check rewriting
    key = {teamMapping, 1};
    mapping.nId = 1;
    mapping.nMType = teamMapping;
    mapping.sName = "Team1-2";
    BOOST_CHECK(!dbCache.Write(key, mapping));

    // check update
    BOOST_CHECK(dbCache.Update(key, mapping));
    BOOST_CHECK(dbCache.Read(key, mapping_r));
    BOOST_CHECK_EQUAL(mapping_r.sName, mapping.sName);
    BOOST_CHECK(dbCache.Flush());
    BOOST_CHECK(db.Read(key, mapping_r));
    BOOST_CHECK_EQUAL(mapping_r.sName, mapping.sName);

    // check erasing
    dbCache.Erase(key);
    BOOST_CHECK(db.Read(key, mapping_r));
    BOOST_CHECK(!dbCache.Read(key, mapping_r));
    dbCache.Flush();
    BOOST_CHECK(!db.Read(key, mapping_r));

    std::cout << "Test of flushable DB passed" << std::endl;
}

BOOST_AUTO_TEST_CASE(betting_flushable_db_iterator_test)
{
    auto &db = *bettingsView->mappings.get();
    std::string nameBases[] = {"Sport", "Round", "Team", "Tournament"};
    // for writing
    MappingKey key{};
    CMapping mapping{};

    for (uint32_t mapType = tournamentMapping; mapType > 0; mapType--) {
        for (uint32_t mapId = 4097; mapId > 0; mapId--) {
            mapping.nMType = mapType;
            mapping.nId = mapId;
            mapping.sName = nameBases[mapType - 1] + std::to_string(mapId);
            key = {mapping.nMType, mapping.nId};
            db.Write(key, mapping);
        }
    }
    db.Flush();

    // check iterator walkthrough
    auto it = db.NewIterator();
    // seek to first record
    it->Seek(std::vector<unsigned char>{});
    for (uint32_t mapType = 1; mapType <= tournamentMapping; mapType++) {
        for (uint32_t mapId = 1; mapId <= 4097; mapId++, it->Next()) {
            BOOST_CHECK(it->Valid());
            CBettingDB::BytesToDbType(it->Value(), mapping);
            BOOST_CHECK_EQUAL(mapping.nMType, mapType);
            BOOST_CHECK_EQUAL(mapping.nId, mapId);
            BOOST_CHECK_EQUAL(mapping.sName, nameBases[mapType - 1] + std::to_string(mapId));
        }
    }

    CBettingDB dbCache{db};

    // erase
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        CBettingDB::BytesToDbType(it->Key(), key);
        dbCache.Erase(key);
    }

    // check cached DB is empty
    it = dbCache.NewIterator();
    // seek to first record
    it->Seek(std::vector<unsigned char>{});
    // check that iterator of empty table is invalid
    BOOST_CHECK(!it->Valid());

    // iterator from main db must be still valid
    it = db.NewIterator();
    // seek to first record
    it->Seek(std::vector<unsigned char>{});
    for (uint32_t mapType = 1; mapType <= tournamentMapping; mapType++) {
        for (uint32_t mapId = 1; mapId <= 4097; mapId++, it->Next()) {
            BOOST_CHECK(it->Valid());
            CBettingDB::BytesToDbType(it->Value(), mapping);
            BOOST_CHECK_EQUAL(mapping.nMType, mapType);
            BOOST_CHECK_EQUAL(mapping.nId, mapId);
            BOOST_CHECK_EQUAL(mapping.sName, nameBases[mapType - 1] + std::to_string(mapId));
        }
    }

    BOOST_CHECK(dbCache.Flush());

    // check main DB is empty
    it = db.NewIterator();
    // seek to first record
    it->Seek(std::vector<unsigned char>{});
    // check that iterator of empty table is invalid
    BOOST_CHECK(!it->Valid());

    std::cout << "Testing of flushable DB iterator passed" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
