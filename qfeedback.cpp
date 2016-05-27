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

#include "qfeedback.h"
#include <qfeedbackactuator.h>
#include <QtCore/QtPlugin>
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtDebug>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

int vibra_evdev_file_search(bool *supportsRumble, bool *supportsPeriodic)
{
    int i = 0;
    int fp = 1;
    char device_file_name[24];
    unsigned long features[4];

    /* fail safe stop at 256 devices */
    while (fp && i < 256) {
        sprintf(device_file_name, "/dev/input/event%d", i);
        fp = open(device_file_name, O_RDWR);
        if (fp == -1) {
            perror("Unable to open input file");
            break;
        }

        /* Query device */
        if (ioctl(fp, EVIOCGBIT(EV_FF, sizeof(unsigned long) * 4), features) < 0) {
            perror("Ioctl query failed");
            close(fp);
            i++;
            continue;
        }

        *supportsRumble = false;
        *supportsPeriodic = false;
        if (test_bit(FF_RUMBLE, features))
            *supportsRumble = true;
        if (test_bit(FF_PERIODIC, features))
            *supportsPeriodic = true;
        if (*supportsRumble || *supportsPeriodic)
            return fp;

        close(fp);
        i++;
    }

    return -1;
}


QFeedbackFFMemless::QFeedbackFFMemless(QObject *parent) : QObject(parent),
    m_profile(0),
    m_profileEnablesVibra(false),
    m_profileTouchscreenVibraLevel(0),
    m_stateChangeTimer(0),
    m_vibraSpiDevice(-1),
    m_actuatorEnabled(false),
    m_periodicEffectIsActive(false),
    m_themeEffectsPossible(false),
    m_customEffectsPossible(false),
    m_periodicThemeEffectsPossible(false),
    m_initialising(false),
    ACTUATOR_SPIN_UP(0),
    ACTUATOR_SPIN_DOWN(0),
    ACTUATOR_RUMBLE_MIN(0),
    ACTUATOR_RUMBLE_MAX(0),
    ACTUATOR_MAGNITUDE_MAX(0),
    ACTUATOR_MAGNITUDE_MEAN(0),
    LONG_PRESS_DURATION(0),
    LONG_PRESS_DELAY(0),
    LONG_PRESS_MAX(0),
    LONG_PRESS_MIN(0),
    BUTTON_PRESS_DURATION(0),
    BUTTON_PRESS_DELAY(0),
    BUTTON_PRESS_MAX(0),
    BUTTON_PRESS_MIN(0),
    KEYPAD_PRESS_DURATION(0),
    KEYPAD_PRESS_DELAY(0),
    KEYPAD_PRESS_MAX(0),
    KEYPAD_PRESS_MIN(0),
    KEYPAD_USE_PERIODIC(0),
    KEYPAD_PERIODIC_ATTACK_TIME(0),
    KEYPAD_PERIODIC_FADE_TIME(0),
    KEYPAD_PERIODIC_ATTACK_LEVEL(0),
    KEYPAD_PERIODIC_FADE_LEVEL(0)
{
    memset(&m_themeEffect, 0, sizeof(m_customHapticEffect));
    memset(&m_customHapticEffect, 0, sizeof(m_customHapticEffect));
    memset(&m_periodicHapticEffect, 0, sizeof(m_periodicHapticEffect));
    initialiseConstants();
    if (initialiseEffects()) {
        m_stateChangeTimer = new QTimer(this);
        m_stateChangeTimer->setSingleShot(true);
        connect(m_stateChangeTimer, SIGNAL(timeout()), this, SLOT(stateChangeTimerTriggered()));
        QFeedbackActuator *actuator = createFeedbackActuator(this, 1);
        m_actuators.append(actuator);

        m_profile = new Profile(this);
        connect(m_profile, SIGNAL(activeProfileChanged(QString)),
                this, SLOT(deviceProfileSettingsChanged()));
        connect(m_profile, SIGNAL(vibrationChanged(QString, bool)),
                this, SLOT(deviceProfileSettingsChanged()));
        connect(m_profile, SIGNAL(touchscreenVibrationLevelChanged(QString, int)),
                this, SLOT(deviceProfileSettingsChanged()));
        deviceProfileSettingsChanged();
    }
}

void QFeedbackFFMemless::deviceProfileSettingsChanged()
{
    m_profileEnablesVibra = m_profile->isVibrationEnabled(m_profile->activeProfile());
    m_profileTouchscreenVibraLevel = m_profile->touchscreenVibrationLevel(m_profile->activeProfile());
}

