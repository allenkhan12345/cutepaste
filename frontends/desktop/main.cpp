/******************************************************************************
 * This file is part of the CutePaste project
 * Copyright (c) 2013 Laszlo Papp <lpapp@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <QSslError>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTcpSocket>

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QScopedPointer>
#include <QTextStream>
#include <QStringList>
#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    application.setOrganizationName("CutePaste");
    application.setApplicationName("CutePaste Desktop Console Frontend");

    QTextStream standardOutputStream(stdout);
    QFile dataFile;
    QString firstArgument = QCoreApplication::arguments().at(1);
    if (!firstArgument.isEmpty()) {
        dataFile.setFileName(firstArgument);
        dataFile.open(QIODevice::ReadOnly);
    } else {
        dataFile.open(stdin, QIODevice::ReadOnly);
    }

    QByteArray pasteTextByteArray = dataFile.readAll();

    QJsonDocument requestJsonDocument;
    QJsonObject requestJsonObject;

    requestJsonObject.insert(QStringLiteral("data"), QString::fromUtf8(pasteTextByteArray));
    requestJsonObject.insert(QStringLiteral("language"), QStringLiteral("Text"));

    QNetworkRequest networkRequest;
    networkRequest.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setUrl(QUrl("http://sandbox.pastebin.kde.org/api/json/create"));

    QNetworkAccessManager networkAccessManager;
    QScopedPointer<QNetworkReply> networkReplyScopedPointer(networkAccessManager.post(networkRequest, &dataFile));
    QObject::connect(networkReplyScopedPointer.data(), &QNetworkReply::finished, [&]() {
        QJsonDocument replyJsonDocument = QJsonDocument::fromJson(networkReplyScopedPointer->readAll());
        if (!replyJsonDocument.isObject())
            return;
        
        QJsonObject replyJsonObject = replyJsonDocument.object();
        QJsonValue identifierValue = replyJsonObject.value(QStringLiteral("id"));

        if (identifierValue.isString())
            endl(standardOutputStream << identifierValue.toString());
    });

    QObject::connect(networkReplyScopedPointer.data(), static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), [&](QNetworkReply::NetworkError networkReplyError) {
        if (networkReplyError != QNetworkReply::NoError)
            endl(standardOutputStream << networkReplyScopedPointer->errorString());
    });

    QObject::connect(networkReplyScopedPointer.data(), &QNetworkReply::sslErrors, [&](QList<QSslError> networkReplySslErrors) {
        if (!networkReplySslErrors.isEmpty()) {
            foreach (const QSslError &networkReplySslError, networkReplySslErrors)
                endl(standardOutputStream << networkReplySslError.errorString());
        }
    });

    bool returnValue = application.exec();

    dataFile.close();
    if (dataFile.error() != QFileDevice::NoError)
        endl(standardOutputStream << dataFile.errorString());

    return returnValue;
}
