#include "ui/MapExporter.h"

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMarginsF>
#include <QObject>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QSize>
#include <QSizeF>
#include <QSvgGenerator>

#include <algorithm>
#include <cmath>
#include <vector>

namespace wbw {
namespace {

constexpr double kTau = 6.2831853071795864769;

QByteArray dataUrlBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive)
        ? QByteArray::fromBase64(payload)
        : QByteArray::fromPercentEncoding(payload);
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    const QColor parsed(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return parsed.isValid() ? parsed : fallback;
}

QPointF pointOf(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
}

double radiusOf(const QJsonValue& value, double fallback) {
    return value.toObject().value(QStringLiteral("radius")).toDouble(fallback);
}

quint32 hashText(const QString& text) {
    quint32 h = 2166136261u;
    const QByteArray bytes = text.toUtf8();
    for (const char c : bytes) { h ^= static_cast<unsigned char>(c); h *= 16777619u; }
    return h;
}

std::vector<QPointF> smoothPath(const QJsonArray& source) {
    std::vector<QPointF> points;
    for (const QJsonValue& v : source) points.push_back(pointOf(v));
    if (points.size() < 3) return points;
    const auto d2 = [](const QPointF& a, const QPointF& b) { const double dx=a.x()-b.x(), dy=a.y()-b.y(); return dx*dx+dy*dy; };
    const bool closed = d2(points.front(), points.back()) < 1e-8;
    if (closed && points.size() > 3) points.pop_back();
    const size_t n = points.size();
    std::vector<QPointF> out;
    const size_t segments = closed ? n : n - 1;
    constexpr int samples = 10;
    for (size_t i = 0; i < segments; ++i) {
        const size_t j = (i + 1) % n;
        const QPointF p0 = (!closed && i == 0) ? points[i] : points[(i + n - 1) % n];
        const QPointF p1 = points[i];
        const QPointF p2 = points[j];
        const QPointF p3 = (!closed && j + 1 >= n) ? p2 : points[(j + 1) % n];
        for (int s = 0; s < samples; ++s) {
            const double t = static_cast<double>(s) / samples, t2=t*t, t3=t2*t;
            out.emplace_back(
                .5*((2*p1.x())+(-p0.x()+p2.x())*t+(2*p0.x()-5*p1.x()+4*p2.x()-p3.x())*t2+(-p0.x()+3*p1.x()-3*p2.x()+p3.x())*t3),
                .5*((2*p1.y())+(-p0.y()+p2.y())*t+(2*p0.y()-5*p1.y()+4*p2.y()-p3.y())*t2+(-p0.y()+3*p1.y()-3*p2.y()+p3.y())*t3));
        }
    }
    out.push_back(closed ? out.front() : points.back());
    return out;
}

QPolygonF projected(const std::vector<QPointF>& points, double sx, double sy) {
    QPolygonF poly;
    poly.reserve(static_cast<int>(points.size()));
    for (const QPointF& p : points) poly.append(QPointF(p.x()*sx, p.y()*sy));
    return poly;
}

QImage terrainImage(const QJsonObject& pilin, const QSize& size) {
    QImage mask(size, QImage::Format_ARGB32_Premultiplied); mask.fill(Qt::transparent);
    QPainter mp(&mask); mp.setRenderHint(QPainter::Antialiasing, true);
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const double sx = size.width()/docW, sy=size.height()/docH;
    for (const QJsonValue& lv : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer=lv.toObject(); if(!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        const double layerOpacity=std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0),0.0,1.0);
        for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) {
            const QJsonObject o=ov.toObject(); const QString type=o.value(QStringLiteral("type")).toString();
            if(type!=QStringLiteral("land")&&type!=QStringLiteral("sea")) continue;
            const QJsonArray pts=o.value(QStringLiteral("points")).toArray(); if(pts.isEmpty()) continue;
            mp.setCompositionMode(type==QStringLiteral("land")?QPainter::CompositionMode_SourceOver:QPainter::CompositionMode_Clear);
            mp.setOpacity(layerOpacity);
            for(int i=0;i<pts.size();++i){const QPointF p=pointOf(pts.at(i));const double r=radiusOf(pts.at(i),180.0);mp.setPen(QPen(Qt::white,std::max(1.0,r*2*sx),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));if(i==0)mp.drawPoint(QPointF(p.x()*sx,p.y()*sy));else{const QPointF a=pointOf(pts.at(i-1));mp.drawLine(QPointF(a.x()*sx,a.y()*sy),QPointF(p.x()*sx,p.y()*sy));}}
            mp.setOpacity(1.0);
        }
    }
    mp.end();
    const QColor land=themeColor(pilin,QStringLiteral("land"),QColor(216,201,158));
    const QColor coast=themeColor(pilin,QStringLiteral("coast"),QColor(65,59,48));
    QImage out(size,QImage::Format_ARGB32_Premultiplied);out.fill(Qt::transparent);
    for(int y=1;y<size.height()-1;++y){const QRgb* src=reinterpret_cast<const QRgb*>(mask.constScanLine(y));const QRgb* up=reinterpret_cast<const QRgb*>(mask.constScanLine(y-1));const QRgb* down=reinterpret_cast<const QRgb*>(mask.constScanLine(y+1));QRgb* dst=reinterpret_cast<QRgb*>(out.scanLine(y));for(int x=1;x<size.width()-1;++x){if(qAlpha(src[x])<100)continue;dst[x]=qRgba(land.red(),land.green(),land.blue(),255);if(qAlpha(up[x])<100||qAlpha(down[x])<100||qAlpha(src[x-1])<100||qAlpha(src[x+1])<100)dst[x]=qRgba(coast.red(),coast.green(),coast.blue(),255);}}
    return out;
}

