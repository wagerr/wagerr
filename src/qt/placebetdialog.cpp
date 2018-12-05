// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "placebetdialog.h"
#include "ui_placebetdialog.h"

#include "addresstablemodel.h"
#include "askpassphrasedialog.h"
#include "bitcoinunits.h"
#include "clientmodel.h"
#include "coincontroldialog.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include "base58.h"
#include "coincontrol.h"
#include "ui_interface.h"
#include "utilmoneystr.h"
#include "wallet.h"

#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>

PlaceBetDialog::PlaceBetDialog(QWidget* parent) : QDialog(parent),
                                                  ui(new Ui::PlaceBetDialog),
                                                  clientModel(0),
                                                  model(0),
                                                  fNewRecipientAllowed(true),
                                                  fFeeMinimized(true),
                                                  betEvent(0)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
//    ui->addButton->setIcon(QIcon());
//    ui->clearButton->setIcon(QIcon());
    ui->placeBetButton->setIcon(QIcon());
#endif

    // GUIUtil::setupAddressWidget(ui->lineEditCoinControlChange, this);

//     addEvent();

//     connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
//     connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));

    // Coin Control
    // connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
    // connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
    // connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString&)), this, SLOT(coinControlChangeEdited(const QString&)));

    // UTXO Splitter
    // connect(ui->splitBlockCheckBox, SIGNAL(stateChanged(int)), this, SLOT(splitBlockChecked(int)));
    // connect(ui->splitBlockLineEdit, SIGNAL(textChanged(const QString&)), this, SLOT(splitBlockLineEditChanged(const QString&)));

    // Wagerr specific
    QSettings settings;
    if (!settings.contains("bUseObfuScation"))
        settings.setValue("bUseObfuScation", false);
    if (!settings.contains("bUseSwiftTX"))
        settings.setValue("bUseSwiftTX", false);

    // bool useSwiftTX = settings.value("bUseSwiftTX").toBool();
    // if (fLiteMode) {
    //     ui->checkSwiftTX->setVisible(false);
    //     CoinControlDialog::coinControl->useObfuScation = false;
    //     CoinControlDialog::coinControl->useSwiftTX = false;
    // } else {
    //     ui->checkSwiftTX->setChecked(useSwiftTX);
    //     CoinControlDialog::coinControl->useSwiftTX = useSwiftTX;
    // }

    // connect(ui->checkSwiftTX, SIGNAL(stateChanged(int)), this, SLOT(updateSwiftTX()));

    // Coin Control: clipboard actions
    QAction* clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction* clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction* clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction* clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction* clipboardPriorityAction = new QAction(tr("Copy priority"), this);
    QAction* clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction* clipboardChangeAction = new QAction(tr("Copy change"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
    // ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    // ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    // ui->labelCoinControlFee->addAction(clipboardFeeAction);
    // ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    // ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    // ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
    // ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    // ui->labelCoinControlChange->addAction(clipboardChangeAction);

    // init transaction fee section
    if (!settings.contains("fFeeSectionMinimized"))
        settings.setValue("fFeeSectionMinimized", true);
    if (!settings.contains("nFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nFeeRadio", 1);                                                                                             // custom
    if (!settings.contains("nFeeRadio"))
        settings.setValue("nFeeRadio", 0);                                                                                                   // recommended
    if (!settings.contains("nCustomFeeRadio") && settings.contains("nTransactionFee") && settings.value("nTransactionFee").toLongLong() > 0) // compatibility
        settings.setValue("nCustomFeeRadio", 1);                                                                                             // total at least
    if (!settings.contains("nCustomFeeRadio"))
        settings.setValue("nCustomFeeRadio", 0); // per kilobyte
    if (!settings.contains("nSmartFeeSliderPosition"))
        settings.setValue("nSmartFeeSliderPosition", 0);
    if (!settings.contains("nTransactionFee"))
        settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
    if (!settings.contains("fPayOnlyMinFee"))
        settings.setValue("fPayOnlyMinFee", false);
    if (!settings.contains("fSendFreeTransactions"))
        settings.setValue("fSendFreeTransactions", false);

//     ui->groupFee->setId(ui->radioSmartFee, 0);
//     ui->groupFee->setId(ui->radioCustomFee, 1);
//     ui->groupFee->button((int)std::max(0, std::min(1, settings.value("nFeeRadio").toInt())))->setChecked(true);
//     ui->groupCustomFee->setId(ui->radioCustomPerKilobyte, 0);
//     ui->groupCustomFee->setId(ui->radioCustomAtLeast, 1);
//     ui->groupCustomFee->button((int)std::max(0, std::min(1, settings.value("nCustomFeeRadio").toInt())))->setChecked(true);
//     ui->sliderSmartFee->setValue(settings.value("nSmartFeeSliderPosition").toInt());
//     ui->customFee->setValue(settings.value("nTransactionFee").toLongLong());
//     ui->checkBoxMinimumFee->setChecked(settings.value("fPayOnlyMinFee").toBool());
//     ui->checkBoxFreeTx->setChecked(settings.value("fSendFreeTransactions").toBool());
//     // ui->checkzWGR->hide();
//     minimizeFeeSection(settings.value("fFeeSectionMinimized").toBool());
}

void PlaceBetDialog::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;

    if (clientModel) {
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(updateSmartFeeLabel()));
    }
}