QFeedbackFFMemless::~QFeedbackFFMemless()
{
    if (m_vibraSpiDevice != -1)
        close(m_vibraSpiDevice);
}

void QFeedbackFFMemless::initialiseConstants()
{
// hardcode constants based on some heuristics.
// these set the "defaults" if no settings file is given.
#ifdef FF_MEMLESS_PIEZO
// Piezo Speaker
//  - super-fast response time
//  - regional vibration and tactility control
ACTUATOR_SPIN_UP        = 0U;
ACTUATOR_SPIN_DOWN      = 0U;
ACTUATOR_RUMBLE_MIN     = 0U;
ACTUATOR_RUMBLE_MAX     = 0x7fffU;
ACTUATOR_MAGNITUDE_MAX  = 0x7fff;
ACTUATOR_MAGNITUDE_MEAN = 0x4fff;
#elif FF_MEMLESS_LRA
// Linear Resonant Actuator
//  - fast response time
//  - whole-device vibration centred on screen
ACTUATOR_SPIN_UP        = 2U;
ACTUATOR_SPIN_DOWN      = 4U;
ACTUATOR_RUMBLE_MIN     = 0x1fffU;
ACTUATOR_RUMBLE_MAX     = 0x7fffU;
ACTUATOR_MAGNITUDE_MAX  = 0x7fff;
ACTUATOR_MAGNITUDE_MEAN = 0x4fff;
#else // FF_MEMLESS_ERM
// Eccentric Rotating Mass
//  - slow response time
//  - whole-device vibration
ACTUATOR_SPIN_UP        = 8U;
ACTUATOR_SPIN_DOWN      = 16U;
ACTUATOR_RUMBLE_MIN     = 0x3fffU;
ACTUATOR_RUMBLE_MAX     = 0x7fffU;
ACTUATOR_MAGNITUDE_MAX  = 0x7fff;
ACTUATOR_MAGNITUDE_MEAN = 0x4fff;
#endif

#ifdef FF_MEMLESS_SETTINGS
// read constants from default settings file
QSettings settings(FF_MEMLESS_SETTINGS, QSettings::IniFormat, this);
ACTUATOR_SPIN_UP        = settings.value("ACTUATOR_SPIN_UP", ACTUATOR_SPIN_UP).value<quint16>();
ACTUATOR_SPIN_DOWN      = settings.value("ACTUATOR_SPIN_DOWN", ACTUATOR_SPIN_DOWN).value<quint16>();
ACTUATOR_RUMBLE_MIN     = settings.value("ACTUATOR_RUMBLE_MIN", ACTUATOR_RUMBLE_MIN).value<quint16>();
ACTUATOR_RUMBLE_MAX     = settings.value("ACTUATOR_RUMBLE_MAX", ACTUATOR_RUMBLE_MAX).value<quint16>();
ACTUATOR_MAGNITUDE_MAX  = settings.value("ACTUATOR_MAGNITUDE_MAX", ACTUATOR_MAGNITUDE_MAX).value<qint16>();
ACTUATOR_MAGNITUDE_MEAN = settings.value("ACTUATOR_MAGNITUDE_MEAN", ACTUATOR_MAGNITUDE_MEAN).value<qint16>();
LONG_PRESS_DURATION     = settings.value("LONG_PRESS_DURATION", (ACTUATOR_SPIN_UP + 75U)).value<quint16>();
LONG_PRESS_DELAY        = settings.value("LONG_PRESS_DELAY", (ACTUATOR_SPIN_DOWN + 0U)).value<quint16>();
LONG_PRESS_MAX          = settings.value("LONG_PRESS_MAX", (ACTUATOR_RUMBLE_MAX - 0x3fffU)).value<quint16>();
LONG_PRESS_MIN          = settings.value("LONG_PRESS_MIN", (ACTUATOR_RUMBLE_MIN + 0U)).value<quint16>();
BUTTON_PRESS_DURATION   = settings.value("BUTTON_PRESS_DURATION", (ACTUATOR_SPIN_UP + 12U)).value<quint16>();
BUTTON_PRESS_DELAY      = settings.value("BUTTON_PRESS_DELAY", (ACTUATOR_SPIN_DOWN + 0U)).value<quint16>();
BUTTON_PRESS_MAX        = settings.value("BUTTON_PRESS_MAX", (ACTUATOR_RUMBLE_MAX - 0x1fffU)).value<quint16>();
BUTTON_PRESS_MIN        = settings.value("BUTTON_PRESS_MIN", (ACTUATOR_RUMBLE_MIN + 0x2fffU)).value<quint16>();
KEYPAD_PRESS_DURATION   = settings.value("KEYPAD_PRESS_DURATION", (ACTUATOR_SPIN_UP + 5U)).value<quint16>();
KEYPAD_PRESS_DELAY      = settings.value("KEYPAD_PRESS_DELAY", (ACTUATOR_SPIN_DOWN + 0U)).value<quint16>();
KEYPAD_PRESS_MAX        = settings.value("KEYPAD_PRESS_MAX", (ACTUATOR_RUMBLE_MAX - 0U)).value<quint16>();
KEYPAD_PRESS_MIN        = settings.value("KEYPAD_PRESS_MIN", (ACTUATOR_RUMBLE_MAX)).value<quint16>();
KEYPAD_USE_PERIODIC          = settings.value("KEYPAD_USE_PERIODIC", 0).value<quint16>();
KEYPAD_PERIODIC_ATTACK_LEVEL = settings.value("KEYPAD_PERIODIC_ATTACK_LEVEL", ACTUATOR_MAGNITUDE_MAX).value<quint16>();
KEYPAD_PERIODIC_FADE_LEVEL   = settings.value("KEYPAD_PERIODIC_FADE_LEVEL", 0).value<quint16>();
KEYPAD_PERIODIC_ATTACK_TIME  = settings.value("KEYPAD_PERIODIC_ATTACK_TIME", ACTUATOR_SPIN_UP).value<quint16>();
KEYPAD_PERIODIC_FADE_TIME    = settings.value("KEYPAD_PERIODIC_FADE_TIME", ACTUATOR_SPIN_DOWN).value<quint16>();
// read override constants from override settings file if specified in environment
QByteArray override = qgetenv("FF_MEMLESS_SETTINGS");
if (override.count() && QFile::exists(override)) {
    QSettings overrideSettings(override, QSettings::IniFormat, this);
    ACTUATOR_SPIN_UP        = overrideSettings.value("ACTUATOR_SPIN_UP", ACTUATOR_SPIN_UP).value<quint16>();
    ACTUATOR_SPIN_DOWN      = overrideSettings.value("ACTUATOR_SPIN_DOWN", ACTUATOR_SPIN_DOWN).value<quint16>();
    ACTUATOR_RUMBLE_MIN     = overrideSettings.value("ACTUATOR_RUMBLE_MIN", ACTUATOR_RUMBLE_MIN).value<quint16>();
    ACTUATOR_RUMBLE_MAX     = overrideSettings.value("ACTUATOR_RUMBLE_MAX", ACTUATOR_RUMBLE_MAX).value<quint16>();
    ACTUATOR_MAGNITUDE_MAX  = overrideSettings.value("ACTUATOR_MAGNITUDE_MAX", ACTUATOR_MAGNITUDE_MAX).value<qint16>();
    ACTUATOR_MAGNITUDE_MEAN = overrideSettings.value("ACTUATOR_MAGNITUDE_MEAN", ACTUATOR_MAGNITUDE_MEAN).value<qint16>();
    LONG_PRESS_DURATION     = overrideSettings.value("LONG_PRESS_DURATION", LONG_PRESS_DURATION).value<quint16>();
    LONG_PRESS_DELAY        = overrideSettings.value("LONG_PRESS_DELAY", LONG_PRESS_DELAY).value<quint16>();
    LONG_PRESS_MAX          = overrideSettings.value("LONG_PRESS_MAX", LONG_PRESS_MAX).value<quint16>();
    LONG_PRESS_MIN          = overrideSettings.value("LONG_PRESS_MIN", LONG_PRESS_MIN).value<quint16>();
    BUTTON_PRESS_DURATION   = overrideSettings.value("BUTTON_PRESS_DURATION", BUTTON_PRESS_DURATION).value<quint16>();
    BUTTON_PRESS_DELAY      = overrideSettings.value("BUTTON_PRESS_DELAY", BUTTON_PRESS_DELAY).value<quint16>();
    BUTTON_PRESS_MAX        = overrideSettings.value("BUTTON_PRESS_MAX", BUTTON_PRESS_MAX).value<quint16>();
    BUTTON_PRESS_MIN        = overrideSettings.value("BUTTON_PRESS_MIN", BUTTON_PRESS_MIN).value<quint16>();
    KEYPAD_PRESS_DURATION   = overrideSettings.value("KEYPAD_PRESS_DURATION", KEYPAD_PRESS_DURATION).value<quint16>();
    KEYPAD_PRESS_DELAY      = overrideSettings.value("KEYPAD_PRESS_DELAY", KEYPAD_PRESS_DELAY).value<quint16>();
    KEYPAD_PRESS_MAX        = overrideSettings.value("KEYPAD_PRESS_MAX", KEYPAD_PRESS_MAX).value<quint16>();
    KEYPAD_PRESS_MIN        = overrideSettings.value("KEYPAD_PRESS_MIN", KEYPAD_PRESS_MIN).value<quint16>();
    KEYPAD_USE_PERIODIC          = overrideSettings.value("KEYPAD_USE_PERIODIC", KEYPAD_USE_PERIODIC).value<quint16>();
    KEYPAD_PERIODIC_ATTACK_LEVEL = overrideSettings.value("KEYPAD_PERIODIC_ATTACK_LEVEL", KEYPAD_PERIODIC_ATTACK_LEVEL).value<quint16>();
    KEYPAD_PERIODIC_FADE_LEVEL   = overrideSettings.value("KEYPAD_PERIODIC_FADE_LEVEL", KEYPAD_PERIODIC_FADE_LEVEL).value<quint16>();
    KEYPAD_PERIODIC_ATTACK_TIME  = overrideSettings.value("KEYPAD_PERIODIC_ATTACK_TIME", KEYPAD_PERIODIC_ATTACK_TIME).value<quint16>();
    KEYPAD_PERIODIC_FADE_TIME    = overrideSettings.value("KEYPAD_PERIODIC_FADE_TIME", KEYPAD_PERIODIC_FADE_TIME).value<quint16>();
}
#else
LONG_PRESS_DURATION     = (ACTUATOR_SPIN_UP   + 75U);
LONG_PRESS_DELAY        = (ACTUATOR_SPIN_DOWN + 0U);
LONG_PRESS_MAX          = (ACTUATOR_RUMBLE_MAX  - 0x3fffU);
LONG_PRESS_MIN          = (ACTUATOR_RUMBLE_MIN  + 0U);
BUTTON_PRESS_DURATION   = (ACTUATOR_SPIN_UP   + 12U);
BUTTON_PRESS_DELAY      = (ACTUATOR_SPIN_DOWN + 0U);
BUTTON_PRESS_MAX        = (ACTUATOR_RUMBLE_MAX  - 0x1fffU);
BUTTON_PRESS_MIN        = (ACTUATOR_RUMBLE_MIN  + 0x2fffU);
KEYPAD_PRESS_DURATION   = (ACTUATOR_SPIN_UP   + 5U);
KEYPAD_PRESS_DELAY      = (ACTUATOR_SPIN_DOWN + 0U);
KEYPAD_PRESS_MAX        = (ACTUATOR_RUMBLE_MAX  - 0U);
KEYPAD_PRESS_MIN        = (ACTUATOR_RUMBLE_MAX);
KEYPAD_USE_PERIODIC          = 0U;
KEYPAD_PERIODIC_ATTACK_LEVEL = (ACTUATOR_RUMBLE_MAX  - 0U);
KEYPAD_PERIODIC_FADE_LEVEL   = 0U;
KEYPAD_PERIODIC_ATTACK_TIME  = 1U;
KEYPAD_PERIODIC_FADE_TIME    = 1U;
#endif // FF_MEMLESS_SETTINGS
}