void drawBuiltinStamp(QPainter& painter, const QString& kind, const QPointF& p, double s, const QColor& color) {
    QPen pen(color,std::max(1.0,s*.09),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);painter.setPen(pen);painter.setBrush(Qt::NoBrush);
    if(kind==QStringLiteral("castle")){painter.drawRect(QRectF(p.x()-s,p.y()-s*.35,s*2,s*1.35));for(int i=-1;i<=1;++i)painter.drawRect(QRectF(p.x()+i*s*.75-s*.23,p.y()-s,s*.46,s*.7));}
    else if(kind==QStringLiteral("tower")){painter.drawRect(QRectF(p.x()-s*.45,p.y()-s,s*.9,s*1.8));painter.drawLine(p+QPointF(-s*.7,-s),p+QPointF(s*.7,-s));}
    else if(kind==QStringLiteral("temple")){QPolygonF roof{p+QPointF(-s,-s*.35),p+QPointF(0,-s),p+QPointF(s,-s*.35)};painter.drawPolyline(roof);for(int i=-1;i<=1;++i)painter.drawLine(p+QPointF(i*s*.55,-s*.3),p+QPointF(i*s*.55,s*.75));painter.drawLine(p+QPointF(-s,s*.75),p+QPointF(s,s*.75));}
    else if(kind==QStringLiteral("ruin")){QPolygonF line{p+QPointF(-s,s*.7),p+QPointF(-s*.7,-s*.6),p+QPointF(-s*.15,-s*.15),p+QPointF(s*.25,-s*.75),p+QPointF(s,s*.7)};painter.drawPolyline(line);}
    else if(kind==QStringLiteral("ship")){painter.drawLine(p+QPointF(-s,s*.35),p+QPointF(s,s*.35));painter.drawLine(p+QPointF(-s,s*.35),p+QPointF(-s*.6,s*.8));painter.drawLine(p+QPointF(-s*.6,s*.8),p+QPointF(s*.65,s*.8));painter.drawLine(p+QPointF(0,-s),p+QPointF(0,s*.35));painter.drawPolyline(QPolygonF{p+QPointF(0,-s*.8),p+QPointF(s*.6,-s*.05),p+QPointF(0,-s*.05)});}
    else if(kind==QStringLiteral("bridge")){painter.drawLine(p+QPointF(-s,s*.55),p+QPointF(s,s*.55));painter.drawPolyline(QPolygonF{p+QPointF(-s,s*.55),p+QPointF(-s*.65,-s*.25),p+QPointF(s*.65,-s*.25),p+QPointF(s,s*.55)});}
    else if(kind==QStringLiteral("compass")){painter.drawLine(p+QPointF(0,-s),p+QPointF(0,s));painter.drawLine(p+QPointF(-s,0),p+QPointF(s,0));painter.drawLine(p+QPointF(-s*.7,-s*.7),p+QPointF(s*.7,s*.7));painter.drawLine(p+QPointF(s*.7,-s*.7),p+QPointF(-s*.7,s*.7));}
    else if(kind==QStringLiteral("mill")){painter.drawRect(QRectF(p.x()-s*.35,p.y()+s*.2,s*.7,s*.8));painter.drawLine(p+QPointF(-s,-s),p+QPointF(s,s*.6));painter.drawLine(p+QPointF(s,-s),p+QPointF(-s,s*.6));}
    else painter.drawRect(QRectF(p.x()-s*.55,p.y()-s*.55,s*1.1,s*1.1));
}

