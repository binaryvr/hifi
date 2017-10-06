//
//  Created by Bradley Austin Davis on 2015/11/13
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RecordingScriptingInterface.h"

#include <QStandardPaths>
#include <QtCore/QThread>
#include <QtCore/QUrl>
#include <QtScript/QScriptValue>
#include <QtWidgets/QFileDialog>

#include <shared/QtHelpers.h>
#include <AssetClient.h>
#include <AssetUpload.h>
#include <BuildInfo.h>
#include <NumericalConstants.h>
#include <PathUtils.h>
#include <Transform.h>
#include <recording/Deck.h>
#include <recording/Recorder.h>
#include <recording/Clip.h>
#include <recording/Frame.h>
#include <recording/ClipCache.h>

#include "ScriptEngineLogging.h"

using namespace recording;

static const QString HFR_EXTENSION = "hfr";

RecordingScriptingInterface::RecordingScriptingInterface() {
    _player = DependencyManager::get<Deck>();
    _recorder = DependencyManager::get<Recorder>();
}

bool RecordingScriptingInterface::isPlaying() const {
    return _player->isPlaying();
}

bool RecordingScriptingInterface::isPaused() const {
    return _player->isPaused();
}

float RecordingScriptingInterface::playerElapsed() const {
    return _player->position();
}

float RecordingScriptingInterface::playerLength() const {
    return _player->length();
}

void RecordingScriptingInterface::playClip(NetworkClipLoaderPointer clipLoader, const QString& url, QScriptValue callback) {
    _player->queueClip(clipLoader->getClip());

    if (callback.isFunction()) {
        QScriptValueList args { true, url };
        callback.call(_scriptEngine->globalObject(), args);
    }
}

void RecordingScriptingInterface::loadRecording(const QString& url, QScriptValue callback) {
    auto clipLoader = DependencyManager::get<recording::ClipCache>()->getClipLoader(url);

    if (clipLoader->isLoaded()) {
        qCDebug(scriptengine) << "Recording already loaded from" << url;
        playClip(clipLoader, url, callback);
        return;
    }

    // hold a strong pointer to the loading clip so that it has a chance to load
    _clipLoaders.insert(clipLoader);

    auto weakClipLoader = clipLoader.toWeakRef();

    // when clip loaded, call the callback with the URL and success boolean
    connect(clipLoader.data(), &recording::NetworkClipLoader::clipLoaded, this,
            [this, weakClipLoader, url, callback]() mutable {

        if (auto clipLoader = weakClipLoader.toStrongRef()) {
            qCDebug(scriptengine) << "Loaded recording from" << url;

            playClip(clipLoader, url, callback);

            // drop our strong pointer to this clip so it is cleaned up
            _clipLoaders.remove(clipLoader);
        }
    });

    // when clip load fails, call the callback with the URL and failure boolean
    connect(clipLoader.data(), &recording::NetworkClipLoader::failed, this, [this, weakClipLoader, url, callback](QNetworkReply::NetworkError error) mutable {
        qCDebug(scriptengine) << "Failed to load recording from" << url;

        if (callback.isFunction()) {
            QScriptValueList args { false, url };
            callback.call(_scriptEngine->currentContext()->thisObject(), args);
        }

        if (auto clipLoader = weakClipLoader.toStrongRef()) {
            // drop out strong pointer to this clip so it is cleaned up
            _clipLoaders.remove(clipLoader);
        }
    });
}

void RecordingScriptingInterface::startPlaying() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "startPlaying");
        return;
    }

    _player->play();
}

void RecordingScriptingInterface::setPlayerVolume(float volume) {
    // FIXME 
}

void RecordingScriptingInterface::setPlayerAudioOffset(float audioOffset) {
    // FIXME 
}

void RecordingScriptingInterface::setPlayerTime(float time) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "setPlayerTime", Q_ARG(float, time));
        return;
    }
    _player->seek(time);
}