bool QFeedbackFFMemless::initialiseEffects()
{
    if (m_initialising) {
        // initialiseEffects called by uploadEffect or writeEffectEvent during initialisation.
        // immediately return false to avoid deadlock.
        return false;
    }

    m_initialising = true;
    m_themeEffectsPossible = false;
    m_customEffectsPossible = false;
    m_actuatorEnabled = false;

    // close the previous fd to the ff-memless ioctl
    if (m_vibraSpiDevice != -1) {
        close(m_vibraSpiDevice);
        m_vibraSpiDevice = -1;
    }

    int fd = vibra_evdev_file_search(&m_supportsRumble, &m_supportsPeriodic);
    if (fd == -1) {
        qWarning() << Q_FUNC_INFO << "Error: did not find vibra spi device!";
        m_initialising = false;
        return false;
    }

    m_vibraSpiDevice = fd;

    m_themeEffectPlayEvent.type = EV_FF;
    m_themeEffectPlayEvent.value = 1;

    if (m_supportsRumble) {
        m_themeEffect.type = FF_RUMBLE;
        m_themeEffect.id = -1;
        m_themeEffect.u.rumble.strong_magnitude = KEYPAD_PRESS_MAX;
        m_themeEffect.u.rumble.weak_magnitude = KEYPAD_PRESS_MIN;
        m_themeEffect.replay.length = KEYPAD_PRESS_DURATION;
        m_themeEffect.replay.delay = KEYPAD_PRESS_DELAY;
        m_themeEffectsPossible = uploadEffect(&m_themeEffect);

        m_customHapticEffect.type = FF_RUMBLE;
        m_customHapticEffect.id = -1;
        m_customHapticEffect.u.rumble.strong_magnitude = ACTUATOR_RUMBLE_MAX;
        m_customHapticEffect.u.rumble.weak_magnitude = ACTUATOR_RUMBLE_MIN;
        m_customHapticEffect.replay.length = 100 + ACTUATOR_SPIN_UP;
        m_customHapticEffect.replay.delay = ACTUATOR_SPIN_DOWN;
        m_supportsRumble = uploadEffect(&m_customHapticEffect);
        m_customEffectsPossible = m_supportsRumble;
        m_actuatorEnabled = m_supportsRumble;
    }

    if (m_supportsPeriodic) {
        m_periodicHapticEffect.type = FF_PERIODIC;
        m_periodicHapticEffect.id = -1;
        m_periodicHapticEffect.u.periodic.waveform = FF_SINE;
        m_periodicHapticEffect.u.periodic.period = 0x500U;   // period
        m_periodicHapticEffect.u.periodic.magnitude = ACTUATOR_MAGNITUDE_MAX;
        m_periodicHapticEffect.u.periodic.offset = ACTUATOR_MAGNITUDE_MEAN;
        m_periodicHapticEffect.u.periodic.phase = 0x0U;      // hshift
        m_periodicHapticEffect.u.periodic.envelope.attack_length = 0x30U;
        m_periodicHapticEffect.u.periodic.envelope.attack_level = 0x500;
        m_periodicHapticEffect.u.periodic.envelope.fade_length = 0x30U;
        m_periodicHapticEffect.u.periodic.envelope.fade_level = 0x0U;
        m_periodicHapticEffect.replay.length = ACTUATOR_SPIN_UP + 100;
        m_periodicHapticEffect.replay.delay = ACTUATOR_SPIN_DOWN;
        m_supportsPeriodic = uploadEffect(&m_periodicHapticEffect);
        if (!m_customEffectsPossible) {
            m_customEffectsPossible = m_supportsPeriodic;
            m_actuatorEnabled = m_supportsPeriodic;
        }

        if (KEYPAD_USE_PERIODIC && m_supportsPeriodic) {
            // if we support periodic custom haptic effects, we can use a
            // periodic effect for the keypress theme effect.  This just
            // allows a nicer, clickier effect.
            m_periodicThemeEffect.type = FF_PERIODIC;
            m_periodicThemeEffect.id = -1;
            m_periodicThemeEffect.u.periodic.waveform = FF_SINE;
            m_periodicThemeEffect.u.periodic.period = KEYPAD_PRESS_DURATION;
            m_periodicThemeEffect.u.periodic.magnitude = KEYPAD_PRESS_MAX / 2; // KPM is 16bU, MAG is 16bS
            m_periodicThemeEffect.u.periodic.offset = KEYPAD_PRESS_MIN / 2;    // KPM is 16bU, OFF is 16bS
            m_periodicThemeEffect.u.periodic.phase = 0x0U;
            m_periodicThemeEffect.u.periodic.envelope.attack_length = KEYPAD_PERIODIC_ATTACK_TIME;
            m_periodicThemeEffect.u.periodic.envelope.attack_level = KEYPAD_PERIODIC_ATTACK_LEVEL;
            m_periodicThemeEffect.u.periodic.envelope.fade_length = KEYPAD_PERIODIC_FADE_TIME;
            m_periodicThemeEffect.u.periodic.envelope.fade_level = KEYPAD_PERIODIC_FADE_LEVEL;
            m_periodicThemeEffect.replay.length = ACTUATOR_SPIN_UP + KEYPAD_PRESS_DURATION;
            m_periodicThemeEffect.replay.delay = ACTUATOR_SPIN_DOWN + KEYPAD_PRESS_DELAY;
            m_periodicThemeEffectsPossible = uploadEffect(&m_periodicThemeEffect);
        }
    }

    // if we can't provide any effects, close the device.
    if (!m_themeEffectsPossible && !m_customEffectsPossible) {
        qWarning() << Q_FUNC_INFO << "Error: unable to provide theme effects or custom effects - closing vibra device";
        close(fd);
        m_vibraSpiDevice = -1;
        m_initialising = false;
        return false;
    }

    m_initialising = false;
    return true;
}