void drawTextObject(QPainter& painter,const QJsonObject& o,const QString& text,const QPointF& pos,double scale,const QJsonObject& pilin,bool boldFallback=false){if(text.trimmed().isEmpty())return;QFont font(o.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Georgia")));font.setPointSizeF(std::clamp(o.value(QStringLiteral("fontSize")).toDouble(54)*scale*.75,6.0,240.0));font.setBold(o.value(QStringLiteral("bold")).toBool(boldFallback));font.setItalic(o.value(QStringLiteral("italic")).toBool(false));QPainterPath path;path.addText(QPointF(0,0),font,text);QColor fill(o.value(QStringLiteral("color")).toString());if(!fill.isValid())fill=themeColor(pilin,QStringLiteral("text"),QColor(40,37,32));QColor outline=themeColor(pilin,QStringLiteral("labelOutline"),QColor(238,226,195));painter.save();painter.translate(pos);painter.rotate(o.value(QStringLiteral("rotation")).toDouble());painter.setPen(QPen(outline,std::max(1.0,font.pointSizeF()*.08)));painter.setBrush(fill);painter.drawPath(path);painter.restore();}

void paintObject(QPainter& painter,const QJsonObject& o,double sx,double sy,const QJsonObject& pilin){const QString type=o.value(QStringLiteral("type")).toString();const double scale=(sx+sy)*.5;
    if(type==QStringLiteral("land")||type==QStringLiteral("sea"))return;
    if(type==QStringLiteral("settlement")){const QPointF p(o.value(QStringLiteral("x")).toDouble()*sx,o.value(QStringLiteral("y")).toDouble()*sy);const double sc=std::clamp(o.value(QStringLiteral("scale")).toDouble(1.0),.25,6.0);const double r=std::clamp(10.0*sc*std::sqrt(scale+.18),4.0,30.0);const QColor c=themeColor(pilin,QStringLiteral("symbol"),QColor(48,40,34));painter.setPen(QPen(c,std::max(1.0,1.4*scale)));const QString kind=o.value(QStringLiteral("kind")).toString();if(kind.contains(QStringLiteral("Capital"),Qt::CaseInsensitive)){painter.setBrush(c);painter.drawRect(QRectF(p.x()-r,p.y()-r,r*2,r*2));painter.setBrush(Qt::NoBrush);painter.drawRect(QRectF(p.x()-r-4,p.y()-r-4,r*2+8,r*2+8));}else if(kind.contains(QStringLiteral("Puerto"),Qt::CaseInsensitive)){painter.setBrush(Qt::NoBrush);painter.drawRect(QRectF(p.x()-r,p.y()-r,r*2,r*2));painter.drawLine(p+QPointF(0,-r*1.6),p+QPointF(0,r*1.6));}else{painter.setBrush(c);painter.drawEllipse(p,r,r);}QJsonObject style=o;if(!style.contains(QStringLiteral("fontSize")))style.insert(QStringLiteral("fontSize"),kind.contains(QStringLiteral("Capital"),Qt::CaseInsensitive)?58:46);drawTextObject(painter,style,o.value(QStringLiteral("label")).toString(),p+QPointF(r+7,-r*.3),scale,pilin,kind.contains(QStringLiteral("Capital"),Qt::CaseInsensitive));return;}
    if(type==QStringLiteral("label")){drawTextObject(painter,o,o.value(QStringLiteral("text")).toString(),QPointF(o.value(QStringLiteral("x")).toDouble()*sx,o.value(QStringLiteral("y")).toDouble()*sy),scale,pilin);return;}
    if(type==QStringLiteral("stamp")){const QPointF p(o.value(QStringLiteral("x")).toDouble()*sx,o.value(QStringLiteral("y")).toDouble()*sy);const double s=o.value(QStringLiteral("size")).toDouble(150)*o.value(QStringLiteral("scale")).toDouble(1.0)*scale*.5;painter.save();painter.translate(p);painter.rotate(o.value(QStringLiteral("rotation")).toDouble());painter.setOpacity(std::clamp(o.value(QStringLiteral("opacity")).toDouble(1.0),0.0,1.0));const QString data=o.value(QStringLiteral("dataUrl")).toString();if(!data.isEmpty()){QImage img;if(img.loadFromData(dataUrlBytes(data)))painter.drawImage(QRectF(-s,-s,s*2,s*2),img);}else drawBuiltinStamp(painter,o.value(QStringLiteral("assetKind")).toString(QStringLiteral("castle")),QPointF(0,0),s,themeColor(pilin,QStringLiteral("symbol"),QColor(48,40,34)));painter.restore();return;}
    if(type==QStringLiteral("forestArea")||type==QStringLiteral("forest")){const QJsonArray pts=o.value(QStringLiteral("points")).toArray();if(pts.isEmpty())return;QRandomGenerator rng(hashText(o.value(QStringLiteral("id")).toString()));const int density=o.value(QStringLiteral("density")).toInt(58);const double symbol=o.value(QStringLiteral("symbolSize")).toDouble(64)*scale;const int count=std::max(24,static_cast<int>(pts.size())*density/2);painter.setPen(QPen(themeColor(pilin,QStringLiteral("forest"),QColor(55,86,53)),std::max(1.0,scale)));for(int i=0;i<count;++i){const int idx=rng.bounded(pts.size());const QPointF c=pointOf(pts.at(idx));const double radius=radiusOf(pts.at(idx),180);const double a=rng.generateDouble()*kTau,rr=std::sqrt(rng.generateDouble())*radius;const QPointF p((c.x()+std::cos(a)*rr)*sx,(c.y()+std::sin(a)*rr)*sy);const double ss=std::clamp(symbol*(.7+rng.generateDouble()*.65),3.0,42.0);painter.drawLine(p+QPointF(0,-ss),p+QPointF(-ss*.62,ss*.35));painter.drawLine(p+QPointF(0,-ss),p+QPointF(ss*.62,ss*.35));painter.drawLine(p+QPointF(-ss*.48,-ss*.05),p+QPointF(ss*.48,-ss*.05));painter.drawLine(p+QPointF(0,ss*.35),p+QPointF(0,ss*.78));}return;}
    if(type==QStringLiteral("mountainArea")||type==QStringLiteral("mountain")){const QJsonArray pts=o.value(QStringLiteral("points")).toArray();if(pts.isEmpty())return;QRandomGenerator rng(hashText(o.value(QStringLiteral("id")).toString())^0x8f34a1u);const int density=o.value(QStringLiteral("density")).toInt(58);const double symbol=o.value(QStringLiteral("symbolSize")).toDouble(72)*scale;const int count=std::max(18,static_cast<int>(pts.size())*density/3);painter.setPen(QPen(themeColor(pilin,QStringLiteral("mountain"),QColor(77,68,59)),std::max(1.0,scale)));for(int i=0;i<count;++i){const int idx=rng.bounded(pts.size());const QPointF c=pointOf(pts.at(idx));const double radius=radiusOf(pts.at(idx),220);const double a=rng.generateDouble()*kTau,rr=std::sqrt(rng.generateDouble())*radius;const QPointF p((c.x()+std::cos(a)*rr)*sx,(c.y()+std::sin(a)*rr)*sy);const double ss=std::clamp(symbol*(.66+rng.generateDouble()*.75),4.0,56.0);painter.drawLine(p+QPointF(-ss,ss*.56),p+QPointF(0,-ss));painter.drawLine(p+QPointF(0,-ss),p+QPointF(ss,ss*.56));painter.drawLine(p+QPointF(-ss*.31,-ss*.05),p+QPointF(0,ss*.2));painter.drawLine(p+QPointF(0,ss*.2),p+QPointF(ss*.28,-ss*.13));}return;}
    const QJsonArray raw=o.value(QStringLiteral("points")).toArray();if(raw.size()<2)return;const QPolygonF poly=projected(smoothPath(raw),sx,sy);const int width=o.value(QStringLiteral("width")).toInt(5);
    if(type==QStringLiteral("region")){QPainterPath path;path.addPolygon(poly);path.closeSubpath();QColor fill=themeColor(pilin,QStringLiteral("region"),QColor(176,117,72));fill.setAlphaF(std::clamp(o.value(QStringLiteral("fillOpacity")).toDouble(.18),0.0,1.0));painter.fillPath(path,fill);painter.setPen(QPen(themeColor(pilin,QStringLiteral("region"),QColor(113,91,57)),std::max(1.0,width*scale*.6),Qt::DashLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(path);return;}
    if(type==QStringLiteral("river")){const QColor dark=themeColor(pilin,QStringLiteral("river"),QColor(37,79,109)),light=dark.lighter(155);for(int i=1;i<poly.size();++i){const double t=static_cast<double>(i)/std::max(1,static_cast<int>(poly.size())-1);const double outer=std::max(1.0,width*(.35+.85*t)*scale);painter.setPen(QPen(dark,outer+2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawLine(poly.at(i-1),poly.at(i));painter.setPen(QPen(light,std::max(1.0,outer*.45),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawLine(poly.at(i-1),poly.at(i));}return;}
    if(type==QStringLiteral("road")){const QColor road=themeColor(pilin,QStringLiteral("road"),QColor(112,78,46));painter.setPen(QPen(road.darker(145),std::max(2.0,(width+3)*scale),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPolyline(poly);painter.setPen(QPen(road.lighter(150),std::max(1.0,width*scale),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPolyline(poly);return;}
    if(type==QStringLiteral("border")){painter.setPen(QPen(themeColor(pilin,QStringLiteral("border"),QColor(145,63,55)),std::max(1.0,width*scale),Qt::DashLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPolyline(poly);return;}
    if(type==QStringLiteral("coast")){QPainterPath path;path.addPolygon(poly);path.closeSubpath();painter.fillPath(path,themeColor(pilin,QStringLiteral("land"),QColor(216,201,158)));painter.setPen(QPen(themeColor(pilin,QStringLiteral("coast"),QColor(65,59,48)),std::max(1.0,2*scale)));painter.drawPath(path);}
}

void paintTemplate(QPainter& painter,const QJsonObject& map,const QJsonObject& pilin,double sx,double sy){const QJsonObject t=pilin.value(QStringLiteral("template")).toObject();if(!t.value(QStringLiteral("visible")).toBool(true))return;QString data=t.value(QStringLiteral("dataUrl")).toString();if(data.isEmpty())data=map.value(QStringLiteral("backgroundImageDataUrl")).toString();if(data.isEmpty())return;QImage img;if(!img.loadFromData(dataUrlBytes(data)))return;const double docW=std::max(1.0,pilin.value(QStringLiteral("width")).toDouble(4096)),docH=std::max(1.0,pilin.value(QStringLiteral("height")).toDouble(2304));const double sc=std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0),.05,5.0);const double w=docW*sx*sc,h=docH*sy*sc;const double x=t.value(QStringLiteral("x")).toDouble()*sx+(docW*sx-w)*.5,y=t.value(QStringLiteral("y")).toDouble()*sy+(docH*sy-h)*.5;painter.save();painter.setOpacity(std::clamp(t.value(QStringLiteral("opacity")).toDouble(.35),0.0,1.0));painter.translate(x+w*.5,y+h*.5);painter.rotate(t.value(QStringLiteral("rotation")).toDouble());painter.drawImage(QRectF(-w*.5,-h*.5,w,h),img);painter.restore();}

void paintMap(QPainter& painter,const QJsonObject& map,const QSize& size){const QJsonObject pilin=map.value(QStringLiteral("pilinRey")).toObject();const double docW=std::max(1.0,pilin.value(QStringLiteral("width")).toDouble(4096)),docH=std::max(1.0,pilin.value(QStringLiteral("height")).toDouble(2304));const double sx=size.width()/docW,sy=size.height()/docH;painter.setRenderHint(QPainter::Antialiasing,true);painter.setRenderHint(QPainter::TextAntialiasing,true);painter.fillRect(QRect(QPoint(0,0),size),themeColor(pilin,QStringLiteral("sea"),QColor(177,194,198)));painter.drawImage(QRect(QPoint(0,0),size),terrainImage(pilin,size));paintTemplate(painter,map,pilin,sx,sy);for(const QJsonValue& lv:pilin.value(QStringLiteral("layers")).toArray()){const QJsonObject layer=lv.toObject();if(!layer.value(QStringLiteral("visible")).toBool(true))continue;painter.save();painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0),0.0,1.0));for(const QJsonValue& ov:layer.value(QStringLiteral("objects")).toArray())paintObject(painter,ov.toObject(),sx,sy,pilin);painter.restore();}}

} // namespace

bool MapExporter::exportPng(const QJsonObject& map,const QString& path,const QSize& pixelSize,QString* error){if(!pixelSize.isValid()||pixelSize.isEmpty()){if(error)*error=QObject::tr("Tamaño de exportación inválido.");return false;}QImage image(pixelSize,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);paintMap(painter,map,pixelSize);painter.end();if(!image.save(path,"PNG")){if(error)*error=QObject::tr("No se pudo escribir el PNG.");return false;}return true;}
bool MapExporter::exportPdf(const QJsonObject& map,const QString& path,const QSize& pixelSize,QString* error){if(!pixelSize.isValid()||pixelSize.isEmpty()){if(error)*error=QObject::tr("Tamaño de exportación inválido.");return false;}QPdfWriter writer(path);writer.setResolution(96);writer.setPageSize(QPageSize(QSizeF(pixelSize.width()*25.4/96.0,pixelSize.height()*25.4/96.0),QPageSize::Millimeter,QStringLiteral("Pilín Rey")));writer.setPageMargins(QMarginsF(0,0,0,0));QPainter painter(&writer);if(!painter.isActive()){if(error)*error=QObject::tr("No se pudo iniciar el exportador PDF.");return false;}paintMap(painter,map,QSize(writer.width(),writer.height()));painter.end();return true;}
bool MapExporter::exportSvg(const QJsonObject& map,const QString& path,const QSize& pixelSize,QString* error){if(!pixelSize.isValid()||pixelSize.isEmpty()){if(error)*error=QObject::tr("Tamaño de exportación inválido.");return false;}QSvgGenerator svg;svg.setFileName(path);svg.setSize(pixelSize);svg.setViewBox(QRect(QPoint(0,0),pixelSize));svg.setTitle(QStringLiteral("Pilín Rey"));QPainter painter(&svg);if(!painter.isActive()){if(error)*error=QObject::tr("No se pudo iniciar el exportador SVG.");return false;}paintMap(painter,map,pixelSize);painter.end();return true;}

} // namespace wbw
