#include "ui/MapSelectionController.h"

#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QPainter>
#include <QPointer>
#include <QShortcut>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {
namespace {

QToolButton* command(QWidget* parent,const QString& text,const QString& tip){auto* b=new QToolButton(parent);b->setObjectName(QStringLiteral("pilinCommand"));b->setText(text);b->setToolTip(tip);b->setMinimumSize(32,30);b->setCursor(Qt::PointingHandCursor);return b;}

QString objectCaption(const QJsonObject& object){
    const QString type=object.value(QStringLiteral("type")).toString();QString kind;
    if(type==QStringLiteral("land"))kind=QObject::tr("Terreno");else if(type==QStringLiteral("sea"))kind=QObject::tr("Mar");else if(type==QStringLiteral("river"))kind=QObject::tr("Río");else if(type==QStringLiteral("road"))kind=QObject::tr("Camino");else if(type==QStringLiteral("border"))kind=QObject::tr("Frontera");else if(type==QStringLiteral("region"))kind=QObject::tr("Región");else if(type==QStringLiteral("forestArea"))kind=QObject::tr("Bosque");else if(type==QStringLiteral("mountainArea"))kind=QObject::tr("Montañas");else if(type==QStringLiteral("settlement"))kind=QObject::tr("Asentamiento");else if(type==QStringLiteral("stamp"))kind=QObject::tr("Asset");else if(type==QStringLiteral("label"))kind=QObject::tr("Etiqueta");else kind=type.isEmpty()?QObject::tr("Objeto"):type;
    QString name=object.value(QStringLiteral("label")).toString().trimmed();if(name.isEmpty())name=object.value(QStringLiteral("text")).toString().trimmed();if(name.isEmpty())name=object.value(QStringLiteral("assetKind")).toString().trimmed();return name.isEmpty()?kind:QStringLiteral("%1 · %2").arg(kind,name);
}

QFrame* layersPanel(PilinReyEditor* editor){if(!editor)return nullptr;for(QFrame* frame:editor->findChildren<QFrame*>(QStringLiteral("pilinPopover")))for(QLabel* label:frame->findChildren<QLabel*>())if(label->text().trimmed()==QObject::tr("Capas"))return frame;return nullptr;}

QJsonObject objectById(PilinReyEditor* editor,const QString& id){
    if(!editor||id.isEmpty())return {};
    const QJsonArray layers=editor->map().value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
    for(const QJsonValue& lv:layers)for(const QJsonValue& ov:lv.toObject().value(QStringLiteral("objects")).toArray()){const QJsonObject o=ov.toObject();if(o.value(QStringLiteral("id")).toString()==id)return o;}
    return {};
}

void replaceObject(PilinReyEditor* editor,const QString& id,const QJsonObject& replacement){
    if(!editor||id.isEmpty())return;QJsonObject map=editor->map();QJsonObject pilin=map.value(QStringLiteral("pilinRey")).toObject();QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();bool found=false;
    for(int li=0;li<layers.size()&&!found;++li){QJsonObject layer=layers.at(li).toObject();QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();for(int oi=0;oi<objects.size();++oi){if(objects.at(oi).toObject().value(QStringLiteral("id")).toString()!=id)continue;objects.replace(oi,replacement);layer.insert(QStringLiteral("objects"),objects);layers.replace(li,layer);found=true;break;}}
    if(!found)return;pilin.insert(QStringLiteral("layers"),layers);map.insert(QStringLiteral("pilinRey"),pilin);editor->setMap(map);QMetaObject::invokeMethod(editor,"mapEdited",Qt::DirectConnection,Q_ARG(QJsonObject,map));editor->selectObjectIds(QStringList{id});
}

class CityPreview final:public QWidget{
public:explicit CityPreview(QWidget* parent=nullptr):QWidget(parent){setMinimumHeight(190);setObjectName(QStringLiteral("settlementPreview"));}
    void setSeed(QString seed){seed_=std::move(seed);update();}
protected:void paintEvent(QPaintEvent*)override{
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing,true);p.fillRect(rect(),QColor("#c8c7ba"));
        quint32 h=qHash(seed_);p.setPen(QPen(QColor(103,99,88),2));const QPointF center(width()*.5,height()*.52);
        for(int r=0;r<9;++r){h=h*1664525u+1013904223u;const double a=(h%628)/100.0;h=h*1664525u+1013904223u;const double len=45+(h%110);QPointF end=center+QPointF(std::cos(a)*len,std::sin(a)*len*.65);p.drawLine(center,end);}
        p.setPen(QPen(QColor(72,72,68),1));p.setBrush(QColor(119,123,122));
        for(int i=0;i<58;++i){h=h*1103515245u+12345u;const double a=(h%628)/100.0;h=h*1103515245u+12345u;const double rr=22+(h%135);h=h*1103515245u+12345u;const double w=7+(h%14),hh=5+((h>>8)%11);QPointF c=center+QPointF(std::cos(a)*rr,std::sin(a)*rr*.58);p.save();p.translate(c);p.rotate((h%180));p.drawRect(QRectF(-w*.5,-hh*.5,w,hh));p.restore();}
        p.setBrush(QColor(81,76,69));p.drawRect(QRectF(center.x()-16,center.y()-13,32,26));
    }
private:QString seed_;
};

