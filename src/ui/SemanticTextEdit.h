#pragma once

#include <QJsonArray>
#include <QTextEdit>
#include <QVector>

class QContextMenuEvent;
class QMenu;
class QMouseEvent;
class QNetworkAccessManager;
class QNetworkReply;

namespace wbw {

class ArchiveDocument;

class SemanticTextEdit final : public QTextEdit {
    Q_OBJECT
public:
    explicit SemanticTextEdit(QWidget* parent = nullptr);

    void setArchiveDocument(ArchiveDocument* document);
    void refreshSemanticReferences();
    void runProofread();
    void clearProofread();

signals:
    void referenceActivated(const QString& kind, const QString& id);
    void referenceIndexChanged(const QJsonArray& references);
    void proofreadStarted();
    void proofreadFinished(int issueCount);
    void proofreadError(const QString& message);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct ReferenceHit {
        int start = 0;
        int length = 0;
        QString kind;
        QString id;
        QString label;
    };

    struct ProofIssue {
        int start = 0;
        int length = 0;
        QString message;
        QStringList replacements;
    };

    void rebuildSelections();
    const ReferenceHit* referenceAt(int position) const;
    const ProofIssue* proofIssueAt(int position) const;
    QJsonObject objectForReference(const ReferenceHit& hit) const;
    void showReferencePopup(const ReferenceHit& hit, const QPoint& globalPosition);
    void closeReferencePopup();
    void applyReplacement(const ProofIssue& issue, const QString& replacement);
    void parseProofreadReply(QNetworkReply* reply);

    ArchiveDocument* document_ = nullptr;
    QNetworkAccessManager* network_ = nullptr;
    QVector<ReferenceHit> references_;
    QVector<ProofIssue> proofIssues_;
    QMenu* referencePopup_ = nullptr;
    QString referencePopupKey_;
    int proofreadBase_ = 0;
};

} // namespace wbw
