// Copyright (c) 2020 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <betting/bet_db.h>
#include <boost/format.hpp>

#define DOUBLE_ODDS(odds) (static_cast<double>(odds) / BET_ODDSDIVISOR)
#define DOUBLE_MODIFIER(mod) (static_cast<double>(mod) / MODIFIER_DIVISOR)
#define DOUBLE_MARGIN(mrg) (static_cast<double>(mrg) / MARGIN_DIVISOR / 100)

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

uint32_t CFieldEventDB::NoneZeroOddsContendersCount()
{
    uint32_t noneZeroCount = 0;
    for (const auto& contender : contenders) {
        if (contender.second.nInputOdds != 0) noneZeroCount++;
    }

    return noneZeroCount;
}

bool CFieldEventDB::IsMarketOpen(const FieldBetOutcomeType type) {
    uint32_t contendersCount = NoneZeroOddsContendersCount();
    switch(type) {
        case outright:
            break; // always open
        case show:
            if (nMarketType == FieldEventMarketType::outrightOnly || contendersCount < 8)
                return false;
            break;
        case place:
            if (nMarketType == FieldEventMarketType::outrightOnly || contendersCount < 5)
                return false;
            break;
        default:
            return false;
    }
    return true;
}

void CFieldEventDB::ExtractDataFromTx(const CFieldEventTx& tx)
{
    nEventId = tx.nEventId;
    nStartTime = tx.nStartTime;
    nSport = tx.nSport;
    nTournament = tx.nTournament;
    nStage = tx.nStage;
    nGroupType = tx.nGroupType;
    nMarketType = tx.nMarketType;
    nMarginPercent = tx.nMarginPercent;

    for (const auto& tx_contender : tx.mContendersInputOdds) {
        contenders[tx_contender.first] = ContenderInfo{tx_contender.second, 0, 0, 0, 0};
    }
}

void CFieldEventDB::ExtractDataFromTx(const CFieldUpdateOddsTx& tx)
{
    // Add new contenders and update current input odds
    for (const auto& tx_contender : tx.mContendersInputOdds) {
        contenders[tx_contender.first].nInputOdds = tx_contender.second;
        contenders[tx_contender.first].nOutrightOdds = 0;
        contenders[tx_contender.first].nPlaceOdds = 0;
        contenders[tx_contender.first].nShowOdds = 0;
    }
}

void CFieldEventDB::ExtractDataFromTx(const CFieldUpdateMarginTx& tx)
{
    // Update margin percent
    nMarginPercent = tx.nMarginPercent;
}

void CFieldEventDB::ExtractDataFromTx(const CFieldUpdateModifiersTx& tx)
{
    // Add new contenders modifiers
    for (const auto& tx_contender : tx.mContendersModifires) {
        contenders[tx_contender.first].nModifier = tx_contender.second;
    }
}

void CFieldEventDB::CalculateFairOdds(std::map<uint32_t, uint32_t>& mContendersFairOdds)
{
    double X = 1.0;
    uint32_t it = 1;
    double delta = 0.0;
    double debugStepMargin = 0.0;
    // calc X
    do {
        double stepMargin = 0.0;
        for (const auto& contender : contenders) {
            if (contender.second.nInputOdds == 0) continue;
            double delim = (DOUBLE_ODDS(contender.second.nInputOdds) - 1 + X);
            if (delim != 0)
                stepMargin += X / delim;
        }
        delta = (stepMargin - 1.0) > 0 ?
            (-0.99 / static_cast<double>(it)) :
            (round((stepMargin - 1.0) * 100000) / 100000) == 0 ?
                0 :
                (0.99 / static_cast<double>(it));
        X = (X + delta) < 0 ? 0 : (X + delta);
        debugStepMargin = stepMargin;
        it++;
    } while (delta != 0 && it < 200);

    LogPrint("wagerr", "CalcFairOdds: X = %f, lastMargin = %f, delta = %f, it = %d\n", X, debugStepMargin, delta, it);

    // create map of fair odds
    for (const auto& contender : contenders) {
        if (contender.second.nInputOdds == 0) {
            mContendersFairOdds.emplace(contender.first, 0);
        }
        else {
            double fairOdds = (1.0 / (X / (X + DOUBLE_ODDS(contender.second.nInputOdds) - 1)));
            mContendersFairOdds.emplace(contender.first, static_cast<uint32_t>(round(fairOdds * BET_ODDSDIVISOR)));
        }
    }
}