void editSettlement(PilinReyEditor* editor,const QString& id){
    QJsonObject object=objectById(editor,id);if(object.value(QStringLiteral("type")).toString()!=QStringLiteral("settlement"))return;
    QDialog dialog(editor);dialog.setWindowTitle(QObject::tr("Editar asentamiento"));dialog.resize(720,560);auto* root=new QHBoxLayout(&dialog);root->setContentsMargins(16,16,16,16);root->setSpacing(16);
    auto* formHost=new QWidget(&dialog);auto* form=new QFormLayout(formHost);form->setContentsMargins(0,0,0,0);form->setVerticalSpacing(9);
    auto* name=new QLineEdit(object.value(QStringLiteral("label")).toString(),formHost);auto* group=new QLineEdit(object.value(QStringLiteral("group")).toString(),formHost);auto* type=new QComboBox(formHost);type->addItems({QObject::tr("Aldea"),QObject::tr("Pueblo"),QObject::tr("Ciudad"),QObject::tr("Ciudad grande"),QObject::tr("Capital"),QObject::tr("Fortaleza"),QObject::tr("Puerto")});const int ti=type->findText(object.value(QStringLiteral("settlementType")).toString());if(ti>=0)type->setCurrentIndex(ti);
    auto* culture=new QLineEdit(object.value(QStringLiteral("culture")).toString(),formHost);auto* population=new QSpinBox(formHost);population->setRange(0,50000000);population->setGroupSeparatorShown(true);population->setValue(object.value(QStringLiteral("population")).toInt());auto* elevation=new QSpinBox(formHost);elevation->setRange(-500,9000);elevation->setSuffix(QObject::tr(" m"));elevation->setValue(object.value(QStringLiteral("elevation")).toInt());auto* notes=new QTextEdit(object.value(QStringLiteral("notes")).toString(),formHost);notes->setMinimumHeight(100);auto* atlas=new QLineEdit(object.value(QStringLiteral("atlasId")).toString(),formHost);atlas->setPlaceholderText(QObject::tr("ID de ficha del Atlas (opcional)"));
    form->addRow(QObject::tr("Nombre"),name);form->addRow(QObject::tr("Grupo / reino"),group);form->addRow(QObject::tr("Tipo"),type);form->addRow(QObject::tr("Cultura"),culture);form->addRow(QObject::tr("Población"),population);form->addRow(QObject::tr("Elevación"),elevation);form->addRow(QObject::tr("Ficha Atlas"),atlas);form->addRow(QObject::tr("Notas"),notes);root->addWidget(formHost,1);
    auto* right=new QVBoxLayout;auto* title=new QLabel(QObject::tr("Vista urbana"),&dialog);title->setObjectName(QStringLiteral("settlementPreviewTitle"));auto* preview=new CityPreview(&dialog);preview->setSeed(id+name->text());right->addWidget(title);right->addWidget(preview,1);auto* hint=new QLabel(QObject::tr("La vista urbana se regenera de forma determinista y queda vinculada a la entidad del mapa."),&dialog);hint->setWordWrap(true);right->addWidget(hint);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);right->addWidget(buttons);root->addLayout(right,1);QObject::connect(name,&QLineEdit::textChanged,preview,[preview,id](const QString& n){preview->setSeed(id+n);});QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.setStyleSheet(QStringLiteral(R"QSS(QDialog{background:#0c1117;color:#dfe5eb;}QLineEdit,QComboBox,QSpinBox,QTextEdit{background:#101820;color:#e4e9ee;border:1px solid #31404d;border-radius:7px;padding:7px;}#settlementPreview{border:1px solid #39434a;border-radius:8px;}#settlementPreviewTitle{font-family:'Georgia';font-size:15pt;font-weight:700;color:#f3efe8;}QPushButton{background:#17212b;color:#dbe3ea;border:1px solid #394a59;border-radius:7px;padding:8px 12px;}QPushButton:hover{background:#20303f;})QSS"));
    if(dialog.exec()!=QDialog::Accepted)return;object.insert(QStringLiteral("label"),name->text().trimmed());object.insert(QStringLiteral("group"),group->text().trimmed());object.insert(QStringLiteral("settlementType"),type->currentText());object.insert(QStringLiteral("culture"),culture->text().trimmed());object.insert(QStringLiteral("population"),population->value());object.insert(QStringLiteral("elevation"),elevation->value());object.insert(QStringLiteral("atlasId"),atlas->text().trimmed());object.insert(QStringLiteral("notes"),notes->toPlainText());replaceObject(editor,id,object);
}

