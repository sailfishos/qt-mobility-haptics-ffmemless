/****************************************************************************
**
** Copyright (C) 2012 Jolla Ltd.
** Contact: Chris Adams <chris.adams@jollamobile.com>
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QFEEDBACK_FFMEMLESS_H
#define QFEEDBACK_FFMEMLESS_H

#ifdef USING_QTFEEDBACK
#include <QtPlugin>
#else
#include <qmobilityglobal.h>
#endif

#include <qfeedbackplugininterfaces.h>
#include <linux/input.h>
#include <QElapsedTimer>
#include <QTimer>

QT_BEGIN_HEADER

#ifdef USING_QTFEEDBACK
QT_USE_NAMESPACE
#define ThemeEffect Effect
#define ThemeBasicKeypad PressWeak
#define ThemeBasicButton Press
#define ThemeLongPress PressStrong
#else
QTM_USE_NAMESPACE
#endif

class QFeedbackFFMemless : public QObject, public QFeedbackHapticsInterface, public QFeedbackThemeInterface
{
    Q_OBJECT
#ifdef USING_QTFEEDBACK
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtFeedbackPlugin" FILE "ffmemless.json")
    Q_INTERFACES(QFeedbackHapticsInterface)
    Q_INTERFACES(QFeedbackThemeInterface)
#else
    Q_INTERFACES(QTM_NAMESPACE::QFeedbackHapticsInterface)
    Q_INTERFACES(QTM_NAMESPACE::QFeedbackThemeInterface)
#endif
public:
    QFeedbackFFMemless(QObject *parent = 0);
    ~QFeedbackFFMemless();

    // theme effect interface
    QFeedbackInterface::PluginPriority pluginPriority();
    bool play(QFeedbackEffect::ThemeEffect effect);

    // haptic (custom) effect interface
    QList<QFeedbackActuator*> actuators();

    void setActuatorProperty(const QFeedbackActuator &, ActuatorProperty, const QVariant &);
    QVariant actuatorProperty(const QFeedbackActuator &, ActuatorProperty);
    bool isActuatorCapabilitySupported(const QFeedbackActuator &, QFeedbackActuator::Capability);

    void updateEffectProperty(const QFeedbackHapticsEffect *, EffectProperty);
    void setEffectState(const QFeedbackHapticsEffect *, QFeedbackEffect::State);
    QFeedbackEffect::State effectState(const QFeedbackHapticsEffect *);

private Q_SLOTS:
    void stateChangeTimerTriggered();

private:
    void stopCustomEffect(QFeedbackHapticsEffect *effect);
    void restartCustomEffect(QFeedbackHapticsEffect *effect);
    int reuploadUpdatedEffect(QFeedbackHapticsEffect *effect);
    void initialiseConstants();
    bool initialiseEffects();
    bool uploadEffect(struct ff_effect *effect);
    bool writeEffectEvent(struct input_event *event);

private:
    // theme effects
    struct input_event m_themeEffectPlayEvent;
    struct ff_effect m_themeEffect;

    // custom effects
    struct ff_effect m_customHapticEffect;
    struct ff_effect m_periodicHapticEffect;
    QList<QFeedbackActuator*> m_actuators;
    QElapsedTimer m_elapsedTimer;
    QTimer *m_stateChangeTimer;
    QFeedbackActuator *m_actuator;
    QFeedbackHapticsEffect *m_activeEffect;
    int m_vibraSpiDevice;
    bool m_actuatorEnabled;
    bool m_periodicEffectIsActive;

    // determined during ctor.
    bool m_supportsRumble;
    bool m_supportsPeriodic;
    bool m_themeEffectsPossible;
    bool m_customEffectsPossible;
    bool m_initialising;

    // custom effect constants, read from settings file if exists.
    quint16 ACTUATOR_SPIN_UP;
    quint16 ACTUATOR_SPIN_DOWN;
    quint16 ACTUATOR_RUMBLE_MIN;
    quint16 ACTUATOR_RUMBLE_MAX;
    qint16 ACTUATOR_MAGNITUDE_MAX;
    qint16 ACTUATOR_MAGNITUDE_MEAN;

    // theme effect constants, read from settings file if exists
    quint16 LONG_PRESS_DURATION;
    quint16 LONG_PRESS_DELAY;
    quint16 LONG_PRESS_MAX;
    quint16 LONG_PRESS_MIN;
    quint16 BUTTON_PRESS_DURATION;
    quint16 BUTTON_PRESS_DELAY;
    quint16 BUTTON_PRESS_MAX;
    quint16 BUTTON_PRESS_MIN;
    quint16 KEYPAD_PRESS_DURATION;
    quint16 KEYPAD_PRESS_DELAY;
    quint16 KEYPAD_PRESS_MAX;
    quint16 KEYPAD_PRESS_MIN;
};


QT_END_HEADER

#endif