void PlaceBetDialog::setModel(WalletModel* model)
{
    this->model = model;

    if (model && model->getOptionsModel()) {
        // for (int i = 0; i < ui->events->count(); ++i) {
        //     PlaceBetEvent* event = qobject_cast<PlaceBetEvent*>(ui->events->itemAt(i)->widget());
        //     if (event) {
        //         event->setModel(model);
        //     }
        // }

        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
                   model->getZerocoinBalance (), model->getUnconfirmedZerocoinBalance (), model->getImmatureZerocoinBalance (),
                   model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount, CAmount,  CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this,
                         SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();

//         // Coin Control
//         connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
//         connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
//         ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
//         coinControlUpdateLabels();

        // fee section
        // connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateSmartFeeLabel()));
        // connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        // connect(ui->sliderSmartFee, SIGNAL(valueChanged(int)), this, SLOT(coinControlUpdateLabels()));
        // connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateFeeSectionControls()));
        // connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        // connect(ui->groupFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));
        // connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(updateGlobalFeeVariables()));
        // connect(ui->groupCustomFee, SIGNAL(buttonClicked(int)), this, SLOT(coinControlUpdateLabels()));
        // connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(updateGlobalFeeVariables()));
        // connect(ui->customFee, SIGNAL(valueChanged()), this, SLOT(coinControlUpdateLabels()));
        // connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(setMinimumFee()));
        // connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateFeeSectionControls()));
        // connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        // connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this, SLOT(coinControlUpdateLabels()));
        // connect(ui->checkBoxFreeTx, SIGNAL(stateChanged(int)), this, SLOT(updateGlobalFeeVariables()));
        // connect(ui->checkBoxFreeTx, SIGNAL(stateChanged(int)), this, SLOT(coinControlUpdateLabels()));
        // ui->customFee->setSingleStep(CWallet::minTxFee.GetFeePerK());
         updateFeeSectionControls();
         updateMinFeeLabel();
         updateSmartFeeLabel();
         updateGlobalFeeVariables();
    }
}

PlaceBetDialog::~PlaceBetDialog()
{
    // QSettings settings;
    // settings.setValue("fFeeSectionMinimized", fFeeMinimized);
    // settings.setValue("nFeeRadio", ui->groupFee->checkedId());
    // settings.setValue("nCustomFeeRadio", ui->groupCustomFee->checkedId());
    // settings.setValue("nSmartFeeSliderPosition", ui->sliderSmartFee->value());
    // settings.setValue("nTransactionFee", (qint64)ui->customFee->value());
    // settings.setValue("fPayOnlyMinFee", ui->checkBoxMinimumFee->isChecked());
    // settings.setValue("fSendFreeTransactions", ui->checkBoxFreeTx->isChecked());

    delete ui;
}