void refreshObjectList(PilinReyEditor* editor,QListWidget* list){if(!editor||!list)return;const QJsonObject pilin=editor->map().value(QStringLiteral("pilinRey")).toObject();const QString active=pilin.value(QStringLiteral("activeLayerId")).toString();const QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();QJsonArray objects;for(const QJsonValue& value:layers){const QJsonObject layer=value.toObject();if(layer.value(QStringLiteral("id")).toString()==active){objects=layer.value(QStringLiteral("objects")).toArray();break;}}
    list->blockSignals(true);list->clear();for(const QJsonValue& value:objects){const QJsonObject object=value.toObject();auto* item=new QListWidgetItem(objectCaption(object),list);item->setData(Qt::UserRole,object.value(QStringLiteral("id")).toString());item->setData(Qt::UserRole+1,object.value(QStringLiteral("type")).toString());item->setToolTip(object.value(QStringLiteral("type")).toString()==QStringLiteral("settlement")?QObject::tr("Doble clic para editar la entidad urbana"):QObject::tr("Seleccionar y transformar este objeto"));}if(objects.isEmpty()){auto* empty=new QListWidgetItem(QObject::tr("Esta capa aún no tiene objetos"),list);empty->setFlags(Qt::NoItemFlags);}list->blockSignals(false);}

