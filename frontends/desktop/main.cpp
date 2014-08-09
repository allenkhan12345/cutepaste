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
#include <QJsonParseError>
#include <QFile>
#include <QScopedPointer>
#include <QTextStream>
#include <QStringList>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>

int main(int argc, char **argv)
{
    QCoreApplication application{argc, argv};
    application.setOrganizationName("CutePaste");
    application.setApplicationName("CutePaste Desktop Console Frontend");

    QCommandLineParser parser;
    parser.setApplicationDescription("CutePaste");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Content of file to send to online paste service.");

    QCommandLineOption langOption(QStringList() << "l" << "lang", "Set language like text, diff, cpp, etc. Default is text.", "language", "text");
    parser.addOption(langOption);
    QCommandLineOption pasteServiceOption(QStringList() << "p" << "pasteService", "Where should the paste be send to. Defaults to http://pastebin.kde.org", "url", "http://pastebin.kde.org");
    parser.addOption(pasteServiceOption);
    parser.process(application);
    const QStringList args = parser.positionalArguments();

    QTextStream standardOutputStream{stdout};
    QFile dataFile;
    if (args.count() > 0 && QFile::exists(args.at(0))) {
        dataFile.setFileName(args.at(0));
        dataFile.open(QIODevice::ReadOnly);
    } else {
        dataFile.open(stdin, QIODevice::ReadOnly);
    }

    QByteArray pasteTextByteArray{dataFile.readAll().trimmed()};

    QJsonObject requestJsonObject;
    requestJsonObject.insert(QStringLiteral("data"), QString::fromUtf8(pasteTextByteArray));
    requestJsonObject.insert(QStringLiteral("language"), parser.value(langOption));

    QJsonDocument requestJsonDocument{requestJsonObject};

    QString baseUrlString{parser.value(pasteServiceOption)};

    // If the -p (or --pasteService) option isn't provided, but the env variable CUTEPASTE_PASTE_SERVICE is, use that instead.
    QByteArray serviceFromEnv = qgetenv("CUTEPASTE_PASTE_SERVICE").trimmed();
    if(!parser.isSet(pasteServiceOption) && !serviceFromEnv.isEmpty()){
        baseUrlString = serviceFromEnv;
    }

    QNetworkRequest networkRequest;
    networkRequest.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setUrl(QUrl(baseUrlString + "/api/json/create"));

    QNetworkAccessManager networkAccessManager;
    QScopedPointer<QNetworkReply> networkReplyScopedPointer(networkAccessManager.post(networkRequest, requestJsonDocument.toJson()));
    QObject::connect(networkReplyScopedPointer.data(), &QNetworkReply::finished, [&] {

        QJsonParseError jsonParseError;
        QByteArray replyJsonByteArray{networkReplyScopedPointer->readAll()};
        QJsonDocument replyJsonDocument{QJsonDocument::fromJson(replyJsonByteArray, &jsonParseError)};
        if (jsonParseError.error != QJsonParseError::NoError) {
            qDebug() << "The json network reply is not valid json:" << jsonParseError.errorString();
            QCoreApplication::quit();
        }

        if (!replyJsonDocument.isObject()) {
            qDebug() << "The json network reply is not an object";
            QCoreApplication::quit();
        }
        
        QJsonObject replyJsonObject{replyJsonDocument.object()};
        QJsonValue resultValue{replyJsonObject.value(QStringLiteral("result"))};

        if (!resultValue.isObject()) {
            qDebug() << QString("The json network reply does not contain an object for the %1 key").arg(result);
            QCoreApplication::quit();
        }

        QJsonValue identifierValue{resultValue.toObject().value(QStringLiteral("id"))};

        if (!identifierValue.isString()) {
            qDebug() << QString("The json network reply does not contain a string for the %1 key").arg(id);
            QCoreApplication::quit();
        }

        endl(standardOutputStream << baseUrlString << '/' << identifierValue.toString());

        QCoreApplication::quit();
    });

    QObject::connect(networkReplyScopedPointer.data(), static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), [&](QNetworkReply::NetworkError networkReplyError) {
        if (networkReplyError != QNetworkReply::NoError)
            endl(standardOutputStream << networkReplyScopedPointer->errorString());
    });

    QObject::connect(networkReplyScopedPointer.data(), &QNetworkReply::sslErrors, [&](QList<QSslError> networkReplySslErrors) {
        if (!networkReplySslErrors.isEmpty()) {
            for (const auto &networkReplySslError : networkReplySslErrors)
                endl(standardOutputStream << networkReplySslError.errorString());
        }
    });

    int returnValue{application.exec()};

    dataFile.close();
    if (dataFile.error() != QFileDevice::NoError)
        endl(standardOutputStream << dataFile.errorString());

    return returnValue;
}
