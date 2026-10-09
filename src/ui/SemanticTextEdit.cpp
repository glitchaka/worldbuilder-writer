#include "ui/SemanticTextEdit.h"

#include "core/ArchiveDocument.h"

#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopeGuard>
#include <QSet>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextFormat>
#include <QUrl>
#include <QUrlQuery>

namespace wbw {
namespace {

QStringList aliasesFor(const QJsonObject& object) {
    QStringList result;
    const QJsonValue aliases = object.value(QStringLiteral("aliases"));
    if (aliases.isArray()) {
        for (const QJsonValue value : aliases.toArray()) {
            const QString alias = value.toString().trimmed();
            if (!alias.isEmpty()) result.append(alias);
        }
    } else if (aliases.isString()) {
        for (QString alias : aliases.toString().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            alias = alias.trimmed();
            if (!alias.isEmpty()) result.append(alias);
        }
    }
    return result;
}

bool boundary(const QString& text, int index) {
    if (index < 0 || index >= text.size()) return true;
    const QChar ch = text.at(index);
    return !(ch.isLetterOrNumber() || ch == QLatin1Char('_'));
}

} // namespace

SemanticTextEdit::SemanticTextEdit(QWidget* parent)
    : QTextEdit(parent), network_(new QNetworkAccessManager(this)) {
    setMouseTracking(true);
}

void SemanticTextEdit::setArchiveDocument(ArchiveDocument* document) {
    document_ = document;
    refreshSemanticReferences();
}

void SemanticTextEdit::refreshSemanticReferences() {
    references_.clear();
    if (!document_) {
        rebuildSelections();
        emit referenceIndexChanged(QJsonArray());
        return;
    }

    const QString plain = toPlainText();
    auto collect = [&](const QString& arrayKey, const QString& kind, const QString& labelKey) {
        for (const QJsonValue value : document_->array(arrayKey)) {
            const QJsonObject object = value.toObject();
            const QString id = object.value(QStringLiteral("id")).toString();
            QStringList labels;
            const QString primary = object.value(labelKey).toString().trimmed();
            if (!primary.isEmpty()) labels.append(primary);
            labels.append(aliasesFor(object));
            labels.removeDuplicates();
            for (const QString& label : labels) {
                if (label.size() < 2) continue;
                int from = 0;
                while (from < plain.size()) {
                    const int pos = plain.indexOf(label, from, Qt::CaseInsensitive);
                    if (pos < 0) break;
                    const int end = pos + label.size();
                    if (boundary(plain, pos - 1) && boundary(plain, end)) {
                        references_.append({pos, label.size(), kind, id, label});
                    }
                    from = qMax(end, pos + 1);
                }
            }
        }
    };

    collect(QStringLiteral("characters"), QStringLiteral("character"), QStringLiteral("name"));
    collect(QStringLiteral("world"), QStringLiteral("world"), QStringLiteral("name"));
    collect(QStringLiteral("magicSystems"), QStringLiteral("magic"), QStringLiteral("name"));
    collect(QStringLiteral("worldTexts"), QStringLiteral("worldText"), QStringLiteral("title"));
    collect(QStringLiteral("magicTexts"), QStringLiteral("magicText"), QStringLiteral("title"));

    std::sort(references_.begin(), references_.end(), [](const ReferenceHit& a, const ReferenceHit& b) {
        if (a.start != b.start) return a.start < b.start;
        return a.length > b.length;
    });

    QVector<ReferenceHit> filtered;
    int occupiedUntil = -1;
    for (const ReferenceHit& hit : references_) {
        if (hit.start < occupiedUntil) continue;
        filtered.append(hit);
        occupiedUntil = hit.start + hit.length;
    }
    references_ = filtered;
    rebuildSelections();

    QJsonArray index;
    QSet<QString> seen;
    for (const ReferenceHit& hit : references_) {
        const QString key = hit.kind + QLatin1Char('|') + hit.id;
        if (seen.contains(key)) continue;
        seen.insert(key);
        index.append(QJsonObject{
            {QStringLiteral("label"), hit.label},
            {QStringLiteral("kind"), hit.kind},
            {QStringLiteral("id"), hit.id}
        });
    }
    emit referenceIndexChanged(index);
}

void SemanticTextEdit::runProofread() {
    const QString text = textCursor().hasSelection() ? textCursor().selectedText() : toPlainText();
    if (text.trimmed().isEmpty()) {
        emit proofreadError(tr("Escribe o selecciona un fragmento para revisar."));
        return;
    }

    emit proofreadStarted();
    QNetworkRequest request(QUrl(QStringLiteral("https://api.languagetool.org/v2/check")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("text"), text.left(18000));
    query.addQueryItem(QStringLiteral("language"), QStringLiteral("es"));
    query.addQueryItem(QStringLiteral("enabledOnly"), QStringLiteral("false"));
    QNetworkReply* reply = network_->post(request, query.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { parseProofreadReply(reply); });
}

void SemanticTextEdit::clearProofread() {
    proofIssues_.clear();
    rebuildSelections();
}

void SemanticTextEdit::rebuildSelections() {
    QList<QTextEdit::ExtraSelection> selections;

    for (const ReferenceHit& hit : references_) {
        QTextCursor cursor(document());
        cursor.setPosition(hit.start);
        cursor.setPosition(hit.start + hit.length, QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        selection.format.setForeground(QColor(QStringLiteral("#175cd3")));
        selection.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        selection.format.setUnderlineColor(QColor(QStringLiteral("#175cd3")));
        selections.append(selection);
    }

    for (const ProofIssue& issue : proofIssues_) {
        QTextCursor cursor(document());
        cursor.setPosition(issue.start);
        cursor.setPosition(issue.start + issue.length, QTextCursor::KeepAnchor);
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        selection.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        selection.format.setUnderlineColor(QColor(QStringLiteral("#c62828")));
        selections.append(selection);
    }

    setExtraSelections(selections);
}

const SemanticTextEdit::ReferenceHit* SemanticTextEdit::referenceAt(int position) const {
    for (const ReferenceHit& hit : references_) {
        if (position >= hit.start && position <= hit.start + hit.length) return &hit;
    }
    return nullptr;
}

const SemanticTextEdit::ProofIssue* SemanticTextEdit::proofIssueAt(int position) const {
    for (const ProofIssue& issue : proofIssues_) {
        if (position >= issue.start && position <= issue.start + issue.length) return &issue;
    }
    return nullptr;
}

void SemanticTextEdit::mouseReleaseEvent(QMouseEvent* event) {
    QTextEdit::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton) return;
    const int position = cursorForPosition(event->position().toPoint()).position();
    if (const ReferenceHit* hit = referenceAt(position)) emit referenceActivated(hit->kind, hit->id);
}

void SemanticTextEdit::contextMenuEvent(QContextMenuEvent* event) {
    QMenu* menu = createStandardContextMenu();
    const int position = cursorForPosition(event->pos()).position();
    if (const ReferenceHit* hit = referenceAt(position)) {
        menu->insertSeparator(menu->actions().isEmpty() ? nullptr : menu->actions().first());
        QAction* open = new QAction(tr("Abrir ficha: %1").arg(hit->label), menu);
        connect(open, &QAction::triggered, this, [this, hit]() { emit referenceActivated(hit->kind, hit->id); });
        menu->insertAction(menu->actions().isEmpty() ? nullptr : menu->actions().first(), open);
    }
    if (const ProofIssue* issue = proofIssueAt(position)) {
        menu->addSeparator();
        QAction* info = menu->addAction(issue->message);
        info->setEnabled(false);
        for (const QString& replacement : issue->replacements.mid(0, 8)) {
            QAction* action = menu->addAction(tr("Reemplazar por “%1”").arg(replacement));
            const ProofIssue copy = *issue;
            connect(action, &QAction::triggered, this, [this, copy, replacement]() { applyReplacement(copy, replacement); });
        }
    }
    menu->exec(event->globalPos());
    delete menu;
}

void SemanticTextEdit::applyReplacement(const ProofIssue& issue, const QString& replacement) {
    QTextCursor cursor(document());
    cursor.setPosition(issue.start);
    cursor.setPosition(issue.start + issue.length, QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
    proofIssues_.clear();
    refreshSemanticReferences();
}

void SemanticTextEdit::parseProofreadReply(QNetworkReply* reply) {
    const auto cleanup = qScopeGuard([reply]() { reply->deleteLater(); });
    if (reply->error() != QNetworkReply::NoError) {
        emit proofreadError(tr("LanguageTool no respondió. Comprueba la conexión e inténtalo de nuevo."));
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(reply->readAll());
    if (!json.isObject()) {
        emit proofreadError(tr("LanguageTool devolvió una respuesta inválida."));
        return;
    }

    proofIssues_.clear();
    for (const QJsonValue value : json.object().value(QStringLiteral("matches")).toArray()) {
        const QJsonObject match = value.toObject();
        ProofIssue issue;
        issue.start = match.value(QStringLiteral("offset")).toInt();
        issue.length = match.value(QStringLiteral("length")).toInt();
        issue.message = match.value(QStringLiteral("message")).toString();
        for (const QJsonValue replacement : match.value(QStringLiteral("replacements")).toArray()) {
            const QString text = replacement.toObject().value(QStringLiteral("value")).toString();
            if (!text.isEmpty()) issue.replacements.append(text);
        }
        if (issue.length > 0) proofIssues_.append(issue);
    }
    rebuildSelections();
    emit proofreadFinished(proofIssues_.size());
}

} // namespace wbw