class MapSelectionFilter final:public QObject{
public:explicit MapSelectionFilter(QMainWindow* window):QObject(window),window_(window){}
    bool eventFilter(QObject*,QEvent* event)override{if(event->type()==QEvent::Show||event->type()==QEvent::LayoutRequest||event->type()==QEvent::ParentChange)QTimer::singleShot(0,this,[this](){apply();});return false;}
    void apply(){if(!window_)return;auto* editor=window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));auto* panel=window_->findChild<QFrame*>(QStringLiteral("pilinSelectionPopover"));if(!editor||!panel)return;
        if(!editor->property("wbwSelectionGeometryShortcuts").toBool()){editor->setProperty("wbwSelectionGeometryShortcuts",true);auto makeShortcut=[editor](const QKeySequence& key,double dx,double dy){auto* shortcut=new QShortcut(key,editor);shortcut->setContext(Qt::WidgetWithChildrenShortcut);QObject::connect(shortcut,&QShortcut::activated,editor,[editor,dx,dy](){editor->nudgeSelection(dx,dy);});};makeShortcut(QKeySequence(QStringLiteral("Alt+Left")),-25,0);makeShortcut(QKeySequence(QStringLiteral("Alt+Right")),25,0);makeShortcut(QKeySequence(QStringLiteral("Alt+Up")),0,-25);makeShortcut(QKeySequence(QStringLiteral("Alt+Down")),0,25);}
        if(!panel->property("wbwSelectionGeometryControls").toBool()){auto* layout=qobject_cast<QVBoxLayout*>(panel->layout());if(layout){auto* label=new QLabel(QObject::tr("MOVER / GEOMETRÍA"),panel);label->setObjectName(QStringLiteral("pilinTinyLabel"));auto* movement=new QHBoxLayout;movement->setSpacing(4);auto* left=command(panel,QStringLiteral("←"),QObject::tr("Mover selección a la izquierda"));auto* up=command(panel,QStringLiteral("↑"),QObject::tr("Mover selección arriba"));auto* down=command(panel,QStringLiteral("↓"),QObject::tr("Mover selección abajo"));auto* right=command(panel,QStringLiteral("→"),QObject::tr("Mover selección a la derecha"));auto* rotateLeft=command(panel,QStringLiteral("↺"),QObject::tr("Rotar trazado −15°"));auto* rotateRight=command(panel,QStringLiteral("↻"),QObject::tr("Rotar trazado +15°"));auto* shrink=command(panel,QStringLiteral("↙"),QObject::tr("Reducir geometría"));auto* grow=command(panel,QStringLiteral("↗"),QObject::tr("Aumentar geometría"));auto* labelStyle=command(panel,QStringLiteral("Aa"),QObject::tr("Estilo de etiqueta"));for(QToolButton* b:{left,up,down,right,rotateLeft,rotateRight,shrink,grow,labelStyle})movement->addWidget(b);layout->addWidget(label);layout->addLayout(movement);QObject::connect(left,&QToolButton::clicked,editor,[editor](){editor->nudgeSelection(-25,0);});QObject::connect(right,&QToolButton::clicked,editor,[editor](){editor->nudgeSelection(25,0);});QObject::connect(up,&QToolButton::clicked,editor,[editor](){editor->nudgeSelection(0,-25);});QObject::connect(down,&QToolButton::clicked,editor,[editor](){editor->nudgeSelection(0,25);});QObject::connect(rotateLeft,&QToolButton::clicked,editor,[editor](){editor->transformSelectedPathGeometry(1,-15);});QObject::connect(rotateRight,&QToolButton::clicked,editor,[editor](){editor->transformSelectedPathGeometry(1,15);});QObject::connect(shrink,&QToolButton::clicked,editor,[editor](){editor->transformSelectedPathGeometry(.9,0);});QObject::connect(grow,&QToolButton::clicked,editor,[editor](){editor->transformSelectedPathGeometry(1.1,0);});QObject::connect(labelStyle,&QToolButton::clicked,editor,&PilinReyEditor::editSelectedLabelStyle);panel->setProperty("wbwSelectionGeometryControls",true);panel->adjustSize();}}
        QFrame* layers=layersPanel(editor);if(!layers)return;auto* objectList=layers->findChild<QListWidget*>(QStringLiteral("pilinLayerObjects"));if(!objectList){auto* layout=qobject_cast<QVBoxLayout*>(layers->layout());if(!layout)return;auto* heading=new QLabel(QObject::tr("OBJETOS DE LA CAPA"),layers);heading->setObjectName(QStringLiteral("pilinTinyLabel"));objectList=new QListWidget(layers);objectList->setObjectName(QStringLiteral("pilinLayerObjects"));objectList->setMinimumHeight(96);objectList->setMaximumHeight(170);layout->insertWidget(2,heading);layout->insertWidget(3,objectList);QObject::connect(objectList,&QListWidget::itemClicked,editor,[editor](QListWidgetItem* item){if(!item||!(item->flags()&Qt::ItemIsEnabled))return;const QString id=item->data(Qt::UserRole).toString();if(!id.isEmpty())editor->selectObjectIds(QStringList{id});});QObject::connect(objectList,&QListWidget::itemDoubleClicked,editor,[editor](QListWidgetItem* item){if(!item)return;const QString id=item->data(Qt::UserRole).toString();if(item->data(Qt::UserRole+1).toString()==QStringLiteral("settlement"))editSettlement(editor,id);else if(!id.isEmpty())editor->selectObjectIds(QStringList{id});});QObject::connect(editor,&PilinReyEditor::mapEdited,objectList,[editor,objectList](const QJsonObject&){refreshObjectList(editor,objectList);});for(QListWidget* candidate:layers->findChildren<QListWidget*>()){if(candidate==objectList)continue;QObject::connect(candidate,&QListWidget::currentRowChanged,objectList,[editor,objectList](int){QTimer::singleShot(0,objectList,[editor,objectList](){refreshObjectList(editor,objectList);});});break;}}
        refreshObjectList(editor,objectList);
    }
private:QPointer<QMainWindow> window_;
};

} // namespace

void installMapSelectionController(QMainWindow* window){if(!window||window->property("wbwMapSelectionControllerV4").toBool())return;window->setProperty("wbwMapSelectionControllerV4",true);auto* filter=new MapSelectionFilter(window);window->installEventFilter(filter);QTimer::singleShot(0,filter,[filter](){filter->apply();});}

} // namespace wbw
