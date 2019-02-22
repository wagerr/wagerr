// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "placebetevent.h"
#include "ui_placebetevent.h"

#include "placebetdialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QApplication>
#include <QClipboard>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>

CEvent::CEvent(
    const std::string& id,
    const std::string& name,
    const std::string& round,
    const std::string& starting,
    const std::string& homeTeam,
    const std::string& homeOdds,
    const std::string& awayTeam,
    const std::string& awayOdds,
    const std::string& drawOdds
) : id(id),
    name(name),
    round(round),
    starting(starting),
    homeTeam(homeTeam),
    homeOdds(homeOdds),
    awayTeam(awayTeam),
    awayOdds(awayOdds),
    drawOdds(drawOdds)
{
}

// Returns null on failure
CEvent* CEvent::ParseEvent(const std::string& descr)
{
//     std::map<std::string, std::string> eventNames;
//     eventNames.insert(make_pair("WCUP", "World Cup"));
//     std::map<std::string, std::string> countryNames;
//     countryNames.insert(make_pair("RUS", "Russia"));
//     countryNames.insert(make_pair("KSA", "Kazakhstan"));
//
    std::vector<std::string> fields;
    boost::split(fields, descr, boost::is_any_of("|"));

    if (fields.size() != 11) {
        return nullptr;
    }

//     evtDes = "";
//
//     // TODO Handle version field.
//
//     // UniValue evt(UniValue::VOBJ);
//
//     // evt.push_back(Pair("id", strs[2]));
//     // evt.push_back(Pair("name", strs[4]));
//     // evt.push_back(Pair("round", strs[5]));
//     // evt.push_back(Pair("starting", strs[3]));
//
//     // // evtDes += evtDescr;
//     // // evtDes += " ";
//     // time_t time = (time_t) std::strtol(strs[2].c_str(), nullptr, 10);
//     // tm *ptm = std::gmtime(&time);
//     // static const char mon_name[][4] = {
//     //     "Jan", "Feb", "Mar", "Apr", "May", "Jun",
//     //     "Jul", "Aug", "Sep", "Aug", "Nov", "Dec"
//     // };
//     // char result[22];
//     // // evtDes += evtDescr;
//     // sprintf(
//     //     result,
//     //     "%.2d:%.2d UTC+0   %.2d %3s",
//     //     ptm->tm_hour,
//     //     ptm->tm_min,
//     //     ptm->tm_mday,
//     //     mon_name[ptm->tm_mon]
//     // );
//     // evtDes += result;
//
//     // printf(">>> %s\n", evtDescr.c_str());
//
//     // evtDes += "   ";
//     // std::map<std::string, std::string>::iterator it;
//     // it = countryNames.find(strs[5]);
//     // if (it == countryNames.end()) {
//     //     continue;
//     // }
//     // evtDes += it->second;
//     // evtDes += " V ";
//     // it = countryNames.find(strs[6]);
//     // if (it == countryNames.end()) {
//     //     continue;
//     // }
//     // evtDes += it->second;
//
//     // // UniValue teams(UniValue::VARR);
//     // // for (unsigned int t = 6; t <= 7; t++) {
//     // //     UniValue team(UniValue::VOBJ);
//     // //     team.push_back(Pair("name", strs[t]));
//     // //     team.push_back(Pair("odds", strs[t+2]));
//     // //     teams.push_back(team);
//     // // }
//     // // evt.push_back(Pair("teams", teams));
//
//     // // ret.push_back(evt);
//
//     // printf(">!> %s\n", evtDes.c_str());
//

    std::string winOddsString;
    std::string loseOddsString;
    std::string drawOddsString;

    // If home odds are set to zero show 'N/A' on the home odds btn. Otherwise show to home odds.
    if( std::stod(fields[8]) > 0 ){
        winOddsString = std::to_string(std::stod(fields[8]) / Params().OddsDivisor());
        winOddsString = winOddsString.substr(0, winOddsString.size() -4);
    }
    else{
        winOddsString = "N/A";
    }

    // If away odds are set to zero show 'N/A' on the away odds btn. Otherwise show to away odds.
    if( std::stod(fields[9]) > 0 ){
        loseOddsString = std::to_string(std::stod(fields[9]) / Params().OddsDivisor());
        loseOddsString = loseOddsString.substr(0, loseOddsString.size() -4);
    }
    else{
        loseOddsString = "N/A";
    }

    // If draw odds are set to zero show 'N/A' on the draw odds btn. Otherwise show to draw odds/
    if( std::stod(fields[10]) > 0 ){
        drawOddsString = std::to_string(std::stod(fields[10]) / Params().OddsDivisor());
        drawOddsString =  drawOddsString.substr(0, drawOddsString.size() -4);
    }
    else{
        drawOddsString = "N/A";
    }

    return new CEvent(
            fields[2],
            fields[4],
            fields[5],
            fields[3],
            fields[6],
            winOddsString,
            fields[7],
            loseOddsString,
            drawOddsString
    );
}