void PlaceBetDialog::on_placeBetButton_clicked()
{
    if (!model || !model->getOptionsModel())
        return;

    if (!betEvent) {
        // process prepareStatus and on error generate message shown to user
        processPlaceBetReturn(WalletModel::NoBetSelected,
            BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), 0), true);
        return;
    }

    if (!ui->payAmount->validate() || ui->payAmount->value(0) < (Params().MinBetPayoutRange() * COIN) || ui->payAmount->value(0) > ( Params().MaxBetPayoutRange() * COIN ) ){
        ui->payAmount->setValid(false);

        QString questionString = tr("Bet amount must be between 50 - 10,000 WGR inclusive!");

        QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
        // Default to a warning message, override if error message is needed
        msgParams.second = CClientUIInterface::MSG_WARNING;

        msgParams.first = tr("Bet amount must be between 50 - 10,000 WGR inclusive!");
        emit message(tr("Invalid Bet Amount!"), msgParams.first, msgParams.second);
        return;
    }


    // //set split block in model
    // // CoinControlDialog::coinControl->fSplitBlock = ui->splitBlockCheckBox->checkState() == Qt::Checked;

    // // if (ui->events->count() > 1 && ui->splitBlockCheckBox->checkState() == Qt::Checked) {
    // //     CoinControlDialog::coinControl->fSplitBlock = false;
    // //     ui->splitBlockCheckBox->setCheckState(Qt::Unchecked);
    // //     QMessageBox::warning(this, tr("Send Coins"),
    // //         tr("The split block tool does not work with multiple addresses. Try again."),
    // //         QMessageBox::Ok, QMessageBox::Ok);
    // //     return;
    // // }

    // if (CoinControlDialog::coinControl->fSplitBlock)
    //     // CoinControlDialog::coinControl->nSplitBlock = int(ui->splitBlockLineEdit->text().toInt());

    // QString strFunds = "";
    QString strFee = "";
    // recipients[0].inputType = ALL_COINS;

    // if (ui->checkSwiftTX->isChecked()) {
    //     recipients[0].useSwiftTX = true;
    //     strFunds += " ";
    //     strFunds += tr("using SwiftX");
    // } else {
    //     recipients[0].useSwiftTX = false;
    // }


    CAmount amount = ui->payAmount->value();
    std::string eventId = betEvent->id;
    std::string team = betTeamToWin;
    std::string eventTime = betEvent->starting;

    // request unlock only if was locked or unlocked for mixing:
    // this way we let users unlock by walletpassphrase or by menu
    // and make many transactions while unlocking through this dialog
    // will call relock
    WalletModel::EncryptionStatus encStatus = model->getEncryptionStatus();
    if (encStatus == model->Locked || encStatus == model->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(model->requestUnlock(AskPassphraseDialog::Context::Send_WGR, true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            return;
        }
        send(amount, eventId, team, eventTime);
        return;
    }
    // already unlocked or not encrypted at all
    send(amount, eventId, team, eventTime);
}

void PlaceBetDialog::send(CAmount amount, const std::string& eventId, const std::string& teamToWin, const std::string& eventTime)
{
    QList<SendCoinsRecipient> recipients;
    WalletModelTransaction currentTransaction(recipients);

    WalletModel::SendCoinsReturn prepareStatus;
    if (!betEvent) {
        // TODO This check is made redundant by the check made in
        // `on_placeBetButton_clicked`.
        prepareStatus = WalletModel::NoBetSelected;
    } else {
        prepareStatus = model->prepareBetTransaction(currentTransaction, amount, eventId, teamToWin);
    }

    // process prepareStatus and on error generate message shown to user
    processPlaceBetReturn(prepareStatus,
        BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()), true);

    if (prepareStatus.status != WalletModel::OK) {
        return;
    }

    CAmount txFee = currentTransaction.getTransactionFee();
    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><br />%1");

    if (txFee > 0) {
        // append fee string if a fee is required
        questionString.append("<hr /><span style='color:#aa0000;'>");
        questionString.append(BitcoinUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span> ");
        questionString.append(tr("are added as transaction fee"));
        questionString.append(" ");

        // Append transaction size.
        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB)");
    }

    // Display message box.
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm place bet"),
        questionString.arg(""),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        return;
    }

    // Check if the event is further than 22 minutes into the future.
    time_t currentTime = std::time(0);
    if((currentTime + 1320) > std::stoi(eventTime)) {
        QString questionString1 = tr("Betting expired! Please ensure you bet more than 20 minutes before the event start time!");
        questionString1.append("<br /><br />%1");

        QMessageBox::question(this, tr("Cannot Bet!"),
        questionString1.arg(""),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

        return;
    }

    // Now send the prepared transaction.
    WalletModel::SendCoinsReturn sendStatus = model->placeBet(currentTransaction, amount, eventId, teamToWin);
    processPlaceBetReturn(sendStatus);

    ui->payAmount->clear();

    //need to fix the
    // if (sendStatus.status == WalletModel::OK) {
    //     printf("I accept your wagerr good sir!! \n");
    //     accept();
    // }else{
    //     printf("I DO NOT ACCEPT your wagerr good sir!! \n");
    // }
}