bool QFeedbackFFMemless::uploadEffect(struct ff_effect *effect)
{
    if (m_vibraSpiDevice == -1)
        return false;

    int uploadRetn = ioctl(m_vibraSpiDevice, EVIOCSFF, effect);

    if (uploadRetn == -1) {
        qWarning() << Q_FUNC_INFO << "Unable to upload effect";
        initialiseEffects();
        return false;
    }

    if (effect->id == -1) {
        qWarning() << Q_FUNC_INFO << "No available effect ids";
        return false;
    }

    if (effect->id == 0) {
        qWarning() << Q_FUNC_INFO << "Effects have been flushed; reinitialising";
        initialiseEffects();
        return false;
    }

    return true;
}

bool QFeedbackFFMemless::writeEffectEvent(struct input_event *event)
{
    if (m_vibraSpiDevice == -1)
        return false;

    int writeRetn = write(m_vibraSpiDevice, (const void*) event, sizeof(*event));

    if (writeRetn == -1) {
        qWarning() << Q_FUNC_INFO << "Unable to write event to effect";
        return false;
    }

    return true;
}

QFeedbackInterface::PluginPriority QFeedbackFFMemless::pluginPriority()
{
    if (m_vibraSpiDevice == -1)
        return PluginLowPriority;
    return PluginNormalPriority;
}

