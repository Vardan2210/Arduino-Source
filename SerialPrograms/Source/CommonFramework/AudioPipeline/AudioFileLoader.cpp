/*  Audio File Loader
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */


#include "AudioFileLoader.h"
#include "AudioFormatUtils.h"
#include "WavFile.h"

#include <iostream>

#include <QUrl>
#include <QString>
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QTimer>
#include <QEventLoop>

// #define DEBUG_AUDIO_DECODER_WORKER


namespace PokemonAutomation{

AudioFileLoader::AudioFileLoader(QObject* parent, const QString& filename, const QAudioFormat& audioFormat):
    QObject(parent), m_filename(filename), m_audioFormat(audioFormat) {}

AudioFileLoader::~AudioFileLoader(){
    if (m_audioDecoderWorker){
        m_audioDecoderWorker->stop();
    }

    m_audioDecoderThread.quit();
    m_audioDecoderThread.wait();

    if (m_audioDecoderWorker){
        delete m_audioDecoderWorker;
        m_audioDecoderWorker = nullptr;
    }

    if (m_wavFile){
        m_wavFile->close();
        delete m_wavFile;
        m_wavFile = nullptr;
    }
}

bool AudioFileLoader::start(){

    m_frames_per_timeout = m_audioFormat.sampleRate() * m_timer_interval_ms / 1000;

    if (m_filename.toLower().endsWith(".wav")){
        // Use WavFile to load unencoded samples:
        if (!initWavFile()){
            return false;
        }

        buildTimer();
        connect(m_timer, &QTimer::timeout, this, &AudioFileLoader::sendBufferFromWavFileOnTimer);
        
        return true;
    }

    // Use QAudioDecoder to decode compressed audio file:
    m_audioDecoderWorker = new AudioDecoderWorker(this, m_filename, m_audioFormat, m_rawBuffer);

    // When the decoder finishes decoding the entire file, launch a timer to send decoded audio
    // frames to outside at desired frame rate.
    // Note: here we save all the decoded raw samples to memory before sending them out.
    // For a large audio file this is problematic, will consume lots of memory. But for small audio
    // files this is OK.
    connect(m_audioDecoderWorker, &AudioDecoderWorker::finished, this, [&](){
        std::cout << "Audio decoder finishes decoding. Start playing audio at desired sample rate" << std::endl;
        buildTimer();
        connect(m_timer, &QTimer::timeout, this, &AudioFileLoader::sendDecodedBufferOnTimer);
    });

    m_audioDecoderWorker->start();

    return m_audioDecoderWorker->startSucceeded();
}

void AudioFileLoader::buildTimer(){
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->start(m_timer_interval_ms);
}

std::tuple<const char*, size_t> AudioFileLoader::loadFullAudio(){
    if (m_filename.toLower().endsWith(".wav")){
        // Use WavFile to load unencoded samples:
        if (!initWavFile()){
            return std::make_tuple<const char*, size_t>(nullptr, 0);
        }

        const size_t numBytes = m_wavFile->size() - m_wavFile->pos();
        m_rawBuffer.resize(numBytes);
        const int bytesRead = m_wavFile->read(m_rawBuffer.data(), numBytes);
        if (bytesRead != numBytes){
            std::cout << "Error: failed to read all bytes from wav file: " << bytesRead << " < " << numBytes << std::endl;
        }
        return convertRawWavSamples();
    }

    // Use QAudioDecoder to decode compressed audio file:
    m_audioDecoderWorker = new AudioDecoderWorker(nullptr, m_filename, m_audioFormat, m_rawBuffer);
    m_audioDecoderWorker->moveToThread(&m_audioDecoderThread);

    connect(this, &AudioFileLoader::runAudioDecoderAsync, m_audioDecoderWorker, &AudioDecoderWorker::start);
    connect(&m_audioDecoderThread, &QThread::finished, m_audioDecoderWorker, &QObject::deleteLater);

    QEventLoop loop;
    connect(m_audioDecoderWorker, &AudioDecoderWorker::errored, &loop, &QEventLoop::quit);
    connect(m_audioDecoderWorker, &AudioDecoderWorker::finished, &loop, &QEventLoop::quit);

    // m_audioDecoderThread takes care of m_audioDecoderWorker via finished -> deleteLater connection.
    // So we don't hold the pointer m_audioDecoderWorker anymore.
    m_audioDecoderWorker = nullptr;

    m_audioDecoderThread.start();
    emit runAudioDecoderAsync();

    // Block the current thread until the m_audioDecoderWorker errored or finished.
    loop.exec();
    
    m_audioDecoderThread.quit();
    m_audioDecoderThread.wait();
    return std::make_tuple<const char*, size_t>(m_rawBuffer.data(), m_rawBuffer.size());
}

bool AudioFileLoader::initWavFile(){
    std::cout << "Initialize WavFile on " << m_filename.toStdString() << std::endl;
    m_wavFile = new WavFile(this);
    if (m_wavFile->open(m_filename) == false){
        std::cout << "Error: WavFile cannot open file " << m_filename.toStdString() << std::endl;
        return false;
    }

    std::cout << "Wav file audio format: ";
    printAudioFormat(m_wavFile->audioFormat());

    int wavSampleRate = m_wavFile->audioFormat().sampleRate();
    if (wavSampleRate != m_audioFormat.sampleRate()){
        std::cout << "Error: we don't interpolate wav file sample rate " << wavSampleRate << " to " << m_audioFormat.sampleRate() << std::endl;
        return false;
    }

    return true;
}


void AudioFileLoader::sendDecodedBufferOnTimer(){
    const int bytesPerFrame = m_audioFormat.bytesPerFrame();
    const int bytesToSend = bytesPerFrame * m_frames_per_timeout;

    const auto& buffer = m_rawBuffer;

    const size_t m_bufferEnd = std::min(m_bufferNext + bytesToSend, buffer.size());

    const float* floatData = reinterpret_cast<const float*>(buffer.data() + m_bufferNext);
    // std::cout << "Timer: " << floatData[0] << " " << floatData[1] << "... " << buffer.size() << " " << m_bufferNext << std::endl;

    const size_t sentLen = m_bufferEnd - m_bufferNext;
    emit bufferReady(buffer.data() + m_bufferNext, sentLen);

    m_bufferNext = m_bufferEnd;

    if (m_bufferEnd == buffer.size()){
        if (m_timer){
            m_timer->stop();
        }
        emit finished();
    }
}

void AudioFileLoader::sendBufferFromWavFileOnTimer(){
    const QAudioFormat& wavAudioFormat = m_wavFile->audioFormat();
    const int wavBytesPerFrame = wavAudioFormat.bytesPerFrame();
    const int wavBytesToRead = wavBytesPerFrame * m_frames_per_timeout;
    m_rawBuffer.resize(wavBytesToRead);
    const int wavBytesRead = m_wavFile->read(m_rawBuffer.data(), wavBytesToRead);
    m_rawBuffer.resize(wavBytesRead);

    if (wavBytesRead == 0){
        if (m_timer){
            m_timer->stop();
        }
        emit finished();
        return;
    }

    const auto bufferPtrLen = convertRawWavSamples();
    emit bufferReady(std::get<0>(bufferPtrLen), std::get<1>(bufferPtrLen));
}

std::tuple<const char*, size_t> AudioFileLoader::convertRawWavSamples(){
    const QAudioFormat& wavAudioFormat = m_wavFile->audioFormat();

    const int wavBytesRead = m_rawBuffer.size();
    
    // Simple case, the target audio format is the same as the format used in the wav file.
    if (m_audioFormat == wavAudioFormat){
        return std::make_tuple<const char*, size_t>(m_rawBuffer.data(), wavBytesRead);
    }

    // Now we need to convert the audio samples to the target format:

    const int wavBytesPerFrame = wavAudioFormat.bytesPerFrame();
    const int wavNumChannels = wavAudioFormat.channelCount();
    const int wavBytesPerSample = wavBytesPerFrame / wavNumChannels;
    const int framesRead = wavBytesRead / wavBytesPerFrame;
    const int samplesRead = wavBytesRead / wavBytesPerSample;
    
    // m_floatBuffer holds the converted float-type samples
    // TODO: design a general format conversion method
    m_floatBuffer.resize(samplesRead);
    convertSamplesToFloat(wavAudioFormat, m_rawBuffer.data(), wavBytesRead, m_floatBuffer.data());
    
    if (m_audioFormat.channelCount() == wavAudioFormat.channelCount()){
        return std::make_tuple<const char*, size_t>(reinterpret_cast<const char*>(m_floatBuffer.data()),
            m_floatBuffer.size() * sizeof(float));
    }
    
    if(m_audioFormat.channelCount() == 1){
        // Input wav file has stereo or more channels, but output format is mono,
        // average L and R channel samples per frame:
        for(int i = 0; i < framesRead; i++){
            m_floatBuffer[i] = (m_floatBuffer[2*i] + m_floatBuffer[2*i+1]) / 2.0f;
        }
        return std::make_tuple<const char*, size_t>(reinterpret_cast<const char*>(m_floatBuffer.data()), framesRead * sizeof(float));
    }
    
    if(wavAudioFormat.channelCount() == 1){
        // Input wav is mono but output format is stereo,
        // Duplicate samples for each channel:
        m_floatBuffer.resize(samplesRead * m_audioFormat.channelCount());
        for(int i = samplesRead-1; i >= 0; i--){
            const float v = m_floatBuffer[i];
            for(int j = m_audioFormat.channelCount()-1; j >= 0; j--){
                m_floatBuffer[m_audioFormat.channelCount()*i+j] = v;
            }
        }
        return std::make_tuple<const char*, size_t>(reinterpret_cast<const char*>(m_floatBuffer.data()),
            framesRead * m_audioFormat.channelCount() * sizeof(float));
    }
    
    std::cout << "Error format conversion" << std::endl;
    std::cout << "Wav file format: ";
    printAudioFormat(wavAudioFormat);
    std::cout << "Audio format to convert to: ";
    printAudioFormat(m_audioFormat);
    return std::make_tuple<const char*, size_t>(reinterpret_cast<const char*>(m_floatBuffer.data()), 0);
}





AudioDecoderWorker::AudioDecoderWorker(
    QObject* parent,
    const QString& filename,
    const QAudioFormat& audioFormat,
    std::vector<char>& decodedBuffer
):
    QObject(parent), m_filename(filename), m_audioFormat(audioFormat), m_decodedBuffer(decodedBuffer){}


void AudioDecoderWorker::start(){
    m_audioDecoder = new QAudioDecoder(this);
    
    // Whenever a new buffer of audo frames decoded, save them by calling readAudioDecoderBuffer().
    connect(m_audioDecoder, &QAudioDecoder::bufferReady, this, &AudioDecoderWorker::readAudioDecoderBuffer);

    // When the decoder finishes decoding the entire file, launch a timer to send decoded audio
    // frames to outside at desired frame rate.
    // Note: here we save all the decoded raw samples to memory before sending them out.
    // For a large audio file this is problematic, will consume lots of memory. But for small audio
    // files this is OK.
    connect(m_audioDecoder, &QAudioDecoder::finished, this, &AudioDecoderWorker::finished);

    m_audioDecoder->setAudioFormat(m_audioFormat);
    if (m_audioDecoder->error() != QAudioDecoder::NoError){
        handleAudioDecoderError();
        m_startSucceeded = false;
        return;
    }

    // Using Qt5, there may be errors about:
    // defaultServiceProvider::requestService(): no service found for - "org.qt-project.qt.audiodecode"
    // The QAudioDecoder object does not have a valid service
    // For solution, see https://stackoverflow.com/questions/22783381/qaudiodecoder-no-service-found
    connect(m_audioDecoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this, [&](QAudioDecoder::Error){
        handleAudioDecoderError();
    });

    // m_audioDecoder->setSourceDevice(&m_file);
#if QT_VERSION_MAJOR == 5
    m_audioDecoder->setSourceFilename(m_filename);
#elif QT_VERSION_MAJOR == 6
    m_audioDecoder->setSource(QUrl::fromLocalFile(m_filename));
#endif

    m_audioDecoder->start();
    std::cout << "Start audio decoder." << std::endl;

    m_startSucceeded = true;
    return;
}

AudioDecoderWorker::~AudioDecoderWorker(){
    stop();
}

void AudioDecoderWorker::stop(){
    if (m_audioDecoder){
        m_audioDecoder->stop();
    }
}

void AudioDecoderWorker::readAudioDecoderBuffer(){
    if (m_audioDecoder == nullptr){
        return;
    }

    const QAudioBuffer& buffer = m_audioDecoder->read();
    size_t oldSize = m_decodedBuffer.size();
    m_decodedBuffer.resize(oldSize + buffer.byteCount());
    memcpy(m_decodedBuffer.data() + oldSize, buffer.data<char>(), buffer.byteCount());

#ifdef DEBUG_AUDIO_DECODER_WORKER
    std::cout << "Read QAudioBuffer (size " << buffer.byteCount() << ") from audio decoder, cur pos " <<
        m_audioDecoder->position() << " ms, total length " << m_audioDecoder->duration() << " ms, " << 
        "buffer length now " << m_decodedBuffer.size() << std::endl;
#endif
}


void AudioDecoderWorker::handleAudioDecoderError(){
    if (m_audioDecoder){
        QAudioDecoder::Error error = m_audioDecoder->error();
        std::string errorStr;
        switch(error){
        case QAudioDecoder::ResourceError:
            errorStr = "ResourceError";
            break;
        case QAudioDecoder::AccessDeniedError:
            errorStr = "AccessDeniedError";
            break;
        case QAudioDecoder::FormatError:
            errorStr = "FormatError";
            break;
#if QT_VERSION_MAJOR == 5
        case QAudioDecoder::ServiceMissingError:
            errorStr = "ServiceMissingError";
#elif QT_VERSION_MAJOR == 6
        case QAudioDecoder::NotSupportedError:
            errorStr = "NotSupportedError";
            break;
#endif
        default:
            errorStr = "UnownError";
        }

        std::cout << "QAudioDecoder error when loading file: " << m_filename.toStdString() << ", " <<
            errorStr << ": " << m_audioDecoder->errorString().toStdString() << std::endl;

        emit errored();
    }
}

}