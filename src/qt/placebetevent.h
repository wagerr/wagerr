// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PLACEBETEVENT_H
#define BITCOIN_QT_PLACEBETEVENT_H

#include "walletmodel.h"

#include <QStackedWidget>

class CEvent
{
public:
    // TODO We would ideally make all member variables `private` and access
    // them using getters and setters, but we make them public for now for speed
    // of implementation.
    std::string id;
    std::string name;
    std::string round;
    std::string starting;
    std::string homeTeam;
    std::string homeOdds;
    std::string awayTeam;
    std::string awayOdds;
    std::string drawOdds;

    CEvent(
        const std::string& id,
        const std::string& name,
        const std::string& round,
        const std::string& starting,
        const std::string& homeTeam,
        const std::string& homeOdds,
        const std::string& awayTeam,
        const std::string& awayOdds,
        const std::string& drawOdds
    );

    static CEvent* ParseEvent(const std::string& descr);
};

class WalletModel;

namespace Ui
{
class PlaceBetEvent;
}

/**
 * A single entry in the dialog for sending bitcoins.
 * Stacked widget, with different UIs for payment requests
 * with a strong payee identity.
 */
class PlaceBetEvent : public QStackedWidget
{
    Q_OBJECT

public:
    explicit PlaceBetEvent(QWidget* parent = 0, CEvent* event = nullptr, const std::string& eventDetails = "", const  std::string& homeOdds = "", const std::string& awayOdds = "", const std::string& drawOdds = "");
    ~PlaceBetEvent();

    void setModel(WalletModel* model);
    bool validate();
    SendCoinsRecipient getValue();

    /** Return whether the entry is still empty and unedited */
    bool isClear();

    void setValue(const SendCoinsRecipient& value);
    void setAddress(const QString& address);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases
     *  (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget* setupTabChain(QWidget* prev);

    void setFocus();

public slots:
    void clear();

signals:
    void currentEventChanged(CEvent* event, const std::string& teamToWin, const std::string& oddsToWin);
    void removeEntry(PlaceBetEvent* entry);
    void payAmountChanged();

private slots:
    void deleteClicked();
    void on_payTo_textChanged(const QString& address);
    void on_pushButtonPlaceHomeBet_clicked();
    void on_pushButtonPlaceAwayBet_clicked();
    void on_pushButtonPlaceDrawBet_clicked();
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
    void updateDisplayUnit();

private:
    SendCoinsRecipient recipient;
    Ui::PlaceBetEvent* ui;
    WalletModel* model;
    CEvent* event;

    bool updateLabel(const QString& address);
};

#endif // BITCOIN_QT_PLACEBETEVENT_H
