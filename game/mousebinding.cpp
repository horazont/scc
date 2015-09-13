/**********************************************************************
File name: mousebinding.cpp
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
#include "mousebinding.hpp"

#include <iostream>
#include <unordered_map>

#include <QCoreApplication>
#include <QDataStream>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimerEvent>


static std::unordered_map<int, QString> button_names({
                                                             {Qt::LeftButton, QT_TRANSLATE_NOOP("MouseBinding", "Left Button")},
                                                             {Qt::MiddleButton, QT_TRANSLATE_NOOP("MouseBinding", "Middle Button")},
                                                             {Qt::RightButton, QT_TRANSLATE_NOOP("MouseBinding", "Right Button")},
                                                             {Qt::BackButton, QT_TRANSLATE_NOOP("MouseBinding", "Back Button")},
                                                             {Qt::ForwardButton, QT_TRANSLATE_NOOP("MouseBinding", "Forward Button")},
        });


MouseBinding::MouseBinding():
    m_valid(false),
    m_modifiers(0),
    m_button(Qt::NoButton)
{

}

MouseBinding::MouseBinding(const Qt::KeyboardModifiers &modifiers,
                           const Qt::MouseButton &button):
    m_valid(true),
    m_modifiers(modifiers),
    m_button(button)
{

}

QString MouseBinding::to_string(QKeySequence::SequenceFormat format) const
{
    if (!m_valid) {
        return "";
    }

    bool native = (format == QKeySequence::NativeText);

    QString s = QKeySequence(m_modifiers).toString(format);

    auto iter = button_names.find(m_button);
    if (iter == button_names.end()) {
        QString format_str = native ? QCoreApplication::translate("MouseBinding", "Button %1") : QStringLiteral("Button %1");
        s += format_str.arg(QString::number((unsigned int)m_button));
    } else {
        s += native ? QCoreApplication::translate("MouseBinding", iter->second.toStdString().c_str()) : iter->second;
    }

    return s;
}

bool MouseBinding::operator==(const MouseBinding &other) const
{
    if (!m_valid || !other.m_valid) {
        return m_valid == other.m_valid;
    }

    return (m_modifiers == other.m_modifiers &&
            m_button == other.m_button);
}


MouseAction::MouseAction(QObject *parent):
    QAction(parent)
{

}

void MouseAction::set_mouse_binding(const MouseBinding &binding)
{
    if (m_binding != binding) {
        m_binding = binding;
        emit changed();
    }
}


MouseBindingEdit::MouseBindingEdit(QWidget *parent):
    QWidget(parent),
    m_layout(this),
    m_edit()
{
    std::cout << "mouse binding edit created" << std::endl;

    m_layout.setContentsMargins(0, 0, 0, 0);
    m_layout.addWidget(&m_edit);

    m_edit.installEventFilter(this);
    m_edit.setFocusProxy(this);
    m_edit.setContextMenuPolicy(Qt::NoContextMenu);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_MacShowFocusRect, true);
    setAttribute(Qt::WA_InputMethodEnabled, false);

}

void MouseBindingEdit::mousePressEvent(QMouseEvent *event)
{
    MouseBinding value(event->modifiers(), event->button());
    set_mouse_binding(value);
    mouseBindingChanged(value);
    event->ignore();
}

void MouseBindingEdit::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Backspace:
    {
        MouseBinding value;
        set_mouse_binding(value);
        mouseBindingChanged(value);
        break;
    }
    }
}

bool MouseBindingEdit::eventFilter(QObject *, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::MouseButtonPress:
    {
        mousePressEvent(static_cast<QMouseEvent*>(event));
        return true;
    }
    case QEvent::MouseButtonRelease:
    {
        return true;
    }
    case QEvent::KeyPress:
    {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return true;
    }
    default:;
    }
    return false;
}

const MouseBinding &MouseBindingEdit::mouse_binding() const
{
    return m_value;
}

void MouseBindingEdit::set_mouse_binding(const MouseBinding &value)
{
    if (value == m_value) {
        return;
    }

    m_value = value;
    m_edit.setText(m_value.to_string(QKeySequence::NativeText));
}


QDataStream &operator<<(QDataStream &s, const MouseBinding &binding)
{
    return s << (uint32_t)binding.modifiers() << (uint32_t)binding.button();
}


QDataStream &operator>>(QDataStream &s, MouseBinding &binding)
{
    uint32_t modifiers;
    uint32_t button;
    s >> modifiers;
    s >> button;
    binding = MouseBinding((Qt::KeyboardModifiers)modifiers, (Qt::MouseButton)button);
    return s;
}