void CFieldEventDB::CalculateOutrightOdds(const std::map<uint32_t, uint32_t>& mContendersFairOdds, std::map<uint32_t, uint32_t>& mContendersOutrightOdds)
{
    double X = 1.0;
    uint32_t it = 1;
    double delta = 0.0;
    double margin = DOUBLE_MARGIN(nMarginPercent);
    double debugStepMargin = 0.0;

    // calc X
    do {
        double stepMargin = 0.0;
        for (const auto& contender : contenders) {
            if (contender.second.nInputOdds == 0) continue;
            double modifier = DOUBLE_MODIFIER(contender.second.nModifier);
            double fairOdds = DOUBLE_ODDS(mContendersFairOdds.at(contender.first));
            double delim = 1 + (fairOdds - 1) * (X + modifier);
            if (delim != 0) {
                // 1 / (1 + (fairOdds(i) - 1) * (X + modifier(i)))
                stepMargin += 1.0 / delim;
            }
        }

        double dIt = static_cast<double>(it);

        delta = (stepMargin - margin) > 0 ?
            (0.99 / dIt) :
            (round((stepMargin - margin) * 100000) / 100000) == 0 ?
                0.0 :
                (-0.99 / dIt);
        X = (X + delta) < 0 ? 0 : (X + delta);
        debugStepMargin = stepMargin;
        it++;

    } while (delta != 0 && it < 200);

    LogPrint("wagerr", "CalcFairOdds: X = %f, lastMargin = %f, delta = %f, it = %d\n", X, debugStepMargin, delta, it);

    // create map of outrights odds
    for (const auto& contender : contenders) {
        if (contender.second.nInputOdds == 0) {
            mContendersOutrightOdds.emplace(contender.first, 0);
        }
        else {
            double modifier = DOUBLE_MODIFIER(contender.second.nModifier);
            double fairOdds = DOUBLE_ODDS(mContendersFairOdds.at(contender.first));
            // 1 + (fairOdds(i) - 1) * (X + modifier(i))
            double outrightOdds = 1 + (fairOdds - 1) * (X + modifier);
            mContendersOutrightOdds.emplace(contender.first, static_cast<uint32_t>(round(outrightOdds * BET_ODDSDIVISOR)));
        }
    }
}