void PlaceBetDialog::clear()
{
    while (ui->events->count()) {
        ui->events->takeAt(0)->widget()->deleteLater();
    }

    updateTabsAndLabels();
}

void PlaceBetDialog::reject()
{
    clear();
}

void PlaceBetDialog::accept()
{
    clear();
}

void PlaceBetDialog::prepareBet(CEvent* event, const std::string& teamToWin, const std::string& oddsToWin)
{
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
    countryNames.insert(make_pair("COL", "Colombia"));
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

    std::string evtDes = "";

    std::map<std::string, std::string>::iterator it;
    it = countryNames.find(teamToWin);
    std::string team = teamToWin;
    if (teamToWin == "DRW") {
        evtDes += "Draw ";
    } else if (it == countryNames.end()) {
        evtDes += teamToWin;
        evtDes += " to win ";
    } else {
        evtDes = it->second;
        evtDes += " to win ";
    }
    evtDes += oddsToWin;
    evtDes += "\n";

    it = countryNames.find(event->homeTeam);
    team = event->homeTeam;
    if (it != countryNames.end()) {
        team = it->second;
    }
    evtDes += team;
    evtDes += " V ";
    it = countryNames.find(event->awayTeam);
    team = event->awayTeam;
    if (it != countryNames.end()) {
        team = it->second;
    }
    evtDes += team;
    evtDes += "   ";

    // evtDes += evtDescr;
    // evtDes += " ";
    time_t time = (time_t) std::strtol(event->starting.c_str(), nullptr, 10);
    tm *ptm = std::gmtime(&time);
    static const char mon_name[][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    char result[22];
    // evtDes += evtDescr;
    sprintf(
        result,
        "%.2d:%.2d UTC+0   %.2d %3s",
        ptm->tm_hour,
        ptm->tm_min,
        ptm->tm_mday,
        mon_name[ptm->tm_mon]
    );
    evtDes += result;
    // printf(">>> %s\n", evtDescr.c_str());

    // set local member - confirm in text - event + odds
    betEvent = event;
    betTeamToWin = teamToWin;
    betOddsToWin = oddsToWin;

    LogPrintf("PlaceBetDialog::prepareBet: about to print\n");
    LogPrintf("PlaceBetDialog::prepareBet: %s %s %s %s", event->id.c_str(), event->homeTeam.c_str(), betTeamToWin.c_str(), betOddsToWin.c_str());

    ui->eventLabel->setText(QString::fromStdString(evtDes));
}

PlaceBetEvent* PlaceBetDialog::addEvent(
    CEvent* evt,
    const std::string& eventDetails,
    const  std::string& homeOdds,
    const std::string& awayOdds,
    const std::string& drawOdds
)
{
    LogPrintf("PlaceBetDialog::addEvent: about to print\n");
    LogPrintf("PlaceBetDialog::addEvent: %s %s\n", eventDetails.c_str(), homeOdds.c_str());

    PlaceBetEvent* event = new PlaceBetEvent(this, evt, eventDetails, homeOdds, awayOdds, drawOdds);
    event->setModel(model);
    ui->events->addWidget(event);
    // connect(event, SIGNAL(removeEvent(PlaceBetEvent*)), this, SLOT(removeEvent(PlaceBetEvent*)));
    // connect(event, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    // passes event and string
    connect(event, SIGNAL(currentEventChanged(CEvent*, const std::string&, const std::string&)),
        this, SLOT(prepareBet(CEvent*, const std::string&, const std::string&)));

    updateTabsAndLabels();

    // Focus the field, so that event can start immediately
    event->clear();
    event->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    qApp->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if (bar)
        bar->setSliderPosition(bar->maximum());
    return event;
}

void PlaceBetDialog::updateTabsAndLabels()
{
    setupTabChain(0);
    coinControlUpdateLabels();
}

void PlaceBetDialog::removeEvent(PlaceBetEvent* event)
{
//     event->hide();
//
//     // If the last event is about to be removed add an empty one
//     if (ui->events->count() == 1)
//         addEntry();
//
//     event->deleteLater();
//
//     updateTabsAndLabels();
}

QWidget* PlaceBetDialog::setupTabChain(QWidget* prev)
{
//     for (int i = 0; i < ui->events->count(); ++i) {
//         PlaceBetEvent* event = qobject_cast<PlaceBetEvent*>(ui->events->itemAt(i)->widget());
//         if (event) {
//             prev = event->setupTabChain(prev);
//         }
//     }
//     QWidget::setTabOrder(prev, ui->placeBetButton);
//     QWidget::setTabOrder(ui->placeBetButton, ui->clearButton);
//     QWidget::setTabOrder(ui->clearButton, ui->addButton);
//     return ui->addButton;
    return ui->placeBetButton;
}

void PlaceBetDialog::setAddress(const QString& address)
{
    // ui->labelCoinControlAutomaticallySelected->setText(address);
    // PlaceBetEvent* event = 0;
    // // Replace the first event if it is still unused
    // if (ui->events->count() == 1) {
    //     PlaceBetEvent* first = qobject_cast<PlaceBetEvent*>(ui->events->itemAt(0)->widget());
    //     if (first->isClear()) {
    //         event = first;
    //     }
    // }
    // if (!event) {
    //     event = addEntry();
    // }

    // event->setAddress(address);
}

void PlaceBetDialog::pasteEntry(const SendCoinsRecipient& rv)
{
//     if (!fNewRecipientAllowed)
//         return;
//
//     PlaceBetEvent* event = 0;
//     // Replace the first event if it is still unused
//     if (ui->events->count() == 1) {
//         PlaceBetEvent* first = qobject_cast<PlaceBetEvent*>(ui->events->itemAt(0)->widget());
//         if (first->isClear()) {
//             event = first;
//         }
//     }
//     if (!event) {
//         event = addEntry();
//     }
//
//     event->setValue(rv);
//     updateTabsAndLabels();
}

bool PlaceBetDialog::handlePaymentRequest(const SendCoinsRecipient& rv)
{
    // Just paste the event, all pre-checks
    // are done in paymentserver.cpp.
    pasteEntry(rv);
    return true;
}

void PlaceBetDialog::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                                 const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance,
                                 const CAmount& watchBalance, const CAmount& watchUnconfirmedBalance, const CAmount& watchImmatureBalance)
{
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    Q_UNUSED(zerocoinBalance);
    Q_UNUSED(unconfirmedZerocoinBalance);
    Q_UNUSED(immatureZerocoinBalance);
    Q_UNUSED(watchBalance);
    Q_UNUSED(watchUnconfirmedBalance);
    Q_UNUSED(watchImmatureBalance);

    if (model && model->getOptionsModel()) {
        uint64_t bal = 0;
        bal = balance;
        ui->labelBalance->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), bal));
    }
}

