// Copyright (c) 2011-2015 The Bitcoin developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"

#include "addressbookpage.h"
#include "bip38tooldialog.h"
#include "bitcoingui.h"
#include "blockexplorer.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "masternodeconfig.h"
#include "multisenddialog.h"
#include "multisigdialog.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "receivecoinsdialog.h"
#include "privacydialog.h"
#include "sendcoinsdialog.h"
#include "placebetdialog.h"
#include "placebetevent.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"

#include "ui_interface.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include <cstdio>
#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>

WalletView::WalletView(QWidget* parent) : QStackedWidget(parent),
                                          clientModel(0),
                                          walletModel(0)
{   
    // Create tabs
    overviewPage = new OverviewPage();
    explorerWindow = new BlockExplorer(this);
    transactionsPage = new QWidget(this);

    // Create Header with the same names as the other forms to be CSS-Id compatible
    QFrame *frame_Header = new QFrame(transactionsPage);
    frame_Header->setObjectName(QStringLiteral("frame_Header"));

    QVBoxLayout* verticalLayout_8 = new QVBoxLayout(frame_Header);
    verticalLayout_8->setObjectName(QStringLiteral("verticalLayout_8"));
    verticalLayout_8->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* horizontalLayout_Header = new QHBoxLayout();
    horizontalLayout_Header->setObjectName(QStringLiteral("horizontalLayout_Header"));

    QLabel* labelOverviewHeaderLeft = new QLabel(frame_Header);
    labelOverviewHeaderLeft->setObjectName(QStringLiteral("labelOverviewHeaderLeft"));
    labelOverviewHeaderLeft->setMinimumSize(QSize(464, 60));
    labelOverviewHeaderLeft->setMaximumSize(QSize(16777215, 60));
    labelOverviewHeaderLeft->setText(tr("HISTORY"));
    QFont fontHeaderLeft;
    fontHeaderLeft.setPointSize(20);
    fontHeaderLeft.setBold(true);
    fontHeaderLeft.setWeight(75);
    labelOverviewHeaderLeft->setFont(fontHeaderLeft);

    horizontalLayout_Header->addWidget(labelOverviewHeaderLeft);
    QSpacerItem* horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout_Header->addItem(horizontalSpacer_3);

    QLabel* labelOverviewHeaderRight = new QLabel(frame_Header);
    labelOverviewHeaderRight->setObjectName(QStringLiteral("labelOverviewHeaderRight"));
    labelOverviewHeaderRight->setMinimumSize(QSize(464, 60));
    labelOverviewHeaderRight->setMaximumSize(QSize(16777215, 60));
    labelOverviewHeaderRight->setText(QString());
    QFont fontHeaderRight;
    fontHeaderRight.setPointSize(14);
    labelOverviewHeaderRight->setFont(fontHeaderRight);
    labelOverviewHeaderRight->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

    horizontalLayout_Header->addWidget(labelOverviewHeaderRight);
    horizontalLayout_Header->setStretch(0, 1);
    horizontalLayout_Header->setStretch(2, 1);
    verticalLayout_8->addLayout(horizontalLayout_Header);

    QVBoxLayout* vbox = new QVBoxLayout();
    QHBoxLayout* hbox_buttons = new QHBoxLayout();
    vbox->addWidget(frame_Header);

    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    QPushButton* exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
#ifndef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    exportButton->setIcon(QIcon(":/icons/export"));
#endif
    hbox_buttons->addStretch();

    // Sum of selected transactions
    QLabel* transactionSumLabel = new QLabel();                // Label
    transactionSumLabel->setObjectName("transactionSumLabel"); // Label ID as CSS-reference
    transactionSumLabel->setText(tr("Selected amount:"));
    hbox_buttons->addWidget(transactionSumLabel);

    transactionSum = new QLabel();                   // Amount
    transactionSum->setObjectName("transactionSum"); // Label ID as CSS-reference
    transactionSum->setMinimumSize(200, 8);
    transactionSum->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hbox_buttons->addWidget(transactionSum);

    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    privacyPage = new PrivacyDialog();
    receiveCoinsPage = new ReceiveCoinsDialog();
    sendCoinsPage = new SendCoinsDialog();
    placeBetPage = new PlaceBetDialog();

    addWidget(overviewPage);
    addWidget(transactionsPage);
    addWidget(privacyPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);
    addWidget(placeBetPage);
    addWidget(explorerWindow);

    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeListPage = new MasternodeList();
        addWidget(masternodeListPage);
    }

    // Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Update wallet with sum of selected transactions
    connect(transactionView, SIGNAL(trxAmount(QString)), this, SLOT(trxAmount(QString)));

    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    // Pass through messages from sendCoinsPage
    connect(sendCoinsPage, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

    // Pass through messages from sendCoinsPage
    connect(placeBetPage, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

    // Pass through messages from transactionView
    connect(transactionView, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI* gui)
{
    if (gui) {
        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoHistoryPage()));

        // Receive and report messages
        connect(this, SIGNAL(message(QString, QString, unsigned int)), gui, SLOT(message(QString, QString, unsigned int)));

        // Pass through encryption status changed signals
        connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Pass through transaction notifications
        connect(this, SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString)), gui, SLOT(incomingTransaction(QString, int, CAmount, QString, QString)));
    }
}

