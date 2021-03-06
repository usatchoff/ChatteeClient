#include "Core.h"
#include "ui_LogFrame.h"
#include "ui_RegFrame.h"
#include "ui_MainWindow.h"
#include "Protocol/Packets/UserRegPacket.h"
#include "Protocol/Packets/UserLogPacket.h"
#include "Protocol/Packets/UserAddMessPacket.h"
#include "Protocol/Packets/UserGetChatPacket.h"
#include "Protocol/Packets/UserGetBngsPacket.h"
#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <cassert>

Core::Core():
    _isPending(true),
    _receivedUsername(0),
    _selectedUsername(0),
    _connection(new QTcpSocket(this))
{
    // In case of failed connection.
    connect(_connection,
        QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
        this, &Core::reactOnDisruption);
    connect(_regFrame.ui->switchToLog, &QPushButton::clicked,
            this, &Core::switchToLog);
    connect(_regFrame.ui->submitButton, &QPushButton::clicked,
            this, &Core::tryRegister);
    connect(_logFrame.ui->switchToReg, &QPushButton::clicked,
            this, &Core::switchToReg);
    connect(_logFrame.ui->submitButton, &QPushButton::clicked,
            this, &Core::tryLogin);
    connect(_mainWindow.ui->listWidget_2, &QListWidget::itemSelectionChanged,
            this, &Core::onUserStateChanged);
    connect(_mainWindow.ui->sendButton, &QPushButton::clicked,
            this, &Core::sendMessage);

    _mainWindow.ui->sendButton->setEnabled(false);
    _logFrame.show();

#ifndef NDEBUG
    qDebug() << "Core created.";
#endif
}

Core::~Core()
{
    _connection->close();
    delete _connection;
    delete _receivedUsername;
    delete _selectedUsername;

#ifndef NDEBUG
    qDebug() << "Core deleted.";
#endif
}

void Core::start(const char* host, const quint16 port)
{
    _connection->abort();
    _connection->connectToHost(host, port);
    _isPending = !_connection->isOpen();

    connect(_connection, &QTcpSocket::readyRead,
            this, &Core::processMessage);
}

void Core::test()
{
    getBindings();
}

void Core::processMessage()
{
    qobject_cast<QTcpSocket*>(sender())->readAll();
}

void Core::reactOnDisruption()
{
    QMessageBox::information(0, "Connection error",
                             "Sorry, the connection is lost\nTry login later");
    _logFrame.close();
    _regFrame.close();
    _mainWindow.close();

#ifndef NDEBUG
    qDebug() << "Connection disrupted.";
#endif
}

void Core::switchToReg()
{
    _regFrame.show();
    _logFrame.close();
    _logFrame.ui->lineEdit->clear();
    _logFrame.ui->lineEdit_2->clear();
}

void Core::switchToLog()
{
    _logFrame.show();
    _regFrame.close();
    _regFrame.ui->lineEdit->clear();
    _regFrame.ui->lineEdit_2->clear();
    _regFrame.ui->lineEdit_3->clear();
    _regFrame.ui->lineEdit_4->clear();
    _regFrame.ui->lineEdit_5->clear();
}

void Core::tryRegister()
{
    // TODO: Validation.
    UserRegPacket packet;
    packet.username() = _regFrame.ui->lineEdit->text();
    packet.bio() = _regFrame.ui->lineEdit_2->text();
    packet.email() = _regFrame.ui->lineEdit_3->text();
    packet.pass() = _regFrame.ui->lineEdit_4->text();
    _connection->write(packet.dump());
    _connection->flush();
    _connection->blockSignals(true);
    _connection->waitForReadyRead();
    if (_connection->readAll().data()[0] == char(1))
    {
        switchToLog();
    }
    else
    {
        QMessageBox::information(0, "Error", "Cannot create a user with such data");
    }
    _connection->blockSignals(false);
}

void Core::tryLogin()
{
    UserLogPacket packet;
    packet.username() = _logFrame.ui->lineEdit->text();
    packet.pass() = _logFrame.ui->lineEdit_2->text();
    _connection->write(packet.dump());
    _connection->flush();
    _connection->blockSignals(true);
    _connection->waitForReadyRead();
    if (_connection->readAll().data()[0] == char(1))
    {
        delete _receivedUsername;
        _receivedUsername = new QString(_logFrame.ui->lineEdit->text());
        _mainWindow.show();
        _logFrame.close();
        getBindings();
    }
    else
    {
        QMessageBox::information(0, "Error", "Invlaid useraname/password");
    }
    _connection->blockSignals(false);
}

void Core::getBindings()
{
    UserGetBngsPacket packet;
    _connection->write(packet.dump());
    _connection->flush();
    _connection->blockSignals(true);
    _connection->waitForReadyRead();

    QByteArray arr = _connection->readAll();
    QDataStream in(&arr, QIODevice::ReadWrite);
    QList<QString> bindings;
    in >> bindings;
    _mainWindow.ui->listWidget_2->addItems(bindings);
    _connection->blockSignals(false);
}

void Core::getMessages()
{
    assert(_selectedUsername);

    UserGetChatPacket packet;
    packet.to() = *_selectedUsername;
    _connection->write(packet.dump());
    _connection->flush();
    _connection->blockSignals(true);
    _connection->waitForReadyRead();

    QByteArray arr = _connection->readAll();
    QDataStream in(&arr, QIODevice::ReadWrite);
    QList<QString> messages;
    in >> messages;
    _mainWindow.ui->listWidget->clear();
    _mainWindow.ui->listWidget->addItems(messages);
    _connection->blockSignals(false);
}

void Core::onUserStateChanged()
{
    _mainWindow.ui->sendButton->setEnabled(true);
    delete _selectedUsername;
    _selectedUsername = new QString(
            _mainWindow.ui->listWidget_2->selectedItems().constFirst()->text());
    getMessages();
}

void Core::sendMessage()
{
    assert(_receivedUsername);
    assert(_selectedUsername);

    QString message = _mainWindow.ui->textEdit->toPlainText();

    UserAddMessPacket packet;
    packet.text() = message;
    packet.to() = *_selectedUsername;
    _connection->write(packet.dump());
    _connection->flush();
    _connection->blockSignals(true);
    _connection->waitForReadyRead();

    if (_connection->readAll().data()[0] == char(1))
    {
        _mainWindow.ui->textEdit->clear();
        _mainWindow.ui->listWidget->addItem((*_receivedUsername) + " ["
            + QDateTime::currentDateTime().toString() + "]\n" + message);
    }
    else
    {
        QMessageBox::information(0, "Connection error", "Unable to send the message");
    }

    _connection->blockSignals(false);
}