bool QFeedbackFFMemless::play(QFeedbackEffect::ThemeEffect effect)
{
    if (Q_UNLIKELY(!m_themeEffectsPossible || !m_profileEnablesVibra))
        return false;

    // use Q_LIKELY to optimise for VKB key presses
    if (Q_LIKELY(effect == QFeedbackEffect::ThemeBasicKeypad && m_periodicThemeEffectsPossible)) {
        if (Q_UNLIKELY(m_profileTouchscreenVibraLevel == 0)) {
            return false;
        }
        m_themeEffectPlayEvent.code = m_periodicThemeEffect.id;
        return writeEffectEvent(&m_themeEffectPlayEvent);
    }

    switch (effect) {
        case QFeedbackEffect::ThemeLongPress:
        {
            m_themeEffect.u.rumble.strong_magnitude = LONG_PRESS_MAX;
            m_themeEffect.u.rumble.weak_magnitude = LONG_PRESS_MIN;
            m_themeEffect.replay.length = LONG_PRESS_DURATION;
            m_themeEffect.replay.delay = LONG_PRESS_DELAY;
        }
        break;
        case QFeedbackEffect::ThemeBasicKeypad:
        {
            if (Q_UNLIKELY(m_profileTouchscreenVibraLevel == 0))
                return false;

            m_themeEffect.u.rumble.strong_magnitude = KEYPAD_PRESS_MAX;
            m_themeEffect.u.rumble.weak_magnitude = KEYPAD_PRESS_MIN;
            m_themeEffect.replay.length = KEYPAD_PRESS_DURATION;
            m_themeEffect.replay.delay = KEYPAD_PRESS_DELAY;
        }
        break;
        case QFeedbackEffect::ThemeBasicButton: // BasicButton is the default.
        {
            if (Q_UNLIKELY(m_profileTouchscreenVibraLevel == 0))
                return false;
        }
        default:
        {
            m_themeEffect.u.rumble.strong_magnitude = BUTTON_PRESS_MAX;
            m_themeEffect.u.rumble.weak_magnitude = BUTTON_PRESS_MIN;
            m_themeEffect.replay.length = BUTTON_PRESS_DURATION;
            m_themeEffect.replay.delay = BUTTON_PRESS_DELAY;
        }
        break;
    }

    if (!uploadEffect(&m_themeEffect))
        return false;

    m_themeEffectPlayEvent.code = m_themeEffect.id;
    return writeEffectEvent(&m_themeEffectPlayEvent);
}