void WalletView::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;

    overviewPage->setClientModel(clientModel);
    sendCoinsPage->setClientModel(clientModel);
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeListPage->setClientModel(clientModel);
    }
}

void WalletView::setWalletModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;

    // Put transaction list in tabs
    transactionView->setModel(walletModel);
    overviewPage->setWalletModel(walletModel);
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeListPage->setWalletModel(walletModel);
    }
    privacyPage->setModel(walletModel);
    receiveCoinsPage->setModel(walletModel);
    sendCoinsPage->setModel(walletModel);
    placeBetPage->setModel(walletModel);

    if (walletModel) {
        // Receive and pass through messages from wallet model
        connect(walletModel, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

        // Handle changes in encryption status
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
        updateEncryptionStatus();

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex, int, int)),
            this, SLOT(processNewTransaction(QModelIndex, int, int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock(AskPassphraseDialog::Context)), this, SLOT(unlockWallet(AskPassphraseDialog::Context)));

        // Show progress dialog
        connect(walletModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));
    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QString address = ttm->index(start, TransactionTableModel::ToAddress, parent).data().toString();

    emit incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address);
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
    // Refresh UI-elements in case coins were locked/unlocked in CoinControl
    walletModel->emitBalanceChanged();
}

void WalletView::gotoHistoryPage()
{
    setCurrentWidget(transactionsPage);
}


void WalletView::gotoBlockExplorerPage()
{
    setCurrentWidget(explorerWindow);
}

void WalletView::gotoMasternodePage()
{
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        setCurrentWidget(masternodeListPage);
    }
}

