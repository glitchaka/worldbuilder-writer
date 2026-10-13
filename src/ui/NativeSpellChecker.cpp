#include "ui/NativeSpellChecker.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <spellcheck.h>
#endif

namespace wbw {
namespace {

#ifdef Q_OS_WIN
QString windowsLocale(QString language) {
    language = language.trimmed();
    if (language.compare(QStringLiteral("es"), Qt::CaseInsensitive) == 0) return QStringLiteral("es-ES");
    if (language.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0) return QStringLiteral("en-US");
    if (language.compare(QStringLiteral("fr"), Qt::CaseInsensitive) == 0) return QStringLiteral("fr-FR");
    if (language.compare(QStringLiteral("de"), Qt::CaseInsensitive) == 0) return QStringLiteral("de-DE");
    if (language.compare(QStringLiteral("pt"), Qt::CaseInsensitive) == 0) return QStringLiteral("pt-BR");
    return language.isEmpty() ? QStringLiteral("es-ES") : language;
}

LPCWSTR wide(const QString& value) {
    return reinterpret_cast<LPCWSTR>(value.utf16());
}
#endif

} // namespace

bool NativeSpellChecker::check(const QString& text, const QString& language, QJsonArray* matches) {
    if (matches) *matches = QJsonArray();
#ifdef Q_OS_WIN
    if (text.trimmed().isEmpty()) return true;

    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ownsCom = init == S_OK || init == S_FALSE;
    if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;

    ISpellCheckerFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        if (ownsCom) CoUninitialize();
        return false;
    }

    QString locale = windowsLocale(language);
    BOOL supported = FALSE;
    hr = factory->IsSupported(wide(locale), &supported);
    if (FAILED(hr) || !supported) {
        const QStringList fallbacks = locale.startsWith(QStringLiteral("es"), Qt::CaseInsensitive)
            ? QStringList{QStringLiteral("es-CL"), QStringLiteral("es-ES")}
            : QStringList{};
        for (const QString& candidate : fallbacks) {
            supported = FALSE;
            if (SUCCEEDED(factory->IsSupported(wide(candidate), &supported)) && supported) {
                locale = candidate;
                break;
            }
        }
    }

    if (!supported) {
        factory->Release();
        if (ownsCom) CoUninitialize();
        return false;
    }

    ISpellChecker* checker = nullptr;
    hr = factory->CreateSpellChecker(wide(locale), &checker);
    factory->Release();
    if (FAILED(hr) || !checker) {
        if (ownsCom) CoUninitialize();
        return false;
    }

    IEnumSpellingError* errors = nullptr;
    hr = checker->Check(wide(text), &errors);
    if (FAILED(hr) || !errors) {
        checker->Release();
        if (ownsCom) CoUninitialize();
        return false;
    }

    QJsonArray result;
    while (true) {
        ISpellingError* error = nullptr;
        const HRESULT next = errors->Next(&error);
        if (next != S_OK || !error) break;

        ULONG start = 0;
        ULONG length = 0;
        error->get_StartIndex(&start);
        error->get_Length(&length);

        QJsonArray replacements;
        const QString misspelled = text.mid(static_cast<int>(start), static_cast<int>(length));
        if (!misspelled.isEmpty()) {
            IEnumString* suggestions = nullptr;
            if (SUCCEEDED(checker->Suggest(wide(misspelled), &suggestions)) && suggestions) {
                for (int i = 0; i < 8; ++i) {
                    LPOLESTR suggestion = nullptr;
                    ULONG fetched = 0;
                    if (suggestions->Next(1, &suggestion, &fetched) != S_OK || fetched == 0 || !suggestion) break;
                    replacements.append(QJsonObject{{QStringLiteral("value"), QString::fromWCharArray(suggestion)}});
                    CoTaskMemFree(suggestion);
                }
                suggestions->Release();
            }
        }

        if (length > 0) {
            result.append(QJsonObject{
                {QStringLiteral("offset"), static_cast<int>(start)},
                {QStringLiteral("length"), static_cast<int>(length)},
                {QStringLiteral("message"), QObject::tr("Posible error ortográfico.")},
                {QStringLiteral("replacements"), replacements}
            });
        }
        error->Release();
    }

    errors->Release();
    checker->Release();
    if (ownsCom) CoUninitialize();
    if (matches) *matches = result;
    return true;
#else
    Q_UNUSED(text)
    Q_UNUSED(language)
    return false;
#endif
}

} // namespace wbw