QList<QFeedbackActuator*> QFeedbackFFMemless::actuators()
{
    return m_actuators;
}

void QFeedbackFFMemless::setActuatorProperty(const QFeedbackActuator &actuator, ActuatorProperty prop, const QVariant &value)
{
    Q_UNUSED(actuator)

    switch (prop)
    {
        case Enabled: m_actuatorEnabled = value.toBool(); break;
        default: break;
    }
}

QVariant QFeedbackFFMemless::actuatorProperty(const QFeedbackActuator &, ActuatorProperty prop)
{
    switch (prop)
    {
        case Name:    return QLatin1String("FF_MEMLESS");
        case State:   return QFeedbackActuator::Ready;
        case Enabled: return m_actuatorEnabled;
        default:      return QVariant();
    }
}

bool QFeedbackFFMemless::isActuatorCapabilitySupported(const QFeedbackActuator &, QFeedbackActuator::Capability cap)
{
    switch(cap)
    {
        case QFeedbackActuator::Envelope:
            return m_supportsPeriodic; // ff_constant also supports envelope.
        case QFeedbackActuator::Period:
            return m_supportsPeriodic;
        default:
            return false;
    }
}

void QFeedbackFFMemless::stateChangeTimerTriggered()
{
    stopCustomEffect(m_activeEffect);
}

