// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_db.h>

std::string CMappingDB::ToTypeName(MappingType type)
{
    switch (type) {
    case sportMapping:
        return "sports";
    case roundMapping:
        return "rounds";
    case teamMapping:
        return "teamnames";
    case tournamentMapping:
        return "tournaments";
    }
    return "";
}

MappingType CMappingDB::FromTypeName(const std::string& name)
{
    if (name == ToTypeName(sportMapping)) {
        return sportMapping;
    }
    if (name == ToTypeName(roundMapping)) {
        return roundMapping;
    }
    if (name == ToTypeName(teamMapping)) {
        return teamMapping;
    }
    if (name == ToTypeName(tournamentMapping)) {
        return tournamentMapping;
    }
    return static_cast<MappingType>(-1);
}

/*
 * CBettingDB methods
 */

CFlushableStorageKV& CBettingDB::GetDb()
{
    return db;
}

bool CBettingDB::Flush()
{
    return db.Flush();
}

std::unique_ptr<CStorageKVIterator> CBettingDB::NewIterator()
{
    return db.NewIterator();
}

unsigned int CBettingDB::GetCacheSize()
{
    return db.GetCacheSize();
}

unsigned int CBettingDB::GetCacheSizeBytesToWrite()
{
    return db.GetCacheSizeBytesToWrite();
}

size_t CBettingDB::dbWrapperCacheSize()
{
    return 10 << 20;
}

std::string CBettingDB::MakeDbPath(const char* name)
{
using namespace boost::filesystem;

    std::string result{};
    path dir{GetDataDir()};

    dir /= "betting";
    dir /= name;

    if (boost::filesystem::is_directory(dir) || boost::filesystem::create_directories(dir) ) {
        result = boost::to_string(dir);
        result.erase(0, 1);
        result.erase(result.size() - 1);
    }

    return result;
}

/*
 * CBettingsView methods
 */

// copy constructor for creating DB cache
CBettingsView::CBettingsView(CBettingsView* phr) {
    mappings = MakeUnique<CBettingDB>(*phr->mappings.get());
    results = MakeUnique<CBettingDB>(*phr->results.get());
    events = MakeUnique<CBettingDB>(*phr->events.get());
    bets = MakeUnique<CBettingDB>(*phr->bets.get());
    undos = MakeUnique<CBettingDB>(*phr->undos.get());
    payoutsInfo = MakeUnique<CBettingDB>(*phr->payoutsInfo.get());
    quickGamesBets = MakeUnique<CBettingDB>(*phr->quickGamesBets.get());
    chainGamesLottoEvents = MakeUnique<CBettingDB>(*phr->chainGamesLottoEvents.get());
    chainGamesLottoBets = MakeUnique<CBettingDB>(*phr->chainGamesLottoBets.get());
    chainGamesLottoResults = MakeUnique<CBettingDB>(*phr->chainGamesLottoResults.get());
    failedBettingTxs = MakeUnique<CBettingDB>(*phr->failedBettingTxs.get());
}

bool CBettingsView::Flush() {
    return mappings->Flush() &&
            results->Flush() &&
            events->Flush() &&
            bets->Flush() &&
            undos->Flush() &&
            payoutsInfo->Flush() &&
            quickGamesBets->Flush() &&
            chainGamesLottoEvents->Flush() &&
            chainGamesLottoBets->Flush() &&
            chainGamesLottoResults->Flush();
            failedBettingTxs->Flush();
}

unsigned int CBettingsView::GetCacheSize() {
    return mappings->GetCacheSize() +
            results->GetCacheSize() +
            events->GetCacheSize() +
            bets->GetCacheSize() +
            undos->GetCacheSize() +
            payoutsInfo->GetCacheSize() +
            quickGamesBets->GetCacheSize() +
            chainGamesLottoEvents->GetCacheSize() +
            chainGamesLottoBets->GetCacheSize() +
            chainGamesLottoResults->GetCacheSize() +
            failedBettingTxs->GetCacheSize();
}

unsigned int CBettingsView::GetCacheSizeBytesToWrite() {
    return mappings->GetCacheSizeBytesToWrite() +
            results->GetCacheSizeBytesToWrite() +
            events->GetCacheSizeBytesToWrite() +
            bets->GetCacheSizeBytesToWrite() +
            undos->GetCacheSizeBytesToWrite() +
            payoutsInfo->GetCacheSizeBytesToWrite() +
            quickGamesBets->GetCacheSizeBytesToWrite() +
            chainGamesLottoEvents->GetCacheSizeBytesToWrite() +
            chainGamesLottoBets->GetCacheSizeBytesToWrite() +
            chainGamesLottoResults->GetCacheSizeBytesToWrite() +
            failedBettingTxs->GetCacheSizeBytesToWrite();
}

void CBettingsView::SetLastHeight(uint32_t height) {
    if (!undos->Exists(std::string("LastHeight"))) {
        undos->Write(std::string("LastHeight"), height);
    }
    else {
        undos->Update(std::string("LastHeight"), height);
    }
}

uint32_t CBettingsView::GetLastHeight() {
    uint32_t height;
    if (!undos->Read(std::string("LastHeight"), height))
        return 0;
    return height;
}

bool CBettingsView::SaveBettingUndo(const BettingUndoKey& key, std::vector<CBettingUndoDB> vUndos) {
    assert(!undos->Exists(key));
    return undos->Write(key, vUndos);
}

bool CBettingsView::EraseBettingUndo(const BettingUndoKey& key) {
    return undos->Erase(key);
}

std::vector<CBettingUndoDB> CBettingsView::GetBettingUndo(const BettingUndoKey& key) {
    std::vector<CBettingUndoDB> vUndos;
    if (undos->Read(key, vUndos))
        return vUndos;
    else
        return std::vector<CBettingUndoDB>{};
}

bool CBettingsView::ExistsBettingUndo(const BettingUndoKey& key) {
    return undos->Exists(key);
}

void CBettingsView::PruneOlderUndos(const uint32_t height) {
    static std::vector<unsigned char> lastHeightKey = CBettingDB::DbTypeToBytes(std::string("LastHeight"));
    std::vector<CBettingUndoDB> vUndos;
    BettingUndoKey key;
    std::string str;
    auto it = undos->NewIterator();
    std::vector<BettingUndoKey> vKeysToDelete;
    for (it->Seek(std::vector<unsigned char>{}); it->Valid(); it->Next()) {
        // check that key is serialized "LastHeight" key and skip if true
        if (it->Key() == lastHeightKey) {
            continue;
        }
        CBettingDB::BytesToDbType(it->Key(), key);
        CBettingDB::BytesToDbType(it->Value(), vUndos);
        if (vUndos[0].height < height) {
            vKeysToDelete.push_back(key);
        }
    }
    for (auto && key : vKeysToDelete) {
        undos->Erase(key);
    }
}

bool CBettingsView::SaveFailedTx(const FailedTxKey& key) {
    assert(!failedBettingTxs->Exists(key));
    return failedBettingTxs->Write(key, 0);
}

bool CBettingsView::ExistFailedTx(const FailedTxKey& key) {
    return failedBettingTxs->Exists(key);
}

bool CBettingsView::EraseFailedTx(const FailedTxKey& key) {
    return failedBettingTxs->Erase(key);
}