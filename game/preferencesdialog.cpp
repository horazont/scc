/**********************************************************************
File name: preferencesdialog.cpp
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
#include "preferencesdialog.hpp"

#include <QKeySequenceEdit>


QWidget *KeybindingsDelegate::createEditor(
        QWidget *parent,
        const QStyleOptionViewItem &,
        const QModelIndex &) const
{
    QWidget *editor = new QKeySequenceEdit(parent);
    return editor;
}

void KeybindingsDelegate::setEditorData(QWidget *editor,
                                        const QModelIndex &index) const
{
    const QAbstractProxyModel *proxy_model = static_cast<const QAbstractProxyModel*>(index.model());
    QModelIndex base_index = proxy_model->mapToSource(index);

    ModelIndexToken *token = static_cast<ModelIndexToken*>(base_index.internalPointer());
    if (!token || token->type != ModelIndexToken::Entry || !token->data) {
        return;
    }

    KeybindingsModel::Item *item = static_cast<KeybindingsModel::Item*>(token->data);
    QKeySequenceEdit *kse = static_cast<QKeySequenceEdit*>(editor);
    if (item->m_changed) {
        kse->setKeySequence(item->m_changed_value);
    } else {
        kse->setKeySequence(item->m_object->shortcut());
    }
}

void KeybindingsDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QAbstractProxyModel *proxy_model = static_cast<QAbstractProxyModel*>(model);
    QAbstractItemModel *base_model = proxy_model->sourceModel();
    QModelIndex base_index = proxy_model->mapToSource(index);

    ModelIndexToken *token = static_cast<ModelIndexToken*>(base_index.internalPointer());
    if (!token || token->type != ModelIndexToken::Entry || !token->data) {
        return;
    }

    KeybindingsModel::Item *item = static_cast<KeybindingsModel::Item*>(token->data);
    QKeySequenceEdit *kse = static_cast<QKeySequenceEdit*>(editor);
    item->m_changed = true;
    item->m_changed_value = kse->keySequence();
    base_model->dataChanged(base_index, base_index);
}

QSize KeybindingsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize original = QItemDelegate::sizeHint(option, index);
    original.setHeight(std::max(22, original.height()));
    return original;
}


QWidget *MousebindingsDelegate::createEditor(
        QWidget *parent,
        const QStyleOptionViewItem &,
        const QModelIndex &) const
{
    QWidget *editor = new MouseBindingEdit(parent);
    return editor;
}

void MousebindingsDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const QAbstractProxyModel *proxy_model = static_cast<const QAbstractProxyModel*>(index.model());
    QModelIndex base_index = proxy_model->mapToSource(index);

    ModelIndexToken *token = static_cast<ModelIndexToken*>(base_index.internalPointer());
    if (!token || token->type != ModelIndexToken::Entry || !token->data) {
        return;
    }

    MousebindingsModel::Item *item = static_cast<MousebindingsModel::Item*>(token->data);
    MouseBindingEdit *mbe = static_cast<MouseBindingEdit*>(editor);
    if (item->m_changed) {
        mbe->set_mouse_binding(item->m_changed_value);
    } else {
        mbe->set_mouse_binding(item->m_object->mouse_binding());
    }
}

void MousebindingsDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QAbstractProxyModel *proxy_model = static_cast<QAbstractProxyModel*>(model);
    QAbstractItemModel *base_model = proxy_model->sourceModel();
    QModelIndex base_index = proxy_model->mapToSource(index);

    ModelIndexToken *token = static_cast<ModelIndexToken*>(base_index.internalPointer());
    if (!token || token->type != ModelIndexToken::Entry || !token->data) {
        return;
    }

    MousebindingsModel::Item *item = static_cast<MousebindingsModel::Item*>(token->data);
    MouseBindingEdit *mbe = static_cast<MouseBindingEdit*>(editor);
    item->m_changed = true;
    item->m_changed_value = mbe->mouse_binding();
    base_model->dataChanged(base_index, base_index);
}

QSize MousebindingsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize original = QItemDelegate::sizeHint(option, index);
    original.setHeight(std::max(22, original.height()));
    return original;
}


PreferencesDialog::PreferencesDialog(KeybindingsModel &keybindings,
                                     MousebindingsModel &mousebindings,
                                     QWidget *parent):
    QDialog(parent),
    m_keybindings(keybindings),
    m_mousebindings(mousebindings)
{
    m_ui.setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);

    m_keybindings_proxy.setSourceModel(&m_keybindings);
    m_keybindings_proxy.setDynamicSortFilter(true);

    m_ui.keyboard_shortcut_list->setModel(&m_keybindings_proxy);
    m_ui.keyboard_shortcut_list->setItemDelegate(&m_keybindings_delegate);

    m_mousebindings_proxy.setSourceModel(&m_mousebindings);
    m_mousebindings_proxy.setDynamicSortFilter(true);

    m_ui.mouse_shortcut_list->setModel(&m_mousebindings_proxy);
    m_ui.mouse_shortcut_list->setItemDelegate(&m_mousebindings_delegate);
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
    m_ui.keyboard_shortcut_list->expandAll();
    m_ui.keyboard_shortcut_list->resizeColumnToContents(0);
    m_ui.mouse_shortcut_list->expandAll();
    m_ui.mouse_shortcut_list->resizeColumnToContents(0);
    QDialog::showEvent(event);
}

void PreferencesDialog::accept()
{
    m_keybindings.submit();
    m_mousebindings.submit();
    QDialog::accept();
}