// ReCalculate Market specific Odds
void CFieldEventDB::CalcOdds()
{

    // calc fair odds (without margin)
    std::map<uint32_t, uint32_t> mContendersFairOdds;
    CalculateFairOdds(mContendersFairOdds);

    // calc outright odds (with margin)
    std::map<uint32_t, uint32_t> mContendersOutrightOdds;
    CalculateOutrightOdds(mContendersFairOdds, mContendersOutrightOdds);

    uint32_t noneZeroCount = NoneZeroOddsContendersCount();

    bool isPlaceOpen = IsMarketOpen(place);
    bool isShowOpen = IsMarketOpen(show);

    double mPlace = 0.0, mShow = 0.0;
    double XPlace = 0.0, XShow = 0.0;

    std::vector<std::pair<uint32_t, uint32_t>> vContendersPlaceOddsMods;
    if (isPlaceOpen) {
        switch (nGroupType) {
        case FieldEventGroupType::animalRacing:
        {
            const double lambda = GetLambda(noneZeroCount);
            for (const auto& cont : mContendersFairOdds) {
                if (cont.second == 0)
                    vContendersPlaceOddsMods.emplace_back(0, 0);
                else
                    vContendersPlaceOddsMods.emplace_back(
                        CalculateAnimalPlaceOdds(cont.first, lambda, mContendersFairOdds),
                        contenders.at(cont.first).nModifier
                    );
            }

            break;
        }
        case FieldEventGroupType::other:
        {
            // Evaluates the probability of the provided player - with index idx - arriving amongst the first 3
            std::vector<std::vector<uint32_t>> vIdsPermutations;
            Permutations2(mContendersFairOdds, vIdsPermutations);
            for (const auto& cont : mContendersFairOdds) {
                if (cont.second == 0)
                    vContendersPlaceOddsMods.emplace_back(0, 0);
                else
                    vContendersPlaceOddsMods.emplace_back(
                        CalculateOddsInFirstN(cont.first, vIdsPermutations, mContendersFairOdds),
                        contenders.at(cont.first).nModifier
                    );
            }

            break;
        }
        default:
            break;
        }

        double realMarginIn = DOUBLE_MARGIN(nMarginPercent) * 2.0;
        mPlace = CalculateM(vContendersPlaceOddsMods, realMarginIn);
        XPlace = CalculateX(vContendersPlaceOddsMods, realMarginIn);

        LogPrint("wagerr", "Place m = %f X = %f\n", mShow, XShow);
    }

    std::vector<std::pair<uint32_t, uint32_t>> vContendersShowOddsMods;
    if (isShowOpen) {
        switch (nGroupType) {
        case FieldEventGroupType::animalRacing:
        {
            const double lambda = GetLambda(noneZeroCount);
            const double rho = GetRHO(noneZeroCount);
            for (const auto& cont : mContendersFairOdds) {
                if (cont.second == 0)
                    vContendersShowOddsMods.emplace_back(0, 0);
                else
                    vContendersShowOddsMods.emplace_back(
                        CalculateAnimalShowOdds(cont.first, lambda, rho, mContendersFairOdds),
                        contenders.at(cont.first).nModifier
                    );
            }

            break;
        }
        case FieldEventGroupType::other:
        {
            // Evaluates the probability of the provided player - with index idx - arriving amongst the first 3
            std::vector<std::vector<uint32_t>> vIdsPermutations;
            Permutations3(mContendersFairOdds, vIdsPermutations);
            for (const auto& cont : mContendersFairOdds) {
                if (cont.second == 0)
                    vContendersShowOddsMods.emplace_back(0, 0);
                else
                    vContendersShowOddsMods.emplace_back(
                        CalculateOddsInFirstN(cont.first, vIdsPermutations, mContendersFairOdds),
                        contenders.at(cont.first).nModifier
                    );
            }

            break;
        }
        default:
            break;
        }

        double realMarginIn = DOUBLE_MARGIN(nMarginPercent) * 3.0;
        mShow = CalculateM(vContendersShowOddsMods, realMarginIn);
        XShow = CalculateX(vContendersShowOddsMods, realMarginIn);

        LogPrint("wagerr", "Show m = %f X = %f\n", mShow, XShow);
    }

    // Update all contenders odds
    size_t i = 0;
    for (auto& contender : contenders) {
        uint32_t modifier = contender.second.nModifier;
        contender.second.nOutrightOdds = mContendersOutrightOdds.at(contender.first);
        contender.second.nPlaceOdds = isPlaceOpen ? CalculateMarketOdds(XPlace, mPlace, vContendersPlaceOddsMods[i].first, modifier) : 0;
        contender.second.nShowOdds = isShowOpen ? CalculateMarketOdds(XShow, mShow, vContendersShowOddsMods[i].first, modifier) : 0;
        i++;
    }
}

double CFieldEventDB::GetLambda(const uint32_t ContendersSize) {
    switch (ContendersSize) {
        case 3: return 0.6667;
        case 4:	return 0.6996;
        case 5:	return 0.7207;
        case 6:	return 0.7359;
        case 7:	return 0.7475;
        case 8:	return 0.7569;
        case 9:	return 0.7648;
        case 10: return 0.7714;
        case 11: return 0.7771;
        case 12: return 0.7822;
        case 13: return 0.7867;
        case 14: return 0.7907;
        default: return 0.76;
    }
}

double CFieldEventDB::GetRHO(const uint32_t ContendersSize) {
    switch (ContendersSize) {
        case 4: return 0.5336;
        case 5: return 0.5703;
        case 6: return 0.5952;
        case 7: return 0.6138;
        case 8: return 0.6285;
        case 9: return 0.6406;
        case 10: return 0.6508;
        case 11: return 0.6596;
        case 12: return 0.6672;
        case 13: return 0.6741;
        case 14: return 0.6802;
        default: return 0.62;
    }
}