void PlaceBetDialog::updateDisplayUnit()
{
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) return;

    setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(),
               model->getZerocoinBalance (), model->getUnconfirmedZerocoinBalance (), model->getImmatureZerocoinBalance (),
               model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
    coinControlUpdateLabels();
    // ui->customFee->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    updateMinFeeLabel();
    updateSmartFeeLabel();
}

void PlaceBetDialog::updateSwiftTX()
{
    QSettings settings;
    // settings.setValue("bUseSwiftTX", ui->checkSwiftTX->isChecked());
    // CoinControlDialog::coinControl->useSwiftTX = ui->checkSwiftTX->isChecked();
    coinControlUpdateLabels();
}

void PlaceBetDialog::processPlaceBetReturn(const WalletModel::SendCoinsReturn& sendCoinsReturn, const QString& msgArg, bool fPrepare)
{
    bool fAskForUnlock = false;

    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
    // Default to a warning message, override if error message is needed
    msgParams.second = CClientUIInterface::MSG_WARNING;

    // This comment is specific to PlaceBetDialog usage of WalletModel::SendCoinsReturn.
    // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
    // all others are used only in WalletModel::prepareTransaction()
    switch (sendCoinsReturn.status) {
    case WalletModel::InvalidAddress:
        msgParams.first = tr("The recipient address is not valid, please recheck.");
        break;
    case WalletModel::InvalidAmount:
        msgParams.first = tr("The amount to pay must be larger than 0.");
        break;
    case WalletModel::AmountExceedsBalance:
        msgParams.first = tr("The amount exceeds your balance.");
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
        break;
    case WalletModel::DuplicateAddress:
        msgParams.first = tr("Duplicate address found, can only send to each address once per send operation.");
        break;
    case WalletModel::TransactionCreationFailed:
        msgParams.first = tr("Transaction creation failed!");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::TransactionCommitFailed:
        msgParams.first = tr("The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::AnonymizeOnlyUnlocked:
        // Unlock is only need when the coins are send
        if(!fPrepare)
            fAskForUnlock = true;
        else
            msgParams.first = tr("Error: The wallet was unlocked only to anonymize coins.");
        break;

    case WalletModel::InsaneFee:
        msgParams.first = tr("A fee %1 times higher than %2 per kB is considered an insanely high fee.").arg(10000).arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), ::minRelayTxFee.GetFeePerK()));
        break;
    case WalletModel::NoBetSelected:
        msgParams.first = tr("Please select an event outcome to bet on.");
        break;
    // included to prevent a compiler warning.
    case WalletModel::OK:
    default:
        return;
    }

    // Unlock wallet if it wasn't fully unlocked already
    if(fAskForUnlock) {
        model->requestUnlock(AskPassphraseDialog::Context::Unlock_Full, false);
        if(model->getEncryptionStatus () != WalletModel::Unlocked) {
            msgParams.first = tr("Error: The wallet was unlocked only to anonymize coins. Unlock canceled.");
        }
        else {
            // Wallet unlocked
            return;
        }
    }

    emit message(tr("Place Bet"), msgParams.first, msgParams.second);
}

