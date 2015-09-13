/**********************************************************************
File name: bindings.hpp
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
#ifndef BINDINGS_H
#define BINDINGS_H

#include <memory>
#include <unordered_map>

#include <QAbstractItemModel>
#include <QCoreApplication>

#include "mousebinding.hpp"


struct ModelIndexToken
{
    enum Type {
        Group = 0,
        Entry = 1
    };

    Type type;
    void *data;
};


template <typename Object, typename Value>
class BindingsModelBase: public QAbstractItemModel
{
public:
    struct Group;

    struct Item
    {
        Item(Group *group, Object *object):
            m_group(group),
            m_object(object),
            m_changed(false),
            m_token{ModelIndexToken::Entry, this}
        {

        }

        Group *m_group;
        Object *m_object;
        Value m_changed_value;
        bool m_changed;
        ModelIndexToken m_token;
    };

    struct Group
    {
        Group(const std::string &name):
            m_name(name),
            m_localized_name(QCoreApplication::translate("Binding", name.c_str())),
            m_token{ModelIndexToken::Group, this}
        {

        }

        const std::string m_name;
        const QString m_localized_name;
        std::vector<std::unique_ptr<Item> > m_items;
        ModelIndexToken m_token;
    };

public:
    BindingsModelBase():
        QAbstractItemModel()
    {

    }

private:
    std::vector<std::unique_ptr<Group> > m_groups;
    std::unordered_map<std::string, Group*> m_group_map;

protected:
    std::pair<unsigned int, Group*> autocreate_group(const std::string &group_name)
    {
        auto iter = m_group_map.find(group_name);
        if (iter == m_group_map.end()) {
            unsigned int index = m_groups.size();
            beginInsertRows(QModelIndex(), index, index);
            m_groups.emplace_back(new Group(group_name));
            m_group_map[group_name] = m_groups[index].get();
            endInsertRows();
            return std::make_pair(index, m_groups[index].get());
        } else {
            Group *group = iter->second;
            return std::make_pair(find_group(group), group);
        }
    }

    Group *group_from_mindex(const QModelIndex &mindex) const
    {
        if (!mindex.isValid()) {
            return nullptr;
        }

        ModelIndexToken *token = static_cast<ModelIndexToken*>(mindex.internalPointer());
        if (!token) {
            return nullptr;
        }

        if (token->type != ModelIndexToken::Group) {
            return nullptr;
        }

        return static_cast<Group*>(token->data);
    }

    unsigned int find_group(const Group *group) const
    {
        auto iter = std::find_if(m_groups.begin(), m_groups.end(),
                                 [group](const std::unique_ptr<Group> &group_ptr){return group_ptr.get() == group;});
        assert(iter != m_groups.end());
        return (iter - m_groups.begin());
    }

    virtual QVariant group_data(int column, const Group *group, int role) const
    {
        switch (role)
        {
        case Qt::DisplayRole:
        {
            switch (column)
            {
            case 0: return group->m_localized_name;
            default:;
            }
        }
        default: return QVariant();
        }
    }

    virtual QVariant item_data(int column, const Item *item, int role) const = 0;
    virtual void submit_item(Item &item) const = 0;

public:
    void add_item(const std::string &group_name, Object *object)
    {
        unsigned int group_index;
        Group *group;
        std::tie(group_index, group) = autocreate_group(group_name);

        QModelIndex group_mindex = index(group_index, 0, QModelIndex());
        beginInsertRows(group_mindex, group->m_items.size(), group->m_items.size());
        group->m_items.emplace_back(new Item(group, object));
        endInsertRows();
    }

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override
    {
        if (column < 0 || column >= 2) {
            return QModelIndex();
        }

        if (parent.isValid()) {
            Group *group = group_from_mindex(parent);
            if (!group) {
                return QModelIndex();
            }
            if ((unsigned int)row >= group->m_items.size() || row < 0) {
                return QModelIndex();
            }
            return createIndex(row, column, &group->m_items[row]->m_token);
        }

        if ((unsigned int)row >= m_groups.size() || row < 0) {
            return QModelIndex();
        }
        return createIndex(row, column, &m_groups[row]->m_token);
    }

    QModelIndex parent(const QModelIndex &child) const override
    {
        ModelIndexToken *token = static_cast<ModelIndexToken*>(child.internalPointer());
        if (!token) {
            return QModelIndex();
        }

        switch (token->type) {
        case ModelIndexToken::Group:
        {
            return QModelIndex();
        }
        case ModelIndexToken::Entry:
        {
            Item *item = static_cast<Item*>(token->data);
            Group *group = item->m_group;
            return createIndex(find_group(group), 0, &group->m_token);
        }
        }

        return QModelIndex();
    }

    int rowCount(const QModelIndex &parent) const override
    {
        if (!parent.isValid()) {
            // root node
            return m_groups.size();
        }

        ModelIndexToken *token = static_cast<ModelIndexToken*>(parent.internalPointer());
        if (!token) {
            return 0;
        }

        switch (token->type) {
        case ModelIndexToken::Group:
        {
            Group *group = static_cast<Group*>(token->data);
            return group->m_items.size();
        }
        default:;
        }

        return 0;
    }

    int columnCount(const QModelIndex &parent) const override
    {
        if (!parent.isValid()) {
            // root node
            return 2;
        }

        ModelIndexToken *token = static_cast<ModelIndexToken*>(parent.internalPointer());
        if (!token) {
            return 0;
        }

        switch (token->type) {
        case ModelIndexToken::Group:
        {
            return 2;
        }
        case ModelIndexToken::Entry:
        {
            return 2;
        }
        }

        return 0;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid()) {
            // root node
            return QVariant();
        }

        ModelIndexToken *token = static_cast<ModelIndexToken*>(index.internalPointer());
        if (!token) {
            return QVariant();
        }

        switch (token->type) {
        case ModelIndexToken::Group:
        {
            Group *group = static_cast<Group*>(token->data);
            return group_data(index.column(), group, role);
        }
        case ModelIndexToken::Entry:
        {
            Item *item = static_cast<Item*>(token->data);
            return item_data(index.column(), item, role);
        }
        }

        return QVariant();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid()) {
            // root node
            return QAbstractItemModel::flags(index);
        }

        ModelIndexToken *token = static_cast<ModelIndexToken*>(index.internalPointer());
        if (!token) {
            return QAbstractItemModel::flags(index);
        }

        switch (token->type) {
        case ModelIndexToken::Group:
        {
            return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        }
        case ModelIndexToken::Entry:
        {
            Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            switch (index.column())
            {
            case 1:
            {
                flags |= Qt::ItemIsEditable;
            }
            default:;
            }
            return flags;
        }
        }

        return QAbstractItemModel::flags(index);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal) {
            return QVariant();
        }

        switch (section)
        {
        case 0:
        {
            switch (role)
            {
            case Qt::DisplayRole:
            {
                return QCoreApplication::translate("BindingList", "Action");
            }
            default:;
            }
        }
        case 1:
        {
            switch (role)
            {
            case Qt::DisplayRole:
            {
                return QCoreApplication::translate("BindingList", "Binding");
            }
            default:;
            }
        }
        default:;
        }

        return QVariant();
    }

    bool submit() override
    {
        for (auto &group: m_groups) {
            for (auto &item: group->m_items) {
                submit_item(*item);
            }
        }
        return true;
    }

    void revert()
    {
        unsigned int i = 0;
        for (auto &group: m_groups) {
            QModelIndex group_mindex(createIndex(i, 0, &group->m_token));
            for (auto &item: group->m_items) {
                item->m_changed = false;
            }
            dataChanged(index(0, 1, group_mindex), index(group->m_items.size()-1, 1, group_mindex));
            i += 1;
        }

    }
};


class KeybindingsModel: public BindingsModelBase<QAction, QKeySequence>
{
    Q_OBJECT
protected: // BindingsModelBase interface
    QVariant item_data(int column, const Item *item, int role) const override;
    void submit_item(Item &item) const override;

};


class MousebindingsModel: public BindingsModelBase<MouseAction, MouseBinding>
{
    Q_OBJECT
protected: // BindingsModelBase interface
    QVariant item_data(int column, const Item *item, int role) const override;
    void submit_item(Item &item) const override;
};


#endif