// returns -1 if failed, 0 if custom effect, 1 if periodic effect.
int QFeedbackFFMemless::reuploadUpdatedEffect(QFeedbackHapticsEffect *effect)
{
    // 0.0 to 1.0, we map to 0x0000 to 0x7fff
    qreal ei = effect->intensity();
    if (ei > 1.0) ei = 1.0;
    if (ei < 0.0) ei = 0.0;

    // 0x7fff (32767 msec) is max duration val
    int ed = effect->duration();
    if (ed > 32767) ed = 32767;
    if (ed < 0) ed = 0;

    int period = effect->period();
    if (period > 32767) period = 32767;
    if (period < 0) period = 0;

    int attktime = effect->attackTime();
    if (attktime > 32767) attktime = 32767;
    if (attktime < 0) attktime = 0;

    int fadetime = effect->fadeTime();
    if (fadetime > 32767) fadetime = 32767;
    if (fadetime < 0) fadetime = 0;

    qreal attkint = effect->attackIntensity();
    if (attkint > 1.0) attkint = 1.0;
    if (attkint < 0.0) attkint = 0.0;

    qreal fadeint = effect->fadeIntensity();
    if (fadeint > 1.0) fadeint = 1.0;
    if (fadeint < 0.0) fadeint = 0.0;

    if (m_supportsPeriodic && (period != 0 || attktime != 0 || fadetime != 0)) {
        m_periodicHapticEffect.u.periodic.period = (quint16)period;
        m_periodicHapticEffect.u.periodic.magnitude = (qint16)(ei * ACTUATOR_MAGNITUDE_MAX);
        m_periodicHapticEffect.u.periodic.offset = (qint16)(ei * ACTUATOR_MAGNITUDE_MEAN);
        m_periodicHapticEffect.u.periodic.envelope.attack_length = (quint16)attktime;
        m_periodicHapticEffect.u.periodic.envelope.attack_level = (quint16)(attkint * ACTUATOR_MAGNITUDE_MAX);
        m_periodicHapticEffect.u.periodic.envelope.fade_length = (quint16)fadetime;
        m_periodicHapticEffect.u.periodic.envelope.fade_level = (quint16)(fadeint * ACTUATOR_MAGNITUDE_MAX);
        m_periodicHapticEffect.replay.length = ACTUATOR_SPIN_UP + (quint16)ed;
        m_periodicHapticEffect.replay.delay = ACTUATOR_SPIN_DOWN;

        // upload the updated effect struct
        if (uploadEffect(&m_periodicHapticEffect)) {
            m_periodicEffectIsActive = true; // periodic is active
            return 1;
        }
    } else {
        m_customHapticEffect.u.rumble.strong_magnitude = (quint16)(ei * ACTUATOR_RUMBLE_MAX);
        m_customHapticEffect.u.rumble.weak_magnitude = ACTUATOR_RUMBLE_MIN;
        m_customHapticEffect.replay.length = ACTUATOR_SPIN_UP + (quint16)ed;

        // upload the updated effect struct
        if (uploadEffect(&m_customHapticEffect)) {
            m_periodicEffectIsActive = false; // rumble is active
            return 0;
        }
    }

    return -1;
}

