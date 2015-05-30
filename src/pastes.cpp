/*
    kladi, Pastebin application
*/

#include "pastes.h"

Pastes::Pastes(QObject *parent) :
    QObject(parent)
{
    _pastes = QString();
    _apiUrl = "http://pastebin.com/api/api_post.php";
    _userKey = getSetting("userkey", QString()).toString();
    _develKey = DEVELKEY;
    _lastRequest = None;

    _manager = new QNetworkAccessManager(this);

    connect(_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
}

Pastes::~Pastes()
{
}

QVariant Pastes::getSetting(QString name, QVariant defaultValue)
{
    QSettings s("harbour-kladi", "harbour-kladi");
    s.beginGroup("Settings");
    QVariant settingValue = s.value(name, defaultValue);
    s.endGroup();

    return settingValue;
}

void Pastes::setSetting(QString name, QVariant value)
{
    QSettings s("harbour-kladi", "harbour-kladi");
    s.beginGroup("Settings");
    s.setValue(name, value);
    s.endGroup();

    if (name == "userkey")
        _userKey = value.toString();
}

void Pastes::newPaste(QString name, QString code, QString format, QString expire, QString priv)
{
    QString opt = "paste";

    if (format.isEmpty())
        format = "text";

    qDebug() << name;
    qDebug() << code;
    qDebug() << format << expire << priv;

    QNetworkRequest req;
    req.setUrl(QUrl(_apiUrl));

    QString datas("api_option=" + opt +
                  "&api_dev_key=" + _develKey +
                  "&api_user_key=" + _userKey +
                  "&api_paste_name=" + QUrl::toPercentEncoding(name) +
                  "&api_paste_format=" + format +
                  "&api_paste_private=" + priv +
                  "&api_paste_expire_date=" + expire +
                  "&api_paste_code=" + QUrl::toPercentEncoding(code));

    QByteArray data = datas.toLocal8Bit();

    req.setRawHeader("Accept", "text/*");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::ContentLengthHeader, QString::number(data.size()));

    _lastRequest = New;

    QNetworkReply *reply = _manager->post(req, data);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorReply(QNetworkReply::NetworkError)));
}

void Pastes::fetchAll()
{
    QString opt = "list";

    QNetworkRequest req;
    req.setUrl(QUrl(_apiUrl));

    QString datas("api_option=" + opt +
                  "&api_dev_key=" + _develKey +
                  "&api_user_key=" + _userKey);

    QByteArray data = datas.toLocal8Bit();

    req.setRawHeader("Accept", "text/*");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::ContentLengthHeader, QString::number(data.size()));

    _lastRequest = List;

    QNetworkReply *reply = _manager->post(req, data);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorReply(QNetworkReply::NetworkError)));
}

void Pastes::fetchRaw(QString key)
{
    if (key.isEmpty())
        return;

    QUrl url("http://pastebin.com/raw.php");
    QUrlQuery q;

    q.addQueryItem("i", key);

    url.setQuery(q);

    QNetworkRequest req(url);

    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    req.setAttribute(QNetworkRequest::Attribute(QNetworkRequest::User + 1), key);
    QNetworkReply *reply = _manager->get(req);

    _lastRequest = Raw;

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorReply(QNetworkReply::NetworkError)));
}

void Pastes::deletePaste(QString key)
{
    if (key.isEmpty())
        return;

    QString opt = "delete";

    QNetworkRequest req;
    req.setUrl(QUrl(_apiUrl));

    QString datas("api_option=" + opt +
                  "&api_dev_key=" + _develKey +
                  "&api_user_key=" + _userKey +
                  "&api_paste_key=" + key);

    QByteArray data = datas.toLocal8Bit();

    req.setRawHeader("Accept", "text/*");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::ContentLengthHeader, QString::number(data.size()));

    _lastRequest = Delete;

    QNetworkReply *reply = _manager->post(req, data);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorReply(QNetworkReply::NetworkError)));
}

void Pastes::requestUserKey(QString username, QString password)
{
    if (username.isEmpty() || password.isEmpty())
        return;

    QNetworkRequest req;
    req.setUrl(QUrl("http://pastebin.com/api/api_login.php"));

    QString datas("api_dev_key=" + _develKey +
                  "&api_user_name=" + QUrl::toPercentEncoding(username) +
                  "&api_user_password=" + QUrl::toPercentEncoding(password));

    QByteArray data = datas.toLocal8Bit();

    req.setRawHeader("Accept", "text/*");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::ContentLengthHeader, QString::number(data.size()));

    _lastRequest = UserKey;

    QNetworkReply *reply = _manager->post(req, data);

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorReply(QNetworkReply::NetworkError)));
}

/*****************/

void Pastes::finished(QNetworkReply *reply)
{
    QString r(reply->readAll());
    reply->deleteLater();

    qDebug() << "Got reply. Possible error:" << reply->errorString();

    if (r.startsWith("Bad API request"))
    {
        qDebug() << "error";
        _lastRequest = None;

        _message = r;
        emit error();
    }
    else if (r.startsWith("<paste>") && _lastRequest == List)
    {
        qDebug() << "looks like data, list of pastes";
        _lastRequest = None;

        _pastes = QString("<?xml version=\"1.0\" encoding=\"utf-8\"?><data>%1</data>").arg(r);
        emit pastesChanged();
    }
    else if (r.startsWith("http") && _lastRequest == New)
    {
        qDebug() << "this was a URL, so paste success";
        _lastRequest = None;

        _message = r;
        emit success();
    }
    else if (r.length() == 32 && _lastRequest == UserKey)
    {
        qDebug() << "seems to be a user key";
        _lastRequest = None;

        setSetting("userkey", r);

        _message = "New key successfully collected";
        emit success();
    }
    else if (r.startsWith("Paste Removed") && _lastRequest == Delete)
    {
        qDebug() << "deleted";
        _lastRequest = None;

        _message = "Deleted succesfully";
        emit success();
    }
    else if (_lastRequest == Raw)
    {
        qDebug() << "raw" << r;
        _lastRequest = None;

        if (r.isEmpty())
        {
            _message = "This paste is empty?";
            emit error();
        }
        else
        {
           _rawPaste = r;
           emit rawPasteChanged();
        }
    }
}

void Pastes::errorReply(QNetworkReply::NetworkError error)
{
    qCritical() << error << ((QNetworkReply *)sender())->errorString();
}