PlaceBetEvent::PlaceBetEvent(
QWidget* parent, CEvent* event, const std::string& eventDetails, const  std::string& homeOdds, const std::string& awayOdds, const std::string& drawOdds) : QStackedWidget(parent),
                                                  ui(new Ui::PlaceBetEvent),
                                                  model(0),
                                                  event(event)
{
    //LogPrintf("PlaceBetEvent::PlaceBetEvent: about to print\n");
    //LogPrintf("PlaceBetEvent::PlaceBetEvent: %s %s\n", event->id.c_str(), event->homeTeam.c_str());
    ui->setupUi(this);

    setCurrentWidget(ui->SendCoins);

    ui->labelCoinControlAutomaticallySelected->setText(QString::fromStdString(eventDetails));
    ui->pushButtonPlaceHomeBet->setText(QString::fromStdString(event->homeOdds));
    ui->pushButtonPlaceAwayBet->setText(QString::fromStdString(event->awayOdds));
    ui->pushButtonPlaceDrawBet->setText(QString::fromStdString(event->drawOdds));
    ui->pushButtonPlaceHomeBet->setStyleSheet(QStringLiteral("QPushButton:disabled{background-color:#c0c0c0}"));
    ui->pushButtonPlaceAwayBet->setStyleSheet(QStringLiteral("QPushButton:disabled{background-color:#c0c0c0}"));
    ui->pushButtonPlaceDrawBet->setStyleSheet(QStringLiteral("QPushButton:disabled{background-color:#c0c0c0}"));
    if(event->homeOdds == "N/A") ui->pushButtonPlaceHomeBet->setEnabled(false);
    if(event->awayOdds == "N/A") ui->pushButtonPlaceAwayBet->setEnabled(false);
    if(event->drawOdds == "N/A") ui->pushButtonPlaceDrawBet->setEnabled(false);

// #ifdef Q_OS_MAC
//     ui->payToLayout->setSpacing(4);
// #endif
// #if QT_VERSION >= 0x040700
//     ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
// #endif
//
//     // normal wagerr address field
//     GUIUtil::setupAddressWidget(ui->payTo, this);
//     // just a label for displaying wagerr address(es)
//     ui->payTo_is->setFont(GUIUtil::bitcoinAddressFont());
//
//     // Connect signals
//     connect(ui->payAmount, SIGNAL(valueChanged()), this, SIGNAL(payAmountChanged()));
//     connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
//     connect(ui->deleteButton_is, SIGNAL(clicked()), this, SLOT(deleteClicked()));
//     connect(ui->deleteButton_s, SIGNAL(clicked()), this, SLOT(deleteClicked()));
}

PlaceBetEvent::~PlaceBetEvent()
{
    delete ui;
}

void PlaceBetEvent::on_pasteButton_clicked()
{
    // // Paste text from clipboard into recipient field
    // ui->payTo->setText(QApplication::clipboard()->text());
}

void PlaceBetEvent::on_pushButtonPlaceHomeBet_clicked()
{
//printf("PlaceBetEvent::on_pushButtonPlaceHomeBet_clicked: about to print\n");
//printf("PlaceBetEvent::on_pushButtonPlaceHomeBet_clicked: %s %s\n", event->id.c_str(), event->homeTeam.c_str());
    emit currentEventChanged(event, event->homeTeam, event->homeOdds);
}

void PlaceBetEvent::on_pushButtonPlaceAwayBet_clicked()
{
//printf("PlaceBetEvent::on_pushButtonPlaceAwayBet_clicked: about to print\n");
//printf("PlaceBetEvent::on_pushButtonPlaceAwayBet_clicked: %s %s\n", event->id.c_str(), event->awayTeam.c_str());
    emit currentEventChanged(event, event->awayTeam, event->awayOdds);
}

void PlaceBetEvent::on_pushButtonPlaceDrawBet_clicked()
{
//printf("PlaceBetEvent::on_pushButtonPlaceDrawBet_clicked: about to print\n");
//printf("PlaceBetEvent::on_pushButtonPlaceDrawBet_clicked: %s %s\n", event->id.c_str(), event->drawOdds.c_str());
    emit currentEventChanged(event, "DRW", event->drawOdds);
}

void PlaceBetEvent::on_addressBookButton_clicked()
{
    if (!model)
        return;
    AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if (dlg.exec()) {
        // ui->payTo->setText(dlg.getReturnValue());
        // ui->payAmount->setFocus();
    }
}

void PlaceBetEvent::on_payTo_textChanged(const QString& address)
{
    updateLabel(address);
}