void QFeedbackFFMemless::stopCustomEffect(QFeedbackHapticsEffect *effect)
{
    if (m_activeEffect != effect)
        return;

    // reset our state.
    m_stateChangeTimer->stop();
    m_activeEffect = 0;

    // write stop.
    struct input_event stopEvent;
    stopEvent.type = EV_FF;
    stopEvent.code = m_customHapticEffect.id;
    stopEvent.value = 0;
    writeEffectEvent(&stopEvent);
    m_periodicEffectIsActive = false;

    // emit stateChanged on the effect
    QMetaObject::invokeMethod(effect, "stateChanged");
}

void QFeedbackFFMemless::restartCustomEffect(QFeedbackHapticsEffect *effect)
{
    // write stop.
    stopCustomEffect(effect);

    // check to see if we should play.
    if (Q_UNLIKELY(!m_profileEnablesVibra))
        return;

    // reupload the (possibly updated) effect.
    int whichEffect = reuploadUpdatedEffect(effect);
    if (whichEffect == -1)
        return;

    // write play.
    struct input_event playEvent;
    playEvent.type = EV_FF;
    playEvent.code = whichEffect ? m_periodicHapticEffect.id : m_customHapticEffect.id;
    playEvent.value = 1;
    if (writeEffectEvent(&playEvent)) {
        // restart timers.
        m_activeEffect = effect;
        m_stateChangeTimer->setInterval(whichEffect ? m_periodicHapticEffect.replay.length : m_customHapticEffect.replay.length);
        m_stateChangeTimer->start();
        m_elapsedTimer.start();
    }
}

void QFeedbackFFMemless::updateEffectProperty(const QFeedbackHapticsEffect *effect, QFeedbackHapticsInterface::EffectProperty prop)
{
    if (!m_customEffectsPossible || m_activeEffect != effect) // not running.
        return;

    if (m_periodicEffectIsActive)
        return; // cannot update periodic effects while they're running.

    switch (prop) {
    case QFeedbackHapticsInterface::Intensity:
        {
            // value should be in the range of 0.0 to 1.0
            // we map those to 0x0000 to 0x7fff
            qreal ei = effect->intensity();
            if (ei > 1.0) ei = 1.0;
            if (ei < 0.0) ei = 0.0;
            m_customHapticEffect.u.rumble.strong_magnitude = (quint16)(ei * ACTUATOR_RUMBLE_MAX);
            m_customHapticEffect.u.rumble.weak_magnitude = ACTUATOR_RUMBLE_MIN;
            quint16 elapsedSinceStart = m_elapsedTimer.elapsed();
            if (elapsedSinceStart < m_customHapticEffect.replay.length)
                m_customHapticEffect.replay.length = m_customHapticEffect.replay.length - elapsedSinceStart;
        }
        break;
    case QFeedbackHapticsInterface::Duration:
        {
            // 0x7fff (32767 msec) is max val
            int ed = effect->duration();
            if (ed > 32767) ed = 32767;
            if (ed < 0) ed = 0;
            m_customHapticEffect.replay.length = (quint16)ed;
        }
        break;
    default:
        return; // do nothing, we can't update this property while it's running.
    }

    // and play the effect.
    restartCustomEffect(const_cast<QFeedbackHapticsEffect*>(effect));
}

void QFeedbackFFMemless::setEffectState(const QFeedbackHapticsEffect *effect, QFeedbackEffect::State state)
{
    if (!m_customEffectsPossible || !m_actuatorEnabled)
        return;

    switch (state) {
        case QFeedbackEffect::Running: restartCustomEffect(const_cast<QFeedbackHapticsEffect*>(effect)); break;
        case QFeedbackEffect::Stopped: stopCustomEffect(const_cast<QFeedbackHapticsEffect*>(effect)); break;
        case QFeedbackEffect::Paused:  // not supported
        case QFeedbackEffect::Loading: // not supported
        default: break;
    }
}

QFeedbackEffect::State QFeedbackFFMemless::effectState(const QFeedbackHapticsEffect *effect)
{
    // probably better ways to do this, via EV_FF_STATUS?
    if (m_activeEffect == effect)
        return QFeedbackEffect::Running;
    return QFeedbackEffect::Stopped;
}


