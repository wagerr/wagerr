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
    case individualSportMapping:
        return "individualSports";
    case contenderMapping:
        return "contenders";
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
    if (name == ToTypeName(individualSportMapping)) {
        return individualSportMapping;
    }
    if (name == ToTypeName(contenderMapping)) {
        return contenderMapping;
    }
    return static_cast<MappingType>(-1);
}

bool CFieldEventDB::IsMarketOpen(const FieldBetMarketType type, const size_t contendersCount) {
    switch(type) {
        case outright:
            break; // always open
        case show:
            if (contendersCount < 8)
                return false;
            break;
        case place:
            if (contendersCount < 5)
                return false;
            break;
        default:
            return false;
    }
    return true;
}

double CFieldEventDB::CalculateX(const std::vector<std::pair<uint32_t, uint32_t>>& vContendersOddsMods, const double realMarginIn)
{
    const double h = 0.000001;
    double X = 1;
    double Xd = X + h;
    double Xs = X - h;

    for (size_t i = 0; i < vContendersOddsMods.size(); i++) {
        double f = 0.0, fd = 0.0, fs = 0.0;
        for (const auto& cont : vContendersOddsMods) {
            const auto& outrightOdds = static_cast<double>(cont.first) / BET_ODDSDIVISOR;
            const auto& modifier = static_cast<double>(cont.second) / MODIFIER_DIVISOR;

            if (outrightOdds == 0.0) continue;

            f += 1.0 / (1.0 + (X + modifier) * (outrightOdds - 1.0));
            fd += 1.0 / (1.0 + (Xd + modifier) * (outrightOdds - 1.0));
            fs += 1.0 / (1.0 + (Xs + modifier) * (outrightOdds - 1.0));
        }

        f -= realMarginIn;
        fd -= realMarginIn;
        fs -= realMarginIn;

        double der = (fd - fs) / h;
        X = X - (f / der);
        Xd = X + h;
        Xs = X - h;
    }

    return X;
}

double CFieldEventDB::CalculateM(const std::vector<std::pair<uint32_t, uint32_t>>& vContendersOddsMods, const double realMarginIn)
{
    const double h = 0.000001;
    double m = 1;
    double md = m + h;
    double ms = m - h;

    for (size_t i = 0; i < vContendersOddsMods.size(); i++) {
        double f = 0.0, fd = 0.0, fs = 0.0;
        for (const auto& cont : vContendersOddsMods) {
            const auto& outrightOdds = static_cast<double>(cont.first) / BET_ODDSDIVISOR;
            const auto& modifier = static_cast<double>(cont.second) / MODIFIER_DIVISOR;

            if (outrightOdds == 0.0) continue;

            double outrightP = 1.0 / outrightOdds;
            f += pow(outrightP, (m + modifier));
            fd += pow(outrightP, (md + modifier));
            fs += pow(outrightP, (ms + modifier));
        }

        f -= realMarginIn;
        fd -= realMarginIn;
        fs -= realMarginIn;

        double der = (fd - fs) / h;
        m = m - (f / der);
        md = m + h;
        ms = m - h;
    }

    return m;
}

uint32_t CFieldEventDB::CalculateMarketOdds(const double X, const double m, const uint32_t outrightOdds, const uint16_t modifier)
{
    uint32_t outputOdds = 0;
    if (outrightOdds == 0) {
        return outputOdds;
    }

    const double o = static_cast<double>(outrightOdds) / BET_ODDSDIVISOR;
    double oddsX = 1.0 + (X + modifier) * (o - 1.0);

    const double p = 1.0 / o;
    double oddsM = pow(p, (-m-modifier));

    outputOdds = static_cast<uint32_t>((oddsM + oddsX) / 2.0 * BET_ODDSDIVISOR);
    return outputOdds;
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
    fieldEvents = MakeUnique<CBettingDB>(*phr->fieldEvents.get());
    fieldResults = MakeUnique<CBettingDB>(*phr->fieldResults.get());
    fieldBets = MakeUnique<CBettingDB>(*phr->fieldBets.get());
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
            fieldEvents->Flush() &&
            fieldResults->Flush() &&
            fieldBets->Flush() &&
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
            fieldEvents->GetCacheSize() +
            fieldResults->GetCacheSize() +
            fieldBets->GetCacheSize() +
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
            fieldEvents->GetCacheSizeBytesToWrite() +
            fieldResults->GetCacheSizeBytesToWrite() +
            fieldBets->GetCacheSizeBytesToWrite() +
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