void PlaceBetEvent::setModel(WalletModel* model)
{
    this->model = model;

    if (model && model->getOptionsModel())
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    clear();
}

void PlaceBetEvent::clear()
{
    // clear UI elements for normal payment
    // ui->payTo->clear();
    // ui->addAsLabel->clear();
    // ui->payAmount->clear();
    // ui->messageTextLabel->clear();
    // ui->messageTextLabel->hide();
    // ui->messageLabel->hide();
    // // clear UI elements for insecure payment request
    // ui->payTo_is->clear();
    // ui->memoTextLabel_is->clear();
    // ui->payAmount_is->clear();
    // // clear UI elements for secure payment request
    // ui->payTo_s->clear();
    // ui->memoTextLabel_s->clear();
    // ui->payAmount_s->clear();

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void PlaceBetEvent::deleteClicked()
{
    emit removeEntry(this);
}

bool PlaceBetEvent::validate()
{
    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    // if (!model->validateAddress(ui->payTo->text())) {
    //     ui->payTo->setValid(false);
    //     retval = false;
    // }

    // if (!ui->payAmount->validate()) {
    //     retval = false;
    // }

    // // Sending a zero amount is invalid
    // if (ui->payAmount->value(0) <= 0) {
    //     ui->payAmount->setValid(false);
    //     retval = false;
    // }

    // // Reject dust outputs:
    // if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
    //     ui->payAmount->setValid(false);
    //     retval = false;
    // }

    return retval;
}

SendCoinsRecipient PlaceBetEvent::getValue()
{
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    // // Normal payment
    // recipient.address = ui->payTo->text();
    // recipient.label = ui->addAsLabel->text();
    // recipient.amount = ui->payAmount->value();
    // recipient.message = ui->messageTextLabel->text();

    return recipient;
}

QWidget* PlaceBetEvent::setupTabChain(QWidget* prev)
{
    // QWidget::setTabOrder(prev, ui->payTo);
    // QWidget::setTabOrder(ui->payTo, ui->addAsLabel);
    // QWidget* w = ui->payAmount->setupTabChain(ui->addAsLabel);
    // QWidget::setTabOrder(w, ui->addressBookButton);
    // QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    // QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    // return ui->deleteButton;
    return nullptr;
}

void PlaceBetEvent::setValue(const SendCoinsRecipient& value)
{
    recipient = value;

    // if (recipient.paymentRequest.IsInitialized()) // payment request
    // {
    //     if (recipient.authenticatedMerchant.isEmpty()) // insecure
    //     {
    //         ui->payTo_is->setText(recipient.address);
    //         ui->memoTextLabel_is->setText(recipient.message);
    //         ui->payAmount_is->setValue(recipient.amount);
    //         ui->payAmount_is->setReadOnly(true);
    //         setCurrentWidget(ui->SendCoins_InsecurePaymentRequest);
    //     } else // secure
    //     {
    //         ui->payTo_s->setText(recipient.authenticatedMerchant);
    //         ui->memoTextLabel_s->setText(recipient.message);
    //         ui->payAmount_s->setValue(recipient.amount);
    //         ui->payAmount_s->setReadOnly(true);
    //         setCurrentWidget(ui->SendCoins_SecurePaymentRequest);
    //     }
    // } else // normal payment
    // {
    //     // message
    //     ui->messageTextLabel->setText(recipient.message);
    //     ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
    //     ui->messageLabel->setVisible(!recipient.message.isEmpty());

    //     ui->addAsLabel->clear();
    //     ui->payTo->setText(recipient.address); // this may set a label from addressbook
    //     if (!recipient.label.isEmpty())        // if a label had been set from the addressbook, dont overwrite with an empty label
    //         ui->addAsLabel->setText(recipient.label);
    //     ui->payAmount->setValue(recipient.amount);
    // }
}

void PlaceBetEvent::setAddress(const QString& address)
{
    // ui->payTo->setText(address);
    // ui->payAmount->setFocus();
}

bool PlaceBetEvent::isClear()
{
    // return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() && ui->payTo_s->text().isEmpty();
    return true;
}

void PlaceBetEvent::setFocus()
{
    // ui->payTo->setFocus();
}

void PlaceBetEvent::updateDisplayUnit()
{
    // if (model && model->getOptionsModel()) {
    //     // Update payAmount with the current unit
    //     ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    //     ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    //     ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    // }
}

bool PlaceBetEvent::updateLabel(const QString& address)
{
    // if (!model)
    //     return false;

    // // Fill in label from address book, if address has an associated label
    // QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    // if (!associatedLabel.isEmpty()) {
    //     ui->addAsLabel->setText(associatedLabel);
    //     return true;
    // }

    return false;
}