void PlaceBetDialog::minimizeFeeSection(bool fMinimize)
{
    // ui->labelFeeMinimized->setVisible(fMinimize);
    // ui->buttonChooseFee->setVisible(fMinimize);
    // ui->buttonMinimizeFee->setVisible(!fMinimize);
    // ui->frameFeeSelection->setVisible(!fMinimize);
    // ui->horizontalLayoutSmartFee->setContentsMargins(0, (fMinimize ? 0 : 6), 0, 0);
    // fFeeMinimized = fMinimize;
}

void PlaceBetDialog::on_buttonChooseFee_clicked()
{
    // minimizeFeeSection(false);
}

void PlaceBetDialog::on_buttonMinimizeFee_clicked()
{
    // updateFeeMinimizedLabel();
    // minimizeFeeSection(true);
}

void PlaceBetDialog::setMinimumFee()
{
    // ui->radioCustomPerKilobyte->setChecked(true);
    // ui->customFee->setValue(CWallet::minTxFee.GetFeePerK());
}

void PlaceBetDialog::updateFeeSectionControls()
{
    // ui->sliderSmartFee->setEnabled(ui->radioSmartFee->isChecked());
    // ui->labelSmartFee->setEnabled(ui->radioSmartFee->isChecked());
    // ui->labelSmartFee2->setEnabled(ui->radioSmartFee->isChecked());
    // ui->labelSmartFee3->setEnabled(ui->radioSmartFee->isChecked());
    // ui->labelFeeEstimation->setEnabled(ui->radioSmartFee->isChecked());
    // ui->labelSmartFeeNormal->setEnabled(ui->radioSmartFee->isChecked());
    // ui->labelSmartFeeFast->setEnabled(ui->radioSmartFee->isChecked());
    // ui->checkBoxMinimumFee->setEnabled(ui->radioCustomFee->isChecked());
    // ui->labelMinFeeWarning->setEnabled(ui->radioCustomFee->isChecked());
    // ui->radioCustomPerKilobyte->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    // ui->radioCustomAtLeast->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
    // ui->customFee->setEnabled(ui->radioCustomFee->isChecked() && !ui->checkBoxMinimumFee->isChecked());
}