void RecordingScriptingInterface::setPlayFromCurrentLocation(bool playFromCurrentLocation) {
    _playFromCurrentLocation = playFromCurrentLocation;
}

void RecordingScriptingInterface::setPlayerLoop(bool loop) {
    _player->loop(loop);
}

void RecordingScriptingInterface::setPlayerUseDisplayName(bool useDisplayName) {
    _useDisplayName = useDisplayName;
}

void RecordingScriptingInterface::setPlayerUseAttachments(bool useAttachments) {
    _useAttachments = useAttachments;
}

void RecordingScriptingInterface::setPlayerUseHeadModel(bool useHeadModel) {
    _useHeadModel = useHeadModel;
}

void RecordingScriptingInterface::setPlayerUseSkeletonModel(bool useSkeletonModel) {
    _useSkeletonModel = useSkeletonModel;
}

void RecordingScriptingInterface::pausePlayer() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "pausePlayer");
        return;
    }
    _player->pause();
}

void RecordingScriptingInterface::stopPlaying() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "stopPlaying");
        return;
    }
    _player->stop();
}

bool RecordingScriptingInterface::isRecording() const {
    return _recorder->isRecording();
}

float RecordingScriptingInterface::recorderElapsed() const {
    return _recorder->position();
}

void RecordingScriptingInterface::startRecording() {
    if (_recorder->isRecording()) {
        qCWarning(scriptengine) << "Recorder is already running";
        return;
    }

    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "startRecording");
        return;
    }

    _recorder->start();
}

void RecordingScriptingInterface::stopRecording() {
    _recorder->stop();
    _lastClip = _recorder->getClip();
    _lastClip->seek(0);
}

QString RecordingScriptingInterface::getDefaultRecordingSaveDirectory() {
    QString directory = PathUtils::getAppLocalDataPath() + "Avatar Recordings/";
    if (!QDir(directory).exists()) {
        QDir().mkdir(directory);
    }
    return directory;
}

void RecordingScriptingInterface::saveRecording(const QString& filename) {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "saveRecording",
            Q_ARG(QString, filename));
        return;
    }

    if (!_lastClip) {
        qWarning() << "There is no recording to save";
        return;
    }

    recording::Clip::toFile(filename, _lastClip);
}

bool RecordingScriptingInterface::saveRecordingToAsset(QScriptValue getClipAtpUrl) {
    if (!getClipAtpUrl.isFunction()) {
        qCWarning(scriptengine) << "The argument is not a function.";
        return false;
    }

    if (QThread::currentThread() != thread()) {
        bool result;
        BLOCKING_INVOKE_METHOD(this, "saveRecordingToAsset",
            Q_RETURN_ARG(bool, result),
            Q_ARG(QScriptValue, getClipAtpUrl));
        return result;
    }

    if (!_lastClip) {
        qWarning() << "There is no recording to save";
        return false;
    }

    if (auto upload = DependencyManager::get<AssetClient>()->createUpload(recording::Clip::toBuffer(_lastClip))) {
        QObject::connect(upload, &AssetUpload::finished, this, [=](AssetUpload* upload, const QString& hash) mutable {
            QString clip_atp_url = "";

            if (upload->getError() == AssetUpload::NoError) {

                clip_atp_url = QString("%1:%2").arg(URL_SCHEME_ATP, hash);
                upload->deleteLater();
            } else {
                qCWarning(scriptengine) << "Error during the Asset upload.";
            }

            QScriptValueList args;
            args << clip_atp_url;
            getClipAtpUrl.call(QScriptValue(), args);
        });
        upload->start();
        return true;
    }

    qCWarning(scriptengine) << "Saving on asset failed.";
    return false;
}

void RecordingScriptingInterface::loadLastRecording() {
    if (QThread::currentThread() != thread()) {
        BLOCKING_INVOKE_METHOD(this, "loadLastRecording");
        return;
    }

    if (!_lastClip) {
        qCDebug(scriptengine) << "There is no recording to load";
        return;
    }

    _player->queueClip(_lastClip);
    _player->play();
}

