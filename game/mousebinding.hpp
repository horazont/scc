/**********************************************************************
File name: mousebinding.hpp
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
#ifndef MOUSEBINDING_H
#define MOUSEBINDING_H

#include <QAction>
#include <QKeySequence>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>


class MouseBinding
{
public:
    MouseBinding();
    MouseBinding(const Qt::KeyboardModifiers &modifiers,
                 const Qt::MouseButton &button);

    MouseBinding(const MouseBinding &ref) = default;
    MouseBinding &operator=(const MouseBinding &ref) = default;

private:
    bool m_valid;
    Qt::KeyboardModifiers m_modifiers;
    Qt::MouseButton m_button;

public:
    /**
     * Return true if the MouseBinding is valid and false otherwise.
     */
    inline bool is_valid() const
    {
        return m_valid;
    }

    /**
     * Convert the MouseBinding to a string. This is analogous to
     * QKeySequence::toString.
     *
     * @param format The format to use for the string.
     * @return The formatted string.
     */
    QString to_string(
            QKeySequence::SequenceFormat format = QKeySequence::PortableText
            ) const;

    /**
     * Compare two MouseBinding instances.
     */
    bool operator==(const MouseBinding &other) const;

    inline bool operator!=(const MouseBinding &other) const
    {
        return !(*this == other);
    }

    /**
     * Swap the contents of one MouseBinding with those of another.
     */
    void swap(MouseBinding &other);

    inline Qt::MouseButton button() const
    {
        return m_button;
    }

    inline Qt::KeyboardModifiers modifiers() const
    {
        return m_modifiers;
    }

    static MouseBinding from_string(const QString &str);

};


class MouseAction: public QAction
{
    Q_OBJECT
public:
    explicit MouseAction(QObject *parent = nullptr);

private:
    MouseBinding m_binding;

public:
    inline const MouseBinding &mouse_binding() const
    {
        return m_binding;
    }

    void set_mouse_binding(const MouseBinding &mouse_binding);

};


class MouseBindingEdit: public QWidget
{
    Q_OBJECT
public:
    MouseBindingEdit(QWidget *parent = nullptr);

private:
    QVBoxLayout m_layout;
    QLineEdit m_edit;

    MouseBinding m_value;

signals:
    void editingFinished();
    void mouseBindingChanged(const MouseBinding &binding);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

public:
    bool eventFilter(QObject *target, QEvent *event) override;

public:
    const MouseBinding &mouse_binding() const;
    void set_mouse_binding(const MouseBinding &value);

};


class MouseActionDispatcher
{
private:
    std::vector<MouseAction*> m_actions;

public:
    std::vector<MouseAction*> &actions()
    {
        return m_actions;
    }

    bool dispatch(QMouseEvent *event) const;

};


QDataStream &operator<<(QDataStream &s, const MouseBinding &binding);
QDataStream &operator>>(QDataStream &s, MouseBinding &binding);

#endif
