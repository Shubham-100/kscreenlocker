/********************************************************************
 KSld - the KDE Screenlocker Daemon
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// own
#include "../ksldapp.h"
// KDE Frameworks
#include <KIdleTime>
// Qt
#include <QtTest/QtTest>
#include <QProcess>
// xcb
#include <xcb/xcb.h>

class KSldTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testEstablishGrab();
    void testActivateOnTimeout();
};

void KSldTest::initTestCase()
{
    QCoreApplication::setAttribute(Qt::AA_ForceRasterWidgets);
}

void KSldTest::testEstablishGrab()
{
    ScreenLocker::KSldApp ksld;
    ksld.initialize();
    QVERIFY(ksld.establishGrab());
    // grab is established, trying again should succeed as well
    QVERIFY(ksld.establishGrab());

    // let's ungrab
    ksld.doUnlock();

    // to get the grab to fail we need another X client
    // we start another process to perform our grab
    QProcess keyboardGrabber;
    keyboardGrabber.start(QStringLiteral("./keyboardGrabber"));
    QVERIFY(keyboardGrabber.waitForStarted());

    // let's add some delay to be sure that keyboardGrabber has it's X stuff done
    QTest::qWait(100);

    // now grabbing should fail
    QVERIFY(!ksld.establishGrab());

    // let's terminate again
    keyboardGrabber.terminate();
    QVERIFY(keyboardGrabber.waitForFinished());

    // now grabbing should succeed again
    QVERIFY(ksld.establishGrab());

    ksld.doUnlock();

    // now the same with pointer
    QProcess pointerGrabber;
    pointerGrabber.start(QStringLiteral("./pointerGrabber"));
    QVERIFY(pointerGrabber.waitForStarted());

    // let's add some delay to be sure that pointerGrabber has it's X stuff done
    QTest::qWait(100);

    // now grabbing should fail
    QVERIFY(!ksld.establishGrab());

    // let's terminate again
    pointerGrabber.terminate();
    QVERIFY(pointerGrabber.waitForFinished());

    // now grabbing should succeed again
    QVERIFY(ksld.establishGrab());
}

void KSldTest::testActivateOnTimeout()
{
    // this time verifies that the screen gets locked on idle timeout, requires the system to be idle
    ScreenLocker::KSldApp ksld;
    ksld.initialize();

    // we need to modify the idle timeout of KSLD, it's in minutes we cannot wait that long
    if (ksld.idleId() != 0) {
        // remove old Idle id
        KIdleTime::instance()->removeIdleTimeout(ksld.idleId());
    }
    ksld.setIdleId(KIdleTime::instance()->addIdleTimeout(5000));

    QSignalSpy lockStateChangedSpy(&ksld, &ScreenLocker::KSldApp::lockStateChanged);
    QVERIFY(lockStateChangedSpy.isValid());

    // let's wait the double of the idle timeout
    QVERIFY(lockStateChangedSpy.wait(10000));
    QCOMPARE(ksld.lockState(), ScreenLocker::KSldApp::AcquiringLock);

    // let's simulate unlock to get rid of started greeter process
    const auto children = ksld.children();
    for (auto it = children.begin(); it != children.end(); ++it) {
        if (qstrcmp((*it)->metaObject()->className(), "LogindIntegration") != 0) {
            continue;
        }
        QMetaObject::invokeMethod(*it, "requestUnlock");
        break;
    }
    QVERIFY(lockStateChangedSpy.wait());
}

QTEST_MAIN(KSldTest)
#include "ksldtest.moc"