uint32_t CFieldEventDB::CalculateAnimalPlaceOdds(const uint32_t idx, const double lambda, const std::map<uint32_t, uint32_t>& mContendersFairOdds)
{
    // First place
    double resultProb = 1.0 / DOUBLE_ODDS(mContendersFairOdds.at(idx));
    // Second place
    for (auto& contender : mContendersFairOdds) {
        if (contender.second == 0) continue;
        if (contender.first == idx) continue;

        const auto idx1 = contender.first;
        const auto idx2 = idx;

        double den = 0.0;
        for (auto it = mContendersFairOdds.begin(); it != mContendersFairOdds.end(); it++) {
            if (it->second == 0) continue;
            if (it->first == contender.first) continue;
            const double prob = 1.0 / DOUBLE_ODDS(it->second);
            den += pow(prob, lambda);
        }

        const double probIdx1 = 1.0 / DOUBLE_ODDS(mContendersFairOdds.at(idx1));
        const double probIdx2 = 1.0 / DOUBLE_ODDS(mContendersFairOdds.at(idx2));
        resultProb += probIdx1 * pow(probIdx2, lambda) / den;
    }

    return static_cast<uint32_t>((1.0 / resultProb) * BET_ODDSDIVISOR);
}

uint32_t CFieldEventDB::CalculateAnimalShowOdds(const uint32_t idx, const double lambda, const double rho, const std::map<uint32_t, uint32_t>& mContendersFairOdds)
{
    // First two places
    double resultProb = 1.0 / DOUBLE_ODDS(CalculateAnimalPlaceOdds(idx, lambda, mContendersFairOdds));
    // Third place
    for (auto it_i = mContendersFairOdds.begin(); it_i != mContendersFairOdds.end(); it_i++) {
        if (it_i->second == 0) continue;
        if (it_i->first == idx) continue;
        for (auto it_j = mContendersFairOdds.begin(); it_j != mContendersFairOdds.end(); it_j++) {
            if (it_j->second == 0) continue;
            if (it_j->first == idx) continue;
            if (it_j->first == it_i->first) continue;

            const auto idx1 = it_i->first;
            const auto idx2 = it_j->first;
            const auto idx3 = idx;

            double den1 = 0.0;
            for (auto it_k = mContendersFairOdds.begin(); it_k != mContendersFairOdds.end(); it_k++) {
                if (it_k->second == 0) continue;
                if (it_k->first == idx1) continue;
                double prob = 1.0 / DOUBLE_ODDS(it_k->second);
                den1 += pow(prob, lambda);
            }

            double den2 = 0.0;
            for (auto it_k = mContendersFairOdds.begin(); it_k != mContendersFairOdds.end(); it_k++) {
                if (it_k->second == 0) continue;
                if (it_k->first == idx1) continue;
                if (it_k->first == idx2) continue;
                double prob = 1.0 / DOUBLE_ODDS(it_k->second);
                den2 += pow(prob, rho);
            }

            double probIdx1 = 1.0 / DOUBLE_ODDS(mContendersFairOdds.at(idx1));
            double probIdx2 = 1.0 / DOUBLE_ODDS(mContendersFairOdds.at(idx2));
            double probIdx3 = 1.0 / DOUBLE_ODDS(mContendersFairOdds.at(idx3));
            resultProb += probIdx1 * pow(probIdx2, lambda) * pow(probIdx3, rho) / (den1 * den2);
        }
    }

    return static_cast<uint32_t>((1.0 / resultProb) * BET_ODDSDIVISOR);
}

void CFieldEventDB::Permutations2(const std::map<uint32_t, uint32_t>& mContendersOdds, std::vector<std::vector<uint32_t>>& perms)
{
    for (const auto& cont_i : mContendersOdds) {
        if (cont_i.second == 0) continue;
        for (const auto& cont_j : mContendersOdds) {
            if (cont_j.second == 0) continue;
            if (cont_i.first == cont_j.first) continue;
            perms.push_back({cont_i.first, cont_j.first});
        }
    }
}

void CFieldEventDB::Permutations3(const std::map<uint32_t, uint32_t>& mContendersOdds, std::vector<std::vector<uint32_t>>& perms)
{
    // TODO: optimize this
    for (const auto& cont_i : mContendersOdds) {
        if (cont_i.second == 0) continue;
        for (const auto& cont_j : mContendersOdds) {
            if (cont_j.second == 0) continue;
            if (cont_i.first == cont_j.first) continue;
            for (const auto& cont_k : mContendersOdds) {
                if (cont_k.second == 0) continue;
                if (cont_k.first == cont_i.first) continue;
                if (cont_k.first == cont_j.first) continue;
                perms.push_back({cont_i.first, cont_j.first, cont_k.first});
            }
        }
    }
}

