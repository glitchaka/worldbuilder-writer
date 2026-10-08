#pragma once

#include <QWidget>

class QAudioOutput;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QMediaPlayer;
class QSpinBox;
class QTabWidget;
class QTextEdit;
class QVideoWidget;

namespace wbw {

class ArchiveDocument;

class ReviewPage final : public QWidget {
    Q_OBJECT
public:
    explicit ReviewPage(QWidget* parent = nullptr);

    void setDocument(ArchiveDocument* document);
    void refresh();

signals:
    void changed();
    void requestExportPdf();
    void requestExportWbw();
    void requestBackup();

private:
    QWidget* buildProjectTab();
    QWidget* buildAnalysisTab();
    QWidget* buildLayoutTab();
    QWidget* buildMediaTab();

    void applyProfile();
    void chooseProfileImage(const QString& key);
    void clearProfileImage(const QString& key);
    void runAnalysis();
    void applyAnalysisSettings();
    void applyLayout();
    void applyLayoutPreset(const QString& preset);
    void applyMediaUrls();
    void addLocalMedia();
    void removeLocalMedia();
    void playSelectedMedia();
    void stopMedia();
    void openExternalUrl(const QString& url);

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    QTabWidget* tabs_ = nullptr;

    QLineEdit* archiveTitle_ = nullptr;
    QLineEdit* storyTitle_ = nullptr;
    QLineEdit* subtitle_ = nullptr;
    QLineEdit* projectLabel_ = nullptr;
    QLineEdit* homeHeading_ = nullptr;
    QLineEdit* location_ = nullptr;
    QLineEdit* author_ = nullptr;
    QLineEdit* genre_ = nullptr;
    QLineEdit* projectStatus_ = nullptr;
    QTextEdit* synopsis_ = nullptr;
    QLineEdit* chapterLabels_ = nullptr;
    QComboBox* theme_ = nullptr;
    QLabel* coverPreview_ = nullptr;
    QLabel* bannerPreview_ = nullptr;
    QLabel* iconPreview_ = nullptr;

    QSpinBox* repetitionWindow_ = nullptr;
    QTextEdit* fillerPhrases_ = nullptr;
    QListWidget* analysisResults_ = nullptr;
    QLabel* analysisSummary_ = nullptr;

    QComboBox* layoutPreset_ = nullptr;
    QDoubleSpinBox* pageWidth_ = nullptr;
    QDoubleSpinBox* pageHeight_ = nullptr;
    QDoubleSpinBox* marginTop_ = nullptr;
    QDoubleSpinBox* marginRight_ = nullptr;
    QDoubleSpinBox* marginBottom_ = nullptr;
    QDoubleSpinBox* marginLeft_ = nullptr;
    QComboBox* fontFamily_ = nullptr;
    QDoubleSpinBox* fontSize_ = nullptr;
    QDoubleSpinBox* lineHeight_ = nullptr;
    QDoubleSpinBox* paragraphIndent_ = nullptr;
    QComboBox* chapterOpening_ = nullptr;
    QLineEdit* sceneSeparator_ = nullptr;
    QLineEdit* headerText_ = nullptr;
    QLineEdit* footerText_ = nullptr;

    QLineEdit* spotifyUrl_ = nullptr;
    QLineEdit* youtubeUrl_ = nullptr;
    QListWidget* mediaList_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    QVideoWidget* videoWidget_ = nullptr;
};

} // namespace wbw