void PlaceBetDialog::updateGlobalFeeVariables()
{
    // if (ui->radioSmartFee->isChecked()) {
    //     nTxConfirmTarget = (int)25 - (int)std::max(0, std::min(24, ui->sliderSmartFee->value()));
    //     payTxFee = CFeeRate(0);
    // } else {
    //     nTxConfirmTarget = 25;
    //     payTxFee = CFeeRate(ui->customFee->value());
    //     fPayAtLeastCustomFee = ui->radioCustomAtLeast->isChecked();
    // }

    // fSendFreeTransactions = ui->checkBoxFreeTx->isChecked();
}

void PlaceBetDialog::updateFeeMinimizedLabel()
{
    // if (!model || !model->getOptionsModel())
    //     return;

    // if (ui->radioSmartFee->isChecked())
    //     ui->labelFeeMinimized->setText(ui->labelSmartFee->text());
    // else {
    //     ui->labelFeeMinimized->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), ui->customFee->value()) +
    //                                    ((ui->radioCustomPerKilobyte->isChecked()) ? "/kB" : ""));
    // }
}

void PlaceBetDialog::updateMinFeeLabel()
{
    // if (model && model->getOptionsModel())
    //     ui->checkBoxMinimumFee->setText(tr("Pay only the minimum fee of %1").arg(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), CWallet::minTxFee.GetFeePerK()) + "/kB"));
}

void PlaceBetDialog::updateSmartFeeLabel()
{
    // if (!model || !model->getOptionsModel())
    //     return;

    // int nBlocksToConfirm = (int)25 - (int)std::max(0, std::min(24, ui->sliderSmartFee->value()));
    // CFeeRate feeRate = mempool.estimateFee(nBlocksToConfirm);
    // if (feeRate <= CFeeRate(0)) // not enough data => minfee
    // {
    //     ui->labelSmartFee->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), CWallet::minTxFee.GetFeePerK()) + "/kB");
    //     ui->labelSmartFee2->show(); // (Smart fee not initialized yet. This usually takes a few blocks...)
    //     ui->labelFeeEstimation->setText("");
    // } else {
    //     ui->labelSmartFee->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), feeRate.GetFeePerK()) + "/kB");
    //     ui->labelSmartFee2->hide();
    //     ui->labelFeeEstimation->setText(tr("Estimated to begin confirmation within %n block(s).", "", nBlocksToConfirm));
    // }

    // updateFeeMinimizedLabel();
}

// UTXO splitter
void PlaceBetDialog::splitBlockChecked(int state)
{
    // if (model) {
    //     CoinControlDialog::coinControl->fSplitBlock = (state == Qt::Checked);
    //     fSplitBlock = (state == Qt::Checked);
    //     // ui->splitBlockLineEdit->setEnabled((state == Qt::Checked));
    //     // ui->labelBlockSizeText->setEnabled((state == Qt::Checked));
    //     // ui->labelBlockSize->setEnabled((state == Qt::Checked));
    //     coinControlUpdateLabels();
    // }
}

//UTXO splitter
void PlaceBetDialog::splitBlockLineEditChanged(const QString& text)
{
    // //grab the amount in Coin Control AFter Fee field
    // QString qAfterFee = ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace("~", "").simplified().replace(" ", "");

    // //convert to CAmount
    // CAmount nAfterFee;
    // ParseMoney(qAfterFee.toStdString().c_str(), nAfterFee);

    // //if greater than 0 then divide after fee by the amount of blocks
    // CAmount nSize = nAfterFee;
    // int nBlocks = text.toInt();
    // if (nAfterFee && nBlocks)
    //     nSize = nAfterFee / nBlocks;

    // //assign to split block dummy, which is used to recalculate the fee amount more outputs
    // CoinControlDialog::nSplitBlockDummy = nBlocks;

    // //update labels
    // ui->labelBlockSize->setText(QString::fromStdString(FormatMoney(nSize)));
    // coinControlUpdateLabels();
}

