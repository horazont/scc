/**********************************************************************
File name: bindings.cpp
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
#include "bindings.hpp"

#include <QAction>


QVariant KeybindingsModel::item_data(int column, const KeybindingsModel::Item *item, int role) const
{
    switch (column)
    {
    case 0:
    {
        switch (role)
        {
        case Qt::DisplayRole:
        {
            return item->m_object->text();
        }
        }
    }
    case 1:
    {
        const QKeySequence &seq = (item->m_changed ? item->m_changed_value : item->m_object->shortcut());
        switch (role)
        {
        case Qt::DisplayRole:
        {
            return seq.toString(QKeySequence::NativeText);
        }
        }
    }
    default:;
    }

    return QVariant();
}

void KeybindingsModel::submit_item(BindingsModelBase::Item &item) const
{
    if (item.m_changed) {
        item.m_changed = false;
        item.m_object->setShortcut(item.m_changed_value);
    }
}


QVariant MousebindingsModel::item_data(int column, const MousebindingsModel::Item *item, int role) const
{
    switch (column)
    {
    case 0:
    {
        switch (role)
        {
        case Qt::DisplayRole:
        {
            return item->m_object->text();
        }
        }
    }
    case 1:
    {
        const MouseBinding &binding = (item->m_changed ? item->m_changed_value : item->m_object->mouse_binding());
        switch (role)
        {
        case Qt::DisplayRole:
        {
            return binding.to_string(QKeySequence::NativeText);
        }
        }
    }
    default:;
    }

    return QVariant();
}

void MousebindingsModel::submit_item(MousebindingsModel::Item &item) const
{
    if (item.m_changed) {
        item.m_changed = false;
        item.m_object->set_mouse_binding(item.m_changed_value);
    }
}