uint32_t CFieldEventDB::CalculateOddsInFirstN(const uint32_t idx, const std::vector<std::vector<uint32_t>>& permutations, const std::map<uint32_t, uint32_t>& mContendersFairOdds)
{
    double resultProb = 0.0;
    for (const auto& perm : permutations) {
        bool isInArr = false;
        for (const auto val : perm) {
            if (val == idx) {
                isInArr = true;
                break;
            }
        }

        if (!isInArr) continue;

        std::vector<double> currentProbs;
        for (uint32_t cont_id : perm) {
            currentProbs.push_back(1.0 / DOUBLE_ODDS(mContendersFairOdds.at(cont_id)));
        }

        double evalExactOrder = 1.0;
        for (const auto prob : currentProbs) {
            evalExactOrder *= prob;
        }

        double den = 1;
        for (size_t i = 0; i < currentProbs.size(); i++) {
            double q = 1;
            for (size_t j = 0; j < i; j++) {
                q = q - currentProbs[j];
            }
            den = den * q;
        }

        resultProb = resultProb + (evalExactOrder / den);
    }

    return static_cast<uint32_t>((1.0 / resultProb) * BET_ODDSDIVISOR);
}

double CFieldEventDB::CalculateX(const std::vector<std::pair<uint32_t, uint32_t>>& vContendersOddsMods, const double realMarginIn)
{
    const double h = 0.000001;
    double X = 1;
    double Xd = X + h;
    double Xs = X - h;

    for (const auto& cont : vContendersOddsMods) {
        if (cont.first == 0) continue;

        double f = 0.0, fd = 0.0, fs = 0.0;
        for (const auto& cont : vContendersOddsMods) {
            const auto& fairOdds =  DOUBLE_ODDS(cont.first);
            const auto& modifier = DOUBLE_MODIFIER(cont.second);

            if (fairOdds == 0.0) continue;

            f += 1.0 / (1.0 + (X + modifier) * (fairOdds - 1.0));
            fd += 1.0 / (1.0 + (Xd + modifier) * (fairOdds - 1.0));
            fs += 1.0 / (1.0 + (Xs + modifier) * (fairOdds - 1.0));
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

    for (const auto& cont : vContendersOddsMods) {
        if (cont.first == 0) continue;

        double f = 0.0, fd = 0.0, fs = 0.0;
        for (const auto& cont : vContendersOddsMods) {
            const auto& fairOdds = DOUBLE_ODDS(cont.first);
            const auto& modifier = DOUBLE_MODIFIER(cont.second);

            if (fairOdds == 0.0) continue;

            double fairP = 1.0 / fairOdds;
            f += pow(fairP, (m + modifier));
            fd += pow(fairP, (md + modifier));
            fs += pow(fairP, (ms + modifier));
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

uint32_t CFieldEventDB::CalculateMarketOdds(const double X, const double m, const uint32_t oddsMods, const uint16_t modifier)
{
    uint32_t outputOdds = 0;
    if (oddsMods == 0) {
        return outputOdds;
    }

    const double o = DOUBLE_ODDS(oddsMods);
    double oddsX = 1.0 + (X + modifier) * (o - 1.0);

    const double p = 1.0 / o;
    double oddsM = pow(p, (-m-modifier));

    outputOdds = static_cast<uint32_t>((oddsM + oddsX) / 2.0 * BET_ODDSDIVISOR);
    return outputOdds;
}

std::string CFieldEventDB::ToString()
{
    boost::format fmt =
        boost::format(
        "Event id: %lu, startTime: %llu, groupType: %d, sport: %lu, tournament: %lu, stage: %lu, margin: %lu\n"
        "Contenders info:\n%s")
        % nEventId % nStartTime % nGroupType % nSport % nTournament % nStage % nMarginPercent % ContendersToString();

    return fmt.str();
}

std::string CFieldEventDB::ContendersToString()
{
    std::string str;
    for (auto &contender : contenders) {
        ContenderInfo &info = contender.second;
        boost::format fmt =
            boost::format("Contender id: %lu, modifier: %lu, inputOdds: %lu, outrightOdds: %lu, placeOdds: %lu, showOdds: %lu\n") %
            contender.first % info.nModifier % info.nInputOdds % info.nOutrightOdds % info.nPlaceOdds % info.nShowOdds;
        str.append(fmt.str());
    }

    return str;
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