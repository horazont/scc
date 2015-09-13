/**********************************************************************
File name: preferencesdialog.hpp
This file is part of: SCC (working title)

LICENSE

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.

FEEDBACK & QUESTIONS

For feedback and questions about SCC please e-mail one of the authors named in
the AUTHORS file.
**********************************************************************/
#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <cassert>
#include <iostream>
#include <memory>
#include <set>
#include <unordered_map>

#include <QAbstractItemModel>
#include <QItemDelegate>
#include <QModelIndex>
#include <QSortFilterProxyModel>

#include "ui_preferencesdialog.h"

#include "bindings.hpp"

class KeybindingsDelegate: public QItemDelegate
{
    Q_OBJECT
public: // QAbstractItemDelegate interface
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


class MousebindingsDelegate: public QItemDelegate
{
    Q_OBJECT
public: // QAbstractItemDelegate interface
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};


class PreferencesDialog: public QDialog
{
    Q_OBJECT
public:
    explicit PreferencesDialog(KeybindingsModel &keybindings,
                               MousebindingsModel &mousebindings,
                               QWidget *parent = 0);

private:
    Ui::PreferencesDialog m_ui;

    KeybindingsModel &m_keybindings;
    QSortFilterProxyModel m_keybindings_proxy;
    KeybindingsDelegate m_keybindings_delegate;

    MousebindingsModel &m_mousebindings;
    QSortFilterProxyModel m_mousebindings_proxy;
    MousebindingsDelegate m_mousebindings_delegate;

protected:
    void showEvent(QShowEvent *event) override;

public:
    void accept() override;

};

#endif // PREFERENCESDIALOG_H
