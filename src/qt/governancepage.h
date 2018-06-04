// Copyright (c) 2018 The WAGERR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GOVERNANCEPAGE_H
#define BITCOIN_QT_GOVERNANCEPAGE_H

#include "masternode.h"
#include "platformstyle.h"
#include "sync.h"
#include "util.h"

#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QTimer>
#include <QWidget>

class ClientModel;
class WalletModel;

namespace Ui
{
    class GovernancePage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog for requesting payment of bitcoins */
class GovernancePage : public QWidget
{
    Q_OBJECT

public:
    explicit GovernancePage(QWidget* parent = 0);
    ~GovernancePage();

    void setClientModel(ClientModel* clientModel);
    void setWalletModel(WalletModel* walletModel);
    void SendVote(std::string strHash, int nVote);

private:
    QMenu* contextMenu;
    int64_t nTimeFilterUpdated;
    bool fFilterUpdated;

public Q_SLOTS:
    void updateProposalList(bool fForce = false);

Q_SIGNALS:

private:
    QTimer* timer;
    Ui::GovernancePage* ui;
    ClientModel* clientModel;
    WalletModel* walletModel;
    QString strCurrentFilter;

private Q_SLOTS:
    void voteButton_clicked(int nVote);
    //void noButton_clicked();
    void on_UpdateButton_clicked();
};
 
#endif // BITCOIN_QT_GOVERNANCEPAGE_H
