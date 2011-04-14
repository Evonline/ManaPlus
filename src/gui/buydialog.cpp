/*
 *  The ManaPlus Client
 *  Copyright (C) 2004-2009  The Mana World Development Team
 *  Copyright (C) 2009-2010  The Mana Developers
 *  Copyright (C) 2011  The ManaPlus Developers
 *
 *  This file is part of The ManaPlus Client.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/buydialog.h"

#include "shopitem.h"
#include "units.h"

#include "gui/setup.h"
#include "gui/trade.h"

#include "gui/widgets/button.h"
#include "gui/widgets/label.h"
#include "gui/widgets/layout.h"
#include "gui/widgets/scrollarea.h"
#include "gui/widgets/shopitems.h"
#include "gui/widgets/shoplistbox.h"
#include "gui/widgets/slider.h"

#include "shopitem.h"
#include "units.h"

#include "net/buysellhandler.h"
#include "net/net.h"
#include "net/npchandler.h"

#include "resources/iteminfo.h"

#include "utils/gettext.h"
#include "utils/stringutils.h"

BuyDialog::DialogList BuyDialog::instances;

BuyDialog::BuyDialog(int npcId):
    Window(_("Buy")),
    mNpcId(npcId), mMoney(0), mAmountItems(0), mMaxItems(0), mNick("")
{
    init();
}

BuyDialog::BuyDialog(std::string nick):
    Window(_("Buy")),
    mNpcId(-1), mMoney(0), mAmountItems(0), mMaxItems(0), mNick(nick)
{
    init();
    logger->log("BuyDialog::BuyDialog nick:" + mNick);
}

void BuyDialog::init()
{
    setWindowName("Buy");
    setResizable(true);
    setCloseButton(true);
    setMinWidth(260);
    setMinHeight(230);
    setDefaultSize(260, 230, ImageRect::CENTER);

    mShopItems = new ShopItems;

    mShopItemList = new ShopListBox(mShopItems, mShopItems);
    mScrollArea = new ScrollArea(mShopItemList);
    mScrollArea->setHorizontalScrollPolicy(gcn::ScrollArea::SHOW_NEVER);

    mSlider = new Slider(1.0);
    mQuantityLabel = new Label(strprintf("%d / %d", mAmountItems, mMaxItems));
    mQuantityLabel->setAlignment(gcn::Graphics::CENTER);
    mMoneyLabel = new Label(strprintf(_("Price: %s / Total: %s"),
                                            "", ""));

    // TRANSLATORS: This is a narrow symbol used to denote 'increasing'.
    // You may change this symbol if your language uses another.
    mIncreaseButton = new Button(_("+"), "inc", this);
    // TRANSLATORS: This is a narrow symbol used to denote 'decreasing'.
    // You may change this symbol if your language uses another.
    mDecreaseButton = new Button(_("-"), "dec", this);
    mBuyButton = new Button(_("Buy"), "buy", this);
    mQuitButton = new Button(_("Quit"), "quit", this);
    mAddMaxButton = new Button(_("Max"), "max", this);

    mDecreaseButton->adjustSize();
    mDecreaseButton->setWidth(mIncreaseButton->getWidth());

    mIncreaseButton->setEnabled(false);
    mDecreaseButton->setEnabled(false);
    mBuyButton->setEnabled(false);
    mSlider->setEnabled(false);

    mSlider->setActionEventId("slider");
    mSlider->addActionListener(this);
    mShopItemList->addSelectionListener(this);

    ContainerPlacer place;
    place = getPlacer(0, 0);

    place(0, 0, mScrollArea, 8, 5).setPadding(3);
    place(0, 5, mDecreaseButton);
    place(1, 5, mSlider, 3);
    place(4, 5, mIncreaseButton);
    place(5, 5, mQuantityLabel, 2);
    place(7, 5, mAddMaxButton);
    place(0, 6, mMoneyLabel, 8);
    place(6, 7, mBuyButton);
    place(7, 7, mQuitButton);

    Layout &layout = getLayout();
    layout.setRowHeight(0, Layout::AUTO_SET);

    center();
    loadWindowState();

    instances.push_back(this);
    setVisible(true);
}

BuyDialog::~BuyDialog()
{
    delete mShopItems;
    mShopItems = 0;

    instances.remove(this);
}

void BuyDialog::setMoney(int amount)
{
    mMoney = amount;
    mShopItemList->setPlayersMoney(amount);

    updateButtonsAndLabels();
}

void BuyDialog::reset()
{
    mShopItems->clear();
    mShopItemList->adjustSize();

    // Reset previous selected items to prevent failing asserts
    mShopItemList->setSelected(-1);
    mSlider->setValue(0);

    setMoney(0);
}

void BuyDialog::addItem(int id, unsigned char color, int amount, int price)
{
    mShopItems->addItem(id, color, amount, price);
    mShopItemList->adjustSize();
}

void BuyDialog::action(const gcn::ActionEvent &event)
{
    if (event.getId() == "quit")
    {
        close();
        return;
    }

    int selectedItem = mShopItemList->getSelected();

    // The following actions require a valid selection
    if (selectedItem < 0 ||
        selectedItem >= static_cast<int>(mShopItems->getNumberOfElements()))
    {
        return;
    }

    if (event.getId() == "slider")
    {
        mAmountItems = static_cast<int>(mSlider->getValue());
        updateButtonsAndLabels();
    }
    else if (event.getId() == "inc" && mAmountItems < mMaxItems)
    {
        mAmountItems++;
        mSlider->setValue(mAmountItems);
        updateButtonsAndLabels();
    }
    else if (event.getId() == "dec" && mAmountItems > 1)
    {
        mAmountItems--;
        mSlider->setValue(mAmountItems);
        updateButtonsAndLabels();
    }
    else if (event.getId() == "max")
    {
        mAmountItems = mMaxItems;
        mSlider->setValue(mAmountItems);
        updateButtonsAndLabels();
    }
    // TODO: Actually we'd have a bug elsewhere if this check for the number
    // of items to be bought ever fails, Bertram removed the assertions, is
    // there a better way to ensure this fails in an _obvious_ way in C++?
    else if (event.getId() == "buy" && mAmountItems > 0 &&
             mAmountItems <= mMaxItems)
    {
        if (mNpcId != -1)
        {
            ShopItem *item = mShopItems->at(selectedItem);
            Net::getNpcHandler()->buyItem(mNpcId, item->getId(),
                item->getColor(), mAmountItems);

            // Update money and adjust the max number of items that can be bought
            mMaxItems -= mAmountItems;
            setMoney(mMoney -
                     mAmountItems * mShopItems->at(selectedItem)->getPrice());

            // Reset selection
            mAmountItems = 1;
            mSlider->setValue(1);
            mSlider->gcn::Slider::setScale(1, mMaxItems);
        }
        else if (tradeWindow)
        {
            ShopItem *item = mShopItems->at(selectedItem);
            if (item)
            {
                Net::getBuySellHandler()->sendBuyRequest(mNick,
                    item, mAmountItems);
//                logger->log("buy button mNick:" + mNick);
                if (tradeWindow)
                {
                    tradeWindow->addAutoMoney(mNick,
                        item->getPrice() * mAmountItems);
                }
            }
        }
    }
}

void BuyDialog::valueChanged(const gcn::SelectionEvent &event _UNUSED_)
{
    // Reset amount of items and update labels
    mAmountItems = 1;
    mSlider->setValue(1);

    updateButtonsAndLabels();
    mSlider->gcn::Slider::setScale(1, mMaxItems);
}

void BuyDialog::updateButtonsAndLabels()
{
    const int selectedItem = mShopItemList->getSelected();
    int price = 0;

    if (selectedItem > -1)
    {
        ShopItem * item = mShopItems->at(selectedItem);
        if (item)
        {
            int itemPrice = item->getPrice();

            // Calculate how many the player can afford
            if (itemPrice)
                mMaxItems = mMoney / itemPrice;
            else
                mMaxItems = 1;

            if (item->getQuantity() > 0 && mMaxItems > item->getQuantity())
                mMaxItems = item->getQuantity();

            if (mAmountItems > mMaxItems)
                mAmountItems = mMaxItems;

            // Calculate price of pending purchase
            price = mAmountItems * itemPrice;
        }
    }
    else
    {
        mMaxItems = 0;
        mAmountItems = 0;
    }

    // Enable or disable buttons and slider
    mIncreaseButton->setEnabled(mAmountItems < mMaxItems);
    mDecreaseButton->setEnabled(mAmountItems > 1);
    mBuyButton->setEnabled(mAmountItems > 0);
    mSlider->setEnabled(mMaxItems > 1);

    // Update quantity and money labels
    mQuantityLabel->setCaption(strprintf("%d / %d", mAmountItems, mMaxItems));
    mMoneyLabel->setCaption(strprintf(_("Price: %s / Total: %s"),
        Units::formatCurrency(price).c_str(),
        Units::formatCurrency(mMoney - price).c_str()));
}

void BuyDialog::setVisible(bool visible)
{
    Window::setVisible(visible);

    if (visible && mShopItemList)
        mShopItemList->requestFocus();
    else
        scheduleDelete();
}

void BuyDialog::closeAll()
{
    DialogList::iterator it = instances.begin();
    DialogList::iterator it_end = instances.end();

    for (; it != it_end; ++it)
    {
        if (*it)
            (*it)->close();
    }
}