// Coin Control: copy label "Quantity" to clipboard
void PlaceBetDialog::coinControlClipboardQuantity()
{
    // GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

// Coin Control: copy label "Amount" to clipboard
void PlaceBetDialog::coinControlClipboardAmount()
{
    // GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// Coin Control: copy label "Fee" to clipboard
void PlaceBetDialog::coinControlClipboardFee()
{
    // GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace("~", ""));
}

// Coin Control: copy label "After fee" to clipboard
void PlaceBetDialog::coinControlClipboardAfterFee()
{
    // GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace("~", ""));
}

// Coin Control: copy label "Bytes" to clipboard
void PlaceBetDialog::coinControlClipboardBytes()
{
    // GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace("~", ""));
}

// Coin Control: copy label "Priority" to clipboard
void PlaceBetDialog::coinControlClipboardPriority()
{
    // GUIUtil::setClipboard(ui->labelCoinControlPriority->text());
}

// Coin Control: copy label "Dust" to clipboard
void PlaceBetDialog::coinControlClipboardLowOutput()
{
    // GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

// Coin Control: copy label "Change" to clipboard
void PlaceBetDialog::coinControlClipboardChange()
{
    // GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace("~", ""));
}

// Coin Control: settings menu - coin control enabled/disabled by user
void PlaceBetDialog::coinControlFeatureChanged(bool checked)
{
    ui->frameCoinControl->setVisible(checked);

    if (!checked && model) // coin control features disabled
        CoinControlDialog::coinControl->SetNull();

    if (checked)
        coinControlUpdateLabels();
}

// Coin Control: button inputs -> show actual coin control dialog
void PlaceBetDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg;
    dlg.setModel(model);
    dlg.exec();
    coinControlUpdateLabels();
}

// Coin Control: checkbox custom change address
void PlaceBetDialog::coinControlChangeChecked(int state)
{
    // if (state == Qt::Unchecked) {
    //     CoinControlDialog::coinControl->destChange = CNoDestination();
    //     ui->labelCoinControlChangeLabel->clear();
    // } else
    //     use this to re-validate an already entered address
    //     coinControlChangeEdited(ui->lineEditCoinControlChange->text());

    // ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
}

// Coin Control: custom change address changed
void PlaceBetDialog::coinControlChangeEdited(const QString& text)
{
    // if (model && model->getAddressTableModel()) {
    //     // Default to no change address until verified
    //     CoinControlDialog::coinControl->destChange = CNoDestination();
    //     // ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");

    //     CBitcoinAddress addr = CBitcoinAddress(text.toStdString());

    //     if (text.isEmpty()) // Nothing entered
    //     {
    //         // ui->labelCoinControlChangeLabel->setText("");
    //     } else if (!addr.IsValid()) // Invalid address
    //     {
    //         // ui->labelCoinControlChangeLabel->setText(tr("Warning: Invalid Wagerr address"));
    //     } else // Valid address
    //     {
    //         CPubKey pubkey;
    //         CKeyID keyid;
    //         addr.GetKeyID(keyid);
    //         if (!model->getPubKey(keyid, pubkey)) // Unknown change address
    //         {
    //             // ui->labelCoinControlChangeLabel->setText(tr("Warning: Unknown change address"));
    //         } else // Known change address
    //         {
    //             // ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");

    //             // Query label
    //             QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
    //             // if (!associatedLabel.isEmpty())
    //             //     ui->labelCoinControlChangeLabel->setText(associatedLabel);
    //             // else
    //             //     ui->labelCoinControlChangeLabel->setText(tr("(no label)"));

    //             CoinControlDialog::coinControl->destChange = addr.Get();
    //         }
    //     }
    // }
}

// Coin Control: update labels
void PlaceBetDialog::coinControlUpdateLabels()
{
//     if (!model || !model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
//         return;
//
//     // set pay amounts
//     CoinControlDialog::payAmounts.clear();
//     for (int i = 0; i < ui->events->count(); ++i) {
//         PlaceBetEvent* event = qobject_cast<PlaceBetEvent*>(ui->events->itemAt(i)->widget());
//         if (event)
//             CoinControlDialog::payAmounts.append(event->getValue().amount);
//     }
//
//     if (CoinControlDialog::coinControl->HasSelected()) {
//         // actual coin control calculation
//         CoinControlDialog::updateLabels(model, this);
//
//         // show coin control stats
//         // ui->labelCoinControlAutomaticallySelected->hide();
//         // ui->widgetCoinControl->show();
//     } else {
//         // hide coin control stats
//         // ui->labelCoinControlAutomaticallySelected->show();
//         // ui->widgetCoinControl->hide();
//     }
}