void WalletView::gotoReceiveCoinsPage()
{
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoPrivacyPage()
{
    setCurrentWidget(privacyPage);
    // Refresh UI-elements in case coins were locked/unlocked in CoinControl
    walletModel->emitBalanceChanged();
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoPlaceBetPage(QString addr)
{
    setCurrentWidget(placeBetPage);

    // go thru blockchain and get data

    // Set the Oracle wallet address. 
    std::string OracleWalletAddr = Params().OracleWalletAddr();

    // Set event name
    std::string evtDes;

    std::map<std::string, std::string> eventNames;
    eventNames.insert(make_pair("WCUP", "World Cup"));

    std::map<std::string, std::string> roundNames;
    roundNames.insert(make_pair("R1", "Round 1"));
    roundNames.insert(make_pair("RD2", "Round 2"));
    roundNames.insert(make_pair("RD3", "Round 3"));
    roundNames.insert(make_pair("F16", "Final 16"));
    roundNames.insert(make_pair("QFL", "Quarter Final"));
    roundNames.insert(make_pair("SFL", "Semi Final"));
    roundNames.insert(make_pair("FIN", "Final"));


    std::map<std::string, std::string> countryNames;
    countryNames.insert(make_pair("ARG", "Argentina"));
    countryNames.insert(make_pair("AUS", "Australia"));
    countryNames.insert(make_pair("BRA", "Brazil"));
    countryNames.insert(make_pair("CRC", "Costa Rica"));
    countryNames.insert(make_pair("DEN", "Denmark"));
    countryNames.insert(make_pair("EGY", "Egypt"));
    countryNames.insert(make_pair("ESP", "Spain"));
    countryNames.insert(make_pair("FRA", "France"));
    countryNames.insert(make_pair("GER", "Germany"));
    countryNames.insert(make_pair("IRN", "Iran"));
    countryNames.insert(make_pair("ISL", "Iceland"));
    countryNames.insert(make_pair("KSA", "Saudi Arabia"));
    countryNames.insert(make_pair("MAR", "Morocco"));
    countryNames.insert(make_pair("MEX", "Mexico"));
    countryNames.insert(make_pair("PER", "Peru"));
    countryNames.insert(make_pair("POR", "Portugal"));
    countryNames.insert(make_pair("RUS", "Russia"));
    countryNames.insert(make_pair("SRB", "Serbia"));
    countryNames.insert(make_pair("URU", "Uruguay"));
    countryNames.insert(make_pair("BEL", "Belgium"));
    countryNames.insert(make_pair("COL", "Columbia"));
    countryNames.insert(make_pair("CRO", "Croatia"));
    countryNames.insert(make_pair("ENG", "England"));
    countryNames.insert(make_pair("JPN", "Japan"));
    countryNames.insert(make_pair("KOR", "Korea Republic"));
    countryNames.insert(make_pair("NGA", "Nigeria"));
    countryNames.insert(make_pair("NIG", "Nigeria"));
    countryNames.insert(make_pair("PAN", "Panama"));
    countryNames.insert(make_pair("POL", "Poland"));
    countryNames.insert(make_pair("SEN", "Senegal"));
    countryNames.insert(make_pair("SWE", "Sweden"));
    countryNames.insert(make_pair("SUI", "Switzerland"));
    countryNames.insert(make_pair("TUN", "Tunisia "));

    placeBetPage->clear();
    std::vector<CEvent *> eventsVector;

    // Look back to the betting start block for events to list on the wallet QT.
    CBlockIndex* pindex =  chainActive.Height() > Params().BetStartHeight() ? chainActive[Params().BetStartHeight()] : NULL;

    while (pindex) {

        CBlock block;
        ReadBlockFromDisk(block, pindex);

        BOOST_FOREACH (CTransaction& tx, block.vtx) {

            // Ensure event TX has been posted by Oracle wallet.
            bool validEventTx = false;
            const CTxIn &txin = tx.vin[0];
            COutPoint prevout = txin.prevout;

            uint256 hashBlock;
            CTransaction txPrev;
            if (GetTransaction(prevout.hash, txPrev, hashBlock, true)) {

                const CTxOut &prevTxOut  = txPrev.vout[0];
                std::string scriptPubKey = prevTxOut.scriptPubKey.ToString();

                txnouttype type;
                vector<CTxDestination> prevAddrs;
                int nRequired;
                if(ExtractDestinations(prevTxOut.scriptPubKey, type, prevAddrs, nRequired)) {

                    BOOST_FOREACH (const CTxDestination &prevAddr, prevAddrs) {
                        if (CBitcoinAddress(prevAddr).ToString() == OracleWalletAddr) {
                            validEventTx = true;
                        }
                    }
                }
            }

            // Check TX vouts to see if they contain event op code.
            for (unsigned int i = 0; i < tx.vout.size(); i++) {

                const CTxOut& txout = tx.vout[i];
                std::string scriptPubKey = txout.scriptPubKey.ToString();

                // TODO Remove hard-coded values from this block.
                if ( validEventTx && scriptPubKey.length() > 0 && strncmp(scriptPubKey.c_str(), "OP_RETURN", 9) == 0) {

                    vector<unsigned char> v = ParseHex(scriptPubKey.substr(9, string::npos));
                    std::string evtDescr(v.begin(), v.end());
                    std::vector<std::string> strs;
                    boost::split(strs, evtDescr, boost::is_any_of("|"));

                    if (strs.size() != 11 ) {
                        continue;
                    }

                    time_t time = (time_t) std::strtol(strs[3].c_str(), nullptr, 10);
                    CEvent *event = CEvent::ParseEvent(evtDescr);
                    time_t currentTime = std::time(0);

                    // Only add events up until 22 minutes (1320 seconds) before they start.
                    if( time > ( currentTime + 1320 ) ){

                        // Add events to vector
                        eventsVector.push_back(event);
                    }
                }
            }
        }

        pindex = chainActive.Next(pindex);
    }

    //remove duplicates from list (Remove older duplicates)
    std::vector<CEvent *>  cleanEventsVector;
    for(unsigned int i = 0; i < eventsVector.size(); i++ ){

        bool found = false;

        //loop through and check if the eventid matches a result event id
        for (unsigned int j = 0; j < cleanEventsVector.size(); j++) {

            if (eventsVector[i]->id == cleanEventsVector[j]->id) {
                found = true;

                //remove the event if all odds are zero (cancel event TX)
                if( eventsVector[i]->homeOdds == "N/A" && eventsVector[i]->awayOdds == "N/A" && eventsVector[i]->drawOdds == "N/A" ){
                    cleanEventsVector.erase(cleanEventsVector.begin() + j);
                }
                //replace the old event details with the updated event details
                else
                {
                    std::replace(cleanEventsVector.begin(), cleanEventsVector.end(), cleanEventsVector[j], eventsVector[i]);
                }
            }
        }

        //if not found put the event into the clean list
        if( ! found){
            cleanEventsVector.push_back(eventsVector[i]);
        }
    }

    struct compare_times{
        inline bool operator() (CEvent *event1, CEvent *event2){
            return (event1->starting < event2->starting);
        }
    };
    std::sort(cleanEventsVector.begin(), cleanEventsVector.end(), compare_times());

    //put events on screen
    for(unsigned int i = 0; i < cleanEventsVector.size(); i++ ){

        std::string evtDes = "";
        time_t time = (time_t) std::strtol(cleanEventsVector[i]->starting.c_str(), nullptr, 10);

        std::map<std::string, std::string>::iterator it;
        it = eventNames.find(cleanEventsVector[i]->name);
        // TODO Investigate whether it would be better to skip event
        // descriptions with unsupported fields rather than to
        // output those fields.
        evtDes += it == eventNames.end() ? cleanEventsVector[i]->name : it->second;

        evtDes += " ";
        it = roundNames.find(cleanEventsVector[i]->round);
        evtDes += it == roundNames.end() ? cleanEventsVector[i]->round : it->second;
        evtDes += "   ";


        tm *ptm = std::gmtime(&time);
        static const char mon_name[][4] = {
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        char result[22];

        sprintf(
            result,
            "%.2d:%.2d UTC+0   %.2d %3s",
            ptm->tm_hour,
            ptm->tm_min,
            ptm->tm_mday,
            mon_name[ptm->tm_mon]
        );
        evtDes += result;

        evtDes += "   ";
        it = countryNames.find(cleanEventsVector[i]->homeTeam);
        evtDes += it == countryNames.end() ? cleanEventsVector[i]->homeTeam : it->second;
        evtDes += " V ";
        it = countryNames.find(cleanEventsVector[i]->awayTeam);
        evtDes += it == countryNames.end() ? cleanEventsVector[i]->awayTeam : it->second;


        placeBetPage->addEvent(
            cleanEventsVector[i],
            evtDes,
            cleanEventsVector[i]->homeOdds,
            cleanEventsVector[i]->awayOdds,
            cleanEventsVector[i]->drawOdds
        );
    }
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void WalletView::gotoBip38Tool()
{
    Bip38ToolDialog* bip38ToolDialog = new Bip38ToolDialog(this);
    //bip38ToolDialog->setAttribute(Qt::WA_DeleteOnClose);
    bip38ToolDialog->setModel(walletModel);
    bip38ToolDialog->showTab_ENC(true);
}

void WalletView::gotoMultiSendDialog()
{
    MultiSendDialog* multiSendDialog = new MultiSendDialog(this);
    multiSendDialog->setModel(walletModel);
    multiSendDialog->show();
}

void WalletView::gotoMultisigDialog(int index)
{
    MultisigDialog* multisig = new MultisigDialog(this);
    multisig->setModel(walletModel);
    multisig->showTab(index);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
    privacyPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    emit encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Mode::Encrypt : AskPassphraseDialog::Mode::Decrypt, this, 
                            walletModel, AskPassphraseDialog::Context::Encrypt);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        tr("Wallet Data (*.dat)"), NULL);

    if (filename.isEmpty())
        return;
    walletModel->backupWallet(filename);
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::Mode::ChangePass, this, walletModel, AskPassphraseDialog::Context::ChangePass);
    dlg.exec();
}

void WalletView::unlockWallet(AskPassphraseDialog::Context context)
{
    if (!walletModel)
        return;
    // Unlock wallet when requested by wallet model

    if (walletModel->getEncryptionStatus() == WalletModel::Locked || walletModel->getEncryptionStatus() == WalletModel::UnlockedForAnonymizationOnly) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Mode::UnlockAnonymize, this, walletModel, context);
        dlg.exec();
    }
}

void WalletView::lockWallet()
{
    if (!walletModel)
        return;

    walletModel->setWalletLocked(true);
}

void WalletView::toggleLockWallet()
{
    if (!walletModel)
        return;

    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();

    // Unlock the wallet when requested
    if (encStatus == walletModel->Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Mode::UnlockAnonymize, this, walletModel, AskPassphraseDialog::Context::ToggleLock);
        dlg.exec();
    }

    else if (encStatus == walletModel->Unlocked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
            walletModel->setWalletLocked(true);
    }
}

void WalletView::usedSendingAddresses()
{
    if (!walletModel)
        return;
    AddressBookPage* dlg = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(walletModel->getAddressTableModel());
    dlg->show();
}

void WalletView::usedReceivingAddresses()
{
    if (!walletModel)
        return;
    AddressBookPage* dlg = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(walletModel->getAddressTableModel());
    dlg->show();
}

void WalletView::showProgress(const QString& title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog)
        progressDialog->setValue(nProgress);
}

/** Update wallet with the sum of the selected transactions */
void WalletView::trxAmount(QString amount)
{
    transactionSum->setText(amount);
}
