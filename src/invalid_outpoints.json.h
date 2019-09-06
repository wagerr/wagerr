// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018 The WAGERR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef WAGERR_INVALID_OUTPOINTS_JSON_H
#define WAGERR_INVALID_OUTPOINTS_JSON_H
#include <string>

std::string LoadInvalidOutPoints()
{
    std::string str = "[\n"
            "]";
    return str;
}

#endif //WAGERR_INVALID_OUTPOINTS_JSON_H
